#include <kernel/bdev.h>
#include <kernel/kmalloc.h>
#include <kernel/vm.h>
#include <kernel/pgcache.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/bdevms.h>
#include <kernel/console.h>
#include <kernel/thread.h>
#include <lib/errcode.h>
#include <lib/bits.h>
#include <kernel/ide.h>

static struct kmem_cache *bdev_allocator = NULL;
static struct kmem_cache *bio_allocator = NULL;

// Root block device
#define ROOT_DEV_NUM 0
#define ROOT_IDE_INDEX 1

// Return the number of blocks in a page
#define N_BLKS_PER_PAGE (pg_size / BDEV_BLK_SIZE)

// Given a block, return the first block in the page which the requested block
// belongs to.
#define FIRST_BLK_IN_PAGE(blk) ((blk / (pg_size / BDEV_BLK_SIZE)) * (pg_size / BDEV_BLK_SIZE))

// Block header allocator
static struct kmem_cache *blk_header_allocator = NULL;

// Block header state bits
#define BLK_HEADER_VALID 0
#define BLK_HEADER_DIRTY 1

/*
 * Initialize block headers for a page (if not initialized before). first_blk is
 * the block number of the first block in the page.
 *
 * Precondition:
 * Caller must hold page->lock.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate block headers.
 */
static err_t init_blk_headers(struct page *page, struct bdev *bdev, blk_t first_blk);

/*
 * Check if all blocks in a page have zero reference.
 *
 * Precondition:
 * Caller must hold page->lock.
 */
static int blocks_zero_ref(struct page *page);

/*
 * Free all block headers in a page if the page is clean. If the page is dirty,
 * a kernel thread will write the page to bdev and free the headers.
 *
 * Precondition:
 * Caller must hold page->lock.
 */
static void free_blk_headers(struct page *page);

static err_t
init_blk_headers(struct page *page, struct bdev *bdev, blk_t first_blk)
{
    struct blk_header *bh;
    blk_t index;

    if (list_empty(&page->blk_headers)) {
        for (index = 0; index < N_BLKS_PER_PAGE; index++) {
            if ((bh = kmem_cache_alloc(blk_header_allocator)) == NULL) {
                return ERR_NOMEM;
            }
            sleeplock_init(&bh->lock);
            list_append(&page->blk_headers, &bh->node);
            bh->bdev = bdev;
            bh->blk = first_blk + index;
            bh->page = page;
            bh->data = (void*)(kmap_p2v(page_to_paddr(page)) + BDEV_BLK_SIZE * index);
            bdev_set_blk_valid(bh, True);
            bdev_set_blk_dirty(bh, False);
            bh->ref = 0;
        }
    }
    return ERR_OK;
}

static int
blocks_zero_ref(struct page *page)
{
    Node *n;
    struct blk_header *bh;

    for (n = list_begin(&page->blk_headers);
         n != list_end(&page->blk_headers);
         n = list_next(n)) {
        bh = list_entry(n, struct blk_header, node);
        if (bh->ref > 0) {
            return False;
        }
    }
    return True;
}

static void
free_blk_headers(struct page *page)
{
    Node *curr, *next;
    struct blk_header *bh;

    // If page is dirty, do not free headers -- a kernel thread will write the
    // dirty page back to bdev, and free the headers.
    if (pmem_is_page_dirty(page)) {
        return;
    }
    for (curr = list_begin(&page->blk_headers); !list_empty(&page->blk_headers); curr = next) {
        bh = list_entry(curr, struct blk_header, node);
        // Page must be clean
        kassert(!bdev_is_blk_dirty(bh));
        next = list_remove(curr); // we are going to free the curr node
        kmem_cache_free(blk_header_allocator, bh);
    }
}

void
bdev_init(void)
{
    // Create object allocators
    if ((bdev_allocator = kmem_cache_create(sizeof(struct bdev))) == NULL) {
        panic("Failed to create bdev_allocator");
    }
    if ((bio_allocator = kmem_cache_create(sizeof(struct bio))) == NULL) {
        panic("Failed to create bio_allocator");
    }
    if ((blk_header_allocator = kmem_cache_create(sizeof(struct blk_header))) == NULL) {
        panic("Failed to create blk_header_allocator");
    }
    // Initialize root block device: currently using IDE
    if ((root_bdev = ide_alloc(ROOT_DEV_NUM, ROOT_IDE_INDEX)) == NULL) {
        panic("Failed to allocate root block device");
    }
    if (ide_init(root_bdev) != ERR_OK) {
        panic("Failed to initialized root block device");
    }
}

struct bdev*
bdev_alloc(dev_t dev)
{
    struct bdev *bdev;

    if ((bdev = kmem_cache_alloc(bdev_allocator)) != NULL) {
        bdev->dev = dev;
        list_init(&bdev->request_queue);
        spinlock_init(&bdev->queue_lock);
        bdev->request_handler = NULL;
        bdev->data = NULL;
        if ((bdev->store = bdevms_alloc(bdev)) == NULL) {
            kmem_cache_free(bdev_allocator, bdev);
            bdev = NULL;
        }
    }
    return bdev;
}

void
bdev_free(struct bdev *bdev)
{
    // XXX handle remaining requests in the queue?
    bdevms_free(bdev->store);
    kmem_cache_free(bdev_allocator, bdev);
}

struct bio*
bio_alloc(void)
{
    struct bio *bio;

    if ((bio = kmem_cache_alloc(bio_allocator)) != NULL) {
        bio->bdev = NULL;
        bio->blk = 0;
        bio->size = 0;
        bio->buffer = NULL;
        bio->status = BIO_PENDING;
        spinlock_init(&bio->lock);
        condvar_init(&bio->cv);
    }
    return bio;
}

void
bio_free(struct bio *bio)
{
    kmem_cache_free(bio_allocator, bio);
}

void
bdev_make_request(struct bio *bio)
{
    struct bdev_request request;

    // Add request to block device's request queue
    bio->status = BIO_PENDING;
    request.bio = bio;
    spinlock_acquire(&bio->bdev->queue_lock);
    list_append(&bio->bdev->request_queue, &request.node);
    spinlock_release(&bio->bdev->queue_lock);
    // Call the device driver to handle the request
    bio->bdev->request_handler(bio->bdev);
    // Wait for block operation to complete
    spinlock_acquire(&bio->lock);
    while (bio->status != BIO_COMPLETE) {
        condvar_wait(&bio->cv, &bio->lock);
    }
    spinlock_release(&bio->lock);
}

int
bdev_is_blk_valid(struct blk_header *bh) {
    return get_state_bit(bh->state, BLK_HEADER_VALID);
}

void
bdev_set_blk_valid(struct blk_header *bh, int valid) {
    bh->state = set_state_bit(bh->state, BLK_HEADER_VALID, valid);
}

int
bdev_is_blk_dirty(struct blk_header *bh) {
    return get_state_bit(bh->state, BLK_HEADER_DIRTY);
}

void
bdev_set_blk_dirty(struct blk_header *bh, int dirty) {
    bh->state = set_state_bit(bh->state, BLK_HEADER_DIRTY, dirty);
    if (dirty) {
        sleeplock_acquire(&bh->page->lock);
        pmem_set_page_dirty(bh->page, True);
        sleeplock_release(&bh->page->lock);
    }
    // Code that writes dirty pages back to bdev is responsible for clearing the
    // page's dirty bit
}

struct blk_header*
bdev_get_blk(struct bdev *bdev, blk_t blk)
{
    struct blk_header *bh;

    if ((bh = bdev_get_blk_unlocked(bdev, blk)) != NULL) {
        sleeplock_acquire(&bh->lock);
    }
    return bh;
}

struct blk_header*
bdev_get_blk_unlocked(struct bdev *bdev, blk_t blk)
{
    struct page *page;
    Node *n;
    struct blk_header *bh;

    sleeplock_acquire(&bdev->store->pgcache_lock);
    if ((page = pgcache_get_page(bdev->store, blk * BDEV_BLK_SIZE)) == NULL) {
        sleeplock_release(&bdev->store->pgcache_lock);
        return NULL;
    }
    sleeplock_release(&bdev->store->pgcache_lock);

    sleeplock_acquire(&page->lock);
    if (init_blk_headers(page, bdev, FIRST_BLK_IN_PAGE(blk)) != ERR_OK) {
        sleeplock_release(&page->lock);
        // XXX dec reference count on the page
        return NULL;
    }

    // Iterate through block headers in the page to find the requested block
    for (n = list_begin(&page->blk_headers);
         n != list_end(&page->blk_headers);
         n = list_next(n)) {
        bh = list_entry(n, struct blk_header, node);
        kassert(bh);
        if (bh->blk == blk) {
            bh->ref++;
            sleeplock_release(&page->lock);
            return bh;
        }
    }
    panic("bdev block not found in page");
}

void
bdev_release_blk(struct blk_header *bh)
{
    sleeplock_release(&bh->lock);
    bdev_release_blk_unlocked(bh);
}

void
bdev_release_blk_unlocked(struct blk_header *bh)
{
    struct page *page = bh->page;
    sleeplock_acquire(&page->lock);
    kassert(bh->ref > 0);
    bh->ref--;
    // if bh still has references, no need to test blocks_zero_ref
    if (bh->ref == 0 && blocks_zero_ref(page)) {
        free_blk_headers(page);
    }
    sleeplock_release(&page->lock);
    // XXX dec reference count on the page
}

err_t
bdev_write_blk(struct blk_header *bh)
{
    struct bio *bio;

    kassert(bdev_is_blk_valid(bh));

    if ((bio = bio_alloc()) == NULL) {
        return ERR_NOMEM;
    }

    bio->bdev = bh->bdev;
    bio->blk = bh->blk;
    bio->size = 1;
    bio->buffer = bh->data;
    bio->op = BIO_WRITE;
    bdev_make_request(bio);
    bio_free(bio);
    // Now the block buffer is clean
    bdev_set_blk_dirty(bh, False);

    return ERR_OK;
}
