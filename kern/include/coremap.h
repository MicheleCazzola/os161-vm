/**
 * Authors: Leone Fabio - 2024
 * Coremap handling, used to track freed frames
 */

#ifndef _COREMAP_H_
#define _COREMAP_H_

/* Enum for Coremap Entry States */
typedef enum {
    COREMAP_BUSY_KERNEL = 0,    /* Page is allocated for kernel use */
    COREMAP_BUSY_USER,          /* Page is allocated for user use */
    COREMAP_UNTRACKED,          /* Page is not yet managed by coremap */
    COREMAP_FREED               /* Page is marked as freed */
} coremap_entry_state;

#include <addrspace.h> 

/*
 * Structure representing the state of a physical memory page.
 * This structure helps in tracking and managing memory pages.
 * 
 * Each entry maintains:
 * - The current state of the page (busy, free or allocated).
 * - Links to adjacent pages for FIFO-based replacement strategy.
 * - Reference to the virtual address and the address space (useful when dealing with pages
 *   that do not belong to the current process (e.g. in swapping).
 */
struct coremap_entry {
    coremap_entry_state entry_type; /* State of the page: busy, untracked, or freed */
    unsigned long allocation_size;        /* Number of contiguous pages allocated */
    
    /* Links for tracking allocated pages for replacement strategies */
    unsigned long previous_allocated;   /* Previous allocated page in the list */
    unsigned long next_allocated;   /* Next allocated page in the list */
    
    vaddr_t virtual_address;                  /* Virtual address of the page */
    addrspace_t *address_space;                /* Address space to which the page is assigned */
};



/*
 * Coremap Functions Description
 * 
 * coremap_init:  Initialize coremap and its structures 
 * 
 * coremap_shutdown:  Shutdown coremap and free associated resources 
 * 
 * vaddr_t alloc_kpages:  Allocate kernel pages and return their virtual address 
 * 
 * void free_kpages:  Free kernel pages starting from the given virtual address 
 * 
 * paddr_t alloc_user_page:  Allocate a user page and return its physical address 
 *
 * void free_user_page:  Free a user page given its physical address 
 * 
 */

void coremap_init(void);           
void coremap_shutdown(void);        
vaddr_t alloc_kpages(unsigned npages); 
void free_kpages(vaddr_t addr);     
paddr_t alloc_user_page(vaddr_t vaddr); 
void free_user_page(paddr_t paddr); 

#endif /* _COREMAP_H_ */
