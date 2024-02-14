// Try adding this in kernel/

#include <kernel/kmalloc.h> // again, this could be added to osv
#include <kernel/bbq.h>

// static prevents this variable from being visible outside this file
static struct kmem_cache *bbq_allocator;

// Wait until there is room and then insert an item.
void bbq_insert(BBQ *q, char item)
{
    spinlock_acquire(&q->lock);

    // Wait until there is space
    while ((q->next_empty - q->front) == MAX_BBQ_SIZE)
    {
        condvar_wait(&q->item_removed, &q->lock);
    }

    // Add the item
    q->items[q->next_empty % MAX_BBQ_SIZE] = item;
    q->next_empty++;

    // Signal that it's there
    condvar_signal(&q->item_added);

    spinlock_release(&q->lock);
}

// Wait until there is an item and then remove an item.
char bbq_remove(BBQ *q)
{
    char item;

    spinlock_acquire(&q->lock);

    // Wait until there is something in the queue
    while (q->front == q->next_empty)
    {
        condvar_wait(&q->item_added, &q->lock);
    }

    // Grab the item
    item = q->items[q->front % MAX_BBQ_SIZE];
    q->front++;

    // Signal that we removed something
    condvar_signal(&q->item_removed);

    spinlock_release(&q->lock);
    return item;
}

// Initialize the queue to empty, the lock to free, and the
// condition variables to empty.
BBQ *bbq_init()
{
    BBQ *q;

    // If the allocator has not been created yet, do so now
    if (bbq_allocator == NULL)
    {
        if ((bbq_allocator = kmem_cache_create(sizeof(BBQ))) == NULL)
        {
            return NULL;
        }
    }

    // Allocate the BBQ struct
    if ((q = kmem_cache_alloc(bbq_allocator)) == NULL)
    {
        return NULL;
    }

    // Initialize state variables
    q->front = 0;
    q->next_empty = 0;

    // Initialize synchronization variables
    spinlock_init(&q->lock);
    condvar_init(&q->item_added);
    condvar_init(&q->item_removed);

    return q;
}

void bbq_free(BBQ *q)
{
    kmem_cache_free(bbq_allocator, q);
}

bool bbq_is_empty(BBQ *q)
{
    spinlock_acquire(&q->lock);
    bool empty = (q->front == q->next_empty);
    spinlock_release(&q->lock);
    return empty;
}

// Checks if the BBQ is full
bool bbq_is_full(BBQ *q)
{
    spinlock_acquire(&q->lock);
    bool full = ((q->next_empty - q->front) == MAX_BBQ_SIZE);
    spinlock_release(&q->lock);
    return full;
}