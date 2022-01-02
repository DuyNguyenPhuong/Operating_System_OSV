#include <lib/test.h>

int
main()
{
    int fd;

    // test open with invalid flags
    if ((fd = open("/REAMDME", 666, EMPTY_MODE)) != ERR_INVAL || 
        (fd = open("/REAMDME", -1, EMPTY_MODE)) != ERR_INVAL) {
        if (fd > 0) {
            error("opened a file with invalid flags or wrong err");
        } else {
            error("returned wrong error code for invalid flags, return value was %d", fd);
        }
    }

    // test open with nonexistent file
    if ((fd = open("/doesnotexist1.txt", FS_RDONLY, EMPTY_MODE)) != ERR_NOTEXIST ||
        (fd = open("/doesnotexist2.txt", FS_RDWR, EMPTY_MODE)) != ERR_NOTEXIST) {
        if (fd > 0) {
            error("opened a file that doesn't exist");
        } else {
            error("returned wrong error code for nonexistent file, return value was %d", fd);
        }
    }

    // test open with invalid path
    if ((fd = open((char*)0xffff0000, FS_RDONLY, EMPTY_MODE)) != ERR_FAULT) {
        if (fd > 0) {
            error("opened a file with invalid pathname");
        } else {
            error("returned wrong error code for invalid path, return value was %d", fd);
        }
    }

    pass("open-bad-args");
    exit(0);
    return 0;
}