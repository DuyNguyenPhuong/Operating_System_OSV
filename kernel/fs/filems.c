#include <kernel/vm.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/filems.h>
#include <kernel/pgcache.h>
#include <kernel/console.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>

static struct kmem_cache *filems_allocator = NULL;

/*
 * File memstore fillpage function.
 */
static err_t fillpage(struct memstore *store, offset_t ofs, struct page *page);

/*
 * File memstore write function.
 */
static err_t write(struct memstore *store, paddr_t paddr, offset_t ofs);

static err_t
fillpage(struct memstore *store, offset_t ofs, struct page *page)
{
    struct filems_info *info;

    kassert(store);
    kassert(store->info);
    kassert(page);
    info = (struct filems_info*)store->info;
    if (info->inode->i_ops->fillpage(info->inode, pg_round_down(ofs), page) != ERR_OK) {
        return ERR_MEMSTORE_IO;
    }
    return ERR_OK;
}

static err_t
write(struct memstore *store, paddr_t paddr, offset_t ofs)
{
    // TODO
    return ERR_OK;
}

struct memstore*
filems_alloc(struct inode *inode)
{
    struct memstore *store;
    struct filems_info *info;

    // must be an inode and has no filems stored
    kassert(inode && inode->store == NULL);
    if (filems_allocator == NULL) {
        if ((filems_allocator = kmem_cache_create(sizeof(struct filems_info))) == NULL) {
            return NULL;
        }
    }
    if ((store = memstore_alloc()) != NULL) {
        if ((store->info = kmem_cache_alloc(filems_allocator)) != NULL) {
            info = (struct filems_info*)store->info;
            store->fillpage = fillpage;
            store->write = write;
            info->inode = inode;
        } else {
            memstore_free(store);
            store = NULL;
        }
    }
    return store;
}

void
filems_free(struct memstore *store)
{
    kassert(store);
    kassert(store->info);
    kmem_cache_free(filems_allocator, store->info);
    memstore_free(store);
}
