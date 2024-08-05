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
static void pt_page_buffer_copy(pt_t *src, pt_t *dest) {
    unsigned long i;

    for(i = 0; i < src->num_pages; i++) {
        dest->page_buffer[i] = src->page_buffer[i];
    }
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

paddr_t pt_get_entry(pt_t *pt, vaddr_t vaddr){

    (void)pt;
    (void)vaddr;

    return (paddr_t)0;
}

void pt_add_entry(pt_t *pt, vaddr_t vaddr, paddr_t paddr){

    (void)pt;
    (void)vaddr;
    (void)paddr;
}

void pt_clear_content(pt_t *pt) {

    (void)pt;
}

void pt_swap_out(pt_t *pt, off_t swapfile_offset, vaddr_t vaddr){

    (void)pt;
    (void)swapfile_offset;
    (void)vaddr;
}

void pt_swap_in(pt_t *pt, vaddr_t vaddr, paddr_t paddr){

    (void)pt;
    (void)vaddr;
    (void)paddr;
}

off_t pt_get_swap_offset(pt_t *pt, vaddr_t vaddr){

    (void)pt;
    (void)vaddr;

    return (off_t)0;
}

void pt_destroy(pt_t *pt) {

    KASSERT(pt != NULL);
    KASSERT(pt->page_buffer != NULL);

    kfree(pt->page_buffer);
    kfree(pt);
}