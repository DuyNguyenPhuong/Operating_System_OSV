#ifndef _CGA_H_
#define _CGA_H_

/*
 * CGA video driver.
 */

/*
 * Initialize CGA.
 */
void cga_init(void);

/*
 * Print a single character ``c``.
 */
void cga_putc(int c);

#endif /* _CGA_H_ */
