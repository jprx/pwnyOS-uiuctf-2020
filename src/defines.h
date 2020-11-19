#ifndef DEFINES_H
#define DEFINES_H

// Set this to 0 this to fall back to VGA graphics:
// For some reason when using HVM on macOS, VGA graphics suck but high-res is super fast
#define USE_HIGHRES_GRAPHICS ((1))

// Mode 0 = 800x600
// Mode 1 = 1024x768
// Mode 2 = 1600x1200
#define RESOLUTION_MODE ((1))

#if USE_HIGHRES_GRAPHICS
#define MULTIBOOT_SET_GRAPHICAL_VIDEO ((1 << 2)) // Try to set resolution to SCREEN_WIDTH by SCREEN_HEIGHT
#else
#define MULTIBOOT_SET_GRAPHICAL_VIDEO ((0))
#endif

#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_ALIGN ((1<<0)) // Align modules on memory pages
#define MULTIBOOT_MEMINFO ((1<<1)) // Provide memory map, apparently
#define MULTIBOOT_FLAGS ((MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO | MULTIBOOT_SET_GRAPHICAL_VIDEO))
#define MULTIBOOT_CHECKSUM ((-(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)))

// Ask GRUB to set these options for screen configuration:
#if (RESOLUTION_MODE == 0)
#define SCREEN_WIDTH ((800))
#define SCREEN_HEIGHT ((600))
#define BG_BUF_WIDTH ((800/2))
#define BG_BUF_HEIGHT ((600/2))
#elif (RESOLUTION_MODE == 1)
#define SCREEN_WIDTH ((1024))
#define SCREEN_HEIGHT ((768))
#define BG_BUF_WIDTH ((1024/2))
#define BG_BUF_HEIGHT ((768/2))
#elif (RESOLUTION_MODE == 2)
#define SCREEN_WIDTH ((1600))
#define SCREEN_HEIGHT ((1200))
#define BG_BUF_WIDTH ((1600/4))
#define BG_BUF_HEIGHT ((1200/4))
#else
#error "Unsupported Resolution Mode"
#endif

#define SCREEN_COLORDEPTH ((32))
#define SCREEN_SIZE ((SCREEN_WIDTH * SCREEN_HEIGHT))

#if (SCREEN_WIDTH == 800)
#define BG_FILE_APPEND "_800_600.bmp"
#elif (SCREEN_WIDTH == 1024)
#define BG_FILE_APPEND ".bmp"
#elif (SCREEN_WIDTH == 1600)
#define BG_FILE_APPEND "_800_600.bmp"
#else
#define BG_FILE_APPEND ".bmp"
#endif

// Segment registers (entries in GDT):
// Recall lower 2 bits of descriptor are the CPL and must match
// the descriptor pointed to's DPL
#define KERNEL_CPL 0
#define KERNEL_DS ((0x10 | KERNEL_CPL))
#define KERNEL_CS ((0x08 | KERNEL_CPL))
#define USER_CPL 3
#define USER_DS ((0x18 | USER_CPL))
#define USER_CS ((0x20 | USER_CPL))
#define KERNEL_TSS 0x28
#define KERNEL_LDT 0x30

// Stack is bottom of 4MiB kernel page:
#define STACK_START ((&kernel_slide + HUGE_PAGE_SIZE))

// Page sizes in bytes:
#define PAGE_SIZE ((1 << 12))
#define HUGE_PAGE_SIZE ((1 << 22))

#endif
