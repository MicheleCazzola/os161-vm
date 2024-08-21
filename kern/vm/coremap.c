/**
 * Authors: Leone Fabio - 2024
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
static bool is_coremap_initialized = false; /* Flag to check if coremap is initialized */

/* Linked list variables for page replacement strategy */
static unsigned long invalid_reference = 0; /* Indicates an invalid reference */
static unsigned long last_allocated_page = 0; /* Refers to the last allocated page */
static unsigned long current_victim_page = 0; /* Refers to the current page selected for replacement */

/*
 * Checks if the coremap is initialized by examining the is_coremap_initialized 'shared' variable.
 * Returns 1 if initialized, 0 otherwise.
 */
static bool is_coremap_active() {
    bool active;
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
    is_coremap_initialized = true;
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
        coremap[found_block_start].allocation_size = npages; //if it is not busy for kernel processes, it is useless to keep track of allocation size (only kernel requests for multiple contiguous pages)
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
        paddr_t physical_address = addr - MIPS_KSEG0; // MIPS_KSEG0 is the base address of the direct-mapped segment in the architecture.
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
    addrspace_t *current_as;  // Pointer to the current address space of the process
    ps_t *victim_ps;
    paddr_t address;  // Physical address of the page to be allocated
    unsigned long last_allocated_temp, current_victim_temp, new_victim;
    off_t swapfile_offset;  // Offset in the swap file where the page will be swapped out
    int result;  // Result code for the swap-out operation

    // Retrieve the current address space of the process
    current_as = proc_getas();
    KASSERT(current_as != NULL);  // Ensure the address space is correctly initialized
    KASSERT((associated_vaddr & PAGE_FRAME) == associated_vaddr);  // Ensure the virtual address is page-aligned

    // Attempt to allocate a free page from the coremap
    address = allocate_free_pages(1, COREMAP_BUSY_USER, current_as, associated_vaddr);
    if (address == 0) {
        // If no free pages are available, acquire memory by calling ram_stealmem()
        spinlock_acquire(&stealmem_lock);  
        address = ram_stealmem(1);  // Request a page from the system
        spinlock_release(&stealmem_lock);  
    }

    // If the coremap is active, update its state
    if (is_coremap_active()) {
        spinlock_acquire(&page_replacement_lock);  
        last_allocated_temp = last_allocated_page;  // Save the index of the last allocated page
        current_victim_temp = current_victim_page;  // Save the index of the current victim page
        spinlock_release(&page_replacement_lock);  

        if (address != 0) {
            long page_index = address / PAGE_SIZE;  // Compute the index of the page in the coremap
            KASSERT(total_ram_frames > page_index);  // Ensure the page index is within valid range

            // Update coremap to reflect the newly allocated page
            spinlock_acquire(&coremap_lock);  // Acquire lock to ensure exclusive access to coremap
            //if (coremap[page_index].entry_type == COREMAP_FREED) {
                // The page was previously freed; we can now allocate it ???
                coremap[page_index].entry_type = COREMAP_BUSY_USER;  
                coremap[page_index].allocation_size = 1;  
                coremap[page_index].address_space = current_as;  
                coremap[page_index].virtual_address = associated_vaddr;  

                // Update the linked list of allocated pages
                if (last_allocated_temp != invalid_reference) {
                    // There were previously allocated pages
                    coremap[last_allocated_temp].next_allocated = page_index;  // Link the last allocated page to the new page
                    coremap[page_index].previous_allocated = last_allocated_temp;  // Set the new page's previous link
                    coremap[page_index].next_allocated = invalid_reference;  // The new page has no further links
                } else {
                    // This is the first allocated page
                    coremap[page_index].previous_allocated = invalid_reference;  // No previous page
                    coremap[page_index].next_allocated = invalid_reference;  // No next page
                }

                spinlock_release(&coremap_lock);  

                // Update page replacement tracking
                spinlock_acquire(&page_replacement_lock);  
                if (current_victim_temp == invalid_reference) {
                    // This is the only page in the coremap, so it becomes the current victim page
                    current_victim_page = page_index;
                }
                last_allocated_page = page_index;  // Update the index of the last allocated page
                spinlock_release(&page_replacement_lock);  
            //}
        } 
        else {
            // SWAP OUT
            // The page was not free, so we need to swap out a page
            address = (paddr_t)current_victim_temp * PAGE_SIZE;  // Convert victim page index to physical address
            result = swap_out(address, &swapfile_offset);  // Swap out the victim page to the swap file
            if (result) {
                panic("Failed to swap out a page to file");  
            }

            // Update coremap to reflect the swapped-out page
            spinlock_acquire(&coremap_lock);

            // Find the segment in the address space that corresponds to the page being considered for swapping out.
            // `coremap[current_victim_temp].address_space` provides the address space of the page being considered.
            // `coremap[current_victim_temp].virtual_address` gives the virtual address of that page.
            // The `as_find_segment` function  searches through the address space to find the corresponding segment
            // that contains this virtual address.
            victim_ps = as_find_segment(coremap[current_victim_temp].address_space, coremap[current_victim_temp].virtual_address);

            // Ensure that the segment pointer `victim_ps` is not NULL, i.e., that the segment containing the virtual address
            // was successfully found in the address space. If `victim_ps` is NULL, it indicates an error, possibly
            // because the virtual address does not belong to any segment in the given address space.
            KASSERT(victim_ps != NULL);

            // Perform the swap out operation for the identified segment. 
            // The `seg_swap_out` function writes the page to the swapfile, using the provided offset (`swapfile_offset`)
            // in the swapfile and the virtual address (`coremap[current_victim_temp].virtual_address`) of the page being swapped out.
            // This step is where the actual data of the page is moved from RAM to the swapfile.
            seg_swap_out(victim_ps, swapfile_offset, coremap[current_victim_temp].virtual_address);


            KASSERT(coremap[current_victim_temp].entry_type == COREMAP_BUSY_USER);  // Ensure the victim page was in use
            KASSERT(coremap[current_victim_temp].allocation_size == 1);  // Ensure the victim page size is correct

            // Update the coremap entry for the swapped-out page
            coremap[current_victim_temp].virtual_address = associated_vaddr;  
            coremap[current_victim_temp].address_space = current_as;  
            new_victim = coremap[current_victim_temp].next_allocated;  // Save the index of the next victim

            // Update the linked list of allocated pages
            coremap[last_allocated_temp].next_allocated = current_victim_temp;  // Link the last allocated page to the victim page
            coremap[current_victim_temp].next_allocated = invalid_reference;  // The victim page has no further links
            coremap[current_victim_temp].previous_allocated = last_allocated_temp;  // Set the victim page's previous link

            spinlock_release(&coremap_lock);  

            // Update page replacement tracking after swapping
            spinlock_acquire(&page_replacement_lock);  
            KASSERT(new_victim != invalid_reference);  // Ensure there is a valid next victim
            last_allocated_page = current_victim_temp;  // Update the last allocated page to be the victim page
            current_victim_page = new_victim;  // Update the current victim page index
            spinlock_release(&page_replacement_lock);  
        }
    }
    return address;  // Return the physical address of the allocated page
}

/*
 * Frees a user page and updates the linked list and coremap.
 * Removes the page from the allocation queue and marks it as freed.
 */
static void free_page_user(paddr_t paddr) {
    long last_allocated_new, current_victim_new;  // Temporary variables for the new state of the allocation queue

    if (is_coremap_active()) {
        long page_index = paddr / PAGE_SIZE;  // Compute the index of the page in the coremap
        KASSERT(total_ram_frames > page_index);  // Ensure the page index is within valid bounds
        KASSERT(coremap[page_index].allocation_size == 1);  // Ensure the page size is 1

        // Capture the current state for updating the allocation queue
        spinlock_acquire(&page_replacement_lock);  
        last_allocated_new = last_allocated_page;  // Save the index of the last allocated page
        current_victim_new = current_victim_page;  // Save the index of the current victim page
        spinlock_release(&page_replacement_lock);  

        // Update the allocation queue and coremap
        spinlock_acquire(&coremap_lock);  
        if (coremap[page_index].previous_allocated == invalid_reference) {
            // The page is the first in the allocation queue
            if (coremap[page_index].next_allocated == invalid_reference) {
                // The page is the only page in the queue
                current_victim_new = invalid_reference;  // No more victim pages
                last_allocated_new = invalid_reference;  // No more allocated pages
            } else {
                // Update the queue to remove the page
                KASSERT(page_index == current_victim_new);  // The page being freed is the current victim
                coremap[coremap[page_index].next_allocated].previous_allocated = invalid_reference;  // Update the next page's previous link
                current_victim_new = coremap[page_index].next_allocated;  // Set the new current victim page
            }
        } else {
            if (coremap[page_index].next_allocated == invalid_reference) {
                // The page is the last in the allocation queue
                KASSERT(page_index == last_allocated_new);  // The page being freed is the last allocated page
                coremap[coremap[page_index].previous_allocated].next_allocated = invalid_reference;  // Update the previous page's next link
                last_allocated_new = coremap[page_index].previous_allocated;  // Set the new last allocated page
            } else {
                // The page is in the middle of the allocation queue
                coremap[coremap[page_index].next_allocated].previous_allocated = coremap[page_index].previous_allocated;  // Update links
                coremap[coremap[page_index].previous_allocated].next_allocated = coremap[page_index].next_allocated;
            }
        }

        // Mark the page as freed in the coremap
        coremap[page_index].next_allocated = invalid_reference;  // No further links in the queue
        coremap[page_index].previous_allocated = invalid_reference;  // No previous links in the queue
        spinlock_release(&coremap_lock);  

        // Free the page
        free_pages(paddr, 1);  // Mark the page as available

        // Update the page replacement tracking
        spinlock_acquire(&page_replacement_lock); 
        current_victim_page = current_victim_new;  // Update the current victim page
        last_allocated_page = last_allocated_new;  // Update the last allocated page
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
