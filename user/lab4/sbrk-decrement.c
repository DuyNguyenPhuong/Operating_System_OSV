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
        if ((p = sbrk(-(a - oldbrk))) != a) {
            error("sbrk negative number doesn't return prev break, return value was %p", p);
        }
        if ((p = sbrk(0)) == oldbrk) {
            printf("Good job! sbrk decrement implemented!\n");
        }
        sbrk(-1);
        if ((p = sbrk(0)) != oldbrk) {
            error("sbrk decrement more than allocated should do nothing, return value was %p", p);
        }
        sbrk(10);
        newbrk = sbrk(0);
        sbrk(-100);
        if ((p = sbrk(0)) != newbrk) {
            error("sbrk decrement more than allocated should do nothing, return value was %p", p);
        }
        sbrk(-10);
        if ((p = sbrk(0)) != oldbrk) {
            error("not releasing all heap memory %p", p);
        }
    } else {
        error("Either first sbrk call did not grow the heap or subsequent sbrk(0) did not return new, larger bound, return value was %p", a);
    }
    pass("sbrk-decrement");
    exit(0);
    return 0;
}