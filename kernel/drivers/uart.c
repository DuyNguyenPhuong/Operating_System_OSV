#include <lib/stddef.h>
#include <kernel/uart.h>
#include <kernel/console.h>
#include <kernel/io.h>
#include <kernel/trap.h>
#include <lib/errcode.h>
#include <arch/trap.h>

#define COM1    0x3F8
#define THB     COM1+0x0    // Transmitter Holding Buffer
#define RBR     COM1+0x0    // Receiver Buffer
#define DL_LO   COM1+0x0    // Divisor Latch Low Byte
#define DL_HI   COM1+0x1    // Divisor Latch High Byte
#define IER     COM1+0x1    // Interrupt Enable Register
#define IIR     COM1+0x2    // Interrupt Identification Register
#define FCR     COM1+0x2    // FIFO Control Register
#define LCR     COM1+0x3    // Line Control Register
#define MCR     COM1+0x4    // Modem Control Register
#define LSR     COM1+0x5    // Line Status Register

static int uart_initialized = 0;

void uart_input_handler(irq_t irq, void *dev, void *regs);

void
uart_init(void)
{
    // Disable FIFO
    writeb(FCR, 0x00);
    // Set Baud Rate to 9600
    writeb(LCR, 0x80);
    writeb(DL_HI, 0x00);
    writeb(DL_LO, 0x0C);
    // Set 8 bit word, 1 stop bit, no parity (also turn off DLA)
    writeb(LCR, 0x03);
    // No hardware flow control
    writeb(MCR, 0x00);
    // Enable interrupt
    writeb(IER, 0x01);

    uart_initialized = 1;

    kassert(trap_register_handler(T_IRQ_COM1, NULL, uart_input_handler) == ERR_OK);
    kassert(trap_enable_irq(T_IRQ_COM1) == ERR_OK);
    uart_putc('\n'); // Start output on a new line
}

void
uart_putc(char c)
{
    int i;

    if (!uart_initialized) {
        return;
    }

    for (i = 0; i < 128 && !(readb(LSR) & 0x20); i++) {
        // Wait until THB is available
    }
    writeb(THB, c);
}

// also need to store it somewhere, this should be invoked on COM1 trap
// need unifying buffer
char
uart_getc(void)
{
    if(!uart_initialized)
        return -1;
    if(!(readb(LSR) & 0x01))
        return -1;
    return readb(RBR);
}

void
uart_input_handler(irq_t irq, void *dev, void *regs)
{
    console_storec(uart_getc());
    trap_notify_irq_completion();
}
