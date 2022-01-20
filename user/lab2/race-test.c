#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int i, pid_original, pid, ret;

    if ((pid_original = fork()) < 0) {
        error("race-test: fork failed, return value was %d", pid_original);
    }
    // both child and parent fork 1000 children
    for (i = 0; i < 1000; i++) {
        if ((pid = fork()) < 0) {
            error("race-test: fork failed, return value was %d", pid);
        }
        if (pid == 0) {
            exit(0);
            error("race-test: children failed to exit");
        }
    }

    // both child and parent wait and reclaim all 1000 children
    for (i = 0; i < 1000; i++) {
        if ((ret = wait(-1, NULL)) < 0) {
            error("race-test: failed to collect children, return value was %d", ret);
        }
    }

    // parent waits for the original child
    if (pid_original) {
        if ((ret = wait(-1, NULL)) < 0) {
            error("race-test: failed to collect children, return value was %d", ret);
        }
        pass("race-test");
        exit(0);
    }

    // original child exits
    exit(0);
}
