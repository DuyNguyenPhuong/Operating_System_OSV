#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int pid, ret, i;
    struct sys_info info_before, info_after;
    size_t PAGES = 2048;
    info(&info_before);
    volatile char *a = sbrk(PAGES * 4096);
    info(&info_after);

    if (info_before.num_pgfault - info_after.num_pgfault > 0) {
        error("cow-large: calling sbrk caused page faults, expected fault number is %d, actual is %d",
            info_before.num_pgfault, info_after.num_pgfault);
    }

    // allocate PAGES pages on the heap, page them in
    // and store current page fault number
    for (i = 0; i < PAGES; i++) {
        a[i * 4096] = i;
    }

    // fork a child
    if ((pid = fork()) == 0) {
        // read the same heap and see if reads cause more page fault
        info(&info_before);
        for (i = 0; i < PAGES; i++) {
            a[i * 4096];
        }
        info(&info_after);
        // read should not increase the amount of memory allocated, though each 
        // process will write to a stack page when returning from fork
        if (info_after.num_pgfault - info_before.num_pgfault > 2) {
            error("cow-large: child caused more page faults on reads, expected fault number is %d, actual is %d",
                info_before.num_pgfault + 2, info_after.num_pgfault);
        }
        // now write to the heap
        for (i = 0; i < PAGES; i++) {
            a[i * 4096] = 'c';
        }
        info(&info_after);
        // write should allocate the PAGES pages of memory
        if (info_after.num_pgfault-info_before.num_pgfault < PAGES) {
            error("cow-large: child didn't result in more page faults on writes, expected fault number is at least %d, actual is %d",
                info_before.num_pgfault + PAGES, info_after.num_pgfault);
        }
        exit(0);
    } else {
        if ((ret = wait(pid, NULL)) != pid) {
            error("cow-large: wait failed, return value was %d", ret);
        }
    }

    pass("cow-large");
    exit(0);
    return 0;
}
