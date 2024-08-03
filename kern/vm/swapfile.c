/* 
 * Authors: Michele Cazzola, Leone Fabio, Filippo Forte - 2024
 * Swap in/out handling
*/

#include <types.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <vm.h>
#include <bitmap.h>
#include <swapfile.h>
#include <vmstats.h>
#include "opt-paging.h"

#define SWAP_DEBUG 0 /* Does extra operations to ensure swap is zeroed and easy to debug */

static struct spinlock swaplock = SPINLOCK_INITIALIZER;

/* 
 * Node handle in the file system  for the swapfile
 * (see SWAPFILE_PATH for location).
 * It is opened at the initialization of the VM and used thereon.
 */
static struct vnode *swapfile;

/*
 * Uses the builtin bitmap structure to represent efficiently which
 * pages in the swapfile have been occupied and which are not 
 * occupied and may be used to swap out.
 */
static struct bitmap *swapmap;

/* 
 * Opens SWAPFILE from root directory, fills it with zeroes if
 * SWAP_DEBUG is set and then initializes and allocs the bitmap swapmap.
 */
int swap_init(void)
{
    return 0; // Example return value
}

/*
 * Writes in the first free page in the swapfile the content of
 * the memory pointed by argument page_paddr.
 * The offset inside the swapfile where the page has been saved
 * is returned by reference (ret_offset)
 * Update the swapmap as a consequence.
 * Returns 0 on success, panic on some failure.
 */
int swap_out(paddr_t page_paddr, off_t *ret_offset)
{
    (void) page_paddr;
    (void) ret_offset;
    return 0; // Example return value
}

/*
 * Read the content of the swapfile at offset ret_offset into a 
 * pre-allocated memory space pointed by page_paddr.
 * Update the swapmap as a consequence.
 */
int swap_in(paddr_t page_paddr, off_t swap_offset)
{
    (void) page_paddr;
    (void) swap_offset;
    return 0; // Example return value
}

/* 
 * Discard the page from the swapfile by clearing its bit 
 * in the bitmap. No zero-fill done in this case.
 */
void swap_free(off_t swap_offset)
{
    (void) swap_offset;
}

/* 
 * Clean up resources used by swapfile.
 */
void swap_shutdown(void)
{
    
}
