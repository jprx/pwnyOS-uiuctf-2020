#ifndef X86_STUFF_H
#define X86_STUFF_H

#include "types.h"
#include "defines.h"

// A TSS can be at minimum 103 bytes apparently
// I am using 104 to be safe
// There actually is no upper limit on TSS size and that can be useful
// for port IO reasons
#define TSS_SIZE ((104))

// "Type code of 9 indicates a non-busy task" -i386 manual
#define TSS_TYPE ((9))

// i386 Manual page 125:
// Let's make an LDT with just 4 NULL entries
#define LDT_TYPE ((2))
#define LDT_ENTRIES ((4))
#define LDT_SIZE ((8 * LDT_ENTRIES)) // 8 bytes per entry

// Number of interrupts we support:
#define IDT_ENTRIES ((256))
#define IDT_SIZE ((8 * IDT_ENTRIES))
#define IDT_INTERRUPT_KIND ((0x0E))

// A generic segment descriptor:
typedef struct __attribute__((packed)) segment_descriptor_struct {
    uint32_t base;
    uint32_t limit; // Only 20 bits of this are used

    bool present;
    uint8_t dpl;
    uint8_t type; // Page 110 of i386 Manual defines these 
                  // (will be different for GDT but GDT done explicitly in assembly)
    bool is_not_system_segment; // 1 for CS, DS stuff, 0 for TSS

    bool size; // 1 for 32 bit mode, 0 for 16 bit mode. For TSS this should be 0
    bool granularity; // 0 for small (1MB), 1 for big (4GB)
} seg_desc;

// A generic segment descriptor but bit for bit correct
typedef struct __attribute__((packed)) segment_descriptor_struct_mem_correct {
    uint16_t limit_0_15;
    uint16_t base_0_15;
    uint8_t base_16_23;
    uint8_t type : 4;
    uint8_t is_not_system_segment : 1;
    uint8_t dpl : 2;
    uint8_t present : 1;
    uint8_t limit_16_19 : 4;
    uint8_t available : 1;
    uint8_t always_0 : 1;
    uint8_t size : 1;
    uint8_t granularity : 1;
    uint8_t base_24_31;
} sec_mem_desc;

// i386 Manual page 132
// https://css.csail.mit.edu/6.858/2014/readings/i386.pdf
typedef struct __attribute__((packed)) tss_struct {
    uint16_t link;
    uint16_t res0;

    // Stack & stack segment for ring 0
    uint32_t esp0;
    uint16_t ss0;
    uint16_t res1;

    // Stack & stack segment for ring 1
    uint32_t esp1;
    uint16_t ss1;
    uint16_t res2;

    // Stack & stack segment for ring 2
    uint32_t esp2;
    uint16_t ss2;
    uint16_t res3;

    uint32_t cr3;

    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;

    uint16_t es;
    uint16_t res_es;

    uint16_t cs;
    uint16_t res_cs;

    uint16_t ss;
    uint16_t res_ss;

    uint16_t ds;
    uint16_t res_ds;

    uint16_t fs;
    uint16_t res_fs;

    uint16_t gs;
    uint16_t res_gs;

    uint16_t ldt;
    uint16_t res_ldt;

    uint8_t T : 1;
    uint8_t res_T : 7;

    uint16_t iomap_base;
} tss_t;

typedef struct __attribute__((packed)) idt_entry_t_struct {
    union {
        uint64_t val;
        struct {
            uint16_t offset_0_15;
            uint16_t segment;
            uint8_t res0;
            uint8_t kind : 5; // Set this to 01110 for IDT entries (0x0E)
            uint8_t dpl : 2;
            uint8_t present : 1;
            uint16_t offset_16_31;
        };
    };
} idt_entry_t;

// A dummy LDT entry
typedef struct __attribute__((packed)) {
    uint64_t value;
} ldt_dummy_entry_t;

// A full iret context
// If this interrupt occured during ring 0 operation, then old SS and old ESP aren't valid
typedef struct __attribute__((packed)) {
    // Error code would be before all of this; assumption is error code was removed
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;

    // These are pushed only when crossing privilege boundary
    uint32_t esp;
    uint32_t ss;
} iret_context;

// The global TSS and TSS descriptor (defined in start.S):
extern tss_t tss;
extern sec_mem_desc _gdt_tss;

// The LDT and LDT descriptor in GDT (defined in start.S):
// (ldt symbol = first LDT entry)
extern ldt_dummy_entry_t ldt;
extern sec_mem_desc _gdt_ldt;

// The IDT:
extern idt_entry_t idt[IDT_ENTRIES];
extern uint64_t idtr_val; // Value to load into idtr

// Wherever the linker puts us:
// Access the location using &kernel_slide (kernel_slide is a symbol)
extern uint32_t kernel_slide;

// Exceptions that I thought were cool
// The rest go to unknown_exception
#define DIV_0 0
#define DBG 1
#define NMI 2
#define BREAKPT 3
#define DBL_FAULT 8
#define GEN_PROTECT_FAULT 13
#define PAGE_FAULT 14

#endif








