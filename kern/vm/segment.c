/**
 * Authors: Michele Cazzola - 2024
 * Segments handling, used to distinguish among code, data, stack
 */

#include <types.h>
#include <lib.h>
#include <vm.h>
#include <kern/errno.h>
#include <swapfile.h>
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

int seg_load_page(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr){

    (void)proc_seg;
    (void)vaddr;
    (void)paddr;

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