/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Memory allocator based on demand paging
 */

#include <types.h>
#include <pagevm.h>
#include <swapfile.h>
#include <coremap.h>
#include <vm_tlb.h>
#include <vm.h>
#include "opt-paging.h"

void pagevm_can_sleep(void)
{
    //write here
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
    //inizializza statistiche
}


/* Spegne il sistema VM, rilasciando risorse e stampando statistiche.
 * 
 * Shuts down the VM system, releasing resources and printing statistics.
 */
void vm_shutdown(void)
{
    swap_shutdown();
    coremap_shutdown();
    //stampa statistiche
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