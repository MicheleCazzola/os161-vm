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
    //write here
    (void)faulttype;
    (void)faultaddress;
    return 0; 
}