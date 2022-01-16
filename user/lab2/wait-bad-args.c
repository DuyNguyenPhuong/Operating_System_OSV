#include <lib/test.h>
#include <lib/stddef.h>

int main()
{
    int ret, pid;
    pid = fork();

    if (pid == 0) {
        exit(0);
        error("wait-bad-args: exit failed to destroy child process");
    }

    // wait on impossible pid
    if ((ret = wait(-100, NULL)) != ERR_CHILD)
    {
        error("wait-bad-args: able to wait for pid -100, return value was %d", ret);
    }

    // wait with bad status arg
    if ((ret = wait(pid, (int*)-1)) != ERR_FAULT)
    {
        error("wait-bad-args: wait did not return ERR_FAULT when passed a bad status arg, return value was %d", ret);
    }

    if ((ret = wait(-1, NULL)) != pid)
    {
        error("wait-bad-args: wait(-1, NULL) did not return child pid %d, return value was %d", pid, ret);
    }

    pass("wait-bad-args");
    exit(0);
    return 0;
}
