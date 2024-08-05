/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Segments handling, used to distinguish among code, data, stack
 */

#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include <types.h>
#include <pt.h>

/* More options should be defined to cover all possible cases, but here these are sufficient */
typedef enum {
    PAGE_RONLY,     /* 0: read-only */
    PAGE_RW,        /* 1: read-write */
    PAGE_EXE,       /* 2: executable */
    PAGE_STACK      /* 3: stack */
} page_permissions_t;

typedef struct  {
    page_permissions_t permissions;
    size_t seg_size_bytes;
    off_t file_offset;
    vaddr_t base_vaddr;
    size_t num_pages;
    size_t seg_size_words;
    struct vnode *elf_vnode;
    pt_t *page_table;
} ps_t;

ps_t *seg_create(void);
int seg_define(ps_t *proc_seg, size_t seg_size_bytes, off_t file_offset, vaddr_t base_vaddr,
                size_t num_pages, size_t seg_size_words, struct vnode *elf_vnode, char read, char write, char execute);
int seg_define_stack(ps_t *proc_seg, vaddr_t base_vaddr, size_t num_pages);
int seg_prepare(ps_t *proc_seg);
int seg_copy(ps_t *src, ps_t **dest);
paddr_t seg_get_paddr(ps_t *proc_seg, vaddr_t vaddr);
void seg_add_pt_entry(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
int seg_load_page(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_swap_out(ps_t *proc_seg, off_t swapfile_offset, vaddr_t vaddr);
void seg_swap_in(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_destroy(ps_t *proc_seg);

#endif  /* _SEGMENT_H_ */