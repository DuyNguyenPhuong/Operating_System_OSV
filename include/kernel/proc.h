#ifndef _PROC_H_
#define _PROC_H_

#include <kernel/synch.h>
#include <kernel/vm.h>
#include <kernel/types.h>
#include <kernel/list.h>

#define ANY_CHILD -1
#define STATUS_ALIVE 0xbeefeeb
#define PROC_MAX_ARG 128
#define PROC_NAME_LEN 32
#define PROC_MAX_FILE 128

struct proc {
    pid_t pid;
    char name[PROC_NAME_LEN];
    struct addrspace as;
    struct inode *cwd;                  // current working directory
    List threads;                       // list of threads belong to the process, right now just 1 per process
    Node proc_node;                     // used by ptable to keep track each process
};

struct proc *init_proc;

void proc_sys_init(void);

/* Spawn a new process specified by executable name and argument */
err_t proc_spawn(char *name, char** argv, struct proc **p);

/* Fork a new process identical to current process */
struct proc* proc_fork();

/* Return current thread's process. NULL if current thread is not associated with any process */
struct proc* proc_current();

/* Attach a thread to a process. */
void proc_attach_thread(struct proc *proc, struct thread *t);

/* Detach a thread from its process. Returns True if detached thread is the 
 * last thread of the process, False otherwise */
bool proc_detach_thread(struct thread *t);

/*
 * Wait for a process to change state. If pid is ANY_CHILD, wait for any child process.
 * If wstatus is not NULL, store the the exit status of the child in wstatus.
 *
 * Return:
 * pid of the child process that changes state.
 * ERR_CHILD - The caller does not have a child with the specified pid.
 */
int proc_wait(pid_t, int* status);

/* Exit a process with a status */
void proc_exit(int);

#endif /* _PROC_H_ */
