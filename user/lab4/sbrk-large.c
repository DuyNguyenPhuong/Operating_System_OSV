#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int pid, ret;
    char *a, *b, *lastaddr, *p;
    size_t amt;

    // try growing heap by 256 pages
    a = sbrk(0);
    amt = 256 * 4096;
    if ((p = sbrk(amt)) != a) {
        error("sbrk test failed to grow big address space or did not return old bound, return value was %p", p);
    }
    lastaddr = (char *)(a + amt - 1);
    *lastaddr = 99;

    // fork a child and make sure heap is inherited/copied over
    if ((pid = fork()) < 0) {
        error("sbrk test fork failed, return value was %d", pid);
    }
    // child grow heap by two pages and exit
    if (pid == 0) {
        b = sbrk(4096);
        b = sbrk(4096);
        if (b != a + amt + 4096) {
            error("sbrk test failed post-fork, return value was %d", b);
        }
        exit(0);
    }
    // parent waits
    if ((ret = wait(pid, NULL)) != pid) {
        error("sbrk test wait failed, return value was %d", ret);
    }

    pass("sbrk-large");
    exit(0);
    return 0;
}