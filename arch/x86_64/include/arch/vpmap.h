#ifndef _ARCH_X86_64_VPMAP_H_
#define _ARCH_X86_64_VPMAP_H_

#include <kernel/types.h>

struct vpmap {
    pml4e_t *pml4;
};

#endif /* _ARCH_X86_64_VPMAP_H_ */
