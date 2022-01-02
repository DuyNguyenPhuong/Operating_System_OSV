#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int pid, ret;
    // create a child process, try allocate 11 pages of stack
    // stack limit should be 10 pages
    if ((pid = fork()) == 0) {
        char *buf = (char *) USTACK_UPPERBOUND - 11 * 4096;
        *buf = 0;
        error("we can get 11 pages on stack");
    } else if (pid < 0) {
        error("fork() failed, return value was %d", pid);
    } else {
        if ((ret = wait(pid, NULL)) != pid) {
            error("stack growth wait failed, return value was %d", ret);
        }
    }
    // try grow stack upwards 
    if ((pid = fork()) == 0) {
        char *buf = (char *) USTACK_UPPERBOUND;
        // can the stack grow upwards?
        *buf = 0;
        error("stack can grow upwards!");
    } else if (pid < 0) {
        error("fork() failed!\n");
    } else {
        if ((ret = wait(pid, NULL)) != pid) {
            error("stack growth wait failed, return value was %d", ret);
        }
    }
    pass("grow-stack-edgecase");
    exit(0);
    return 0;
}