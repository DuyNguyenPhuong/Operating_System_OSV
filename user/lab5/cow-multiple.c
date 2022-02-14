#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    int proc_count = 0;
    int pid, ret;
    struct sys_info info_before, info_after;

    // allocate a chunk of stack memory
    char a[4096 * 9];
    a[4096 * 4] = '!';

    while (proc_count < 5) {
        if ((pid = fork()) == 0) {
            info(&info_before);
            // reading should not cause additional faults
            proc_count++;
            char c = a[4096 * 4];
            write(1, &c, 1);
            info(&info_after);
            if (info_after.num_pgfault != info_before.num_pgfault) { 
                error("cow-multiple: reading caused a fault");
            }
        } else {
            if (pid == ERR_NOMEM) {
                error("cow-multiple: fork failed, return value was %d", pid);
            }
            if ((ret = wait(-1, NULL)) != pid) {
                error("cow-multiple: wait returned incorrect pid, return value was %d, should have been %d", ret, pid);
            }
            break;
        }
    }
    info(&info_before);
    a[4096 * 4] = '?';
    info(&info_after);
    if (info_after.num_pgfault != info_before.num_pgfault + 1) { 
        error("cow-multiple: writing did not cause a fault or caused too many, expected fault number is %d, actual is %d", info_before.num_pgfault + 1, info_after.num_pgfault);
    }

    if (proc_count == 0) {
        pass("cow-multiple");
    }
    exit(0);
}