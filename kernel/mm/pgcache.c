#include <kernel/pgcache.h>
#include <kernel/console.h>
#include <kernel/vm.h>
#include <kernel/kmalloc.h>
#include <kernel/radix_tree.h>
#include <kernel/memstore.h>
#include <kernel/pmem.h>
#include <lib/errcode.h>

struct page*
pgcache_get_page(struct memstore *store, offset_t ofs)
{
    struct page *page;
    paddr_t paddr;

    kassert(store);
    paddr = PADDR_NONE;

    if ((page = radix_tree_lookup(&store->cached_pages, ofs / pg_size)) == NULL) {
        // Page not found in cache -- allocate a new page, and update the page
        // with data read from the backing store
        if (pmem_alloc(&paddr) != ERR_OK) {
            return NULL;
        }
        page = paddr_to_page(paddr);
        if (store->fillpage(store, ofs, page) != ERR_OK) {
            pmem_free(paddr);
            return NULL;
        }
        switch (radix_tree_insert(&store->cached_pages, ofs / pg_size, page)) {
            case ERR_RADIX_TREE_ALLOC:
                pmem_free(paddr);
                return NULL;
            case ERR_RADIX_TREE_NODE_EXIST:
                panic("node should not exist");
        }
    }

    return page;
}

void
pgcache_remove_page(struct memstore *store, offset_t ofs)
{
    kassert(store);
    radix_tree_remove(&store->cached_pages, ofs / pg_size);
}
