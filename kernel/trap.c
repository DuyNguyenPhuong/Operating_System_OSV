#include <arch/cpu.h>
#include <arch/trap.h>
#include <kernel/trap.h>
#include <kernel/kmalloc.h>
#include <kernel/console.h>
#include <kernel/synch.h>
#include <kernel/timer.h>
#include <kernel/radix_tree.h>
#include <lib/errcode.h>

/*
 * Use a radix tree to store registered trap handlers.
 */
static struct radix_tree_root trap_handler_table;
static struct spinlock table_lock;
struct table_entry {
    void *dev; // device information
    trap_handler *handler; // trap handler;
};
static struct kmem_cache *entry_allocator;

/*
 * Machine-dependent trap handler registration.
 */
extern err_t syscall_register_trap_handler(void);
extern err_t pgfault_register_trap_handler(void);

void
trap_sys_init(void)
{
    radix_tree_construct(&trap_handler_table);
    if ((entry_allocator = kmem_cache_create(sizeof(struct table_entry))) == NULL) {
        panic("Failed to create trap handler table entry allocator\n");
    }
    spinlock_init(&table_lock);
    // Register all pre-defined trap handlers
    if (timer_register_trap_handler() != ERR_OK) {
        goto fail;
    }
    if (syscall_register_trap_handler() != ERR_OK) {
        goto fail;
    }
    if (pgfault_register_trap_handler() != ERR_OK) {
        goto fail;
    }
    return;
fail:
    panic("Failed to register trap handlers\n");
}

err_t
trap_register_handler(irq_t irq, void *dev, trap_handler *handler)
{
    struct table_entry *entry;
    err_t err;

    kassert(handler);
    if ((entry = kmem_cache_alloc(entry_allocator)) == NULL) {
        return ERR_TRAP_REG_FAIL;
    }
    entry->dev = dev;
    entry->handler = handler;
    // Add entry to table
    spinlock_acquire(&table_lock);
    err = radix_tree_insert(&trap_handler_table, irq, entry);
    spinlock_release(&table_lock);

    return err == ERR_OK ? ERR_OK : ERR_TRAP_REG_FAIL;
}

err_t
trap_unregister_handler(irq_t irq)
{
    struct table_entry *entry;

    spinlock_acquire(&table_lock);
    entry = radix_tree_lookup(&trap_handler_table, irq);
    if (entry != NULL) {
        radix_tree_remove(&trap_handler_table, irq);
        kmem_cache_free(entry_allocator, entry);
    }
    spinlock_release(&table_lock);

    return entry != NULL ? ERR_OK : ERR_TRAP_NOT_FOUND;
}

err_t
trap_invoke_handler(irq_t irq, void *regs)
{
    struct table_entry *entry;
    void *dev;
    trap_handler *handler;
    spinlock_acquire(&table_lock);
    entry = radix_tree_lookup(&trap_handler_table, irq);
    if (entry != NULL) {
        dev = entry->dev;
        handler = entry->handler;
    }
    spinlock_release(&table_lock);

    if (entry != NULL) {
        handler(irq, dev, regs);
        return ERR_OK;
    } else {
        return ERR_TRAP_NOT_FOUND;
    }
}
