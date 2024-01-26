#include <stddef.h>
#include <lib/test.h>

int
main()
{
    int pid;

    // Fork one process
    pid = fork();
    if (pid < 0) {
        error("fork-wait2: fork returned an error");
    }
    if (pid == 0) {
        printf("Child! (pid=%d)\n", pid);
        // make child slow so that parent will likely exit first
        for (int i = 0; i < 100000000; i++)
        {
        }
        exit(getpid());
        error("fork-wait2: exit failed to destroy process %d", getpid());
    }
    else {
        printf("Parent! (pid=%d, child=%d)\n", getpid(), pid);
        if (wait(pid, NULL) != pid) // wait on this pid
        {
            error("fork-wait2: wait failed to return pid");
        }
    }

    pass("fork-wait2");
    exit(0);
    return 0;
}
