#ifndef _TEST_H_
#define _TEST_H_

#include <lib/usyscall.h>
#include <lib/stdio.h>

// Will print an error message and exit
#define error(msg, ...)                                                    \
    do {                                                                   \
        printf(ANSI_COLOR_RED "ERROR: " ANSI_COLOR_RESET);                 \
        printf("(line %d)", __LINE__);                                     \
        printf(msg, ##__VA_ARGS__);                                        \
        printf("\n");                                                      \
        exit(-1);                                                          \
    } while (0)

// After error functionality is tested, this is just a convenience
#define assert(a)                                                          \
    do {                                                                   \
        if (!(a)) {                                                        \
            printf("Assertion failed (line %d): %s\n", __LINE__, #a);      \
            exit(-1);                                                      \
        }                                                                  \
    } while (0)

// Print passing message in green woohoo
#define pass(msg, ...)                                                     \
    do {                                                                   \
        printf(ANSI_COLOR_GREEN "passed " ANSI_COLOR_RESET);               \
        printf(msg, ##__VA_ARGS__);                                        \
        printf( "\n");                                                     \
    } while (0)

#endif /* _TEST_H_ */
