#ifndef _CONSOLE_H_
#define _CONSOLE_H_
#include <kernel/fs.h>

#define kassert(expr) \
        ((expr) ? (void)0 : panic(#expr))

#define BACKSPACE 0x100

void panic(const char *format) __attribute__((noreturn));
void kprintf(const char *format, ...);

// define stdin and stdout file structs
struct file stdin;
struct file stdout;

/* initialize console devices and buffer */
void console_init(void);

/* write a ``buf`` of length ``size`` to console */
int console_write(const char* buf, int size);

/* read console buffer of length ``size`` into ``buf`` */
int console_read(char* buf, int size);

/* write char c in console buffer, overwrite the stale entry if buffer is full */
void console_storec(char c);


#endif /* _CONSOLE_H_ */
