#include <lib/test.h>

int
main()
{
    int n, status, ret;
    int nproc = 6;
    int pid, mypid;

    // fork nproc number of process and exit them with their pids as exit status
    for (n = 0; n < nproc; n++) {
        pid = fork();
        if (pid < 0) {
            break;
        }
        if (pid == 0) {
            exit(getpid());
            error("fork-test: exit failed to destroy process %d", getpid());
        }
    }

    if (n != nproc) {
        error("fork-test: in a loop calling fork %d times, fork returned an error after %n calls", nproc, n);
    }

    // wait to reclaim all children, make sure their exit status has greater pid than parent
    mypid = getpid();
    for (; n > 0; n--) {
        if ((ret = wait(-1, &status)) < 0 || status < mypid) {
            error("fork-test: wait failed or got wrong status, return value was %d", ret);
        }
    }

    // try waiting for more children, should fail
    if ((ret = wait(-1, &status)) != ERR_CHILD) {
        error("fork-test: calling wait after all children were already waited on did not return ERR_CHILD, return value was %d", ret);
    }
    pass("fork-test");
    exit(0);
    return 0;
}
