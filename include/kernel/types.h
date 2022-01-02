#ifndef _TYPES_H_
#define _TYPES_H_

#ifndef __ASSEMBLER__
#include <arch/types.h>

typedef int8_t bool;
typedef int32_t err_t; // error code
typedef uint32_t state_t; // state of an object (typically each bit is a flag)
typedef int32_t pid_t; // process ID
typedef uint32_t tid_t; // thread ID
typedef uint32_t swapid_t; // swap ID
typedef uint32_t inum_t; // inode number
typedef uint16_t dev_t; // device number
typedef uint32_t blk_t; // block number
typedef uint32_t irq_t; // IRQ number
typedef uint32_t fmode_t; // file permission mode

#endif /* __ASSEMBLER__ */
#endif /* _TYPES_H_ */
