#include <lib/test.h>
#include <lib/string.h>

int main()
{
    int n;
    int nproc = 6;
    int pid, fd;

    // fork nproc number of process and exit them with their pids as exit status
    assert((fd = open("testfile", FS_RDONLY, EMPTY_MODE)) > 0);
    for (n = 0; n < nproc; n++)
    {
        pid = fork();
        if (pid < 0)
        {
            break;
        }
        if (pid == 0)
        {
            // make children slow so that parent will likely exit first
            for (int i = 0; i < 10000000; i++)
            {
            }
            // ensure that all six children can correctly continue and exit after parent has exited
            char buf[10];
            read(fd, buf, 1);
            if (strcmp("f", buf) == 0)
            {
                pass("exit-test");
            }
            exit(getpid());
            error("exit-test: exit failed to destroy process %d", getpid());
        }
    }

    if (n != nproc)
    {
        error("exit-test: in a loop calling fork %d times, fork returned an error after %n calls", nproc, n);
    }
    close(fd);
    exit(0);
    return 0;
}
