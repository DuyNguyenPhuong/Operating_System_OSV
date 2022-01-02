#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int nproc = 6;
    int pids[nproc];
    int n, ret;

    // fork nproc number of process
    for (n = 0; n < nproc; n++) {
        pids[n] = fork();
        if (pids[n] < 0) {
            break;
        }
        if (pids[n] == 0) {
            exit(0);
            error("wait-twice: exit failed to destroy this process");
        }
    }

    if (n != nproc) {
        error("wait-twice: fork claimed to work %d times! but only %d", nproc, n);
    }

    // try waiting for each child twice
    for (; n > 0; n--) {
        if ((ret = wait(pids[n-1], NULL)) != pids[n-1] ) {
            error("wait-twice: wait failed or got wrong status, return value was %d", ret);
        }
        // wait for the same child again
        if ((ret = wait(pids[n-1], NULL)) != ERR_CHILD) {
            error("wait-twice: able to wait for the same child twice, return value was %d", ret);
        }
    }

    pass("wait-twice");
    exit(0);
    return 0;
}
