
.extern syscall
.global syscall_entry

syscall_retval:
    .long 0

# Syscalls all enter here through int $0x80:
syscall_entry:
    # Start with interrupts disabled
    # These are interrupt gates & not trap gates, so they should already start with interrupts
    # disabled, but better safe than sorry
    cli

    # Save registers:
    pushfl
    pushal

    # Arguments:
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    call syscall

    # Save return value:
    movl %eax, syscall_retval

    # Pop arguments off stack:
    # (4 arguments so add 16)
    addl $16, %esp

    # Restore registers
    popal
    popfl

    # Leak a kernel address in ecx
    leal kernel_slide, %ecx

    # Restore eax to return value
    movl syscall_retval, %eax

    # Return from interrupt
    iret

# Perform a syscall
.global do_syscall
do_syscall_retval:
    .long 0
do_syscall:
    pushal
    movl -4(%ebp), %eax
    movl -8(%ebp), %ebx
    movl -12(%ebp), %ecx
    movl -16(%ebp), %edx

    int $0x80
    movl %eax, do_syscall_retval
    popal
    movl do_syscall_retval, %eax
    ret
