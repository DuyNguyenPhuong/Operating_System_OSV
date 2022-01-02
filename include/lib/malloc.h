#ifndef _MALLOC_H_
#define _MALLOC_H_

#include <arch/types.h>

/*
 * Allocate ``size`` bytes of memory.
 */
void *malloc(size_t size);

/*
 * Free block of memory.
 */
void free(void *ptr);

#endif /* _MALLOC_H_ */
