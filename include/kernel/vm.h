#ifndef _VM_H_
#define _VM_H_

/*
 * Virtual memory system. Each process has a single virtual address space.  An
 * address space is segmented into multiple memory regions. Each memory region
 * is backed by a memory store (files, devices, etc.), and all addresses within
 * the region have the same protection attributes. A machine dependent virtual
 * to physical translation unit, vpmap, is defined for each address space.
 */
#include <kernel/types.h>
#include <kernel/list.h>
#include <kernel/synch.h>

#define ADDR_ANYWHERE 0Xfffffff
#define UHEAP_INIT_PAGES 1000

// Error Codes
#define ERR_VM_BOUND 1 // bound error
#define ERR_VM_INVALID 2 // operation not allowed
#define ERR_VM_RESOURCE_UNAVAIL 3 // resource required for an operation is not available

/*
 * Memory permissions
 */
typedef enum {
    MEMPERM_R = 0, // Kernel Read-only
    MEMPERM_RW = 1, // Kernel Read-write
    MEMPERM_UR = 2, // User Read-only
    MEMPERM_URW = 3 // User Read-write
} memperm_t;

/*
 * Address space. Each address space consists of a set of memory regions, and a
 * machine dependent virtual-to-physical translation unit ``vpmap``.
 */
struct memregion {
    struct addrspace *as;
    Node as_node;           // used to connect all memregions within an addrspace
    vaddr_t start;          // starting addr of memregion
    vaddr_t end;            // ending addr of memregion
    memperm_t perm;
    int shared;             // 1:shared 0:private
    struct memstore *store;
    offset_t ofs;           // offset into memstore
};

struct addrspace {
    List regions;
    struct vpmap *vpmap;
    struct sleeplock as_lock;
    struct memregion *heap; // track heap memregion to ease extension
};

// Kernel address space.
extern struct addrspace *kas;

/*
 * Machine-dependent vm attributes
 * ``pg_size``: page size
 * ``kvm_base``: base address of kernel memory
 * ``kmap_start``: starting virtual address of kmap
 * ``kmap_end``: end virtual address of kmap
 */
extern size_t pg_size;
extern vaddr_t kvm_base;
extern vaddr_t kmap_start;
extern vaddr_t kmap_end;

/* Initialize the vm system */
void vm_init(void);

/* Initialize a new address space. Return error code if failed to do so. */
err_t as_init(struct addrspace*);

/* Destroy and free all memory in the address space */
void as_destroy(struct addrspace *as);

/*
 * Dump the content starting at vaddr in as to the end of its memregion
 */
void as_dump(struct addrspace *as, vaddr_t vaddr);

/*
 * Copy src address space's memregions into dst_as.
 * Return ERR_OK on success, ERR_NOMEM if fails to allocate memregion in dst.
 */
err_t as_copy_as(struct addrspace *src_as, struct addrspace *dst_as);

/*
 * Allocate and map a region of memory within the address space. Region need to
 * be page aligned.
 * Return NULL if failed to allocate.
 * addr: starting address of the region, if (addr==ADDR_ANYWHERE) { allocate within the function }
 * size: size of the region.
 * memperm: expected permission for the region.
 * store: mapped memory store. NULL if memory is anonymous.
 * ofs: offset into the memory store.
 * shared: 1:shared 0:private.
 */
struct memregion *as_map_memregion(struct addrspace *as, vaddr_t addr, size_t size,
    memperm_t perm, struct memstore *store, offset_t ofs, int shared);

/*
 * Find which region is associated with the specific address.
 * Return NULL if the address is not associated with any region.
 * size: in bytes
 */
struct memregion *as_find_memregion(struct addrspace *as, vaddr_t addr, size_t size);

/*
 * Copy a region of memory to an address space. Return the new memregion, or
 * NULL if failed to copy.
 * as: destination address space
 * src: memory region being copied
 * addr: address of the destination memory region (page aligned)
 */
struct memregion *as_copy_memregion(struct addrspace *as, struct memregion *src, vaddr_t addr);

/*
 * Print current virtual memory information of an address space.
 * as: the address space
 */
void as_meminfo(struct addrspace *as);

/*
 * Extend a region of virtual memory by size bytes.
 * End is extended size and old_bound is returned (note: size can be negative).
 * Return ERR_VM_INVALID if the resulting region would have negative extent
 * (ending address before starting address).
 * Return ERR_VM_BOUND if the extended region overlaps with other regions in the
 * address space.
 */
err_t memregion_extend(struct memregion *region, ssize_t size, vaddr_t *old_bound);

/*
 * Change permission of a region of memory.
 * Return ERR_VM_INVALID if permission error.
 */
err_t memregion_set_perm(struct memregion *region, memperm_t perm);

/*
 * Unmap and free a memory region.
 */
void memregion_unmap(struct memregion *region);

/*
 * VM utility functions
 */
static inline vaddr_t pg_round_up(vaddr_t addr)
{
    return (addr + pg_size - 1) & ~((vaddr_t)pg_size - 1);
}

static inline vaddr_t pg_round_down(vaddr_t addr)
{
    return addr & ~((vaddr_t)pg_size - 1);
}

static inline vaddr_t pg_ofs(vaddr_t addr)
{
    return addr & ((vaddr_t)pg_size - 1);
}

static inline int pg_aligned(vaddr_t addr)
{
    return (addr & ((vaddr_t)pg_size - 1)) == 0;
}

static inline int is_user_addr(vaddr_t addr)
{
    return addr < kvm_base;
}

static inline int is_kern_memperm(memperm_t perm)
{
    return perm < MEMPERM_UR;
}

static inline int is_write_memperm(memperm_t perm)
{
    return perm == MEMPERM_RW || perm == MEMPERM_URW;
}

#endif /* _VM_H_ */
