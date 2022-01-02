#include <arch/mmu.h>
#include <arch/cpu.h>
#include <arch/pmem.h>

# Start the first CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.code16
.globl start
start:
    cli

# Zero DS, ES and SS
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

# Enable address line A20
seta20.1:
    inb $0x64, %al
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al
    outb %al, $0x64

seta20.2:
    inb $0x64, %al
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al
    outb %al, $0x60

# Detect E820 memory map
e820_start:
    xorl %ebx, %ebx
    movw $E820_MAP_PADDR, %di # store e820 entires at E820_MAP_PADDR
e820_loop:
    movl $0x534D4150, %edx
    movl $0xE820, %eax
    movl $20, %ecx # ignore ACPI 3.0 extended attributes
    int $0x15
    jc e820_end # carry should be cleared
    cmpl $0x534D4150, %eax
    jne e820_end # eax should be set to the magic number
    cmpw $20, %cx
    jg e820_skip # entry should be at least 20 bytes
    addw $20, %di # next entry
e820_skip:
    test %ebx, %ebx
    jne e820_loop # done when ebx=0
e820_end:
    mov $E820_MAP_PADDR_END, %eax
    mov %edi, (%eax)

# Switch to protected mode
    lgdt gdtdesc
    movl %cr0, %eax
    orl $CR0_PE, %eax
    movl %eax, %cr0

    ljmp $(SEG_KCODE<<3), $start32

.code32
start32:
# Set up data segment
    movw $(SEG_KDATA<<3), %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0, %ax
    movw %ax, %fs
    movw %ax, %gs

# call into C bootloader code
    movl $start, %esp
    call bootmain

# halt the CPU if returned
    hlt

.p2align 2
gdt:
    SEG_NULL
    SEG32_ASM(STA_X|STA_R, 0x0, 0xFFFFFFFF)
    SEG32_ASM(STA_W, 0x0, 0xFFFFFFFF)

gdtdesc:
    .word (gdtdesc - gdt - 1)
    .long gdt
