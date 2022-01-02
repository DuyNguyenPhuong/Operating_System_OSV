#ifndef _SCHED_H_
#define _SCHED_H_

#include <kernel/synch.h>
#include <kernel/thread.h>

struct spinlock sched_lock;

/* initialize lists used by scheduler */
void sched_sys_init();

/* start scheduling for the calling AP, interrupt must be off when calling this */
err_t sched_start();

/* start scheduling for the calling AP, interrupt must be off when calling this */
err_t sched_start_ap();

/* Add thread to ready queue */
void sched_ready(struct thread*);

/* 
 * Schedule another thread to run. 
 * Current thread transition to the next state, release lock passed in if not null 
*/
void sched_sched(threadstate_t next_state, void* lock) ;


#endif /* _SCHED_H_ */
