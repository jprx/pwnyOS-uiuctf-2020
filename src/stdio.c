#include "stdio.h"
#include "typeable.h"
#include "util.h"
#include "terminal.h"
#include "gui.h"

uint32_t    stdio_open(fd_t *fd, char *fname);
void        stdio_close(fd_t *fd);
size_t      stdio_read(fd_t *fd, char *buf, size_t size);
size_t      stdio_write(fd_t *fd, char *src, size_t size);

// STDIO mount for all processes
mount_t stdio_mount = {
    .ops = {
        .open = stdio_open,
        .close = stdio_close,
        .read = stdio_read,
        .write = stdio_write,
    },
    .in_use = true,
    .path = STDIO_MOUNT,
};

/*
 * stdio_open
 *
 * Configure a file descriptor to be setup for standard i/o
 */
uint32_t stdio_open(fd_t *fd, char *fname) {
    // Right now doesn't need to do anything
    return 0;
}

/*
 * stdio_close
 *
 * Close standard i/o? This should do nothing, user is wrong
 */
void stdio_close(fd_t *fd) {
    return;
}

/*
 * stdio_read
 *
 * Read from the current typeable to userspace
 */
size_t stdio_read(fd_t *fd, char *buf, size_t size) {
    // @TODO: copy_to_user
    uint32_t flags = cli_and_save();
    sti();
    current_typeable_flush_input();
    size_t bytes_read = current_typeable_read(buf, size);

    // Insert null terminator:
    if (bytes_read == size) {
        bytes_read--;
    }
    buf[bytes_read] = '\0';
    bytes_read++;

    restore_flags(flags);
    return bytes_read;
}

/*
 * stdio_write
 *
 * Write to the current typeable from userspace
 */
size_t stdio_write(fd_t *fd, char *src, size_t size) {
    // @TODO: copy_from_user
    char *cursor = src;
    size_t bytes_read = 0;

    // If we are writing something massive, and using high-res GUI,
    // for the sake of performance, let's cut to the chace:
    // This definitely breaks the typeable abstraction, but something drastic
    // is needed to fix this performance issue
    // Actually- this issue will be fixed by making rash print less stuff
    // if (USE_HIGHRES_GRAPHICS && current_typeable->putc == terminal_putc) {
    //     size_t bytes_to_read = strlen(src);
    //     while (bytes_to_read > TERM_ROUGH_PAGE_SIZE) {
    //         cursor += TERM_ROUGH_PAGE_SIZE/2;
    //         bytes_read = TERM_ROUGH_PAGE_SIZE;
    //         bytes_to_read = strlen(cursor);
    //     }
    // }

    // if (USE_HIGHRES_GRAPHICS && current_typeable->putc == terminal_putc) {
    //     _terminal_gui_clear((terminal *)current_typeable);
    //     terminal_repaint((terminal *)current_typeable);
    // }

    if (USE_HIGHRES_GRAPHICS && current_typeable->putc == terminal_putc) {
        terminal_start_write((terminal *)current_typeable);
    }

    while (*cursor && bytes_read < size) {
        current_typeable_putc(*cursor);
        bytes_read++;
        cursor++;
    }

    if (USE_HIGHRES_GRAPHICS && current_typeable->putc == terminal_putc) {
        terminal_stop_write((terminal *)current_typeable);
    }

    return bytes_read;
}
