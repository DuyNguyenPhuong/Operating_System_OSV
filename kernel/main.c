#include <arch/mp.h>
#include <kernel/vm.h>
#include <kernel/thread.h>
#include <kernel/proc.h>
#include <kernel/console.h>
#include <kernel/arch.h>
#include <kernel/tests.h>
#include <kernel/sched.h>
#include <kernel/trap.h>
#include <kernel/bdev.h>
#include <kernel/fs.h>
#include <kernel/vpmap.h>
#include <kernel/pmem.h>
#include <lib/errcode.h>

int kernel_init(void *args);

int
kernel_init(void *args)
{
    bdev_init();
    fs_init();
    mp_start_ap();
    kprintf("OSV initialization...Done\n\n");

    // spawn initial process - init
    char* argv[2] = {"init", NULL};
    kassert(proc_spawn("init", argv, &init_proc) == ERR_OK);
    return 0;
}

int
main(void)
{
    vm_init();
    arch_init();

    // thread needs to be initialized before other sub systems can use locks
    thread_sys_init();
    synch_init();
    proc_sys_init();
    trap_sys_init();
    console_init();
    pmem_info();

    // start scheduling: turn on interrupt
    sched_start();
    // Create a kernel thread to run the rest of initialization (in case they
    // need to run blocking I/O)
    struct thread *t = thread_create("init/testing thread", NULL, DEFAULT_PRI);
    kassert(t);
    thread_start_context(t, kernel_init, NULL);
    for (;;) { }
}

// Other CPUs jump here from entry_ap.S.
void
main_ap(void)
{
    vpmap_load(kas->vpmap);
    arch_init_ap();
    // start scheduling: create an idle thread for this cpu and turn on interrupt
    sched_start_ap();
    for (;;) { } // loop bc we are idle
}
