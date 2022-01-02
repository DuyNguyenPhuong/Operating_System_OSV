#ifndef _VPMAP_H_
#define _VPMAP_H_

/*
 * Machine dependent virtual to physical memory mapping. The mapping is enclosed
 * in a ``vpmap`` structure, which is defined in the machine-dependent
 * ``vpmap.h`` header file.
 */

#include <kernel/vm.h>
#include <arch/vpmap.h> // defines struct vpmap

/*
 * Error codes
 */
#define ERR_VPMAP_NOTPRESENT 1 // entry not present
#define ERR_VPMAP_MAP 2 // failed to map entries

// Kernel vpmap
extern struct vpmap *kvpmap;
/*
 * vpmap initialization.
 */
void vpmap_init(void);

/*
 * Create a new vpmap.
 */
struct vpmap *vpmap_create(void);

/*
 * Load ``vpmap`` into the current processor.
 */
err_t vpmap_load(struct vpmap *vpmap);

/*
 * Map n virtual pages starting at ``vaddr`` to physical pages starting at
 * ``paddr``. Physical pages are contiguous if ``n`` greater than one.
 * Return ERR_VPMAP_MAP if failed map any pages in range.
 */
err_t vpmap_map(struct vpmap *vpmap, vaddr_t vaddr, paddr_t paddr, size_t n, memperm_t memperm);

/*
 * Remove mappings starting at virtual address vaddr for n pages.
 * If free_swap is set, any mapping that resides in swap will be removed from swap.
 */
void vpmap_unmap(struct vpmap *vpmap, vaddr_t vaddr, size_t n, int free_swap);

/*
 * Remove all mappings in a vpmap.
 */
void vpmap_destroy(struct vpmap *vpmap);

/*
 * Copy mapping of n pages from src vpmap to dst vpmap.
 * memperm indicates the memory permission for src and dst after the copy.
 * Return ERR_VPMAP_MAP if failed to map pages in dstvpmap
 */
err_t vpmap_copy(struct vpmap *srcvpmap, struct vpmap *dstvpmap, vaddr_t srcaddr, vaddr_t dstaddr, size_t n, memperm_t memperm);

/*
 * Copy mapping of first level entries from kernel vpmap to dst vpmap. Permission is perserved.
 * Return ERR_VPMAP_MAP if failed to map pages in dstvpmap
 */
err_t vpmap_copy_kernel_mapping(struct vpmap *dstvpmap);

/*
 * Putting x into pte entry of vaddr entry.
 * Return ERR_VPMAP_NOTPRESENT if entry not present.
 */
err_t vpmap_put_swapid(struct vpmap *vpmap, vaddr_t vaddr, swapid_t swapid);

/*
 * Look up vaddr. If it is present then populate paddr and return ERR_OK.
 * Otherwise return ERR_VPMAP_NOTPRESENT, and populate swapid if the page is in
 * swap.
 */
err_t vpmap_lookup_vaddr(struct vpmap *vpmap, vaddr_t vaddr, paddr_t *paddr, swapid_t *swapid);

/*
 * Convert virtual address to physical address in the kmap region.
 */
paddr_t kmap_v2p(vaddr_t vaddr);

/*
 * Convert physical address to virtual address in the kmap region.
 */
vaddr_t kmap_p2v(paddr_t paddr);

/*
 * Convert I/O physical address to virtual address in the kmap region.
 */
vaddr_t kmap_io2v(paddr_t io_paddr);

/*
 * Change permission of a region of memory.
 */
void vpmap_set_perm(struct vpmap *vpmap, vaddr_t vaddr, size_t n, memperm_t memperm);

/*
 * Mark the page as dirty.
 */
void vpmap_set_dirty(struct vpmap *vpmap, vaddr_t vaddr);

/*
 * Check if the page is dirty.
 * Return ERR_VPMAP_NOTPRESET if no physical page is mapped to the address.
 */
err_t vpmap_get_dirty(struct vpmap *vpmap, vaddr_t vaddr, int *dirty);

/*
 * Check if the page is accessed.
 * Return ERR_VPMAP_NOTPRESET if no physical page is mapped to the address.
 */
err_t vpmap_get_accessed(struct vpmap *vpmap, vaddr_t vaddr, int *accessed);

/*
 *  Flush tlb
 */
void vpmap_flush_tlb();

#endif /* _VPMAP_H_ */
