// Exception handlers but in C
#include "vga.h"
#include "exception.h"
#include "util.h"
#include "process.h"
#include "typeable.h"
#include "x86_stuff.h"
#include "system.h"
#include "typeable.h"
#include "gui.h"
#include "interrupt.h"
#include "keymap.h"

// Instead of panicing, just kill the crashing process
// iretctx should point to iret context that was pushed to this stack
static inline void _sysret_if_user (iret_context *iretctx, char *message, uint32_t error_code) {
    // Kill the user process (if we are executing a user process):
    // First, ensure the fault occurred in the USER_CS segment and not KERNEL_CS
    // We can do this by looking at which code segment the iretctx pushed
    if (iretctx->cs == USER_CS) {
        if (current_proc != NULL && !current_proc->kern_proc) {
            if (message) {
                current_typeable_printf(message);
            }

            // @TODO: Make this return some kind of standardized kernel error code:
            sysret(error_code);
        }
    }
}

// Common high-res graphics drawing routines for crashes
void _crash_gui_common() {
    gui_cursor_disable();
    blur(framebuffer, framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, 2 * DARK_MODE_LIGHTEN_AMOUNT);
    gui_redraw();

    gui_draw_string((SCREEN_WIDTH/2) - ((4 * FONT_WIDTH)/2), (SCREEN_HEIGHT/2) - (FONT_HEIGHT/12) - FONT_HEIGHT/3, "Kernel Panic", 3, 0xFF2222);

    gui_draw_string ((SCREEN_WIDTH/2) - (21 * FONT_WIDTH/12), (SCREEN_HEIGHT/2) - (FONT_HEIGHT/12) + 4*(FONT_HEIGHT/6), "Press Enter to Reboot", 6, 0xFFFFFF);
}

// Write the name of this fault to the screen centered
void _gui_draw_crash_reason (char *reason) {
    size_t len = strlen(reason);
    gui_draw_string((SCREEN_WIDTH/2) - ((len * FONT_WIDTH)/12), (SCREEN_HEIGHT/2) - (FONT_HEIGHT/12), reason, 6, 0xFFFFFF);
}

void _exception_reboot_keyboard () {
    // Only reboot on key down
    uint8_t keycode;

    keycode = inb(0x60);

    hw_pic_eoi(IRQ_KEYBOARD);

    // // Allow any key pressed down
    // // (This returns if it encounters anything but a keypress down)
    // if (keycode & 0x80 || keycode == 0x61) return;
    // reboot();

    // Just wait for enter:
    if (keycode == PS2_ENTER) {
        reboot();
    }
}

/*
 * crash_reboot
 *
 * Exceptions should call this after they display their animation and error message.
 * This will prompt the user to press enter, after which it will call "reboot" to reboot the machine.
 */
void crash_reboot(void) {
    // Mask all interrupts but keyboard:
    for (int i = 0; i < NUM_INTERRUPTS; i++) {
        hw_pic_mask(i);
    }
    hw_pic_unmask(IRQ_KEYBOARD);
    sti();

    // Remap keyboard interrupt to reboot:
    init_irq_kern(INT_BASE + IRQ_KEYBOARD, _exception_reboot_keyboard);

    while(1) {
        sti();
    }
}

// We hit an exception that's pretty bad
void crash(void) {
    // @TODO: add _sysret_if_user here
    // This is non-trivial because some exceptions have error codes, others don't (thanks intel)
    if (!vga_use_highres_gui) {
        vga_popup_clear();
        vga_setcolor(0x0F);
        vga_popup(30, 4, "This is an Exception!", 0x04);
        vga_cursor_disable();
    }
    else {
        _crash_gui_common();
        _gui_draw_crash_reason("Unknown Exception");
    }
    crash_reboot();
}

// Generic kernel panic
void crash_reason(char *reason) {
    if (!vga_use_highres_gui) {
        vga_popup_clear();
        vga_setcolor(0x0F);
        vga_cursor_disable();

        int dummy = 0;
        int box_w = 0;
        int box_h = 4;
        int box_w_target = 30;
        for (box_w = 0; box_w < box_w_target; box_w++) {
            vga_box_attr((VGA_WIDTH/2) - (box_w/2), (VGA_HEIGHT/2) - (box_h/2), box_w, box_h, 0x04);
            while (dummy < 13500000/2) { dummy++; }
            vga_clear_box((VGA_WIDTH/2) - (box_w/2), (VGA_HEIGHT/2) - (box_h/2), box_w, box_h);
            dummy = 0;
        }

        vga_popup(40, 4, "Kernel Panic!", 0x04);
        vga_printf_centered((VGA_HEIGHT/2), reason);
    }
    else {
        _crash_gui_common();
        _gui_draw_crash_reason(reason);
    }
    
    crash_reboot();
}

// Kernel panic with an error code
void crash_reason_error_code(char *reason, char *error_code_name, uint32_t error_code) {
    if (!vga_use_highres_gui) {
        vga_popup_clear();
        vga_setcolor(0x0F);
        vga_cursor_disable();

        int dummy = 0;
        int box_w = 0;
        int box_h = 4;
        int box_w_target = 30;
        for (box_w = 0; box_w < box_w_target; box_w++) {
            vga_box_attr((VGA_WIDTH/2) - (box_w/2), (VGA_HEIGHT/2) - (box_h/2), box_w, box_h, 0x04);
            while (dummy < 13500000/2) { dummy++; }
            vga_clear_box((VGA_WIDTH/2) - (box_w/2), (VGA_HEIGHT/2) - (box_h/2), box_w, box_h);
            dummy = 0;
        }

        vga_popup(40, 4, "Kernel Panic!", 0x04);
        vga_printf_centered((VGA_HEIGHT/2), reason);
    }
    else {
        _crash_gui_common();
        _gui_draw_crash_reason(reason);

        char errbuf[64];
        snprintf(errbuf, sizeof(errbuf), "%s: 0x%0x", error_code_name, error_code);
        size_t errbuf_size = strlen(errbuf);
        gui_draw_string((SCREEN_WIDTH/2) - (errbuf_size * FONT_WIDTH)/12, (SCREEN_HEIGHT/2) - (FONT_HEIGHT/12) + FONT_HEIGHT/6, errbuf, 6, 0xFFFFFF);
    }
    
    crash_reboot();
}

/*
 * page_fault_handler
 *
 * This is called whenever a page fault occurs.
 *
 * bad_addr is copied from CR2 and placed on the stack for us by page_fault_entry.
 * Implicitly, there is also an iret context and error code placed on the stack by the exception entry.
 * page_fault_entry doesn't do copy it or anything- the variable "iretctx" is the real iret context!
 */
void page_fault_handler(uint32_t bad_addr, uint32_t error_code, iret_context iretctx) {
    _sysret_if_user(&iretctx, "Segmentation Fault\n", bad_addr);

    if (!vga_use_highres_gui) {
        vga_popup_clear();
        vga_setcolor(0x0F);
        vga_cursor_disable();

        int dummy = 0;
        int box_w = 0;
        int box_h = 4;
        int box_w_target = 30;
        for (box_w = 0; box_w < box_w_target; box_w++) {
            vga_box_attr((VGA_WIDTH/2) - (box_w/2), (VGA_HEIGHT/2) - (box_h/2), box_w, box_h, 0x04);
            while (dummy < 13500000/2) { dummy++; }
            vga_clear_box((VGA_WIDTH/2) - (box_w/2), (VGA_HEIGHT/2) - (box_h/2), box_w, box_h);
            dummy = 0;
        }

        vga_popup(30, 5, "Kernel Panic: Page Fault", 0x04);
        vga_printf_xy((VGA_WIDTH/2) - (18/2), (VGA_HEIGHT/2), "Address: 0x%0x", bad_addr);
        //vga_clear_box((VGA_WIDTH/2) - (24/2), (VGA_HEIGHT/2) + 3, 25, 3);
        //vga_printf_centered((VGA_HEIGHT/2) + 4, "(Press Enter to Reboot)");
        vga_printf_centered((VGA_HEIGHT/2) + 1, "(Press Enter to Reboot)"); // This is 23 chars long
    }
    else {
        _crash_gui_common();
        _gui_draw_crash_reason("Page Fault");

        char addrbuf[64];
        snprintf(addrbuf, sizeof(addrbuf), "Address: 0x%0x", bad_addr);
        size_t addrbuf_size = strlen(addrbuf);
        gui_draw_string((SCREEN_WIDTH/2) - (addrbuf_size * FONT_WIDTH)/12, (SCREEN_HEIGHT/2) - (FONT_HEIGHT/12) + FONT_HEIGHT/6, addrbuf, 6, 0xFFFFFF);
    }

    crash_reboot();
}

void double_fault_handler(void) {
    if (!vga_use_highres_gui) {
        vga_popup_clear();
        vga_setcolor(0x0F);
        vga_popup(30, 4, "Double Fault", 0x0F);
        vga_cursor_disable();
    }
    else {
        _crash_gui_common();
        _gui_draw_crash_reason("Double Fault");
    }
    crash_reboot();
}

void gen_protect_fault_handler(uint32_t error_code, iret_context iretctx) {
    _sysret_if_user(&iretctx, "General Protection Fault\n", error_code);

    if (!vga_use_highres_gui) {
        vga_popup_clear();
        vga_setcolor(0x0F);
        vga_popup(30, 4, "Gen Protect Fault", 0x04);
        vga_printf_centered((VGA_HEIGHT/2), "Code: 0x%x", error_code);
        vga_cursor_disable();
    }
    else {
        _crash_gui_common();
        _gui_draw_crash_reason("Gen Protect Fault");

        char codebuf[64];
        snprintf(codebuf, sizeof(codebuf), "Code: 0x%0x", error_code);
        size_t codebuf_size = strlen(codebuf);
        gui_draw_string((SCREEN_WIDTH/2) - (codebuf_size * FONT_WIDTH)/12, (SCREEN_HEIGHT/2) - (FONT_HEIGHT/12) + FONT_HEIGHT/6, codebuf, 6, 0xFFFFFF);
    }
    
    crash_reboot();
}