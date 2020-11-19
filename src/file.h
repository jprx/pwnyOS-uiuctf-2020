#ifndef FILE_H
#define FILE_H

#include "types.h"
#include "user.h"
struct fd_t;
// Need this for xentry type
// But need to include this after we have defined struct fd_t because filesystem.h uses that
#include "filesystem.h"
#include "sandbox.h"

// The "File" abstraction
// Allows for multiple filesystems to be mounted and accessed in the same file space
// At search time, all filesystems are searched until the file in question is found
// Filesystems should not have files of the same name
// This can be avoided by having special filesystems put their files in special folders
// (EX /dev, /proc, etc.)
// If two filesystems have a file at the same location with the same name, these methods will
// default to returning the file in the order of declaration of filesystems in the filesystem mount array.

/*
 * open_t
 *
 * The method "open_common" in file.c will be the entrypoint for all file opens.
 * This method finds a free file descriptor, if available.
 * Then, it calls the open_t method on all mounted filesystems.
 *
 * A filesystem should first locate the file (if it exists) and then modify 
 * the given file descriptor as necessary.
 *
 * If the file does not exist, this method returns false.
 * If everything was successful, this method returns true.
 *
 * Filesystems should NOT modify the file descriptor if they return false
 *
 * Filesystems are not bound to only contain files as subdirectories of their mountpoint-
 * this method also receives paths as absolute directories.
 * @TODO: Make these relative to the mountpoint
 * (IE- in open_common, strip off the mountpoint before sending path here).
 */
typedef uint32_t (*open_t)(struct fd_t *fd, char *fname);

/*
 * close_t
 *
 * This method will be called whenever a program is requesting to close a file.
 */
typedef void (*close_t)(struct fd_t *fd);

/*
 * read_t
 *
 * This method is called for a filesystem when the user is requesting to read from it.
 */
typedef size_t (*read_t)(struct fd_t *fd, char *buf, size_t size);

/*
 * write_t
 *
 * This method is called when the user wants to write to a file descriptor.
 */
typedef size_t (*write_t)(struct fd_t *fd, char *src, size_t size);

/*
 * check_perm_t
 *
 * This method checks the permissions of a file, writing it into the provided resource pointer
 *
 * resource provided is accessible by all (UID of 0, kind is RESOURCE_PUBLIC).
 * If this method does nothing, the resource will be allowed to be accessed by all.
 * Otherwise, set the UID and kind as desired into resource, and open_common will ensure only processes
 * of correct privilege level can access the resource.
 */
typedef void (*check_perm_t)(char *path, resource_t *resource);

// Max depth of mountpoint name:
// If a file's path is longer than this, it is assumed to be an on-disk file as part of filesystem 0
#define MAX_MOUNTPOINT_PATH ((1024))

// Special file descriptor for stdio:
// (Can't close this one)
#define FD_STDIO ((0))

// All operations possible on a filesystem
typedef struct {
    open_t open;
    close_t close;
    read_t read;
    write_t write;
    check_perm_t check_perm;
} fs_ops;

// A mounted file system
typedef struct {
    fs_ops ops;
    bool in_use;

    // Path to this mountpoint
    // Really only used during mount and unmount calls- open doesn't care about this
    // (open will always pass absolute paths to each FS, its up to the FS to drop any
    // files that aren't contained within it)
    // EX: A mountpoint at /dev/ can contain files in /whatever if it wants
    char path[MAX_MOUNTPOINT_PATH];
} mount_t;

typedef struct fd_t {
    // Mountpoint for this FD:
    mount_t *mount;

    // Use uint32_t and not bool for alignment to make these stack nicely:
    uint32_t in_use;

    // Protection:
    // Filesystems can update this during their check_perm call to reflect the permissions of this file descriptor
    // It is always checked in open_common
    // This is unused if the mountpoint doesn't define check_perm
    resource_t protected_resource;

    // Used by the custom filesystem ("fs"):
    struct xentry *fs_xentry;

    // Common to all filesystems:
    size_t fs_offset;

    #ifdef UIUCTF
    // Allows you to leak this many extra bytes
    // Never to exceed actual length of the file
    size_t freaky_offset;
    bool is_freaky;
    #endif
} fd_t;

#define MAX_FILESYSTEMS ((8))
extern mount_t filesystems[MAX_FILESYSTEMS];

// Filesystem related API
bool mount_fs (char *mountpoint, fs_ops *ops_to_copy);
bool unmount_fs (char *mountpoint);

#endif
