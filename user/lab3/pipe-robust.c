#include <lib/test.h>
#include <lib/stddef.h>
#include <lib/string.h>

int
main()
{
    int pid, ret, i;
    int fds[2];
    char *buf = "pipetestwrite";

    if ((ret = pipe(fds)) != ERR_OK) {
        error("pipe-robust: pipe() failed, return value was %d", ret);
    }
    if ((pid = fork()) == 0) {
        // close read end
        if ((ret = close(fds[0])) != ERR_OK) {
            error("pipe-robust: failed to close read pipe, return value was %d", ret);
        }
        // child writes to pipe
        for (i=0; i<10000000; i++) {}
        if ((ret = write(fds[1], buf, 14)) != 14) {
            error("pipe-robust: failed to write all data to pipe, wrote %d bytes", ret);
        }
        exit(0);
    } else if (pid > 0) {
        // parent closes write end pipe 
        if ((ret = close(fds[1])) != ERR_OK) {
            error("pipe-robust: failed to close write pipe, return value was %d", ret);
        }
        if ((ret = wait(-1, NULL)) != pid) {
            error("pipe-robust: wait() failed, return value was %d", ret);
        }
        // read data from pipe into buf2
        int bytes_to_read = 14;
        int n = 0;
        char buf2[14];
        while( bytes_to_read > 0) {
            if ((ret = read(fds[0], buf2 + n, bytes_to_read)) <= 0) {
                error("pipe-robust: read() failed, read %d bytes", ret);
            }
            n += ret;
            bytes_to_read -= ret;
        }
        // making sure there's no more data to read
        if ((ret = read(fds[0], buf2, 1)) != 0) {
            error("pipe-robust: read() returns non-zero at EOF, return value was %d", ret);
        }
        if (strcmp(buf, buf2) != 0) {
            error("pipe-robust: output test failed, read %s instead", buf2);
        }
        if ((ret = close(fds[0])) != ERR_OK) {
            error("pipe-robust: failed to close read end, return value was %d", ret);
        }
    } else {
        error("pipe-robust: fork() failed, return value was %d", pid);
    }

    // open a pipe, make sure it's writable first, then close read end and tries to write again
    if ((ret = pipe(fds)) != ERR_OK) {
        error("pipe-robust: pipe() failed, return value was %d", ret);
    }
    if ((ret = write(fds[1], buf, 14) != 14)) {
        error("pipe-robust: write should fail, return value was %d", ret);
    }
    if ((ret = close(fds[0])) != ERR_OK) {
        error("pipe-robust: failed to close read end, return value was %d", ret);
    }
    if ((ret = write(fds[1], buf, 14) != ERR_END)) {
        error("pipe-robust: not returning ERR_END when read-end is closed, return value was %d", ret);
    }
    if ((ret = close(fds[1])) != ERR_OK) {
        error("pipe-robust: failed to close write end, return value was %d", ret);
    }
    pass("pipe-robust");
    exit(0);
    return 0;
}
