/* 
 * Authors: Leone Fabio - 2024
 * Swap in/out handling
*/

#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#define SWAPFILE_SIZE 9 * 1024 * 1024 
#define SWAPFILE_PATH "emu0:/SWAPFILE"

/*
 * Swapfile Functions Description
 * 
 * swap_init: Initializes the swap system.
 * 
 * swap_out: Swaps a page out of physical memory to the swap file.
 * 
 * swap_in: Swaps a page in from the swap file to physical memory.
 * 
 * swap_free: Frees a slot in the swap file that was previously used.
 * 
 * swap_shutdown: Shuts down the swap system and cleans up resources.
 * 
 */


int swap_init(void);
int swap_out(paddr_t page_paddr, off_t *ret_offset);
int swap_in(paddr_t page_paddr, off_t swap_offset);
void swap_free(off_t swap_offset);
void swap_shutdown(void);

#endif /* _SWAPFILE_H_ */