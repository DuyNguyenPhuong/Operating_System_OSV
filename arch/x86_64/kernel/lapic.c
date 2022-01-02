#include <kernel/console.h>
#include <arch/types.h>
#include <arch/lapic.h>
#include <arch/trap.h>
#include <arch/asm.h>

/*
 * LAPIC registers
 */
#define REG_ID      0x020/4
#define REG_VER     0x030/4
#define REG_TPR     0x080/4
#define REG_EOI     0x0B0/4
#define REG_SVR     0x0F0/4
#define REG_ESR     0x280/4
#define REG_ICR_LO  0x300/4
#define REG_ICR_HI  0x310/4
#define REG_TIMER   0x320/4
#define REG_THERMAL 0x330/4
#define REG_PMC     0x340/4
#define REG_LINT0   0x350/4
#define REG_LINT1   0x360/4
#define REG_ERROR   0x370/4
#define REG_ICR     0x380/4
#define REG_CCR     0x390/4
#define REG_DCR     0x3E0/4
// SIV fields
#define LAPIC_EN    0x00000100
// TIMER related fields
#define PERIODIC    0x00020000
#define DIV_1       0x0000000B
#define TIMER_INTVL 10000000
// LVT fields
#define MASK        0x00010000
// IPI fields
#define IPI_INIT    0x00000500
#define IPI_STARTUP 0x00000600
#define IPI_ASSERT  0x00004000
#define IPI_LEVEL   0x00008000
#define IPI_BCAST   0x00080000
#define IPI_DELIVER 0x00001000
// CMOS
#define CMOS_PORT   0x70
#define CMOS_RETURN 0x71

static void
lapic_reg_write(int reg, uint32_t value)
{
    lapic[reg] = value;
    // Waiting for write to finish, by reading
    // from the register
    lapic[reg];
}

void
lapic_init(void)
{
    // Enable LAPIC
    lapic_reg_write(REG_SVR, LAPIC_EN | T_IRQ_SPURIOUS);

    // Disable local interrupt LINT0 and LINT1p
    lapic_reg_write(REG_LINT0, MASK);
    lapic_reg_write(REG_LINT1, MASK);

    // Disable performance counter register. Performance
    // counter register is only available on P6, Pentium 4
    // and Xeon.
    if (((lapic[REG_VER] >> 16) & 0xFF) >= 4) {
        lapic_reg_write(REG_PMC, MASK);
    }

    // Disable thermal monitor register. Thermal monitor
    // register is only available on Pentium 4 and Xeon.
    if (((lapic[REG_VER] >> 16) & 0xFF) >= 5) {
        lapic_reg_write(REG_THERMAL, MASK);
    }

    // Initialize APIC timer interrupt.
    lapic_reg_write(REG_DCR, DIV_1);
    lapic_reg_write(REG_ICR, TIMER_INTVL);
    lapic_reg_write(REG_TIMER, PERIODIC | T_IRQ_TIMER);

    // Initialize error interrupt.
    lapic_reg_write(REG_ERROR, T_IRQ_ERROR);

    // Signal completion of outstanding interrupts.
    lapic_reg_write(REG_EOI, 0);

    // Set arbitration ID to APIC ID for all processors, by broadcasting an
    // INIT level de-assert IPI. Only P6 and Pentium processors require this step.
    if (((lapic[REG_VER] >> 16) & 0xFF) <= 4) {
        lapic_reg_write(REG_ICR_LO, IPI_INIT | IPI_LEVEL | IPI_BCAST);
        // Wait until sent
        while (lapic[REG_ICR_LO] & IPI_DELIVER) {
        }
    }

    // Clear error status register. After the first write, ESR
    // may contain errors from the previous write, so need to
    // write a second time.
    lapic_reg_write(REG_ESR, 0);
    lapic_reg_write(REG_ESR, 0);

    // Set task priority class to 0 to enable interrupts
    lapic_reg_write(REG_TPR, 0);
}

// Start additional processor running entry code at addr.
// See Appendix B of MultiProcessor Specification.
void
lapic_start_ap(uint8_t apicid, paddr_t addr)
{
    int i;
    uint16_t *wrv;

    // "The BSP must initialize CMOS shutdown code to 0AH
    // and the warm reset vector (DWORD based at 40:67) to point at
    // the AP startup code prior to the [universal startup algorithm]."
    outb(CMOS_PORT, 0xF);  // offset 0xF is shutdown code
    outb(CMOS_PORT+1, 0x0A);
    wrv = (uint16_t*)KMAP_P2V((0x40<<4 | 0x67));  // Warm reset vector
    wrv[0] = 0;
    wrv[1] = addr >> 4;

    // "Universal startup algorithm."
    // Send INIT (level-triggered) interrupt to reset other CPU.
    lapic_reg_write(REG_ICR_HI, apicid<<24);
    lapic_reg_write(REG_ICR_LO, IPI_INIT | IPI_LEVEL | IPI_ASSERT);
    for (i = 0; i<100000; i++) { }
    lapic_reg_write(REG_ICR_LO, IPI_INIT | IPI_LEVEL);
    for (i = 0; i<100000; i++) { }

    // Send startup IPI (twice!) to enter code.
    // Regular hardware is supposed to only accept a STARTUP
    // when it is in the halted state due to an INIT.  So the second
    // should be ignored, but it is part of the official Intel algorithm.
    // Bochs complains about the second one.  Too bad for Bochs.
    for(i = 0; i < 2; i++) {
        lapic_reg_write(REG_ICR_HI, apicid<<24);
        lapic_reg_write(REG_ICR_LO, IPI_STARTUP | (addr>>12));
        for (i = 0; i<100000; i++) { }
    }
}

uint8_t
lapic_read_id(void)
{
    return (uint8_t)(lapic[REG_ID] >> 24);
}

void
lapic_eoi(void)
{
    lapic_reg_write(REG_EOI, 0);
}
