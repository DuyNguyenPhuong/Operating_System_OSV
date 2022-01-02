#ifndef _MEMSTORE_H_
#define _MEMSTORE_H_

#include <kernel/rmap.h>
#include <kernel/synch.h>
#include <kernel/types.h>
#include <kernel/radix_tree.h>

#define ERR_MEMSTORE_NOMEM 1
#define ERR_MEMSTORE_NORES 2
#define ERR_MEMSTORE_IO 3

struct page;

/*
 * Memory backing store. A memory store can be a regular file, a swap file, an
 * I/O device, etc. A memory store needs to implement two functions: read and
 * write. The VM system uses these two functions to handle page fault and page
 * out.
 */
struct memstore {
    /*
     * Private data for specific memstore implementation.
     */
    void *info;

    /*
     * Reverse mapping.
     */
    struct rmap rmap;

    /*
     * Radix tree to track the set of memstore pages currently cached in
     * physical memory.
     */
    struct sleeplock pgcache_lock;
    struct radix_tree_root cached_pages;

    /*
     * Fill a page with data read from this store at the offset position. Each
     * type of memstore implements its own version of the fillpage function.
     * Return:
     * ERR_MEMSTORE_NOMEM if failed to allocate memory.
     * ERR_MEMSTORE_IO for I/O errors.
     */
    err_t (*fillpage)(struct memstore*, offset_t, struct page*);

    /*
     * Write a page to this store.
     * Function prototype:
     * err_t write(struct memstore *this, paddr_t paddr, offset_t ofs);
     */
    err_t (*write)(struct memstore*, paddr_t, offset_t);
};

/*
 * Allocate a memstore. Return NULL if failed to allocate.
 */
struct memstore *memstore_alloc(void);

/*
 * Free a memstore. Specific memstore implementation should clean up store->data
 * before calling this function.
 */
void memstore_free(struct memstore *store);

#endif /* _MEMSTORE_H_ */
