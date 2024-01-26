#include <lib/test.h>

int
main()
{
    int pid;

    // Fork one process
    pid = fork();
    if (pid < 0) {
        error("fork-simple: fork returned an error");
    }

    // Just print out that we're here
    if (pid == 0) {
        printf("\tChild! (pid=%d)\n", pid);
    }
    else {
        printf("\tParent! (pid=%d, child=%d)\n", getpid(), pid);
    }

    pass("fork-simple");
    exit(pid+1);
    return 0;
}