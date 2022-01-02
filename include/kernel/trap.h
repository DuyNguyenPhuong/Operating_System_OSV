#ifndef _TRAP_H_
#define _TRAP_H_

/*
 * Interrupt and exception handling interface
 */

#include <kernel/types.h>

/*
 * Error codes
 */
#define ERR_TRAP_REG_FAIL 1
#define ERR_TRAP_NOT_FOUND 2

/*
 * Trap handler function.
 * irq: IRQ number.
 * dev: device information, as registered in the trap_register function.
 * regs: machine-dependent register states when trap occurred.
 */
typedef void trap_handler(irq_t irq, void *dev, void *regs);

/*
 * Initialize the trap subsystem.
 */
void trap_sys_init(void);

/*
 * Register a trap handler.
 * irq: IRQ number.
 * dev: device information, NULL if not required.
 * handler: trap handler function.
 * Return:
 * ERR_TRAP_REG_FAIL if fail to register IRQ
 */
err_t trap_register_handler(irq_t irq, void *dev, trap_handler *handler);

/*
 * Unregister a trap handler.
 * irq: IRQ number.
 * Return:
 * ERR_TRAP_NOT_FOUND if IRQ was not registered before
 */
err_t trap_unregister_handler(irq_t irq);

/*
 * Invoke a trap handler.
 * irq: IRQ number
 * regs: machine-dependent register states
 * Return:
 * ERR_TRAP_NOT_FOUND if IRQ was not registered before
 */
err_t trap_invoke_handler(irq_t irq, void *regs);

/*
 * Machine-dependent function to enable an IRQ
 */
err_t trap_enable_irq(irq_t irq);

/*
 * Machine-dependent function to disable an IRQ
 */
err_t trap_disable_irq(irq_t irq);

/*
 * Machine-dependent function to notify completion of IRQ handling
 */
void trap_notify_irq_completion(void);

/*
 * Interrupt status
 */
typedef enum intr_level {
    INTR_OFF,             /* Interrupts disabled. */
    INTR_ON               /* Interrupts enabled. */
}intr_t;

/*
 * Machine-dependent function to return the current interrupt status.
 */
intr_t intr_get_level(void);

/*
 * Machine-dependent function to set interrupt level to the specified level and
 * return the previous interrupt level. The function handles nested INTR_ONs and
 * INTR_OFFs properly.
 */
intr_t intr_set_level(intr_t level);

#endif /* _TRAP_H_ */
