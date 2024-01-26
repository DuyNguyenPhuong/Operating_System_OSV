#include <lib/test.h>

int
main()
{
    int pid;

    // Fork one process
    pid = fork();
    if (pid < 0) {
        error("fork-exit: fork returned an error");
    }
    if (pid == 0) {
        printf("Child! (pid=%d)\n", pid);
        exit(getpid());
        error("fork-exit: exit failed to destroy process %d", getpid());
    }
    else {
        printf("Parent! (pid=%d, child=%d)\n", getpid(), pid);
    }

    pass("fork-exit");
    exit(0);
    return 0;
}
