// jprx
#ifndef VGA_H
#define VGA_H

#include "types.h"

// Screen
#define VGA_WIDTH ((80))
#define VGA_HEIGHT ((25))
#define VGA_VMEM_SIZE ((VGA_WIDTH * VGA_HEIGHT))

#define VGA_VIDMEM ((0xB8000))

// VGA "Art" constants
#define BOX_HORIZ 205
#define BOX_VERT 186
#define BOX_CORNER_UL 201
#define BOX_CORNER_LL 200
#define BOX_CORNER_UR 187
#define BOX_CORNER_LR 188

// Attribute register methods
void vga_disable_hw_blink(void);

// Window affecting methods
void vga_clear(void);
void vga_setcolor(char attr);
void vga_clear_box (int x, int y, int w, int h);

// Single char methods
void vga_setc(int x, int y, char c);
void vga_setattr(int x, int y, char attr);

// Printf
void vga_printf(char *str, ...);
void vga_printf_xy(int x, int y, char *str, ...);
void vga_printf_centered(int h, char *str, ...);

// Pseudo-terminal: always prints next char after the previous
// call to vga_printf_term regardless of other printf_xy calls
//void vga_printf_term(char *str, ...);
void vga_putc_term(char c);
void vga_set_cursor_term(int x, int y);

// Box/ windowing methods
void vga_box(int x, int y, int w, int h);
void vga_box_attr(int x, int y, int w, int h, char attr);
void vga_box_title(int x, int y, int w, int h, char *title);
void vga_box_title_centered (int w, int h, char *title);
void vga_statusbar(int h, char attr);
void vga_popup_simple(int w, int h, char *title); // Just use default attribute for background
void vga_popup(int w, int h, char *title, char bg_attr);
void vga_popup_clear(void);

// Cursor methods
// What shape does the cursor take within the character?
// 0 = top of char, 15 = bottom of char
#define CURSOR_TOP 14
#define CURSOR_BOTTOM 14

void vga_cursor_enable(void);
void vga_cursor_disable(void);
void vga_cursor_setxy(int x, int y);

// Switch this to true to tell VGA methods to use GUI methods instead:
extern bool vga_use_highres_gui;

#endif
