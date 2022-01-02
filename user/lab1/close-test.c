#include <lib/test.h>

int
main()
{
    int i, fd;
    
    if ((i = close(15)) != ERR_INVAL || (i = close(130)) != ERR_INVAL) {
         error("able to close non open file, return value was %d", i);
    }

    // open a valid file and try invalid read params
    if ((fd = open("/README", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("failed to open a valid file, return value was %d", fd);
    }

    // try close the same fd twice
    if ((i = close(fd)) != ERR_OK && (i = close(fd)) != ERR_INVAL) {
         error("able to close same file twice, return value was %d", i);
    }

    pass("close-test");

    exit(0);
    return 0;
}