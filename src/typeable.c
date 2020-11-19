#include "typeable.h"
#include "vga.h"
#include "keymap.h"
#include "util.h"
#include "gui.h"

// The current typeable object (all keyboard interrupts write here):
typeable *current_typeable;

void set_current_typeable (typeable *t) {
    vga_set_cursor_term(t->x, t->y);
    current_typeable = t;
}

typeable *get_current_typeable (void) {
    return current_typeable;
}

// Create a typeable:
// All implementations of typeable should call this before doing any custom construction
void typeable_create(typeable *t) {
    if (!t) return;
    t->putc = typeable_putc_default;
    t->clear = typeable_clear_default;
    t->read = typeable_read_default;
    t->enter = typeable_enter_default;
    t->x = 0;
    t->y = 0;
    t->frame.x = 0;
    t->frame.y = 0;
    t->frame.w = VGA_WIDTH;
    t->frame.h = VGA_HEIGHT;
    t->buf[0] = 0;
    t->offset = 0;
}

// Destroy a typeable:
// All implementations of typeable should call this after doing their destruction work:
void typeable_destroy(typeable *t) {
    return;
}


// Helper method that returns the buffer index of a given typeable pointer:
#define get_buf_idx(tptr) (((tptr->x - tptr->frame.x) + ((tptr->y - tptr->frame.y) * t->frame.w)))

// Helper method that writes a character to the screen, as well as into the buffer
// This separate check is needed for the buffer since the buffer is only so big
// Typeables can be bigger than the buffer size so we need to be careful about that
// x and y are in SCREEN (VGA) space!
// NOTE: This method is capable of displaying characters on-screen that are NOT
// inserted into the buffer!
void _typeable_setc(typeable *t, char c_for_buf, char c_for_vga) {
    vga_setc(t->x, t->y, c_for_vga);

    uint32_t buffer_idx = get_buf_idx(t);
    if (buffer_idx < TYPEABLE_BUF_LEN) {
        t->buf[buffer_idx] = c_for_buf;
    }
}

// Return the real character in the buffer at the current position - 1:
char _typeable_getc_before(typeable *t) {
    uint32_t idx = get_buf_idx(t);
    return (idx != 0) ? t->buf[get_buf_idx(t)-1] : 0;
}

// Indent typeable to next line:
void _typeable_indent(typeable *t) {
    if (!t) return;

    uint32_t x_new = t->x;
    uint32_t y_new = t->y;
    if (t->y < t->frame.y + t->frame.h - 1) {
        x_new = t->frame.x;
        y_new = t->y+1;
    }

    // @TODO: Test this (these 3 lines used to be after the if statement)
    // Was moved during refactor to add cursor support to GUI terminals
    vga_cursor_setxy(x_new, y_new);
    if (x_new != t->x || y_new != t->y) {
        // If x new and y new are the same as the old values, we are at
        // the end of this typeable. Do not update character

        // Since one of them is different, update
        _typeable_setc(t, '\n', '\0');
    }
    t->x = x_new;
    t->y = y_new;
}

// Helper method that updates the screen and current position as well as buffer
// This allows for displaying a different character than is inserted into buffer
// (IE for password fields)
#define out_of_bounds() (((t->x >= t->frame.x + t->frame.w || t->y >= t->frame.y + t->frame.h)))
void _typeable_putc (typeable *t, char c_real, char c_display) {
    if (c_real == ASCII_BACKSPACE) {
        // Continually remove any \t's we find
        // Always remove at least 1 character
        // If we find a \t then remove more
        // @TODO: Use the _typeable_backspace from terminal.c in the future to be consistent
        //do {
            // At the top:
            if (t->x == t->frame.x && t->y == t->frame.y) {
                return;
            }

            if (t->x > t->frame.x) {
                t->x--;
            }
            else if (t->x == t->frame.x) {
                if (t->y > t->frame.y) {
                    t->x = t->frame.x + t->frame.w - 1;
                    t->y--;
                }
            }
            if (vga_use_highres_gui) {
                gui_cursor_disable();
            }
            vga_cursor_setxy(t->x, t->y);
            _typeable_setc(t, 0, '\0');
            if (vga_use_highres_gui) {
                gui_cursor_enable();
            }
        //} while (_typeable_getc_before(t) == '\t');
        return;
    }

    if (c_real == ASCII_TAB) {
        // Tabs not supported for now
        return;
    }

    if (c_real == ASCII_RETURN) {
        if (t->reading) {
            // If we are reading, then this is the end of that
            t->enter(t);
        }
        else {
            // Otherwise this is a \n from some command sequence, indent!
            _typeable_indent(t);
        }
        return;
    }

    // Restrict typing to within the bounds of the frame
    if (out_of_bounds()) {
        return;
    }

    // Update cursor with next valid position:
    uint32_t x_new = t->x;
    uint32_t y_new = t->y;
    if (t->x < t->frame.x + t->frame.w - 1) {
        x_new = t->x+1;
    }
    else if (t->x == t->frame.x + t->frame.w - 1) {
        if (t->y < t->frame.y + t->frame.h - 1) {
            x_new = t->frame.x;
            y_new = t->y+1;
        }
    }

    vga_cursor_setxy(x_new, y_new);

    if (x_new != t->x || y_new != t->y) {
        // If x new and y new are the same as the old values, we are at
        // the end of this typeable. Do not update character

        // Since one of them is different, update
        _typeable_setc(t, c_real, c_display);
    }

    t->x = x_new;
    t->y = y_new;

    return;
}

// Default handlers (probably useful for most cases):
void typeable_putc_default (typeable *t, char c) {
    _typeable_putc(t, c, c);
}

void typeable_putc_private (typeable *t, char c) {
    _typeable_putc(t, c, '*');
}

void typeable_clear_default (typeable *t) {
    t->x = t->frame.x;
    t->y = t->frame.y;
    t->buf[0] = '\0';
    vga_clear_box (t->frame.x, t->frame.y, t->frame.w, t->frame.h);
    vga_set_cursor_term(t->x, t->y);
    return;
}

size_t typeable_read_default (typeable *t, char *readbuf, size_t bytes_to_read) {
    size_t bytes_to_copy;

    t->reading = 1;
    while (t->reading) { asm volatile("hlt"); /* Nicely wait for next interrupt */ }
    // User has pressed enter, so put buffer into readbuf
    bytes_to_copy = bytes_to_read;
    if (bytes_to_copy > TYPEABLE_BUF_LEN) { bytes_to_copy = TYPEABLE_BUF_LEN; }

    // bytes_to_copy is now the absolute max we can copy
    uint32_t buf_idx_max = get_buf_idx(t) + 1;
    if (bytes_to_copy > buf_idx_max) { bytes_to_copy = buf_idx_max; }

    return strncpy(readbuf, t->buf, bytes_to_copy);
}

// By default just clear the reading flag
void typeable_enter_default (typeable *t) {
    t->reading = 0;
}

// For text areas, we want return to move the cursor down a line
void typeable_enter_textarea (typeable *t) {
    int tmp = 0;
    if (t->y < t->frame.y + t->frame.h) {
        for (tmp = t->x; tmp < t->frame.x + t->frame.w; tmp++) {
            // Set all remaining bits in the buffer to '\t' so as to not
            // introduce undefined memory into the valid buffer!
            // We can even use \t later as a "this line stopped here"
            t->x = tmp;
            _typeable_setc(t, '\t', '\0');
        }
        t->x = t->frame.x;
        t->y++;
        vga_set_cursor_term(t->x, t->y);
    }
}

// Global versions that operate on current_typeable:
void current_typeable_putc (char c) {
    if (NULL == current_typeable->putc) { return; }
    current_typeable->putc(current_typeable, c);
}

void current_typeable_clear (void) {
    if (NULL == current_typeable->clear) { return; }
    current_typeable->clear(current_typeable);
}

size_t current_typeable_read (char *readbuf, size_t bytes_to_read) {
    if (NULL == current_typeable->read) { return 0; }
    return current_typeable->read(current_typeable, readbuf, bytes_to_read);
}

void current_typeable_enter (void) {
    if (NULL == current_typeable->enter) { return; }
    current_typeable->enter(current_typeable);
}

void current_typeable_break (void) {
    if (NULL == current_typeable->send_break) { return; }
    current_typeable->send_break(current_typeable);
}

// @TODO: Make this support more things
void current_typeable_printf(char *str, ...) {
    char *cursor = str;
    while (*cursor) {
        current_typeable_putc(*cursor);
        cursor++;
    }
}

// Clear input stream leaving screen contents as they are
void current_typeable_flush_input (void) {
    if (!current_typeable) return;
    memset(current_typeable->buf, '\0', sizeof(current_typeable->buf));
}

