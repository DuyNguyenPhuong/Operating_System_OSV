#include <kernel/console.h>
#include <arch/ioapic.h>
#include <arch/trap.h>
#include <arch/mmu.h>

#define IOAPIC_ADDR 0xFEC00000

// IOAPIC registers
#define IOAPIC_REG_ID   0x00
#define IOAPIC_REG_VER  0x01
#define IOAPIC_RD_TABLE 0x10
// Redirection table entry fields
#define INT_MASK   0x00010000

static volatile struct ioapic *ioapic;

struct ioapic {
    uint32_t reg;
    uint32_t pad[3];
    uint32_t data;
};

static uint32_t
ioapic_read_reg(uint32_t reg)
{
    ioapic->reg = reg;
    return ioapic->data;
}

static void
ioapic_write_reg(uint32_t reg, uint32_t data)
{
    ioapic->reg = reg;
    ioapic->data = data;
}

void
ioapic_init(void)
{
    uint32_t max_entry, i;

    ioapic = (volatile struct ioapic*)  KMAP_IO2V(IOAPIC_ADDR);
    if ((ioapic_read_reg(IOAPIC_REG_ID) >> 24) != ioapic_id) {
        panic("Mismatched ioapic id");
    }
    max_entry = (ioapic_read_reg(IOAPIC_REG_VER) >> 16) & 0xFF;

    // Disable all interrupts
    for (i = 0; i < max_entry; i++) {
        ioapic_write_reg(IOAPIC_RD_TABLE+2*i, INT_MASK | (T_IRQ0 + i));
        ioapic_write_reg(IOAPIC_RD_TABLE+2*i+1, 0);
    }
}

void
ioapic_enable(irq_t irq, uint8_t cpu_id)
{
    ioapic_write_reg(IOAPIC_RD_TABLE+2*(irq-T_IRQ0), irq);
    ioapic_write_reg(IOAPIC_RD_TABLE+2*(irq-T_IRQ0)+1, cpu_id << 24);
}

void
ioapic_disable(irq_t irq)
{
    ioapic_write_reg(IOAPIC_RD_TABLE+2*(irq-T_IRQ0), INT_MASK | irq);
}
