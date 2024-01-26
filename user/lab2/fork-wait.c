#include <stddef.h>
#include <lib/test.h>

int main()
{
    int pid;

    // Fork one process
    pid = fork();
    if (pid < 0)
    {
        error("fork-wait: fork returned an error");
    }
    if (pid == 0)
    {
        printf("Child! (pid=%d)\n", pid);
        exit(getpid());
        error("fork-wait: exit failed to destroy process %d", getpid());
    }
    else
    {
        printf("Parent! (pid=%d, child=%d)\n", getpid(), pid);
        if (wait(pid, NULL) != pid)
        {
            error("fork-wait: wait failed to return pid");
        }
    }

    pass("fork-wait");
    exit(0);
    return 0;
}
