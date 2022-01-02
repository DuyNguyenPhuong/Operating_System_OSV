#ifndef _FILEMS_H_
#define _FILEMS_H_

#include <kernel/memstore.h>
#include <kernel/fs.h>

/*
 * Memstore backed by regular files.
 */

/*
 * File memstore descriptor.
 */
struct filems_info {
    struct inode *inode;    // File inode
};

/*
 * Allocate a file memstore. Return NULL if failed to allocate.
 */
struct memstore *filems_alloc(struct inode *inode);

/*
 * Free a file memstore.
 */
void filems_free(struct memstore *store);

#endif /* _FILEMS_H_ */
