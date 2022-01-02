# Call into thread_kstart
#
# void kstart();
# 
# only called once to start a kernel thread
# set up proper arguments into thread_kstart
# not supposed to return

.globl kstart
kstart:
    mov %r12, %rdi
    mov %r13, %rsi
    jmp thread_kstart 
