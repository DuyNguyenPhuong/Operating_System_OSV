#ifndef _PGCACHE_H_
#define _PGCACHE_H_

/*
 * Page Cache.
 */
#include <kernel/types.h>

/* inform compiler that these structs exist */
struct page;
struct memstore;

/*
 * Query a page from the page cache. If the page is not present in the cache,
 * read the page using the memstore, and store the page into the cache.
 *
 * Precondition:
 * Caller must hold store->pgcache_lock.
 *
 * Return:
 * NULL if failed to read the page from the memstore.
 */
struct page *pgcache_get_page(struct memstore *store, offset_t ofs);

/*
 * Remove a cached page from the page cache.
 *
 * Precondition:
 * Caller must hold store->pgcache_lock.
 */
void pgcache_remove_page(struct memstore *memstore, offset_t ofs);

#endif /* _PGCACHE_H_ */
