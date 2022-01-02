#include <lib/test.h>
#include <lib/string.h>

int stdin = 0;
int stdout = 1;

int
main()
{
    int stdout_cpy, i, fd;
    char buf[100];

    // dup stdout to stdout_cpy, close stdout, and dup stdout_cpy 
    // if fds are reused, returned fd should be 1

    if ((stdout_cpy = dup(stdout)) < 0) {
        error("dup stdout failed, return value was '%d'", stdout_cpy);
    }
    
    if ((i = close(stdout)) != ERR_OK) {
        error("failed to close stdout, return value was %d", i);
    }

    if ((fd = dup(stdout_cpy)) != 1) {
        error("returned fd from dup that was not the smallest free fd, was '%d'", fd);    
    }
   
    char *consolestr = "print to console directly from write\n";
    strcpy(buf, consolestr);

    if ((i = write(stdout_cpy, consolestr, strlen(consolestr))) != strlen(consolestr)) {
        error("couldn't write to console from duped fd, wrote %d bytes", i);
    }
    assert(close(stdout_cpy) == ERR_OK);
    
    pass("dup-console");

    exit(0);
    return 0;
}
