#include "procfs.h"
#include "util.h"
#include "file.h"
#include "process.h"
#include "types.h"

// Outward facing API for using this filesystem:
// Return true if the file exists, false otherwise
// If the file does exist, we are free to configure fd however we please
uint32_t proc_open(fd_t *fd, char *fname) {
    if (!fd) return false;
    if (!fname) return false;

    // Attempt to open the file:
    // This file system contains just 1 file, /proc/all
    char *cursor = fname;
    while (*cursor == '/') cursor++;
    if (!strncmp(cursor, "proc/all", MAX_MOUNTPOINT_PATH)) return false;

    fd->fs_offset = 0;

    // Found file, update fd:
    return true;
}

void proc_close(fd_t *fd) {
    if (!fd) return;
    fd->fs_offset = 0;
    return;
}

// Write into buf and update bytes_read, all in one go
size_t _proc_read_copy_to_buffer (char *to, char *from, size_t max_bytes, size_t *bytes_read) {
    size_t bytes_to_read = (max_bytes - *bytes_read);
    *bytes_read += strncpy(to + *bytes_read, from, bytes_to_read);
    return *bytes_read;
}

size_t proc_read(fd_t *fd, char *buf, size_t size) {
    if (!fd) return 0;
    if (!buf) return 0;
    size_t bytes_read = 0;
    uint32_t i = 0;

    if (fd->fs_offset != 0) {
        // @TODO: Seek in the proc FS
        return 0;
    }

    // char headerbuf[64];
    // strncpy(headerbuf, "Proclist: List all processes\n[PID]: [NAME]\n", sizeof(headerbuf));
    // _proc_read_copy_to_buffer(buf, headerbuf, size, &bytes_read);

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].in_use) {
            char linebuf[128];
            if (processes[i].kern_proc) {
                snprintf(linebuf, sizeof(linebuf), "%x: %s [KERNEL]\n", i, processes[i].name);
            }
            else {
                snprintf(linebuf, sizeof(linebuf), "%x: %s (UID = %x)\n", i, processes[i].name, processes[i].uid);
            }

            _proc_read_copy_to_buffer(buf, linebuf, size, &bytes_read);
        }
    }

    fd->fs_offset += bytes_read;
    return bytes_read;
}

size_t proc_write(fd_t *fd, char *src, size_t size) {
    // Writing to /proc makes no sense!
    return 0;
}
