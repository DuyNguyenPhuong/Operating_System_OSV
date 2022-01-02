#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    char *oldbrk, *p, *a, *newbrk;

    // first increment sbrk
    oldbrk = sbrk(4096 * 10 + 10);
    // retrieve current sbrk
    a = sbrk(0);
    if (a > oldbrk) {
        if ((p = sbrk(-(a- oldbrk))) != a) {
            error("sbrk negative number doesn't return prev break, return value was %p", p);
        }
        if ((p = sbrk(0)) == oldbrk) {
            printf("Good job! sbrk decrement implemented!\n");
        }
        if ((p = sbrk(-1)) != oldbrk) {
            error("sbrk decrement more than allocated should do nothing, return value was %p", p);
        }
        sbrk(10);
        newbrk = sbrk(0);
        if ((p = sbrk(-100)) != newbrk) {
            error("sbrk decrement more than allocated should do nothing, return value was %p", p);
        }
        sbrk(-10);
        if ((p = sbrk(0)) != oldbrk) {
            error("not releasing all heap memory %p", p);
        }
    }
    pass("sbrk-decrement");
    exit(0);
    return 0;
}