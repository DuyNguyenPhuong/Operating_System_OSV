#include <kernel/multiboot.h>
#include <arch/asmboot.h>

#define SECT_SIZE 512

void read_seg(uint8_t *paddr, uint32_t count, uint32_t offset);

void
bootmain(void)
{
    struct multiboot_header *hdr;
    uint32_t* elf;
    void (*entry)(void);

    // Read first page of disk into memory address 0x10000
    // (should be enough for ELF header)
    elf = (uint32_t*) 0x10000;
    read_seg((uint8_t*) elf, 4196, 0);
    // don't have enough space to check ELF magic oops
    for (int n = 0; n < 4196/sizeof(*elf); n++) {
        if (elf[n] == MULTIBOOT_HEADER_MAGIC) {
            hdr = (struct multiboot_header *) (elf + n);
            goto found_multiboot;
        }
    }
    return;
found_multiboot:
    read_seg((uint8_t*) hdr->load_addr, (hdr->load_end_addr - hdr->load_addr),
        ((uint32_t) hdr - (uint32_t) elf) - (hdr->header_addr - hdr->load_addr));

    // If too much RAM was allocated, then zero redundant RAM
    if (hdr->bss_end_addr > hdr->load_end_addr) {
        stosb((void*) hdr->load_end_addr, 0,
        hdr->bss_end_addr - hdr->load_end_addr);
    }

    entry = (void(*)(void))(hdr->entry_addr);
    // Call the entry point from the multiboot header.
    // Should not return!
    asm volatile("\tmovl %0, %%eax\n"
               "\tcall *%1\n"
               :
               : "r"(MULTIBOOT_BOOTLOADER_MAGIC), "r"(entry)
               : "eax");
}

void
wait_disk()
{
    // Wait for disk ready.
    while((inb(0x1F7) & 0xC0) != 0x40) {
    }
}

void
read_sect(void *dst, uint32_t offset)
{
    // Issue command
    wait_disk();
    outb(0x1F2, 1);   // count = 1
    outb(0x1F3, offset);
    outb(0x1F4, offset >> 8);
    outb(0x1F5, offset >> 16);
    outb(0x1F6, (offset >> 24) | 0xE0);
    outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

    // Read data.
    wait_disk();
    insl(0x1F0, dst, SECT_SIZE/4);
}

void
read_seg(uint8_t *paddr, uint32_t count, uint32_t offset)
{
    uint8_t* end_paddr;
    end_paddr = paddr + count;
    paddr -= offset % SECT_SIZE;
    // kernel image starts at sector 1 (sector 0 contains bootloader)
    offset = (offset / SECT_SIZE) + 1;

    for (; paddr < end_paddr; paddr += SECT_SIZE, offset++) {
        read_sect(paddr, offset);
    }
}
