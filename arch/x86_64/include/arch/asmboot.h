#include <stdint.h>

static inline uint8_t
inb(uint16_t port)
{
    uint8_t res;
    asm volatile("inb %1, %0"
                 : "=a" (res)
                 : "d" (port));
    return res;
}

static inline void
insl(uint16_t port, void *addr, uint32_t cnt)
{
    asm volatile("cld; rep insl"
                 : "=D" (addr), "=c" (cnt)
                 : "0" (addr), "1" (cnt), "d" (port)
                 : "memory", "cc");
}

static inline void
outb(uint16_t port, uint8_t data)
{
    asm volatile("outb %0, %1"
                 :
                 : "a" (data), "d" (port));
}

static inline void
stosb(void *addr, uint8_t data, uint32_t cnt)
{
    asm volatile("cld; rep stosb"
                 : "=D" (addr), "=c" (cnt)
                 : "0" (addr), "1" (cnt), "a" (data)
                 : "memory", "cc");
}
