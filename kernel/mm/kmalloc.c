#include <kernel/kmalloc.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/console.h>
#include <kernel/util.h>
#include <lib/string.h>
#include <lib/errcode.h>
#include <lib/stddef.h>

/*
 * We use a slab allocator [Bonwick] to reduce memory fragmentation. Each object
 * allocator dynamically allocates slabs to store fixed-size objects.
 * Each slab is divided into object-sized slots, and a linked list is used to
 * track free slots. The linked list is stored in the beginning of the slab.
 */

/*
 * Allocator for kmem_cache
 */
static struct kmem_cache allocator_cache;

/*
 * kmalloc allocators
 */
struct kmalloc_allocator {
    struct kmem_cache *kmem_cache;
    size_t size;
};
static struct kmalloc_allocator kmalloc_allocators[] =
{
    { NULL, 32 },
    { NULL, 64 },
    { NULL, 128 },
    { NULL, 256 },
    { NULL, 512 },
    { NULL, 1024 },
    { NULL, 2048 },
    { NULL, 4096 }
};

/*
 * Create a new slab for an object allocator. The slab should fit at least
 * MIN_OBJS_PER_SLAB objects.
 */
#define MIN_OBJS_PER_SLAB 128
static struct slab *slab_create(struct kmem_cache *kmem_cache);

/*
 * Destroy a slab.
 */
static void slab_destroy(struct slab *slab);

/*
 * Return the free index array of a slab.
 */
#define SLAB_FREEARR(s) ((int*)(s + 1))

/*
 * Destroy all slabs in the linked list.
 */
static void slab_list_destroy(List *list);

static struct slab*
slab_create(struct kmem_cache *kmem_cache)
{
    paddr_t paddr;
    size_t n_pages, n_objs, i;
    struct slab *slab;
    struct page *page;

    kassert(kmem_cache);
    kassert(kmem_cache->obj_size > 0);

    // We store at least MIN_OBJS_PER_SLAB objects, the slab structure, and the
    // free list (MIN_OBJS_PER_SLAB indices) in the slab
    n_pages = pg_round_up(sizeof(struct slab) + (kmem_cache->obj_size + sizeof(int)) * MIN_OBJS_PER_SLAB) / pg_size;
    if (pmem_nalloc(&paddr, n_pages) != ERR_OK) {
        return NULL;
    }

    // The actual number of objects that can be stored may be greater than
    // MIN_OBJS_PER_SLAB
    n_objs = (n_pages * pg_size - sizeof(struct slab)) / (kmem_cache->obj_size + sizeof(int));
    kassert(n_objs >= MIN_OBJS_PER_SLAB);

    slab = (struct slab*)kmap_p2v(paddr);
    kassert(slab);
    slab->objs = &SLAB_FREEARR(slab)[n_objs]; // objects placed after free index array
    slab->free = 0;
    slab->n_pages = n_pages;

    // Link allocator and slab into each allocated page's page structure. The
    // purpose is for easy lookup during free.
    for (i = 0; i < n_pages; i++, paddr += pg_size) {
        page = paddr_to_page(paddr);
        kassert(page);
        page->kmem_cache = kmem_cache;
        page->slab = slab;
    }

    // Initialize the free index array
    for (i = 0; i < n_objs - 1; i++) {
        SLAB_FREEARR(slab)[i] = i + 1;
    }
    SLAB_FREEARR(slab)[n_objs - 1] = -1; // end marker

    // Add slab to the allocator
    list_append(&kmem_cache->free, &slab->node);

    return slab;
}

static void
slab_destroy(struct slab *slab)
{
    kassert(slab);
    pmem_nfree(kmap_v2p((vaddr_t)slab), slab->n_pages);
}

static void
slab_list_destroy(List *list)
{
    Node *curr, *next;

    kassert(list);
    curr = list_begin(list);
    while (curr != list_end(list)) {
        next = list_remove(curr);
        slab_destroy(list_entry(curr, struct slab, node));
        curr = next;
    }
}

void
kmalloc_init(void)
{
    struct kmalloc_allocator *ka;

    // Initialize allocator cache
    list_init(&allocator_cache.full);
    list_init(&allocator_cache.free);
    spinlock_init(&allocator_cache.lock);
    allocator_cache.obj_size = sizeof(struct kmem_cache);

    // Initialize kmalloc allocators
    for (ka = kmalloc_allocators; ka < &kmalloc_allocators[N_ELEM(kmalloc_allocators)]; ka++) {
        if ((ka->kmem_cache = kmem_cache_create(ka->size)) == NULL) {
            panic("Failed to allocate kmalloc allocator");
        }
    }
}

struct kmem_cache*
kmem_cache_create(size_t size)
{
    struct kmem_cache *kmem_cache;

    if ((kmem_cache = kmem_cache_alloc(&allocator_cache)) == NULL) {
        return NULL;
    }

    list_init(&kmem_cache->full);
    list_init(&kmem_cache->free);
    spinlock_init(&kmem_cache->lock);
    kmem_cache->obj_size = size;

    return kmem_cache;
}

void
kmem_cache_destroy(struct kmem_cache *kmem_cache)
{
    kassert(kmem_cache);

    // Destroy all slabs
    slab_list_destroy(&kmem_cache->free);
    slab_list_destroy(&kmem_cache->full);

    // Free the object allocator
    kmem_cache_free(&allocator_cache, kmem_cache);
}

void*
kmem_cache_alloc(struct kmem_cache *kmem_cache)
{
    struct slab *slab;
    void *obj;

    kassert(kmem_cache);

    spinlock_acquire(&kmem_cache->lock);
    // Find a slab that still have free slots. Allocate a new slab if no free
    // slab is found.
    if (list_empty(&kmem_cache->free)) {
        if ((slab = slab_create(kmem_cache)) == NULL) {
            goto fail;
        }
    } else {
        slab = list_entry(list_begin(&kmem_cache->free), struct slab, node);
    }

    // Allocate an object from the slab.
    kassert(slab);
    kassert(slab->free != -1);
    obj = (void*)((vaddr_t)slab->objs + kmem_cache->obj_size * slab->free);
    slab->free = SLAB_FREEARR(slab)[slab->free];

    if (slab->free == -1) {
        // slab is full
        list_remove(&slab->node);
        list_append(&kmem_cache->full, &slab->node);
    }
    spinlock_release(&kmem_cache->lock);
    memset(obj, 0x2b, kmem_cache->obj_size);
    return obj;

fail:
    spinlock_release(&kmem_cache->lock);
    return NULL;
}

void
kmem_cache_free(struct kmem_cache *kmem_cache, void *obj)
{
    struct slab *slab;
    struct page *page;
    paddr_t paddr;
    int index, full;

    kassert(kmem_cache);

    spinlock_acquire(&kmem_cache->lock);
    // memset freed object to 0x2b to detect uses of freed memory
    memset(obj, 0x2b, kmem_cache->obj_size);
    // Find the slab the object belongs to
    paddr = kmap_v2p((vaddr_t)obj);
    page = paddr_to_page(paddr);
    kassert(page);
    slab = page->slab;
    kassert(slab);
    full = slab->free == -1;

    // Add object to the free list
    index = ((vaddr_t)obj - (vaddr_t)slab->objs) / kmem_cache->obj_size;
    SLAB_FREEARR(slab)[index] = slab->free;
    slab->free = index;

    // If slab was full, move it to the free slabs list
    if (full) {
        list_remove(&slab->node);
        list_append(&kmem_cache->free, &slab->node);
    }
    spinlock_release(&kmem_cache->lock);
}

void*
kmalloc(size_t size)
{
    struct kmalloc_allocator *alloc;

    if (size == 0) {
        return NULL;
    }

    if (size > kmalloc_allocators[N_ELEM(kmalloc_allocators) - 1].size) {
        // We don't have an allocator capable of allocating ``size`` bytes
        return NULL;
    }

    // Find kmalloc allocator with a big enough size
    for (alloc = kmalloc_allocators; alloc < &kmalloc_allocators[N_ELEM(kmalloc_allocators)]; alloc++) {
        if (alloc->size >= size) {
            return kmem_cache_alloc(alloc->kmem_cache);
        }
    }

    panic("Unreachable");
}

void
kfree(void *ptr)
{
    struct kmem_cache *kmem_cache;
    struct page *page;
    paddr_t paddr;

    // Find the kmalloc allocator the memory belongs to
    paddr = kmap_v2p((vaddr_t)ptr);
    page = paddr_to_page(paddr);
    kassert(page);
    kmem_cache = page->kmem_cache;
    kassert(kmem_cache);

    kmem_cache_free(kmem_cache, ptr);
}
