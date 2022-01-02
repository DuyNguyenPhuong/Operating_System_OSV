#include <lib/test.h>
#include <lib/stddef.h>

int
main()
{
    // spawn arg checking
    int pid, ret, status;

    // test for invalid argv
    if ((pid = spawn((char*) 0xf00df00d)) != ERR_FAULT) {
        error("spawnargs: invalid args, return value was %d", pid);
        exit(0);
    }

    // first spawn echo without arguments, should work with skeleton code
    if ((pid = spawn("echo")) < 0) {
        error("spawn failed, return value was %d", pid);
    }

    if ((ret = wait(pid, NULL)) != pid) {
        error("failed to wait for children, return value was %d", ret);
    }

    // then spawn eacho with arguments
    if ((pid = spawn("echo h e l l o")) < 0) {
        error("spawn with arguments failed, return value was %d", pid);
    }

    if ((ret = wait(pid, &status)) != pid || status != 6) {
        error("failed to wait for children or return value wrong, return value was %d", ret);
    }

    pass("spawn-args");
    exit(0);
    return 0;
}
