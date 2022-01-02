#include <stdint.h>
#include <lib/stdio.h>
#include <lib/usyscall.h>
#include <lib/stdarg.h>

static char digits[] = "0123456789ABCDEF";

int
puts(char *buf, int size)
{
    return write(1, buf, size);
}

char*
gets(char *buf, int max)
{
    int i, cc;
    char c;

    for(i = 0; i+1 < max; ) {
        cc = read(0, &c, 1);
        if(cc < 1) {
            break;
        }
        buf[i++] = c;
        if(c == '\n' || c == '\r') {
            break;
        }
    }
    buf[i] = '\0';
    return buf;
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
        write(1, "-", 1);
    }

    if(base == 16) {
        write(1, "0x", 2);
    }

    while(--i >= 0) {
        write(1, buf+i, 1);
    }
}
#endif

static void
printnum(uint32_t num, int base, int sign)
{
    char buf[16];
    int i;

    if (sign && (sign = (int32_t)num < 0)) {
        num = (uint32_t)(-(int32_t)num);
    }

    i = 0;
    do {
        buf[i++] = digits[num % base];
    } while ((num /= base) > 0);

    if (sign) {
        write(1, "-", 1);
    }

    if(base == 16) {
        write(1, "0x", 2);
    }

    while (--i >= 0) {
        write(1, buf+i, 1);
    }
}

void printf(const char *format, ...)
{
    char *sptr;
    va_list valist;
    va_start(valist, format);

    if (format == 0) {
        puts("hi", 3);
        return;
    }

    for (; *format != 0; format++) {
        if (*format == '%') {
            format++;
            if (*format == 0) {
                break;
            }
            switch (*format) {
                case 'c': {
                    // char c = (char) va_arg(valist, int);
                    // write(1, &c, 1);
                    break;
                }
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
                        write(1, sptr, 1);
                    }
                    break;
                case '%':
                    write(1, format, 1);
                    break;
                default:
                    // Should really be a compiler error
                    write(1, format, 1);
                    break;
            }
        } else {
            write(1, format, 1);
        }
    }
}
