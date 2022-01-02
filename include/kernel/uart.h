#ifndef _UART_H_
#define _UART_H_

/*
 * UART serial driver.
 */

/*
 * Initialize UART
 */
void uart_init(void);

/*
 * Print a single character ``c``.
 */
void uart_putc(char c);

/*
 * Get a single character from serial port.
 */
char uart_getc(void);

#endif /* _UART_H_ */
