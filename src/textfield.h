#ifndef TEXTFIELD_H
#define TEXTFIELD_H

#include "typeable.h"

// 64 chars max
#define TEXTFIELD_LEN ((64))

/*
 * textfield
 * A textfield is a kernel structure used to represent any
 * input fields in the interface that users can type into.
 *
 * It "extends" the 'typeable' struct.
 */

typedef struct textfield_struct_t {
    typeable typeable;
    char buffer[TEXTFIELD_LEN];
} textfield;

// Initialize a textfield:
void create_textfield(textfield *tf);

// Destroy a textfield:
void destroy_textfield(textfield *tf);

#endif