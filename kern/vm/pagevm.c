/**
 * Authors: Filippo Forte - 2024
 * Memory allocator based on demand paging
 */

#include <types.h>
#include <pagevm.h>
#include <vmstats.h>
#include <swapfile.h>
#include <coremap.h>
#include <vm_tlb.h>
#include <vm.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <pt.h>
#include <spl.h>
#include <mips/tlb.h>
#include <kern/errno.h>
#include "opt-paging.h"

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in pagevm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * pagevm starts blowing up during the VM assignment.
 */

void pagevm_can_sleep(void)
{
    if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

/* Chiamata alla fine del processo di avvio per inizializzare 
 * le strutture dati necessarie per pagevm.
 * 
 * Called at the end of the boot process to initialize 
 * required data structures for pagevm.
 */
void vm_bootstrap(void)
{
    vm_tlb_reset_current_victim();
    coremap_init();
    swap_init();
    vmstats_init();
}


/* Spegne il sistema VM, rilasciando risorse e stampando statistiche.
 * 
 * Shuts down the VM system, releasing resources and printing statistics.
 */
void vm_shutdown(void)
{
    swap_shutdown();
    coremap_shutdown();
    vmstats_show();
}

/* 
 * Handles TLB misses. Called when a page is in the page table 
 * but not in the TLB or needs to be loaded from disk.
 */
int vm_fault(int faulttype, vaddr_t faultaddress)
{
    addrspace_t *as;
    ps_t *process_segment;
    paddr_t physical_address;
    vaddr_t page_aligned_fault_address = faultaddress & PAGE_FRAME;
    char unpopulated;
    int result, spl;
    uint64_t peek;

    // Ensure the fault type is valid: it must be read, write, or readonly
    if((faulttype != VM_FAULT_READONLY) && (faulttype != VM_FAULT_READ) && (faulttype != VM_FAULT_WRITE)){
        return EINVAL;  // Invalid argument error
    }

    /* 
     * Handle read-only faults.
     * This happens when a write is attempted on a page marked as read-only.
     * In OS161, if the dirty bit is 0, the page is writable. If a write request
     * is made and the dirty bit is 0, but the page is not actually writable, 
     * a read-only fault occurs. This is an access violation.
     */
    if(faulttype == VM_FAULT_READONLY){
        return EACCES;  // Access violation error
    }

    // Ensure the current process exists
    if(curproc == NULL){
        return EFAULT;  // Fault error
    }

    // Get the address space of the current process
    as = proc_getas();
    if(as == NULL){
        return EFAULT;  // Fault error
    }

    // Find the segment within the address space that contains the fault address
    process_segment = as_find_segment(as, faultaddress);
    if(process_segment == NULL){
        return EFAULT;  // Fault error, no segment found for the address
    }

    // Get the physical address associated with the fault address in the segment
    physical_address = seg_get_paddr(process_segment, faultaddress);

    unpopulated = 0;

    // Handle the case where the page table entry is empty (page not yet allocated)
    if(physical_address == PT_EMPTY_ENTRY){

        // Allocate a new physical page for the fault address
        physical_address = alloc_user_page(page_aligned_fault_address);
        // Add the newly allocated page to the page table
        seg_add_pt_entry(process_segment, faultaddress, physical_address);

        // If the segment is for the stack, zero out the page
        if(process_segment->permissions == PAGE_STACK){
            bzero((void *)PADDR_TO_KVADDR(physical_address), PAGE_SIZE);
            vmstats_increment(VMSTAT_PAGE_FAULT_ZERO);  // Increment zero-filled page fault counter
        }

        unpopulated = 1;  // Mark as a newly populated page
    }

    // Handle the case where the page is swapped out (page needs to be loaded from disk)
    else if(physical_address == PT_SWAPPED_ENTRY){

        // Allocate a new physical page
        physical_address = alloc_user_page(page_aligned_fault_address);
        // Load the page from swap into the new physical page
        seg_swap_in(process_segment, faultaddress, physical_address);
    }
    else{
        // If the page was already allocated, just reload it into the TLB
        vmstats_increment(VMSTAT_TLB_RELOAD);  // Increment TLB reload counter
    }

    // Ensure the physical address is page-aligned
    KASSERT((physical_address & PAGE_FRAME) == physical_address);

    // If it's not a stack page and the page was just populated, load the page from file
    if (process_segment->permissions != PAGE_STACK && unpopulated) {
        result = seg_load_page(process_segment, faultaddress, physical_address);
        if (result)
            return EFAULT;  // Fault error if page loading fails
    }

    // Disable interrupts to safely update the TLB
    spl = splhigh();

    // Get the TLB entry that will be replaced (victim selection)
    peek = vm_tlb_peek_victim();

    // Check if the victim entry is valid
    if(peek & (TLBLO_VALID*1ULL)){
        vmstats_increment(VMSTAT_TLB_MISS_REPLACE);  // Increment TLB miss replace counter
    }else{
        vmstats_increment(VMSTAT_TLB_MISS_FREE);  // Increment TLB miss free counter
    }

    // Increment TLB miss counter
    vmstats_increment(VMSTAT_TLB_MISS);

    // Write the new mapping into the TLB
    vm_tlb_write(faultaddress, physical_address, 
                 process_segment->permissions == PAGE_RW || process_segment->permissions == PAGE_STACK);

    // Re-enable interrupts
    splx(spl);
    
    return 0;  // Return success
}
