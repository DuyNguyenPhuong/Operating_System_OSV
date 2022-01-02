#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/lapic.h>
#include <arch/ioapic.h>
#include <arch/mmu.h>
#include <arch/trap.h>
#include <kernel/console.h>
#include <kernel/proc.h>
#include <kernel/trap.h>
#include <kernel/vpmap.h>
#include <kernel/thread.h>
#include <lib/errcode.h>
#include <lib/stddef.h>

// IDT shared by all processors
struct gatedesc idt[256];

// Vector table addresses, defined in a separate vectors file
extern vaddr_t vectors[];

void
idt_init(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        idt[i] = GATE(vectors[i], SEG_KCODE << 3, 0, DPL_KERNEL);
    }
    // Syscall from user space requires DPL_USER
    idt[T_SYSCALL] = GATE(vectors[T_SYSCALL], SEG_KCODE << 3, 1, DPL_USER);
}

void
idt_load(void)
{
    lidt(idt, sizeof(idt));
}

void
tf_set_return(struct trapframe *tf, uint64_t retval)
{
    kassert(tf);
    tf->rax = retval;
}

void
tf_proc(struct trapframe *tf, struct proc *p, uint64_t entry_point, uint64_t stack_ptr)
{
    paddr_t paddr = 0;
    uint64_t *sp;
    kassert(tf && p);
    // double check given stackptr is mapped to a physical address
    sleeplock_acquire(&p->as.as_lock);
    kassert(vpmap_lookup_vaddr(p->as.vpmap, stack_ptr, &paddr, NULL) == ERR_OK);
    sleeplock_release(&p->as.as_lock);
    sp = (uint64_t*) kmap_p2v(paddr);

    tf->cs = (SEG_UCODE << 3) | DPL_USER;
    tf->ss = (SEG_UDATA << 3) | DPL_USER;
    tf->rflags = FL_IF;
    tf->rsp = stack_ptr;
    tf->rip = entry_point;
    // also need to set up arguments for new process
    tf->rdi = sp[1];
    tf->rsi = sp[2];
}

void
trap(struct trapframe *tf)
{
    err_t err;
    kassert(tf != NULL);
    err = trap_invoke_handler(tf->trapnum, tf);
    if (err == ERR_TRAP_NOT_FOUND) {
        // IRQ not registered yet. Just notify completion of interrupt.
        trap_notify_irq_completion();
    }
}

/*
 * Implementation of machine-dependent functions in <kernel/trap.h>
 */
err_t
trap_enable_irq(irq_t irq)
{
    // XXX currently route all IRQ to cpu 0
    ioapic_enable(irq, 0);
    return ERR_OK;
}

err_t
trap_disable_irq(irq_t irq)
{
    ioapic_disable(irq);
    return ERR_OK;
}

void
trap_notify_irq_completion(void)
{
    lapic_eoi();
}

static void
intr_enable(void)
{
    sti();
}

static void
intr_disable(void)
{
    cli();
}

intr_t
intr_get_level(void)
{
    return readeflags() & FL_IF ? INTR_ON : INTR_OFF;
}

intr_t
intr_set_level(intr_t level)
{
    intr_t current_level = intr_get_level();
    struct x86_64_cpu *cpu = NULL;

    if (level == INTR_ON) {
        kassert(current_level == INTR_OFF);
        cpu = mycpu();
        if (cpu->num_disabled == 0) {
            intr_enable();
            return current_level;
        }
        // reverse number of interrupt disable
        if (cpu->num_disabled > 0) {
            cpu->num_disabled--;
        }
        if (cpu->num_disabled == 0 && cpu->intr_enabled) {
            intr_enable();
        }
    } else {
        intr_disable();
        cpu = mycpu();
        if (cpu->num_disabled == 0) {
            cpu->intr_enabled = (current_level == INTR_ON);
        }
        cpu->num_disabled++;
    }
    return current_level;
}
