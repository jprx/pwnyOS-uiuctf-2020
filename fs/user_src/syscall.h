#ifndef SYSCALL_H
#define SYSCALL_H
// Wrapper for user syscalls

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

int syscall(int num, ...) {
    int *args = (int *)&num;
    int retval;
    asm volatile(
        /* Save B, C, and D */
        "pushl %%edx\n" \
        "pushl %%ecx\n" \
        "pushl %%ebx\n" \

        /* Call int 0x80 with A, B, C, D as args */
        "movl %0, %%eax\n" \
        "movl 12(%%eax), %%edx\n" \
        "movl 8(%%eax), %%ecx\n" \
        "movl 4(%%eax), %%ebx\n" \
        "movl (%%eax), %%eax\n" \
        "int $0x80\n" \

        /* Restore B, C, and D */
        "popl %%ebx\n" \
        "popl %%ecx\n" \
        "popl %%edx\n" \
        : "=a"(retval)
        : "rm"(args)
    );

    return retval;
}

int execute(char *file) {
    return syscall(SYS_EXEC, file);
}

int open(char *file) {
    return syscall(SYS_OPEN, file);
}

int close(int fd) {
    return syscall(SYS_CLOSE, fd);
}

int read(int fd, char *buf, int size) {
    return syscall(SYS_READ, fd, buf, size);
}

int write(int fd, char *buf, int size) {
    return syscall(SYS_WRITE, fd, buf, size);
}

/* Call this with buf as a string */
#define swrite(fd, buf) ((write((fd), (buf), sizeof((buf)))))

/* Call this with buf as a c char array */
#define sread(fd, buf) ((read((fd), (buf), sizeof((buf)))))

int alert(char *message) {
    return syscall(SYS_ALERT, message);
}

// Options for envconfig:
#define ENV_CONFIG_HIDE_CHARS ((0))
int env_config(unsigned int code, unsigned int val) {
    return syscall(SYS_ENVCONFIG, code, val);
}

/* This will only work for superuser (UID=0) */
int reboot() {
    return syscall(SYS_REBOOT);
}

/* This will only work for superuser (UID=0) */
/* This will also not work on real hardware */
int shutdown() {
    return syscall(SYS_SHUTDOWN);
}

int switch_user(char *username, char *password) {
    return syscall(SYS_SWITCHUSER, username, password);
}

int get_user(char *username, unsigned int num_bytes, int *id) {
    return syscall(SYS_GETUSER, username, num_bytes, id);
}

int remote_switch_user(int pid) {
    return syscall(SYS_REMOTE_SWITCHUSER, pid);
}

int mmap() {
    return syscall(SYS_MMAP);
}

#define clear_screen() do { \
    env_config(1, 0);   \
} while (0);

#endif