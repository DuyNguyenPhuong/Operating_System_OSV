#ifndef _ARCH_X86_64_CPU_H_
#define _ARCH_X86_64_CPU_H_

/*
 * x86 CPU related data structures and functions.
 */

// EFLAGS
#define FL_IF           0x0200 // Interrupt enable flag

// MSR
#define MSR_IA32_GS_BASE		0xc0000101
#define MSR_IA32_KERNEL_GS_BASE	0xc0000102
#define MSR_IA32_TSC_AUX        0xc0000103
#define MSR_EFER		        0xc0000080	/* extended features */

#define EFER_SCE            0x00000001
#define EFER_LME            0x00000100

// Control Register flags
#define CR0_PE          0x00000001      // Protection Enable
#define CR0_MP          0x00000002      // Monitor coProcessor
#define CR0_EM          0x00000004      // Emulation
#define CR0_TS          0x00000008      // Task Switched
#define CR0_ET          0x00000010      // Extension Type
#define CR0_NE          0x00000020      // Numeric Errror
#define CR0_WP          0x00010000      // Write Protect
#define CR0_AM          0x00040000      // Alignment Mask
#define CR0_NW          0x20000000      // Not Writethrough
#define CR0_CD          0x40000000      // Cache Disable
#define CR0_PG          0x80000000      // Paging

#define CR4_PSE         0x00000010      // Page size extension
#define CR4_PAE         0x00000020      // Page size extension


#ifndef __ASSEMBLER__

#include <stdint.h>
#include <arch/mmu.h>

// X86-64 specific CPU data structure
struct x86_64_cpu {
    uint8_t lapic_id;
    struct segdesc gdt[NSEGS];
    struct taskstate ts;         // Used by x86 to find stack for interrupt
    struct thread *idle_thread;  // tracks idle thread for this cpu
    struct thread *thread;       // The thread running on this cpu or null
    volatile uint32_t started;   // Has the CPU started?
    int num_disabled;            // Depth of cli nesting.
    int intr_enabled;            // Were interrupts enabled before it's first diabled?
    struct x86_64_cpu *cpu;          // stores current cpu struct address
};
#define MAX_NCPU 32
extern struct x86_64_cpu x86_64_cpus[MAX_NCPU];
extern int ncpu;

/*
 * Return the ``x86_cpu`` struct for the current running processor.
 * Should only be called when interrupt is disabled or when there is only one processor up.
 */
struct x86_64_cpu *mycpu(void);

// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %rax, %rcx, %rdx, because the
// x86-64 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save rip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
};

struct thread* cpu_idle_thread(struct x86_64_cpu*);
void cpu_set_idle_thread(struct x86_64_cpu*, struct thread*);
void cpu_clear_thread(struct x86_64_cpu*);
struct thread* cpu_switch_thread(struct x86_64_cpu*, struct thread*);
#endif /* __ASSEMBLER__ */

#endif /* _ARCH_X86_64_CPU_H_ */
