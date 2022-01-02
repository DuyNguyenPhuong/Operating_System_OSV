#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int pid, ret, i;
    struct sys_info info1, info2, info3;
    size_t size = 200 * 4096;
    volatile char *a = sbrk(size);

    // allocate 200 pages on the heap, page them in
    // and store current page fault number
    for (i = 0; i < size/4096; i++) {
        a[i * 4096] = i;
    }
    info(&info1);

    // fork a child
    if ((pid = fork()) == 0) {
        // read the same heap and see if reads cause more page fault
        for (i = 0; i < size/4096; i++) {
            a[i * 4096];
        }
        info(&info2);
        // read should not increase the amount of memory allocated, leave 10 pages slack for 
        // cow stack values
        if (info2.num_pgfault - info1.num_pgfault > 10) {
            error("cow caused more page faults on reads, expected fault number is %d, actual is %d",
                info1.num_pgfault, info2.num_pgfault);
        }
        // now write to the heap
        for (i = 0; i < size/4096; i++) {
            a[i * 4096] = 'c';
        }
        info(&info3);
        // write should allocate the 200 pages of memory
        if (info3.num_pgfault-info2.num_pgfault < 200) {
            error("cow didn't result in more page faults on writes, expected fault number is at least %d, actual is %d",
                info2.num_pgfault + 200, info3.num_pgfault);
        }
        exit(0);
    } else {
        if ((ret = wait(pid, NULL)) != pid) {
            error("stack growth wait failed, return value was %d", ret);
        }
    }

    pass("cow-fork");
    exit(0);
    return 0;
}
