#ifndef _ARCH_X86_64_MMU_H_
#define _ARCH_X86_64_MMU_H_

#include <kernel/types.h>

/*
 * Segments
 */
// Various segment selectors
#define SEG_KCODE       1  // kernel code
#define SEG_KDATA       2  // kernel data+stack
#define SEG_UCODE       3  // user code
#define SEG_UDATA       4  // user data+stack
#define SEG_TSS         5  // this process's task state
#define SEG_TSS_UPPER   6  // upper half of TSS address  
#define NSEGS           7

// Application segment types
#define STA_X     0x8       // Executable segment
#define STA_E     0x4       // Expand down (non-executable segments)
#define STA_C     0x4       // Conforming code segment (executable only)
#define STA_W     0x2       // Writeable (non-executable segments)
#define STA_R     0x2       // Readable (executable segments)
#define STA_A     0x1       // Accessed

// System segment types
#define STS_LDT		0x2	/* local descriptor table */
#define STS_T64A	0x9	/* available 64-bit TSS */
#define STS_T64B	0xB	/* busy 64-bit TSS */
#define STS_CG64	0xC	/* 64-bit call gate */
#define STS_IG64	0xE	/* 64-bit interrupt gate */
#define STS_TG64	0xF	/* 64-bit trap gate */

#define BIT64(n)	(1UL << (n))
#define SEG_A		BIT64(40)
#define SEG_W		BIT64(41)
#define SEG_CODE	BIT64(43)
#define SEG_S		BIT64(44)
#define SEG_DPL(dpl) ((dpl) << 45)
#define SEG_P		BIT64(47)
#define SEG_L		BIT64(53)
#define SEG_G		BIT64(55)

// DPLs
#define DPL_KERNEL  0x0
#define DPL_USER    0x3

// assembly for bootstrapping GDT
#define SEG_NULL \
        .word 0, 0; \
        .byte 0, 0, 0, 0

#define SEG32_ASM(type,base,lim) \
        .word (((lim) >> 12) & 0xffff), ((base) & 0xffff); \
        .byte (((base) >> 16) & 0xff), (0x90 | (type)), \
                (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#define KERNEL_DS_DESC (SEG_P | SEG_S | SEG_DPL(DPL_KERNEL) | SEG_A | SEG_W | SEG_G)
#define KERNEL_CS_DESC (SEG_CODE | SEG_L | KERNEL_DS_DESC )

#ifndef __ASSEMBLER__

// Task state segment format
struct taskstate {
	uint32_t reserved0;
    // RSP for privilege level 0-2
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
    // interrupt stack table pointers
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t reserved2;
	uint16_t reserved3;
    // I/O map base address
	uint16_t iomb;
} __attribute__ ((packed));

// Segment Descriptor
struct segdesc {
    unsigned int limit_15_0 : 16;
    unsigned int base_15_0  : 16;
    unsigned int base_23_16 : 8;
    unsigned int type       : 4;
    unsigned int s          : 1;
    unsigned int dpl        : 2;
    unsigned int p          : 1;
    unsigned int limit_19_16: 4;
    unsigned int avl        : 1;
    unsigned int l          : 1;
    unsigned int db         : 1;
    unsigned int g          : 1;
    unsigned int base_31_24 : 8;
};

#define SEG(type, base, lim, resv1, dpl) (struct segdesc)    \
{ ((lim) >> 12) & 0xffff, (uint64_t)(base) & 0xffff,      \
	((uint64_t)(base) >> 16) & 0xff, type, 1, dpl, 1,       \
	(uint64_t)(lim) >> 28, 0, resv1, 0, 0, (uint64_t)(base) >> 24 }

#define SEGTSS(type, base, lim, dpl) (struct segdesc)  \
{ (lim) & 0xffff, (uint64_t)(base) & 0xffff,              \
  ((uint64_t)(base) >> 16) & 0xff, type, 0, dpl, 1,       \
  ((uint64_t)(lim) >> 16) & 0xf, 0, 0, 0, 0, ((uint64_t)(base) >> 24) & 0xff}

// Gate descriptor
struct gatedesc {
    uint64_t offset_15_0    : 16;
    uint64_t seg_selector   : 16;
    uint64_t rsvd           : 8;
    uint64_t type           : 4;
    uint64_t s              : 1;    // must be 0 (system)
    uint64_t dpl            : 2;
    uint64_t p              : 1;
    uint64_t offset_31_16   : 16;
    uint64_t offset_63_32   : 32;   // really high bits of offset
    uint64_t rsv2           : 32;   // reserved
};

#define GATE(offset, seg, trap, dpl) (struct gatedesc) \
{ (uint64_t)(offset) & 0xFFFF, seg, 0, trap ? STS_TG64 : STS_IG64, 0, dpl, 1, \
    ((uint64_t)(offset) >> 16) & 0xffff, ((uint64_t)(offset) >> 32 & 0xffffffff), 0}

#endif /* __ASSEMBLER__ */

/*
 * Page and page table
 */
#define PG_SIZE 4096
#define PHYS_ADDR_MASK  (~0xFFF8000000000000)
#define N_PTE_PER_PG    (PG_SIZE / sizeof(pte_t))
#define N_PDE_PER_PG    (PG_SIZE / sizeof(pde_t))
#define N_PDPTE_PER_PG  (PG_SIZE / sizeof(pdpte_t))
#define N_PML4E_PER_PG  (PG_SIZE / sizeof(pml4e_t))

#define PG_SHIFT    12
#define PTX_SHIFT   12
#define PDX_SHIFT   21
#define PDPTX_SHIFT 30
#define PML4X_SHIFT 39

#define VPN(v) ((vaddr_t)(v) & ~0xFFF)
#define PPN(p) ((paddr_t)(p) & ~0xFFF & PHYS_ADDR_MASK)
#define ENTRY_IDX(v, shift) (((v) >> shift) & 0x1FF)
#define PTX(v) ENTRY_IDX(v, PTX_SHIFT)
#define PDX(v) ENTRY_IDX(v, PDX_SHIFT)
#define PDPTX(v) ENTRY_IDX(v, PDPTX_SHIFT)
#define PML4X(v) ENTRY_IDX(v, PML4X_SHIFT)

// x86-64 has the same 
#define ENTRY_ADDR(e) (e & ~0xFFF & PHYS_ADDR_MASK)
#define ENTRY_FLAGS(e) (e & 0xFFF)
#define PTE_ADDR(pte) ENTRY_ADDR(pte)
#define PTE_FLAGS(pte) ENTRY_FLAGS(pte)
#define PDE_ADDR(pde) ENTRY_ADDR(pde)
#define PDE_FLAGS(pde) ENTRY_FLAGS(pde)
#define PDPTE_ADDR(pdpte) ENTRY_ADDR(pdpte)
#define PDPTE_FLAGS(pdpte) ENTRY_FLAGS(pdpte)
#define PML4E_ADDR(pml4e) ENTRY_ADDR(pml4e)
#define PML4E_FLAGS(pml4e) ENTRY_FLAGS(pml4e)

#define PTE_P           0x001   // Present
#define PTE_W           0x002   // Writeable
#define PTE_U           0x004   // User
#define PTE_PWT         0x008   // Write-Through
#define PTE_PCD         0x010   // Cache-Disable
#define PTE_A           0x020   // Accessed
#define PTE_D           0x040   // Dirty
#define PTE_PS          0x080   // Page Size (4MB)
#define PTE_MBZ         0x180   // Bits must be zero

#ifndef __ASSEMBLER__

/*
 * Initial page directory (with 4MB pages) for kernel initialization. Kernel
 * will eventually replace this page directory with actual kernel page tables.
 */
extern pml4e_t kpml4_tmp[];


/*
 * Linker stores the end address of the kernel image in ``_end``. Physical
 * memory pages for kernel image are pinned, and are not available for kernel
 * allocation.
 */
extern char _end[];

/*
 * Linker stores the start address of kernel's data section in ``_data``. Memory
 * below ``_data`` is kernel text, and is read-only.
 */
extern char _data[];

#endif /* __ASSEMBLER__ */

/*
 * Kernel maps all physical memory to virtual memory starting at KMAP_BASE.
 * Conveniently, the linker also loads the kernel image within KMAP, so we don't
 * need to create page tables for the kernel image twice.
 * virtual address v -> physical address (v - KMAP_BASE)
 * physical address p -> virtual address (p + KMAP_BASE)
 */
#define KMAP_BASE 0xFFFFFFFF80000000
#define KMAP_V2P(v) ((vaddr_t)(v) - KMAP_BASE)
#define KMAP_P2V(p) ((paddr_t)(p) + KMAP_BASE)
#define KMAP_IO2V(a) (a + 0xFFFFFFFF00000000)
#define KMAP_V2P_ASM(v) ((v) - KMAP_BASE)
#define KMAP_P2V_ASM(p) ((p) + KMAP_BASE)

#define USTACK_PAGES 10
#define USTACK_UPPERBOUND 0xFFFFFF7FFFFFF000
#define USTACK_LOWERBOUND USTACK_UPPERBOUND - (USTACK_PAGES*PG_SIZE)

/*
 * Initial kernel stack lives in the kernel image data section, with size
 * INIT_KSTACK_SIZE. Kernel may later allocates memory and maps the stack to a
 * different address.
 */
#define INIT_KSTACK_SIZE PG_SIZE
#define STACK_TOP(n)	 ((_end + (n) * INIT_KSTACK_SIZE + INIT_KSTACK_SIZE))

/*
 * Kernel image is loaded at physical address ``EXTMEM_BASE``. Physical
 * addresses below ``EXTMEM_BASE`` are base and I/O memory, and are not
 * available for kernel memory allocation.
 */
#define EXTMEM_BASE 0x00100000

/*
 * Base device memory virtual address. All memory addresses above
 * ``DEVMEM_BASE`` are used for memory mapped I/O, and not available for kernel
 * memory allocation.
 */
#define DEVMEM_BASE 0xFFFFFFFFFE000000

#endif /* _ARCH_X86_64_MMU_H_ */
