#include <arch/trap.h>
#include <arch/asm.h>
#include <kernel/thread.h>
#include <kernel/types.h>
#include <kernel/pgfault.h>
#include <kernel/console.h>
#include <kernel/trap.h>
#include <lib/stddef.h>

/*
 * Page fault error masks
 */
#define PF_P    0x1
#define PF_W    0x2
#define PF_U    0x4

/*
 * x86_64 page fault trap handler
 */
static void x86_64_pgfault_trap_handler(irq_t irq, void *dev, void *regs);

static void
x86_64_pgfault_trap_handler(irq_t irq, void *dev, void *regs)
{
    struct trapframe *tf = (struct trapframe*) regs;

    kassert(regs);
    kassert(irq == T_PF);

    uint32_t err_code = tf->err;
    thread_current()->tf = tf;
    handle_page_fault(rcr2(), err_code & PF_P, err_code & PF_W, err_code & PF_U);
}

err_t
pgfault_register_trap_handler(void)
{
    return trap_register_handler(T_PF, NULL, x86_64_pgfault_trap_handler);
}
