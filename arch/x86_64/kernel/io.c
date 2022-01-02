#include <kernel/io.h>
#include <arch/asm.h>

uint8_t
readb(port_t port)
{
    return inb(port);
}

void
readn(port_t port, void *addr, size_t n)
{
    insl(port, addr, n/4);
}

void
writeb(port_t port, uint8_t data)
{
    outb(port, data);
}

void
writen(port_t port, const void *addr, size_t n)
{
    outsl(port, addr, n/4);
}
