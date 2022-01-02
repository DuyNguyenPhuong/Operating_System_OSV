#include <lib/test.h>

int
main()
{
    int fd, tmpfd, badfd;

    assert((fd = open("/smallfile", FS_RDONLY, EMPTY_MODE)) >= 0);

    for (tmpfd = fd + 1; tmpfd < NUM_FILES; tmpfd++) {
        int newfd = open("/smallfile", FS_RDONLY, EMPTY_MODE);
        if (newfd != tmpfd) {
            error("returned fd from open was not the smallest free fd, was '%d'",
                newfd);
        }
    }

    // Opening should fail as we have reached the NOFILE limit
    if ((badfd = open("/smallfile", FS_RDONLY, EMPTY_MODE)) != ERR_NOMEM) {
        error("opened more files than allowed, returned fd %d", badfd);
    }

    // Opening should work once there is a fd available
    assert(close(NUM_FILES - 1) == ERR_OK);
    int fd2 = open("/smallfile", FS_RDONLY, EMPTY_MODE);
    if (fd2 < 0) {
        error("unable to open file after an fd is available, return value was %d", fd2);
    }

    assert(fd2 == NUM_FILES - 1);
    for (tmpfd = fd; tmpfd < NUM_FILES; tmpfd++) {
        assert(close(tmpfd) == ERR_OK);
    }

    pass( "fd-limit");
    exit(0);
    return 0;
}