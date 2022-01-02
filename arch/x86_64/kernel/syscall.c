#include <arch/trap.h>
#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/thread.h>
#include <kernel/console.h>
#include <kernel/trap.h>
#include <lib/errcode.h>
#include <lib/stddef.h>

extern sysret_t syscall(int num, void *arg);

/*
 * x86 syscall trap handler
 */
static void x86_64_syscall_trap_handler(irq_t irq, void *dev, void *regs);

static void
x86_64_syscall_trap_handler(irq_t irq, void *dev, void *regs)
{
    struct trapframe *tf;

    kassert(regs);
    kassert(irq == T_SYSCALL);
    tf = (struct trapframe*)regs;
    thread_current()->tf = tf;
    tf->rax = syscall(tf->rax, (void*) tf);
}

err_t
syscall_register_trap_handler(void)
{
    return trap_register_handler(T_SYSCALL, NULL, x86_64_syscall_trap_handler);
}

bool
fetch_arg(void *arg, int n, sysarg_t *ret)
{
    kassert(ret);
    struct trapframe *tf = (struct trapframe*) arg;
    switch (n) {
        case 1:
            *ret = tf->rdi;
            break;
        case 2:
            *ret = tf->rsi;
            break;
        case 3:
            *ret = tf->rdx;
            break;
        case 4:
            *ret = tf->rcx;
            break;
        case 5:
            *ret = tf->r8;
            break;
        case 6:
            *ret = tf->r9;
            break;
        default:
            return False;
    }
    return True;
}
