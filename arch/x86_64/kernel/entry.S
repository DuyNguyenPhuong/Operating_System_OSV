#include <arch/cpu.h>
#include <arch/mmu.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>

#define MULTIBOOT_HEADER_FLAGS	(MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE)

.section .head.text
.balign	4
multiboot_header:
	.long   MULTIBOOT_HEADER_MAGIC
	.long	MULTIBOOT_HEADER_FLAGS
	.long	- (MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
	.long	KMAP_V2P_ASM(multiboot_header)
	.long	KMAP_V2P_ASM(_start)
	.long	KMAP_V2P_ASM(_edata)
.long	KMAP_V2P_ASM(_end)
	.long	KMAP_V2P_ASM(start_multiboot)

.balign	8
multiboot2_header:
	.long	MULTIBOOT2_HEADER_MAGIC
	.long	MULTIBOOT_ARCHITECTURE_I386
	.long	(multiboot2_header_end - multiboot2_header)
	.long	- (MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot2_header_end - multiboot2_header))
.balign	8
address_tag_start:
	.short	MULTIBOOT_HEADER_TAG_ADDRESS
	.short	0
	.long	address_tag_end - address_tag_start
	.long	KMAP_V2P_ASM(multiboot2_header)
	.long	KMAP_V2P_ASM(_start)
	.long	KMAP_V2P_ASM(_edata)
	.long	KMAP_V2P_ASM(_end)
address_tag_end:
.balign	8
entry_address_tag_start:
	.short	MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
	.short	0
	.long	entry_address_tag_end - entry_address_tag_start
	.long	KMAP_V2P_ASM(start_multiboot2)
entry_address_tag_end:
.balign	8
	.short	MULTIBOOT_HEADER_TAG_END
	.short	0
	.long	8
multiboot2_header_end:

/*
 * eax: magic value
 * ebx: physical address of the multiboot structure
 */
.code32
start_multiboot:
	/* check for multiboot bootloader magic */
	cmpl $MULTIBOOT_BOOTLOADER_MAGIC, %eax
	jne	error_no_mb
	/* save multiboot pointer */
	movl %ebx, KMAP_V2P_ASM(multiboot_info)
	jmp	start_bsp

start_multiboot2:
	cmpl $MULTIBOOT2_BOOTLOADER_MAGIC, %eax
	jne	error_no_mb
	movl %ebx, KMAP_V2P_ASM(multiboot2_addr)
	jmp	start_bsp

start_bsp:
	/* check for the highest extended function */
	movl $0x80000000, %eax
	cpuid
	cmpl $0x80000001, %eax
	jb error_no_lm

	/* check for lm (long mode) */
	movl $0x80000001, %eax
	cpuid
	testl $(1U << 29), %edx
	jz	error_no_lm

	# /* check for pdpe1gb (1G pages) */
	# testl	$(1 << 26), %edx
	# jz error_no_pdpe1gb

	/* set BSP's cpunum to zero */
	movl $MSR_IA32_TSC_AUX, %ecx
	movl $0, %eax
	movl $0, %edx
	wrmsr

	# /* set BSP's stack */
	movl $(0x6000), %esp
	#$(KMAP_V2P_ASM(STACK_TOP(0))), %esp
	andl %esp, 0xfffffff0

.global	start_common
start_common:
 	/* CR4: enable PAE, PSE */
	movl %cr4, %eax
	orl $(CR4_PAE|CR4_PSE), %eax
	movl %eax, %cr4

	/* CR3: load initial page table */
	movl $(KMAP_V2P_ASM(kpml4_tmp)), %eax
    movl %eax, %cr3

	/* MSR EFER: enable LME (and syscall) */
	movl $MSR_EFER, %ecx
	rdmsr
	#orl	$(EFER_SCE|EFER_LME), %eax
	orl	$(EFER_LME), %eax
	wrmsr

	/* CR0: enable paging and write protect */
	movl %cr0, %eax
	orl	$(CR0_PG|CR0_WP), %eax
	movl %eax, %cr0

	lgdt KMAP_V2P_ASM(gdtdesc)
	movl $(SEG_KDATA << 3), %eax
	movw %ax, %ss
	movw %ax, %ds
	movw %ax, %es

	/* enter 64-bit mode */
	ljmp $(SEG_KCODE << 3), $KMAP_V2P_ASM(_start)

error_no_mb:
	lea	KMAP_V2P_ASM(msg_no_mb), %esi
	jmp	error

error_no_lm:
	lea	KMAP_V2P_ASM(msg_no_lm), %esi
	jmp	error

error_no_pdpe1gb:
	lea	KMAP_V2P_ASM(msg_no_pdpe1gb), %esi
	jmp	error

error:
	/* write to COM1 */
	movl $0x3f8, %edx
error_loop:
	lodsb
	test %al, %al
	jz error_end
	outb %al, %dx
	jmp	error_loop
error_end:
	movl $0x0a, %eax
	outb %al, %dx
spin:
	hlt

.code64
.global _start
_start:
	movq $entry64high, %rax
  	jmp *%rax
entry64high:
 	movq $0xFFFFFFFF80010000, %rax
  	movq %rax, %rsp
  	movq multiboot_info, %rax
  	movq %rax, %rdi
	call main
	hlt
	jmp	spin

.section .rodata
msg_no_mb:
	.string	"no multiboot bootloader"
msg_no_lm:
	.string	"no long mode"
msg_no_pdpe1gb:
	.string	"no 1GB pages"

/* boot GDT (kernel segments only) */
.p2align 4
gdt:
	.quad 0
	.quad KERNEL_CS_DESC
	.quad KERNEL_DS_DESC
gdtdesc:
	.word (gdtdesc - gdt - 1)
	.quad KMAP_V2P_ASM(gdt)

/*
 * Entry pml4 for booting. Maps the following region
 * 1. vaddr 0:2MB -> paddr 0:2MB
 * 2. vaddr KMAP_BASE:KMAP_BASE+2MB -> paddr 0:2MB
 * PML4X(0) = PDPTX(0) = PDX(0) = 0
 * PML4X(KMAP_BASE) = 511, PDPTX(KMAP_BASE) = 510, PDX(KMAP_BASE) = 0
 */
.section .data
.balign	PG_SIZE
.global	kpml4_tmp
kpml4_tmp:
	.quad	KMAP_V2P_ASM(kpml3low) + PTE_P + PTE_W
	.rept	512 - 2
	.quad	0
	.endr
	.quad	KMAP_V2P_ASM(kpml3high) + PTE_P + PTE_W

.balign	PG_SIZE
kpml3low:
	.quad	KMAP_V2P_ASM(kpml2) + PTE_P + PTE_W
	.rept	512 - 1
	.quad	0
	.endr

.balign	PG_SIZE
kpml3high:
	.rept	512 - 2
	.quad	0
	.endr
	.quad	KMAP_V2P_ASM(kpml2) + PTE_P + PTE_W
	.quad	0

.balign	PG_SIZE
kpml2:
    index = 0
    .rept   512
    .quad   (index * 0x200000UL) + PTE_P + PTE_W + PTE_PS
    index = index + 1
    .endr

.section .bss
.balign	8
.global	multiboot_info
multiboot_info:
	.space	8
.global	multiboot2_addr
multiboot2_addr:
	.space	8


