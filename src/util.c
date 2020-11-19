// Utilities for jprx toy OS
#include "util.h"
#include "process.h"

// Helper internal methods using string methods in assembly:
void _memset_byte(char *fbuf, char charcpy, size_t sizecpy);
void _memset_long(char *fbuf, uint32_t valcpy, size_t sizecpy);
void _memcpy_byte(uint8_t *to, uint8_t *from, size_t max_bytes);
void _memcpy_long(uint32_t *to, uint32_t *from, size_t max_integers);

int strlen(char *str) {
	int i = 0;
	while (*str) {
		str++;
		i++;
	}
	return i;
}

//@TODO: Use x86 string instructions instead
size_t strncpy(char *to, const char *from, size_t max_bytes) {
    if (max_bytes == 0) return 0;
    
    size_t i = 0;
    while (i < max_bytes - 1) {
        to[i] = from[i];
        if (from[i] == '\0') return i;
        i++;
    }
    // Insert null terminator:
    to[max_bytes-1] = '\0';
    return i;
}

size_t memcpy(void *to, void *from, size_t max_bytes) {
    if (max_bytes == 0) return 0;

    _memcpy_byte((uint8_t*)to, (uint8_t*)from, max_bytes);
    return max_bytes;
}

// Memcpy a bunch of integers
size_t memcpyl(uint32_t *to, uint32_t *from, size_t max_integers) {
    if (max_integers == 0) return 0;
    _memcpy_long(to, from, max_integers);
    return max_integers;
}

size_t memset(char *to, char val, size_t max_bytes) {
    if (max_bytes == 0) return 0;
    // Use hardware accelerated string version of this:
    _memset_byte(to, val, max_bytes);
    return max_bytes;
}

// Memset a bunch of integers
size_t memsetl(void *to, uint32_t val, size_t max_integers) {
    if (max_integers == 0) return 0;
    // Use hardware accelerated string version of this:
    _memset_long(to, val, max_integers);
    return max_integers;
}

bool _strncmp_timing_side_channel (const char *str1, const char *str2, size_t max_bytes) {
    size_t i = 0;
    while (i < max_bytes) {
        if (str1[i] != str2[i]) return false;
        if (str1[i] == '\0') return true;
        process_sleep(2);
        i++;
    }

    // If a string is longer than max_bytes we need to say they're unequal,
    // even if they've been equal thus far.
    // This is since we can't look past max_bytes, and they may be unequal after.
    // If this is return true, then comparing the files "s" and "src/" will return true
    // (since files are compared by what the user typed, not the length of the overall name)
    return false;
}

bool strncmp (const char *str1, const char *str2, size_t max_bytes) {
    size_t i = 0;
    while (i < max_bytes) {
        if (str1[i] != str2[i]) return false;
        if (str1[i] == '\0') return true;
        i++;
    }

    // If a string is longer than max_bytes we need to say they're unequal,
    // even if they've been equal thus far.
    // This is since we can't look past max_bytes, and they may be unequal after.
    // If this is return true, then comparing the files "s" and "src/" will return true
    // (since files are compared by what the user typed, not the length of the overall name)
    return false;
}

// Prints 'cur_char' into dest, space permitting
// Increments *cur_byte by 1
// Leaves space for the null terminator
static inline void _snprintf_putc(char *dest, size_t max_bytes, size_t *cur_byte, char cur_char) {
    if (*cur_byte < max_bytes - 1) {
        dest[*cur_byte] = cur_char;
        *cur_byte = *cur_byte + 1;
    }
}

// Helper method for snprintf- this does all the dirty work
// snprintf just sets up args and calls this
static inline void _snprintf(char *dest, size_t max_bytes, char *format, uint32_t *arg) {
    char *idx;

    bool is_special_code = false;
    bool should_print_preleading_zeroes = false;
    bool printed_anything = false;

    size_t cur_byte = 0;

    uint8_t arg_char_counter = 0;

    for (idx = format; *idx != NULL; idx++) {
        if (is_special_code) {
            if (*idx != 'c' && *idx != 'C') { arg_char_counter = 0; }
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
                            _snprintf_putc(dest, max_bytes, &cur_byte, cur_digit + 0x30);
                        }
                        else {
                            // Alpha
                            _snprintf_putc(dest, max_bytes, &cur_byte, cur_digit + 0x41 - 0x0A);
                        }
                        printed_anything = true;
                    }

                    arg_val = arg_val << 4;
                    counter--;
                }

                if (!printed_anything) {
                    // Thing to print was 0 and we didn't enable preleading zeroes
                    // So just print a 0 and move on
                    _snprintf_putc(dest, max_bytes, &cur_byte, '0');
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
            else if (*idx == 's' || *idx == 'S') {
                // Print string
                char *cursor = *(char **)arg;
                while (*cursor) {
                    _snprintf_putc(dest, max_bytes, &cur_byte, *cursor);
                    cursor++;
                }

                should_print_preleading_zeroes = false;
                is_special_code = false;
                arg++;
                continue;
            }
            else if (*idx == 'c' || *idx == 'C') {
                // Print char
                uint32_t cur_val = *arg;
                char print_char = (cur_val >> 8 * (3 - arg_char_counter)) & 0x0FF;
                _snprintf_putc(dest, max_bytes, &cur_byte, print_char);

                arg_char_counter++;
                if (arg_char_counter == 4) {
                    arg_char_counter = 0;
                    arg++;
                }
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

        if (*idx == '%') {
            is_special_code = true;
            continue;
        }

        _snprintf_putc(dest, max_bytes, &cur_byte, *idx);
    }
    dest[cur_byte] = '\0';
}

// Helper method for snprintf- this does all the dirty work
// snprintf just sets up args and calls this
static inline void _snprintf_unsafe (char *dest, size_t max_bytes, char *format, int32_t stack_bytes_readable, uint32_t *arg) {
    char *idx;

    bool is_special_code = false;
    bool should_print_preleading_zeroes = false;
    bool printed_anything = false;

    size_t cur_byte = 0;

    uint8_t arg_char_counter = 0;

    for (idx = format; *idx != NULL; idx++) {
        if (is_special_code) {
            if (*idx != 'c' && *idx != 'C') { arg_char_counter = 0; stack_bytes_readable -= 4; }
            else {stack_bytes_readable -= 1;}

            if (stack_bytes_readable > 0) {
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
                                _snprintf_putc(dest, max_bytes, &cur_byte, cur_digit + 0x30);
                            }
                            else {
                                // Alpha
                                _snprintf_putc(dest, max_bytes, &cur_byte, cur_digit + 0x41 - 0x0A);
                            }
                            printed_anything = true;
                        }

                        arg_val = arg_val << 4;
                        counter--;
                    }

                    if (!printed_anything) {
                        // Thing to print was 0 and we didn't enable preleading zeroes
                        // So just print a 0 and move on
                        _snprintf_putc(dest, max_bytes, &cur_byte, '0');
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
                else if (*idx == 's' || *idx == 'S') {
                    // Print string
                    char *cursor = *(char **)arg;
                    while (*cursor) {
                        _snprintf_putc(dest, max_bytes, &cur_byte, *cursor);
                        cursor++;
                    }

                    should_print_preleading_zeroes = false;
                    is_special_code = false;
                    arg++;
                    continue;
                }
                else if (*idx == 'c' || *idx == 'C') {
                    // Print char
                    uint32_t cur_val = *arg;
                    char print_char = (cur_val >> 8 * (3 - arg_char_counter)) & 0x0FF;
                    _snprintf_putc(dest, max_bytes, &cur_byte, print_char);

                    arg_char_counter++;
                    if (arg_char_counter == 4) {
                        arg_char_counter = 0;
                        arg++;
                    }
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
        }

        if (*idx == '%') {
            is_special_code = true;
            continue;
        }

        _snprintf_putc(dest, max_bytes, &cur_byte, *idx);
    }
    dest[cur_byte] = '\0';
}

void snprintf_unsafe (char *dest, size_t max_bytes, char *format, int32_t stack_bytes_readable, ...) {
    // Pointer to first argument (assuming only 32 bit args):
    uint32_t *arg = ((void*)&stack_bytes_readable);
    arg=arg+1;
    _snprintf_unsafe(dest, max_bytes, format, stack_bytes_readable, arg);
}

/*
 * snprintf
 *
 * Prints up to max_bytes chars into dest_str, using format as a format string
 */
void snprintf (char *dest, size_t max_bytes, char *format, ...) {
    // Pointer to first argument (assuming only 32 bit args):
    uint32_t *arg = ((void*)&format);
    arg=arg+1;
    _snprintf(dest, max_bytes, format, arg);
}