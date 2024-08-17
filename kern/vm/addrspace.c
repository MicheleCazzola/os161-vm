/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/**
 * Authors: Filippo Forte - 2024
 * Memory allocator based on demand paging
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <vfs.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <spl.h>
#include <vm_tlb.h>
#include "opt-paging.h"

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

addrspace_t *as_create(void){
	addrspace_t *as;

	as = kmalloc(sizeof(addrspace_t));
	if (as == NULL) {
		return NULL;
	}

	#if OPT_PAGING

		as->seg_code = NULL;
		as->seg_data = NULL;
		as->seg_stack = NULL;

	#endif

	return as;
}


void as_destroy(addrspace_t *as){
	
	#if OPT_PAGING
		struct vnode *v;
		KASSERT(as != NULL);

		/* 
		* Destroy the defined segments, close the ELF file 
		* and free the structure 
		*/
		v = as->seg_code->elf_vnode;
		if (as->seg_code != NULL)
			seg_destroy(as->seg_code);

		if (as->seg_data != NULL)
			seg_destroy(as->seg_data);

		if (as->seg_stack != NULL)
			seg_destroy(as->seg_stack);

		if (v != NULL)
			vfs_close(v);

	#endif 

	kfree(as);
}


// Function to activate an address space
void as_activate(void)
{
    addrspace_t *as;

    // Get the current process's address space
    as = proc_getas();
    if (as == NULL) {
        /*
         * Kernel thread without an address space; leave the
         * prior address space in place.
         */
        return;
    }

    #if OPT_PAGING
    int spl;

    // Disable interrupts
    spl = splhigh();

    // Initialize the TLB (Translation Lookaside Buffer)
    vm_tlb_init();

    // Restore interrupt state
    splx(spl);
    #endif
}


// Function to deactivate an address space
void as_deactivate(void)
{
    /*
     * Write this. For many designs it won't need to actually do
     * anything. See proc.c for an explanation of why it (might)
     * be needed.
     */
}


// Function to copy an address space
int as_copy(addrspace_t *old_as, addrspace_t **ret){
    addrspace_t *new_as;

    #if OPT_PAGING
        KASSERT(old_as != NULL);
        KASSERT(old_as->seg_code != NULL);
        KASSERT(old_as->seg_data != NULL);
        KASSERT(old_as->seg_stack != NULL);
    #endif

    // Create a new address space
    new_as = as_create();
    if (new_as == NULL) {
        return ENOMEM;
    }

    #if OPT_PAGING
    // Copy the code segment, destroy the new address space if it fails
    if (!seg_copy(old_as->seg_code, &(new_as->seg_code))) {
        as_destroy(new_as);
        return ENOMEM;
    }

    // Copy the data segment, destroy the new segments and address space if it fails
    if (!seg_copy(old_as->seg_data, &(new_as->seg_data))) {
        seg_destroy(new_as->seg_code);
        as_destroy(new_as);
        return ENOMEM;
    }

    // Copy the stack segment, destroy the new segments and address space if it fails
    if (!seg_copy(old_as->seg_stack, &(new_as->seg_stack))) {
        seg_destroy(new_as->seg_code);
        seg_destroy(new_as->seg_data);
        as_destroy(new_as);
        return ENOMEM;
    }
    #endif

    // Set the return pointer to the new address space
    *ret = new_as;
    return 0;
}


int as_prepare_load(addrspace_t *as)
{
#if OPT_PAGING

	if (seg_prepare(as->seg_code) != 0)
	{
		return ENOMEM;
	}

	if (seg_prepare(as->seg_data) != 0)
	{
		return ENOMEM;
	}
#endif 

	return 0;
}


int as_complete_load(addrspace_t *as)
{

	(void)as;
	return 0;
}


/*
 * Set up a memory segment at the virtual address VADDR with a size of MEMSIZE.
 * The segment in memory will range from VADDR up to (but not including) VADDR + MEMSIZE.
 *
 * The flags READABLE, WRITEABLE, and EXECUTABLE are set if the segment should 
 * have read, write, or execute permissions, respectively. These flags are currently 
 * placeholders and are not yet implemented. When implementing the VM system, these 
 * flags may be used to enforce specific permissions.
 */
#if OPT_PAGING

int as_define_region(addrspace_t *as, vaddr_t vaddr, size_t memsize, size_t file_size,
                     off_t offset, struct vnode *v, int readable, int writeable, int executable)
{
    size_t npages;

    // Ensure the address space and vnode are valid
    KASSERT(as != NULL);
    KASSERT(v != NULL);

    /* 
     * Compute the number of pages needed for the segment.
     * First, account for any offset within the first page.
     * Then, round up the size to the nearest page boundary.
     */
    npages = memsize + (vaddr & ~(vaddr_t)PAGE_FRAME);   // Add offset within the first page
    npages = (npages + PAGE_SIZE - 1) & PAGE_FRAME;      // Round up to the nearest page
    npages = npages / PAGE_SIZE;                         // Calculate the total number of pages
	

    // If the code segment is not defined, create and define it
    if (as->seg_code == NULL)
    {
        as->seg_code = seg_create();  // Create a new segment for code
        if (as->seg_code == NULL)
        {
            return ENOMEM;  // Return if memory allocation fails
        }
        seg_define(as->seg_code, vaddr, file_size, offset, memsize, npages, v, readable, writeable, executable);
        return 0;
    }

    // If the data segment is not defined, create and define it
    if (as->seg_data == NULL)
    {
        as->seg_data = seg_create();  // Create a new segment for data
        if (as->seg_data == NULL)
        {
            return ENOMEM;  // Return if memory allocation fails
        }
        seg_define(as->seg_data, vaddr, file_size, offset, memsize, npages, v, readable, writeable, executable);
        return 0;
    }

    /*
     * Currently, support for more than two regions (code and data) is not implemented.
     * If an attempt is made to define more than two regions, a warning is issued.
     */
    kprintf("paging: Warning: too many regions\n");

    return ENOSYS;  // Return a 'function not implemented' error code
}

#else

int as_define_region(addrspace_t *as, vaddr_t vaddr, size_t memsize,int readable, int writeable, int executable)
{

	(void)as;
	(void)vaddr;
	(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;


	return ENOSYS;
}

#endif



int as_define_stack(addrspace_t *as, vaddr_t *stackptr)
{

    vaddr_t stack_size = PAGEVM_STACKPAGES * PAGE_SIZE;

    //check that the as is not null
    KASSERT(as!=NULL);
    //check that the stack is not already been created
    KASSERT(as->seg_stack==NULL);

    as->seg_stack=seg_create();

    if(as->seg_stack==NULL){
        return ENOMEM;
    }


    if(seg_define_stack(as->seg_stack, stack_size, PAGEVM_STACKPAGES) != 0){
        return ENOMEM;
    }

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}


#if OPT_PAGING
ps_t *as_find_segment(addrspace_t *as, vaddr_t vaddr)
{
    ps_t *ps;
    vaddr_t base_code, top_code, base_data, top_data, base_stack, top_stack;

    // Ensure that the address space and its segments are valid
    KASSERT(as != NULL);
    KASSERT(as->seg_code != NULL);
    KASSERT(as->seg_data != NULL);
    KASSERT(as->seg_stack != NULL);
    KASSERT(as->seg_code->page_table != NULL);
    KASSERT(as->seg_data->page_table != NULL);
    KASSERT(as->seg_stack->page_table != NULL);

    // Calculate the base and top addresses for the code segment
    base_code = as->seg_code->base_vaddr;
    top_code = base_code + as->seg_code->seg_size_words;

    // Calculate the base and top addresses for the data segment
    base_data = as->seg_data->base_vaddr;
    top_data = base_code + as->seg_data->seg_size_words;
    
    // Calculate the base and top addresses for the stack segment
    base_stack = as->seg_stack->base_vaddr;
    top_stack = base_stack + as->seg_stack->seg_size_words;

    // Determine which segment the given virtual address (vaddr) belongs to
    if (vaddr >= base_code && vaddr < top_code) {
        ps = as->seg_code;  // The address belongs to the code segment
    }
    else if (vaddr >= base_data && vaddr < top_data) {
        ps = as->seg_data;  // The address belongs to the data segment
    }
    else if (vaddr >= base_stack && vaddr < top_stack) {
        ps = as->seg_stack; // The address belongs to the stack segment
    } else {
        return NULL;  // The address does not belong to any known segment
    }

    return ps;  // Return the segment that the address belongs to
}
#endif