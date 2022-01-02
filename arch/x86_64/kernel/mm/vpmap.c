#include <kernel/vpmap.h>
#include <kernel/pmem.h>
#include <kernel/proc.h>
#include <kernel/kmalloc.h>
#include <kernel/console.h>
#include <kernel/util.h>
#include <kernel/trap.h>
#include <lib/errcode.h>
#include <lib/string.h>
#include <lib/stddef.h>
#include <arch/mmu.h>
#include <arch/asm.h>

/*
 * vpmap allocator
 */
static struct kmem_cache *vpmap_allocator = NULL;

/*
 * Find the page table entry for virtual address ``vaddr``. If ``alloc`` is set,
 * allocate a page table if not present.
 */
static pte_t *find_pte(pml4e_t *pml4, vaddr_t vaddr, int alloc);

/*
 * Clear entry of a pte. Decrement page reference count if page present. Free
 * swap entry if in swap.
 */
static void clear_pte(pml4e_t *pml4, int free_swap);

/*
 * Map a range of virtual addresses from ``vaddr`` to ``vaddr + size``, to
 * physical address starting at ``paddr``. Set all page permission to ``perm``.
 * Return ERR_VPMAP_MAP if failed to map any page in range.
 */
static err_t map_pages(pml4e_t *pml4, vaddr_t vaddr, paddr_t paddr, size_t size, pteperm_t perm);

/*
 * Unmap a range of virtual addresses from ``vaddr`` to ``end``. end exclusive
 * When ``free_swap`` is set, entry in swap will be freed.
 * When ``free_imm`` is set, all intermediate page tables are freed as well
 */
static void unmap_pages(pml4e_t *pml4, vaddr_t vaddr, vaddr_t end, int free_swap, int free_imm);

/* Utility functions for unmapping other page dir*/
static void unmap_pdpt(pdpte_t *pdpt, vaddr_t start_addr, vaddr_t end_addr,
    int pdptx_start, int pdptx_end, int free_swap, int free_imm);

static void unmap_pd(pde_t *pde, vaddr_t start_addr, vaddr_t end_addr,
    int pdx_start, int pdx_end, int free_swap, int free_imm);

/*
 * memperm to pteperm translation.
 */
static pteperm_t memperm_to_pteperm(memperm_t memperm);

static pte_t*
find_pte(pml4e_t *pml4, vaddr_t vaddr, int alloc)
{
    kassert(pml4);
    pml4e_t *pml4e;
    pdpte_t *pdpt, *pdpte;
    pde_t *pgdir, *pde;
    pte_t *pgtab;
    paddr_t paddr;

    // top level walk
    pml4e = &pml4[PML4X(vaddr)];
    if ((*pml4e & PTE_P) == 0) {
        if (!alloc || pmem_alloc(&paddr) != ERR_OK) {
            return NULL;
        }
        memset((void*) KMAP_P2V(paddr), 0, pg_size);
        *pml4e = paddr | PTE_P | PTE_W | PTE_U;
    }

    // second level walk
    pdpt = (pdpte_t* )KMAP_P2V(PML4E_ADDR(*pml4e));
    pdpte = &pdpt[PDPTX(vaddr)];
    if ((*pdpte & PTE_P) == 0) {
        if (!alloc || pmem_alloc(&paddr) != ERR_OK) {
            return NULL;
        }
        memset((void*) KMAP_P2V(paddr), 0, pg_size);
        *pdpte = paddr | PTE_P | PTE_W | PTE_U;
    }

    // third level walk
    pgdir = (pde_t*) KMAP_P2V(PDPTE_ADDR(*pdpte));
    pde = &pgdir[PDX(vaddr)];
    if ((*pde & PTE_P) == 0) {
        if (!alloc || pmem_alloc(&paddr) != ERR_OK) {
            return NULL;
        }
        memset((void*) KMAP_P2V(paddr), 0, pg_size);
        *pde = paddr | PTE_P | PTE_W | PTE_U;
    }

    pgtab = (pte_t*) KMAP_P2V(PDE_ADDR(*pde));
    return &pgtab[PTX(vaddr)];
}

static void
clear_pte(pte_t *pte, int free_swap) {
    kassert(pte);
    if (*pte & PTE_P) {
        pmem_dec_refcnt(PPN(*pte));
    }
    //*pte = PTE_FLAGS(*pte) & 0xffe;
    *pte = 0;
}

static err_t
map_pages(pml4e_t *pml4, vaddr_t vaddr, paddr_t paddr, size_t size, pteperm_t perm)
{
    pte_t *pte;
    vaddr_t v, vend;

    kassert(pml4 != 0);

    v = pg_round_down(vaddr);
    vend = pg_round_down(vaddr + size);
    paddr = pg_round_down(paddr);

    // We can't test using '<=' in the for loop, because some mappings end at
    // virtual address 0
    for (; v != vend; v += pg_size, paddr += pg_size) {
        if ((pte = find_pte(pml4, v, 1)) == NULL) {
            return ERR_VPMAP_MAP;
        }
        *pte = PPN(paddr) | PTE_P | perm;
    }
    return ERR_OK;
}


static void
unmap_pages(pml4e_t *pml4, vaddr_t start, vaddr_t end, int free_swap, int free_imm)
{
    kassert(PML4X(start) <= PML4X(end-1));

    int pml4x, pdptx, limit;
    // Optimization: instead of walking the page table for each page in range
    // (using find_pte), iterate through the page directory and each page table.
    for (pml4x = PML4X(start), pdptx = PDPTX(start); pml4x <= PML4X(end-1); pml4x++, pdptx = 0) {
        if (pml4[pml4x] & PTE_P) {
            limit = pdptx < PDPTX(end) ? N_PDPTE_PER_PG : PDPTX(end) + 1;
            unmap_pdpt((pdpte_t*) KMAP_P2V(PML4E_ADDR(pml4[pml4x])), start, end, pdptx, limit, free_swap, free_imm);
            if (free_imm) {
                pmem_free(PML4E_ADDR(pml4[pml4x]));
            }
        }
    }
}

static void
unmap_pdpt(pdpte_t *pdpt, vaddr_t start_addr, vaddr_t end_addr, int pdptx_start, int pdptx_end, int free_swap, int free_imm)
{
    int pdptx, pdx, limit;

    for (pdptx = pdptx_start, pdx = PDX(start_addr); pdptx < pdptx_end; pdptx++, pdx = 0) {
        if (pdpt[pdptx] & PTE_P) {
            limit = pdx < PDX(end_addr) ? N_PDE_PER_PG : PDX(end_addr) + 1;
            unmap_pd((pde_t*) KMAP_P2V(PDPTE_ADDR(pdpt[pdptx])), start_addr, end_addr, pdx, limit, free_swap, free_imm);
            if (free_imm) {
                pmem_free(PDPTE_ADDR(pdpt[pdptx]));
            }
        }
    }
}

static void
unmap_pd(pde_t *pde, vaddr_t start_addr, vaddr_t end_addr, int pdx_start, int pdx_end, int free_swap, int free_imm)
{
    int pdx, ptx, limit;
    pte_t *pgtable;

    for (pdx = pdx_start, ptx = PTX(start_addr); pdx < pdx_end; pdx++, ptx = 0) {
        if (pde[pdx] & PTE_P) {
            limit = ptx < PTX(end_addr) ? N_PTE_PER_PG : PTX(end_addr) + 1;
            pgtable = (pte_t*) KMAP_P2V(PDE_ADDR(pde[pdx]));
            for (; ptx < limit; ptx++) {
                clear_pte(&pgtable[ptx], free_swap);
            }
            if (free_imm) {
                pmem_free(PDE_ADDR(pde[pdx]));
            }
        }
    }
}

static pteperm_t
memperm_to_pteperm(memperm_t memperm) {
    pteperm_t pteperm;
    switch (memperm) {
        case MEMPERM_R:
            pteperm = 0;
            break;
        case MEMPERM_RW:
            pteperm = PTE_W;
            break;
        case MEMPERM_UR:
            pteperm = PTE_U;
            break;
        case MEMPERM_URW:
            pteperm = PTE_U | PTE_W;
            break;
        default:
            panic("vpmap: Unknown memperm type");
    };
    return pteperm;
}

void
vpmap_init(void)
{
    kassert(kvpmap);
    struct mapping *m;
    paddr_t paddr;

    /*
    * Kernel vpmap includes the following regions (pinned):
    * 1. KMAP: maps all physical at virtual address KMAP_BASE. Because the linker
    * also loads kernel text within KMAP, we need to map those pages read only.
    * 2. Device memory.
    * Each region is page table aligned.
    */
    struct mapping {
        vaddr_t vaddr;
        paddr_t paddr_start;
        paddr_t paddr_end;
        pteperm_t perm; // No need to specify PTE_P0xffffffff7f82e000
    } kernel_mappings[] = {
        { KMAP_BASE, 0, EXTMEM_BASE, PTE_W }, // KMAP (before kernel text)
        { KMAP_BASE+EXTMEM_BASE, EXTMEM_BASE, KMAP_V2P(_data), 0 }, // KMAP(kernel text)
        { (vaddr_t)_data, KMAP_V2P(_data), pmemconfig.pmem_end, PTE_W }, // KMAP (rest of physical memory)
        { DEVMEM_BASE, 0xFE000000, 0x100000000, PTE_W} // Device memory
    };

    // Allocate one physical page for the kvpmap page directory.
    if (pmem_alloc(&paddr) != ERR_OK) {
        panic("vpmap: cannot allocate physical memory for kvpmap pgdir");
    }
    kvpmap->pml4 = (pde_t*)KMAP_P2V(paddr);
    memset(kvpmap->pml4, 0, pg_size);

    // Create kernel mappings
    for (m = kernel_mappings; m < &kernel_mappings[N_ELEM(kernel_mappings)]; m++) {
        if (map_pages(kvpmap->pml4, m->vaddr, m->paddr_start, m->paddr_end - m->paddr_start, m->perm) != ERR_OK) {
            panic("vpmap: failed to create kernel mappings");
        }
    }

    // Activate kernel vpmap
    if (vpmap_load(kvpmap) != ERR_OK) {
        panic("vpmap: failed to load kernel vpmap");
    }
}

struct vpmap*
vpmap_create(void)
{
    struct vpmap *vpmap;
    paddr_t paddr;

    // Create allocator for vpmap
    if (vpmap_allocator == NULL) {
        if ((vpmap_allocator = kmem_cache_create(sizeof(struct vpmap))) == NULL) {
            panic("vpmap: failed to create allocator");
        }
    }

    if ((vpmap = kmem_cache_alloc(vpmap_allocator)) == NULL) {
        return NULL;
    }
    if (pmem_alloc(&paddr) != ERR_OK) {
        return NULL;
    }
    vpmap->pml4 = (pde_t*)KMAP_P2V(paddr);
    memset(vpmap->pml4, 0, pg_size);

    // TODO: initialize with no regions?
    return vpmap;
}

err_t
vpmap_load(struct vpmap *vpmap)
{
    kassert(vpmap);
    kassert(vpmap->pml4);
    lcr3(KMAP_V2P(vpmap->pml4));
    return ERR_OK;
}

err_t
vpmap_map(struct vpmap *vpmap, vaddr_t vaddr, paddr_t paddr, size_t n, memperm_t memperm)
{
    kassert(vpmap);
    if (n == 0) {
        return ERR_OK;
    }
    return map_pages(vpmap->pml4, pg_round_down(vaddr), pg_round_down(paddr), n * pg_size, memperm_to_pteperm(memperm));
}

void
vpmap_unmap(struct vpmap *vpmap, vaddr_t vaddr, size_t n, int free_swap)
{
    if (vpmap == NULL || vpmap->pml4 == NULL || n == 0) {
        return;
    }
    vaddr_t start = pg_round_down(vaddr);
    vaddr_t end = start + n * pg_size;
    kassert(PML4X(start) <= PML4X(end));
    unmap_pages(vpmap->pml4, start, end, free_swap, 0);
}

void
vpmap_destroy(struct vpmap *vpmap)
{
    if (vpmap == NULL || vpmap->pml4 == NULL) {
        return;
    }
    kassert(vpmap != kvpmap);
    // Deallocate all allocated userspace memory and their corresponding
    // page tables. shouldn't use unmap because we need to free intermediate page tables.
    unmap_pages(vpmap->pml4, 0, USTACK_UPPERBOUND, 1, 1);
    pmem_free(KMAP_V2P(vpmap->pml4));
    kmem_cache_free(vpmap_allocator, vpmap);
}

err_t
vpmap_copy(struct vpmap *srcvpmap, struct vpmap *dstvpmap, vaddr_t srcaddr, vaddr_t dstaddr, size_t n, memperm_t memperm) {
    kassert(srcvpmap && dstvpmap);
    pte_t *src_pte, *dst_pte;
    pteperm_t perm = memperm_to_pteperm(memperm);
    size_t i;

    srcaddr = pg_round_down(srcaddr);
    dstaddr = pg_round_down(dstaddr);
    for (i = 0; i < n; i++, srcaddr += pg_size, dstaddr += pg_size) {
        if ((src_pte = find_pte(srcvpmap->pml4, srcaddr, 0)) == NULL ||
            PPN(*src_pte) == 0) {
            continue;
        }
        if ((dst_pte = find_pte(dstvpmap->pml4, dstaddr, 1)) == NULL ||
            PPN(*dst_pte) != 0) {
            // Return an error if address already mapped
            return ERR_VPMAP_MAP;
        }
        err_t err;
        paddr_t paddr;
        if ((err = pmem_alloc(&paddr)) != ERR_OK) {
            return err;
        }
        memcpy((void*)KMAP_P2V(paddr), (void*)KMAP_P2V(PTE_ADDR(*src_pte)), pg_size);
        *dst_pte = PPN(paddr) | PTE_P | perm;
    }
    return ERR_OK;
}

err_t
vpmap_copy_kernel_mapping(struct vpmap *dstvpmap) {
    kassert(dstvpmap);
    int pml4x;
    for (pml4x = PML4X(KMAP_BASE); pml4x < N_PML4E_PER_PG; pml4x++) {
        if (kvpmap->pml4[pml4x] & PTE_P) {
            dstvpmap->pml4[pml4x] = kvpmap->pml4[pml4x];
            // TODO: may need to increment refcnt if we ever allow kernel pt to be swapped
        }
    }
    return ERR_OK;
}


err_t
vpmap_lookup_vaddr(struct vpmap *vpmap, vaddr_t vaddr, paddr_t *paddr, swapid_t *swapid) {
    kassert(vpmap);
    // Initialize paddr and swapid first
    if (paddr) {
        *paddr = PADDR_NONE;
    }
    if (swapid) {
        *swapid = SWAPID_NONE;
    }
    // Lookup page table
    pte_t *pte = find_pte(vpmap->pml4, vaddr, 0);
    if (pte) {
        if (*pte & PTE_P) {
            if (paddr) {
                *paddr = PPN(*pte) + (vaddr - VPN(vaddr));
            }
            return ERR_OK;
        }
    }
    return ERR_VPMAP_NOTPRESENT;
}

paddr_t
kmap_v2p(vaddr_t vaddr)
{
    kassert(vaddr >= KMAP_BASE && vaddr < KMAP_BASE + pmemconfig.pmem_end);
    return KMAP_V2P(vaddr);
}

vaddr_t
kmap_p2v(paddr_t paddr)
{
    return KMAP_P2V(paddr);
}

vaddr_t
kmap_io2v(paddr_t paddr)
{
    return KMAP_IO2V(paddr);
}

void
vpmap_set_perm(struct vpmap *vpmap, vaddr_t vaddr, size_t n, memperm_t memperm) {
    kassert(vpmap);
    pteperm_t perm = memperm_to_pteperm(memperm);
    size_t i;
    vaddr = pg_round_down(vaddr);
    // TODO: fix pte flags. only using the last 3 bits right now.
    for (i = 0; i < n; i++) {
        pte_t* pte = find_pte(vpmap->pml4, vaddr+i*pg_size, 0);
        if (pte) {
            *pte = PPN(*pte) | (PTE_FLAGS(*pte)&PTE_P) | perm;
        }
    }
}

void
vpmap_set_dirty(struct vpmap *vpmap, vaddr_t vaddr) {
    pte_t *pte = find_pte(vpmap->pml4, vaddr, 0);
    if (pte) {
        *pte = *pte | PTE_D;
    }
}

err_t
vpmap_get_dirty(struct vpmap *vpmap, vaddr_t vaddr, int *dirty) {
    kassert(dirty);
    pte_t *pte = find_pte(vpmap->pml4, vaddr, 0);
    if (pte) {
        *dirty = *pte & PTE_D;
        return ERR_OK;
    }
    return ERR_VPMAP_NOTPRESENT;
}

err_t
vpmap_get_accessed(struct vpmap *vpmap, vaddr_t vaddr, int *accessed) {
    kassert(accessed);
    pte_t *pte = find_pte(vpmap->pml4, vaddr, 0);
    if (pte) {
        *accessed = *pte & PTE_A;
        return ERR_OK;
    }
    return ERR_VPMAP_NOTPRESENT;
}

void
vpmap_flush_tlb() {
    intr_set_level(INTR_OFF);
    vaddr_t pml4 = (vaddr_t) kvpmap->pml4;
    struct proc *p = proc_current();
    if (p) {
        pml4 = (vaddr_t) p->as.vpmap->pml4;
    }
    lcr3(KMAP_V2P(pml4));
    intr_set_level(INTR_ON);
}
