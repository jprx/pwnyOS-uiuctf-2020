#include "interrupt.h"
#include "x86_stuff.h"
#include "defines.h"
#include "util.h"
#include "exception.h"
#include "paging.h"
#include "syscall.h"
#include "scheduler.h"
#include "rtc.h"

idt_entry_t idt[IDT_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

// Configure all interrupts into a ready state & load IDT
void initialize_interrupts(void) {
    int i;
    uint32_t saved_flags = cli_and_save();

    for (i = 0; i < IDT_ENTRIES; i++) {
        idt[i].present = 0;
        idt[i].val = 0;
    }

    for (i = 0; i < NUM_EXCEPTIONS; i++) {
        // Setup exception handlers
        init_irq_kern(i, unknown_exception);
    }

    // Exceptions I care about:
    init_irq_kern(GEN_PROTECT_FAULT, gen_protect_fault_entry);
    init_irq_kern(PAGE_FAULT, page_fault_entry);
    init_irq_kern(DBL_FAULT, double_fault_entry);

    // Interrupts:
    init_irq_kern(INT_BASE + IRQ_PIT,       scheduler_entry);
    init_irq_kern(INT_BASE + IRQ_KEYBOARD,  keyboard_handler_entry);
    init_irq_kern(INT_BASE + IRQ_RTC,       rtc_handler_entry);

    // Syscalls:
    init_irq_user(SYSCALL_INT_NUM, syscall_entry);

    // Load IDT:
    asm volatile ("lidt %0" : : "m"(idtr_val));

    restore_flags(saved_flags);
}

void init_irq(uint8_t irq_num, void *callback, bool user_accessible) {
    uint32_t saved_flags = cli_and_save();
    uint32_t callback_int = (uint32_t)callback;
    idt[irq_num].val = 0; // Clear it

    idt[irq_num].offset_0_15 = callback_int & 0x0FFFF;
    idt[irq_num].offset_16_31 = (callback_int & 0x0FFFF0000) >> 16;
    idt[irq_num].segment = KERNEL_CS;
    idt[irq_num].kind = IDT_INTERRUPT_KIND;
    idt[irq_num].dpl = user_accessible ? 3 : 0; // Ring 0 gang only?
    idt[irq_num].present = 1;

    restore_flags(saved_flags);
}

// User-friendly and clear methods for setting up IRQs
// User = users can call this (IE syscalls)
// Kern = only can be called from PL of 0 (Ring 0 gang only)
void init_irq_kern (uint8_t irq_num, void *callback) {
    init_irq(irq_num, callback, false);
}

void init_irq_user (uint8_t irq_num, void *callback) {
    init_irq(irq_num, callback, true);
}

void hw_pic_enable(void) {
    // i8259 is our PIC
    // Let's enable it
    outb(PIC1_CMD, PIC_ICW1);
    outb(PIC2_CMD, PIC_ICW1);

    outb(PIC1_DATA, PIC_ICW2_PIC1);
    outb(PIC2_DATA, PIC_ICW2_PIC2);

    outb(PIC1_DATA, PIC_ICW3_PIC1);
    outb(PIC2_DATA, PIC_ICW3_PIC2);

    outb(PIC1_DATA, PIC_ICW4);
    outb(PIC2_DATA, PIC_ICW4);

    for (int i = 0; i < NUM_INTERRUPTS; i++) {
        hw_pic_mask(i);
    }

    // Enable second PIC
    hw_pic_unmask(PIC2_IRQNUM_ON_PIC1);
    hw_pic_eoi(PIC2_IRQNUM_ON_PIC1);
}

void hw_pic_eoi(uint8_t irq_num) {
    // If IRQ was on second PIC, we need to send EOI to both PICs
    // Otherwise just to the one that issued the IRQ
    if (irq_num >= 8) {
        outb(PIC2_CMD, PIC_EOI | (irq_num - 8));
        outb(PIC1_CMD, PIC_EOI | PIC2_IRQNUM_ON_PIC1);
    }
    else {
        outb(PIC1_CMD, PIC_EOI | irq_num);
    }
}

void hw_pic_unmask(uint8_t irq_num) {
    // Whenver you read from DATA you get the current IMR
    // (Interrupt Mask Register)
    // i8259A Datasheet page 17
    uint8_t pic1_mask, pic2_mask;

    // Use OCW1 (addr = 1) to set
    if (irq_num >= 8) {
        // On PIC 2
        pic2_mask = inb(PIC2_DATA);
        outb(PIC2_DATA, pic2_mask & ~(1 << (irq_num - 8)));
    }
    else {
        // On PIC 1
        pic1_mask = inb(PIC1_DATA);
        outb(PIC1_DATA, pic1_mask & ~(1 << irq_num));
    }
}

void hw_pic_mask(uint8_t irq_num) {
    // Whenver you read from DATA you get the current IMR
    // (Interrupt Mask Register)
    // i8259A Datasheet page 17
    uint8_t pic1_mask, pic2_mask;

    // Use OCW1 (addr = 1) to set
    if (irq_num >= 8) {
        // On PIC 2
        pic2_mask = inb(PIC2_DATA);
        outb(PIC2_DATA, pic2_mask | (1 << (irq_num - 8)));
    }
    else {
        // On PIC 1
        pic1_mask = inb(PIC1_DATA);
        outb(PIC1_DATA, pic1_mask | (1 << irq_num));
    }
}
