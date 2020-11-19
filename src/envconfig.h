#ifndef ENVCONFIG_H
#define ENVCONFIG_H
#include "types.h"

// Environment or terminal configuration variables
// These are adjusted via the termconfig syscall

// OPTION 0: HIDE_CHARS
// Print characters as '*'s?
// 0 = no, anything else = yes

#define NUM_ENV_VARS ((2))

typedef struct env_var {
    uint32_t value;
} env_var;

// Array of system configurations:
extern env_var env_vars[NUM_ENV_VARS];

// Named system configuration variables:
#define ENV_CONFIG_HIDE_CHARS_IDX ((0))
#define ENV_CONFIG_HIDE_CHARS() ((env_vars[ENV_CONFIG_HIDE_CHARS_IDX].value))

#define ENV_CONFIG_CLEAR_SCREEN ((1))

#endif
