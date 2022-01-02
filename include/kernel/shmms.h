#ifndef _SHMMS_H_
#define _SHMMS_H_

#include <kernel/memstore.h>

/*
 * Memstore backed by shared memory.
 */

/*
 * Allocate a shared memory memstore. TODO: parameters
 * Return NULL if failed to allocate.
 */
struct memstore *shmms_alloc(void);

/*
 * Free a shared memory memstore.
 */
void shmms_free(struct memstore *store);

#endif /* _SHMMS_H_ */
