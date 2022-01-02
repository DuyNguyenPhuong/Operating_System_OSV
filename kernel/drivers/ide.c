#include <kernel/kmalloc.h>
#include <kernel/io.h>
#include <kernel/console.h>
#include <kernel/trap.h>
#include <lib/errcode.h>
#include <kernel/ide.h>
// T_IRQ_IDE is defined in arch-specific trap header
#include <arch/trap.h>

#define IDE_SECTOR_SIZE     512 // sector size
// IDE registers
#define IDE_REG_DATA        0x01F0 // data register
#define IDE_REG_COUNT       0x01F2 // sector count register
#define IDE_REG_SECTOR      0x01F3 // sector number register
#define IDE_REG_CYL_L       0x01F4 // cylinder low register
#define IDE_REG_CYL_H       0x01F5 // cylinder high register
#define IDE_REG_DRIVE       0x01F6 // drive selection register
#define IDE_REG_STATUS_CMD  0x01F7 // status and command register
#define IDE_REG_CTRL        0x03F6 // control register
// IDE status masks
#define IDE_STATUS_BSY      0x80
#define IDE_STATUS_DRDY     0x40
#define IDE_STATUS_DF       0x20
#define IDE_STATUS_ERR      0x01
// IDE commands
#define IDE_CMD_READ        0x20
#define IDE_CMD_WRITE       0x30
#define IDE_CMD_RDMUL       0xC4
#define IDE_CMD_WRMUL       0xC5

static struct kmem_cache *ide_allocator = NULL;

/*
 * Retrieve the first operation in the device's request queue. Set pop to True
 * to also remove the first request in the queue. Return NULL if request queue
 * is empty.
 */
static struct bio *bdev_front_bio(struct bdev *bdev, int pop);

/*
 * IDE request handling function
 */
static void ide_request_handler(struct bdev *bdev);

/*
 * IDE trap handler function
 */
static void ide_trap_handler(irq_t irq, void *dev, void *regs);

/*
 * Wait for IDE disk to become ready (busy waiting). Return ERR_IDE_DISK_ERR is
 * the disk reports an error.
 */
#define ERR_IDE_DISK_ERR 1
static err_t ide_wait(struct bdev *bdev);

/*
 * Issue a command to the IDE controller. Must hold the ide descriptor lock when
 * calling this function.
 */
static void ide_issue_cmd(struct bdev *bdev, struct bio *bio);

static struct bio*
bdev_front_bio(struct bdev *bdev, int pop)
{
    struct bio *bio;
    struct bdev_request *request;
    Node *node;
    bio = NULL;

    kassert(bdev);
    spinlock_acquire(&bdev->queue_lock);
    if (!list_empty(&bdev->request_queue)) {
        node = list_begin(&bdev->request_queue);
        request = list_entry(node, struct bdev_request, node);
        bio = request->bio;
        if (pop) {
            list_remove(node);
        }
    }
    spinlock_release(&bdev->queue_lock);
    return bio;
}

static void
ide_request_handler(struct bdev *bdev)
{
    struct ide_dev *ide;
    struct bio *bio;

    kassert(bdev);
    kassert(bdev->data);
    ide = (struct ide_dev*)bdev->data;

    spinlock_acquire(&ide->lock);
    // Only issue command if there is no ongoing commands
    if (ide->status == IDE_IDLE) {
        // Issue the first request in the request queue. Do not remove the
        // request here -- interrupt handler requires access to the bio, and the
        // interrupt handler will remove the request
        if ((bio = bdev_front_bio(bdev, False)) != NULL) {
            kassert(bio->status == BIO_PENDING);
            ide_issue_cmd(bdev, bio);
        }
    }
    spinlock_release(&ide->lock);
}

static void
ide_trap_handler(irq_t irq, void *dev, void *regs)
{
    struct bdev *bdev;
    struct ide_dev *ide;
    struct bio *bio;
    kassert(dev);

    bdev = (struct bdev*)dev;
    ide = (struct ide_dev*)bdev->data;
    bio = NULL;

    spinlock_acquire(&ide->lock);
    // Nothing to do if no command was previously issued
    if (ide->status == IDE_BUSY) {
        // First request in the queue is the active request. Can safely remove
        // the request in the queue now.
        bio = bdev_front_bio(bdev, True);
        kassert(bio);
        kassert(bio->status == BIO_PENDING);
        if (bio->op == BIO_READ) {
            readn(IDE_REG_DATA, bio->buffer, bio->size * BDEV_BLK_SIZE);
        }
        // Complete the request, and wake up the thread waiting for
        // completion
        spinlock_acquire(&bio->lock);
        bio->status = BIO_COMPLETE;
        condvar_signal(&bio->cv);
        spinlock_release(&bio->lock);
        // Issue the next command in the queue (if present)
        if ((bio = bdev_front_bio(bdev, False)) != NULL) {
            kassert(bio->status == BIO_PENDING);
            ide_issue_cmd(bdev, bio);
        } else {
            // We have completed all the requests
            ide->status = IDE_IDLE;
        }
    }
    spinlock_release(&ide->lock);
    trap_notify_irq_completion();
}

static err_t
ide_wait(struct bdev *bdev)
{
    int status;

    while (((status = readb(IDE_REG_STATUS_CMD)) & (IDE_STATUS_BSY | IDE_STATUS_DRDY)) != IDE_STATUS_DRDY) {
        ;
    }
    if ((status & (IDE_STATUS_DF | IDE_STATUS_ERR)) != 0) {
        return ERR_IDE_DISK_ERR;
    }
    return ERR_OK;
}

static void
ide_issue_cmd(struct bdev *bdev, struct bio *bio)
{
    struct ide_dev *ide;
    int sector, num_sectors, cmd = 0;

    kassert(bdev);
    kassert(bdev->data);
    kassert(bio);
    ide = (struct ide_dev*)bdev->data;
    // Determine the command
    sector = bio->blk * (BDEV_BLK_SIZE / IDE_SECTOR_SIZE);
    num_sectors = bio->size * (BDEV_BLK_SIZE / IDE_SECTOR_SIZE);
    // Currently can write a maximum of 8 sectors at once
    kassert(num_sectors > 0 && num_sectors <= 8);
    if (bio->op == BIO_READ) {
        cmd = num_sectors > 1 ? IDE_CMD_RDMUL : IDE_CMD_READ;
    } else if (bio->op == BIO_WRITE) {
        cmd = num_sectors > 1 ? IDE_CMD_WRMUL : IDE_CMD_WRITE;
    }
    // Issue the command
    ide_wait(bdev);
    writeb(IDE_REG_CTRL, 0);
    writeb(IDE_REG_COUNT, num_sectors);
    writeb(IDE_REG_SECTOR, sector & 0xFF);
    writeb(IDE_REG_CYL_L, (sector >> 8) & 0xFF);
    writeb(IDE_REG_CYL_H, (sector >> 16) & 0xFF);
    writeb(IDE_REG_DRIVE, 0xE0 | (ide->ide_index << 4) | ((sector >> 24) & 0x0F));
    writeb(IDE_REG_STATUS_CMD, cmd);
    if (bio->op == BIO_WRITE) {
        writen(IDE_REG_DATA, bio->buffer, bio->size * BDEV_BLK_SIZE);
    }
    // Change status to busy
    ide->status = IDE_BUSY;
}

struct bdev*
ide_alloc(dev_t dev, uint8_t ide_index)
{
    struct bdev *bdev;
    struct ide_dev *ide;

    if (ide_allocator == NULL) {
        if ((ide_allocator = kmem_cache_create(sizeof(struct ide_dev))) == NULL) {
            return NULL;
        }
    }
    if ((bdev = bdev_alloc(dev)) == NULL) {
        return NULL;
    }
    if ((ide = kmem_cache_alloc(ide_allocator)) == NULL) {
        bdev_free(bdev);
        return NULL;
    }
    spinlock_init(&ide->lock);
    ide->status = IDE_IDLE;
    ide->ide_index = ide_index;
    bdev->data = (void*)ide;
    bdev->request_handler = ide_request_handler;
    return bdev;
}

void
ide_free(struct bdev *bdev)
{
    struct ide_dev *ide;

    kassert(bdev);
    kassert(bdev->data);
    ide = (struct ide_dev*)bdev->data;
    // XXX wait for device to become idle?
    kmem_cache_free(ide_allocator, ide);
    bdev_free(bdev);
}

err_t
ide_init(struct bdev *bdev)
{
    kassert(bdev);
    // register trap handler
    if (trap_register_handler(T_IRQ_IDE, bdev, ide_trap_handler) != ERR_OK) {
        return ERR_IDE_INIT_FAIL;
    }
    // Enable IRQ
    if (trap_enable_irq(T_IRQ_IDE) != ERR_OK) {
        return ERR_IDE_INIT_FAIL;
    }
    // Wait for the disk to become ready
    ide_wait(bdev);
    return ERR_OK;
}
