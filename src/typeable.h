#ifndef TYPEABLE_H
#define TYPEABLE_H

#include "types.h"

// 256 chars max
#define TYPEABLE_BUF_LEN ((256))

/* Typeables
 * A typeable is a target that can be typed into
 * This can be anything, from a terminal, to a text field in the GUI
 *
 * Each typeable "inherits" from the typeable struct, that contains
 * a buffer for keyboard interrupts to write into, and a function pointer
 * for what to do when a keypress is incoming (typically this is insert it
 * into the buffer, but could be something else), and a function pointer
 * for when the buffer is cleared, as well as when the user types enter.
 *
 * Structs implementing typeable should define their own buffering schemes.
 */

struct typeable_struct_t;

// Write a character into the typeable:
typedef void (*typeable_putc_t)(struct typeable_struct_t *t, char c);

// Clear this typeable's buffer (user typed "option + L")
typedef void (*typeable_clear_t)(struct typeable_struct_t *t);

// Blocking read (blocks until user hits 'enter' as typeables are line-buffered):
// [Returns bytes read]
typedef size_t (*typeable_read_t)(struct typeable_struct_t *t, char *readbuf, size_t bytes_to_read);

// User pressed enter while this is the active typeable
// By default this clears the blocking read flag, but can do other things too!
typedef void (*typeable_enter_t)(struct typeable_struct_t *t);

// We received "control + c" (break code)
typedef void (*typeable_break_t)(struct typeable_struct_t *t);

// @TODO: Consolidate break, enter, and clear into 1 method? ("Special char" method)

typedef struct typeable_struct_t {
    // Write a character into the typeable:
    typeable_putc_t putc;

    // Clear this typeable's buffer (user typed "option + L")
    typeable_clear_t clear;

    // Blocking read (blocks until user hits 'enter' as typeables are line-buffered):
    // [Returns bytes read]
    typeable_read_t read;

    // User pressed enter while this is the active typeable
    // By default this clears the blocking read flag, but can do other things too!
    typeable_enter_t enter;

    // Send break code:
    typeable_break_t send_break;

    // Position on screen:
    int x;
    int y;

    // Window frame:
    struct {
        int x;
        int y;
        int w;
        int h;
    } frame;

    // Buffer (used by default methods):
    char buf[TYPEABLE_BUF_LEN];

    // Buffer offset (used by alternative typeables where input is linear)
    uint32_t offset;

    // Start position of most recent user input stream
    uint32_t x_read, y_read;

    // Blocking read
    // Cleared by keyboard interrupt when 'return' is pressed
    volatile bool reading;
} typeable;

// The current typeable object (all keyboard interrupts write here):
extern typeable *current_typeable;

// Sets the current typeable, updating VGA cursor and stuff
void set_current_typeable (typeable *t);
typeable *get_current_typeable (void);

// Global methods for operating on the current typeable:
void current_typeable_putc (char c);
void current_typeable_clear (void);
size_t current_typeable_read (char *readbuf, size_t bytes_to_read);
void current_typeable_enter (void);
void current_typeable_break (void);

// Write a formatted string into a typeable (repeatedly calls putc):
// This is quite slow
void current_typeable_printf(char *str, ...);

// Constructor and destructor:
void typeable_create(typeable *t);
void typeable_destroy(typeable *t);

// Default implementations of all typeable "virtual" functions:
// Plus their provided different ones
void typeable_putc_default (typeable *t, char c);
void typeable_putc_private (typeable *t, char c);

void typeable_clear_default (typeable *t);

size_t typeable_read_default (typeable *t, char *readbuf, size_t bytes_to_read);

void typeable_enter_default (typeable *t);
void typeable_enter_textarea (typeable *t);

// Flush input on a typeable without modifying screen contents:
void current_typeable_flush_input (void);

#endif
