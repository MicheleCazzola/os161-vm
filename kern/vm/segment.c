/**
 * Authors: Michele Cazzola - 2024
 * Segments handling, used to distinguish among code, data, stack
 */

#include <types.h>
#include <lib.h>
#include <vm.h>
#include <kern/errno.h>
#include <uio.h>
#include <vnode.h>
#include <swapfile.h>
#include <vmstats.h>
#include <segment.h>

/**
 * Creates a new segment, with zeroed fields
 */
ps_t *seg_create(void) {

    ps_t *proc_segment;

    /**
     * Memory allocation
     */
    proc_segment = (ps_t *)kmalloc(sizeof(ps_t));

    if (proc_segment == NULL) {
        return (ps_t *)NULL;
    }

    /**
     * Zeroing of all fields
     */
    proc_segment->permissions = 0;
    proc_segment->seg_size_bytes = 0;
    proc_segment->file_offset = 0;
    proc_segment->base_vaddr = 0;
    proc_segment->num_pages = 0;
    proc_segment->seg_size_words = 0;
    proc_segment->elf_vnode = NULL;
    proc_segment->page_table = NULL;

    return proc_segment;
}

/**
 * Defines the values of all the fields of a given segments
 * Not used for stack, which has special values
 */
int seg_define(ps_t *proc_seg, size_t seg_size_bytes, off_t file_offset, vaddr_t base_vaddr,
                size_t num_pages, size_t seg_size_words, struct vnode *elf_vnode, char read, char write, char execute) {
    
    KASSERT(read);  /* Read operation should be always allowed */
    KASSERT(proc_seg != NULL);
    KASSERT(proc_seg->elf_vnode != NULL);
    KASSERT(proc_seg->page_table != NULL);
    
    if (write) {
        proc_seg->permissions = PAGE_RW;
    }
    else if (execute) {
        proc_seg->permissions = PAGE_EXE;
    }
    else {
        proc_seg->permissions = PAGE_RONLY;
    }

    proc_seg->seg_size_bytes = seg_size_bytes;
    proc_seg->file_offset = file_offset;
    proc_seg->base_vaddr = base_vaddr;
    proc_seg->num_pages = num_pages;
    proc_seg->seg_size_words = seg_size_words;
    proc_seg->elf_vnode = elf_vnode;

    return 0;
}

/**
 * Defines the values of all the fields for a stack segment
 */
int seg_define_stack(ps_t *proc_seg, vaddr_t base_vaddr, size_t num_pages) {

    pt_t *page_table;

    KASSERT(proc_seg != NULL);
    KASSERT(proc_seg->elf_vnode != NULL);
    KASSERT(proc_seg->page_table != NULL);
    KASSERT(num_pages > 0);     /* Stack cannot have 0 pages */

    proc_seg->permissions = PAGE_STACK;
    proc_seg->seg_size_bytes = 0;
    proc_seg->file_offset = 0;
    proc_seg->base_vaddr = base_vaddr;
    proc_seg->num_pages = num_pages;
    proc_seg->seg_size_words = num_pages * PAGE_SIZE;
    proc_seg->elf_vnode = NULL;     /* Not necessary since there are no stack pages to load from disk */

    /**
     * For stack, seg_prepare is not invoked -> Page table initialization is done here
     */
    page_table = pt_create(proc_seg->num_pages, proc_seg->base_vaddr);

    if (page_table == NULL) {
        return ENOMEM;
    }

    proc_seg->page_table = page_table;

    return 0;
}

/**
 * Segment preparation, that is page table creation
 * Invoked only for text and data segments, not for stack
 */
int seg_prepare(ps_t *proc_seg) {

    pt_t *page_table;

    /**
     * Page table initialization for segment
     * Invoked only for text and data segments
     */
    page_table = pt_create(proc_seg->num_pages, proc_seg->base_vaddr);

    if (page_table == NULL) {
        return ENOMEM;
    }

    proc_seg->page_table = page_table;

    return 0;
}

/**
 * Copies a given segment into another one, which is newly created here
 */
int seg_copy(ps_t *src, ps_t **dest) {

    ps_t *new_seg;
    int seg_define_result;

    KASSERT(dest != NULL);
    KASSERT(src != NULL);
    KASSERT(src->page_table != NULL);
    KASSERT(src->permissions == PAGE_STACK || src->elf_vnode != NULL);  /* vnode pointer NULL is allowed only for stack */

    /**
     * Segment creation
     */
    new_seg = seg_create();

    if (new_seg == NULL) {
        return ENOMEM;
    }

    /**
     * Segment definition: discrimination between stack and other segments
     */
    if (src->permissions != PAGE_STACK) {
        seg_define_result = seg_define(
            new_seg, src->seg_size_bytes, src->file_offset, src->base_vaddr, src->num_pages, src->seg_size_words, 
            src->elf_vnode, 1, src->permissions == PAGE_RW ? 1 : 0, src->permissions == PAGE_EXE ? 1 : 0
        );
    }
    else {
        seg_define_result = seg_define_stack(new_seg, src->base_vaddr, src->num_pages);
    }

    if (seg_define_result) {
        return ENOMEM;
    }

    /**
     * Segment preparation (page table creation) for text and data segment
     * Similar to the pattern used in address space definition and preparation
     */
    if (src->permissions != PAGE_STACK) {
        seg_prepare(new_seg);
    }
    
    *dest = new_seg;

    return 0;
}

/**
 * Retrieves physical address of the page to which the given virtual address belongs
 * Sort of wrapper for pt_get_entry, except for some checks
 */
paddr_t seg_get_paddr(ps_t *proc_seg, vaddr_t vaddr) {

    paddr_t paddr;

    KASSERT(proc_seg != NULL);
    KASSERT(proc_seg->page_table != NULL);

    paddr = pt_get_entry(proc_seg->page_table, vaddr);

    return paddr;
}

/**
 * Inserts a new entry (page virtual address, physical address) to page table.
 * Sort of wrapper for pt_add_entry, except for some checks
 */
void seg_add_pt_entry(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr) {

    KASSERT(proc_seg != NULL);
    KASSERT(proc_seg->page_table != NULL);

    pt_add_entry(proc_seg->page_table, vaddr, paddr);
}

/**
 * Physically loads a new page in memory: it consists in a read operation from ELF file.
 * Invoked only for text and data segment, not stack, and only if page has never been loaded before;
 * in other cases, it is only swapped in, using the proper function seg_swap_in, from the swapfile.
 * No operations on the page table are performed here.
 */
int seg_load_page(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr) {

    vaddr_t page_seg_base_vaddr, seg_base_offset_in_page, previous_pages_offset;
    unsigned long index;
    paddr_t load_paddr;
    size_t load_len_bytes;
    off_t elf_offset;
    struct iovec iovec;
    struct uio uio;
    int result;

    KASSERT(proc_seg != NULL);
    KASSERT(proc_seg->elf_vnode != NULL);
    
    /**
     * Segment start virtual address, page aligned. Used
     * to compute index of entry in page table.
     */
    page_seg_base_vaddr = proc_seg->base_vaddr & PAGE_FRAME;
    index = (vaddr - page_seg_base_vaddr) / PAGE_SIZE;

    KASSERT(index < proc_seg->num_pages);

    // Offset of segment start virtual address in its first page
    seg_base_offset_in_page = proc_seg->base_vaddr & (~PAGE_FRAME);

    /**
     * Faulty virtual address is in first page. Segment virtual space could
     * begin with an internal page offset: page loading must start from begin of
     * segment for needed length, zeroing the starting (and final, if needed)
     * portion of the physical frame.
     */
    if (index == 0) {
        // Physical frame is filled as it was in the ELF file
        load_paddr = paddr + seg_base_offset_in_page;

        // Loading starts from the beginning of the segment virtual space
        elf_offset = proc_seg->file_offset;

        /**
         * If the number of bytes to load is limited to the current page, all
         * segment is loaded, otherwise only the portion in the current page
         * is loaded
         */
        if (proc_seg->seg_size_bytes > PAGE_SIZE - seg_base_offset_in_page) {
            load_len_bytes = PAGE_SIZE - seg_base_offset_in_page;
        }
        else {
            load_len_bytes = proc_seg->seg_size_bytes;
        }
    }
    /**
     * Faulty virtual address is in last page. Loading point is at page beginning,
     * number of bytes to read depend to the length of the segment virtual space.
     * The other portion of the page must be zeroed.
     */
    else if (index == proc_seg->num_pages - 1) {

        // Loading starts at page beginning
        load_paddr = paddr;

        /**
         * Offset in ELF is given by:
         * - segment offset in ELF
         * - number of bytes of previous pages
         * - removing the base offset of the segment in the first page
         */
        previous_pages_offset = (proc_seg->num_pages - 1) * PAGE_SIZE - seg_base_offset_in_page;
        elf_offset = proc_seg->file_offset + previous_pages_offset;

        /**
         * The number of bytes to read could be:
         * - 0: the effective dimension of the segment is lower than what can be stored in the pages allocated for it
         *      and the faulty address is there
         * - >0: there is some content of the segment to read in the current page
         */
        if (proc_seg->seg_size_bytes <= previous_pages_offset) {
            load_len_bytes = 0;
        }
        else {
            load_len_bytes = proc_seg->seg_size_bytes - previous_pages_offset;
        }
    }
    /**
     * Faulty address is inside a middle page. Loading point is at page beginning,
     * file offset depend on page index; the number of bytes to read could be the whole page,
     * a portion of it or it could be zero. The other portion of the page must be zeroed.
     */
    else {
        // Loading starts at page beginning
        load_paddr = paddr;

        /**
         * Offset in ELF is computed as in the previous case
         */
        previous_pages_offset = index * PAGE_SIZE - seg_base_offset_in_page;
        elf_offset = proc_seg->file_offset + previous_pages_offset;

        /**
         * The number of bytes to read could be:
         * - 0: as in previous case
         * - PAGE_SIZE: the segment does not finish in the current page, so this must be fully loaded
         * - otherwise: the segment ends in the current page, but this is not the last one allocated for the segment
         */
        if (proc_seg->seg_size_bytes <= previous_pages_offset) {
            load_len_bytes = 0;
        }
        else if (proc_seg->seg_size_bytes > PAGE_SIZE + previous_pages_offset) {
            load_len_bytes = PAGE_SIZE;
        }
        else {
            load_len_bytes = proc_seg->seg_size_bytes - previous_pages_offset;
        }
    }

    /**
     * The physical frame corresponding to the page to load
     * is zeroed: it will be filled (if necessary) using VOP_READ
     */
    bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);

    /**
     * Update statistics:
     * - if there are no bytes to read, it is a page fault requiring a new zeroed page
     * - otherwise, it is a page fault that requires a read operation from ELF file and, consequently, from disk
     */
    if (load_len_bytes == 0) {
        vmstats_increment(VMSTAT_PAGE_FAULT_ZERO);
    }
    else {
        vmstats_increment(VMSTAT_PAGE_FAULT_DISK);
        vmstats_increment(VMSTAT_PAGE_FAULT_ELF);
    }

    /**
     * Read from ELF file, given physical start addres in memory, start offset in ELF file
     * and number of bytes to read 
     */
    uio_kinit(&iovec, &uio, (void *)PADDR_TO_KVADDR(load_paddr), load_len_bytes, elf_offset, UIO_READ);
    result = VOP_READ(proc_seg->elf_vnode, &uio);

    if (result) {
        return result;
    }

    /**
     * Successful read, but stopped before reading all bytes for some reason
     */
    if (uio.uio_resid > 0) {
        kprintf("Error: truncated read on executable");
        return ENOEXEC;
    }

    return 0;
}

/**
 * Wrapper for pt_swap_out, except for some checks.
 */
void seg_swap_out(ps_t *proc_seg, off_t swapfile_offset, vaddr_t vaddr) {

    KASSERT(proc_seg != NULL);
    KASSERT(proc_seg->page_table != NULL);

    pt_swap_out(proc_seg->page_table, swapfile_offset, vaddr);
}

/**
 * Performs swap in of the page corresponding to the given virtual address, known
 * the physical address where to store it. This operation is performed both at
 * swapfile and at page table layer.
 */
void seg_swap_in(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr){

    off_t swapfile_offset;

    /**
     * Computes swapfile offset of the page of the given virtual address
     */
    swapfile_offset = pt_get_swap_offset(proc_seg->page_table, vaddr);

    /**
     * Performs actual page swap in
     */
    swap_in(paddr, swapfile_offset);

    /**
     * Marks as stored in memory the swapped-in page, together with its physical address
     */
    pt_swap_in(proc_seg->page_table, vaddr, paddr);
}

/**
 * Destroys segment, freeing all associated memory resources
 */
void seg_destroy(ps_t *proc_seg){

    KASSERT(proc_seg != NULL);

    /**
     * Page table clearing and destruction
     */
    if (proc_seg->page_table != NULL) {
        pt_clear_content(proc_seg->page_table);
        pt_destroy(proc_seg->page_table);
    }

    kfree(proc_seg);
}