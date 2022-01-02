OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

SECTIONS
{
    . = 0xFFFFFFFF80100000;

    .text : AT(0x100000) {
        *(.head.text)
        *(.text .text.*)
        _etext = .;
    }
    .rodata : {
        *(.rodata .rodata.*)
    }
    . = ALIGN(0x1000);
    PROVIDE(_data = .);
    .data : {
        *(.data .data.*)
        _edata = .;
    }
    .bss : {
        *(.bss .bss.*)
    }
    . = ALIGN(0x1000);
    PROVIDE(_end = .);
}
