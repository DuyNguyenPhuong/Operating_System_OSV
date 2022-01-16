#ifndef _THREAD_H_
#define _THREAD_H_

#include <kernel/types.h>
#include <kernel/list.h>

#define THREAD_NAME_LEN 32
#define DEFAULT_PRI 10

/* States a thread can be in. */
typedef enum {
    RUNNING,    /* running */
    READY,      /* ready to run */
    SLEEPING,   /* sleeping */
    ZOMBIE,     /* zombie; exited but not yet deleted */
} threadstate_t;


struct thread {
    char name[THREAD_NAME_LEN];
    int priority;
    tid_t tid;
    threadstate_t state;
    struct proc *proc;
    struct context* sched_ctx;  // thread context used for scheduling
    struct trapframe *tf;       // current trapframe of the thread
    Node node;                  // used to track the thread in ready list or other blocking list 
    Node thread_node;           // connect threads belonging to the same process
};

typedef int thread_func(void *aux);

void thread_sys_init(void);

/* Create a new thread, attach it to process p */
struct thread* thread_create(const char *name, struct proc *p, int priority);


/* Return current thread running on mycpu */
struct thread* thread_current(void);

/* Return current thread kstack address */
vaddr_t thread_kstack(void);

/* Mark current thread exit */
void thread_exit(int exit_status);

/* Free up the given thread's resource */
void thread_cleanup(struct thread *t);

/* Return the idle thread */
struct thread* thread_idle();

/* Start execution of a thread, func and aux should be NULL for user process threads. */
void thread_start_context(struct thread *t, thread_func func, void *aux); 


// void* thread_get_context(struct thread *t);
/* Debug function */
// void dump_context(struct thread* t);

#endif /* _THREAD_H_ */
