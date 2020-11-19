#ifndef INTERRUPT_H
#define INTERRUPT_H
#include "types.h"

// Setup default interrupts:
void initialize_interrupts(void);

// Enable a particular interrupt
void init_irq(uint8_t irq_num, void *callback, bool user_accessible);

// User/ kern distinction: user means users can call it using int $0x80, kern they cannot
void init_irq_kern (uint8_t irq_num, void *callback);
void init_irq_user (uint8_t irq_num, void *callback);

// Hardware interrupt controller stuff
void hw_pic_enable(void); // Enable PIC
void hw_pic_eoi(uint8_t irq_num); // Send EOI
void hw_pic_mask(uint8_t irq_num); // Mask vector
void hw_pic_unmask(uint8_t irq_num); // Unmask vector

// i8259 Control Defines:
#define PIC1_ADDR 0x20
#define PIC2_ADDR 0xA0
#define PIC1_CMD PIC1_ADDR
#define PIC1_DATA ((PIC1_ADDR + 1))
#define PIC2_CMD PIC2_ADDR
#define PIC2_DATA ((PIC2_ADDR + 1))

// Which IRQ on the main PIC is the secondary PIC installed?
#define PIC2_IRQNUM_ON_PIC1 0x02

#define INT_BASE 0x20

// i8259A Manual: https://pdos.csail.mit.edu/6.828/2008/readings/hardware/8259A.pdf

// i8259A Manual says ICW1 is defined "When A0=0 and D4=1"
// D0 means that we are sending an ICW4
// 0x11 = Initializing PIC and sending an ICW 4
#define PIC_ICW1 0x11

// IDT index for each PIC
#define PIC_ICW2_PIC1 ((INT_BASE))
#define PIC_ICW2_PIC2 ((INT_BASE + 0x8))

// For master, this is the bit vector where slave is connected (IRQ2 so bit 2)
// For slave, this is the index on the master we are connected to (IRQ2 so 2)
#define PIC_ICW3_PIC1 ((1 << PIC2_IRQNUM_ON_PIC1)) // Should be 4
#define PIC_ICW3_PIC2 ((PIC2_IRQNUM_ON_PIC1)) // Should be 2

// This puts the PIC into '8086 Mode' instead of something like buffered mode
#define PIC_ICW4 0x01

// OCW2 is the EOI command
// OR this value with the IRQ number to send EOI
// Could also set bit 5 to tell PIC this is "non-specific EOI"
#define PIC_EOI 0x20

#define NUM_INTERRUPTS 16

// IRQ defines
#define IRQ_PIT 0
#define IRQ_KEYBOARD 1
#define IRQ_RTC 8

// Interrupt handler assembly headers:
void keyboard_handler(void);
extern void keyboard_handler_entry(void);
void rtc_handler(void);
extern void rtc_handler_entry(void);

#endif
