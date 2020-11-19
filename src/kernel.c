// A kernel
// jprx
#include "vga.h"
#include "types.h"
#include "defines.h"
#include "x86_stuff.h"
#include "paging.h"
#include "interrupt.h"
#include "keyboard.h"
#include "util.h"
#include "typeable.h"
#include "textfield.h"
#include "multiboot.h"
#include "filesystem.h"
#include "exception.h"
#include "syscall.h"
#include "process.h"
#include "file.h"
#include "terminal.h"
#include "user.h"
#include "system.h"
#include "gui.h"
#include "font.h"
#include "scheduler.h"
#include "procfs.h"
#include "sandbox.h"
#include "rtc.h"

// Default typeable
typeable typeable_default = {
    .putc=typeable_putc_default,
    .clear=typeable_clear_default,
    .read=typeable_read_default,
    .enter=typeable_enter_default,
    .x=0,
    .y=1,
    .frame={
        .x = 0,
        .y = 1,
        .w = 0,
        .h = 0
    },
    .buf[0] = 0
};

typeable notepad_typeable = {
    .putc=typeable_putc_default,
    .clear=typeable_clear_default,
    .read=typeable_read_default,
    .enter=typeable_enter_textarea,
    .x=(VGA_WIDTH/2) - 15 + 2,
    .y=(VGA_HEIGHT/2) - 10 + 2,
    .frame={
        .x = (VGA_WIDTH/2) - 15 + 2,
        .y = (VGA_HEIGHT/2) - 10 + 2,
        .w = 30 - 4,
        .h = 20 - 4,
    },
    .buf[0] = 0,
};

// File picker field
typeable filename_typeable = {
    .putc=typeable_putc_default,
    .clear=typeable_clear_default,
    .read=typeable_read_default,
    .enter=typeable_enter_default,
    .x=(VGA_WIDTH/2) - 21,
    .y=(VGA_HEIGHT/2),
    .frame={
        .x = (VGA_WIDTH/2) - 21,
        .y = (VGA_HEIGHT/2),
        .w = 42,
        .h = 1
    },
    .buf[0] = 0
};

char username_buf[USERNAME_LEN];

// Root filesystem operations struct:
fs_ops root_fs_ops = {
    .open = fs_open,
    .close = fs_close,
    .read = fs_read,
    .write = fs_write,
    .check_perm = fs_check_perm,
};

// Special filesystem operations struct:
fs_ops proc_fs_ops = {
    .open = proc_open,
    .close = proc_close,
    .read = proc_read,
    .write = proc_write,
    .check_perm = NULL,
};

// Default terminal:
CREATE_TERMINAL_XY(term1, 5, 3, ((VGA_WIDTH) - 10), (VGA_HEIGHT) - 5);
//CREATE_TERMINAL_XY(gui_term1, 5, 3, ((GUI_FONT_SCREEN_WIDTH) - 4), (GUI_FONT_SCREEN_HEIGHT) - 5);
CREATE_TERMINAL_XY(gui_term1, 15, 6, ((GUI_FONT_SCREEN_WIDTH) - 24), (GUI_FONT_SCREEN_HEIGHT) - 12);

// Configure TSS and LDT:
static inline void configure_segments(void);

// Launch Daemon:
void launchd ();

// Kernel threads:
void watchdog0();
void watchdog1();

// Sandbox level:
#ifdef UIUCTF
uint32_t sandbox_level;
bool password_timing_side_channel;
#endif

// Main entry:
void entry (multiboot_t *boot_info) {
    configure_segments();

    videomem = (color *)boot_info->framebuffer_addr;

    vga_use_highres_gui = false;
    vga_clear();
    vga_setcolor(0xf0);
    vga_disable_hw_blink();

    fs_root = (xentry *)boot_info->mods_addr[0];

    enable_paging();

    // Map all filesystems:
    mount_fs("/", &root_fs_ops);
    mount_fs("/proc", &proc_fs_ops);

    // Load filesystem into virtual memory:
    fs_root = map_filesys_pages(fs_root);
    if (!fs_root) {
        crash_reason("Couldn't map filesystem pages");
    }

    // Load users directory:
    load_users_from_file("/prot/passwd");

    initialize_interrupts();

    // Clear the screen:
    vga_clear();
    vga_setcolor(0x0f);

    // Setup PIC:
    hw_pic_enable();
    // Mask everything to start with:
    for (int i = 0; i < NUM_INTERRUPTS; i++) {
        hw_pic_mask(i);
    }

    // Enable keyboard:
    init_keyboard();

    // Unmask keyboard:
    hw_pic_unmask(IRQ_KEYBOARD);

    // Initialize process table:
    setup_pcb_table();

    // Unmask PIT:
    init_pit();
    hw_pic_unmask(IRQ_PIT);

    // Setup RTC:
    init_rtc();
    hw_pic_unmask(IRQ_RTC);
    hw_pic_unmask(2);

#ifdef UIUCTF
    password_timing_side_channel = true;
#endif

    // Enable interrupts:
    sti();

    // Setup nice GUI:
    if (USE_HIGHRES_GRAPHICS) {
        // Reroute all VGA methods to use highres GUI
        vga_use_highres_gui = true;

        // Map pages for GUI address:
        map_huge_page_kern((uint32_t)videomem, (uint32_t)videomem);
        map_huge_page_kern((uint32_t)videomem + HUGE_PAGE_SIZE, (uint32_t)videomem + HUGE_PAGE_SIZE);
        gui_init(videomem);

        // Wait for user to press enter:
        //vga_printf_centered(((3 * SCREEN_HEIGHT)/FONT_HEIGHT), "Press Enter to Begin.");
        // set_current_typeable(&typeable_default);
        // current_typeable_read(NULL, 0);
    }
    else {
        vga_use_highres_gui = false;
        vga_printf_centered((VGA_HEIGHT/2), "Press Enter to Begin.");

        // Wait for user to press enter:
        set_current_typeable(&typeable_default);
        current_typeable_read(NULL, 0);
    }
    
    if (USE_HIGHRES_GRAPHICS) {
        // Draw desktop:
        gui_respring();
    }
    else {
        vga_clear();
        vga_setcolor(0xf0);
        vga_setattr(0,1,0xFF);
    }
    vga_cursor_enable();

    // Launch the compositor/ shell manager process:
    // "launchd"
    while (1) {
        execute_kern_blocking(launchd, 0, "launchd");
    }

    while(1);
}

// Keep watching processes for the crazy_caches process
void crazy_caches_watchdog ();

// just launchd
#define NUM_KERN_PROCS ((1))

// Launch daemon
// This cannot ever return except by calling sysret
void launchd () {
    // Keep the shell open:
    bool played_animation = false;
    bool needs_redraw = false;

    // Launch daemons here:
    //execute_kern_nonblocking(watchdog0, 0, "watchdog0");

#ifdef UIUCTF
    sandbox_level = SANDBOX_1;
#endif

    while(true) {
        // Login:
        if (USE_HIGHRES_GRAPHICS) {
            // Clear typeables
            if (needs_redraw) {
                // First time around we don't need to draw anything here
                gui_respring();
                gui_redraw();
            }
            needs_redraw = true;
        }
        else {
            vga_clear();
            vga_setcolor(0xf0);
            vga_statusbar(0, 0x0f);
            vga_printf_centered(0,"jprx Kernel");
        }

        set_current_typeable(&typeable_default);
        gui_cursor_activate();
        gui_cursor_enable();
        uid_t logged_in_uid = login_window(!played_animation);

#ifdef UIUCTF
    password_timing_side_channel = false;
#endif

        //uid_t logged_in_uid = 1;
        played_animation = true;

        if (USE_HIGHRES_GRAPHICS) {
            // Setup correct colorscheme:
            display_user_colorscheme(logged_in_uid);

            // Setup default terminal:
            set_active_terminal(&gui_term1);
        }
        else {
            // Clear login box:
            vga_clear();

            // Setup correct colorscheme:
            display_user_colorscheme(logged_in_uid);

            // Setup default terminal:
            set_active_terminal(&term1);
        }

        current_typeable_clear();

        if (USE_HIGHRES_GRAPHICS) {
            // Draw the terminal window:
            // blur_region(
            //     framebuffer,
            //     framebuffer,
            //     SCREEN_WIDTH,
            //     SCREEN_HEIGHT,
            //     50 - 15,
            //     50 - 15 + 5,
            //     SCREEN_WIDTH - 100 + 30,
            //     SCREEN_HEIGHT - 100 + 30,
            //     25,
            //     DARK_MODE_LIGHTEN_AMOUNT);

            blur_region(
                framebuffer,
                framebuffer,
                SCREEN_WIDTH,
                SCREEN_HEIGHT,
                130,
                100,
                SCREEN_WIDTH - 260,
                SCREEN_HEIGHT - 215,
                25,
                DARK_MODE_LIGHTEN_AMOUNT);

            char window_title[64] = "Terminal";
            size_t window_title_len = strlen(window_title);
            uint32_t window_title_scalar = 8;
            //gui_vga_api_color.value = 0;

            // gui_draw_string_framebuffer((SCREEN_WIDTH/2) - (window_title_len * FONT_WIDTH)/(2 * window_title_scalar),
            //     50 - 10 + 5,
            //     window_title,
            //     window_title_scalar,
            //     0xeeeeee);
            gui_draw_string_framebuffer((SCREEN_WIDTH/2) - (window_title_len * FONT_WIDTH)/(2 * window_title_scalar),
                105,
                window_title,
                window_title_scalar,
                0xeeeeee);
            gui_redraw();
        }
        else {
            vga_box_title(3, 2, VGA_WIDTH - 6, VGA_HEIGHT - 3, "Terminal");
        }

#ifdef UIUCTF
        // Sandbox 1:
        execute_user_blocking("/prot/intros/level1", logged_in_uid);

        current_typeable_clear();
        while (sandbox_level == SANDBOX_1) {
            execute_user_blocking("/bin/binexec", logged_in_uid);
        }

        execute_user_blocking("/prot/intros/level2", logged_in_uid);
        //execute_kern_nonblocking(&crazy_caches_watchdog, 0, "crazy_caches_watchdog");

        while (1) {
            // Launch sandbox 2 level:
            sandbox_level = SANDBOX_2;
            display_user_colorscheme(logged_in_uid);

            // Always relaunch crazy_caches
            execute_user_nonblocking("/prot/crazy_caches", USER_USER);
            //execute_kern_nonblocking(crazy_caches_watchdog, 0, "watchdogcc");
            execute_user_blocking("/bin/rash", logged_in_uid);
            current_typeable_clear();

            // Kill all current processes but us and the crazy caches watchdog:
            int i;
            for (i = NUM_KERN_PROCS; i < MAX_PROCESSES; i++) {
                processes[i].should_die = true;
            }
        }
#endif

        // Start the shell:
        execute_user_blocking("/bin/rash", logged_in_uid);
    }
}

// Keep watching processes for the crazy_caches process
void crazy_caches_watchdog () {
    sti();
    int i;
    while (1) {
        bool found_it = false;
        for (i = 0; i < MAX_PROCESSES; i++) {
            if (processes[i].in_use && strncmp(processes[i].name, "/bin/crazy_caches", FS_NAME_LEN)) {
                found_it = true;
            }
        }

        if (!found_it) {
            // Kill all procs but us and launchd
            int j;
            for (j = NUM_KERN_PROCS; j < MAX_PROCESSES; j++) {
                processes[j].should_die = true;
            }
        }
    }
}

// Search a huge page starting at addr
// Maps that page to 0x16400000
void _watchdog0_search_huge_page (void *addr) {
    char *search_addr = (char *)0x16400000;
    map_huge_page_kern(0x16400000, (uint32_t)addr);

    uint32_t j = 0;
    for (j = 0; j < HUGE_PAGE_SIZE; j++) {
        if (search_addr[j] == 'A') {
            uint32_t k = 0;
            for (k = 0; k < 4; k++) {
                if (search_addr[j+k] != 'A') {
                    break;
                }
            }

            // Hacking number found!
            if (k == 4) {
                crash_reason_error_code("watchdog0: Hacking Attempt Detected!", "Sequence Found", ((uint32_t *)(search_addr))[j/4]);
            }
        }
    }
}

void watchdog0() {
    sti();
    while (1) {
        // Search through user process memory and look for "hacking numbers"
        uint32_t idx = 0;
        for (idx = 0; idx < MAX_PROCESSES; idx++) {
            if (processes[idx].in_use && !processes[idx].kern_proc) {
                _watchdog0_search_huge_page(processes[idx].phys_addr);
            }
        }

        // Give kernel memory a scan while we're at it:
        _watchdog0_search_huge_page(&kernel_slide);
    }
}

void watchdog1() {
    sti();
    int i = 0;
    while (1) {
        i+=2;
    }
}

static inline void configure_segments() {
    // Setup LDT:
    sec_mem_desc ldt_desc;
    ldt_desc.limit_0_15 = LDT_SIZE & 0x0FFFF;
    ldt_desc.base_0_15 = (int)&ldt & 0x0FFFF;
    ldt_desc.base_16_23 = ((int)&ldt & 0x00FF0000) >> 16;
    ldt_desc.type = LDT_TYPE; // (9)
    ldt_desc.is_not_system_segment = 0; // We ARE a system segment descriptor
    ldt_desc.dpl = 0;
    ldt_desc.present = 1;
    ldt_desc.limit_16_19 = (LDT_SIZE & 0x00F00000) >> 20;
    ldt_desc.available = 1;
    ldt_desc.always_0 = 0;
    ldt_desc.size = 0;
    ldt_desc.granularity = 0;
    ldt_desc.base_24_31 = ((int)&ldt & 0xFF000000) >> 24;

    // Write LDT descriptor into GDT and load it:
    _gdt_ldt = ldt_desc;
    asm volatile ("lldt %w0" : : "rm"(KERNEL_LDT));

    // Setup TSS:
    // Limit should = TSS allowed size (TSS_SIZE)
    // Base is the address of where our TSS will live in memory
    sec_mem_desc tss_desc;
    tss_desc.limit_0_15 = TSS_SIZE & 0x0FFFF;
    tss_desc.base_0_15 = (int)&tss & 0x0FFFF;
    tss_desc.base_16_23 = ((int)&tss & 0x00FF0000) >> 16;
    tss_desc.type = TSS_TYPE; // (9)
    tss_desc.is_not_system_segment = 0; // We ARE a system segment descriptor
    tss_desc.dpl = 0;
    tss_desc.present = 1;
    tss_desc.limit_16_19 = (TSS_SIZE & 0x00F00000) >> 20;
    tss_desc.available = 1;
    tss_desc.always_0 = 0;
    tss_desc.size = 0;
    tss_desc.granularity = 0;
    tss_desc.base_24_31 = ((int)&tss & 0xFF000000) >> 24;

    // Preconfigure this TSS with esp0 and ss0 for ring 0 (where we are now)
    // I don't think this is necessary but it can't hurt
    uint32_t esp;
    asm volatile("movl %%esp, %0" : "=r"(esp));
    tss.esp0 = esp;
    tss.ss0 = KERNEL_DS; // We use data segment for stack
    tss.ldt = KERNEL_LDT; // LDT descriptor in GDT

    // Write tss into GDT entry for it:
    _gdt_tss = tss_desc;

    // Write TSS into task register (TR):
    asm volatile("ltr %w0" : : "rm"(KERNEL_TSS) );

    // Reload GDT:
    //asm volatile("lgdt _gdt_ptr");
}

/*
 * reboot
 *
 * Crash the system, causing a triple fault (which reboots it :D)
 * This is called by the sysreboot syscall, and after exceptions.
 *
 * Credit for the idea to use triple fault to reboot to OSDEV Wiki
 */
size_t reboot() {
    // Create an empty fake IDT and load IDTR with it
    struct {
        uint16_t idt_size;
        uint32_t idt_addr;
    } null_idtr_val;
    null_idtr_val.idt_size = 0;
    null_idtr_val.idt_addr = 0;
    asm volatile ("lidt %0" : : "m"(null_idtr_val));

    // IDT does not exist! Any interrupts now will cause triple-fault (reboot :D)

    // Bye-bye:
    asm volatile ("int $0x03");

    // Somehow that failed?
    // @TODO: Kernel error code
    return -1;
}

/*
 * shutdown
 *
 * Like reboot, except never starts up again.
 */
size_t shutdown() {
    // @TODO: Make shutdown work

    // @TODO: Kernel error code
    return -1;
}
