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
    vm_tlb_init();
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


/* Gestisce le TLB miss. Chiamata quando una pagina è nella tabella delle pagine 
 * ma non nella TLB o deve essere caricata dal disco.
 * 
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
    unsigned int tlb_index;
    uint64_t peek;

/*
    //check that the fault type is correct
    if((faulttype != VM_FAULT_READONLY) && (faulttype != VM_FAULT_READ) && (faulttype != VM_FAULT_WRITE)){
        return EINVAL;
    }
*/

    if(faulttype == VM_FAULT_READONLY){
        return EACCES;
    }

    //check that the current process exists
    if(curproc == NULL){
        return EFAULT;
    }

    //get the address space of the current process
    as = proc_getas();
    if(as == NULL){
        return EFAULT;
    }

    //get the segment in the address space that causes the fault
    process_segment = as_find_segment(as, faultaddress);
    if(process_segment == NULL){
        return EFAULT;
    }

    //get the physical address of the fault address
    physical_address = seg_get_paddr(process_segment, faultaddress);


    unpopulated = 0;
    if(physical_address == PT_EMPTY_ENTRY){

        physical_address = alloc_user_page(page_aligned_fault_address);
        seg_add_pt_entry(process_segment, faultaddress, physical_address);

        if(process_segment->permissions == PAGE_STACK){
            bzero((void *)PADDR_TO_KVADDR(physical_address), PAGE_SIZE);
            vmstats_increment(VMSTAT_PAGE_FAULT_ZERO);
        }

        unpopulated = 1;
    }

    else if(physical_address == PT_SWAPPED_ENTRY){

        physical_address = alloc_user_page(page_aligned_fault_address);
        seg_swap_in(process_segment, faultaddress, physical_address);
    }
    else{
        vmstats_increment(VMSTAT_TLB_RELOAD);
    }

    /* make sure it's page-aligned */
    KASSERT( (physical_address & PAGE_FRAME) == physical_address );

    if (process_segment->permissions != PAGE_STACK && unpopulated) {
        /* Load page from file*/
        result = seg_load_page(process_segment, faultaddress, physical_address);
        if (result)
            return EFAULT;
    }

    spl = splhigh();

    tlb_index = vm_tlb_get_victim_round_robin();

    peek = vm_tlb_peek_victim();

    if(peek & (TLBLO_VALID*1ULL)){
        vmstats_increment(VMSTAT_TLB_MISS_REPLACE);
    }else{
        vmstats_increment(VMSTAT_TLB_MISS_FREE);
    }


    vm_tlb_write(faultaddress, physical_address, tlb_index);

    splx(spl);
    
    return 0; 
}