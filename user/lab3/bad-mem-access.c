#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int pid, ret;
    char *a;

    for (a = (char *)(KMAP_BASE); a < (char *)(KMAP_BASE + 2000000); a += 50000) {
        if ((pid = fork()) < 0) {
            error("fork failed, return value was %d", pid);
        }
        if (pid == 0) {
            printf("oops could read %x = %x\n", a, *a);
            error("a bad process is not killed!");
        }
        if ((ret = wait(pid, NULL)) != pid) {
            error("child kernel address access test wait failed, return value was %d", ret);
        }
    }
    pass("bad-mem-access");
    exit(0);
    return 0;
}