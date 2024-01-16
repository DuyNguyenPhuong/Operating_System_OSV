#include <lib/test.h>
#include <lib/string.h>

int
main()
{
    int fd1, fd2, i;
    char buf[100];

    // test dup with invalid arguments
    if ((i = dup(15)) != ERR_INVAL || (i = dup(130)) != ERR_INVAL) {
        error("able to duplicated a non open file, return value was %d", i);
    }

    // Test smallest next descriptor
    if ((fd1 = open("/smallfile", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("unable to open small file, return value was %d", fd1);
    }

    if ((fd2 = dup(fd1)) != fd1 + 1) {
        error("returned fd from dup was not the smallest free fd, was '%d'", fd2);
    }

    // test offsets are respected in dupped files
    // read 10 bytes from the original fd
    assert(read(fd1, buf, 10) == 10);
    buf[10] = 0;

    if (strcmp(buf, "aaaaaaaaaa") != 0) {
        error("couldn't read from original fd after dup");
    }
    // read the next 10 bytes from the dupped fd
    if ((i = read(fd2, buf, 10)) != 10) {
        error("coudn't read 10 byytes from the dupped fd, read %d bytes", i);
    }
    buf[10] = 0;

    if (strcmp(buf, "aaaaaaaaaa") == 0) {
        error("the duped fd didn't respect the read offset from the other file.");
    }

    if (strcmp(buf, "bbbbbbbbbb") != 0) {
        error("the duped fd didn't read the correct 10 bytes at the 10 byte offset");
    }

    if ((i = close(fd1)) != ERR_OK) {
        error("closing the original file, return value was %d", i);
    }

    if ((i = read(fd2, buf, 5)) != 5) {
        error("wasn't able to read last 5 bytes from the duped file after the original file"
                "was closed, read %d bytes", i);
    }

    buf[5] = 0;
    assert(strcmp(buf, "ccccc") == 0);

    if ((i = close(fd2)) != ERR_OK) {
        error("closing the duped file, return value was %d", i);
    }

    pass( "dup-read");
    exit(0);
    return 0;
}
