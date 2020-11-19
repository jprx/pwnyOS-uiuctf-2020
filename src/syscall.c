#include "syscall.h"
#include "types.h"
#include "util.h"
#include "vga.h"
#include "typeable.h"
#include "process.h"
#include "user.h" // For sysreboot
#include "system.h"
#include "sandbox.h"
#include "interrupt.h"

typeable typeable_syscall = {
    .putc=typeable_putc_default,
    .clear=typeable_clear_default,
    .read=typeable_read_default,
    .enter=typeable_enter_default,
    .send_break=NULL,
    .x=0,
    .y=1,
    .frame={
        .x = 0,
        .y = 1,
        .w = 0,
        .h = 0
    },
    .buf[0] = 0
};

// Ensure pointers are userspace pointers:
static inline bool _is_user_pointer (uint32_t ptr) {
    return (DIR_IDX(ptr) == DIR_IDX(PROC_VIRT_ADDR));
}

// Kill a misbehaving process
static inline void _kill_misbehaving() {
    char str_to_show[] = "Hmm... That doesn't look like a user pointer to me!\n";
    syswrite(0, str_to_show, strlen(str_to_show));
    sysret(0);
}

// Kill a misbehaving process
static inline int _sandbox_deny() {
    char str_to_show[] = "Sandbox Policy Prevents that!\n";
    syswrite(0, str_to_show, strlen(str_to_show));

    // Access denied:
    return -5;
}

void sys_alert(char *message);

int32_t syscall (uint32_t syscall_num, ...) {
    uint32_t *args = &syscall_num;
    bool system_resource_ok = false;
    args += 1;

#ifdef UIUCTF
    // Only restrict processes with sandbox user perms:
    if (current_proc->uid == SANDBOX_USER) {
        if (sandbox_level == SANDBOX_1) {
            // Enforce sandbox level 1 here
            if (syscall_num > SYS_WRITE && syscall_num != SYS_SANDBOX_EXIT && syscall_num != SYS_ENVCONFIG && syscall_num != SYS_EXEC) {
                return _sandbox_deny();
            }
        }
        if (sandbox_level == SANDBOX_2) {
            // Enforce sandbox level 2 here
            if (syscall_num == SYS_SWITCHUSER || syscall_num == SYS_GETUSER || syscall_num == SYS_MMAP || syscall_num == SYS_REMOTE_SWITCHUSER) {
                return _sandbox_deny();
            }
        }
    }
#endif

    // Common syscall args:
    uint32_t arg1 = ((uint32_t *)(args))[0];
    uint32_t arg2 = ((uint32_t *)(args))[1];
    uint32_t arg3 = ((uint32_t *)(args))[2];
    int32_t fd = (int32_t)arg1;

    // Check permissions level of this process:
    system_resource_ok = false;
    if (current_proc != NULL) {
        if (access_ok(current_proc->uid, &system_resource)) {
            // Allowed to access the system resource
            system_resource_ok = true;
        }
    }

    // @TODO: copy_from_user
    switch (syscall_num) {
        // Close a process:
        case SYS_SYSRET:
        sysret(arg1);
        break;

        // Execute a new child process:
        case SYS_EXEC:
        //@TODO: copy_from_user
        if (_is_user_pointer((uint32_t)arg1)) {
            return sysexec((char *)arg1);
        }
        else {
            _kill_misbehaving();
            return -1;
        }
        break;

        /*
         * Filesystem operations just use fops table associated with the fd
         */
        case SYS_OPEN:
        // @TODO: copy_from_user
        if (_is_user_pointer((uint32_t)arg1)) {
            return sysopen((char *)arg1);
        }
        else {
            _kill_misbehaving();
            return -1;
        }
        break;

        case SYS_CLOSE:
        return sysclose(fd);
        break;

        case SYS_READ:
        // @TODO: copy_from_user
        if (_is_user_pointer(arg2)) {
            return sysread(fd, (char *)arg2, (size_t)arg3);
        }
        else {
            _kill_misbehaving();
            return 0;
        }
        break;

        case SYS_WRITE:
        // @TODO: copy_from_user
        if (_is_user_pointer(arg2)) {
            return syswrite(fd, (char *)arg2, (size_t)arg3);
        }
        else {
            _kill_misbehaving();
            return 0;
        }
        break;

        // Throw a popup on the screen:
        case SYS_ALERT:
        if (_is_user_pointer(arg1)) {
            sys_alert((char *)arg1);
            return 0;
        }
        else {
            _kill_misbehaving();
            return 0;
        }
        
        // Shouldn't get here:
        return 0;
        break;

        case SYS_ENVCONFIG:
        return sys_envconfig(arg1, arg2);
        break;

        case SYS_REBOOT:
        if (system_resource_ok) {
            // This will never return:
            // But if it does, return the error code
            // (permission check already performed but sysreboot does it again)
            return sysreboot();
        }
        //@TODO: Use some sort of kernel error code specification here:
        return -1;
        break;

        case SYS_SHUTDOWN:
        if (system_resource_ok) {
            // This will never return:
            // But if it does, return the error code
            // (permission check already performed but sysshutdown does it again)
            return sysshutdown();
        }
        //@TODO: Use some sort of kernel error code specification here:
        return -1;
        break;

        case SYS_SWITCHUSER:
        // @TODO: ENSURE THESE BUFFERS ARE IN USER MEMORY AND NOT KERNEL MEMORY!!!!
        if (_is_user_pointer(arg1) && _is_user_pointer(arg2)) {
            return sysswitchuser((char *)arg1, (char *)arg2);
        }
        else {
            _kill_misbehaving();
            return -1;
        }
        break;

        case SYS_GETUSER:
        // @TODO: ENSURE THESE BUFFERS ARE IN USER MEMORY AND NOT KERNEL MEMORY!!!!
        if (_is_user_pointer(arg1) && _is_user_pointer(arg3)) {
            return sysgetuser((char *)arg1, (size_t)arg2, (uid_t *)arg3);
        }
        else {
            _kill_misbehaving();
            return -1;
        }
        break;

        case SYS_REMOTE_SWITCHUSER:
        // Elevate a remote process's permission level to the level of this process
        return sys_remote_switchuser(arg1);
        break;

        case SYS_MMAP:
        return sys_mmap();
        break;

        case SYS_SANDBOX_EXIT:
        if (sandbox_level == SANDBOX_1) {
            sandbox_level = SANDBOX_2;
            sysret(0);
        }
        else {
            if (arg1 == (uint32_t)&kernel_slide) {
                //give_flag(FLAG_KERN_LEAK);
                sys_alert("uiuctf{l34ky_k3rn3l}");
                return 0;
            }
        }
        return -1;
        break;
    }

    // Syscall not found
    return -1;
}

/*
 * sysreboot
 *
 * Crash the system, causing a triple fault (which reboots it :D)
 *
 * Credit for the idea to use triple fault to reboot to OSDEV Wiki
 */
size_t sysreboot() {
    // First, CHECK PERMS!
    // Permission check should already have been performed by syscall handler, but doesn't
    // hurt to trust but verify. I may remove that check in the future for performance.
    if (current_proc != NULL) {
        if (!access_ok(current_proc->uid, &system_resource)) {
            // Not allowed to access system resource, return failure code
            return -1;
        }
    }
    else {
        // Null process???
        // This means something has probably gone wrong already
        // Lets just crash it
    }

    return reboot();
}

/*
 * sysshutdown
 *
 * Shut the whole PC down
 * Returns -1 if the process doesn't have access rights to the system resource,
 * or if shutdown failed.
 */
size_t sysshutdown(void) {
    // First, CHECK PERMS!
    // Permission check should already have been performed by syscall handler, but doesn't
    // hurt to trust but verify. I may remove that check in the future for performance.
    if (current_proc != NULL) {
        if (!access_ok(current_proc->uid, &system_resource)) {
            // Not allowed to access system resource, return failure code
            return -1;
        }
    }
    else {
        // Null process???
        // This means something has probably gone wrong already
        // Lets just crash it
    }

    return shutdown();
}

void sys_alert(char *message) {
    typeable *prev_typeable = NULL;
    uint32_t flags = cli_and_save();
    prev_typeable = get_current_typeable();
    vga_cursor_disable();
    vga_popup_simple(30, 5, message);
    set_current_typeable(&typeable_syscall);
    hw_pic_mask(IRQ_PIT);
    sti();
    current_typeable_read(NULL, 0);
    restore_flags(flags);
    hw_pic_unmask(IRQ_PIT);
    vga_popup_clear();
    vga_cursor_enable();
    set_current_typeable(prev_typeable);
}