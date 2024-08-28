/**
 * Authors: Michele Cazzola - 2024
 * Statistics registration for memory handling
 */

#ifndef _VMSTATS_H_
#define _VMSTATS_H_

#define VMSTATS_NUM 10

enum vmstats_counters {
    VMSTAT_TLB_MISS,
    VMSTAT_TLB_MISS_FREE,
    VMSTAT_TLB_MISS_REPLACE,
    VMSTAT_TLB_INVALIDATION,
    VMSTAT_TLB_RELOAD,
    VMSTAT_PAGE_FAULT_ZERO,
    VMSTAT_PAGE_FAULT_DISK,
    VMSTAT_PAGE_FAULT_ELF,
    VMSTAT_PAGE_FAULT_SWAPFILE,
    VMSTAT_SWAPFILE_WRITE
};

/**
 * Functions description
 * 
 * VMSTATS_init: initializes statistics to 0. Invoked at bootstrap.
 * 
 * VMSTATS_increment: increments by one the statistic specified.
 * 
 * VMSTATS_show: shows (by printing on stdout) the statistics collected.
 * Invoked at shutdown.
 * 
 */

void vmstats_init(void);
void vmstats_increment(unsigned int stat_index);
void vmstats_show(void);

#endif  /* _VMSTATS_H_ */