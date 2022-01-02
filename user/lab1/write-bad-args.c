#include <lib/test.h>

int
main()
{
    int fd, i;
    char buf[11];

    // open a valid file and try invalid read params
    if ((fd = open("/README", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("failed to open a valid file, return value was %d", fd);
    }

    // test bad fds for writes
    if ((i = write(15, buf, 11)) != ERR_INVAL || 
        (i = write(130, buf, 11)) != ERR_INVAL) {
        error("write on a non existent file descriptor, return value was %d", i);
    }

    // try writing to a read only file descriptor
    if ((i = write(fd, buf, 11)) > 0) {
        error("able to write to a file using a read only descriptor, return value was %d", i);
    }

    pass("write-bad-args");
    exit(0);
    return 0;
}