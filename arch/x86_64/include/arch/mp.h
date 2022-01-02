#ifndef _ARCH_X86_64_MP_H_
#define _ARCH_X86_64_MP_H_

/*
 * Multi-processor initialization.
 */
void mp_init(void);

/*
 * Start application processors.
 */
void mp_start_ap(void);

#endif /* _ARCH_X86_64_MP_H_ */
