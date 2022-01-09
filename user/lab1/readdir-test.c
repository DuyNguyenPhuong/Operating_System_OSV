#include <lib/test.h>
#include <lib/string.h>

int
main()
{
    int fd, fd_f, i;
    struct dirent dir;

    // Test read only funcionality
    if ((fd = open("/", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("unable to open root, return value was %d", fd);
    }

    // test bad fds for reads
    if ((i = readdir(15, &dir)) != ERR_INVAL ||
        (i = readdir(130, &dir)) != ERR_INVAL) {
        error("readdir on a non existent file descriptor, return value was %d", i);
    }

    // try reading into a bad dirent
    if ((i = readdir(fd, (struct dirent *)0xffffff00)) != ERR_FAULT) {
        error("able to readdir to a dirent not in the process's memory region, return value was %d", i);
    }

    // try reading a file
    if ((fd_f = open("/README", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("unable to open README, return value was %d", fd);
    }

    if ((i = readdir(fd_f, &dir)) != ERR_FTYPE) {
        error("able to readdir a file, return value was %d", i);
    }

    if ((i = close(fd_f)) != ERR_OK) {
        error("error closing README, return value was %d", i);
    }

    // try normal reading
    if ((i = readdir(fd, &dir)) != ERR_OK) {
        error("readdir of first directory entry unsuccessful, return value was %d", i);
    }

    if (strcmp(dir.name, "README") != 0) {
        error("entry was not README, was: '%s'", dir.name);
    }

    if ((i = readdir(fd, &dir)) != ERR_OK) {
        error("readdir of second directory entry unsuccessful, return value was %d", i);
    }

    if (strcmp(dir.name, "largefile") != 0) {
        error("entry was not largefile, was: '%s'", dir.name);
    }

    if ((i = readdir(fd, &dir)) != ERR_OK) {
        error("readdir of third directory entry unsuccessful, return value was %d", i);
    }

    if (strcmp(dir.name, "smallfile") != 0) {
        error("entry was not smallfile, was: '%s'", dir.name);
    }

    // try reading to the end of the directory
    int c = 3;
    while ((i = readdir(fd, &dir)) == ERR_OK) {
        c++;
    }

    if (i != ERR_END) {
        error("readdir unable to reach directory end, return value was %d after %d files", i, c);
    }

    if ((i = close(fd)) != ERR_OK) {
        error("error closing fd, return value was %d", i);
    }

    pass("readdir-test");
    exit(0);
    return 0;
}
