#include <lib/test.h>
#include <lib/string.h>

/* ASSUMES PROC_MAX_FILE is 128 */

int
main()
{
    int fds[128];
    int pipe_fds[2];
    char buf;
    int ret;

    // open 128 files (including stdin, stdout)
    for (int i = 2; i < 128; i++) {
        if ((fds[i] = open("/smallfile", FS_RDONLY, EMPTY_MODE)) != i) {
            error("pipe-errors: open() returned incorrect fd, should have been %d, but return value was %d\n", i, fds[i]);
        }
    }

    // try to create a pipe, but there shouldn't be any available fds
    if ((ret = pipe(pipe_fds)) != ERR_NOMEM) {
        error("pipe-errors: pipe() should have failed to find 2 file descriptors and returned ERR_NOMEM, return value was %d\n", ret);
    }

    if ((ret = close(fds[127])) != ERR_OK) {
        error("pipe-errors: failed to close file, return value was %d\n", ret);
    }
    
    // try to create a pipe, but there shouldn't be only one available fd
    if ((ret = pipe(pipe_fds)) != ERR_NOMEM) {
        error("pipe-errors: pipe() should have failed to find 2 file descriptors and returned ERR_NOMEM, return value was %d\n", ret);
    }

    if ((ret = close(fds[126])) != ERR_OK) {
        error("pipe-errors: failed to close file, return value was %d\n", ret);
    }

    // after closing a second file, pipe() should now succeed
    if ((ret = pipe(pipe_fds)) != ERR_OK) {
        error("pipe-errors: pipe() failed, return value was %d\n", ret);
    }

    if ((pipe_fds[0] == fds[126] && pipe_fds[1] == fds[127]) || (pipe_fds[0] == fds[127] && pipe_fds[1] == fds[126])) {
        // reading and writing from the wrong ends of the pipe should return 0 bytes read/written
        if ((ret = write(pipe_fds[0], &buf, 1)) != 0) {
            error("pipe-errors: non-zero return value when trying to write to the read end of a pipe, return value ewas %d\n", ret);
        }
        if ((ret = read(pipe_fds[1], &buf, 1)) != 0) {
            error("pipe-errors: non-zero return value when trying to read from the write end of a pipe, return value ewas %d\n", ret);
        }
    } else {
        error("pipe-errors: pipe() reused already open file descriptors %d and %d\n", pipe_fds[0], pipe_fds[1]);
    }


    pass("pipe-errors");
    exit(0);
}