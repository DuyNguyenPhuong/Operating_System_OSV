#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int i;
    char *a, *b;

    // check sbrk() with bytes less than a page?
    // extend heap by one byte for 5000 times
    a = sbrk(0);
    for (i = 0; i < 5000; i++) {
        if ((b = sbrk(1)) != a) {
            error("sbrk test failed %d %x %x\n", i, a, b);
        }
        // write to newly extended heap region
        *b = 1;
        a = b + 1;
    }
    pass("sbrk-small");
    exit(0);
    return 0;
}