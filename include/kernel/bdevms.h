#ifndef _BDEVMS_H_
#define _BDEVMS_H_

/*
 * Memstore for block devices.
 */

#include <kernel/memstore.h>

struct bdevms_info {
    struct bdev *bdev;
};

/*
 * Allocate a bdev memstore.  Return NULL if failed to allocate.
 */
struct memstore *bdevms_alloc(struct bdev *bdev);

/*
 * Free a bdev memstore.
 */
void bdevms_free(struct memstore *store);

#endif /* _BDEVMS_H_ */
