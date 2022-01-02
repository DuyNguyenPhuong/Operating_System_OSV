#include <kernel/console.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <kernel/vpmap.h>
#include <kernel/trap.h>
#include <arch/mmu.h>
#include <arch/cpu.h>
#include <arch/asm.h>
#include <arch/lapic.h>
#include <lib/stddef.h>
#include <kernel/kmalloc.h>

extern struct thread* swtch(struct context**, struct context*);

struct x86_64_cpu*
mycpu(void)
{
    uint8_t lapic_id;
    int i;

    lapic_id = lapic_read_id();
    for (i = 0; i < ncpu; i++) {
        if (x86_64_cpus[i].lapic_id == lapic_id) {
            return &x86_64_cpus[i];
        }
    }
    panic("CPU not properly initialized");
    return 0;
}

void
cpu_clear_thread(struct x86_64_cpu *c)
{
    c->thread = NULL;
}

struct thread*
cpu_switch_thread(struct x86_64_cpu* c, struct thread* t)
{
    kassert(intr_get_level() == INTR_OFF);
    struct addrspace *as;
    struct thread *prev;
    struct thread *curr = thread_current();

    as = t->proc == NULL ? kas : &t->proc->as;
    c->thread = t;
    c->ts.rsp0 = pg_round_down((vaddr_t)t->sched_ctx) + pg_size;
    // switch to process's address space
    vpmap_load(as->vpmap);
    c->intr_enabled = 1;
    prev = (struct thread *) swtch(&curr->sched_ctx, t->sched_ctx);
    return prev;
}

struct thread*
cpu_idle_thread(struct x86_64_cpu* cpu)
{
    return cpu->idle_thread;
}

void
cpu_set_idle_thread(struct x86_64_cpu* cpu, struct thread *t)
{
    kassert(cpu->idle_thread == NULL);
    kassert(t != NULL);
    cpu->idle_thread = t;
}
