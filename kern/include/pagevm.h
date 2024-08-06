/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Memory allocator based on demand paging
 */

#ifndef _PAGEVM_H_
#define _PAGEVM_H_

/* Function prototypes */
void vm_bootstrap(void);
void vm_shutdown(void);
int vm_fault(int faulttype, vaddr_t faultaddress);
void pagevm_can_sleep(void);

#endif  /* _PAGEMVM_H_ */