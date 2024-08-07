/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte
 * Coremap handling, used to track freed frames
 */

#ifndef _COREMAP_H_
#define _COREMAP_H_

void coremap_init(void);
void coremap_shutdown(void);
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);
paddr_t alloc_upage(vaddr_t vaddr);
void free_upage(paddr_t paddr);

#endif  /* _COREMAP_H_ */