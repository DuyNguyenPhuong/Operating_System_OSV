#ifndef _ARCH_X86_64_IOAPIC_H_
#define _ARCH_X86_64_IOAPIC_H_

#include <kernel/types.h>

extern uint8_t ioapic_id;

void ioapic_init(void);

void ioapic_enable(irq_t irq, uint8_t cpu_id);

void ioapic_disable(irq_t irq);

#endif /* _ARCH_X86_64_IOAPIC_H_ */
