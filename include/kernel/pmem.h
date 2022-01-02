#ifndef _PMEM_H_
#define _PMEM_H_

#include <kernel/kmalloc.h>
#include <kernel/rmap.h>
#include <kernel/types.h>
#include <kernel/list.h>

/*
 * Physical memory allocator.
 */

/*
 * Each physical page has an associated struct page.
 */
struct page {
    struct sleeplock lock;
    Node node;
    struct kmem_cache *kmem_cache;
    struct slab *slab;
    // reverse mapping
    struct rmap *rmap;
    // reference count
    int refcnt;
    // size of the block (power of two number of pages)
    int order;
    // Status of the page. Contains the following flags:
    // - DIRTY
    state_t state;
    // used by bdev to locate block headers
    List blk_headers;
};

/*
 * Translate physical address to struct page.
 */
struct page *paddr_to_page(paddr_t paddr);

/*
 * Translate struct page to physical page address.
 */
paddr_t page_to_paddr(const struct page *page);

/*
 * Initialize the boot memory allocator.
 */
void pmem_boot_init(void);

/*
 * Initialize the actual memory allocator.
 */
void pmem_init(void);

/*
 * Print physical memory status
 */
void pmem_info(void);

/*
 * Machine-dependent physical memory initialization: store the physical memory
 * configuration in a ``pmemconfig`` struct.
 */
struct pmemconfig {
    paddr_t pmem_start;
    paddr_t pmem_end;
};
extern struct pmemconfig pmemconfig;
void pmem_arch_init(void);

/*
 * Allocate one physical page. Store the address of the page in paddr.
 *
 * Return:
 * ERR_OK - Physical page successfully allocated.
 * ERR_NOMEM - Failed to allocate physical page.
 */
err_t pmem_alloc(paddr_t *paddr);

/*
 * Allocate n physical pages. Physical pages are guaranteed to be contiguous.
 * Store the address of the first physical page in ``paddr``.
 *
 * Return:
 * ERR_OK - n physical pages successfully allocated.
 * ERR_NOMEM - Failed to allocated physical pages.
 */
err_t pmem_nalloc(paddr_t *paddr, size_t n);

/*
 * Deallocate one physical page at ``addr``.
 */
void pmem_free(paddr_t paddr);

/*
 * Deallocate n physical pages starting at ``addr``.
 */
void pmem_nfree(paddr_t paddr, size_t n);

/*
 * Functions that get/set state of a page.
 *
 * Precondition:
 * Caller must hold page->lock.
 */
int pmem_is_page_dirty(struct page *page);
void pmem_set_page_dirty(struct page *page, int dirty);

int pmem_get_refcnt(paddr_t paddr);

/*
 * Increment the reference count of a physical page by n.
 */
void pmem_inc_refcnt(paddr_t paddr, int n);

/*
 * Decrement the reference count of a physical page by 1.
 */
void pmem_dec_refcnt(paddr_t paddr);

#endif /* _PMEM_H_ */
