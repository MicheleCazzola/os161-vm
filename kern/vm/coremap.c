/**
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Coremap handling, used to track freed frames
 */

#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <addrspace.h>
#include <pagevm.h>
#include <coremap.h>
#include <swapfile.h>
#include "opt-paging.h"


// INIT
/*
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
static struct coremap_entry *coremap = NULL;
static int nRamFrames = 0;
static int coremapActive = 0;
static struct spinlock victim_lock = SPINLOCK_INITIALIZER;
static unsigned long const_invalid_reference = 0;
static unsigned long victim = 0;
static unsigned long last_alloc = 0;
*/

/*
 * Checks if the coremap is active by checking
 * the status of the shared variable coremapActive
 */
/*static int isCoremapActive(void)
{
    return 0; // Example return value
}*/

/*
 *  This function allocates the arrays containing info on the memory
 *  and enables the coremap functionality.
 *  It is called by vm_bootstrap() in pagevm.c
 */
void coremap_init(void)
{
    return;
}

void coremap_shutdown(void) {
    return;
}

/* 
 *  Search in freeRamFrames if there is a slot npages long
 *  of *freed* frames that can be occupied. If there is then
 *  occupy it and return the base physical address
 */
/*static paddr_t getfreeppages(unsigned long npages,
                             unsigned char entry_type,
                             struct addrspace *as,
                             vaddr_t vaddr)
{
    (void) npages;
    (void) entry_type;
    (void) as;
    (void) vaddr;
    return 0; // Example return value
}*/

/*
 *  Only called by the kernel because the user can alloc 1 page at a time
 *  Get pages to occupy, first search in free pages otherwise
 *  call ram_stealmem()
 */
/*static paddr_t getppages(unsigned long npages)
{
    (void) npages;
    return 0; // Example return value
}*/

/*
 *  Free a desired number of pages starting from addr.
 *  These free pages are now managed by coremap and not the
 *  lower level ram module.
 */
/*static int freeppages(paddr_t addr, unsigned long npages)
{
    (void) addr;
    (void) npages;
    return 0; // Example return value
}*/

/* Allocate some kernel-space pages, also used in kmalloc() */
/*vaddr_t alloc_kpages(unsigned npages)
{
    (void) npages;
    return 0; // Example return value
}*/

/*
 * Free the range of memory pages that have been
 * previously allocated to the kernel by alloc_kpages()
 * Also used in kfree()
 */
/*void free_kpages(vaddr_t addr)
{
    (void) addr;
}*/

/* pageVM paging support */

/*
 *  Only called by the user to alloc 1 page 
 *  First search in free pages otherwise call ram_stealmem()
 *  then update the history linked list accordingly
 */
/*static paddr_t getppage_user(vaddr_t associated_vaddr)
{
    (void) associated_vaddr;
    return 0; // Example return value
}*/

/*
 * Free a page that has been previously allocated by a user program
 * and update the linked list indexes for the removal of a node
 * distinguishing each case
 */
/*static void freeppage_user(paddr_t paddr)
{
    (void) paddr;
}*/

/* Wrapper for getppage_user */
paddr_t alloc_upage(vaddr_t vaddr)
{
    (void) vaddr;
    return 0; // Example return value
}

/* Wrapper for freeppage_user */
void free_upage(paddr_t paddr)
{
    (void) paddr;
}
