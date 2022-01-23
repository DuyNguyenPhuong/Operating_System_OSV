#include <lib/test.h>
#include <lib/string.h>

int
main()
{
    char buf[2];
    int fds[2];
    int ret;

    // create a pipe
    if ((ret = pipe(fds)) != ERR_OK) {
        error("pipe-basic: pipe() failed, return value was %d", ret);
    }

    // write a byte to the pipe
    if ((ret = write(fds[1], "!", 1)) != 1) {
        error("pipe-basic: failed to write all buffer content, return value was %d", ret);
    }

    // read a byte from the pipe
    if ((read(fds[0], buf, 1)) != 1) {
        error("pipe-basic: failed to read byte written to pipe, return value was %d", ret);
    }
    buf[1] = 0; // add null terminator

    // check that correct byte was read
    if (strcmp(buf, "!") != 0) {
        error("pipe-basic: failed to read correct byte from pipe, read %s", buf);
    }

    pass("pipe-basic");
    exit(0);
}