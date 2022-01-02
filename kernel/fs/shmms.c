#include <kernel/vm.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/shmms.h>
#include <kernel/pgcache.h>
#include <kernel/console.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>

/*
 * shared memory memstore fillpage function.
 */
static err_t fillpage(struct memstore *store, offset_t ofs, struct page *page);

/*
 * Shared memory memstore write function.
 */
static err_t write(struct memstore *store, paddr_t paddr, offset_t ofs);


static err_t
fillpage(struct memstore *store, offset_t ofs, struct page *page)
{
    // Fill in 0s
    kassert(store);
    kassert(page);

    memset((void*)kmap_p2v(page_to_paddr(page)), 0, pg_size);
    return ERR_OK;
}

static err_t
write(struct memstore *store, paddr_t paddr, offset_t ofs)
{
    // TODO
    return ERR_OK;
}

struct memstore*
shmms_alloc(void)
{
    struct memstore *store;

    if ((store = memstore_alloc()) != NULL) {
        // TODO: initialize shared memory specific metadata
        store->fillpage = fillpage;
        store->write = write;
    }
    return store;
}

void
shmms_free(struct memstore *store)
{
    kassert(store);
    memstore_free(store);
}
