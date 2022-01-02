#include <kernel/pmem.h>
#include <kernel/vm.h>
#include <kernel/console.h>
#include <kernel/vpmap.h>
#include <lib/errcode.h>
#include <lib/string.h>
#include <lib/stddef.h>
#include <lib/bits.h>

/*
 * pmem contains two memory allocators:
 *
 * 1. A boot memory allocator: this allocator is used to bootstrap some kernel
 * structures, including the binary buddy memory allocator. The boot memory
 * allocator keeps a bitmap of free pages, and uses a simple first-fit
 * algorithm.
 *
 * 2. A binary buddy allocator [Knuth]. Physical memory is divided into
 * blocks where each block is a power of two page size. pmem creates a linked
 * list of free blocks for each size. When allocating memory for a certain page
 * size, pmem searches the corresponding list. If no free block is available, it
 * finds a bigger block in the next list, splits the block into two, puts one of
 * them in the current free list and returns the other one. The two blocks are
 * called "buddies". When two buddy blocks are both freed, they merge into a
 * bigger block and is moved to the next free list.
 */

struct pmemconfig pmemconfig;

// Page state bits
#define PAGE_DIRTY_BIT 0

// Lock protecting page allocation and deallocation
static struct spinlock pmem_lock;

/*
 * Bitmap used by the boot memory allocator
 */
static uint8_t *bitmap;
// start index of contiguous unallocated boot memory
static int bitmap_contig_start;
// end index of allocatable boot memory
static int bitmap_end;

/*
 * pagemap stores struct page for all physical pages.
 */
static struct page *pagemap;
static struct page *pagemap_end;
static bool pagemap_initialized;

/*
 * freeblocks keeps a linked list of free blocks for each order n, up to
 * MAX_ORDER.
 */
#define MAX_ORDER 10
static List freeblocks[MAX_ORDER+1];

/*
 * Initialize bitmap for the boot memory allocator.
 */
static void bitmap_init(void);

/*
 * Set one bit in bitmap.
 */
#define BITMAP_SET(i) bitmap[(i)/8] |= 1 << ((i) % 8)

/*
 * Unset one bit in bitmap.
 */
#define BITMAP_UNSET(i) bitmap[(i)/8] &= ~(1 << ((i) % 8))

/*
 * Check if bit is set in bitmap.
 */
#define BITMAP_ISSET(i) bitmap[(i)/8] & (1 << ((i) % 8))

/*
 * Convert bitmap index to physical address
 */
#define BITMAP_ITOP(i) (i) * pg_size + pmemconfig.pmem_start

/*
 * Convert physical address to bitmap index
 */
#define BITMAP_PTOI(p) ((p) - pmemconfig.pmem_start) / pg_size

/*
 * Mark n pages starting from index as allocated in bitmap.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 */
static void bitmap_alloc(int index, size_t n);

/*
 * Mark n pages starting from index as free in bitmap.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 */
static void bitmap_free(int index, size_t n);

/*
 * Find the first occurrence of n contiguous free pages in bitmap. Return the
 * index of the starting page, or -1 if not found.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 */
static int bitmap_find(size_t n);

/*
 * Initialize pagemap.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 */
static void pagemap_init(void);

/*
 * Initialize freeblocks.
 *
 * Preconditoin:
 * Caller must hold pmem_lock.
 */
static void freeblocks_init(void);

/*
 * Print freeblocks (only the first MAX_PRINT_BLOCKS blocks in each list).
 */
#define MAX_PRINT_BLOCKS 5
static void freeblocks_print(void);

/*
 * Calculate the largest page order that starts at address start and does
 * not exceed end.
 */
static int get_max_page_order(paddr_t start, paddr_t end);

/*
 * Calculate the minimum page order that is at least n pages.
 */
static int get_min_page_order(size_t n);

/*
 * Find a free block with the specified order. If split is true, split the
 * block into two, return one of them, and inserts the other one into the free
 * list of order - 1. If no free block is found, recursively search
 * the next order.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 *
 * Return:
 * NULL - Failed to find a free block.
 */
static struct page *find_freeblock(int order, bool split);

/*
 * Find page's buddy block.
 */
static struct page *find_buddy(struct page *page);

/*
 * Merge block with its buddy if the buddy block is also unallocated. If
 * successfully merged, recursively merge with the higher order buddy block.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 *
 * Return the merged block.
 */
static struct page *merge_block(struct page *page);

/*
 * Insert page into the free list.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 */
static void freeblocks_insert(struct page *page);

/*
 * Add range of pages to freeblocks.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 */
static void freeblocks_insert_range(paddr_t start, paddr_t end);

/*
 * Remove page from the free list.
 *
 * Precondition:
 * Caller must hold pmem_lock.
 */
static void freeblocks_remove(struct page *page);

/*
 * Implementation of pmem_nalloc. Argument lock indicates if the function should
 * acquire/release pmem_lock.
 */
static err_t pmem_nalloc_internal(paddr_t *paddr, size_t n, bool lock);

/*
 * Implementation of pmem_nfree. Argument lock indicates if the function should
 * acquire/release pmem_lock.
 */
static void pmem_nfree_internal(paddr_t paddr, size_t n, bool lock);

static void
bitmap_init(void)
{
    int bitmap_n_pages;

    kassert(pmemconfig.pmem_start < pmemconfig.pmem_end);
    kassert(pg_aligned(pmemconfig.pmem_start));
    kassert(pg_aligned(pmemconfig.pmem_end));

    // bitmap is mapped in the KMAP region. We allocate the first n available
    // physical pages to hold bitmap.
    bitmap = (uint8_t*)kmap_p2v(pmemconfig.pmem_start);

    bitmap_contig_start = 0;
    bitmap_n_pages = pg_round_up(((pmemconfig.pmem_end - pmemconfig.pmem_start) / pg_size) / 8) / pg_size;
    bitmap_end = (pmemconfig.pmem_end - pmemconfig.pmem_start) / pg_size;
    kassert(bitmap_n_pages < bitmap_end);
    memset(bitmap, 0, pg_size * bitmap_n_pages);

    // Mark pages used by bitmap as allocated
    bitmap_alloc(0, bitmap_n_pages);
}


static void
bitmap_alloc(int index, size_t n)
{
    int i;

    kassert(index + n <= bitmap_end);

    for (i = 0; i < n; i++) {
        kassert(!(BITMAP_ISSET(index + i)));
        BITMAP_SET(index + i);
    }

    if (index + n > bitmap_contig_start) {
        bitmap_contig_start = index + n;
    }
}

static void
bitmap_free(int index, size_t n)
{
    int i;

    kassert(index + n <= bitmap_end);
    kassert(index + n <= bitmap_contig_start);

    for (i = 0; i < n; i++) {
        kassert(BITMAP_ISSET(index + i));
        BITMAP_UNSET(index + i);
    }

    if (index + n == bitmap_contig_start) {
        // Scan for contiguous free pages
        while (index > 0) {
            if (BITMAP_ISSET(index - 1)) {
                break;
            }
            index--;
        }
        bitmap_contig_start = index;
    }
}

static int
bitmap_find(size_t n)
{
    int i, first, n_free_pages;

    kassert(n > 0);
    for (i = 0, first = 0, n_free_pages = 0; i < bitmap_end; i++) {
        if (BITMAP_ISSET(i)) {
            n_free_pages = 0;
            first++;
        } else {
            if (++n_free_pages >= n) {
                return first;
            }
        }
    }
    return -1;
}

static void
pagemap_init(void)
{
    int n_pages, pagemap_n_pages;
    paddr_t paddr;

    n_pages = pmemconfig.pmem_end / pg_size;
    pagemap_n_pages = pg_round_up(n_pages * sizeof(struct page)) / pg_size;

    if (pmem_nalloc_internal(&paddr, pagemap_n_pages, False) != ERR_OK) {
        panic("Failed to allocate memory for pagemap");
    }

    // pagemap is mapped in the KMAP region
    pagemap = (struct page*)kmap_p2v(paddr);
    memset(pagemap, 0, pagemap_n_pages * pg_size);
    pagemap_end = pagemap + n_pages;
}

static void
freeblocks_init(void)
{
    paddr_t paddr;
    int bitmap_n_pages, index, probe;
    struct page *page;
    List *list;

    // Free pages used by bitmap
    bitmap_n_pages = pg_round_up(((pmemconfig.pmem_end - pmemconfig.pmem_start) / pg_size) / 8) / pg_size;
    paddr = kmap_v2p((vaddr_t)bitmap);
    pmem_nfree_internal(paddr, bitmap_n_pages, False);

    // Initialize free lists
    for (list = freeblocks; list != &freeblocks[MAX_ORDER+1]; list++) {
        list_init(list);
    }

    // Mark unavailable physical pages as allocated
    for (paddr = 0; paddr < pmemconfig.pmem_start; paddr += pg_size) {
        if ((page = paddr_to_page(paddr)) == NULL) {
            panic("Failed to find page");
        }
        page->refcnt = 1;
        page->order = 0;
    }

    // Go through bitmap to add free pages to free lists
    index = 0;
    while (index < bitmap_contig_start) {
        if (BITMAP_ISSET(index)) {
            // Page is already allocated.
            // TODO: we don't know how many pages were allocated by the
            // pmem_nalloc call, just assume 1 here. (Likely those pages will
            // never be freed, so should be fine)
            if ((page = paddr_to_page(BITMAP_ITOP(index))) == NULL) {
                panic("Failed to find page");
            }
            page->refcnt = 1;
            page->order = 0;
            index++;
        } else {
            // Try to add the maximum power of two free pages to free lists.
            for (probe = index + 1; probe < bitmap_contig_start && !(BITMAP_ISSET(probe)); probe++) {
                ;
            }
            freeblocks_insert_range(BITMAP_ITOP(index), BITMAP_ITOP(probe));
            index = probe;
        }
    }
    // Add the remaining physical pages to free lists
    freeblocks_insert_range(BITMAP_ITOP(bitmap_contig_start), pmemconfig.pmem_end);
}

#pragma GCC diagnostic ignored "-Wunused-function"
static void
freeblocks_print(void)
{
    int i, j;
    Node *node;
    kprintf("Freeblocks:\n");
    for (i = 0; i <= MAX_ORDER; i++) {
        kprintf("    Order %d:", i);
        for (j = 0, node = list_begin(&freeblocks[i]); j < MAX_PRINT_BLOCKS && node != list_end(&freeblocks[i]); j++, node = list_next(node)) {
            kprintf(" %x", page_to_paddr(list_entry(node, struct page, node)));
        }
        kprintf("\n");
    }
}

static int
get_max_page_order(paddr_t start, paddr_t end)
{
    int order, start_index, end_index;

    start_index = start / pg_size;
    end_index = end / pg_size;
    kassert(end_index > start_index);

    for (order = MAX_ORDER; order > 0; order--) {
        if (start_index % (1 << order) == 0 && start_index + (1 << order) <= end_index) {
            return order;
        }
    }
    return 0;
}

static int
get_min_page_order(size_t n)
{
    int order;

    for (order = 0; order <= MAX_ORDER; order++) {
        if (1 << order >= n) {
            return order;
        }
    }
    panic("size larger than MAX_ORDER");
}

static struct page*
find_freeblock(int order, bool split)
{
    struct page *page, *buddy;

    if (order < 0 || order > MAX_ORDER) {
        return NULL;
    }

    if (!list_empty(&freeblocks[order])) {
        page = list_entry(list_begin(&freeblocks[order]), struct page, node);
        freeblocks_remove(page);
    } else {
        // Recursively splitting higher order blocks
        if ((page = find_freeblock(order + 1, True)) == NULL) {
            return NULL;
        }
    }

    if (split) {
        page->order = order - 1;
        if ((buddy = find_buddy(page)) == NULL) {
            panic("Failed find buddy block");
        }
        buddy->order = order - 1;
        freeblocks_insert(buddy);
    }

    return page;
}

static struct page*
find_buddy(struct page *page)
{
    struct page *buddy;
    int index;

    kassert(page);
    kassert(page->order >= 0 && page->order <= MAX_ORDER);

    // Find the index of block (within order n blocks)
    index = page - pagemap;
    kassert(index % (1 << page->order) == 0);
    index /= (1 << page->order);

    buddy = (index & 1) ? page - (1 << page->order) : page + (1 << page->order);
    if (buddy >= pagemap_end) {
        return NULL;
    }
    return buddy;
}

static struct page*
merge_block(struct page *page)
{
    struct page *buddy;

    kassert(page);
    kassert(page->refcnt == 0);
    kassert(page->order >= 0 && page->order <= MAX_ORDER);

    if (page->order == MAX_ORDER) {
        return page;
    }

    buddy = find_buddy(page);
    // We can only merge the two blocks if:
    // 1. The buddy block is also free
    // 2. The two blocks have the same order. If the buddy block has a different
    // order (should be smaller), the buddy block has been splitted and there
    // exist smaller allocated block within the buddy block.
    //
    // Once we merge the two blocks, recursively merge the next level blocks
    if (buddy && buddy->refcnt == 0 && buddy->order == page->order) {
        freeblocks_remove(buddy);
        if (page < buddy) {
            page->order += 1;
            return merge_block(page);
        } else {
            buddy->order += 1;
            return merge_block(buddy);
        }
    } else {
        // Otherwise, just return the current block
        return page;
    }
}

static void
freeblocks_insert(struct page *page)
{
    kassert(page);
    kassert(page->order >= 0 && page->order <= MAX_ORDER);

    page->refcnt = 0;
    list_append(&freeblocks[page->order], &page->node);
}

static void
freeblocks_insert_range(paddr_t start, paddr_t end)
{
    int order;
    struct page *page;

    while (start < end) {
        order = get_max_page_order(start, end);
        page = paddr_to_page(start);
        kassert(page);

        page->order = order;
        freeblocks_insert(page);
        start += (1 << order) * pg_size;
    }
}

static void
freeblocks_remove(struct page *page)
{
    kassert(page);
    kassert(page->refcnt == 0);
    kassert(page->order >= 0 && page->order <= MAX_ORDER);

    list_remove(&page->node);
}

static err_t
pmem_nalloc_internal(paddr_t *paddr, size_t n, bool lock)
{
    int order, index;
    struct page *page;

    kassert(n > 0);
    if (lock) {
        spinlock_acquire(&pmem_lock);
    }
    if (!pagemap_initialized) {
        // Boot memory allocator
        if ((index = bitmap_find(n)) == -1) {
            goto fail;
        }
        bitmap_alloc(index, n);
        *paddr = BITMAP_ITOP(index);
    } else {
        // Buddy allocator
        order = get_min_page_order(n);
        if ((page = find_freeblock(order, False)) == NULL) {
            goto fail;
        }
        sleeplock_init(&page->lock);
        page->kmem_cache = NULL;
        page->slab = NULL;
        page->rmap = NULL;
        pmem_set_page_dirty(page, False);
        kassert(page->refcnt == 0);
        page->refcnt = 1;
        list_init(&page->blk_headers);
        *paddr = page_to_paddr(page);
        kassert(*paddr != NULL);
    }
    if (lock) {
        spinlock_release(&pmem_lock);
    }
    return ERR_OK;

fail:
    if (lock) {
        spinlock_release(&pmem_lock);
    }
    return ERR_NOMEM;
}

static void
pmem_nfree_internal(paddr_t paddr, size_t n, bool lock)
{
    struct page *page;

    kassert(n > 0);
    if (lock) {
        spinlock_acquire(&pmem_lock);
    }
    if (!pagemap_initialized) {
        // Boot memory allocator
        bitmap_free(BITMAP_PTOI(paddr), n);
    } else {
        // Buddy allocator
        page = paddr_to_page(paddr);
        if (1 << page->order != n) {
            // Buddy allocator requires freeing the same number of pages as
            // allocated.
            // TODO: how do we handle this case? for now, just free all
            // allocated pages
        }
        page->refcnt = 0;
        page = merge_block(page);
        kassert(page != NULL);
        freeblocks_insert(page);
    }
    if (lock) {
        spinlock_release(&pmem_lock);
    }
}

struct page*
paddr_to_page(paddr_t paddr)
{
    return &pagemap[paddr/pg_size];
}

paddr_t
page_to_paddr(const struct page *page)
{
    return (page - pagemap) * pg_size;
}

void
pmem_boot_init(void)
{
    pmem_arch_init();
    bitmap_init();
    spinlock_init(&pmem_lock);
    pagemap_initialized = False;
}

void
pmem_init(void)
{
    spinlock_acquire(&pmem_lock);
    pagemap_init();
    freeblocks_init();
    pagemap_initialized = True;
    spinlock_release(&pmem_lock);
}

err_t
pmem_alloc(paddr_t *paddr)
{
    return pmem_nalloc(paddr, 1);
}

err_t
pmem_nalloc(paddr_t *paddr, size_t n)
{
    return pmem_nalloc_internal(paddr, n, True);
}

void
pmem_free(paddr_t paddr)
{
    pmem_nfree(paddr, 1);
}

void
pmem_nfree(paddr_t paddr, size_t n)
{
    pmem_nfree_internal(paddr, n, True);
}

int
pmem_is_page_dirty(struct page *page)
{
    return get_state_bit(page->state, PAGE_DIRTY_BIT);
}

void
pmem_set_page_dirty(struct page *page, int dirty)
{
    page->state = set_state_bit(page->state, PAGE_DIRTY_BIT, dirty);
}

int
pmem_get_refcnt(paddr_t paddr)
{
    struct page *page;

    page = paddr_to_page(paddr);
    kassert(page);
    return page->refcnt;
}

void
pmem_inc_refcnt(paddr_t paddr, int n)
{
    struct page *page;

    page = paddr_to_page(paddr);
    kassert(page);

    spinlock_acquire(&pmem_lock);
    kassert(page->refcnt > 0);
    kassert(page->order == 0);

    page->refcnt += n;
    spinlock_release(&pmem_lock);
}

void
pmem_dec_refcnt(paddr_t paddr)
{
    struct page *page;

    page = paddr_to_page(paddr);
    kassert(page);

    spinlock_acquire(&pmem_lock);
    kassert(page->refcnt > 0);
    kassert(page->order == 0);

    page->refcnt--;
    if (page->refcnt == 0) {
        pmem_nfree_internal(paddr, 1, False);
    }
    spinlock_release(&pmem_lock);
}
