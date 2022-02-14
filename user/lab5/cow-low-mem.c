#include <lib/test.h>
#include <lib/stddef.h>

/**
 * This test assumes qemu is run with 4 MB of physical memory 
 * (use make qemu-low-mem)
 */

int
main()
{
    int proc_count = 0;
    int pid, ret;

    while (proc_count < 35) {
        if ((pid = fork()) == 0) {
            proc_count++;
        } else {
            if (pid == ERR_NOMEM) {
                error("cow-low-mem: fork failed, return value was %d", pid);
            }
            if ((ret = wait(-1, NULL)) != pid) {
                error("cow-low-mem: wait returned incorrect pid, return value was %d, should have been %d", ret, pid);
            }
            break;
        }
    }

    if (proc_count == 0) {
        pass("cow-low-mem");
    }
    exit(0);
}