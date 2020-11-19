#include "keyboard.h"
#include "util.h"
#include "vga.h"
#include "interrupt.h"
#include "keymap.h"
#include "typeable.h"
#include "rtc.h"
#include "system.h"

#define NUM_PS2_CODES 256

#define PS2_RELEASE_VECTOR 0x80

// Switch statement used to remap shifted keys:
// I- the character to map from (non-shifted)
// O- the character to map to
// sym- the variable to assign mapped code to
#define remap(I, O, sym) case I: sym = O; break;

/*
 * test_key
 *
 * Assign a special variable if special key is hit
 * Afterwords goto KEYBOARD_END immediately to skip the
 * printable character handling logic.
 *
 * CHECK_CODE - the PS2 keycode to match
 * action- the variable to store true/ false into
 * kcode- the keycode to test
 */
#define test_key(CHECK_CODE, action, kcode) do {                        \
    if ((kcode & ~PS2_RELEASE_VECTOR) == CHECK_CODE) {                  \
        action = ((kcode & PS2_RELEASE_VECTOR) == 0) ? true : false;    \
        goto KEYBOARD_END;                                              \
    }                                                                   \
} while(0);

// Non-printable characters are set to 0 here
static char ps2_to_ascii[NUM_PS2_CODES] = {
    // PS2 code 0:
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',

    // 0x08 = backspace
    '-', '=', ASCII_BACKSPACE, ASCII_TAB,
    // PS2 code 0x10:
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 
    '[', ']', ASCII_RETURN, 0,
    'A', 'S', 'D', 'F',  'G', 'H', 'J', 'K', 'L', 
    ';', '\'', '`', 0, '\\',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', 
    ',', '.', '/', 0, '*', 0, ' '
};

// Returns true if this char is in the A-Z range of ASCII
static inline bool is_ascii_alpha(char c) {
    return ((c >= 0x41) && (c < (0x41 + 26)));
}

// Any characters held down
static bool chars_held[NUM_PS2_CODES];

static bool lshift = false;
static bool rshift = false;
static bool control = false;
static bool option = false; // 'Alt'
static uint8_t caps_lock_state = 0;
static bool caps_lock = false;
static bool escape = false;

void init_keyboard(void) {
    int tmp;

    for (tmp = 0; tmp < NUM_PS2_CODES; tmp++) {
        chars_held[tmp] = false;
    }

    // Poll status register
    while (inb(0x64) & 0x01) { tmp = inb(0x60); }
    outb(0x64, 0xd0);
    inb(0x60);

    outb(0x64, 0x20);
    tmp = inb(0x60);
    outb(0x64, 0x60);
    outb(0x60, tmp | 0x01 | 0x40);
    outb(0x64, 0xAE);
}

void keyboard_handler(void) {
    uint8_t keycode;
    uint32_t flags = cli_and_save();

    keycode = inb(0x60);

    ticks_since_kb = 0;
    hw_pic_eoi(IRQ_KEYBOARD);

    if (keycode == 0x61) {
        // Initialization code, apparently
        // Ignore this one
        goto KEYBOARD_END;
    }

    // If any of these keys are pressed, we immediately
    // go to KEYBOARD_END!!!
    test_key(PS2_LSHIFT, lshift, keycode);
    test_key(PS2_RSHIFT, rshift, keycode);
    test_key(PS2_LCONTROL, control, keycode);
    test_key(PS2_LOPTION, option, keycode);
    test_key(PS2_ESCAPE, escape, keycode);

    // This one is weird
    // Empirically tested and it works
    // On macOS Caps Lock sends 2 release packets instead of
    // a pressed and released packet
    // So use this to count every caps lock release packet, and every
    // other packet we flip caps lock (since 2 packets per keypress)
    if (keycode == PS2_CAPSLOCK_RELEASE) {
        caps_lock_state++;
        caps_lock = (caps_lock_state & 0x2);
        if (caps_lock_state > 3) caps_lock_state = 0;
        goto KEYBOARD_END;
    }

    // *****************
    // Take care of released keys
    // *****************

    if (keycode & 0x80) {
        // Key released
        chars_held[keycode] = false;

        // Nothing else to do here
        goto KEYBOARD_END;
    }
    else {
        chars_held[keycode] = true;
    }

    // *****************
    // Translate to ASCII
    // *****************

    if (keycode <= 0x39) {
        keycode = ps2_to_ascii[keycode];
    }
    else {
        // Unsupported (yet) keycode
        goto KEYBOARD_END;
    }

    // *****************
    // END Special key code handling
    // At this point, keycode is ASCII and is a printable character
    // *****************
    // BEGIN key combination handling
    // *****************

    if (control && (keycode == 'L')) {
        // Clear screen
        sti(); // In case we get a key release while clearing the screen
        current_typeable_clear();
        goto KEYBOARD_END;
    }

    if (control && (keycode == 'X')) {
        reboot();
    }

    // if (control && (keycode == 'P')) {
    //     *(int32_t *)0xdeadc0de = 0;
    //     goto KEYBOARD_END;
    // }

    // if (control && (keycode == 'C')) {
    //     current_typeable_break();
    //     goto KEYBOARD_END;
    // }

    if (keycode == '\n') {
        current_typeable_enter();
        goto KEYBOARD_END;
    }

    // *****************
    // End Key combo handling
    // Take care of upper/ lower and shift conditions here:
    // *****************

    bool should_print_caps = ((lshift && !caps_lock) || (rshift && !caps_lock) || (!lshift && !rshift && caps_lock));
    if (!should_print_caps) {
        if (is_ascii_alpha(keycode)) {
            // Make it lowercase
            keycode = keycode | 0x20;
        }
    }

    // Post-translation to ASCII so we can use chars here
    // Map keys to their shifted versions
    if (lshift || rshift) {
        switch (keycode) {
            // remap is a macro for case CHAR1: keycode = CHAR2; break;
            remap('\\', '|', keycode);
            remap('[', '{', keycode);
            remap(']', '}', keycode);
            remap(',', '<', keycode);
            remap('.', '>', keycode);
            remap('/', '?', keycode);
            remap('\'', '"', keycode);
            remap('1', '!', keycode);
            remap('2', '@', keycode);
            remap('3', '#', keycode);
            remap('4', '$', keycode);
            remap('5', '%', keycode);
            remap('6', '^', keycode);
            remap('7', '&', keycode);
            remap('8', '*', keycode);
            remap('9', '(', keycode);
            remap('0', ')', keycode);
            remap('-', '_', keycode);
            remap('=', '+', keycode);
            remap('`', '~', keycode);
            remap(';', ':', keycode);
        }
    }

    current_typeable_putc(keycode);

KEYBOARD_END:
    // In case any new keys came in, flush the buffer
    while (inb(0x64) & 0x01) { inb(0x60); }

    restore_flags(flags);
}