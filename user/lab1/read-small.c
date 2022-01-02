#include <lib/test.h>
#include <lib/string.h>

int
main()
{
    int fd, i;
    char buf[11];

    // Test read only funcionality
    if ((fd = open("/smallfile", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("unable to open small file, return value was %d", fd);
    }

    if ((i = read(fd, buf, 10)) != 10) {
        error("read of first 10 bytes unsucessful was %d bytes", i);
    }

    buf[10] = 0;
    if (strcmp(buf, "aaaaaaaaaa") != 0) {
        error("buf was not 10 a's, was: '%s'", buf);
    }

    if ((i = read(fd, buf, 10)) != 10) {
        error("read of second 10 bytes unsucessful was %d bytes", i);
    }

    buf[10] = 0;
    if (strcmp(buf, "bbbbbbbbbb") != 0) {
        error("buf was not 10 b's, was: '%s'", buf);
    }

    // only 25 byte file
    if ((i = read(fd, buf, 10)) != 6) {
        error("read of last 6 bytes unsucessful was %d bytes", i);
    }

    buf[6] = 0;
    if (strcmp(buf, "ccccc\n") != 0) {
        error("buf was not 5 c's (and a newline), was: '%s'", buf);
    }

    if (read(fd, buf, 10) != 0) {
        error("read more bytes than should be possible, was %d bytes", i);
    }

    if ((i = close(fd)) != ERR_OK) {
        error("error closing fd, return value was %d", i);
    }

    pass("read-small");
    exit(0);
    return 0;
}
