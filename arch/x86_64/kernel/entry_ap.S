#include <arch/cpu.h>
#include <arch/mmu.h>

# Each non-boot CPU ("AP") is started up in response to a STARTUP
# IPI from the boot CPU.  Section B.4.2 of the Multi-Processor
# Specification says that the AP will start in real mode with CS:IP
# set to XY00:0000, where XY is an 8-bit value sent with the
# STARTUP. Thus this code must start at a 4096-byte boundary.
#
# Because this code sets DS to zero, it must sit
# at an address in the low 2^16 bytes.
#
# mp_start_ap (in mp.c) sends the IPI_STARTUPs one at a time.
# It copies this code (start) at 0x7000.  It puts the address of
# a newly allocated per-core stack in start-16,the address of the
# place to jump to (main_ap) in start-24, and the physical address
# of kmpl4_tmp in start-24.
#
# This code combines elements of bootasm.S and entry.S.

.code16           
.globl start
start:
    # Switch to the stack allocated by mp_start_ap()
    cli

    # Zero data segment registers DS, ES, and SS.
    xorw %ax,%ax
    movw %ax,%ds
    movw %ax,%es
    movw %ax,%ss

    # Switch from real to protected mode.  Use a bootstrap GDT that makes
    # virtual addresses map directly to physical addresses so that the
    # effective memory map doesn't change during the transition.
    lgdt gdtdesc
    movl %cr0, %eax
    orl  $CR0_PE, %eax
    movl %eax, %cr0

    # Complete the transition to 32-bit protected mode by using a long jmp
    # to reload %cs and %eip.  The segment descriptors are set up with no
    # translation, so that the mapping is still the identity mapping.
    ljmpl $(SEG_KCODE<<3), $(start32)

.code32  # Tell assembler to generate 32-bit code now.
start32:
    # Set up the protected-mode data segment registers
    movw $(SEG_KDATA<<3), %ax    # Our data segment selector
    movw %ax, %ds                # -> DS: Data Segment
    movw %ax, %es                # -> ES: Extra Segment
    movw %ax, %ss                # -> SS: Stack Segment
    movw $0, %ax                 # Zero segments not ready for use
    movw %ax, %fs                # -> FS
    movw %ax, %gs                # -> GS
    
    # CR4: enable PAE, PSE
    movl %cr4, %eax
    orl $(CR4_PAE|CR4_PSE), %eax
    movl %eax, %cr4

    # Use kmpl4_tmp as our initial page table
    movl (start-4), %eax
    movl %eax, %cr3

    # MSR EFER: enable LME (and syscall)
    movl $MSR_EFER, %ecx
    rdmsr
    orl	$(EFER_LME), %eax
    wrmsr

    # CR0: enable paging and write protect
    movl %cr0, %eax
    orl	$(CR0_PG|CR0_WP), %eax
    movl %eax, %cr0

    lgdt gdtdesc64
    movl $(SEG_KDATA << 3), %eax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es

    /* enter 64-bit mode */
    ljmp $(SEG_KCODE << 3), $_start

.code64
.global _start
_start:
    movq $entryap64phigh, %rax
    jmp *%rax
entryap64phigh:
    mov (start-16), %rsp
    # Call main_ap()
    call *(start-24)
    hlt

.p2align 2
gdt:
    SEG_NULL
    SEG32_ASM(STA_X|STA_R, 0x0, 0xFFFFFFFF)
    SEG32_ASM(STA_W, 0x0, 0xFFFFFFFF)
gdtdesc:
    .word (gdtdesc - gdt - 1)
    .long gdt

.p2align 4
gdt64:
    .quad 0
    .quad KERNEL_CS_DESC
    .quad KERNEL_DS_DESC
gdtdesc64:
    .word (gdtdesc64 - gdt64 - 1)
    .quad gdt64