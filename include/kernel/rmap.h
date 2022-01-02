#ifndef _RMAP_H_
#define _RMAP_H_

#include <kernel/list.h>

/*
 * Reverse mapping for tracking shared memory regions.
 */

struct rmap {
    List regions;
};

/*
 * Allocate a new reverse mapping.
 */
struct rmap *rmap_alloc(void);

/*
 * Free a reverse mapping.
 */
void rmap_free(struct rmap *rmap);

/*
 * Constructor for an existing reverse mapping
 */
void rmap_construct(struct rmap *rmap);

/*
 * Destructor for a reverse mapping
 */
void rmap_destroy(struct rmap *rmap);

/*
 * Unmap all memory mappings of a physical page
 */
err_t rmap_unmap(struct rmap *rmap, paddr_t paddr);

#endif /* _RMAP_H_ */
