#include <kernel/proc.h>
#include <kernel/console.h>
#include <kernel/trap.h>
#include <arch/mmu.h>
#include <lib/errcode.h>
#include <kernel/vpmap.h>
#include <string.h>

size_t user_pgfault = 0;

void handle_page_fault(vaddr_t fault_addr, int present, int write, int user)
{
    if (user)
    {
        __sync_add_and_fetch(&user_pgfault, 1);
    }
    // turn on interrupt now that we have the fault address
    intr_set_level(INTR_ON);

    struct proc *curproc = proc_current();
    kassert(curproc);

    // Check valid access within the stack region
    vaddr_t stack_lower_bound = USTACK_UPPERBOUND - (pg_size * USTACK_PAGES);
    if (user && fault_addr >= stack_lower_bound && fault_addr < USTACK_UPPERBOUND)
    {
        // Allocate and map a new page for stack growth
        paddr_t paddr;
        if (pmem_alloc(&paddr) != ERR_OK)
        {
            proc_exit(-1);
            return;
        }

        // Zero the page
        memset((void *)kmap_p2v(paddr), 0, pg_size);

        // Set the offset bit to 0s
        vaddr_t aligned_fault_addr = fault_addr & ~(pg_size - 1);
        if (vpmap_map(curproc->as.vpmap, aligned_fault_addr, paddr, 1, MEMPERM_URW) != ERR_OK)
        {
            pmem_free(paddr);
            proc_exit(-1);
            return;
        }
        return;
    }
    else if (user && fault_addr >= curproc->as.heap->start && fault_addr < curproc->as.heap->end)
    {
        // The fault address is within the heap region.
        // if (!present)
        // {
        paddr_t paddr;
        if (pmem_alloc(&paddr) != ERR_OK)
        {
            proc_exit(-1);
            return;
        }

        memset((void *)kmap_p2v(paddr), 0, pg_size);

        // Set the offset bit to 0s
        vaddr_t aligned_fault_addr = fault_addr & ~(pg_size - 1);
        if (vpmap_map(curproc->as.vpmap, aligned_fault_addr, paddr, 1, MEMPERM_URW) != ERR_OK)
        {
            pmem_free(paddr);
            proc_exit(-1);
            return;
        }
        return;
    }

    if (user)
    {
        proc_exit(-1);
        panic("unreachable");
    }
    else
    {
        // kprintf("fault addr %p\n", fault_addr);
        panic("Kernel error in page fault handler\n");
    }
}
