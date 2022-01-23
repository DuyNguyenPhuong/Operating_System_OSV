#include <lib/test.h>

int
main()
{
    char buf[500];
    int fds[2], pid;
    int seq, i, n, cc, total, ret, status;

    // create a pipe
    if ((ret = pipe(fds)) != ERR_OK) {
        error("pipe-test: pipe() failed, return value was %d", ret);
    }

    // fork, children writes to pipe, parent reads
    seq = 0;
    if ((pid = fork()) == 0) {
        // children close read pipe
        if ((ret = close(fds[0])) != ERR_OK) {
            error("pipe-test: failed to close read pipe, return value was %d", ret);
        }
        // write to pipe 5 times
        for (n = 0; n < 5; n++) {
            // create buffer content
            for (i = 0; i < 95; i++) {
                buf[i] = seq++;
            }
            // write buffer to write end of pipe
            if ((ret = write(fds[1], buf, 95)) != 95) {
                error("pipe-test: failed to write all buffer content, return value was %d", ret);
            }
        }
        exit(getpid());
    } else if (pid > 0) {
        // parent close its write pipe
        if ((ret = close(fds[1])) != ERR_OK) {
            error("pipe-test: failed to close write pipe, return value was %d", ret);
        }
        total = 0;
        cc = 1;
        while ((n = read(fds[0], buf, cc)) > 0) {
            for (i = 0; i < n; i++) {
                if ((buf[i] & 0xff) != (seq++ & 0xff)) {
                    error("pipe-test: read wrong values from pipe\n");
                }
            }
            total += n;
            cc = cc * 2 > sizeof(buf) ? sizeof(buf) : cc * 2;
        }
        if (total != 5 * 95) {
            error("pipe-test: failed to read all bytes from pipe, read %d bytes", total);
        }
        if ((ret = close(fds[0])) != ERR_OK) {
            error("pipe-test: failed to close read pipe, return value was %d", ret);
        }
        if ((ret = wait(-1, &status)) < 0 || status != pid) {
            error("pipe-test: failed to wait for child or child exit status wrong, return value was %d", ret);
        }
    } else {
        error("pipe-test: fork() failed %d", pid);
    }
    pass("pipe-test");
    exit(0);
    return 0;
}