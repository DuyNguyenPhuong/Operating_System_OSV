#ifndef _IDE_H_
#define _IDE_H_

/*
 * IDE disk driver
 */
#include <kernel/synch.h>
#include <kernel/bdev.h>

/*
 * Error codes
 */
#define ERR_IDE_INIT_FAIL 1

/*
 * Status of an IDE device. Possible states are:
 * IDE_IDLE: device is not processing any requests.
 * IDE_BUSY: device is currently processing a request.
 */
typedef enum {
    IDE_IDLE,
    IDE_BUSY
} ide_status_t;

/*
 * IDE device descriptor
 */
struct ide_dev {
    struct spinlock lock; // lock to protect this descriptor
    ide_status_t status;
    uint8_t ide_index; // 0 for master, 1 for slave
};

/*
 * Allocate a block device descriptor for an IDE device, with device number dev
 * and (master/slave) index ide_index. Return NULL if failed to allocate.
 */
struct bdev *ide_alloc(dev_t dev, uint8_t ide_index);

/*
 * Free an IDE block device descriptor.
 */
void ide_free(struct bdev *bdev);

/*
 * Initialize an IDE device. Return ERR_IDE_INIT_FAIL if failed to initialize.
 */
err_t ide_init(struct bdev *bdev);

#endif /* _IDE_H_ */
