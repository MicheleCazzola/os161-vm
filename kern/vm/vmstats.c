/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Statistics registration for memory handling
 */
#include <types.h>
#include <spl.h>
#include <synch.h>
#include <lib.h>
#include <vmstats.h>
#include "opt-paging.h"

bool vmstats_active = false;
struct spinlock vmstats_lock;

unsigned int vmstats_counts[VMSTATS_NUM];

static const char *vmstats_names[VMSTATS_NUM] = {
    "TLB faults",                   /* 0: TLB misses */
    "TLB faults with free",         /* 1: TLB misses with no replacement */
    "TLB faults with replace",      /* 2: TLB misses with replacement */
    "TLB invalidations",            /* 3: TLB invalidations (not number of entries, only number of times) */
    "TLB reloads",                  /* 4: TLB misses for pages stored in memory */
    "Page faults (zeroed)",         /* 5: TLB misses that requires a new zero-filled page allocation */
    "Page faults (disk)",           /* 6: TLB misses that requires a page to be loaded from disk */
    "Page faults from ELF",         /* 7: page faults that require loading a page from ELF file */
    "Page faults from swapfile",    /* 8: page faults that require loading a page from swapfile */
    "Swapfile writes"               /* 9: page faults that require writing a page on swapfile */
};

/**
 * Constraints on statistics:
 * - TLB faults (0) = TLB faults with free (1) + TLB faults with replacement (2)
 * - TLB faults (0) = TLB reloads (3) + Page faults (zeroed) (5) + Page faults (disk) (6)
 * - Page faults (disk) (6) = Page faults from ELF (7) + Page faults from swapfile (8) 
 */

/**
 * Statistics initialization and vmstats activation
 */
void vmstats_init() {
    unsigned long i;

    spinlock_acquire(&vmstats_lock);

    for(i = 0; i < VMSTATS_NUM; i++) {
        vmstats_counts[i] = 0;
    }
    vmstats_active = true;

    spinlock_release(&vmstats_lock);
}

/**
 * Increments a specific statistic, if vmstats is already active
 */
void vmstats_increment(unsigned int stat_index) {

    KASSERT(stat_index < VMSTATS_NUM);

    spinlock_acquire(&vmstats_lock);

    if (vmstats_active) {
        vmstats_counts[stat_index]++;
    }

    spinlock_release(&vmstats_lock);
}

void vmstats_show() {
    (void)vmstats_names;
}