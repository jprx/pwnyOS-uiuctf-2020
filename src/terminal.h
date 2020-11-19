#ifndef TERMINAL_H
#define TERMINAL_H
#include "types.h"
#include "vga.h"
#include "typeable.h"

// ESC code:
#define TERM_CONTROL_CODE ((0x1B))
#define TERM_CLEAR_SCREEN ((0x18)) // ASCII for "Cancel"

typedef struct terminal {
    typeable typeable;

    // Copy of screen memory for this terminal:
    char *screen_buf;

    // Dimensions of the screen buf array (setup at creation time):
    const uint32_t screen_w, screen_h;

    // Is the next character supposed to be a control code?
    // If we receive code TERM_CONTROL_CODE this is set to true
    // Next char is treated as a control char
    bool expecting_control_char;

    // Are we visible? Disabling this disables all VGA operations
    // (Even when this is disabled, writes to the screen update screen_buf)
    bool visible;

    // Scroll inhibited flag:
    // This is set during writes from stdio to prevent lag
    bool scroll_inhibited;
    size_t scroll_count;
    size_t scroll_start_line; // The line we were on when terminal_start_write was called
} terminal;

/*
 * CREATE_TERMINAL
 *
 * This is how terminal objects should be created.
 * Keep in mind that this defines TWO new variables:
 *     terminal (name)
 *     char _(name)_screenbuf
 *
 * If you are using this dynamically, do not forget about the screen buffer that is allocated
 * This buffer is allocated separately from the terminal, and a pointer to it is maintained.
 *
 * This is used for creating terminals of variable size at creation.
 *
 * NOTE: UNLIKE TYPEABLES, terminals use the typeable.x and y fields as frame-relative positions,
 * as opposed to absolute position. This is to facilitate using terminals in different screen resolutions later.
 */
#define CREATE_TERMINAL_XY(name, x_in, y_in, w_in, h_in)    \
    char __screenbuf_##name [((w_in)) * ((h_in))] = {0};    \
    terminal name = {                                       \
        .typeable = {                                       \
            .putc=terminal_putc,                            \
            .clear=terminal_clear,                          \
            .read=terminal_read,                            \
            .enter=terminal_enter,                          \
            .send_break=terminal_break,                     \
            .x=0,                                           \
            .y=0,                                           \
            .frame = {                                      \
                .x = ((x_in)),                              \
                .y = ((y_in)),                              \
                .w = ((w_in)),                              \
                .h = ((h_in)),                              \
            },                                              \
            .buf[0] = 0,                                    \
            .offset = 0,                                    \
            .x_read = 0,                                    \
            .y_read = 0,                                    \
        },                                                  \
        .screen_buf = __screenbuf_##name,                   \
        .screen_w = ((w_in)),                               \
        .screen_h = ((h_in)),                               \
        .expecting_control_char = false,                    \
        .visible = false,                                   \
        .scroll_inhibited = false,                          \
        .scroll_count = 0,                                  \
        .scroll_start_line = 0,                             \
    };

#define CREATE_TERMINAL(name, w_in, h_in)                   \
    CREATE_TERMINAL_XY(name,                                \
        (VGA_WIDTH / 2) - (((w_in)) / 2),                   \
        (VGA_HEIGHT / 2) - (((h_in)) / 2),                  \
        w_in,                                               \
        h_in);                                              \

// A terminal is a kind of typeable specially designed for terminal things
// These methods implement the "typeable" interface, and then just convert the typeable* to terminal*
// Since all terminals start with a typeable this works. 
// WARNING! Undefined behavior if these are called with non-terminal typeables.
void terminal_putc (typeable *t, char c);
void terminal_enter (typeable *t);
void terminal_clear (typeable *t);
size_t terminal_read (typeable *t, char *readbuf, size_t bytes_to_read);
void terminal_break (typeable *t);

// Start/ stop long writes
// start write sets a flag that prevents the terminal from scrolling,
// stop write clears that flag and scrolls as necessary
void terminal_start_write(terminal *t);
void terminal_stop_write(terminal *t);

// Sets terminal as the active/ main terminal
// Also writes into current typeable!
void set_active_terminal (terminal *t);

/*
 * terminal_repaint
 *
 * Force a repaint of a given terminal.
 */
void terminal_repaint (terminal *term);

#endif