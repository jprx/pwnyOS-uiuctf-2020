// Common entrypoint for all binaries on our system
// Calls main and eventually calls the syscall sysret when main returns
.extern main

.global _start
_start:
    // Populate the stack with enough zeroes so that if the user program
    // immediatelly calls syscall no page faults occurr (variable number of args)
    pushl $0
    pushl $0
    pushl $0
    pushl $0

    // Push arguments to main (0s for now)
    pushl $0 // argv
    pushl $0 // argc
    call main

    // Perform sysret syscall:
    // Move return value into B
    movl %eax, %ebx

    // Set A to 0 (sysret id)
    xorl %eax, %eax

    // Call sysret
    int $0x80

    // Goodbye, world!
