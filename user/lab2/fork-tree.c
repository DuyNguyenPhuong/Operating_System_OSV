#include <lib/test.h>
#include <lib/stddef.h>

static void
childpidtesthelper(int depth)
{
    int pids[3];
    int i, ret;

    // each call to childpidtesthelper with depth > 0, creates exactly 3 direct
    // children, total number of children created will be 3^(depth_og, depth_og-1...)
    // where depth_og is the depth of the first call
    if (depth == 0) {
        return;
    }

    for (i = 0; i < 3; i++) {
        if ((pids[i] = fork()) < 0) {
            error("fork() failed, return value was %d", pids[i]);
        }
        if (pids[i] == 0) {
            childpidtesthelper(depth - 1);
            exit(0);
        }
    }

    // each parent waits for the 3 direct children spawned
    for (i = 0; i < 3; i++) {
        if ((ret = wait(pids[i], NULL)) != pids[i]) {
            error("failed to wait for children, return value was %d", ret);
        }
    }

    // Should have no more children
    if ((ret = wait(-1, NULL)) != ERR_CHILD) {
        error("there should be no more children, return value was %d", ret);
    }
}

int
main()
{
    childpidtesthelper(2);

    pass("fork-tree");
    exit(0);
    return 0;
}
