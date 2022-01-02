#include <kernel/vm.h>
#include <kernel/console.h>
#include <arch/mmu.h>
#include <arch/vm.h>
#include <arch/cpu.h>
#include <arch/asm.h>

size_t pg_size = PG_SIZE;

void
seg_init(void)
{
    struct x86_64_cpu *cpu;
    cpu = mycpu();
    uint32_t *tss = (uint32_t*)&cpu->ts;
    tss[16] = 0x00680000; // IO Map Base = End of TSS

    *(uint64_t*) &cpu->gdt[0] = 0; // NULL segment
    cpu->gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0, 0, 1, DPL_KERNEL);
    cpu->gdt[SEG_KDATA] = SEG(STA_W, 0, 0, 0, DPL_KERNEL);
    cpu->gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0, 0, 1, DPL_USER);
    cpu->gdt[SEG_UDATA] = SEG(STA_W, 0, 0, 0, DPL_USER);
    cpu->gdt[SEG_TSS] = SEGTSS(STS_T64A, (uint64_t)&cpu->ts, sizeof(cpu->ts), DPL_KERNEL);
    *(uint64_t*) &cpu->gdt[SEG_TSS_UPPER] = ((uint64_t)&cpu->ts) >> 32;
    lgdt(cpu->gdt, sizeof(cpu->gdt));
    ltr(SEG_TSS << 3);
}
