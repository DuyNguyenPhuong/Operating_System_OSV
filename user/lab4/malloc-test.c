#include <lib/test.h>
#include <lib/stddef.h>
#include <lib/malloc.h>

int
main()
{
    void *m1, *m2;
    int i;

    // malloc 10 buffer each of size 10001
    // each buffer's beginning stores address of the previous buffer
    for (i = 0; i < 10; i++) {
        m2 = malloc(10001);
        *(char **)m2 = m1;
        m1 = m2;
    }

    // go through and free allocated buffers
    while (m1) {
        m2 = *(char **)m1;
        free(m1);
        m1 = m2;
    }
    
    // allocate more more buffers
    if ((m1 = malloc(1024 * 20)) == NULL) {
        error("failed to malloc");
    }
    free(m1);

    pass("malloc-test");
    exit(0);
    return 0;
}
