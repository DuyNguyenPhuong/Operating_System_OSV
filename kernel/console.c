#include <kernel/console.h>
#include <kernel/types.h>
#include <kernel/trap.h>
#include <kernel/synch.h>
#include <lib/stdarg.h>
#include <kernel/cga.h>
#include <kernel/uart.h>
#include <kernel/keyboard.h>

static void console_putc(int c);
static void printnum(uint32_t num, int base, int sign);

#define BUF_LEN 256
struct {
    char buf[BUF_LEN];
    uint32_t r;  // Read index
    uint32_t w;  // Write index
    uint32_t e;  // Edit index
} input;

static char digits[] = "0123456789ABCDEF";
struct spinlock _console_lock; // lock protecting console buffer
struct spinlock *console_lock = &_console_lock; 
struct condvar read_cv;

// function definitions
static void console_putc(int c);
static void printnum(uint32_t num, int base, int sign);
static ssize_t stdin_read(struct file *file, void *buf, size_t count, offset_t *ofs);
static ssize_t stdout_write(struct file *file, const void *buf, size_t count, offset_t *ofs);

static struct file_operations stdin_ops = {
    .read = stdin_read,
};

static struct file_operations stdout_ops = {
    .write = stdout_write,
};

static void
console_putc(int c)
{
    if (c == BACKSPACE) {
        uart_putc('\b');
        uart_putc(' ');
        uart_putc('\b');
    } else {
        uart_putc(c);
    }
    cga_putc(c);
}

#ifdef X64
static void
printnum64(uint64_t num, int base, int sign)
{
    char buf[32];
    int i = 0;
    if (sign && (sign = (int64_t)num < 0)) {
        num = (uint64_t)(-(int64_t)num);
    }
    
    do {
        buf[i++] = digits[num % base];
    } while((num /= base) != 0);

    if(sign) {
        console_putc('-');
    }

    if (base == 16) {
        console_putc('0');
        console_putc('x');
    }

    while(--i >= 0) {
        console_putc(buf[i]);
    }
}
#endif

static void
printnum(uint32_t num, int base, int sign)
{
    char buf[16];
    int i = 0;

    if (sign && (sign = (int32_t)num < 0)) {
        num = (uint32_t)(-(int32_t)num);
    }

    do {
        buf[i++] = digits[num % base];
    } while ((num /= base) > 0);

    if (sign) {
        console_putc('-');
    }

    if (base == 16) {
        console_putc('0');
        console_putc('x');
    }

    while (--i >= 0) {
        console_putc(buf[i]);
    }
}    // initialize console buffer

void
console_init(void)
{
    spinlock_init(console_lock);
    condvar_init(&read_cv);
    cga_init();
    uart_init();
    keyboard_init();

    // Initialize stdin and stdout file struct
    sleeplock_init(&stdin.f_lock);
    sleeplock_init(&stdout.f_lock);
    stdin.oflag = FS_RDONLY;
    stdout.oflag = FS_WRONLY;
    stdin.f_ops = &stdin_ops;
    stdout.f_ops = &stdout_ops;
}

void
console_storec(char c)
{
    spinlock_acquire(console_lock);
    // put into console buffer then output
    switch(c) {
        case C('U'):  // Kill line.
            while(input.e != input.w &&
                input.buf[(input.e-1) % BUF_LEN] != '\n') {
                input.e--;
                console_putc(BACKSPACE);
            }
            break;
        case C('H'): case '\x7f':  // Backspace
            if(input.e != input.w){
                input.e--;
                console_putc(BACKSPACE);
            }
            break;
        default:
            if(c != 0 && input.e-input.r < BUF_LEN) {
                c = (c == '\r') ? '\n' : c;
                input.buf[input.e++ % BUF_LEN] = c;
                if(c == '\n' || c == C('D') || input.e == input.r+BUF_LEN) {
                    input.w = input.e;
                }
                console_putc(c);
                condvar_signal(&read_cv);
            }
    }
    spinlock_release(console_lock);
}

// keep a buffer in console
int
console_read(char* buf, int size)
{
    int n = size;
    spinlock_acquire(console_lock);
    // read from console buffer, return read size
    while(n > 0) {
        while(input.r == input.w){
            condvar_wait(&read_cv, console_lock);
        }
        char c = input.buf[input.r++ % BUF_LEN];
        if(c == C('D')){  // EOF
            if(n < size){
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                input.r--;
            }
            break;
        }
        *buf++ = c;
        --n;
        if(c == '\n') {
            break;
        }
    }
    spinlock_release(console_lock);
    return size-n;
}

int
console_write(const char* buf, int size)
{
    int i = 0;
    while (i < size) {
        console_putc(buf[i]);
        i++;
    }
    return size;
}

void
panic(const char *format)
{
    kprintf("PANIC: %s\n", format);
    for (;;);
}

/*
   Currently only support %c, %d, %u, %x, %p, and %s.
 */
void
kprintf(const char *format, ...)
{
    char *sptr;
    va_list valist;
    va_start(valist, format);

    if (format == 0) {
        panic("kprintf: null format\n");
    }
    // arg_addr = (vaddr_t)&format + sizeof(char*);
    // args gets past in from something else
    for (; *format != 0; format++) {
        if (*format == '%') {
            format++;
            if (*format == 0) {
                break;
            }
            switch (*format) {
                case 'c':
                    console_putc(va_arg(valist, int));
                    break;
                case 'd':
                    printnum(va_arg(valist, int32_t), 10, 1);
                    break;
                case 'u':
                    printnum(va_arg(valist, uint32_t), 10, 0);
                    break;
                case 'x':
                case 'p':
                #ifdef X64
                    printnum64(va_arg(valist, uint64_t), 16, 0);
                #else
                    printnum(va_arg(valist, uint32_t), 16, 0);
                #endif
                    break;
                case 's':
                    sptr = (char*) va_arg(valist, char *);
                    if (sptr == 0) {
                        sptr = "(null)";
                    }
                    for (; *sptr != 0; sptr++) {
                        console_putc(*sptr);
                    }
                    break;
                case '%':
                    console_putc('%');
                    break;
                default:
                    // Should really be a compiler error
                    console_putc('%');
                    console_putc(*format);
                    break;
            }
        } else {
            console_putc(*format);
        }
    }
    va_end(valist);
}

static ssize_t
stdin_read(struct file *file, void *buf, size_t count, offset_t *ofs)
{
    return console_read(buf, (int) count);
}

static ssize_t
stdout_write(struct file *file, const void *buf, size_t count, offset_t *ofs)
{
    return console_write((const char*)buf, (int)count);
}