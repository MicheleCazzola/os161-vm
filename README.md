# Project OS161: C1 - paging

## Introduction

The project carried out concerns the realisation of a virtual memory manager, which exploits demand paging and dynamic loading; its name is _pagevm_, as can be seen from one of the files in the project.

The files added to the DUMBVM configuration follow the instructions given in the project outline, to which is added the pagevm.c file (and its header file), containing basic memory management functions (detailed later).

The project was carried out in the variant called *C1.1*, with independent page tables for each process (further details are provided in the following sections).

## Composition and division of workloads

The workload was divided fairly neatly among the group members, using a shared repository on Github:

- Filippo Forte (s322788): address space and memory manager, modification to existing files in *DUMBVM*;

- Michele Cazzola (s323270): segments, page tables, statistics and abstraction for the TLB, with wrappers for existing functions;

- Leone Fabio (s330500): coremap and swapfile management.

Each member of the group managed the implementation of the modules independently, including taking care of the documentation and their own part of the report, after initially agreeing on the interface defined in the header files and the dependencies between them. 

Communication mainly took place via weekly video calls lasting 2-3 hours each, during which the work done was discussed and the work to be done was planned.

## Project architecture

The project was realised using a kind of _layer architecture_, with rare exceptions, whereby each module provides abstractions of a specific level, with (almost) unidirectional dependencies. The description of the modules follows their level of abstraction:

- at a high level, interface with existing modules, including _runprogram.c_, _loadelf.c_, _main.c_, _trap.c_;

- at lower levels, provide intermediate services: segment and page table are dependencies of the address space (one directly, the other indirectly), coremap is a dependency of several modules (including page table and higher-level modules), similarly to swapfile;

- others are auxiliary modules: the abstraction for the TLB is used in several modules to access its functionality, statistics are calculated only by invoking interface functions. 

## Source code

### Address space

Each process has an address space, which in our case is represented by three different segments: code, data and stack.

#### Data Structure

```C
typedef struct {
    ps_t *seg_code;
    ps_t *seg_data;
    ps_t *seg_stack;
} addrspace_t ;
```

#### Implementation

The functions in addrspace.c are mainly high-level functions that create, manage and destroy address space by relying on lower-level functions implemented in segment.c. The corresponding prototypes are:

```C
addrspace_t *as_create(void);
void as_destroy(addrspace_t *);
int as_copy(addrspace_t *src, addrspace_t **ret);
void as_activate(void);
void as_deactivate(void);
int as_prepare_load(addrspace_t *as);
int as_complete_load(addrspace_t *as);
int as_define_stack(addrspace_t *as, vaddr_t *initstackptr);
int as_define_region(addrspace_t *as,
                                   vaddr_t vaddr, size_t memsize,
                                   size_t file_size,
                                   off_t offset,
                                   struct vnode *v,
                                   int readable,
                                   int writeable,
                                   int executable);
ps_t *as_find_segment(addrspace_t *as, vaddr_t vaddr);
ps_t *as_find_segment_coarse(addrspace_t *as, vaddr_t vaddr);
```

##### Creation and destruction

The functions _as_create_ and _as_destroy_ have the task of allocating and freeing the memory space required to accommodate the data structure; _as_destroy_, in addition to destroying the 3 corresponding segments by means of _seg_destroy_, also has the task of closing the program file, which is left open to allow dynamic loading. 

##### Copying and activation

Functions are used:
- _as_copy_: takes care of creating a new address space and copying the one received as a parameter. It is based on _seg_copy_.
- _as_activate_: This function is called in _runprogram_ immediately after creating and setting the process address space. In particular, its task is to invalidate the TLB entries.

##### Define

The functions _as_define_region_ and _as_define_stack_ are used to define code segment and data segment (for the _as_define_region_) and stack segment (for the _as_define_stack_). They essentially act as wrappers for the corresponding lower-level functions. What they add is the calculation of the size of the relevant segments, which is necessary for the functions defined in segment.c.

##### Find

There are 2 functions which, given an address space and a virtual address, make it possible to trace the relevant segment. The two functions differ in the granularity of the search, they are _as_find_segment_ and _as_find_segment_coarse_. Both functions have the task of calculating the start and end of the 3 segments (queue, date and stack) and checking to which of these the passed address belongs.

Compared to the as_find_segment function, the coarse granularity version, which operates at page level, is used to handle the problem of the virtual base address of a segment not being aligned with pages. However, this solution presents security risks: the page-alignment operation may erroneously consider some virtual addresses, which do not actually belong to any specific segment, as belonging to a segment.

### Memory manager (pagevm)

In this module, many of the key functions implemented for this project are collected and utilised. The module is responsible for initialising and terminating the entire virtual memory management, as well as handling any memory faults.

#### Implementation

The functions implemented in this module are:

```C
void vm_bootstrap(void);
void vm_shutdown(void);
int vm_fault(int faulttype, vaddr_t faultaddress);
void pagevm_can_sleep(void);
```

#### Initialisation and Termination

The functions _vm_bootstrap_ and _vm_shutdown_ have the task, respectively, of initialising and destroying all the ancillary structures required for virtual memory management. These structures include the coremap, the swap system, the TLB and the statistics collection system. These functions essentially act as containers that call the initialisation and termination routines of other modules.

- **vm_bootstrap**: This function is called during system start-up to configure the entire virtual memory system. Among its main operations, it resets the victim pointer in the TLB, initialises the coremap, sets up the swap system and prepares the statistics management module.
- **vm_shutdown**: This function is invoked during system shutdown to safely release used resources and print statistics on memory usage. It handles the shutdown of the swap system, the coremap and produces the output of the collected statistics.

#### Fault Management

The central function of this module is _vm_fault_, which deals with the handling of TLB misses.
- **vm_fault**: This function is responsible for managing the creation of a new correspondence between the physical address and the virtual address in the TLB each time a TLB miss occurs. The operation of the function consists of the following steps:
  1. **Fault Check**: The function starts by checking the type of fault (e.g. read-only, read, write) and ensuring that the current process and its address space are valid.
  2. **Physical Address Recovery**: Next, retrieve the physical address corresponding to the virtual address where the fault occurred using the _seg_get_paddr_ function.
  3. **Page Management**: If the fault is due to a page not yet allocated or a page previously _swapped out_, a new page is allocated. Depending on the type of fault, the low-level functions _seg_add_pt_entry_ (to add the new page to the page table) or _seg_swap_in_ (to load the page from the swapfile) are then called.
  4. **Updating the TLB**: Finally, using a round-robin algorithm, the victim to be replaced in the TLB is chosen and the TLB is updated with the new physical-virtual address match.

### Segment

A process has an address space consisting of several segments, which are memory areas with a common semantics; in our case, there are three:
- code: contains the code of the executable, is read-only; 
- data: contains global variables, is read-write;
- stack: contains the stack frames of the functions called during process execution, is read-write.

They are not necessarily contiguous in physical memory, so the solution adopted is the creation of a page table for each of them; the number of pages required is calculated downstream of reading the executable, except in the case of the stack, where it is constant.

#### Data structures

The data structure representing the individual segment is defined in _segment.h_ and is as follows:

```C
typedef struct {
    seg_permissions_t permissions;
    size_t seg_size_bytes;
    off_t file_offset;
    vaddr_t base_vaddr
    size_t num_pages;
    size_t seg_size_words;
    struct vnode *elf_vnode;
    pt_t *page_table;
} ps_t;
```

The fields have the following meaning:

- _permissions_: permissions associated with the segment, defined by the enumerative type:

```C
typedef enum {
    PAGE_RONLY, /* 0: read-only */.
    PAGE_RW, /* 1: read-write */
    PAGE_EXE, /* 2: executable */
    PAGE_STACK /* 3: stack */
} seg_permissions_t;
```

- _seg_size_bytes_: size of the segment in the executable, in bytes;
- _file_offset_: offset of the segment within the executable;
- _base_vaddr_: virtual initial address of the segment;
- _num_pages_: number of pages occupied by the segment, in memory;
- _seg_size_words_: memory occupied by the segment, considering whole words;
- _elf_vnode_: pointer to the vnode of the executable file to which the segment belongs;
- _page_table_: pointer to the page table associated with the segment

with the constraint that _seg_size_bytes_ ≤ _seg_size_words_; furthermore, in the event that the actual size of the segment is less than the memory it occupies, the residue is completely zeroed out.

#### Implementation

The functions of this module take care of segment-level operations, possibly acting as simple wrappers for lower-level functions. They are used within the modules _addrspace_ or _pagevm_; the prototypes are as follows:

```C
ps_t *seg_create(void);
int seg_define(
    ps_t *proc_seg, size_t seg_size_bytes, off_t file_offset,
    vaddr_t base_vaddr, size_t num_pages, size_t seg_size_words,
    struct vnode *elf_vnode, char read, char write, char execute
);
int seg_define_stack(ps_t *proc_seg, vaddr_t base_vaddr, size_t num_pages);
int seg_prepare(ps_t *proc_seg);
int seg_copy(ps_t *src, ps_t **dest);
paddr_t seg_get_paddr(ps_t *proc_seg, vaddr_t vaddr);
void seg_add_pt_entry(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
int seg_load_page(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_swap_out(ps_t *proc_seg, off_t swapfile_offset, vaddr_t vaddr);
void seg_swap_in(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_destroy(ps_t *proc_seg);
```

##### Creation and destruction

Functions are used:
- _seg_create_: creates a new segment, allocating the necessary memory by means of _kmalloc_ and resetting all fields, it is invoked at the creation of an address space;
- _seg_destroy_: destroys the given segment, also freeing the memory held by its page table, is invoked upon destruction of the address space to which the segment belongs.

##### Definition, preparation and copying

Functions are used:
- _seg_define_: defines the value of the fields of the given segment, using the parameters passed by the caller (i.e. _as_define_region_); they do not include information regarding the page table, which is defined later; this function is only used for code and data regions, not for the stack;
- _seg_define_stack_: like the previous one, but only used for the stack and invoked by _as_define_stack_:
    * it does not exist within the file, so offset and size in the file are zero fields;
    * the number of pages is defined by the constant _PAGEVM_STACKPAGES_, equal to 18;
    * the size occupied in memory (whole words) is directly related to the number of pages, according to the constant _PAGE_SIZE_;
    * the pointer to the vnode is _NULL_, as there is no need to keep this information: it is used, in the case of the other regions, to load pages into memory from the executable, which is not the case with the stack.
    
    It also allocates the page table, for consistency with the pattern followed in the _DUMBVM_ configuration, in which the preparation function is not invoked on the stack segment.

- _seg_prepare_: used to allocate the page table relating to the code and data segments, invoked once for each of the segments, within _as_prepare_load_;
- _seg_copy_: performs the deep copy of a given segment into a target segment, invoked in _as_copy_; makes use of the similar function of the _pt_ module for page table copying.

##### Address translation operations

Functions are used:
- _seg_get_paddr_: obtains the physical address of a memory page, given the virtual address that caused a TLB miss; is invoked by _vm_fault_, following a TLB miss; directly uses the analogous function of the _pt_ module;
- _seg_add_pt_entry_: adds the pair (virtual address, physical address), passed as parameters, to the page table using the analogous function of the _pt_ module; it is invoked in _vm_fault_, following a TLB miss.

##### Loading (dynamic) of a page from the executable

The _seg_load_page_ function is used, which constitutes a large part of the complexity of this module and allows, in effect, the dynamic loading of programme pages from the executable. Given the associated segment, the objective is to load the page associated with a virtual address (assuming it is neither memory resident nor _swapped_) into memory, to a physical address already appropriately derived (and passed as a parameter).

The requested page is represented by an index within the executable, calculated from the _base_vaddr_ field of the segment; there are three distinct cases:

- **first page**: the requested page is the first page of the executable, whose offset within the page itself may not be zero (i.e., the virtual start address of the executable may not be _page aligned_) and, for simplicity, this offset is also maintained at the physical level, by loading the first page even if it is only partially occupied by the executable; there are two possible sub-cases:
  * the executable terminates on the current page;
  * the executable also occupies other pages,

  by which we determine how many bytes to read from the ELF file.

- **last page**: the requested page is the last page of the executable, so the loading takes place at an _page aligned_ address, while the offset within the ELF file is calculated from the total number of pages and the offset within the first page; there are two possible sub-cases:
  * the executable terminates on the current page;
  * the executable terminates on a previous page, but the current page is still occupied: this is due to the fact that an executable file has a _filesize_ (represented in _ps_t_ by _seg_size_bytes_) and a _memsize_ (represented in _ps_t_ by _seg_size_words_) that may differ, with the former less than or equal to the latter; in this case, the occupied (but not valued) memory area must be reset,

  and, in particular, in the second case, no bytes are read from the ELF file.

- **intermediate page**: the loading is similar to the previous case, at the level of the offset in the ELF file and the physical address, but there are three possible sub-cases, with regard to the number of bytes to be read:
  * the executable terminates on a previous page;
  * the executable terminates on the current page;
  * the executable also takes up subsequent pages,

  which are handled similarly to the two previous situations.

After defining the three loading parameters, viz:
- _load_paddr_: physical address in memory at which to load the page;
- _load_len_bytes_: number of bytes to be read;
- _elf_offset_: read start offset within the executable file,

resets the memory region designated to host the page, and then reads, following the pattern given by the operations:
- _uio_kinit_ to set up the _uio_ and _iovec_ data structures;
- _VOP_READ_ to perform the actual read operation.

They are also carried out:
- checks on the result of the read operation, to return any errors to the caller;
- check on the _load_len_bytes_ parameter, for statistical purposes: if it is null, a page fault is recorded concerning a page that has been reset, otherwise it concerns a page to be loaded from the executable (and from disk).

##### Swapping operations

Functions are used:
- _seg_swap_out_: marks as _swapped out_ the page corresponding to the virtual address passed, by means of the analogous function of the _pt_ module; it is invoked by _getppage_user_ within the _coremap_ module, within which the frame is physically swapped out;
- _seg_swap_in_: swaps in the frame corresponding to the virtual address provided, assuming the page was in a _swapped_ state:
    * obtains the offset in the swapfile from the contents of the page table to the appropriate entry;
    * physically swaps in the frame, using the physical address provided;
    * uses the analogous function of the form _pt_ to insert the correspondence (virtual address, physical address) in the appropriate entry.

### Page table

As mentioned above, there are three page tables for each process, one for each of the three constituent segments; for this reason, no form of locking is required, as the page table is not a resource shared between different processes, but is specific to a single process.

#### Data structures

The data structure used to represent the page table is defined in _pt.h_ and is as follows:

```C
typedef struct {
    unsigned long num_pages;
    vaddr_t base_vaddr;
    paddr_t *page_buffer;
} pt_t;
```

whose fields have the following meaning:
- _num_pages_: number of pages within the page table;
- _base_vaddr_: initial virtual address of the page table, corresponds with the virtual base address of the segment and is used to calculate the page to which a requested virtual address belongs;
- _page_buffer_: vector of physical addresses of the represented pages, dynamically allocated when creating the page table.

Each page table entry (i.e. each individual page buffer element) can take on the following values:
- _PT_EMPTY_ENTRY_ (0): since 0 is not a valid physical address (it is occupied by the kernel), it is used to indicate an empty entry, i.e. a page not yet loaded into memory;
- _PT_SWAPPED_ENTRY_ (1) + swap offset: as 1 is not a valid physical address (it is occupied by the kernel), it is used to indicate a page that has been swapped out; of the remaining 31 bits, the least significant are used to represent the page offset in the swapfile (it is 9 MB in size, so 24 bits would be sufficient); the contents of these entries do not interfere with valid physical addresses as and CPU works with addresses multiples of 4;
- other values: in this case there is a valid physical address for the page, i.e. it is present in memory and a page fault has not occurred.

In order to be able to easily derive the index of the entry in the buffer from a virtual address, the buffer was constructed in such a way that each entry occupies an entire page: this occupies more memory, but greatly simplifies the performance of almost all operations carried out on the page table, since deriving the index from a virtual address is necessary in many of them.

#### Implementation

The page table management functions, as in the case of segments, are divided into different groups according to the task they perform. The prototypes, defined in the _pt.h_ file, are as follows:

```C
pt_t *pt_create(unsigned long num_pages, vaddr_t base_address);
int pt_copy(pt_t *src, pt_t **dest);
paddr_t pt_get_entry(pt_t *pt, vaddr_t vaddr);
void pt_add_entry(pt_t *pt, vaddr_t vaddr, paddr_t paddr);
void pt_clear_content(pt_t *pt);
void pt_swap_out(pt_t *pt, off_t swapfile_offset, vaddr_t vaddr);
void pt_swap_in(pt_t *pt, vaddr_t vaddr, paddr_t paddr);
off_t pt_get_swap_offset(pt_t *pt, vaddr_t vaddr);
void pt_destroy(pt_t *pt);
```

##### Creating and copying

Functions are used:
- _pt_create_: allocates a new page table, defining the number of pages and the virtual starting address, passed as parameters; the buffer used for paging is allocated and cleared, using the constant _PT_EMPTY_ENTRY_, as initially the page table is (conceptually) empty;
- _pt_copy_: copies the contents of a page table to a new one, allocated within the function; only used in the context of copying an address space, invoked by _seg_copy_.

##### Deletion and destruction

Functions are used:
- _pt_clear_content_: performs the side effects of deleting page table contents on swapfiles and physical memory:
  * if an entry is _swapped_, it deletes it from the swapfile;
  * If an entry is in memory, it frees physical memory,

  and is used when destroying an address space, invoked by _seg_destroy_;

- _pt_destroy_: releases the memory resources held by the page table, including the buffers contained within; like the previous one, it is used when destroying an address space and is invoked by _seg_destroy_.

##### Address translation operations

Functions are used:
- _pt_get_entry_: obtains the physical address of a page from the virtual address; in particular, it returns the constants:
  * _PT_EMPTY_ENTRY_ if the virtual address belongs to a non-stored, non _swapped_ page;
  * _PT_SWAPPED_ENTRY_ if the virtual address belongs to a _swapped_ page;
- _pt_add_entry_: inserts a physical address in the entry corresponding to the virtual address; both are passed as parameters and, in particular, the physical address is appropriately derived and supplied by the caller.

##### Swapping operations

Functions are used:
- _pt_swap_out_: marks as _swapped_ the entry corresponding to the supplied virtual address; using the constant _PT_SWAPPED_MASK_, it also stores the offset in the swapfile of the page to which the virtual address belongs;
- _pt_swap_in_: dual to the previous one, it is in fact only a wrapper for _pt_add_entry_, as it only requires the writing of a new physical address at the entry for the page to which the given virtual address belongs;
- _pt_get_swap_offset_: given a virtual address, obtains the offset in the swapfile of the page to which it belongs, via the 31 most significant bits of the corresponding entry; it is used during the swap in operation, invoked by _seg_swap_in_. 

### TLB

The _vm_tlb.c_ module (with its header file) contains an abstraction for managing and interfacing with the TLB: no data structures are added, but only functions that act as wrappers (or little more) to the already existing read/write functions, as well as the management of the replacement policy.

#### Implementation

The functions implemented in this module have the following prototypes:

```C
void vm_tlb_invalidate_entries(void);
void vm_tlb_reset_current_victim(void);
uint64_t vm_tlb_peek_victim(void);
void vm_tlb_write(vaddr_t vaddr, paddr_t paddr, unsigned char dirty);
```

and perform the following tasks:

- _vm_tlb_invalidate_entries_: invalidates all TLB entries, using the appropriate masks defined in _mips/tlb.h_; it is invoked by _as_activate_, i.e. at the start of the process and at each context switching;
- _vm_tlb_reset_current_victim_: resets the position of the victim of the round-robin algorithm, used to manage the replacement, to 0; it is invoked by _vm_bootstrap_, during the bootstrap of the operating system;
- _vm_tlb_peek_victim_: performs a read in the TLB (by means of the _tlb_read_ function), of the entry corresponding to the current victim; it is used to check whether the current victim is a valid entry or not, for statistical purposes, following TLB misses;
- _vm_tlb_write_: writes the pair (_vaddr_, _paddr_), within the entry corresponding to the current victim (which in turn may or may not be a valid entry), using the _tlb_write_ function; the position of the victim is obtained through the _vm_tlb_get_victim_round_robin_ function, which increments the victim's index by one unit (in a circular fashion), and then returns the current one, effectively performing the replacement algorithm; it is invoked following a TLB miss, in the absence of other errors. In addition, if the virtual address belongs to a page with write permission, the _dirty bit_ is set, which (in the TLB of os161) indicates whether the corresponding entry contains the address of a _writable_ page.

The functions _tlb_read_ and _tlb_write_ are implemented directly in assembly language and their prototypes are defined in the file _mips/tlb.h_.

### Statistics

The _vmstats.c_ module (with its header file) contains the data structures and functions for handling the memory manager statistics.

#### Data structures

Data structures are defined in _vmstats.c_, in order to be able to implement _information hiding_, accessing them only with externally exposed functions. They are:

```C
bool vmstats_active = false;
struct spinlock vmstats_lock;

unsigned int vmstats_counts[VMSTATS_NUM];

static const char *vmstats_names[VMSTATS_NUM] = {
    "TLB faults", /* TLB misses */
    "TLB faults with free", /* TLB misses with no replacement */
    "TLB faults with replace", /* TLB misses with replace */
    "TLB invalidations", /* TLB invalidations (number of times) */
    "TLB reloads", /* TLB misses for pages stored in memory */
    "Page faults (zeroed)", /* TLB misses requiring zero-filled page */
    "Page faults (disk)", /* TLB misses requiring load from disk */
    "Page faults from ELF", /* Page faults requiring load from ELF */.
    "Page faults from swapfile", /* Page faults requiring load from swapfile */
    "Swapfile writes" /* Page faults requiring write on swapfile */
};
```

in order:

- _vmstats_active_: flag to indicate whether the module is active (i.e. the counters have been appropriately initialised);
- _vmstats_lock_: spinlock for mutually exclusive access, necessary as this module (with its data structures) is shared by all processes and requires the increments to be independent;
- _vmstats_counts_: vector of counters, one for each statistic;
- _vmstats_names_: vector of strings, containing the names of the statistics to be collected, useful when printing.

In the header file (_vmstats.h_) they are defined instead:

```C
#define VMSTATS_NUM 10

enum vmstats_counters {
    VMSTAT_TLB_MISS,
    VMSTAT_TLB_MISS_FREE,
    VMSTAT_TLB_MISS_REPLACE,
    VMSTAT_TLB_INVALIDATION,
    VMSTAT_TLB_RELOAD,
    VMSTAT_PAGE_FAULT_ZERO,
    VMSTAT_PAGE_FAULT_DISK,
    VMSTAT_PAGE_FAULT_ELF,
    VMSTAT_PAGE_FAULT_SWAPFILE,
    VMSTAT_SWAPFILE_WRITE
};
```

where:
- _VMSTATS_NUM_: the number of statistics to be collected;
- _vmstats_counters_: the names of the statistics to be collected, used as an index in _vmstats_counts_ and _vmstats_names_, exposed externally for use in the invocation of the increment function

#### Implementation

The functions implemented in this module have the following prototypes:

```C
void vmstats_init(void);    
void vmstats_increment(unsigned int stat_index);
void vmstats_show(void);
```

They perform the following tasks:

- _vmstats_init_: initialises the _vmstats_active_ flag, after resetting all counters in _vmstats_counts_; it is invoked at the bootstrap of the virtual memory manager;
- _vmstats_increment_: increments the statistics associated with the index provided as a parameter by one unit;
- _vmstats_show_: prints, for each statistic, the associated count value, displaying any warning messages if the relationships between the statistics are not respected; it is invoked on shutdown of the virtual memory manager.

Each operation carried out within the initialisation and increment functions is protected by spinlocks, as it requires mutually exclusive access, since writes are made to shared data; the print function only performs reads, and therefore does not require the use of any form of locking.

### Coremap

The coremap is a fundamental component for managing physical memory within the virtual memory system. This data structure keeps track of the state of each page in physical memory, allowing the system to know which pages are currently in use, which are free and which need to be replaced or retrieved from disk. The coremap manages both the pages used by the kernel and those used by user processes, facilitating dynamic memory management according to the system's needs.

#### Data structures

The data structure used to manage the coremap is defined in coremap.h and is as follows:

```C
struct coremap_entry {
    coremap_entry_state entry_type; 
    unsigned long allocation_size;        
    unsigned long previous_allocated;   
    unsigned long next_allocated;   
    vaddr_t virtual_address;                
    addrspace_t *address_space;               
};
```

This structure serves to represent the state and properties of a single page of physical memory. Each field has a specific role:
- _entry_type_: indicates the current state of the page, using the enum _coremap_entry_state_. It may take the state:
  * COREMAP_BUSY_KERNEL: The page is allocated for kernel use.
  * COREMAP_BUSY_USER: The page is allocated for user use.
  * COREMAP_UNTRACKED: the page is not yet managed by the coremap.
  * COREMAP_FREED: the page was freed and can be reused.
- _allocation_size_: specifies the size of the allocation, i.e. the number of contiguous pages allocated. This is particularly relevant for kernel allocations, which may require blocks of contiguous pages.
- _previous_allocated_ and _next_allocated_: act as pointers to a linked list of allocated pages. They are used to implement a FIFO (First-In-First-Out) strategy for page replacement, facilitating the tracking of pages in order of allocation.
- _virtual_address_: stores the virtual address associated with the page. It is particularly important for user pages, where the system must map the virtual address of the user to the corresponding physical page.
- _address_space_: points to the address space to which the page is allocated. It is used to identify which user process is using the page.

#### Implementation

The coremap implementation is fundamental to memory management within the operating system. The following prototypes are defined to handle coremap initialisation, allocation, deallocation and shutdown:

```C
void coremap_init(void);            
void coremap_shutdown(void);        
vaddr_t alloc_kpages(unsigned npages); 
void free_kpages(vaddr_t addr);     
paddr_t alloc_user_page(vaddr_t vaddr); 
void free_user_page(paddr_t paddr); 
```

##### Initialisation and Termination

The following functions are used:
- _coremap_init()_: it initialises the coremap, allocating the memory required to manage all available physical memory pages. It sets each entry initially as COREMAP_UNTRACKED, indicating that the pages are not yet managed.
- _coremap_shutdown()_: it is responsible for shutting down and cleaning the coremap, freeing allocated memory and deactivating the coremap when the system no longer needs it.

##### Page Allocation and Deallocation - Kernel

The following functions are used:
- _alloc_kpages(unsigned npages)_: it handles page allocation for the kernel. It attempts to allocate contiguous _npages_, returning the virtual address of the first block. If there are not enough free pages, the system attempts to "steal" memory by calling the _ram_stealmem()_ function, provided by default by os161 in ram.c, which returns the physical address of a frame that has not yet been used.
- _free_kpages(vaddr_t addr)_: it frees the pages allocated to the kernel, marking them as COREMAP_FREED in the coremap, making them available for future allocation.

##### Page Allocation and Deallocation - User Processes

The following functions are used:
- _alloc_user_page(vaddr_t vaddr)_: it manages page allocation for user processes. It first searches for free pages and, if necessary, replaces an existing page using a FIFO replacement strategy. If a page is replaced, the function interacts with the swapfile to manage the transfer of the victim page to disk. Furthermore, it is crucial from an implementation point of view to identify the segment that contains the page selected as the victim. This step is essential in order to mark as 'swapped' the correct page table entry corresponding to the virtual address and to save the offset of the swapfile where the page was stored. The segment search is performed using

    `ps_t *as_find_segment_coarse(addrspace_t *as, vaddr_t vaddr)` 

    defined in _addrspace.h_.
- _free_user_page(paddr_t paddr)_: it frees pages allocated to user processes, removing the page from the allocation queue and marking it as COREMAP_FREED in the coremap.

### Swapfile

The swapfile is an essential component to extend the physical memory capacity of the system, allowing the operating system to handle more processes than can be contained in the available physical memory. When the RAM memory is full, the swapfile allows pages to be temporarily moved to disk, freeing memory for other processes.

#### Implementation

The swapfile implementation provides various functions for managing swap space and transferring pages between physical memory and disk. The swapfile is limited to 9 MB (size defined in _swapfile.h_) and if more swap space is requested at runtime, the system panics indicating the violation. The prototypes of the main functions are:

```C
int swap_init(void);
int swap_out(paddr_t page_paddr, off_t *ret_offset);
int swap_in(paddr_t page_paddr, off_t swap_offset);
void swap_free(off_t swap_offset);
void swap_shutdown(void);
```

##### Initialisation

Function _swap_init()_ is used: it initialises the swapfile and the bitmap useful for tracking the utilisation status of pages in the swap file.

##### Swapping operations

The following functions are used:
- _swap_out(paddr_t page_paddr, off_t *ret_offset)_: it writes a page from physical memory to the swap file. The parameter _page_paddr_ indicates the physical address of the page to be moved to disk. The function returns the offset in the swap file where the page was stored, allowing it to be retrieved later.
- _swap_in(paddr_t page_paddr, off_t swap_offset)_: it reads a page from the swap file and restores it to physical memory. The parameter _swap_offset_ indicates the offset in the swap file from which to retrieve the page.

##### Cleaning and Termination

The following functions are used:
- _swap_free(off_t swap_offset)_: it frees the space in the swap file associated with a page that is no longer needed. The parameter _swap_offset_ specifies the offset of the page in the swap file to be freed.
- _swap_shutdown()_: it closes and releases the resources associated with the swapfile when the system no longer needs them. It closes the swapfile and releases the memory used by the bitmap.

### Modifications to other files

Below are the (minor, but necessary) changes made to other os161 kernel files, which already existed in the source version.

#### trap.c

Within the _kill_curthread_ function, in the case of:
- TLB misses in read/write;
- TLB hit in case of write request (memory store) on read-only memory segment;

an error printout is executed, followed by a invocation to the system call _sys__exit_, to effect the _graceful_ termination of the process, freeing the allocated resources; this usually occurs following the return of a non-zero value by the _vm_fault_ function.

In this way, it is possible to avoid an operating system _panic_, should such an error occurs, allowing either the execution of further tests (or the repetition of the same one); furthermore, this allows the operating system to be terminated correctly (with the _q_ command), tracking statistics for the _faulter_ test.

This change is only valid when the conditional option _OPT_PAGING_ is enabled.

#### runprogram.c

In this implementation, a modification with a conditional flag (using #if !OPT_PAGING) was added to determine whether the file remains open or is closed immediately after loading the executable. If the paging option (OPT_PAGING) is disabled, the file is closed immediately. Otherwise, the file remains open to be closed later during address space destruction by calling _as_destroy_. This change was introduced to support dynamic loading, which requires continuous access to the executable file during programme execution.

#### loadelf.c

In this implementation, the code has been modified to support dynamic loading, which allows pages of the executable to be loaded into memory only when the conditional option OPT_PAGING is enabled. In this case, _as_define_region_ is called with additional parameters that include the file offset, the size in memory, the file size and the pointer to the file. This allows the function to handle memory regions in a way that supports paging on demand.

The function _as_prepare_load_ is called to prepare the loading of the programme into the address space. However, if paging on demand is active, the actual loading of the segment pages in _load_segment_ is not performed at this stage.

#### main.c

Within the _boot_ function, a call to _vm_shutdown_ was inserted, in order to perform the termination of the virtual memory manager allowing, among other things, the display of statistics on the terminal; this invocation only occurs when the conditional option _OPT_PAGING_ is enabled.

## Test

In order to verify the correct functioning of the system, we used the tests already present within os161, choosing those suitable for what was developed:
- _palin_: performs a simple check on a string of 8000 characters, without stressing the VM; does not cause TLB replacements or page swaps;
- _matmult_: performs a matrix product (checking the result against the expected result), taking up a lot of memory space and putting more stress on the VM than the previous one;
- _sort_: sorts a large array using the _quick sort_ algorithm;
- _zero_: checks that the memory areas to be zeroed in the allocation are correctly zeroed (the check performed on the syscall _sbrk_ is ignored);
- _faulter_: verifies that illegal access to a memory area causes the programme to be interrupted;
- _ctest_: performs the traversal of a linked list;
- _huge_: allocates and manipulates a large array.

To ensure that the basic functions of the kernel were already correctly implemented, we performed the following kernel tests:
- _at_: arrays handling;
- _at2_: like the previous one, but with a larger array;
- _bt_: bitmap management;
- _km1_: verification of kmalloc;
- _km2_: as the previous one, but under stress conditions.

Below are the statistics recorded for each test: each was run only once, and then the system was shut down.

| Name test | TLB faults | TLB faults (free) | TLB faults (replace) | TLB invalidations | TLB reloads | Page faults (zeroed) | Page faults (disk) | Page faults (ELF) | Page faults (swapfile) | Swapfile writes |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
|palin|13889|13889|0|7789|13884|1|4|4|0|0|
|matmult|4342|4323|19|1222|3534|380|428|3|425|733|
|sort|7052|7014|38|3144|5316|289|1447|4|1443|1661|
|zero|143|143|0|139|137|3|3|3|0|0|
|faulter|61|61|0|132|58|2|1|1|0|0|
|ctest|248591|248579|12|249633|123627|257|124707|3|124704|124889|
|huge|7459|7441|18|6752|3880|512|3067|3|3064|3506|
