/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Memory allocator based on demand paging
 */

#include <types.h>
#include <pagevm.h>
#include <vm.h>
#include "opt-paging.h"

/* Function prototypes */
void vm_bootstrap(void);
void vm_shutdown(void);
void vm_tlbshootdown(const struct tlbshootdown *ts);
int vm_fault(int faulttype, vaddr_t faultaddress);
void pagevm_can_sleep(void);
//sunsigned int tlb_get_rr_victim(void);

//static unsigned int current_victim;


/* Restituisce il prossimo indice TLB da sostituire utilizzando la politica round-robin.
 * 
 * Returns the next TLB index to replace using round-robin policy.
 */

/*
static unsigned int tlb_get_rr_victim(void)
{
    //write here

    return 0; 
}
*/

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
    //write here
}


/* Spegne il sistema VM, rilasciando risorse e stampando statistiche.
 * 
 * Shuts down the VM system, releasing resources and printing statistics.
 */
void vm_shutdown(void)
{
    //write here
}

/* Gestisce le richieste di tlb shootdown, che non sono utilizzate in questa implementazione.
 * 
 * Handles TLB shootdown requests, which are not used in this implementation.
 */
void vm_tlbshootdown(const struct tlbshootdown *ts)
{
    //write here

    (void)ts;
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