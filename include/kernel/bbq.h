// Try adding this in include/kernel/

#ifndef __BBQ_H__
#define __BBQ_H__

#include <kernel/synch.h> // this would be something you could actually add to osv

#define MAX_BBQ_SIZE 512

typedef struct
{
    // Synchronization variables
    struct spinlock lock;
    struct condvar item_added;
    struct condvar item_removed;

    // State variables
    char items[MAX_BBQ_SIZE];
    int front;
    int next_empty;
} BBQ;

BBQ *bbq_init();
void bbq_free(BBQ *q);
void bbq_insert(BBQ *q, char item);
char bbq_remove(BBQ *q);

#endif // __BBQ_H__