#ifndef _ELF64_H_
#define _ELF64_H_

#include <stdint.h>
#include <arch/types.h>

#define ELF_MAGIC 0x464C457FU
#define PT_LOAD 0x1

// Flag bits for Proghdr flags
#define PF_W 2

struct elfhdr {
    uint32_t magic;
    uint8_t elf[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct proghdr {
    uint32_t type;
    uint32_t flags;
    uint64_t off;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

#endif /* _ELF64_H_ */
