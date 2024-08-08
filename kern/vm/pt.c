/*
 * Authors: Michele Cazzola - 2024
 * Page table handling module
 */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <vm.h>
#include <swapfile.h>
#include <coremap.h>
#include <pt.h>

/**
 * Performs page buffer copy from a page table to another
 */
static void pt_page_buffer_copy(pt_t *src, pt_t *dest) {
    unsigned long i;

    for(i = 0; i < src->num_pages; i++) {
        dest->page_buffer[i] = src->page_buffer[i];
    }
}

/**
 * Retrieves entry index, given virtual address
 */
static unsigned long pt_get_entry_index(pt_t *pt, vaddr_t vaddr) {

    vaddr_t page_vaddr;
    unsigned long index;

    /**
     * Given virtual address, its page is loaded at entry position
     * in page aligned way, so that each entry is stored in a specific page
     * and conversion from page virtual address to page table index is easy
     */
    page_vaddr = vaddr & PAGE_FRAME;
    index = (page_vaddr - pt->base_vaddr) / PAGE_SIZE;

    /* Boundary check for computed index */
    KASSERT(index < pt->num_pages);

    return index;
}


/**
 * Creates a new page table and initializes all its entries to empty
 */
pt_t *pt_create(unsigned long num_pages, vaddr_t base_address) {

    unsigned long i;
    pt_t *page_table;
    paddr_t *page_buffer;

    /**
     * Page table allocation
     */
    page_table = (pt_t *)kmalloc(sizeof(pt_t));

    if (page_table == NULL) {
        return (pt_t *)NULL;
    }

    page_table->num_pages = num_pages;
    page_table->base_vaddr = base_address;
    
    /**
     * Page buffer allocation
     */
    page_buffer = (paddr_t *)kmalloc(sizeof(paddr_t) * num_pages);

    if (page_buffer == NULL) {
        return (pt_t *)NULL;
    }

    page_table->page_buffer = page_buffer;

    /**
     * Buffer initialization to empty entries
     */
    for(i = 0; i < page_table->num_pages; i++) {
        page_table->page_buffer[i] = PT_EMPTY_ENTRY;
    }
    
    return page_table;
}

/**
 * Copies a page table into another one
 */
int pt_copy(pt_t *src, pt_t **dest){

    pt_t *new_page_table;

    KASSERT(src != NULL);
    KASSERT(dest != NULL);

    /**
     * New page table creation and initialization to empty
     */
    new_page_table = pt_create(src->num_pages, src->base_vaddr);

    if (new_page_table == NULL) {
        return ENOMEM;
    }

    /**
     * Page table copy to destination
     */
    new_page_table->num_pages = src->num_pages;
    new_page_table->base_vaddr = src->base_vaddr;
    pt_page_buffer_copy(new_page_table, src);

    *dest = new_page_table;

    return 0;
}

/**
 * Retrieves physical address from virtual address
 */
paddr_t pt_get_entry(pt_t *pt, vaddr_t vaddr) {

    unsigned long index;
    paddr_t page_paddr;

    KASSERT(pt != NULL);
    KASSERT(pt->page_buffer != NULL);

    index = pt_get_entry_index(pt, vaddr);

    /**
     * Entry is not already populated
     */
    if (pt->page_buffer[index] == PT_EMPTY_ENTRY) {
        page_paddr = PT_EMPTY_ENTRY;
    }
    /**
     * Entry has been already populated, but its page is currently swapped
     */
    else if ((pt->page_buffer[index] & PT_SWAPPED_MASK) == PT_SWAPPED_ENTRY) {
        page_paddr = PT_SWAPPED_ENTRY;
    }
    /**
     * Entry has been already populated, and its page is currently in memory
     */
    else {
        page_paddr = pt->page_buffer[index];
    }

    return page_paddr;
}

/**
 * Inserts a new entry (virtual page address, physical address) into page table
 */
void pt_add_entry(pt_t *pt, vaddr_t vaddr, paddr_t paddr) {

    unsigned long index;

    KASSERT(pt != NULL);
    KASSERT(pt->page_buffer != NULL);

    index = pt_get_entry_index(pt, vaddr);

    /**
     * Content check: entry cannot be already populated with physical page in memory
     */
    KASSERT(pt->page_buffer[index] == PT_EMPTY_ENTRY || (pt->page_buffer[index] & PT_SWAPPED_MASK) == PT_SWAPPED_ENTRY);

    pt->page_buffer[index] = paddr;
}

/**
 * Clears the content of page table, basing on the content of each entry
 */
void pt_clear_content(pt_t *pt) {

    unsigned long i;

    KASSERT(pt != NULL);

    for(i = 0; i < pt->num_pages; i++) {
        /**
         * Swapped page: cleared from swapfile
         */
        if((pt->page_buffer[i] & PT_SWAPPED_MASK) == PT_SWAPPED_ENTRY) {
            swap_free(pt->page_buffer[i] & (~PT_SWAPPED_MASK));
        }
        /**
         * Page stored in memory: memory is freed
         */
        else if(pt->page_buffer[i] != PT_EMPTY_ENTRY) {
            free_user_page(pt->page_buffer[i]);
        }
    }
}

/**
 * Marks as swapped the entry of the page corresponding to the given
 * virtual address, and saves the swapfile offset of the page in its entry
 */
void pt_swap_out(pt_t *pt, off_t swapfile_offset, vaddr_t vaddr) {

    unsigned long index;
    
    KASSERT(pt != NULL);
    KASSERT(pt->page_buffer != NULL);

    index = pt_get_entry_index(pt, vaddr);

    /**
     * Content check: entry to swap out must be already populated
     * and with its page stored in memory
     */
    KASSERT(pt->page_buffer[index] != PT_EMPTY_ENTRY);
    KASSERT((pt->page_buffer[index] & PT_SWAPPED_MASK) != PT_SWAPPED_ENTRY);

    pt->page_buffer[index] = swapfile_offset | PT_SWAPPED_MASK;
}

/**
 * Dual of pt_swap_out, it is a wrapper of pt_add_entry
 */
void pt_swap_in(pt_t *pt, vaddr_t vaddr, paddr_t paddr) {
    pt_add_entry(pt, vaddr, paddr);
}

/**
 * Retrieves swapfile offset from the entry corresponding to the given
 * virtual address
 */
off_t pt_get_swap_offset(pt_t *pt, vaddr_t vaddr) {

    unsigned long index;
    off_t swapfile_offset;

    KASSERT(pt != NULL);
    KASSERT(pt->page_buffer != NULL);

    index = pt_get_entry_index(pt, vaddr);

    /**
     * Content check: entry must correspond to a swapped page
     */
    KASSERT((pt->page_buffer[index] & PT_SWAPPED_MASK) == PT_SWAPPED_ENTRY);

    swapfile_offset = pt->page_buffer[index] & (~PT_SWAPPED_MASK);

    return swapfile_offset;
}

/**
 * Destroys the given page table, freeing all memory resources
 */
void pt_destroy(pt_t *pt) {

    KASSERT(pt != NULL);
    KASSERT(pt->page_buffer != NULL);

    kfree(pt->page_buffer);
    kfree(pt);
}