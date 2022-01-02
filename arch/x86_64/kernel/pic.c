#include <arch/pic.h>
#include <arch/asm.h>

#define IO_PIC0_CMD     0x20
#define IO_PIC0_DATA    0x21
#define IO_PIC1_CMD     0xA0
#define IO_PIC1_DATA    0xA1

void
pic_init(void)
{
    // Disable PIC. Assume APIC and IOAPIC are used.
    outb(IO_PIC0_DATA, 0xFF);
    outb(IO_PIC1_DATA, 0xFF);
}
