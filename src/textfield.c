#include "textfield.h"

// Typeable methods for textfields:
void textfield_putc(char c);
void textfield_clear(void);
size_t textfield_read(char *readbuf, size_t bytes_to_read);

void create_textfield(textfield *tf) {
    if (!tf) return;
    // tf->typeable.putc = textfield_putc;
    // tf->typeable.clear = textfield_clear;
    // tf->typeable.read = textfield_read;
}

void destroy_textfield(textfield *tf) {
    // Clear any dynamic memory here
    return;
}

void textfield_putc(char c) {
    return;
}

void textfield_clear(void) {
    return;
}

size_t textfield_read(char *readbuf, size_t bytes_to_read) {
    return 0;
}
