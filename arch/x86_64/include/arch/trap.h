#ifndef _ARCH_X86_64_TRAP_H_
#define _ARCH_X86_64_TRAP_H_

// x86 64 defined exceptions and interrupts
#define T_DE            0
#define T_DB            1
#define T_NMI           2
#define T_BP            3
#define T_OF            4
#define T_BR            5
#define T_UD            6
#define T_NM            7
#define T_DF            8
#define T_TS            10
#define T_NP            11
#define T_SS            12
#define T_GP            13
#define T_PF            14
#define T_MF            16
#define T_AC            17
#define T_MC            18
#define T_XM            19
#define T_VE            20

// Hardware interrupts
#define T_IRQ0          32  // 0-31 are reserved
#define T_IRQ_TIMER     (T_IRQ0+0)
#define T_IRQ_KBD       (T_IRQ0+1)
#define T_IRQ_COM1      (T_IRQ0+4)
#define T_IRQ_IDE       (T_IRQ0+14)
#define T_IRQ_ERROR     (T_IRQ0+19)
#define T_IRQ_SPURIOUS  (T_IRQ0+31)

// Syscall
#define T_SYSCALL       64

// Error codes
#define ERR_X86_TRAP_REG_FAIL 1

#ifndef __ASSEMBLER__
#include <stdint.h>

// Trap frame
struct trapframe {
    // Pushed by trap handler
    uint64_t rax;
  	uint64_t rbx;
  	uint64_t rcx;
  	uint64_t rdx;
  	uint64_t rbp;
  	uint64_t rsi;
  	uint64_t rdi;
  	uint64_t r8;
  	uint64_t r9;
  	uint64_t r10;
  	uint64_t r11;
  	uint64_t r12;
  	uint64_t r13;
  	uint64_t r14;
  	uint64_t r15;
    uint64_t trapnum;

	/* error code, pushed by hardware or 0 by software */
	uint64_t err;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	/* ss:rsp is always pushed in long mode */
	uint64_t rsp;
	uint64_t ss;
};

/*
 * Initialize interrupt descriptor table.
 */
void idt_init(void);

/*
 * Load interrupt descriptor table into the current processor.
 */
void idt_load(void);

/* Set return value of the trapframe */
void tf_set_return(struct trapframe *tf, uint64_t retval);

struct proc;
/* set up trapframe for a newly created process given entry point and stack pointer */
void tf_proc(struct trapframe *tf, struct proc *p, uint64_t entry_point, uint64_t stack_ptr);

#endif /* __ASSEMBLER__ */

#endif /* _ARCH_X86_64_TRAP_H_ */
