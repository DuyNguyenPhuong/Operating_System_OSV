#include <kernel/vm.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/kmalloc.h>
#include <kernel/console.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/memstore.h>
#include <kernel/list.h>
#include <lib/errcode.h>
#include <arch/mmu.h>
#include <lib/string.h>
#include <lib/stddef.h>

/* Memory allocator for memregions */
static struct kmem_cache *memregion_allocator;

/*
 * Allocate kas and kvpmap in the data section of the kernel image, so that we
 * can use them before kernel vm is initialized.
 */
struct addrspace _kas;
struct vpmap _kvpmap;
struct addrspace *kas = &_kas;
struct vpmap *kvpmap = &_kvpmap;

/* Initialize the kernel address space, only called once */
static void kas_init(void);

/* memregion comparator used to keep memregion in order by their addresses */
static int memregion_comparator(const Node *a, const Node *b, void *aux);

static void memregion_unmap_internal(struct memregion *region);

static struct memregion* memregion_map_internal(struct addrspace *as, vaddr_t addr, size_t size, 
        memperm_t perm, struct memstore *store, offset_t ofs, int shared);

static struct memregion* memregion_copy_internal(struct addrspace *as, 
        struct memregion *src, vaddr_t addr);

static struct memregion* memregion_find_internal(struct addrspace *as, vaddr_t addr, size_t size);

/* find free memory addresses of size ``size`` starting at *ret_addr */
static err_t find_free_vaddr(struct addrspace *as, size_t size, vaddr_t *ret_addr);

static char *perm_strings[] = {
    [MEMPERM_R] = "Kernel Read-only",
    [MEMPERM_RW] = "Kernel Read-write",
    [MEMPERM_UR] = "User Read-only",
    [MEMPERM_URW] = "User Read-write",
};

static void
kas_init(void)
{
    sleeplock_init(&kas->as_lock);
    list_init(&kas->regions);
    kas->vpmap = kvpmap;
}

void
vm_init(void)
{
    pmem_boot_init();
    vpmap_init();
    pmem_init(); 
    kmalloc_init();

    if ((memregion_allocator = kmem_cache_create(sizeof(struct memregion))) == NULL) {
        panic("vm init: failed to create memregion allocator");
    }

    // Initialize kernel address space
    kas_init();
}

err_t
as_init(struct addrspace* as)
{
    kassert(as);
    sleeplock_init(&as->as_lock);
    list_init(&as->regions);
    if ((as->vpmap = vpmap_create()) == NULL) {
        return ERR_VM_RESOURCE_UNAVAIL;
    }
    as->heap = NULL;
    // maybe we should copy in kvm here, every as starts implictly with kas
    // NOTE: temp hack, go through kas and copy all region
    return vpmap_copy_kernel_mapping(as->vpmap);
}

void
as_destroy(struct addrspace *as)
{
    kassert(as);
    kassert(as != kas); // Cannot destroy kernel address space
    sleeplock_acquire(&as->as_lock);
    vpmap_destroy(as->vpmap);
    as->vpmap = NULL; // make sure memregion_unmap won't walk page tables

    for (Node *n = list_begin(&as->regions); n != list_end(&as->regions);) {
        struct memregion *region = (struct memregion*) list_entry(n, struct memregion, as_node);
        // We are going to destroy the region, so advance node pointer now
        n = list_next(n);
        memregion_unmap_internal(region);
    }
    sleeplock_release(&as->as_lock);
}

err_t
as_copy_as(struct addrspace *src_as, struct addrspace *dst_as)
{
    kassert(src_as && dst_as);
    kassert(src_as != dst_as);
    err_t err = ERR_OK;

    // grab both locks before we move on
    sleeplock_acquire(&src_as->as_lock);
    while (sleeplock_try_acquire(&dst_as->as_lock) != ERR_OK) {
        sleeplock_release(&src_as->as_lock);
        sleeplock_acquire(&src_as->as_lock);
    }

    // go through all src regions and copy them
    for (Node *n = list_begin(&src_as->regions); n != list_end(&src_as->regions); n = list_next(n)) {
        struct memregion *r = list_entry(n, struct memregion, as_node);
        struct memregion *dst_r = memregion_copy_internal((struct addrspace*) dst_as, r, r->start);
        if (dst_r == NULL) {
            err = ERR_NOMEM;
            break;
        }
        // if copying heap region, set the dst_as's heap
        if (r == src_as->heap) {
            dst_as->heap = dst_r;
        }
    }
    sleeplock_release(&src_as->as_lock);
    sleeplock_release(&dst_as->as_lock);
    return err;
}

struct memregion*
as_map_memregion(struct addrspace *as, vaddr_t addr, size_t size, memperm_t perm, 
                    struct memstore *store, offset_t ofs, int shared)
{
    kassert(as);
    struct memregion *r;

    sleeplock_acquire(&as->as_lock);
    r = memregion_map_internal(as, addr, size, perm, store, ofs, shared);
    sleeplock_release(&as->as_lock);
    return r;
}

struct memregion*
as_find_memregion(struct addrspace *as, vaddr_t addr, size_t size)
{
    kassert(as);
    struct memregion *r;

    if (addr > addr+size) {
        return NULL;
    }
    sleeplock_acquire(&as->as_lock);
    r = memregion_find_internal(as, addr, size);
    sleeplock_release(&as->as_lock);
    return r;
}

struct memregion*
as_copy_memregion(struct addrspace *as, struct memregion *src, vaddr_t addr)
{
    struct memregion *dst = NULL;

    kassert(as);
    kassert(src);
    kassert(pg_aligned(addr));

    // grab both locks before we move on
    sleeplock_acquire(&as->as_lock);
    if (as != src->as) {
        while (sleeplock_try_acquire(&src->as->as_lock) != ERR_OK) {
            sleeplock_release(&as->as_lock);
            sleeplock_acquire(&as->as_lock);
        }
    }
    
    dst = memregion_copy_internal(as, src, addr);

    sleeplock_release(&as->as_lock);
    if (as != src->as) {
        sleeplock_release(&src->as->as_lock);
    }
    return dst;
}

void
as_meminfo(struct addrspace *as)
{    
    sleeplock_acquire(&as->as_lock);
    List *list = &as->regions;
    for (Node *n = list_begin(list); n != list_end(list); n = list_next(n)) {
        struct memregion *r = (struct memregion*) list_entry(n, struct memregion, as_node);
        kprintf("[%p - %p] %s | shared: %d \n", r->start, r->end, perm_strings[r->perm], r->shared);
    }
    sleeplock_release(&as->as_lock);
}

static inline int isprint (int c) { return c >= 32 && c < 127; }

void
as_dump(struct addrspace *as, vaddr_t vaddr)
{
    paddr_t paddr;
    size_t *data;
    struct memregion *region;

    sleeplock_acquire(&as->as_lock);
    List *list = &as->regions;
    for (Node *n = list_begin(list); n != list_end(list); n = list_next(n)) {
        region = (struct memregion*) list_entry(n, struct memregion, as_node);
        if (vaddr >= region->start && vaddr + sizeof(size_t) < pg_round_up(region->end)) {
            goto found;
        }
    }
    sleeplock_release(&as->as_lock);
    kprintf("memregion containing addr %p is not found\n", vaddr);
    return;
found:
    kprintf("dumping memregion %p to %p\n", vaddr, region->end);
    for (; vaddr < region->end; vaddr += pg_size) {
        // dumped memregion must be mapped
        if (vpmap_lookup_vaddr(as->vpmap, vaddr, &paddr, NULL) != ERR_OK) {
            sleeplock_release(&as->as_lock);
            kprintf("stoping at %p because address not currently mapped in memory\n", vaddr);
            return;
        }
        // dump memory content
        for (data = (size_t*)kmap_p2v(paddr); (vaddr_t)data < pg_round_up(kmap_p2v(paddr)); data += 1) {
            kprintf("vaddr: %p | data (hex):  ", vaddr);
            for (char *c = (char*)data; c < (char*)(data + 1); c++) {
                kprintf("%p%c", (*c & 0xff), ' ');
            }
            kprintf(" | data (ascii): ");
            for (char *c = (char*)data; c < (char*)(data + 1); c++) {
                kprintf("%c", isprint(*c) ? *c : '.');
            }
            kprintf("|\n");
            vaddr += sizeof(*data);
        }
    }
    sleeplock_release(&as->as_lock);
}

err_t
memregion_extend(struct memregion *region, ssize_t size, vaddr_t *old_bound)
{
    return ERR_OK;
}

err_t
memregion_set_perm(struct memregion *region, memperm_t perm)
{
    kassert(region);
    // User address space cannot have kernel permissions
    if (is_kern_memperm(perm) && region->as != kas) {
        return ERR_VM_INVALID;
    }

    sleeplock_acquire(&region->as->as_lock);
    // Update memory mappings
    vpmap_set_perm(region->as->vpmap, region->start, pg_round_up(region->end - region->start)/pg_size, perm);
    region->perm = perm;
    vpmap_flush_tlb();
    sleeplock_release(&region->as->as_lock);
    return ERR_OK;
}

void
memregion_unmap(struct memregion *region)
{
    struct addrspace *as = region->as;
    sleeplock_acquire(&as->as_lock);
    memregion_unmap_internal(region);
    sleeplock_release(&as->as_lock);
}

static int
memregion_comparator(const Node *a, const Node *b, void *aux)
{
    struct memregion *mr_a = list_entry(a, struct memregion, as_node);
    struct memregion *mr_b = list_entry(b, struct memregion, as_node);
    // if a and b's memregion is consecutive, we want a after b in the list, +1 bumps 0 to postivie
    return mr_a->start >= mr_b->end;
}

static err_t
find_free_vaddr(struct addrspace *as, size_t size, vaddr_t *ret_addr)
{
    kassert(as != kas); // should only be used finding user addresses
    kassert(ret_addr);
    kassert(as->as_lock.holder == thread_current());

    List *list = &as->regions;
    // start looking at first memregion address if exists
    vaddr_t addr = 0;
    if (!list_empty(list)) {
        addr = ((struct memregion*)list_entry(list_begin(list), struct memregion, as_node))->end;
    }

    // memregion address goes up, only need to check if end is smaller than the start
    for (Node *n = list_begin(list); n != list_end(list); n = list_next(n)) {
        struct memregion *r = list_entry(n, struct memregion, as_node);
        if (pg_round_up(addr + size) < r->start) {
            *ret_addr = addr;
            return ERR_OK;
        }
        addr = pg_round_up(r->end);
    }
    // check address space after the last memregion allocated
    if (pg_round_up(addr + size) < USTACK_LOWERBOUND - size) {
        *ret_addr = addr;
        return ERR_OK;
    }

    return !ERR_OK;
}

static void
memregion_unmap_internal(struct memregion *region)
{
    kassert(region);
    kassert(region->as->as_lock.holder == thread_current());

    // Remove all memory mappings
    vpmap_unmap(region->as->vpmap, region->start,
            pg_round_up(region->end - region->start) / pg_size, 1);
    // Detach from address space
    list_remove(&region->as_node);
    vpmap_flush_tlb();
    kmem_cache_free(memregion_allocator, region);
}

static struct memregion*
memregion_map_internal(struct addrspace *as, vaddr_t addr, size_t size, memperm_t perm, 
                        struct memstore *store, offset_t ofs, int shared)
{
    kassert(as);
    kassert(as->as_lock.holder == thread_current());

    struct memregion *r;
    if (addr == ADDR_ANYWHERE && find_free_vaddr(as, size, &addr) != ERR_OK) {
        return NULL;
    }

    kassert(pg_aligned(addr));
    kassert((addr + pg_round_up(size)) >= addr);

    if (as != kas && (is_kern_memperm(perm) || !is_user_addr(pg_round_up(addr+size)&(~0xFFF)))) {
        return NULL;
    }
    // Fail if any address in the range overlaps with an existing region
    if (memregion_find_internal(as, addr, addr+size)) {
        return NULL;
    }

    if ((r = kmem_cache_alloc(memregion_allocator)) == NULL) {
        return NULL;
    }

    // Link into address space's region list and memstore's reverse mapping
    list_append_ordered(&as->regions, &r->as_node, memregion_comparator, NULL);
    
    r->as = as;
    r->start = addr;
    r->end = addr + size;
    r->perm = perm;
    r->shared = shared;
    r->store = store;
    r->ofs = ofs;
    return r;
}

// lock for as and src as must be held
static struct memregion*
memregion_copy_internal(struct addrspace *as, struct memregion *src, vaddr_t addr)
{
    struct memregion *dst;
    // Try mapping a region with the same attributes as the source
    if ((dst = memregion_map_internal(as, addr, src->end - src->start, 
            src->perm, src->store, src->ofs, src->shared)) != NULL) {
        // hard copy over everything
        if (vpmap_copy(src->as->vpmap, as->vpmap, src->start, addr,
             pg_round_up(src->end - src->start)/pg_size, src->perm) != ERR_OK) {
            memregion_unmap_internal(dst);
            return NULL;
        }
    }
    return dst;
}

static struct memregion*
memregion_find_internal(struct addrspace *as, vaddr_t addr, size_t size)
{
    List *list = &as->regions;
    for (Node *n = list_begin(list); n != list_end(list); n = list_next(n)) {
        struct memregion *r = (struct memregion*) list_entry(n, struct memregion, as_node);
        if (addr >= r->start && addr+size <= pg_round_up(r->end)) {
            return r;
        }
    }
    return NULL;
}
