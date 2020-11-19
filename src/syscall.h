#ifndef SYSCALL_H
#define SYSCALL_H
#include "types.h"
#include "user.h"

#define SYSCALL_INT_NUM ((0x80))

extern int32_t do_syscall(uint32_t syscall_num, uint32_t arg1, uint32_t arg2);
extern int32_t syscall_entry(void);

// Syscall numbers
// Process operations
#define SYS_SYSRET 0
#define SYS_EXEC 1

// File operations
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_READ 4
#define SYS_WRITE 5

// Terminal environment
#define SYS_ALERT 6
#define SYS_ENVCONFIG 7

// System status
#define SYS_REBOOT 8
#define SYS_SHUTDOWN 9

// User/ privilege -related
#define SYS_SWITCHUSER 10
#define SYS_GETUSER 11
#define SYS_REMOTE_SWITCHUSER 12

// Memory related
#define SYS_MMAP 13

// Sandbox related
#define SYS_SANDBOX_EXIT 14

// @TODO: STANDARDIZE KERNEL ERROR TYPES!
// typedef int32_t kern_err_t; or something. Needs to be signed!

// Syscall handlers:
/*
 * sysret
 *
 * Return from a process, ending it. Its exit code is the only argument to
 * this syscall.
 */
uint32_t sysret(uint32_t proc_retval);

/*
 * sysexec
 *
 * Execute a new program.
 *
 * Inputs:
 *      fname- Name of file to execute
 *
 * Returns the process exit code to the caller upon completion.
 * Returns -1 on error.
 */
uint32_t sysexec(char *fname);

/*
 * sysopen
 *
 * Given a filename return a file descriptor for the user process.
 * Returns -1 on failure. -2 on permission denied.
 */
int32_t sysopen(char *fname);

/*
 * sysclose
 *
 * Close a file descriptor
 */
int32_t sysclose(int32_t fd_idx);

/*
 * sysread
 *
 * Read from a file descriptor
 */
size_t sysread(int32_t fd_idx, char *buf, size_t size);

/*
 * syswrite
 *
 * Write to a file descriptor
 */
size_t syswrite(int32_t fd_idx, char *src, size_t size);

/*
 * sys_envconfig (Environment Config)
 *
 * Configure a terminal or environment-specific variable.
 * Example: screen color, cursor on/ off, whether characters print as *'s or not, etc.
 */
size_t sys_envconfig(uint32_t option_code, uint32_t option_value);

/*
 * sysreboot
 *
 * Crash the system, causing a triple fault (which reboots it :D)
 * Returns -1 if the process doesn't have access rights to the system resource
 *
 * Credit for the idea to use triple fault to reboot to OSDEV Wiki
 */
size_t sysreboot(void);

/*
 * sysshutdown
 *
 * Shut the whole PC down
 * Returns -1 if the process doesn't have access rights to the system resource,
 * or if shutdown failed.
 */
size_t sysshutdown(void);

/*
 * sysswitchuser
 *
 * Attempt to authenticate as a different user.
 * Returns 0 on success, -1 on no such name, -2 on wrong password.
 */
int32_t sysswitchuser(char *username, char *password);

/*
 * sysgetuser
 *
 * Retrieves login information about the current user.
 */
int32_t sysgetuser (char *name, size_t name_size, uid_t *id);


/*
 * sys_remote_switchuser
 *
 * Elevate a remote process to have the permissions of this current process.
 *
 * Does nothing if they are at our have higher privilege level than us.
 * Privilege level: lower UID = more privileged
 */
int32_t sys_remote_switchuser(uint32_t pid);

/*
 * mmap
 *
 * Request another page of memory to do whatever with.
 *
 * This will grant a single huge page, and can only grant 1 page per process.
 * This returns the virtual address of the new huge page (or 0 if unsuccessful).
 */
uint32_t sys_mmap ();

#endif
