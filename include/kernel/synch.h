#ifndef _SYNCH_H_
#define _SYNCH_H_

#include <kernel/list.h>

#define SPIN 0
#define SLEEP 1
#define LOCK_TYPE(lk) (*(uint8_t*)lk)

/* Spinlock */
struct spinlock {
    uint8_t type;
    uint8_t lock_status;
    struct thread *holder;
};

/* Condition variable */
struct condvar {
    List waiters;
};

/* Sleeplock */
struct sleeplock {
    uint8_t type;
    struct spinlock lk;     // spinlock that protects access to waiters
    struct condvar waiters;
    struct thread *holder;
};

void synch_init(void);

/* spinlock operations */

void spinlock_init(struct spinlock *lock);

err_t spinlock_try_acquire(struct spinlock* lock);

void spinlock_acquire(struct spinlock *lock);

void spinlock_release(struct spinlock *lock);

/* sleep lock operations */

void sleeplock_init(struct sleeplock *lock);

err_t sleeplock_try_acquire(struct sleeplock* lock);

void sleeplock_acquire(struct sleeplock *lock);

void sleeplock_release(struct sleeplock *lock);

/* generic lock (can be spin or sleeplock) operations */

void lock_acquire(void *lock);

void lock_release(void *lock);

/* condition variable operations */

void condvar_init(struct condvar *cv);

void condvar_wait(struct condvar *cv, void* lock);

void condvar_signal(struct condvar *cv);

void condvar_broadcast(struct condvar *cv);

#endif /* _SYNCH_H_ */
