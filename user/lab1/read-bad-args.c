#include <lib/test.h>

int 
main()
{
    int fd, i;
    char buf[11];

    // test bad fds for reads
    if ((i = read(15, buf, 11)) != ERR_INVAL || 
        (i = read(130, buf, 11)) != ERR_INVAL) {
        error("read on a non existent file descriptor, return value was %d", i);
    }

    // open a valid file and try invalid read params
    if ((fd = open("/README", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("failed to open a valid file, return value was %d", fd);
    }   

    // try reading negative number of bytes
    if ((i = read(fd, buf, -100)) >= 0) {
        error("negative n didn't return error, return value was %d", i);
    }

    // try reading into a bad buffer
    if ((i = read(fd, (char *)0xffffff00, 10)) != ERR_FAULT) {
        error("able to read to a buffer not in the process's memory region, return value was %d", i);
    }

    pass("read-bad-args");
    exit(0);
    return 0;
}