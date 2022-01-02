#include <stdint.h>
#include <lib/usyscall.h>
#include <lib/string.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/malloc.h>

#define MAXLINE 256
#define MAXARGS 64

static char prompt[] = "$ ";

void eval(char *cmdline);
int builtin_cmd(char **argv);
void parseline(const char *cmdline, char **argv);
void malloc_test(void);

int
main(int argc, char **argv)
{
    char cmdline[MAXLINE];
    while (1) {
        puts(prompt, 3);
        gets(cmdline, MAXLINE);
        eval(cmdline);
    }
    exit(0);
    return 0;
}

/*
 * eval - Evaluate the command line that the user has just typed in
 */
void
eval(char *cmdline)
{
    char *argv[MAXARGS];

    parseline(cmdline, argv);  // parse command line input
    if (argv[0] == NULL) {
        return;  // ignore empty lines
    }
    /* check if the cmd is builtin execute it immediately,
     * else fork a child process and lets the child process run the job */
    if (!builtin_cmd(argv)) {
        int pid, status;
        // replace newline character with end of string
        char *c = strchr(cmdline, '\n');
        if (c != NULL) {
            *c = '\0';
        }
        // shell waits for the foreground job to terminate
        if ((pid = spawn(cmdline)) > 0) {
            wait(pid, &status);
            printf("foregroud job %s exited with status %d\n", cmdline, status);
        }
    }
    return;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 * Return 1 if a builtin command was executed; return 0
 * if the argument passed in is *not* a builtin command.
 */
int
builtin_cmd(char **argv)
{
    if(!strcmp(argv[0], "quit")) {
        halt();
    }
    if(!strcmp(argv[0], "testing")) {
        puts("testing command\n", 17);
        return 1;
    }
    if(!strcmp(argv[0], "open")) {
        int fd;
        if ((fd = open(argv[1], FS_RDONLY, 0)) < 1) {
            printf("open failed: %d\n", fd);
        } else {
            printf("open succeeded: %d\n", fd);
        }
        return 1;
    }
    if(!strcmp(argv[0], "meminfo")) {
        meminfo();
        return 1;
    }
    if(!strcmp(argv[0], "malloc")) {
        malloc_test();
        return 1;
    }
    return 0;  // not a builtin command
}

/***********************
 * Helper routines
 ***********************/

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
void
parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    /* ignore leading spaces */
    while (*buf && (*buf == ' ')) {
        buf++;
    }

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    }
    else {
	    delim = strchr(buf, ' ');
    }

    while (delim) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) {
            buf++;
        }

        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        }
        else {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;
}

void
malloc_test(void)
{
    printf("starting malloc test\n");
    void *fail_buf, *buf, *buf2;
    fail_buf = malloc(-1);
    if (fail_buf != NULL) {
        free(fail_buf);
        printf("malloc(-1) succeeded\n");
        return;
    }
    buf = malloc(0);
    if (buf == NULL) {
        printf("malloc(0) failed\n");
        return;
    }
    buf2 = malloc(512*1024);
    if (buf2 == NULL) {
        printf("malloc(512K) failed\n");
        return;
    }
    free(buf2);
    free(buf);
    printf("malloc test passed\n");
}
