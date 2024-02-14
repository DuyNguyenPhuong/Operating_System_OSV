#ifndef _BBQ_H_
#define _BBQ_H_

#include <kernel/types.h>
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

#define BBQ_SIZE 512 // Buffer size

// Bounded Buffer Queue
typedef struct bbq
{
    char buffer[BBQ_SIZE];
    unsigned int head;
    unsigned int tail;
    unsigned int count;
    struct spinlock lock;
} bbq_t;

void bbq_init(bbq_t *bbq);
ssize_t bbq_write(bbq_t *bbq, const char *data, size_t size);
ssize_t bbq_read(bbq_t *bbq, char *data, size_t size);

#endif // _BBQ_H_
