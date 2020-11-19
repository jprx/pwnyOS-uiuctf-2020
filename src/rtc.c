#include "rtc.h"
#include "process.h"
#include "interrupt.h"
#include "util.h"
#include "system.h"

// Used for timing out
#define TIMEOUT_MINS ((15))
#define WARNING_TICKS ((5))
#define TIMEOUT_TICKS ((120 * TIMEOUT_MINS))
uint32_t ticks_since_kb = 0;

// Traverse all processes and decrement their number of ticks
void rtc_handler() {
    hw_pic_eoi(IRQ_RTC);
    outb(0x70, 0x0C);
    inb(0x71);

    uint32_t i = 0;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].in_use) {
            if (processes[i].sleeping) {
                processes[i].num_ticks_remaining--;
            }
            if (processes[i].num_ticks_remaining == 0) {
                processes[i].sleeping = false;
            }
        }
    }

    ticks_since_kb++;

    if (ticks_since_kb > TIMEOUT_TICKS) {
        reboot();
    }
}

// Initialize RTC to 2Hz:
void init_rtc() {
    // Thanks to OSDev Wiki: https://wiki.osdev.org/RTC
    outb(0x70, 0x0B);
    char val = inb(0x71);
    outb(0x70, 0x0B);
    outb(0x71, val | 0x40);

    // Set freq to slow
    outb(0x70, 0x0A);
    val = inb(0x71);
    outb(0x70, 0x0A);
    outb(0x71, (val & 0xF0) | 0x0F);

    outb(0x70, 0x0C);
    inb(0x71);
}
