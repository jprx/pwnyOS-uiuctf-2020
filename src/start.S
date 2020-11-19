#include "defines.h"

// Exports:
// By default linker will link 'start' as the entrypoint symbol:
// We define an explicit linker script overriding this with _start
// Just in case, I also export 'start' pointing to same address
.global _start, main, start

// Multiboot header:
// Documentation: https://www.gnu.org/software/grub/manual/multiboot/html_node/multiboot_002eh.html
// https://www.gnu.org/software/grub/manual/multiboot/html_node/index.html
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

// Multiboot header stuff for AOUT_KLUDGE, whatever that is
.long 0
.long 0
.long 0
.long 0
.long 0

// Multiboot header for video configuration:
.long 0
.long SCREEN_WIDTH
.long SCREEN_HEIGHT
.long SCREEN_COLORDEPTH

// Location where the linker puts the kernel huge page:
.extern kernel_slide

.align 4
.section .text
.type _start, @function
start:
_start:
    // Stack should be at the end of the kernel huge page
    movl $kernel_slide, %esp
    addl $HUGE_PAGE_SIZE, %esp

    // Load GDT:
    lgdt _gdt_ptr

    // Setup all 6 segment registers
    // Kernel CS is selector 1 (0x8), kernel ds is selector 2 (0x10)
    // Can just load these right away into selectors
    // Can't just move data into CS, need to use long jump
    jmp $KERNEL_CS, $_load_data_segments

_load_data_segments:
    mov $KERNEL_DS, %eax
    mov %eax, %ds
    mov %eax, %ss
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs

    pushl %ebx

    call entry
    hlt

.align 256
.section .data
.global _gdt, _gdt_ptr
.global _gdt_ldt, _gdt_tss
_gdt:
    // Start with unused GDT entry:
    // quadword = 8 bytes (size of GDT entry)
    .quad 0

_gdt_kcs:
    // Kernel CS
    // First 16 bits are segment limit
    .byte 0xff
    .byte 0xff

    // Next 16 bits are segment base
    .byte 0x00
    .byte 0x00

    // Next 8 bits are segment base 16->23
    .byte 0x00

    // Next is the following:
    // Present (1), DPL (2), Is Not System Segment (1), Type (3), Accessed (1)
    // Type = {Executable, Conforming/ Expand-Down, Writable (for data) or readable (for code)}
    // Setting present to 1, DPL to 0, is not system segment to 1, type to executable and readable, accessed to 0
    .byte ((1 << 7)) | (1 << 4) | (1 << 3) | (1 << 1) // 0x9A

    // Next byte is {Limit 16->19, Flags}
    // Set limit to 1111
    // Flags: available, always 0, size, granularity (granularity = bit 7, available = bit 4)
    // size of 0 is 16 bit mode, size of 1 is 32 bit mode
    // Granularity of 0 allows 1 MB limit, granularity of 1 allows 4 GB limit
    // We want size and granularity 1
    .byte (0x0f) | (0xc0) //0xcf

    // Last byte is base again
    .byte 0x00

_gdt_kds:
    // Kernel DS
    // Same drill except instead of executable we mark it as not that
    .byte 0xff
    .byte 0xff
    .byte 0x00
    .byte 0x00
    .byte 0x00
    .byte ((1 << 7)) | (1 << 4) | (1 << 1) // 0x92
    .byte (0x0f) | (0xc0) //0xcf
    .byte 0x00

_gdt_uds:
    // User DS
    .byte 0xff
    .byte 0xff
    .byte 0x00
    .byte 0x00
    .byte 0x00
    .byte ((1 << 7)) | (3 << 5) | (1 << 4) | (1 << 1) // 0xF2
    .byte (0x0f) | (0xc0) //0xcf
    .byte 0x00

_gdt_ucs:
    // User CS
    .byte 0xff
    .byte 0xff
    .byte 0x00
    .byte 0x00
    .byte 0x00
    .byte ((1 << 7)) | (3 << 5) | (1 << 4) | (1 << 3) | (1 << 1) // 0xFA
    .byte (0x0f) | (0xc0) //0xcf
    .byte 0x00

_gdt_tss:
    // The task state segment (TSS) our OS will use
    .quad 0

_gdt_ldt:
    // The LDT entry for our TSS
    .quad 0

_gdt_end:

_gdt_ptr:
    // Lower 16 bytes are size of GDT, upper 32 point to GDT
    .short (_gdt_end - _gdt)
    .long _gdt

.align 256
.global tss
tss:
    // Make 104 bytes free:
    .rept 104
    .byte 0x00
    .endr

.global ldt
ldt:
    // 256 null entries:
    .rept 256
    .quad 0
    .endr

// Interrupt descriptor table
// 256 entries
.extern idt
// idt:
//     .rept 256
//     .quad 0
//     .endr

// Same deal as GDTR
.global idtr_val
idtr_val:
    .short 2048
    .long idt


