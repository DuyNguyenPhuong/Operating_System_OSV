#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int i, pid, ret;

    // spawn 100 children
    for (i = 0; i < 100; i++) {
        if ((pid = fork()) < 0) {
            error("racetest: fork failed, return value was %d", pid);
        }
        if (pid == 0) {
            exit(0);
            error("racetest: children failed to exit");
        }
    }

    // wait and reclaim all 100 children
    for (i = 0; i < 100; i++) {
        if ((ret = wait(-1, NULL)) < 0) {
            error("racetest: failed to collect children, return value was %d", ret);
        }
    }

    pass("race-test");
    exit(0);
    return 0;
}