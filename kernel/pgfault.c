#include <kernel/proc.h>
#include <kernel/console.h>
#include <kernel/trap.h>


size_t user_pgfault = 0;


void
handle_page_fault(vaddr_t fault_addr, int present, int write, int user) {
    if (user) {
        __sync_add_and_fetch(&user_pgfault, 1);
    }
    // turn on interrupt now that we have the fault address 
    intr_set_level(INTR_ON);

    /* Your Code Here. */

    if (user) {
        // kprintf("fault addres %p, present %d, wrie %d, user %d\n", fault_addr, present, write, user);
        proc_exit(-1);
        panic("unreachable");
    } else {
        // kprintf("fault addr %p\n", fault_addr);
        panic("Kernel error in page fault handler\n");
    }
}
