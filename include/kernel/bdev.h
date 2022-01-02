#ifndef _BDEV_H_
#define _BDEV_H_

/*
 * Generic block device interface.
 */
#include <kernel/types.h>
#include <kernel/synch.h>
#include <kernel/list.h>

struct bdev;
struct bdev_request;
struct bio;
struct super_block;

// Block size used by the block device interface
#define BDEV_BLK_SIZE 512

/*
 * Initialize the block device subsystem.
 */
void bdev_init(void);

/*
 * Generic block device descriptor.
 */
struct bdev {
    dev_t dev; // device number
    struct list request_queue; // request queue for this device
    struct spinlock queue_lock; // spinlock to protect the request queue
    void (*request_handler)(struct bdev*); // request handler function (defined by drivers)
    void *data; // device specific data
    struct memstore *store; // memstore to read memory pages from this device
    struct super_block *sb; // bdev's super block if available
};

// Root block device (for root file system)
struct bdev *root_bdev;

/*
 * Block device request.
 */
struct bdev_request {
    struct bio *bio; // operation
    struct list_node node; // list node for request queue
};

typedef enum {
    BIO_READ,
    BIO_WRITE
} bio_op_t;

typedef enum {
    BIO_PENDING,
    BIO_COMPLETE
} bio_status_t;

/*
 * Block device operation
 */
struct bio {
    struct bdev *bdev; // pointer to block device
    blk_t blk; // starting block number
    size_t size; // number of blocks in the operation
    void *buffer; // buffer for data transfer
    bio_op_t op;
    bio_status_t status;
    struct spinlock lock; // lock to synchronize access to status
    struct condvar cv; // cv to check status
};

/*
 * Allocate a generic block device descriptor. Initialize device number to dev.
 * To allocate a descriptor for a specific device, call the device's alloc
 * function.
 *
 * Return:
 * NULL - Failed to allocate memory for the descriptor.
 */
struct bdev *bdev_alloc(dev_t dev);

/*
 * Free a block device descriptor.
 */
void bdev_free(struct bdev *bdev);

/*
 * Allocate a bio descriptor.
 *
 * Return:
 * NULL - Failed to allocate memory for the descriptor.
 */
struct bio *bio_alloc(void);

/*
 * Free a bio descriptor.
 */
void bio_free(struct bio *bio);

/*
 * Submit a block device request. This function is synchronous: it returns only
 * when the request is completed by the block device.
 */
void bdev_make_request(struct bio *bio);

/*
 * Header for bdev blocks stored in page cache.
 */
struct blk_header {
    // Lock to protect data structures in the header
    struct sleeplock lock;
    // List node for page->blk_headers
    Node node;
    // Block device that the block belongs to
    struct bdev *bdev;
    // Block number
    blk_t blk;
    // Cached page where the block is stored
    struct page *page;
    // Pointer to the block buffer. When block header is VALID, this
    // address should be within the cached page.
    void *data;
    // Status of the block. Contains the following flags:
    // - VALID
    // - DIRTY
    state_t state;
    // Reference counter. This counter is protected by page->lock.
    unsigned int ref;
};

/*
 * Functions that get/set state of a block.
 *
 * Precondition:
 * Caller must hold bh->lock.
 */
int bdev_is_blk_valid(struct blk_header *bh);
void bdev_set_blk_valid(struct blk_header *bh, int valid);
int bdev_is_blk_dirty(struct blk_header *bh);
void bdev_set_blk_dirty(struct blk_header *bh, int dirty);

/*
 * Search for a single bdev block. If the block is not in memory, fill the
 * block with data read from bdev. Increase the reference count on the block
 * buffer by one. Acquire blk_header's lock when returns.
 *
 * Return:
 * NULL - Failed to allocate memory.
 */
struct blk_header *bdev_get_blk(struct bdev *bdev, blk_t blk);

/*
 * Same as bdev_get_blk, but without acquiring blk_header's lock.
 */
struct blk_header *bdev_get_blk_unlocked(struct bdev *bdev, blk_t blk);

/*
 * Release a block buffer. Decrement the reference count on the block buffer.
 * bh is locked when calling this function -- the function will unlock bh before
 * releasing it.
 */
void bdev_release_blk(struct blk_header *bh);

/*
 * Same as bdev_release_blk, but without releasing blk_header's lock.
 */
void bdev_release_blk_unlocked(struct blk_header *bh);

/*
 * Write a block buffer to the backing block device.
 *
 * Precondition:
 * Caller must hold bh->lock.
 *
 * Return:
 * ERR_NOMEM - Failed to allocate memory.
 */
err_t bdev_write_blk(struct blk_header *bh);

#endif /* _BDEV_H_ */
