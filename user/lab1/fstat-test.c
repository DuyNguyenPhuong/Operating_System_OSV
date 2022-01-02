#include <lib/test.h>

int
main()
{
    int fd, i;
    struct stat st;

    // open a valid file
    if ((fd = open("/README", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("failed to open a valid file, return value was %d", fd);
    }

    // test non file fd for fstat
    if ((i = fstat(15, &st)) != ERR_INVAL || 
        (i = fstat(130, &st)) != ERR_INVAL) {
        error("able to fstat on a non existent file descriptor, return value was %d", i);
    }

    if ((i = fstat(0, &st)) != ERR_INVAL || 
        (i = fstat(1, &st)) != ERR_INVAL) {
        error("able to fstat on console file descriptor, return value was %d", i);
    }

    if ((i = fstat(fd, (struct stat *)0x65439876)) != ERR_FAULT) {
        error("able to fstat with invalid struct stat address, return value was %d", i);
    }

    // fstat functionality test 
    if ((i = fstat(fd, &st)) != ERR_OK) {
        error("couldn't stat on '/README', return value was %d", i);
    }

    assert(st.ftype == FTYPE_FILE);
    assert(st.size == 150);

    pass("fstat-test");
    exit(0);
    return 0;
}
