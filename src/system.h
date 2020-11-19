#ifndef SYSTEM_H
#define SYSTEM_H
// Any system-level relevant functions, like rebooting, etc.

/*
 * reboot
 *
 * Crash the system, causing a triple fault (which reboots it :D)
 * This is called by the sysreboot syscall, and after exceptions.
 *
 * Credit for the idea to use triple fault to reboot to OSDEV Wiki
 */
size_t reboot();

/*
 * shutdown
 *
 * Like reboot, except never starts up again.
 */
size_t shutdown();

/*
 * sys_alert
 *
 * Show an alert message
 */
void sys_alert(char *message);

#endif
