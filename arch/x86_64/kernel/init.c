#include <arch/asm.h>
#include <arch/mp.h>
#include <arch/lapic.h>
#include <arch/pic.h>
#include <arch/ioapic.h>
#include <arch/vm.h>
#include <arch/trap.h>
#include <arch/cpu.h>

void
arch_init(void)
{
    mp_init();
    lapic_init();
    pic_init();
    ioapic_init();
    seg_init();
    idt_init();
    idt_load();
    xchg(&(mycpu()->started), 1); // inform other processors we are up
}

void
arch_init_ap(void)
{
    seg_init();
    lapic_init();
    idt_load();
    xchg(&(mycpu()->started), 1); // inform other processors we are up
}