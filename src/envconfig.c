#include "envconfig.h"
#include "typeable.h"

// Array of system configurations:
env_var env_vars[NUM_ENV_VARS];

/*
 * sys_termconfig (Terminal Config)
 *
 * Configure a terminal or environment-specific variable.
 * Example: screen color, cursor on/ off, whether characters print as *'s or not, etc.
 */
size_t sys_envconfig(uint32_t option_code, uint32_t option_value) {
    if (option_code < NUM_ENV_VARS) {
        if (option_code == ENV_CONFIG_CLEAR_SCREEN) {
            current_typeable_clear();
        }
        else {
            env_vars[option_code].value = option_value;
        }
        return 0;
    }

    // Option not found:
    return -1;
}