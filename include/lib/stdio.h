#ifndef _STDIO_H_
#define _STDIO_H_

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

char* gets(char *buf, int max);
int puts(char *buf, int size);
void printf(const char *format, ...);

#endif /* _STDIO_H_ */
