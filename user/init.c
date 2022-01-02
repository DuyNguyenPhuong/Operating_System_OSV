#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>

int main(int argc, char** argv) {
    int pid = spawn("sh");
    if (pid < 0) {
        printf("init process fails to spawn children\n");
    }
    while (1) {
        // just waits for all processes to terminate
        wait(-1, NULL); 
    }
    exit(1);
    return 0;
}
