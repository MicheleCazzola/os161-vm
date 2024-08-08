/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Coremap handling, used to track freed frames
 */

#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <addrspace.h>
#include <pagevm.h>
#include <coremap.h>
#include <swapfile.h>
#include "opt-paging.h"

/**
 * Coremap management for handling physical memory pages.
 * Manages the allocation and deallocation of memory pages,
 * including support for both kernel and user pages.
 */


/* Spinlocks for synchronization */
static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;   /* Lock for coremap operations */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER; /* Lock for stealmem operations */
static struct spinlock page_replacement_lock = SPINLOCK_INITIALIZER; /* Lock for page replacement */

/* Coremap structure and related variables */
static struct coremap_entry *coremap = NULL; /* Coremap array to track pages */
static int total_ram_frames = 0; /* Total number of RAM frames */
static int is_coremap_initialized = 0; /* Flag to check if coremap is initialized */

/* Linked list variables for page replacement strategy */
static unsigned long invalid_reference = 0; /* Indicates an invalid reference */
static unsigned long last_allocated_page = 0; /* Refers to the last allocated page */
static unsigned long current_victim_page = 0; /* Refers to the current page selected for replacement */

/*
 * Checks if the coremap is initialized by examining the is_coremap_initialized 'shared' variable.
 * Returns 1 if initialized, 0 otherwise.
 */
static int is_coremap_active() {
    int active;
    spinlock_acquire(&coremap_lock);
    active = is_coremap_initialized;
    spinlock_release(&coremap_lock);
    return active;
}

/*
 * Initializes the coremap structure and sets up memory management.
 * Allocates memory for coremap entries and initializes each entry.
 * It's called at the bootstrap (by pagevm.c)
 */
void coremap_init(void) {
    int i;

    /* Calculate the total number of RAM frames based on system RAM size */
    total_ram_frames = ((int)ram_getsize()) / PAGE_SIZE;

    /* Allocate memory for coremap entries */
    coremap = kmalloc(sizeof(struct coremap_entry) * total_ram_frames);
    if (coremap == NULL) {
        panic("Failed to allocate memory for coremap");
    }

    /* Initialize each coremap entry */
    for (i = 0; i < total_ram_frames; i++) {
        coremap[i].entry_type = COREMAP_UNTRACKED; /* Entry type as untracked initially */
        coremap[i].virtual_address = 0;
        coremap[i].address_space = NULL;
        coremap[i].allocation_size = 0;
        coremap[i].next_allocated = invalid_reference;
        coremap[i].previous_allocated = invalid_reference;
    }

    /* Initialize linked list variables */
    invalid_reference = total_ram_frames;
    last_allocated_page = invalid_reference;
    current_victim_page = invalid_reference;

    /* Set coremap as initialized */
    spinlock_acquire(&coremap_lock);
    is_coremap_initialized = 1;
    spinlock_release(&coremap_lock);
}

/*
 * Shuts down coremap and frees allocated resources.
 */
void coremap_shutdown(void) {
    spinlock_acquire(&coremap_lock);
    is_coremap_initialized = 0;
    spinlock_release(&coremap_lock);

    /* Free the handler (i.e. the array coremap)*/
    kfree(coremap);
}

/*
 * Finds and allocates a contiguous block of freed pages of size npages.
 * Updates coremap to mark the allocated pages as busy.
 */
static paddr_t allocate_free_pages(unsigned long npages,
                                   unsigned char entry_type,
                                   addrspace_t *as,
                                   vaddr_t vaddr) {
    paddr_t address;
    long i, first_free_page, found_block_start;
    long pages_needed = (long)npages;

    if (!is_coremap_active()) {
        return 0;
    }

    spinlock_acquire(&coremap_lock);
    for (i = 0, first_free_page = found_block_start = -1; i < total_ram_frames; i++) {
        if (coremap[i].entry_type == COREMAP_FREED) {
            if (i == 0 || coremap[i - 1].entry_type != COREMAP_FREED) {
                first_free_page = i; /* Start of a new free block */
            }
            if (i - first_free_page + 1 >= pages_needed) {
                found_block_start = first_free_page;
                break;
            }
        }
    }

    if (found_block_start >= 0) {
        /* Allocate the block of pages */
        for (i = found_block_start; i < found_block_start + pages_needed; i++) {
            coremap[i].entry_type = entry_type;
            coremap[i].allocation_size = (entry_type == COREMAP_BUSY_KERNEL) ? npages : 0; //if it is not busy for kernel processes, it is useless to keep track of allocation size (only kernel requests for multiple contiguous pages)

            if (entry_type == COREMAP_BUSY_USER) {
                coremap[i].address_space = as;
                coremap[i].virtual_address = vaddr;
            } else if (entry_type == COREMAP_BUSY_KERNEL){
                coremap[i].address_space = NULL;
                coremap[i].virtual_address = 0;
            } else {
                return EINVAL;
            }
        }
        address = (paddr_t)found_block_start * PAGE_SIZE;
    } else {
        address = 0;
    }

    spinlock_release(&coremap_lock);
    return address;
}

/*
 * Allocates kernel pages. If no free pages are available, it calls ram_stealmem().
 * Updates coremap to track the allocated pages.
 */
static paddr_t allocate_kernel_pages(unsigned long npages) {
    paddr_t address;
    unsigned long i;

    /* Attempt to allocate from freed pages managed by coremap */
    address = allocate_free_pages(npages, COREMAP_BUSY_KERNEL, NULL, 0);
    if (address == 0) {
        /* Call stealmem if no free pages are available */
        spinlock_acquire(&stealmem_lock);
        address = ram_stealmem(npages);
        spinlock_release(&stealmem_lock);
    }

    /* Update coremap to track the newly obtained pages */
    if (address != 0 && is_coremap_active()) {
        spinlock_acquire(&coremap_lock);
        for (i = 0; i < npages; i++) {
            int page_index = (address / PAGE_SIZE) + i;
            coremap[page_index].entry_type = COREMAP_BUSY_KERNEL;
        }
        coremap[address / PAGE_SIZE].allocation_size = npages;
        spinlock_release(&coremap_lock);
    }

    return address;
}

/*
 * Frees a contiguous block of pages starting from address.
 * Updates coremap to mark the pages as freed.
 */
static int free_pages(paddr_t address, unsigned long npages) {
    long i, first_page, pages_to_free = (long)npages;

    if (!is_coremap_active()) {
        return 0;
    }
    first_page = address / PAGE_SIZE;
    KASSERT(total_ram_frames > first_page);

    spinlock_acquire(&coremap_lock);
    for (i = first_page; i < first_page + pages_to_free; i++) {
        coremap[i].entry_type = COREMAP_FREED;
        coremap[i].virtual_address = 0;
        coremap[i].address_space = NULL;
    }
    coremap[first_page].allocation_size = 0;
    spinlock_release(&coremap_lock);

    return 1;
}

/*
 * Allocates kernel-space pages.
 * Calls allocate_kernel_pages() to allocate pages and then converts physical address to virtual address.
 */
vaddr_t alloc_kpages(unsigned npages) {
    paddr_t physical_address;

    pagevm_can_sleep(); //assert we are in a context where sleeping is safe
    physical_address = allocate_kernel_pages(npages);
    if (physical_address == 0) {
        return 0;
    }
    return PADDR_TO_KVADDR(physical_address); /* Convert physical address to kernel virtual address */
}

/*
 * Frees a range of memory pages allocated to the kernel.
 * Calls free_pages() to free the pages and update coremap.
 */
void free_kpages(vaddr_t addr) {
    if (is_coremap_active()) {
        paddr_t physical_address = addr - MIPS_KSEG0; // MIPS_KSEG0 is the base address of the direct-mapped segment in the architecture (that is MIPS).
        long first_page = physical_address / PAGE_SIZE;
        KASSERT(total_ram_frames > first_page);
        free_pages(physical_address, coremap[first_page].allocation_size);
    }
}

/*
 * Allocates a single user page.
 * Tries to find a free page managed by coremap first; if not found, it calls ram_stealmem().
 * Updates the coremap to track the allocated page and handles page replacement if necessary.
 */
static paddr_t allocate_user_page(vaddr_t associated_vaddr) {
    addrspace_t *current_as;
    //ps_t *victim_segment;
    paddr_t address;
    unsigned long last_allocated_temp, current_victim_temp, new_victim;
    off_t swapfile_offset;
    int result;

    current_as = proc_getas();
    KASSERT(current_as != NULL); /* Ensure that address space is initialized */
    KASSERT((associated_vaddr & PAGE_FRAME) == associated_vaddr); /* Ensure page alignment */

    /* Try to allocate from freed pages managed by coremap */
    address = allocate_free_pages(1, COREMAP_BUSY_USER, current_as, associated_vaddr);
    if (address == 0) {
        /* Call stealmem if no free pages are available */
        spinlock_acquire(&stealmem_lock);
        address = ram_stealmem(1);
        spinlock_release(&stealmem_lock);
    }
    /* Update the coremap to track the newly obtained page from ram_stealmem */
	if (is_coremap_active())
	{
		spinlock_acquire(&page_replacement_lock);
		last_allocated_temp = last_allocated_page;
		current_victim_temp = current_victim_page;
		spinlock_release(&page_replacement_lock);

        if (address != 0) {
            long page_index = address / PAGE_SIZE;
            KASSERT(total_ram_frames > page_index);

            /* Update coremap to track the allocated page */
            spinlock_acquire(&coremap_lock);
            if (coremap[page_index].entry_type == COREMAP_FREED) {
                last_allocated_temp = last_allocated_page;
                current_victim_temp = current_victim_page;

                coremap[page_index].entry_type = COREMAP_BUSY_USER;
                coremap[page_index].allocation_size = 1;
                coremap[page_index].address_space = current_as;
                coremap[page_index].virtual_address = associated_vaddr;

                /* Update the linked list for the allocation queue */
                if (last_allocated_temp != invalid_reference) {
                    coremap[last_allocated_temp].next_allocated = page_index;
                    coremap[page_index].previous_allocated = last_allocated_temp;
                    coremap[page_index].next_allocated = invalid_reference;
                } else {
                    coremap[page_index].previous_allocated = invalid_reference;
                    coremap[page_index].next_allocated = invalid_reference;
                }
                spinlock_release(&coremap_lock);

                spinlock_acquire(&page_replacement_lock);
                if (current_victim_temp == invalid_reference) {
                    current_victim_page = page_index;
                }
                last_allocated_page = page_index;
                spinlock_release(&page_replacement_lock);
            } else {
                /* If no free pages, swap out a page */
                address = (paddr_t)current_victim_temp * PAGE_SIZE;
                result = swap_out(address, &swapfile_offset);
                if (result) {
                    panic("Failed to swap out a page to file");
                }

                spinlock_acquire(&coremap_lock);
                //victim_segment = as_find_segment_coarse(coremap[current_victim_temp].address_space, coremap[current_victim_temp].virtual_address);
                //KASSERT(victim_segment != NULL);

                //seg_swap_out(victim_segment, swapfile_offset, coremap[current_victim_temp].virtual_address);

                /* Update coremap after swapping */
                KASSERT(coremap[current_victim_temp].entry_type == COREMAP_BUSY_USER);
                KASSERT(coremap[current_victim_temp].allocation_size == 1);

                coremap[current_victim_temp].virtual_address = associated_vaddr;
                coremap[current_victim_temp].address_space = current_as;
                new_victim = coremap[current_victim_temp].next_allocated;

                coremap[last_allocated_temp].next_allocated = current_victim_temp;
                coremap[current_victim_temp].next_allocated = invalid_reference;
                coremap[current_victim_temp].previous_allocated = last_allocated_temp;
                spinlock_release(&coremap_lock);

                spinlock_acquire(&page_replacement_lock);
                KASSERT(new_victim != invalid_reference);
                last_allocated_page = current_victim_temp;
                current_victim_page = new_victim;
                spinlock_release(&page_replacement_lock);
            }
        }
    }
    return address;
}

/*
 * Frees a user page and updates the linked list and coremap.
 * Removes the page from the allocation queue and marks it as freed.
 */
static void free_page_user(paddr_t paddr) {
    long last_allocated_new, current_victim_new;

    if (is_coremap_active()) {
        long page_index = paddr / PAGE_SIZE;
        KASSERT(total_ram_frames > page_index);
        KASSERT(coremap[page_index].allocation_size == 1);

        /* Capture old state for updating allocation queue */
        spinlock_acquire(&page_replacement_lock);
        last_allocated_new = last_allocated_page;
        current_victim_new = current_victim_page;
        spinlock_release(&page_replacement_lock);

        /* Update the allocation queue and coremap */
        spinlock_acquire(&coremap_lock);
        if (coremap[page_index].previous_allocated == invalid_reference) {
            if (coremap[page_index].next_allocated == invalid_reference) {
                current_victim_new = invalid_reference;
                last_allocated_new = invalid_reference;
            } else {
                KASSERT(page_index == current_victim_new);
                coremap[coremap[page_index].next_allocated].previous_allocated = invalid_reference;
                current_victim_new = coremap[page_index].next_allocated;
            }
        } else {
            if (coremap[page_index].next_allocated == invalid_reference) {
                KASSERT(page_index == last_allocated_new);
                coremap[coremap[page_index].previous_allocated].next_allocated = invalid_reference;
                last_allocated_new = coremap[page_index].previous_allocated;
            } else {
                coremap[coremap[page_index].next_allocated].previous_allocated = coremap[page_index].previous_allocated;
                coremap[coremap[page_index].previous_allocated].next_allocated = coremap[page_index].next_allocated;
            }
        }

        coremap[page_index].next_allocated = invalid_reference;
        coremap[page_index].previous_allocated = invalid_reference;
        spinlock_release(&coremap_lock);

        /* Free the page in coremap */
        free_pages(paddr, 1);

        spinlock_acquire(&page_replacement_lock);
        current_victim_page = current_victim_new;
        last_allocated_page = last_allocated_new;
        spinlock_release(&page_replacement_lock);
    }
}

/* Wrapper for user page allocation */
paddr_t alloc_user_page(vaddr_t vaddr) {
    paddr_t paddr;

    pagevm_can_sleep(); //assert we are in a context where sleeping is safe
    paddr = allocate_user_page(vaddr);
    return paddr;
}

/* Wrapper for user page deallocation */
void free_user_page(paddr_t paddr) {
    free_page_user(paddr);
}
