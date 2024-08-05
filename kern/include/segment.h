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
} seg_permissions_t;

/**
 * Data structure description
 * 
 * permissions: segment permissions, as defined by seg_permissions_t
 * seg_size_bytes: dimension (in bytes) of the segment in the corresponding ELF file
 * file_offset: offset (in bytes) of the segment (as its starting point) in the corresponding ELF file
 * base_vaddr: starting virtual address of the segment in the corresponding ELF file
 * num_pages: number of pages needed to store all the content of the segment (there could be internal fragmentation)
 * seg_size_words: length (in number of words) of the segment in the corresponding ELF file
 * elf_vnode: pointer to the vnode of the corresponding ELF file
 * page_table: pointer to the page table of the segment
 */

typedef struct  {
    seg_permissions_t permissions;
    size_t seg_size_bytes;
    off_t file_offset;
    vaddr_t base_vaddr;
    size_t num_pages;
    size_t seg_size_words;
    struct vnode *elf_vnode;
    pt_t *page_table;
} ps_t;

/**
 * Functions description
 * 
 * SEG_create: creates a new segment, with zeroed values
 * 
 * SEG_define: fills all the fields of a newly created segment with proper values.
 * Used only for text and data segment, not stack
 * 
 * SEG_define_stack: same as segment definition, but with special values for stack
 * It includes the functionalities implemented in segment preparation
 * 
 * SEG_prepare: creates and initializes page table for a segment. Used only for text
 * and data segment, not stack, after segment definition
 * 
 * SEG_copy: copies a given segment into another one, which is created inside
 * 
 * SEG_get_paddr:
 * 
 * SEG_add_pt_entry:
 * 
 * SEG_load_page:
 * 
 * SEG_swap_out:
 * 
 * SEG_swap_in:
 * 
 * SEG_destroy: destroys the segment, freeing all memory resources
 * 
 */

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