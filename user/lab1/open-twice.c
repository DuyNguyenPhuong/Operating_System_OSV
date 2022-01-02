#include <lib/test.h>

int
main()
{
    int fd, fd2;

    // test for open file twice
    if ((fd = open("/README", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("failed to create a new file");
    }

    // ensure opening the same file results with a
    // different file descriptor.
    if ((fd2 = open("/README", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("cannot open just created file on 2nd try");
    }
        
    if (fd == fd2) {
        error("opening same file results with same file descriptor");
    }

    close(fd);
    close(fd2);

    pass("open-twice");
    exit(0);
    return 0;
}