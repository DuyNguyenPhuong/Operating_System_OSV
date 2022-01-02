#include <arch/trap.h>
#include <lib/syscall-num.h>

#define SYSCALL(name) \
  .globl name; \
  name: \
    mov $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(spawn)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(getpid)
SYSCALL(sleep)
SYSCALL(open)
SYSCALL(close)
SYSCALL(read)
SYSCALL(write)
SYSCALL(link)
SYSCALL(unlink)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(readdir)
SYSCALL(rmdir)
SYSCALL(fstat)
SYSCALL(sbrk)
SYSCALL(meminfo)
SYSCALL(dup)
SYSCALL(pipe)
SYSCALL(info)
SYSCALL(halt)
