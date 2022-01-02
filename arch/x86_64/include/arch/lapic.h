#ifndef _ARCH_X86_64_LAPIC_H_
#define _ARCH_X86_64_LAPIC_H_

#include <stdint.h>
#include <arch/types.h>

// LAPIC register memory
extern volatile uint32_t *lapic;

/*
 * LAPIC initialization.
 */
void lapic_init(void);

/*
 * LAPIC setup to start an AP.
 */
void lapic_start_ap(uint8_t apicid, paddr_t addr);

/*
 * Read the LAPIC ID of the current processor.
 */
uint8_t lapic_read_id(void);

void lapic_eoi(void);

#endif /* _ARCH_X86_64_LAPIC_H_ */
