# Lab 4: On-Demand Paging

## Design Document

Authur: Dave Nguyen (Duy Nguyen)

## Overview

The goal of this assignment is to implement allocate pages of physical memory for a user process. 

For stack memory, this will be just at the moment the process needs them (i.e., allocating them on-demand). 

For the heap, it will be when a user process requests more heap memory.

## Major parts

**_Memory regions_**: In osv, information about a virtual address space is tracked via a list of memory regions (i.e., segments). A memory region tracks a contiguous region of memory within an address space. A memory region belongs to only one address space; an address space can have many memory regions. The kernel sets up memory regions for each process and uses them to track valid memory ranges for a process to access. Each user process has a memory region for code, stack, and heap on startup.

**_Page tables_**: In addition to managing each process’s virtual address space, the kernel is also responsible for setting up the address translation table (page table) which maps a virtual address to a physical address. Each virtual address space has its own page table, which is why when two processes access the same virtual address they are actually accessing different data. In osv, a struct vpmap is used to represent a page table. In the file include/kernel/vpmap.h there is a list of operations defined for a page table. For example, vpmap_map maps a virtual page to a physical page.

**_Page faults_**: A page fault is a hardware exception that occurs when a process accesses a virtual memory page without a valid page table mapping, or with a valid mapping, but where the process does not have permission to perform the operation.

On a page fault, the process will trap into the kernel and trigger the page fault handler. If the fault address is within a valid memory region and has the proper permission, the page fault handler should allocate a physical page, map it to the process’s page table, and resume process execution (i.e., return). Note that a write on a read-only memory permission is not a valid access and the calling process should terminate.


### Implementation

#### `Stack`: 

###### `proc.c:stack_setup`:

- Allocate one page of physical memory (`pmem_alloc`)

- Limit your overall stack size of USTACK_PAGES (10) pages total

- Add a one-page memory region (segment) to the process’s address space (`as_map_memregion`). Note that `as_map_memregion` takes the address of the start of the region as its second argument, which will be the lowest address within the region

- Add a page table entry to map the virtual address of the stack’s page to the address of the newly allocated physical page (`vpmap_map`). This function also takes the address of the start of a page, which will again be the lowest address within the page.

- Make sure it is mapping the first (i.e., the highest) page in the stack

- Make sure the address is between `USTACK_UPPERBOUND` and `USTACK_LOWERBOUND`

###### Handle Page Fault

-  To avoid information leaking, you need to memset the allocated physical page to `0s`

- You need to translate the physical address to a kernel virtual address using `kmap_p2v`(paddr) before you do the memset


#### `Heap`: 

`kernel/mm/vm.c:memregion_extend`: Track how much memory has been allocated to each process’s heap and also extend/decrease the size of the heap based on the process’s request

A process can call `sbrk` to allocate/deallocate memory at byte granularity

In user space, we have provided an implementation of malloc and free (in lib/malloc.c) that uses `sbrk`. 

After the implementation of `sbrk` is done, user-level applications should be able to call malloc and free.


## Risk analysis

- Is there a built in function for handle page fault or we need to build for ourselves

### Staging of work

We plan to implement the work in this lab as follows:

1. `stack_setup`

2. Handle Page Fault funtion

3. `memregion_extend`

4. `sys_sbrk`

### Time estimation

1. `stack_setup`: 3 hours

2. Handle Page Fault funtion: 3 hours

3. `memregion_extend`: 3 hours

4. `sys_sbrk`: 3 hours

Total: 12 hours