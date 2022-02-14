#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int pid, ret, i;
    struct sys_info info1, info2, info3;
    size_t PAGES = 1;
    volatile char *a = sbrk(PAGES * 4096);

    // allocate one page on the heap, page them in
    // and store current page fault number
    for (i = 0; i < PAGES; i++) {
        a[i * 4096] = i;
    }
    info(&info1);

    // fork a child
    if ((pid = fork()) == 0) {
        // read the same heap and see if reads cause more page fault
        for (i = 0; i < PAGES; i++) {
            a[i * 4096];
        }
        info(&info2);
        // read should not increase the amount of memory allocated, though each 
        // process will write to a stack page when returning from fork
        if (info2.num_pgfault - info1.num_pgfault > 2) {
            error("cow-small: child caused more page faults on reads, expected fault number is %d, actual is %d",
                info1.num_pgfault + 2, info2.num_pgfault);
        }
        // now write to the heap
        for (i = 0; i < PAGES; i++) {
            a[i * 4096] = 'c';
        }
        info(&info3);
        // write should allocate the 1 page of memory
        if (info3.num_pgfault-info2.num_pgfault < PAGES) {
            error("cow-small: child didn't result in more page faults on writes, expected fault number is at least %d, actual is %d",
                info2.num_pgfault + PAGES, info3.num_pgfault);
        }
        exit(pid);
    } else {
        if ((ret = wait(pid, NULL)) != pid) {
            error("cow-small: wait failed, return value was %d", ret);
        }
    }

    pass("cow-small");
    exit(pid);
    return 0;
}
