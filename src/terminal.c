#include "terminal.h"
#include "keymap.h"
#include "vga.h"
#include "util.h"
#include "envconfig.h"
#include "gui.h"

// Terminals are the typeable associated with standard IO
// They "inherit" from typeable and use the typeable buffer as the read buffer
// They maintain a separate "screen buffer" containing characters & attributes
// Their size is defined at object creation time using the CREATE_TERMINAL macro.

/*
 * _terminal_draw_char
 *
 * Output a character at the given position in a terminal's screen buffer.
 * Also draws into VGA memory.
 *
 * Position (x,y) is in terminal position relative (frame-relative), NOT absolute positioning like
 * in a regular typeable.
 *
 * Does NOT affect terminal's typeable read buffer!
 */
void _terminal_draw_char (terminal *term, char c, uint32_t x, uint32_t y) {
    // Write into terminal screen buffer:
    uint32_t pos = (term->screen_w * y) + x;
    if (pos >= (term->screen_w * term->screen_h)) {
        // Outside of screen buffer bounds!
        return;
    }

    term->screen_buf[pos] = c;

    if (!term->scroll_inhibited) {
        if (vga_use_highres_gui) {
            // Disable the cursor before printing here
            // This is because _terminal_increment will call cursor set XY and erase what we just drew
            gui_cursor_disable();
        }
    	vga_setc(x + term->typeable.frame.x, y + term->typeable.frame.y, c);
    }
}

/*
 * terminal_repaint
 *
 * Force a repaint of a region of a given terminal.
 *
 * [line_start, line_end] inclusive will be repainted
 */
void _terminal_repaint_region (terminal *term, uint32_t line_start, uint32_t line_end) {
    if (!term->visible) return;

    // We clear the terminal before calling repaint
    // if (vga_use_highres_gui) {
    //     // Just clear the entire terminal first
    //     gui_erase_text_region(term->typeable.frame.x, term->typeable.frame.y, term->typeable.frame.w, term->typeable.frame.h);
    // }

    if (line_end < line_start) return;
    if (line_end >= term->screen_h || line_start >= term->screen_h) return;

    uint32_t x, y;
    for (x = 0; x < term->screen_w; x++) {
        for (y = line_start; y <= line_end; y++) {
            vga_setc(x + term->typeable.frame.x,
                y + term->typeable.frame.y,
                term->screen_buf[x + (term->screen_w) * y]);
        }
    }
}

/*
 * terminal_repaint
 *
 * Force a repaint of a given terminal.
 */
void terminal_repaint (terminal *term) {
    _terminal_repaint_region(term, 0, term->screen_h - 1);
}

void _terminal_gui_clear (terminal *term) {
    // Option 1:
    // uint32_t x, y;
    // for (x = 0; x < term->screen_w; x++) {
    //     for (y = 0; y < term->screen_h; y++) {
    //         char bufchar = term->screen_buf[x + (term->screen_w) * y];
    //         vga_setc(x + term->typeable.frame.x,
    //             y + term->typeable.frame.y,
    //             ' ');
    //     }
    // }

    // Option 2:
    // gui_erase_text_region(term->typeable.frame.x, term->typeable.frame.y, term->typeable.frame.w, term->typeable.frame.h);

    // Option 3:
    // @TODO: Make this read actual terminal size
    gui_erase_region(35, 35, SCREEN_WIDTH - 70, SCREEN_HEIGHT - 70);
}

// Start/ stop long writes
// start write sets a flag that prevents the terminal from scrolling,
// stop write clears that flag and scrolls as necessary
void terminal_start_write(terminal *t) {
    if (!t) return;
    t->scroll_count = 0;
    t->scroll_inhibited = true;
    t->scroll_start_line = t->typeable.y;
    gui_cursor_disable();
}

void terminal_stop_write(terminal *t) {
    t->scroll_inhibited = false;

    // If using high res GUI, disable cursor before writing
    // So that we don't overwrite anything we just drew to the screen when we enable it
    if (vga_use_highres_gui) {
        gui_cursor_disable();
    }

    if (t->scroll_count != 0) {
        _terminal_gui_clear(t);
        terminal_repaint(t);

        // @TODO: If necessary, use scroll count field to optimize repaint
    }
    else {
    	// We didn't scroll, just redraw the newly written region
    	_terminal_repaint_region(t, t->scroll_start_line, t->typeable.y);
    }

    // Draw cursor:
    if (t->visible) {
        if (vga_use_highres_gui) {
            gui_cursor_enable();
        }
        vga_cursor_setxy(t->typeable.x + t->typeable.frame.x,
                         t->typeable.y + t->typeable.frame.y);
    }

    t->scroll_count = 0;
}

/*
 * _terminal_scroll_n_lines
 *
 * Scrolls the terminal up n lines
 *
 * Will update typeable Y and Y_read positions! Also changes screen buf! And will cause a repaint.
 *
 * This WILL recalculate the Y read base if currently reading,
 * and if visible, this will update VGA cursor position.
 */
void _terminal_scroll_n_lines (terminal *term, uint32_t n) {
    // If scrolling is inhibited, don't scroll, but increment scroll counter
    if (term->scroll_inhibited) {
        term->scroll_count++;
    }

    // If in high res mode, need to clear terminal screen:
    if (vga_use_highres_gui && !term->scroll_inhibited) {
        _terminal_gui_clear(term);
    }

    uint32_t line_idx = 0;
    if (n < 0 || n == 0 || n >= term->screen_h) return;
    for (line_idx = n; line_idx < term->screen_h; line_idx++) {
        memcpy(&(term->screen_buf[term->screen_w * (line_idx-n)]),
               &(term->screen_buf[term->screen_w * (line_idx)]),
               term->screen_w);
    }
    
    // Clear last n lines:
    for (line_idx = 0; line_idx < n; line_idx++) {
        uint32_t line_to_clear = (term->screen_h - 1) - line_idx;
        memset(&(term->screen_buf[term->screen_w * line_to_clear]),
           '\0',
           term->screen_w);
    }

    if (term->typeable.reading) {
        // If we are reading, we need to set y_read accordingly
        if (term->typeable.y_read < n) { term->typeable.y_read = 0; }
        else { term->typeable.y_read -= n; }
    }
    // Update Y position
    if (term->typeable.y < n) { term->typeable.y = 0; }
    else { term->typeable.y -= n; }

    // If we are visible, draw cursor:
    if (term->visible) {
        if (vga_use_highres_gui) {
            gui_cursor_enable();
        }

        vga_cursor_setxy(term->typeable.x + term->typeable.frame.x, 
                         term->typeable.y + term->typeable.frame.y);
    }
    
    if ((vga_use_highres_gui && !term->scroll_inhibited) || !vga_use_highres_gui) {
        // Only repaint every line if this isn't high res
        // Otherwise, we wait for next read to repaint
        terminal_repaint(term);
    }
}

/*
 * _terminal_scroll_up
 *
 * Scrolls the terminal up 1 line
 *
 * Does not update typeable X and Y positions! Only affects screen buf! And will cause a repaint.
 */
void _terminal_scroll_up (terminal *term) {
    _terminal_scroll_n_lines(term, 1);
}

/*
 * _terminal_readbuf_increment
 *
 * Move the read buffer forward 1 character
 *
 * This does not affect the screen at all
 */
void _terminal_readbuf_increment (terminal *term) {
    // Check stop conditions:
    if (term->typeable.reading) {
        // If we are reading, then only increment if the offset is less than TYPEABLE_BUF_LEN
        if (term->typeable.offset >= TYPEABLE_BUF_LEN) { return; }
        term->typeable.offset++;
    }
}

/*
 * _terminal_increment
 *
 * Move a terminal a character forward.
 *
 * If the screen is full, this will move everything up a line
 */
void _terminal_increment (terminal *term) {
    _terminal_readbuf_increment(term);

    // Check if we need to scroll up a line:
    if (term->typeable.x == term->typeable.frame.w - 1 && term->typeable.y == term->typeable.frame.h - 1) {
        _terminal_scroll_up(term);
    }

    // Move x and y forward as necessary
    if (term->typeable.x < term->typeable.frame.w - 1) {
        term->typeable.x++;
    }
    else {
        if (term->typeable.y < term->typeable.frame.h - 1) {
            term->typeable.x = 0;
            term->typeable.y++;
        }
    }

    // If we are visible, draw cursor:
    if (term->visible) {
        vga_cursor_setxy(term->typeable.x + term->typeable.frame.x, 
                         term->typeable.y + term->typeable.frame.y);
        if (vga_use_highres_gui) {
            gui_cursor_enable();
        }
    }
}

/*
 * _terminal_increment
 *
 * Move to the next line if possible
 *
 * Otherwise, this will trigger a scroll and repaint
 */
void _terminal_indent (terminal *term) {
    // Check if we need to scroll up a line:
    if (term->typeable.y == term->typeable.frame.h - 1) {
        _terminal_scroll_up(term);
    }
    if (term->typeable.y < term->typeable.frame.h) {
        term->typeable.x = 0;
        term->typeable.y++;
    }

    // If we are visible, draw cursor:
    if (term->visible) {
        vga_cursor_setxy(term->typeable.x + term->typeable.frame.x, 
                         term->typeable.y + term->typeable.frame.y);
    }
}

/*
 * _terminal_backspace
 *
 * Handle a backspace of a terminal.
 *
 * Returns true if we moved back, false if otherwise
 */
bool _terminal_backspace (terminal *term) {
    // Coordinates to stop backspacing at:
    // Remember, terminals use frame-relative positioning, so (0,0) is the top left of the typeable frame
    uint32_t smallest_x = term->typeable.reading ? term->typeable.x_read : 0;
    uint32_t smallest_y = term->typeable.reading ? term->typeable.y_read : 0;

    typeable *t = &term->typeable;

    // First, handle typeable read buffer & offset:
    if (t->reading) {
        if (term->typeable.offset > 0) { term->typeable.offset--; }
        term->typeable.buf[term->typeable.offset] = '\0';
    }

    if (t->x <= smallest_x && t->y <= smallest_y) {
        // Illegal position
        return false;
    }

    if (t->x > smallest_x) {
        t->x--;
    }
    else if (t->x == smallest_x) {
        if (t->y > smallest_y) {
            t->x = t->frame.w - 1;
            t->y--;
        }
        else {
            // Illegal position (shouldn't get here, but for safety, return anyways if we do)
            return false;
        }
    }
    else {
        // Illegal position (shouldn't get here, but for safety, return anyways if we do)
        return false;
    }

    if (term->visible && vga_use_highres_gui) {
        gui_cursor_disable();
    }

    _terminal_draw_char(term, '\0', term->typeable.x, term->typeable.y);

    // If we are visible, draw cursor:
    if (term->visible) {
        vga_cursor_setxy(term->typeable.x + term->typeable.frame.x, 
                         term->typeable.y + term->typeable.frame.y);
        if (vga_use_highres_gui) {
            gui_cursor_enable();
        }
    }

    // We moved backwards a space, so return true:
    return true;
}

/*
 * terminal_putc
 *
 * Recieve a single character of input. This could be from a process or
 * from a keyboard. If the typeable reading flag is enabled, then we should
 * read into the buffer. Otherwise, just display it on screen.
 *
 * WARNING: Do not use this as a method on any typeable that isn't a terminal!
 * That will cause issues, as this assumes this is a terminal (inheriting from typeable)
 */
void terminal_putc (typeable *t, char c) {
    terminal *term = (terminal *)t;

    // Handle special chars:
    switch (c) {
        case TERM_CONTROL_CODE:
        term->expecting_control_char = true;
        return;
        break;

        case TERM_CLEAR_SCREEN:
        // Special code to clear screen
        terminal_clear(t);
        return;
        break;

        case ASCII_BACKSPACE:
        _terminal_backspace(term);
        return;
        break;

        case ASCII_RETURN:
        // If reading, set t->reading to false
        // Otherwise add a newline to screen buffer
        if (t->reading) {
            t->reading = false;
        }
        _terminal_indent(term);
        return;
        break;

        case ASCII_TAB:
        // Insert 4 spaces
        break;
    }

    // Handle control character
    if (term->expecting_control_char) {
        // @TODO: Whatever we do here
        term->expecting_control_char = false;
        return;
    }

    // Update read buffer as needed:
    if (t->reading) {
        // Insert to typeable buffer at offset
        if (t->offset < TYPEABLE_BUF_LEN) {
            t->buf[t->offset] = c;

            // Offset will be updated at call to _terminal_increment
            //t->offset++; <- This happens in _terminal_increment
        }
        else {
            // Can't insert this character (buffer full)
            return;
        }
    }

    // Handle updating the screen:
    {
        if (t->reading && ENV_CONFIG_HIDE_CHARS()) {
            // If we are reading, and the env_var ENV_VAR_HIDE_CHARS is set,
            // we should change 'c' to be '*' to hide the character from the screen
            // (its real value is already in the buffer)
            //c = '*';

            // Even better, just call _terminal_readbuf_increment and exit (only put char into readbuf and not screen):
            _terminal_readbuf_increment(term);
            return;
        }

        // Regardless of whether we are reading or not, output char to screen
        // Cast typeable pointer to a terminal pointer (C style inheritance)
        _terminal_draw_char(term, c, term->typeable.x, term->typeable.y);

        // Move to next screen position:
        // (This also calls _terminal_readbuf_increment to increment the read buffer as well as update the screen cursor)
        _terminal_increment(term);
    }
}

void terminal_enter (typeable *t) {
    // Same thing as just writing \n into a terminal
    // Terminal takes care of indentation logic in putc
    terminal_putc(t, '\n');
}

void terminal_clear (typeable *t) {
    // Keep backspacing until we can't
    terminal *term = (terminal *)t;
    while (_terminal_backspace(term));

    if (vga_use_highres_gui) {
        _terminal_gui_clear(term);
    }

    // Clear screen and move start line to top
    _terminal_scroll_n_lines(term, term->typeable.y_read);

    if (vga_use_highres_gui) {
        terminal_repaint(term);
    }

    term->typeable.y_read = 0;
    term->typeable.y = 0;

    t->offset = 0;
    t->buf[0] = '\0';
    //memset(t->buf, '\0', sizeof(t->buf)); // <- This isn't needed with proper bounds checking in terminal_read
}

size_t terminal_read (typeable *t, char *readbuf, size_t bytes_to_read) {
    size_t bytes_to_copy;

    t->x_read = t->x;
    t->y_read = t->y;
    t->offset = 0;
    memset(t->buf, '\0', sizeof(t->buf));
    t->reading = true;

    while (t->reading) { asm volatile ("hlt"); /* Gracefully wait for interrupt. Could also use scheduler_pass here */ }

    // User has pressed enter, so put buffer into readbuf
    bytes_to_copy = bytes_to_read;
    if (bytes_to_copy > TYPEABLE_BUF_LEN) { bytes_to_copy = TYPEABLE_BUF_LEN; }
    if (bytes_to_copy > t->offset + 1) { bytes_to_copy = t->offset + 1; }
    return strncpy(readbuf, t->buf, bytes_to_copy);
}

// Control + C handler (break current process or read)
void terminal_break (typeable *t) {
    return;
}

void set_active_terminal (terminal *t) {
    set_current_typeable(&t->typeable);
    t->visible = true;
}
