#include <kernel/trap.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/console.h>
#include <kernel/sched.h>
#include <kernel/timer.h>
#include <lib/errcode.h>
#include <lib/string.h>
#include <lib/stddef.h>
#include <arch/cpu.h>
#include <arch/trap.h>


struct kmem_cache *thread_allocator;
struct spinlock tid_lock;
static tid_t tid_allocator;

void thread_sys_init(void)
{
    sched_sys_init();

    thread_allocator = kmem_cache_create(sizeof(struct thread));
    kassert(thread_allocator);
    spinlock_init(&tid_lock);
    // initialize idle thread and starts interrupt
    kassert(thread_create("idle thread", NULL, DEFAULT_PRI) != NULL);
}

/* Create a new thread, attach it to process p */
struct thread*
thread_create(const char *name, struct proc *p, int priority)
{
    kassert(name);

    struct thread *t = kmem_cache_alloc(thread_allocator);
    if (t == NULL) {
        return NULL;
    }

    // allocate kstack for thread t
    vaddr_t vaddr;
    // call to mycpu is safe because either cpu_idle_thread is NULL
    // which implies that interrupt is disabled and scheduler hasn't started
    // on this cpu, or cpu_idle_thread will not be NULL.
    if (cpu_idle_thread(mycpu()) == NULL) {
        vaddr = (vaddr_t) thread_kstack();
    } else {
        paddr_t paddr;
        if (pmem_alloc(&paddr) != ERR_OK) {
            return NULL;
        }
        vaddr = kmap_p2v(paddr);
    }
    spinlock_acquire(&tid_lock);
    t->tid = ++tid_allocator;
    spinlock_release(&tid_lock);
    size_t slen = strlen(name);
    slen = slen < THREAD_NAME_LEN-1 ? slen : THREAD_NAME_LEN-1;
    memcpy(t->name, name, slen);
    t->name[slen] = 0;
    t->proc = p;
    t->priority = priority;

    // allocate a trapframe for thread at top of kstack
    t->tf = (void*) (vaddr + pg_size - sizeof(*t->tf)); 
    // store current kstack top at sched_ctx
    t->sched_ctx = (void*) t->tf;
    // add reference to thread at the bottom of kstack
    *(struct thread**) (vaddr) = t; 

    proc_attach_thread(p, t);
    return t;
}

void
thread_exit(int exit_status)
{
    // user process only have one thread so there's no thread joining 
    sched_sched(ZOMBIE, NULL);
}

void thread_cleanup(struct thread *t)
{
    kassert(thread_current() != t);
    // safe to free kstack because this thread will not be scheduled again
    vaddr_t kstack = pg_round_down((vaddr_t)t->sched_ctx);
    pmem_free(kmap_v2p(kstack));
    // free the thread struct itself
    kmem_cache_free(thread_allocator, t);
}

/* basic thread start used for starting user threads */
void
thread_start()
{
    kassert(intr_get_level() == INTR_OFF);
    spinlock_release(&sched_lock);
}

/*
 * Intermediate function that makes sure all threads we create either
 * never return to us or exit properly through us.
 * Used for starting kernel threads
*/
void
thread_kstart(thread_func func, void *aux)
{
    kassert(thread_current()->proc == NULL);
    thread_start();
    kassert(func);
    thread_exit(func(aux));
    panic("thread_exit should not return");
}
