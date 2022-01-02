#include <kernel/vm.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/bdevms.h>
#include <kernel/pgcache.h>
#include <kernel/bdev.h>
#include <kernel/console.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>

static struct kmem_cache *bdevms_allocator = NULL;

/*
 * Bdev memstore fillpage function.
 */
static err_t fillpage(struct memstore *store, offset_t ofs, struct page *page);

/*
 * Bdev memstore write function.
 */
static err_t write(struct memstore *store, paddr_t paddr, offset_t ofs);

static err_t
fillpage(struct memstore *store, offset_t ofs, struct page *page)
{
    struct bdevms_info *info;
    struct bio *bio;

    kassert(store);
    kassert(store->info);
    kassert(page);
    info = (struct bdevms_info*)store->info;
    if ((bio = bio_alloc()) == NULL) {
        return ERR_MEMSTORE_NOMEM;
    }
    bio->bdev = info->bdev;
    // Direct translation: use memstore offset as raw address for the block device
    bio->blk = pg_round_down(ofs) / BDEV_BLK_SIZE;
    bio->size = pg_size / BDEV_BLK_SIZE;
    bio->buffer = (void*)kmap_p2v(page_to_paddr(page));
    bio->op = BIO_READ;
    bdev_make_request(bio);
    bio_free(bio);
    return ERR_OK;
}

static err_t
write(struct memstore *store, paddr_t paddr, offset_t ofs)
{
    // TODO
    return ERR_OK;
}

struct memstore*
bdevms_alloc(struct bdev *bdev)
{
    struct memstore *store;
    struct bdevms_info *info;

    kassert(bdev);
    if (bdevms_allocator == NULL) {
        if ((bdevms_allocator = kmem_cache_create(sizeof(struct bdevms_info))) == NULL) {
            return NULL;
        }
    }
    if ((store = memstore_alloc()) != NULL) {
        if ((store->info = kmem_cache_alloc(bdevms_allocator)) != NULL) {
            info = (struct bdevms_info*)store->info;
            store->fillpage = fillpage;
            store->write = write;
            info->bdev = bdev;
        } else {
            memstore_free(store);
            store = NULL;
        }
    }
    return store;
}

void
bdevms_free(struct memstore *store)
{
    kassert(store);
    kassert(store->info);
    kmem_cache_free(bdevms_allocator, store->info);
    memstore_free(store);
}
