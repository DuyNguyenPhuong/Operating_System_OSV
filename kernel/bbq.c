#include <kernel/bbq.h>
#include <kernel/types.h> // For size_t
#include <kernel/console.h>
#include <kernel/proc.h>
#include <kernel/kmalloc.h>
#include <kernel/thread.h>
#include <kernel/list.h>
#include <kernel/fs.h>
#include <kernel/vpmap.h>
#include <arch/elf.h>
#include <arch/trap.h>
#include <arch/mmu.h>
#include <lib/errcode.h>
#include <lib/stddef.h>
#include <lib/string.h>

void bbq_init(bbq_t *bbq)
{
    bbq->head = 0;
    bbq->tail = 0;
    bbq->count = 0;
    spinlock_init(&bbq->lock);
}

ssize_t bbq_write(bbq_t *bbq, const char *data, size_t size)
{
    spinlock_acquire(&bbq->lock);
    size_t bytes_written = 0;
    while (bytes_written < size && bbq->count < BBQ_SIZE)
    {
        bbq->buffer[bbq->tail] = data[bytes_written++];
        bbq->tail = (bbq->tail + 1) % BBQ_SIZE;
        bbq->count++;
    }
    spinlock_release(&bbq->lock);
    return bytes_written;
}

ssize_t bbq_read(bbq_t *bbq, char *data, size_t size)
{
    spinlock_acquire(&bbq->lock);
    size_t bytes_read = 0;
    while (bytes_read < size && bbq->count > 0)
    {
        data[bytes_read++] = bbq->buffer[bbq->head];
        bbq->head = (bbq->head + 1) % BBQ_SIZE;
        bbq->count--;
    }
    spinlock_release(&bbq->lock);
    return bytes_read;
}
