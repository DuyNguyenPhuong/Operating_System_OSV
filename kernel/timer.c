#include <kernel/timer.h>
#include <kernel/synch.h>
#include <kernel/console.h>
#include <kernel/trap.h>
#include <kernel/sched.h>
// T_IRQ_TIMER is defined in arch-specific trap header
#include <arch/trap.h>
#include <arch/cpu.h>

static uint32_t ticks;
static struct spinlock timer_lock;

/*
 * timer trap handler
 */
static void timer_trap_handler(irq_t irq, void *dev, void *regs);

static void
timer_trap_handler(irq_t irq, void *dev, void *regs)
{
    // Increment timer ticks
    spinlock_acquire(&timer_lock);
    ticks++;
    spinlock_release(&timer_lock);
    trap_notify_irq_completion();
    sched_sched(READY, NULL);
}

err_t timer_register_trap_handler(void)
{
    ticks = 0;
    spinlock_init(&timer_lock);
    return trap_register_handler(T_IRQ_TIMER, NULL, timer_trap_handler);
}
