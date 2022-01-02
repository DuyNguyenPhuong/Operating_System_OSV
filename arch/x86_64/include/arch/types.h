#ifndef _ARCH_X86_64_TYPES_H_
#define _ARCH_X86_64_TYPES_H_

#ifndef __ASSEMBLER__
#include <stdint.h>

typedef uint64_t sysarg_t;
typedef uint64_t sysret_t;
typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef uint64_t offset_t;
typedef uint64_t paddr_t;
typedef uint64_t vaddr_t;
typedef uint16_t port_t;

/*
 * x86-64 specific types
 */
typedef uint64_t pml4e_t;
typedef uint64_t pdpte_t;
typedef uint64_t pde_t;
typedef uint64_t pte_t;
typedef uint64_t pteperm_t;

#endif /* __ASSEMBLER__ */
#endif /* _ARCH_X86_64_TYPES_H_ */
