/* 
 * Authors: Leone Fabio - 2024
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

#define SWAP_DEBUG 0 /* Enable this to fill the swap file with zeroes for debugging */

/* Spinlock for synchronizing access to the swap file and bitmap */
static struct spinlock swaplock = SPINLOCK_INITIALIZER;

/* Handle for the swap file in the file system (see SWAPFILE_PATH for location) */
static struct vnode *swapfile;

/* Bitmap to track used and free pages in the swap file */
static struct bitmap *swapmap;

/* 
 * Initializes the swap system by opening the swap file and setting up
 * the bitmap to track free pages.
 * If SWAP_DEBUG is enabled, it fills the swap file with zeroes for debugging.
 */
int swap_init(void)
{
    int result;
    char path[32];

    /* Open the swap file for read and write, creating it if necessary */
    strcpy(path, SWAPFILE_PATH);
    result = vfs_open(path, O_RDWR | O_CREAT, 0, &swapfile);
    if (result) {
        panic("swapfile.c: Failed to open the swap file");
        //return EIO;  // Error opening swap file
    }

#if SWAP_DEBUG
    char *zeroes;
    struct uio u;
    struct iovec iov;
    off_t offset;
    const size_t zeroes_size = 1024;

    /* Write zeroes to the swap file to ensure it's empty */
    zeroes = (char *) kmalloc(zeroes_size);
    bzero(zeroes, zeroes_size);
    for (offset = 0; offset < SWAPFILE_SIZE; offset += zeroes_size) {
        uio_kinit(&iov, &u, zeroes, zeroes_size, offset, UIO_WRITE);
        result = VOP_WRITE(swapfile, &u);
        if (result) {
            panic("DEBUG ERROR: Unable to zero out swap file");
        }
    }
    kfree(zeroes);
#endif

    /* Create a bitmap to track free and used pages in the swap file */
    swapmap = bitmap_create(SWAPFILE_SIZE / PAGE_SIZE);

    return 0;
}

/* 
 * Writes a page from physical memory to the swap file and updates
 * the bitmap to mark the page as used.
 * Returns the offset in the swap file where the page was written.
 * Panics on failure.
 */
int swap_out(paddr_t page_paddr, off_t *ret_offset)
{
    unsigned int free_index;
    off_t free_offset;
    struct iovec iov;
    struct uio u;
    int result;

    KASSERT(page_paddr != 0);
    KASSERT((page_paddr & PAGE_FRAME) == page_paddr);

    spinlock_acquire(&swaplock);
    result = bitmap_alloc(swapmap, &free_index);
    if (result) {
        panic("swapfile.c: No space available in swap file");
        //spinlock_release(&swaplock);
        //return ENOSPC;  // No space available in swap file
    }
    spinlock_release(&swaplock);

    free_offset = free_index * PAGE_SIZE;
    KASSERT(free_offset < SWAPFILE_SIZE);

    /* Write the page to the swap file */
    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, free_offset, UIO_WRITE);
    VOP_WRITE(swapfile, &u);
    if (u.uio_resid != 0) {
        panic("swapfile.c: Failed to write to swap file");
        //return EIO; //Error writing to swap file
    }

    *ret_offset = free_offset;

    vmstats_increment(VMSTAT_SWAPFILE_WRITE);
    return 0;
}

/* 
 * Reads a page from the swap file into physical memory and updates
 * the bitmap to mark the page as free.
 * Returns 0 on success.
 */
int swap_in(paddr_t page_paddr, off_t swap_offset) {
    unsigned int swap_index;
    struct iovec iov;
    struct uio u;

    KASSERT((page_paddr & PAGE_FRAME) == page_paddr);
    KASSERT((swap_offset & PAGE_FRAME) == swap_offset);
    KASSERT(swap_offset < SWAPFILE_SIZE);

    swap_index = swap_offset / PAGE_SIZE;

    spinlock_acquire(&swaplock);
    if (!bitmap_isset(swapmap, swap_index)) {
        panic("swapfile.c: Accessing an uninitialized page in swap file");
        //spinlock_release(&swaplock);
        //return EINVAL;  // Accessing an uninitialized page in swap file
    }
    spinlock_release(&swaplock);

    /* Read the page from the swap file */
    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, swap_offset, UIO_READ);
    VOP_READ(swapfile, &u);
    if (u.uio_resid != 0) {
        panic("swapfile.c: Failed to read from swap file");
        //return EIO; //Error reading from swap file
    }

    spinlock_acquire(&swaplock);
    bitmap_unmark(swapmap, swap_index);
    spinlock_release(&swaplock);

    vmstats_increment(VMSTAT_PAGE_FAULT_SWAPFILE);
    vmstats_increment(VMSTAT_PAGE_FAULT_DISK);
    return 0;
}

/* 
 * Frees a page in the swap file by updating the bitmap to mark it as free.
 * Does not zero out the page data.
 */
void swap_free(off_t swap_offset) {
    unsigned int swap_index;

    KASSERT((swap_offset & PAGE_FRAME) == swap_offset);
    KASSERT(swap_offset < SWAPFILE_SIZE);

    swap_index = swap_offset / PAGE_SIZE;

    spinlock_acquire(&swaplock);
    if (!bitmap_isset(swapmap, swap_index)) {
        panic("swapfile.c: Attempting to free an uninitialized page");
        //spinlock_release(&swaplock);
        //return;  // Invalid attempt to free an uninitialized page
    }

    /* Mark the page as free without zeroing it */
    bitmap_unmark(swapmap, swap_index);
    spinlock_release(&swaplock);
}

/* 
 * Closes the swap file and frees the bitmap resources.
 * Ensures no swap operations are in progress.
 */
void swap_shutdown(void) {
    KASSERT(swapfile != NULL);
    KASSERT(swapmap != NULL);

    /* Close the swap file and destroy the bitmap */
    vfs_close(swapfile);
    bitmap_destroy(swapmap);
}
