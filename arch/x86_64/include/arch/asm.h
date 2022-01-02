#ifndef _ARCH_X86_64_ASM_H_
#define _ARCH_X86_64_ASM_H_

#include <arch/asmboot.h>
#include <arch/mmu.h>

static inline void
outsl(uint16_t port, const void *addr, uint32_t cnt)
{
    asm volatile("cld; rep outsl"
                 : "=S" (addr), "=c" (cnt)
                 : "0" (addr), "1" (cnt), "d" (port)
                 : "cc");
}

static inline void
ltr(uint16_t sel)
{
  asm volatile("ltr %0" : : "r" (sel));
}

static inline uint64_t
rcr2()
{
    uint64_t cr2;
    asm volatile("mov %%cr2, %0"
                 : "=a" (cr2));
    return cr2;
}

static inline void
lcr3(uint64_t addr)
{
    asm volatile("mov %0, %%cr3"
                 :
                 : "r" (addr));
}

static inline void
lgdt(struct segdesc *gdt, size_t size)
{
    volatile uint16_t ptr[5];

    ptr[0] = size-1;
    ptr[1] =  (uint16_t)((vaddr_t) gdt);
    ptr[2] = (uint16_t) ((vaddr_t) gdt >> 16);
    ptr[3] = (uint16_t) ((vaddr_t) gdt >> 32);
    ptr[4] = (uint16_t) ((vaddr_t) gdt >> 48);

    asm volatile("lgdt (%0)"
                 :
                 : "r" (ptr));
}

static inline void
lidt(struct gatedesc *idt, size_t size)
{
    volatile uint16_t ptr[5];

    ptr[0] = size - 1;
    ptr[1] = (uint16_t) ((vaddr_t) idt);
    ptr[2] = (uint16_t) ((vaddr_t) idt >> 16);
    ptr[3] = (uint16_t) ((vaddr_t) idt >> 32);
    ptr[4] = (uint16_t) ((vaddr_t) idt >> 48);
    asm volatile("lidt (%0)"
                 :
                 : "r" (ptr));
}

static inline void
loadgs(uint16_t v)
{
    asm volatile("movw %0, %%gs" : : "r" (v));
}

static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = val & 0xffffffff, hi = val >> 32;
    asm volatile ("wrmsr"
            : : "c" (msr), "a" (lo), "d" (hi)
            : "memory");
}

static inline uint64_t
readeflags(void)
{
    uint64_t eflags;
    asm volatile("pushf; pop %0" : "=r" (eflags));
    return eflags;
}

static inline void
cli(void)
{
    asm volatile("cli");
}

static inline void
sti(void)
{
    asm volatile("sti");
}

static inline uint32_t
xchg(volatile uint32_t *addr, uint32_t newval)
{
    uint32_t result;

    // The + in "+m" denotes a read-modify-write operand.
    asm volatile("lock; xchgl %0, %1" :
                "+m" (*addr), "=a" (result) :
                "1" (newval) :
                "cc");
    return result;
}

static inline void
shutdown()
{
    int data = 0x2000;
    int port = 0x604;
    asm volatile ("outw %w0, %w1" : : "a" (data), "Nd" (port));
    asm volatile ("cli; hlt" : : : "memory");
}

#endif /* _ARCH_X86_64_ASM_H_ */
