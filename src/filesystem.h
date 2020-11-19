#ifndef FILESYSTEM_H
#define FILESYSTEM_H
// An implementation of the custom filesystem described in fs/make_fs.py comments
#include "x86_stuff.h"
#include "file.h"

#define FS_NAME_LEN ((64))
#define FS_BLOCK_SIZE ((4096))

// Data blocks need the size header
#define FS_DATA_BLOCK_SIZE ((FS_BLOCK_SIZE - 4))

// Length of a fentry or dentry or data block index within a directory block or fentry:
// (4 bytes = 32 bit)
#define FS_INDEX_LEN ((4))

// Length of directory block header in bytes
#define FS_DIR_BLOCK_HEADER_LEN ((8 + FS_NAME_LEN))

// Length of fentry header in bytes
#define FS_FENTRY_HEADER_LEN ((8 + FS_NAME_LEN))

// Max number of files in a directory block
#define FS_MAX_FILES_IN_DIR ((FS_BLOCK_SIZE - FS_DIR_BLOCK_HEADER_LEN)/FS_INDEX_LEN)

#define FS_MAX_DATA_BLOCKS_IN_FENTRY ((FS_BLOCK_SIZE - FS_FENTRY_HEADER_LEN)/FS_INDEX_LEN)

#define FS_DIR_MAGIC ((0xdeadd150))
#define FS_DAT_MAGIC ((0xdeadda7a))

// Just to make distinguishing int vs index a bit easier:
typedef uint32_t xentry_idx;

// A data block:
typedef struct data_block_struct_t {
    uint8_t data[FS_BLOCK_SIZE];
} data_block_t;

// A fentry:
typedef struct fentry_block_struct_t {
    uint32_t magicnum;
    uint32_t num_entries;
    char name[FS_NAME_LEN];

    // These could be subdirectories, or just files
    xentry_idx blocks[FS_MAX_DATA_BLOCKS_IN_FENTRY];
} fentry_t;

// A directory block:
typedef struct dir_block_struct_t {
    uint32_t magicnum;
    uint32_t num_entries;
    char name[FS_NAME_LEN];

    // These could be subdirectories, or just files
    xentry_idx files[FS_MAX_FILES_IN_DIR];
} dir_block_t;

// Either a fentry or dentry block, they can be treated the same
typedef struct xentry {
    union {
        // Fields for directory entries and fentries:
        struct {
            uint32_t magicnum;
            uint32_t num_entries;
            char name[FS_NAME_LEN];

            // These could be subdirectories, or just files, or fentries, who cares?
            xentry_idx blocks[FS_MAX_FILES_IN_DIR];
        };

        struct {
            // If this xentry points to a data block, the first 4 bytes are its size:
            uint32_t size;

            // Data
            int8_t data [FS_BLOCK_SIZE - 4];
        } data_block;
    };
} xentry;

// The filesystem root:
extern xentry *fs_root;

// Returns an xentry pointer to the fentry or dentry of this path:
// NULL on failure
xentry *filesys_lookup (char *path);

// Given a block index, return the appropriate xentry pointer:
// NULL on failure
xentry *filesys_lookup_idx (xentry_idx idx);

// Methods for reading and writing files
// If this is a directory, reading from it will return a list of file names
// If this is a file, reading from it will return data
// Returns number of bytes read
size_t filesys_read_bytes (xentry *entry, size_t offset, int8_t *buf, size_t bytes_to_read);

// Outward facing API for using this filesystem:
uint32_t fs_open(struct fd_t *fd, char *fname);
void fs_close(struct fd_t *fd);
size_t fs_read(struct fd_t *fd, char *buf, size_t size);
size_t fs_write(struct fd_t *fd, char *src, size_t size);
void fs_check_perm (char *path, resource_t *resource);

#endif
