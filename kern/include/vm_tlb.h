/**
 * Authors: Michele Cazzola, Filippo Forte - 2024
 * TLB high-level handling, including replacement policy
 */

#ifndef _VM_TLB_H
#define _VM_TLB_H

/**
 * Functions description
 * 
 * VM_TLB_init: used to initialize TLB, needed at each process start and at bootstrap.
 * 
 * VM_TLB_get_victim_round_robin: executes replacement algorithm, returning victim index.
 * Needed on TLB misses, when necessary to load a new entry into TLB.
 * 
 * VM_TLB_peek_victim: reads TLB victim entry, without executing replacement.
 * Needed on TLB misses, when it is necessary to understand whether the selected entry
 * was valid (actual replacement) or not (simple write).
 * 
 * VM_TLB_write: wrapper for TLB_write, it works with virtual and physical addresses
 * instead of raw TLB entries.
 * 
 */

void vm_tlb_init(void);
unsigned int vm_tlb_get_victim_round_robin(void);
uint64_t vm_tlb_peek_victim(void);
void vm_tlb_write(vaddr_t vaddr, paddr_t paddr,/* unsigned char dirty,*/ unsigned int index);

#endif  /* _VM_TLB_H */