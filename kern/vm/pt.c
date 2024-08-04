/*
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte
 * Page table handling module
 */

#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <pt.h>
#include "opt-paging.h"

/**
 * Performs page buffer copy from a page table to another
 */
static void pt_page_buffer_copy(pt_type *src, pt_type *dest) {
    unsigned long i;

    for(i = 0; i < src->num_pages; i++) {
        dest->page_buffer[i] = src->page_buffer[i];
    }
}

/**
 * Creates a new page table and initializes all its entries to empty
 */
pt_type *pt_create(unsigned long num_pages, vaddr_t base_address) {

    unsigned long i;
    pt_type *page_table;
    paddr_t *page_buffer;

    /**
     * Page table allocation
     */
    page_table = (pt_type *)kmalloc(sizeof(pt_type));

    if (page_table == NULL) {
        return (pt_type *)NULL;
    }

    page_table->num_pages = num_pages;
    page_table->base_vaddr = base_address;
    
    /**
     * Page buffer allocation
     */
    page_buffer = (paddr_t *)kmalloc(sizeof(paddr_t) * num_pages);

    if (page_buffer == NULL) {
        return (pt_type *)NULL;
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
int pt_copy(pt_type *src, pt_type **dest){

    pt_type *new_page_table;

    KASSERT(src != NULL);

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

    if (dest != NULL) {
        *dest = new_page_table;
    }
    else {  /* Caller invokes pt_copy with NULL destination address */
        return EINVAL;
    }

    return 0;
}

paddr_t pt_get_entry(pt_type *pt, vaddr_t vaddr){

    (void)pt;
    (void)vaddr;

    return (paddr_t)0;
}

void pt_add_entry(pt_type *pt, vaddr_t vaddr, paddr_t paddr){

    (void)pt;
    (void)vaddr;
    (void)paddr;
}

void pt_clear_content(pt_type *pt) {

    (void)pt;
}

void pt_swap_out(pt_type *pt, off_t swapfile_offset, vaddr_t vaddr){

    (void)pt;
    (void)swapfile_offset;
    (void)vaddr;
}

void pt_swap_in(pt_type *pt, vaddr_t vaddr, paddr_t paddr){

    (void)pt;
    (void)vaddr;
    (void)paddr;
}

off_t pt_get_swap_offset(pt_type *pt, vaddr_t vaddr){

    (void)pt;
    (void)vaddr;

    return (off_t)0;
}

void pt_destroy(pt_type *pt) {

    KASSERT(pt != NULL);
    KASSERT(pt->page_buffer != NULL);

    kfree(pt->page_buffer);
    kfree(pt);
}