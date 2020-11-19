#ifndef UTIL_H
#define UTIL_H
#include "types.h"
#include "sandbox.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t retval;
    asm volatile ("inb %1, %0" : "=a"(retval) : "Nd"(port) );
    return retval;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t retval;
    asm volatile ("inw %1, %0" : "=a"(retval) : "Nd"(port) );
    return retval;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t retval;
    asm volatile ("inl %1, %0" : "=a"(retval) : "Nd"(port) );
    return retval;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port) );
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint32_t cli_and_save(void) {
    uint32_t tmp;
    asm volatile (
        "pushfl\n" \
        "popl %0\n" \
        "cli" : "=r"(tmp)
    );
    return tmp;
}

static inline void restore_flags(uint32_t flags) {
    asm volatile (
        "pushl %0\n" \
        "popfl" : : "a"(flags)
    );
}

#define sti() asm volatile("sti");

/* Be careful with this one: */
#define cli() asm volatile("cli");

// Returns ceil(a/b)
#define ceil_div(a, b) ((((((a)) + ((b)) - 1))/((b))))
#define floor_div(a, b) ((((a))/((b))))

int strlen(char *str);

size_t strncpy(char *to, const char *from, size_t max_bytes);
size_t memcpy(void *to, void *from, size_t max_bytes);
size_t memset(char *to, char val, size_t max_bytes);
size_t memsetl(void *to, uint32_t val, size_t max_bytes);

// This one just returns true if the strings are the same
// That's all anyone should care about when calling strncmp anyways!
bool strncmp (const char *str1, const char *str2, size_t max_bytes);

#ifdef UIUCTF
bool _strncmp_timing_side_channel (const char *str1, const char *str2, size_t max_bytes);
#endif

// snprintf- format a string into another string buffer
void snprintf (char *dest, size_t max_bytes, char *format, ...);

// snprintf_unsafe- format a string into another string buffer
// Except the format string is from an unsafe source
// stack_bytes_readable is the number of bytes allowed to be read from the stack
void snprintf_unsafe (char *dest, size_t max_bytes, char *format, int32_t stack_bytes_readable, ...);

// get_eip- returns the value of EIP
extern uint32_t get_eip(void);

#endif
