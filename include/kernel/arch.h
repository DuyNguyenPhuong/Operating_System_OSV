#ifndef _ARCH_H_
#define _ARCH_H_

/*
 * Architecture specific initialization for bootstrap processor.
 */
void arch_init(void);

/*
 * Architecture specific initialization for application processors.
 */
void arch_init_ap(void);

#endif /* _ARCH_H_ */
