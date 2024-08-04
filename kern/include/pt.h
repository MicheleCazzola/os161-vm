/*
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte
 * Page table handling 
 */

#ifndef _PT_H_
#define _PT_H_

#include <types.h>

/* Physical 0 is not valid -> Used to mark empty entry */
#define PT_EMPTY_ENTRY      0

/* 
 *  Physical 1 is not valid -> Used to mark (as LSB) swapped entry
 *  The mask is used to get last bit, or higher ones (if negated)
 *  which contain the offset of the page in the swapfile
 */
#define PT_SWAPPED_ENTRY    1
#define PT_SWAPPED_MASK     0x00000001

typedef struct {
    unsigned long num_pages;
    vaddr_t base_vaddr;
    paddr_t *page_buffer;
} pt_type;

pt_type *pt_create(unsigned long num_pages, vaddr_t base_address);
int pt_copy(pt_type *src, pt_type **dest);
paddr_t pt_get_entry(pt_type *pt, vaddr_t vaddr);
void pt_add_entry(pt_type *pt, vaddr_t vaddr, paddr_t paddr);
void pt_clear_content(pt_type *pt);
void pt_swap_out(pt_type *pt, off_t swapfile_offset, vaddr_t vaddr);
void pt_swap_in(pt_type *pt, vaddr_t vaddr, paddr_t paddr);
off_t pt_get_swap_offset(pt_type *pt, vaddr_t vaddr);
void pt_destroy(pt_type *pt);

#endif  /* _PT_H_ */