#ifndef _TIMER_H_
#define _TIMER_H_

#include <kernel/types.h>

/*
 * Register timer trap handler. Return ERR_TRAP_REG_FAIL if failed to register.
 */
err_t timer_register_trap_handler(void);

#endif /* _TIMER_H_ */
