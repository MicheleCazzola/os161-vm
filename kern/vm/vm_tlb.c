/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte
 * TLB high-level handling, including replacement policy
 */

#include <types.h>
#include <vm.h>
#include <mips/tlb.h>
#include <vm_tlb.h>
#include "opt-paging.h"

static unsigned int current_victim;

/**
 * Invalidates all TLB entries
 */
static void vm_tlb_invalidate_entries() {
    int i;

    for(i = 0; i < NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
}

/**
 * Initializes (or resets) replacement algorithm parameter
 */
static void vm_tlb_init_current_victim() {
    current_victim = 0;
}

/**
 * Initializes TLB, both for entry invalidation and
 * round-robin replacement algorithm parameter 
 */
void vm_tlb_init() {
    vm_tlb_invalidate_entries();
    vm_tlb_init_current_victim();
}

/**
 * Round-robin replacement algorithm execution: current victim is
 * selected, and algorithm parameter is modified, following the policy used
 */
unsigned int vm_tlb_get_victim_round_robin() {

    unsigned int victim;

    victim = current_victim;
    current_victim = (current_victim + 1) % NUM_TLB;

    return victim;
}

/**
 * Retrieves current victim without replacing it
 */
uint64_t vm_tlb_peek_victim() {
    uint64_t victim_content = 0;
    uint32_t entry_hi, entry_lo;

    tlb_read(&entry_hi, &entry_lo, current_victim);

    victim_content = (victim_content & entry_hi) << 32;
    victim_content &= entry_lo;

    return victim_content;
}

/**
 * Writes a TLB entry into a specified position
 * NOTE: to decide whether consider dirty bit handling or not
 */
void vm_tlb_write(vaddr_t vaddr, paddr_t paddr,/* unsigned char dirty,*/ unsigned int index) {

    uint32_t entry_hi, entry_lo;

    entry_hi = vaddr & PAGE_FRAME;
    entry_lo = (paddr & TLBLO_VALID);

    /*
    if (dirty) {
        entry_lo |= TLBLO_DIRTY;
    }
    */

    tlb_write(entry_hi, entry_lo, index);
}