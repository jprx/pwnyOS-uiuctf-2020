// jprx
#include "vga.h"
#include "types.h"
#include "util.h"
#include "defines.h"
#include "keymap.h"
#include "gui.h"
#include "font.h"

static char *vmem = (char*)VGA_VIDMEM;
static int vmem_x = 0;
static int vmem_y = 0;

// When this is true, all operations writing to VGA video memory are rerouted
// to the high-resolution screen framebuffer
bool vga_use_highres_gui = false;

// On real hardware, bit 7 by default of the attribute value of a location
// Determines if it blinks or not. So 0xF0 would really mean "blink" and set background
// color to mostly white. We want 0xF0 to mean "full brightness background" and not blink.
// Set regsiter index 10h bit 3 to 0 to do this. (Attribute mode control register)
// FreeVGA docs: http://www.scs.stanford.edu/10wi-cs140/pintos/specs/freevga/vga/vgareg.htm#attribute
void vga_disable_hw_blink(void) {
    inb(0x3DA);
    uint8_t saved_addr_data_reg = inb(0x3C0);
    outb(0x3C0, 0x10);
    uint8_t cur_attr_status = inb(0x3C1);
    cur_attr_status &= ~(1 << 3);
    outb(0x3C0, cur_attr_status);
    outb(0x3C0, saved_addr_data_reg);
}

void vga_clear(void) {
    int i;
    for (i = 0; i < (VGA_HEIGHT * VGA_WIDTH); i++) {
        vmem[2*i] = 0;
    }
}

static char _color = 0xf0;
void vga_setcolor(char attr) {
    int i;
    _color = attr;
    for (i = 0; i < (VGA_HEIGHT * VGA_WIDTH); i++) {
        vmem[2*i+1] = attr;
    }
}

// 0 = not in VMEM, 1 = in VMEM
int vga_safe_loc(int x, int y) {
    return (x + y * VGA_WIDTH) < VGA_VMEM_SIZE;
}

void vga_setc(int x, int y, char c) {
    if (vga_use_highres_gui) {
        gui_setc(x, y, c, gui_vga_api_color.value);
    }
    else {
        if (vga_safe_loc(x,y)) {
            vmem[2*(x+y*VGA_WIDTH)] = c;
        }
    }
}

void vga_setattr(int x, int y, char attr) {
    if (vga_safe_loc(x,y))
        vmem[2*(x+y*VGA_WIDTH)+1] = attr;
}

// Put a single char, wrapping as needed
void vga_putc(char c) {
    if (vga_use_highres_gui) {
        gui_setc(vmem_x, vmem_y, c, gui_vga_api_color.value);
        vmem_x++;
        if (vmem_x > GUI_FONT_SCREEN_WIDTH) {
            vmem_x = 0;
            vmem_y++;
        }
    }
    else {
        vga_setc(vmem_x, vmem_y, c);
        vmem_x += 1;
        if (vmem_x > VGA_WIDTH) {
            vmem_x = 0;
            vmem_y++;
        }
    }
}

// Version of printf used by all other versions of printf
static inline void _vga_printf(char *str, uint32_t *arg) {
    char *idx;

    bool is_special_code = false;
    bool should_print_preleading_zeroes = false;
    bool printed_anything = false;

    for (idx = str; *idx != NULL; idx++) {
        if (is_special_code) {
            if (*idx == 'x' || *idx == 'X') {
                // Treat *arg as a hex number
                uint32_t arg_val = *arg;
                bool has_seen_nonzero = false;
                uint32_t counter = 8; // Max length
                while (counter > 0) {
                    int cur_digit = ((arg_val & 0xF0000000) >> 28);
                    if (cur_digit != 0) has_seen_nonzero = true;

                    if ((!(cur_digit == 0 && !has_seen_nonzero)) || should_print_preleading_zeroes) {
                        if (cur_digit < 0x0A) {
                            // Numeric
                            vga_putc(cur_digit + 0x30);
                        }
                        else {
                            // Alpha
                            vga_putc(cur_digit + 0x41 - 0x0A);
                        }
                        printed_anything = true;
                    }

                    arg_val = arg_val << 4;
                    counter--;
                }

                if (!printed_anything) {
                    // Thing to print was 0 and we didn't enable preleading zeroes
                    // So just print a 0 and move on
                    vga_putc('0');
                }

                // Move to next argument:
                is_special_code = false;
                should_print_preleading_zeroes = false;
                printed_anything = false;
                arg++;
                continue;
            }
            else if (*idx == '0') {
                // This means preprint the leading zeroes
                should_print_preleading_zeroes = true;

                // Not done with this argument yet so continue:
                continue;
            }
            else if (*idx == 's') {
                // Print string
                char *cursor = *(char **)arg;
                while (*cursor) {
                    if (*cursor == 0x0A) {
                        vmem_x = 0;
                        vmem_y++;
                    }
                    else {
                        vga_putc(*cursor);
                    }
                    cursor++;
                }

                should_print_preleading_zeroes = false;
                is_special_code = false;
                arg++;
                continue;
            }
            else if (*idx == 'c') {
                // Print char
                vga_putc(*arg);

                arg++;
                is_special_code = false;
                continue;
            }
            else {
                // Couldn't handle this argument type,
                // Move to next argument:
                is_special_code = false;
                arg++;
                continue;
            }
        }

        if (*idx == 0x0A) {
            vmem_x = 0;
            vmem_y++;
            continue;
        }
        if (*idx == '%') {
            is_special_code = true;
            continue;
        }

        vga_putc(*idx);
    }
}

// printf_xy for use by misc. methods internally to support variable number of args
static inline void _vga_printf_xy (int x, int y, char *str, uint32_t *arg) {
    int saved_vmem_x = vmem_x;
    int saved_vmem_y = vmem_y;
    vmem_x = x;
    vmem_y = y;
    _vga_printf(str, arg);
    vmem_x = saved_vmem_x;
    vmem_y = saved_vmem_y;
}

void vga_printf(char *str, ...) {
    // Pointer to first argument (assuming only 32 bit args):
    uint32_t *arg = ((void*)&str);
    arg=arg+1;
    _vga_printf(str, arg);
}

void vga_printf_xy(int x, int y, char *str, ...) {
    // Pointer to first argument (assuming only 32 bit args):
    uint32_t *arg = ((void*)&str);
    arg=arg+1;

    _vga_printf_xy(x,y,str,arg);
}

static int term_x = 0;
static int term_y = 1;
void vga_putc_term(char c) {
    if (c == ASCII_BACKSPACE) {
        if (term_x > 0) {
            term_x--;
        }
        else {
            term_x = VGA_WIDTH-1;
            term_y--;
        }
        vga_setc(term_x, term_y, ' ');
        vga_cursor_setxy(term_x, term_y);
        return;
    }

    if (c == ASCII_RETURN) {
        term_x = 0;
        term_y++;
        vga_cursor_setxy(term_x, term_y);
        return;
    }

    if (c == ASCII_TAB) {
        term_x+=4;
        if (term_x > VGA_WIDTH) {
            term_x = (term_x - VGA_WIDTH);
            term_y++;
        }
        vga_cursor_setxy(term_x, term_y);
        return;
    }

    vga_setc(term_x, term_y, c);
    term_x++;
    if (term_x > VGA_WIDTH) {
        term_x = 0;
        term_y++;
    }
    vga_cursor_setxy(term_x, term_y);
}

void vga_set_cursor_term(int x, int y) {
    term_x = x;
    term_y = y;
    vga_cursor_setxy(x,y);
}

void vga_printf_centered(int h, char *str, ...) {
    // Pointer to first argument (assuming only 32 bit args):
    uint32_t *arg = ((void*)&str);
    arg=arg+1;

    int len = strlen(str);
    int start_x = 0;
    if (vga_use_highres_gui) {
        start_x = ((3 * SCREEN_WIDTH)/FONT_WIDTH) - (len/2) + 1;
    }
    else {
        start_x = (VGA_WIDTH/2) - (len/2);
    }
    _vga_printf_xy(start_x, h, str, arg);
}

// #define BOX_VERT 205
// #define BOX_HORIZ 186
// #define BOX_CORNER_UL 201
// #define BOX_CORNER_LL 200
// #define BOX_CORNER_UR 187
// #define BOX_CORNER_LR 188
void vga_box(int x, int y, int w, int h) {
    int i, j;
    for (i = x; i < x+w; i++) {
        vga_setc(i,y,BOX_HORIZ);
    }
    for (i = x; i < x+w; i++) {
        vga_setc(i,y+h-1,BOX_HORIZ);
    }
    for (j = y; j < y+h; j++) {
        vga_setc(x,j,BOX_VERT);
    }
    for (j = y; j < y+h; j++) {
        vga_setc(x+w-1,j,BOX_VERT);
    }

    vga_setc(x,y,BOX_CORNER_UL);
    vga_setc(x+w-1,y,BOX_CORNER_UR);
    vga_setc(x,y+h-1,BOX_CORNER_LL);
    vga_setc(x+w-1,y+h-1,BOX_CORNER_LR);
}

void vga_box_attr(int x, int y, int w, int h, char attr) {
    int i, j;
    for (i = x; i < x+w; i++) {
        vga_setc(i,y,BOX_HORIZ);
        vga_setattr(i,y,attr);
    }
    for (i = x; i < x+w; i++) {
        vga_setc(i,y+h-1,BOX_HORIZ);
        vga_setattr(i,y+h-1,attr);
    }
    for (j = y; j < y+h; j++) {
        vga_setc(x,j,BOX_VERT);
        vga_setattr(x,j,attr);
    }
    for (j = y; j < y+h; j++) {
        vga_setc(x+w-1,j,BOX_VERT);
        vga_setattr(x+w-1,j,attr);
    }

    vga_setc(x,y,BOX_CORNER_UL);
    vga_setc(x+w-1,y,BOX_CORNER_UR);
    vga_setc(x,y+h-1,BOX_CORNER_LL);
    vga_setc(x+w-1,y+h-1,BOX_CORNER_LR);
    vga_setattr(x,y,attr);
    vga_setattr(x+w-1,y,attr);
    vga_setattr(x,y+h-1,attr);
    vga_setattr(x+w-1,y+h-1,attr);
}

// Returns scaled width to accomodate a 1-line message
int vga_rescale_to_fit(int w, char *msg) {
    int title_len = strlen(msg) + 3;
    return (title_len > w) ? title_len : w;
}

void vga_box_title(int x, int y, int w, int h, char *title) {
    // Auto rescale width to fit title

    // Yeah this calls strlen twice but eh, its ok
    int title_len = strlen(title);
    w = vga_rescale_to_fit(w, title);
    vga_box(x,y,w,h);
    vga_printf_xy((w/2)-(title_len/2) + x, y, title);
}

void vga_box_title_centered (int w, int h, char *title) {
    vga_box_title((VGA_WIDTH/2) - (w/2), (VGA_HEIGHT/2) - (h/2), w, h, title);
}

// Popups need to obscure what's behind them, saving it for later
static bool popup_present = false;
static int cover_x, cover_y, cover_w, cover_h;
static char cover_buffer [VGA_VMEM_SIZE*2];
void vga_popup (int w, int h, char *title, char bg_attr) {
    int i, j;
    int topleft_x, topleft_y;

    if (vga_use_highres_gui) {
        gui_popup(w, h, title);
        return;
    }

    if (popup_present) {
        vga_popup_clear();
    }
    popup_present = true;
    w = vga_rescale_to_fit(w, title);

    int title_len = strlen(title);

    topleft_x = (VGA_WIDTH/2) - (w/2);
    topleft_y = (VGA_HEIGHT/2) - (h/2);
    for (i = topleft_x; i < topleft_x + w; i++) {
        for (j = topleft_y; j < topleft_y + h; j++) {
            int idx = (i + j * VGA_WIDTH);
            cover_buffer[2 * idx] = vmem[2 * idx];
            cover_buffer[2 * idx + 1] = vmem[2 * idx + 1];
            vga_setc(i,j,' ');
            vga_setattr(i,j,bg_attr);
        }
    }

    // cover_* are static vars that correspond to what is being covered on screen right now
    cover_x = topleft_x;
    cover_y = topleft_y;
    cover_w = w;
    cover_h = h;

    // Draw box:
    vga_box(topleft_x, topleft_y, w, h);
    vga_printf_xy(topleft_x + (w/2) - (title_len/2), topleft_y + (h/2)-1, title);
}

void vga_popup_simple(int w, int h, char *title) {
    // Use last attribute VGA was configured as bg
    vga_popup(w, h, title, _color);
}

// Clear a popup, restoring video memory from before
void vga_popup_clear(void) {
    if (vga_use_highres_gui) {
        gui_popup_clear();
        return;
    }

    if (!popup_present) return;
    int i, j;
    for (i = cover_x; i < cover_x + cover_w; i++) {
        for (j = cover_y; j < cover_y + cover_h; j++) {
            int idx = (i + j * VGA_WIDTH);
            vga_setc(i,j,cover_buffer[2 * idx]);
            vga_setattr(i,j,cover_buffer[2 * idx + 1]);
        }
    }
    popup_present = false;
}

void vga_statusbar(int h, char attr) {
    int i;
    for (i = 0; i < VGA_WIDTH; i++) {
        vga_setattr(i, h, attr);
    }
}

// See: https://wiki.osdev.org/Text_Mode_Cursor
void vga_cursor_disable(void) {
    if (vga_use_highres_gui) {
        gui_cursor_disable();
        return;
    }
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_cursor_enable(void) {
    if (vga_use_highres_gui) {
        gui_cursor_enable();
        return;
    }

    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | CURSOR_TOP);

    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | CURSOR_BOTTOM);
}

// See: https://wiki.osdev.org/Text_Mode_Cursor
void vga_cursor_setxy(int x, int y) {
    if (vga_use_highres_gui) {
        gui_cursor_setxy(x,y,gui_vga_api_color.value);
        return;
    }

    uint16_t pos = y * VGA_WIDTH + x;
 
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void vga_clear_box (int x, int y, int w, int h) {
    int i, j;
    for (i = x; i < x+w; i++) {
        for (j = y; j < y+h; j++) {
            vga_setc(i,j,'\0');
        }
    }
}

