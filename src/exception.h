#ifndef EXCEPTION_H
#define EXCEPTION_H

#define NUM_EXCEPTIONS 32

// Kernel panic
void crash_reason(char *reason);

// Kernel panic but with an error code:
void crash_reason_error_code(char *reason, char *error_code_name, uint32_t error_code);

extern void unknown_exception(void);
extern void page_fault_entry(void);
extern void double_fault_entry(void);
extern void gen_protect_fault_entry(void);

#endif
