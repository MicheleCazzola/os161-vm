/*
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte
 * Page table handling module
 */

#include <types.h>
#include <pt.h>
#include "opt-paging.h"

page_table_type *pt_create(unsigned long num_pages, vaddr_t base_address){
    
    (void)num_pages;
    (void)base_address;

    return (page_table_type *)NULL;
}

int pt_copy(page_table_type *src, page_table_type **dest){
    (void)src;
    (void)dest;

    return 0;
}

paddr_t pt_get_entry(page_table_type *pt, vaddr_t vaddr){

    (void)pt;
    (void)vaddr;

    return (paddr_t)0;
}

void pt_add_entry(page_table_type *pt, vaddr_t vaddr, paddr_t paddr){

    (void)pt;
    (void)vaddr;
    (void)paddr;
}

void pt_clear_content(page_table_type *pt){

    (void)pt;
}

void pt_swap_out(page_table_type *pt, off_t swapfile_offset, vaddr_t vaddr){

    (void)pt;
    (void)swapfile_offset;
    (void)vaddr;
}

void pt_swap_in(page_table_type *pt, vaddr_t vaddr, paddr_t paddr){

    (void)pt;
    (void)vaddr;
    (void)paddr;
}

off_t pt_get_swap_offset(page_table_type *pt, vaddr_t vaddr){

    (void)pt;
    (void)vaddr;

    return (off_t)0;
}

void pt_destroy(page_table_type *pt){

    (void)pt;
}