#ifndef _ARCH_X86_64_PMEM_H_
#define _ARCH_X86_64_PMEM_H_

/*
 * Architecture specific physical memory data structures and functions.
 */

/*
 * E820 memory map
 */
#define E820_MAP_PADDR 0x9000
#define E820_MAP_PADDR_END 0x8FFC

#ifndef __ASSEMBLER__

#include <kernel/types.h>

#define E820_TYPE_USABLE 1
#define E820_TYPE_RESERVED 2
#define E820_TYPE_RECLAIM 3
#define E820_TYPE_NVS 4
#define E820_TYPE_BAD 5

char* e820_string[] = {
    [E820_TYPE_USABLE] = "usable",
    [E820_TYPE_RESERVED] = "reserved",
    [E820_TYPE_RECLAIM] = "reclaimed",
    [E820_TYPE_NVS] = "nvs",
    [E820_TYPE_BAD] = "bad",
};

struct e820_entry {
    uint64_t base_addr;
    uint64_t len;
    uint32_t type;
} __attribute__ ((packed));

#define MAX_E820_N_ENTRIES 64

struct e820_map {
    size_t n_entries;
    struct e820_entry entries[MAX_E820_N_ENTRIES];
};

extern struct e820_map e820_map;

#endif /* __ASSEMBER__ */

#endif /* _ARCH_X86_64_PMEM_H_ */
