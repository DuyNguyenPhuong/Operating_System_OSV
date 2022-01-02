#ifndef _IO_H_
#define _IO_H_

/*
 * Machine dependent device I/O interface.
 */

#include <kernel/types.h>

/*
 * Read a single byte from the device.
 */
uint8_t readb(port_t port);

/*
 * Write n bytes into buffer at addr.
 */
void readn(port_t port, void *addr, size_t n);

/*
 * Write a single byte into the device.
 */
void writeb(port_t port, uint8_t data);

/*
 * Write n bytes from buffer to the device.
 */
void writen(port_t port, const void *addr, size_t n);

#endif /* _IO_H_ */
