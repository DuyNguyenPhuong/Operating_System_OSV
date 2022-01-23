#include <lib/test.h>

int
main()
{
    char c;
    int fds[2], pid;
    int i, total, ret, status, read_dup, write_dup;

    // create a pipe
    if ((ret = pipe(fds)) != ERR_OK) {
        error("pipe-race: pipe() failed, return value was %d\n", ret);
    }

    total = 0;
    for (i = 0; i < 100; i++) {
        if (i % 2 == 0) {
            if ((read_dup = dup(fds[0])) < 0) {
                error("pipe-race: dup failed, return value was %d\n", read_dup);
            }
            if ((pid = fork()) == 0) {
                if ((ret = read(read_dup, &c, 1)) != 1) {
                    error("pipe-race: read failed to read exactly one byte, return value was %d\n", ret);
                }
                exit(c);
            }
        } else {
            total += i;
            if ((write_dup = dup(fds[1])) < 0) {
                error("pipe-race: dup failed, return value was %d\n", write_dup);
            }
            if ((pid = fork()) == 0) {
                c = i;
                if ((ret = write(write_dup, &c, 1)) != 1) {
                    error("pipe-race: write failed to write exactly one byte, return value was %d\n", ret);
                }
                exit(0);
            }
        }
    }
    close(fds[0]);
    close(fds[1]);

    for (i = 0; i < 100; i++) {
        if ((ret = wait(-1, &status)) < 0) {
            error("pipe-race: failed to wait for child %d, return value was %d\n", i, ret);
        }
        total -= status;
    }
    if (total != 0) {
        error("pipe-race: values read by children and gathered via wait/exit do not match values written, difference is %d\n", total);
    }

    pass("pipe-race");
    exit(0);
}