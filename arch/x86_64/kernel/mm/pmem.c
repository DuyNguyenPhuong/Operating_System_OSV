#include <kernel/console.h>
#include <kernel/pmem.h>
#include <kernel/vm.h>
#include <arch/mmu.h>
#include <arch/pmem.h>

struct e820_map e820_map;

vaddr_t kvm_base;
vaddr_t kmap_start;
vaddr_t kmap_end;

/*
 * Initialize E820 memory map.
 */
static void pmem_e820_init(void);

static void
pmem_e820_init(void)
{
    size_t i;
    struct e820_entry *entry, *end;

    entry = (struct e820_entry*)KMAP_P2V(E820_MAP_PADDR);
    end = (struct e820_entry*)KMAP_P2V(*(struct e820_entry**)KMAP_P2V(E820_MAP_PADDR_END));

    for (i = 0; entry <= end && i < MAX_E820_N_ENTRIES; i++, entry++) {
        e820_map.entries[i] = *entry;
    }
    e820_map.n_entries = i-1;
}

void
pmem_info(void)
{
    struct e820_entry *entry;
    kprintf("E820: physical memory map [mem %p-%p]\n", pmemconfig.pmem_start, pmemconfig.pmem_end);
    for (entry = e820_map.entries; entry < &e820_map.entries[e820_map.n_entries]; entry++) {
        paddr_t end = entry->base_addr + entry->len;
        kprintf(" [%p - %p] %s\n", entry->base_addr, end, e820_string[entry->type]);
    }
    kprintf("\n");
}

void
pmem_arch_init(void)
{
    struct e820_entry *entry;

    pmem_e820_init();

    // Physical pages for base memory, I/O space, and kernel image are pinned
    // and not available for kernel allocation. We only use contiguous memory
    // region above kernel image.
    pmemconfig.pmem_start = pg_round_up(KMAP_V2P(_end));

    // Find usable memory region in E820 map
    pmemconfig.pmem_end = 0;
    for (entry = e820_map.entries; entry < &e820_map.entries[e820_map.n_entries]; entry++) {
        if (entry->type == E820_TYPE_USABLE) {
            if (entry->base_addr + entry->len > KMAP_V2P(_end)) {
                kassert(entry->base_addr <= KMAP_V2P(_end));
                pmemconfig.pmem_end = pg_round_down(entry->base_addr + entry->len);
                break;
            }
        } 
    }
    kassert(pmemconfig.pmem_end != 0);
    kassert(pmemconfig.pmem_end < DEVMEM_BASE);

    // Kernel memory layout depends on physical memory size. Can initialize the
    // following attributes now.
    kvm_base = KMAP_BASE;
    kmap_start = KMAP_BASE;
    kmap_end = KMAP_BASE + pmemconfig.pmem_end;
}
