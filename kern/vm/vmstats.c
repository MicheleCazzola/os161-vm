/**
 * Authors: Michele Cazzola - 2024
 * Statistics registration for memory handling
 */
#include <types.h>
#include <spl.h>
#include <synch.h>
#include <lib.h>
#include <vmstats.h>

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

/**
 * Shows all collected statistics, by printing them on the standard output.
 * Provides warning messages if statistics constraints are not respected.
 * Invoked at shutdown of virtual memory manager (on system shutdown)
 */
void vmstats_show() {
    unsigned long i;

    kprintf("--Virtual memory statistics--");
    for(i = 0; i < VMSTATS_NUM; i++) {
        kprintf("%s: %d", vmstats_names[i], vmstats_counts[i]);
    }

    if(vmstats_counts[0] != (vmstats_counts[1] + vmstats_counts[2])) {
        kprintf("Warning: sum of TLB faults with free and with replace not equal to number of TLB faults");
    }

    if(vmstats_counts[0] != (vmstats_counts[3] + vmstats_counts[5] + vmstats_counts[6])) {
        kprintf("Warning: sum of TLB reloads, zeroed-page faults and page faults from disk not equal to number of TLB faults");
    }

    if(vmstats_counts[6] != (vmstats_counts[7] + vmstats_counts[8])) {
        kprintf("Warning: sum of page faults from ELF and from swapfile not equal to number of page faults from disk");
    }
}