#ifndef _KMALLOC_H_
#define _KMALLOC_H_

/*
 * Allocator for kernel memory. The allocator supports two ways of allocating
 * kernel memory:
 *
 * 1. Allocation of fixed-size objects. An object allocator is created by
 * calling ``kmem_cache_create`` with the object size. The object allocator
 * provides two functions, ``kmem_cache_alloc`` and ``kmem_cache_free``, to
 * allocate and free objects.
 *
 * 2. A generic kmalloc function. The caller specifies the size of allocation.
 * The function however may allocate more memory than requested.
 */

#include <kernel/types.h>
#include <kernel/list.h>
#include <kernel/synch.h>

/*
 * Slab metadata.
 */
struct slab {
    // Linked list of slabs
    Node node;
    // Address of the first object
    void *objs;
    // Index of the next free slot
    int free;
    // Size of the slab (number of pages)
    size_t n_pages;
};

/*
 * Object allocator.
 */
struct kmem_cache {
    List full; // Linked-list of slabs that are fully allocated
    List free; // Linked-list of slabs that have free slots
    struct spinlock lock;
    size_t obj_size;
};

/*
 * Initialize kernel memory allocator.
 */
void kmalloc_init(void);

/*
 * Create an allocator that allocates/frees objects of size ``size``.
 */
struct kmem_cache *kmem_cache_create(size_t size);

/*
 * Destroy an object allocator.
 */
void kmem_cache_destroy(struct kmem_cache *kmem_cache);

/*
 * Allocates an object from the allocator.
 */
void *kmem_cache_alloc(struct kmem_cache *kmem_cache);

/*
 * Free an object and return it to the allocator.
 */
void kmem_cache_free(struct kmem_cache *kmem_cache, void *obj);

/*
 * Allocate ``size`` bytes of memory.
 */
void *kmalloc(size_t size);

/*
 * Free block of memory.
 */
void kfree(void *ptr);

#endif /* _KMALLOC_H_ */
