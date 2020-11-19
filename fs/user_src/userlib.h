#ifndef USERLIB_H
#define USERLIB_H
// jprx
// Userland library for jprxOS
#include "syscall.h"

#define true ((1))
#define false ((0))
#define NULL ((0))

typedef char bool;

typedef unsigned char uint8_t;
typedef char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long uint64_t;
typedef long int64_t;
typedef uint32_t size_t;

/*
 * _xprintf_putc
 *
 * Adds a character safely to the destination buffer
 * Used by printf family of functions
 *
 * Inputs:
 *      dest- buffer to write into
 *      nbytes- size of dest
 *      bytes_written- pointer to number of bytes written so far by _xprintf_putc
 *      c- char to write
 *
 * Returns:
 *      None
 *      (Technically it returns a size_t, but this should be ignored. This is to make the
 *      compiler happy after incrementing bytes_written).
 *
 * This should be the only function ever to write into dest by any printf style function
 */
static inline size_t _xprintf_putc(char *dest, size_t nbytes, size_t *bytes_written, char c) {
    if (*bytes_written > nbytes) return *bytes_written;
    dest[*bytes_written] = c;
    *bytes_written = *bytes_written + 1;
    return *bytes_written;
}

// Pretty much just vga_printf from the kernel side but with added safety
size_t snprintf(char *dest, size_t nbytes, char *str, ...) {
    char *idx;

    uint32_t *arg = (uint32_t *)&str;
    arg++;

    bool is_special_code = false;
    bool should_print_preleading_zeroes = false;
    bool printed_anything = false;

    size_t bytes_printed = 0;

    for (idx = str; *idx != NULL; idx++) {
        if (bytes_printed == nbytes) { break; }

        if (is_special_code) {
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
                            _xprintf_putc(dest, nbytes, &bytes_printed, cur_digit + 0x30);
                        }
                        else {
                            // Alpha
                            _xprintf_putc(dest, nbytes, &bytes_printed, cur_digit + 0x41 - 0x0A);
                        }
                        printed_anything = true;
                    }

                    arg_val = arg_val << 4;
                    counter--;
                }

                if (!printed_anything) {
                    // Thing to print was 0 and we didn't enable preleading zeroes
                    // So just print a 0 and move on
                    _xprintf_putc(dest, nbytes, &bytes_printed, '0');
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
            else if (*idx == 's') {
                // Print string
                char *cursor = *(char **)arg;
                while (*cursor) {
                    if (*cursor == 0x0A) {
                        _xprintf_putc(dest, nbytes, &bytes_printed, '\n');
                    }
                    else {
                        _xprintf_putc(dest, nbytes, &bytes_printed, *cursor);
                    }
                    cursor++;
                }

                should_print_preleading_zeroes = false;
                is_special_code = false;
                arg++;
                continue;
            }
            else if (*idx == 'c') {
                // Print char
                _xprintf_putc(dest, nbytes, &bytes_printed, *arg);

                arg++;
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

        if (*idx == 0x0A) {
            _xprintf_putc(dest, nbytes, &bytes_printed, '\n');
            continue;
        }
        if (*idx == '%') {
            is_special_code = true;
            continue;
        }

        _xprintf_putc(dest, nbytes, &bytes_printed, *idx);
    }

    // Add null terminator:
    if (bytes_printed > nbytes) {
        bytes_printed--;
    }
    _xprintf_putc(dest, nbytes, &bytes_printed, '\0');

    return bytes_printed;
}

// Memset from my kernel as well
size_t memset(char *to, char val, size_t max_bytes) {
    if (max_bytes == 0) return 0;
    
    size_t i = 0;
    while (i < max_bytes) {
        to[i] = val;
        i++;
    }
    return i;
}

// You guessed it- same as the kernel implementation ;D
bool strncmp (const char *str1, const char *str2, size_t max_bytes) {
    size_t i = 0;
    while (i < max_bytes) {
        if (str1[i] != str2[i]) return false;
        if (str1[i] == '\0') return true;
        i++;
    }
    return false;
}

// Returns true if strings are longer than length requested
bool strncmp_prefix (const char *str1, const char *str2, size_t max_bytes) {
    size_t i = 0;
    while (i < max_bytes) {
        if (str1[i] != str2[i]) return false;
        if (str1[i] == '\0') return true;
        i++;
    }
    return true;
}

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

int strlen(char *str) {
    int i = 0;
    while (*str) {
        str++;
        i++;
    }
    return i;
}

#endif