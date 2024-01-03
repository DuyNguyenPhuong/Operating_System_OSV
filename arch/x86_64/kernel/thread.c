#include <arch/cpu.h>
#include <arch/trap.h>
#include <kernel/thread.h>
#include <kernel/vm.h>
#include <kernel/sched.h>
#include <kernel/console.h>
#include <lib/stddef.h>
#include <lib/string.h>

struct stack_frame {
    void *return_addr;
};

extern void trapret(void);
extern void thread_start();
extern void kstart(); // sets up arguments and call into thread_kstart

struct thread* 
thread_current(void)
{
    return *(struct thread**) thread_kstack();
}

vaddr_t
thread_kstack(void) 
{
    uint64_t rsp;
    /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page. Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the current thread. */
    asm("mov %%rsp, %0" : "=g"(rsp));
    return pg_round_down(rsp);
}

void 
dump_context(struct thread* t)
{
    struct context *context = t->sched_ctx;
    kprintf("====================== Dumping %s's context \n", t->name);
    kprintf("r15: %p \n", context->r15);
    kprintf("r14: %p \n", context->r14);
    kprintf("r13: %p \n", context->r13);
    kprintf("r12: %p \n", context->r12);
    kprintf("r11: %p \n", context->r11);
    kprintf("rbx: %p \n", context->rbx);
    kprintf("rbp: %p \n", context->rbp);
    kprintf("rip: %p \n", context->rip);
    kprintf("====================== \n");
}

void
thread_start_context(struct thread *t, thread_func func, void *aux) 
{
    kassert(t);
    vaddr_t* return_addr = (vaddr_t*) t->sched_ctx;
    return_addr--;

    // determine if a thread is attached to a process or not
    if (t->proc) {
        *return_addr = (vaddr_t) trapret; 
        t->sched_ctx = (struct context*) return_addr;
        t->sched_ctx--;
        t->sched_ctx->rip = (vaddr_t) thread_start;
    } else {
        kassert(func);
        *return_addr = (vaddr_t) NULL; 
        t->sched_ctx = (struct context*) return_addr;
        t->sched_ctx--;
        t->sched_ctx->rip = (vaddr_t) kstart;
        t->sched_ctx->r12 = (vaddr_t) func;
        t->sched_ctx->r13 = (vaddr_t) aux;
    }
    sched_ready(t);
}
