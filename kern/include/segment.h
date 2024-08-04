/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Segments handling, used to distinguish among code, data, stack
 */

#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include <types.h>
#include <pt.h>

/* More options should be defined to cover all possible cases, but here these are sufficient */
#define PAGE_RONLY  0
#define PAGE_RW     1
#define PAGE_EXE    2
#define PAGE_STACK  3

typedef struct  {
    char permissions;
    size_t seg_size_bytes;
    off_t file_offset;
    vaddr_t base_vaddr;
    size_t num_pages;
    size_t seg_size_words;
    struct vnode *elf_vnode;
    pt_type *page_table;
} ps_type;

ps_type *seg_create(void);
int seg_define(ps_type *proc_seg, size_t seg_size_bytes, off_t file_offset, vaddr_t base_vaddr,
                size_t num_pages, size_t seg_size_words, struct vnode *elf_vnode, char read, char write, char execute);
int seg_define_stack(ps_type *proc_seg, vaddr_t base_vaddr, size_t num_pages);
int seg_prepare(ps_type *proc_seg);
int seg_copy(ps_type *src, ps_type **dest);
paddr_t seg_get_paddr(ps_type *proc_seg, vaddr_t vaddr);
void seg_add_pt_entry(ps_type *proc_seg, vaddr_t vaddr, paddr_t paddr);
int seg_load_page(ps_type *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_swap_out(ps_type *proc_seg, off_t swapfile_offset, vaddr_t vaddr);
void seg_swap_in(ps_type *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_destroy(ps_type *proc_seg);

#endif  /* _SEGMENT_H_ */