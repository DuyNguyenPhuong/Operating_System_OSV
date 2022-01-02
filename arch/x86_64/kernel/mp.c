#include <kernel/console.h>
#include <kernel/types.h>
#include <kernel/pmem.h>
#include <kernel/vpmap.h>
#include <kernel/vm.h>
#include <kernel/trap.h>
#include <kernel/arch.h>
#include <lib/errcode.h>
#include <lib/string.h>
#include <arch/mmu.h>
#include <arch/asm.h>
#include <arch/mp.h>
#include <arch/lapic.h>
#include <arch/ioapic.h>
#include <arch/cpu.h>

#define MP_SIGNATURE "_MP_"
#define MPCONFIG_SIGNATURE "PCMP"

#define MP_TYPE_PROC 0
#define MP_TYPE_BUS 1
#define MP_TYPE_IOAPIC 2
#define MP_TYPE_IOINT 3
#define MP_TYPE_LINT 4

#define IMCR_BIT 0x80
#define MP_PROC_BP_FLAG 0x02

struct mp {
    uint8_t signature[4];
    uint32_t phy_addr;
    uint8_t len;
    uint8_t spec_rev;
    uint8_t checksum;
    uint8_t config_type;
    uint8_t imcr;
    uint8_t rsvd[3];
};

struct mpconfig {
    uint8_t signature[4];
    uint16_t base_table_len;
    uint8_t spec_rev;
    uint8_t checksum;
    uint8_t product_oem_id[20];
    uint32_t oem_table_pointer;
    uint16_t oem_table_size;
    uint16_t entry_count;
    uint32_t lapic_addr;
    uint16_t ext_table_len;
    uint8_t ext_table_checksum;
    uint8_t rsvd;
};

struct mp_proc {
    uint8_t entry_type;
    uint8_t lapic_id;
    uint8_t lapic_ver;
    uint8_t cpu_flags;
    uint8_t cpu_signature[4];
    uint32_t feature_flags;
    uint8_t rsvd[8];
};

struct mp_ioapic {
    uint8_t entry_type;
    uint8_t ioapic_id;
    uint8_t ioapic_ver;
    uint8_t ioapic_flags;
    uint32_t mmap_addr;
};

struct mp_bus {
    uint8_t entry_type;
    uint8_t bus_id;
    uint8_t bus_type_str[6];
};

struct mp_ioint {
    uint8_t entry_type;
    uint8_t int_type;
    uint16_t ioint_flags;
    uint8_t src_bus_id;
    uint8_t src_bus_irq;
    uint8_t dst_ioapic_id;
    uint8_t dst_ioapic_intin;
};

struct mp_lint {
    uint8_t entry_type;
    uint8_t int_type;
    uint16_t lint_flags;
    uint8_t src_bus_id;
    uint8_t src_bus_irq;
    uint8_t dst_lapic_id;
    uint8_t dst_lapic_lintin;
};

extern void main_ap(void);

/*
 * x86 MP contains lapic, ioapic, and CPU configurations
 */
volatile uint32_t *lapic;
uint8_t ioapic_id;
int ncpu;
struct x86_64_cpu x86_64_cpus[MAX_NCPU];

/*
 * Calculate checksum
 */
static uint8_t
checksum(uint8_t *addr, size_t len)
{
    uint8_t sum, i;

    sum = 0;
    for (i = 0; i < len; i++) {
        sum += addr[i];
    }
    return sum;
}

/*
 * Search for the MP Floating Pointer structure
 * within the memory range [paddr, paddr+len]
 */
static struct mp*
_mp_search(paddr_t paddr, size_t len)
{
    uint8_t *ptr, *eptr;

    // Assume addr and addr + len are 16 bytes aligned
    for (ptr = (uint8_t*)KMAP_P2V(paddr), eptr = ptr + len;
        ptr + sizeof(struct mp) < eptr;
        ptr += sizeof(struct mp)) {
        if (memcmp(ptr, MP_SIGNATURE, strlen(MP_SIGNATURE)) == 0 && checksum(ptr, sizeof(struct mp)) == 0) {
            return (struct mp*)ptr;
        }
    }
    return 0;
}

/*
 * Search for the MP Floating Pointer structure
 * in the following order
 * 1. First KB of EBDA
 * 2. Last KB of base memory
 * 3. BIOS ROM address space between F0000 - FFFFF
 */
static struct mp*
mp_search()
{
    uint16_t *ebda, *base_mem;
    paddr_t paddr;
    struct mp *mp;

    ebda = (uint16_t*)KMAP_P2V(0x040E);
    if ((paddr = (paddr_t)(*ebda << 4))) {
        if ((mp = _mp_search(paddr, 1024))) {
            return mp;
        }
    } else {
        base_mem = (uint16_t*)KMAP_P2V(0x0413);
        if ((mp = _mp_search((paddr_t)((*base_mem - 1) * 1024), 1024))) {
            return mp;
        }
    }
    return _mp_search(0xF0000, 0x10000);
}

/* Parse the MP configuration table
 */
static void
process_mpconfig(struct mpconfig *config)
{
    uint8_t *ptr;
    uint16_t n_entries;
    struct mp_proc *proc;
    struct mp_ioapic *ioapic;

    if (memcmp(config, MPCONFIG_SIGNATURE, strlen(MPCONFIG_SIGNATURE)) != 0 ||
        checksum((uint8_t*)config, config->base_table_len) != 0 ||
        (config->spec_rev != 1 && config->spec_rev != 4)) {
        panic("mpconfig format error");
    }

    // IO mapped differetly
    lapic = (uint32_t*) kmap_io2v(config->lapic_addr);
    n_entries = config->entry_count;
    ptr = (uint8_t*)(config + 1);

    while (n_entries-- > 0) {
        switch (*ptr) {
        case MP_TYPE_PROC: {
            proc = (struct mp_proc*)ptr;
            if (ncpu < MAX_NCPU) {
                x86_64_cpus[ncpu++].lapic_id = proc->lapic_id;
            }
            ptr += sizeof(struct mp_proc);
            break;
        }
        case MP_TYPE_IOAPIC: {
            ioapic = (struct mp_ioapic*)ptr;
            ioapic_id = ioapic->ioapic_id;
            ptr += sizeof(struct mp_ioapic);
            break;
        }
        case MP_TYPE_BUS: {
            ptr += sizeof(struct mp_bus);
            break;
        }
        case MP_TYPE_IOINT: {
            ptr += sizeof(struct mp_ioint);
            break;
        }
        case MP_TYPE_LINT: {
            ptr += sizeof(struct mp_lint);
            break;
        }
        default:
            panic("unrecognized mp entry");
        }
    }
}

// this code is meant to be called only once before mp is enabled
void
mp_start_ap(void)
{
    extern uint8_t _binary_entry_ap_start[], _binary_entry_ap_size[];
    uint8_t *code;
    struct x86_64_cpu *c;
    paddr_t stack;

    // Write entry code to unused memory at 0x7000.
    // The linker has placed the image of entry_ap.S in
    // _binary_entry_ap_start.
    code = (uint8_t*) KMAP_P2V(0x7000);
    memmove(code, _binary_entry_ap_start, (size_t)_binary_entry_ap_size);
    for(c = x86_64_cpus; c < x86_64_cpus+ncpu; c++) {
        if(c == mycpu()) {
            // We've started already.
            continue;
        }

        // Tell entry_ap.S what stack to use, where to enter, and what
        // pgdir to use. We cannot use kernel pgdir yet, because the AP processor
        // is running in low memory, so we use kpml4_tmp for the APs too.
        if (pmem_alloc(&stack) != ERR_OK) {
            continue;
        }
        *(uint32_t*)(code-4) = KMAP_V2P(kpml4_tmp);
        *(vaddr_t*)(code-16) = KMAP_P2V(stack+4096);
        *(void**)(code-24) = main_ap;
        lapic_start_ap(c->lapic_id, KMAP_V2P(code));
        // wait for cpu to finish initializing
        while(c->started == 0) {
            ;
        }
    }
}


void
mp_init(void)
{
    struct mp *mp;
    struct mpconfig *config;

    if ((mp = mp_search()) == 0) {
        panic("Fail to locate mp");
    }
    if (mp->spec_rev != 1 && mp->spec_rev != 4) {
        panic("MP version number error");
    }
    if (mp->config_type != 0) {
        panic("Default MP configuration not supported");
    }
    if (mp->phy_addr == 0) {
        panic("MP config table does not exist");
    }
    config = (struct mpconfig*)KMAP_P2V(mp->phy_addr);
    process_mpconfig(config);

    if (mp->imcr & IMCR_BIT) {
        // Enable APIC
        outb(0x22, 0x70);
        outb(0x23, inb(0x23) | 0x1);
    }
}
