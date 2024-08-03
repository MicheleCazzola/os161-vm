/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Segments handling, used to distinguish among code, data, stack
 */

#include <segment.h>
#include "opt-paging.h"

proc_segment_type *seg_create(void){

    return (proc_segment_type *)NULL;
}

int seg_define(proc_segment_type *proc_seg, size_t seg_size_bytes, off_t file_offset, vaddr_t base_vaddr,
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

int seg_define_stack(proc_segment_type *proc_seg, vaddr_t base_vaddr, size_t num_pages){
    
    (void)proc_seg;
    (void)base_vaddr;
    (void)num_pages;

    return 0;
}

int seg_prepare(proc_segment_type *proc_seg){
    
    (void)proc_seg;

    return 0;
}

int seg_copy(proc_segment_type *src, proc_segment_type **dest){
    
    (void)src;
    (void)dest;

    return 0;
}

paddr_t seg_get_paddr(proc_segment_type *proc_seg, vaddr_t vaddr){

    (void)proc_seg;
    (void)vaddr;

    return (paddr_t)0;
}

void seg_add_pt_entry(proc_segment_type *proc_seg, vaddr_t vaddr, paddr_t paddr){

    (void)proc_seg;
    (void)vaddr;
    (void)paddr;

    return;
}

int seg_load_page(proc_segment_type *proc_seg, vaddr_t vaddr, paddr_t paddr){

    (void)proc_seg;
    (void)vaddr;
    (void)paddr;

    return 0;
}

void seg_swap_out(proc_segment_type *proc_seg, off_t swapfile_offset, vaddr_t vaddr){

    (void)proc_seg;
    (void)swapfile_offset;
    (void)vaddr;

    return;
}

void seg_swap_in(proc_segment_type *proc_seg, vaddr_t vaddr, paddr_t paddr){

    (void)proc_seg;
    (void)vaddr;
    (void)paddr;

    return;
}

void seg_destroy(proc_segment_type *proc_seg){

    (void)proc_seg;

    return;
}