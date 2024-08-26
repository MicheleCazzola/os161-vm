/**
 * Authors: Michele Cazzola, Filippo Forte - 2024
 * TLB high-level handling, including replacement policy
 */

#ifndef _VM_TLB_H
#define _VM_TLB_H

#include <types.h>

/**
 * Functions description
 * 
 * VM_TLB_invalidate_entries: used to initialize TLB, needed at each process start.
 * 
 * VM_TLB_reset_current_victim: used to reset to 0 the index of the TLB current victim in the round
 * robin algorithm, needed at the VM bootstrap.
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

void vm_tlb_invalidate_entries(void);
void vm_tlb_reset_current_victim(void);
uint64_t vm_tlb_peek_victim(void);
void vm_tlb_write(vaddr_t vaddr, paddr_t paddr, unsigned char dirty);

#endif  /* _VM_TLB_H */