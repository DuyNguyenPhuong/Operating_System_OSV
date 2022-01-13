#include <lib/test.h>
#include <lib/stddef.h>
#include <lib/string.h>

int
main()
{
    int fd, pid, ret, i;
    int fds[4];
    char buf[11] = {0};

    assert((fd = open("smallfile", FS_RDONLY, EMPTY_MODE)) > 0);

    // create a child to read from the same descriptor and make sure file position
    // is shared by both parent and child

    if ((pid = fork()) == 0) {
        // child read 10 bytes
        if ((ret = read(fd, buf, 10)) != 10) {
            error("fork-fd: tried to read 10 bytes from fd, read %d bytes", ret);
        }
        if (strcmp("aaaaaaaaaa", buf) != 0) {
            error("fork-fd: should have read 10 'a's, instead: '%s'", buf);
        }
        if ((ret = close(fd)) != ERR_OK) {
            error("fork-fd: failed to close the fd inherited, return value was %d", ret);
        }
        exit(0);
        error("fork-fd: failed to exit");
    } else {
        if ((ret = wait(pid, NULL)) != pid) {
            error("fork-fd: failed to wait for child, return value was %d", ret);
        }
    }

    // parent reads the next 10 byte
   if ((ret = read(fd, buf, 10)) != 10) {
        error("fork-fd: tried to read 10 bytes from fd, read %d bytes", ret);
    }
    if (strcmp("bbbbbbbbbb", buf)) {
        error("fork-fd: should have read 10 b's, instead: '%s'", buf);
    }
    if ((ret = close(fd)) != ERR_OK) {
        error("fork-fd: failed to close fd, return values was %d", ret);
    }

    // Open 4 new file descriptors, close 2 in the middle, fork and make sure
    // closed file descriptors are no longer readable in the children
    for (i = 0; i < 4; i++) {
        assert((fds[i] = open("smallfile", FS_RDONLY, EMPTY_MODE)) > 0); 
    }

    assert(close(fds[1]) == ERR_OK);
    assert(close(fds[2]) == ERR_OK);

    if ((pid = fork()) == 0) {
        assert(read(fds[0], buf, 10) == 10);
        assert(read(fds[1], buf, 10) == ERR_INVAL); // this fd shouldn't be open
        assert(read(fds[2], buf, 10) == ERR_INVAL); // this fd shouldn't be open
        assert(read(fds[3], buf, 10) == 10);
        // close all opened files before exit
        assert(close(fds[0]) == ERR_OK);
        assert(close(fds[3]) == ERR_OK);
        exit(0);
    } else {
        if ((ret = wait(pid, NULL)) != pid) {
            error("fork-fd: failed to wait for child, return value was %d", ret);
        }
        assert(close(fds[0]) == ERR_OK);
        assert(close(fds[3]) == ERR_OK);
    }
    pass("fork-fd");
    exit(0);
    return 0;
}
