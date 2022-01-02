#include <kernel/rmap.h>
#include <kernel/kmalloc.h>
#include <kernel/console.h>
#include <lib/errcode.h>
#include <lib/stddef.h>

struct kmem_cache *rmap_allocator = NULL;

struct rmap*
rmap_alloc(void)
{
    struct rmap *rmap;
    if (rmap_allocator == NULL) {
        if ((rmap_allocator = kmem_cache_create(sizeof(struct rmap))) == NULL) {
            return NULL;
        }
    }
    if ((rmap = kmem_cache_alloc(rmap_allocator)) != NULL) {
        rmap_construct(rmap);
    }
    return rmap;
}

void
rmap_free(struct rmap *rmap)
{
    kassert(rmap);
    rmap_destroy(rmap);
    kmem_cache_free(rmap_allocator, rmap);
}

void
rmap_construct(struct rmap *rmap)
{
    kassert(rmap);
    list_init(&rmap->regions);
}

void
rmap_destroy(struct rmap *rmap)
{
    kassert(rmap);
    kassert(list_empty(&rmap->regions));
    // nothing to do
}

err_t
rmap_unmap(struct rmap *rmap, paddr_t paddr)
{
    // TODO
    return ERR_OK;
}
