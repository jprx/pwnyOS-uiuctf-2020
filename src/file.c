#include "file.h"
#include "process.h"
#include "util.h"

// File descriptors, file system mountpoints, etc.

// All mounted file systems:
mount_t filesystems[MAX_FILESYSTEMS] = {0};

/*
 * mount_fs
 *
 * Mounts a filesystem
 *
 * Returns true on success, false on failure
 */
bool mount_fs (char *mountpoint, fs_ops *ops_to_copy) {
    uint32_t i;
    if (!mountpoint) return false;
    if (!ops_to_copy) return false;

    for (i = 0; i < MAX_FILESYSTEMS; i++) {
        if (!filesystems[i].in_use) {
            filesystems[i].in_use = true;
            break;
        }
    }
    if (i == MAX_FILESYSTEMS) return false;

    memcpy(&filesystems[i].ops, ops_to_copy, sizeof(filesystems[i].ops));

    strncpy((char *)&(filesystems[i].path), mountpoint, MAX_MOUNTPOINT_PATH);

    return true;
}

/*
 * unmount_fs
 *
 * Un-mount a filesystem
 *
 * Returns true on success, false on failure
 */
bool unmount_fs (char *mountpoint) {
    uint32_t i;
    if (!mountpoint) return false;

    for (i = 0; i < MAX_FILESYSTEMS; i++) {
        if (strncmp(mountpoint, filesystems[i].path, MAX_MOUNTPOINT_PATH)) {
            filesystems[i].in_use = false;
            return true;
        }
    }

    // Couldn't find a matching filesystem
    return false;
}

/*
 * open_common
 *
 * Whenever we receive an open syscall, we pass through here first.
 * It will figure out which filesystem the file we are opening is associated with
 * and will then call the open function of that filesystem.
 *
 * First, find a free file descriptor. Then, call open on all available filesystems until
 * one of them returns true (file found). Pass the fully setup file descriptor back to the caller.
 *
 * Stores -1 into failed_reason if file not found, -2 if permission denied, and -3 if no free FD
 * Stores 0 into failed_reason on success
 */
fd_t *open_common (char *fname, int *failed_reason) {
    uint32_t i;
    fd_t *new_fd = NULL;

    // Find free file descriptor in process
    for (i = 0; i < NUM_FDS; i++) {
        if (!current_proc->fds[i].in_use) {
            current_proc->fds[i].in_use = true;
            new_fd = &(current_proc->fds[i]);
            break;
        }
    }
    if (!new_fd) {
        if (failed_reason != NULL) { *failed_reason = -3; }
        return NULL;
    }

    // Search all mounted filesystems for this file:
    bool encountered_permission_denied = false; // Have we found a file we couldn't access?
    for (i = 0; i < MAX_FILESYSTEMS; i++) {
        if (filesystems[i].in_use) {
            // Check permissions first (if filesystem provides check_perm, which is optional):
            if (filesystems[i].ops.check_perm != NULL) {
                // Set the resource ID of this FD to be public and owned by root (all users can access)
                // If this is changed by the filesystem open call, when we call access_ok
                // we will check against proper permissions. If access OK returns false, then we call sysclose on
                // the FD and return
                new_fd->protected_resource.uid = 0;
                new_fd->protected_resource.kind = RESOURCE_PUBLIC;

                filesystems[i].ops.check_perm(fname, &new_fd->protected_resource);

                // Try a different filesystem if access fails on this one:
                // 2 filesystems may have the same file at the same point, just with different perms
                if (!access_ok(current_proc->uid, &new_fd->protected_resource)) {
                    encountered_permission_denied = true; // We will remember that
                    continue;
                }
            }

            // Try to open this file:
            if (filesystems[i].ops.open(new_fd, fname)) {
                // Successful open, file found- return the fd:
                new_fd->mount = &(filesystems[i]);

                #ifdef UIUCTF
                new_fd->freaky_offset = 0;
                new_fd->is_freaky = 0;
                size_t _freaky_offset = 0;

                for (i = 0; i < NUM_FDS; i++) {
                    if (current_proc->fds[i].in_use && current_proc->fds[i].is_freaky) {
                        _freaky_offset += current_proc->fds[i].fs_offset;
                    }
                }

                // Find end of name:
                size_t name_len = strlen(fname);
                if (name_len >= 14) {
                    if (strncmp((char *)&fname[name_len-14], "freaky_fds.txt", FS_NAME_LEN)) {
                        // We found freaky fds file
                        // Perform vulnearbility on this one
                        new_fd->is_freaky = true;
                        new_fd->freaky_offset = _freaky_offset;
                    }
                }
                #endif

                if (failed_reason != NULL) { *failed_reason = 0; }
                return new_fd;
            }
        }
    }

    if (failed_reason != NULL) {
        // Permission denied? Or just couldn't find it
        if (encountered_permission_denied) *failed_reason = -2;
        else *failed_reason = -1;
    }
    return NULL;
}

/*
 * sysopen
 *
 * System call to open a file.
 *
 * Given a filename return a file descriptor for the user process.
 * Returns -1 on failure.
 */
int32_t sysopen(char *fname) {
    fd_t *opened_fd = NULL;
    int32_t failed_reason = 0;
    opened_fd = open_common(fname, &failed_reason);

    if (!opened_fd) return failed_reason;

    // Convert fd_t pointer to index and return it:
    return (opened_fd - (fd_t*)&(current_proc->fds));
}

static inline fd_t *_check_fd(int32_t fd_idx) {
    if (fd_idx < 0) return 0;
    if (fd_idx > NUM_FDS) return 0;
    fd_t *fd_to_ret = &(current_proc->fds[fd_idx]);
    if (!fd_to_ret->in_use) return 0;
    return fd_to_ret;
}

/*
 * sysclose
 *
 * Close a file descriptor
 */
 int32_t sysclose(int32_t fd_idx) {
    fd_t *fd = _check_fd(fd_idx);
    if (!fd || !fd->mount || !fd->mount->ops.read) return 0;

    if (fd_idx != FD_STDIO) {
        // Close the FD unless it corresponds to STDIO
        fd->mount->ops.close(fd);
        fd->in_use = false;
    }

    return 0;
}

/*
 * sysread
 *
 * Read from a file descriptor
 */
size_t sysread(int32_t fd_idx, char *buf, size_t size) {
    fd_t *fd = _check_fd(fd_idx);
    if (!fd || !fd->mount || !fd->mount->ops.read) return 0;
    return fd->mount->ops.read(fd, buf, size);
}

/*
 * syswrite
 *
 * Write to a file descriptor
 */
size_t syswrite(int32_t fd_idx, char *src, size_t size) {
    fd_t *fd = _check_fd(fd_idx);
    if (!fd || !fd->mount || !fd->mount->ops.write) return 0;
    return fd->mount->ops.write(fd, src, size);
}
