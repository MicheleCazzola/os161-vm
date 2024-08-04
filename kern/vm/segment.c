/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Segments handling, used to distinguish among code, data, stack
 */

#include <types.h>
#include <lib.h>
#include <segment.h>
#include "opt-paging.h"

ps_type *seg_create(void) {

    ps_type *proc_segment;

    proc_segment = (ps_type *)kmalloc(sizeof(ps_type));

    if (proc_segment == NULL) {
        return (ps_type *)NULL;
    }

    proc_segment->permissions = 0;
    proc_segment->seg_size_bytes = 0;
    proc_segment->file_offset = 0;
    proc_segment->base_vaddr = 0;
    proc_segment->num_pages = 0;
    proc_segment->seg_size_words = 0;
    proc_segment->elf_vnode = NULL;
    proc_segment->page_table = NULL;

    return proc_segment;
}

int seg_define(ps_type *proc_seg, size_t seg_size_bytes, off_t file_offset, vaddr_t base_vaddr,
                size_t num_pages, size_t seg_size_words, struct vnode *elf_vnode, char read, char write, char execute){

    (void)proc_seg;
    (void)seg_size_bytes;
    (void)file_offset;
    (void)base_vaddr;
    (void)num_pages;
    (void)seg_size_words;
    (void)elf_vnode;
    (void)read;
    (void)write;
    (void)execute;

    return 0;
}

int seg_define_stack(ps_type *proc_seg, vaddr_t base_vaddr, size_t num_pages){
    
    (void)proc_seg;
    (void)base_vaddr;
    (void)num_pages;

    return 0;
}

int seg_prepare(ps_type *proc_seg){
    
    (void)proc_seg;

    return 0;
}

int seg_copy(ps_type *src, ps_type **dest){
    
    (void)src;
    (void)dest;

    return 0;
}

paddr_t seg_get_paddr(ps_type *proc_seg, vaddr_t vaddr){

    (void)proc_seg;
    (void)vaddr;

    return (paddr_t)0;
}

void seg_add_pt_entry(ps_type *proc_seg, vaddr_t vaddr, paddr_t paddr){

    (void)proc_seg;
    (void)vaddr;
    (void)paddr;

    return;
}

int seg_load_page(ps_type *proc_seg, vaddr_t vaddr, paddr_t paddr){

    (void)proc_seg;
    (void)vaddr;
    (void)paddr;

    return 0;
}

void seg_swap_out(ps_type *proc_seg, off_t swapfile_offset, vaddr_t vaddr){

    (void)proc_seg;
    (void)swapfile_offset;
    (void)vaddr;

    return;
}

void seg_swap_in(ps_type *proc_seg, vaddr_t vaddr, paddr_t paddr){

    (void)proc_seg;
    (void)vaddr;
    (void)paddr;

    return;
}

void seg_destroy(ps_type *proc_seg){

    KASSERT(proc_seg != NULL);

    if (proc_seg->page_table != NULL) {
        pt_clear_content(proc_seg->page_table);
        pt_destroy(proc_seg->page_table);
    }

    kfree(proc_seg);

    return;
}