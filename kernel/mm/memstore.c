#include <kernel/memstore.h>
#include <kernel/kmalloc.h>
#include <kernel/console.h>
#include <lib/string.h>
#include <lib/stddef.h>

static struct kmem_cache *memstore_allocator = NULL;

struct memstore*
memstore_alloc(void)
{
    struct memstore *store;

    if (memstore_allocator == NULL) {
        if ((memstore_allocator = kmem_cache_create(sizeof(struct memstore))) == NULL) {
            return NULL;
        }
    }
    if ((store = kmem_cache_alloc(memstore_allocator)) != NULL) {
        rmap_construct(&store->rmap);
        sleeplock_init(&store->pgcache_lock);
        radix_tree_construct(&store->cached_pages);
    }
    return store;
}

// TODO: file data, we can take in parameter to specify write data location

void
memstore_free(struct memstore *store)
{
    kassert(store);
    rmap_destroy(&store->rmap);
    kmem_cache_free(memstore_allocator, store);
}
