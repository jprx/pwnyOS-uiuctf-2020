#include "filesystem.h"
#include "types.h"
#include "util.h"
#include "vga.h"
#include "file.h"
#include "sandbox.h"

// The filesystem root:
xentry *fs_root;

// Returns index in string of the next character identified by char_to_find
// Returns -1 if the char wasn't found
int strfind (char *str, char char_to_find) {
    size_t iterations = 0;
    char *cursor = str;
    while (*cursor) {
        if (*cursor == char_to_find) {
            return iterations;
        }
        cursor++;
        iterations++;
    }

    return -1;
}

// Split 'source' in two at the first occurance of 'split_char'
// Stores the first half in first_half, returns pointer to second half
// max_bytes is just the max number of bytes into first_half we will ever write
char *split(char *source, char *first_half, char split_char, size_t max_bytes) {
    size_t iterations = 0;
    char *cursor = source;
    while (*cursor && iterations < max_bytes) {
        if (*cursor == split_char) {
            first_half[iterations] = '\0';
            return cursor+1;
        }
        first_half[iterations] = source[iterations];
        iterations++;
        cursor++;
    }
    if (iterations < max_bytes) {
        first_half[iterations] = '\0';
    }
    return cursor;
}

// Returns if this path is a valid path
bool check_path(char *path) {
    char *cursor = path;
    bool seen_a_slash = false;
    uint32_t current_entry_name_len = 0;
    while (*cursor) {
        if (*cursor == '/') {
            if (seen_a_slash) {
                // Several slashes in a row
                return false;
            }
            seen_a_slash = true;
            current_entry_name_len = 0;
        }
        else {
            seen_a_slash = false;
            current_entry_name_len++;
            if (current_entry_name_len > FS_NAME_LEN) {
                // Name too long
                return false;
            }
        }
        cursor++;
    }
    return true;
}

// Returns an xentry pointer to the fentry or dentry of this path:
// NULL on failure
xentry *filesys_lookup (char *path) {
    // Name we are searching for at this level of iteration:
    char name_to_find[FS_NAME_LEN];

    // Current xentry being explored:
    xentry *current = fs_root;

    // Path that gets shorter as we move down directories
    // It is relative to the current searching directory:
    char *path_remaining = path;

    // Check for the root directory "/":
    if (strncmp(path, "/", 2)) {
        return &fs_root[0];
    }

    // Strip any extra '/'s at the beginning
    while (*path_remaining == '/') {
        path_remaining++;
    }

    // @TODO: If the path started with a / don't append working directory
    // Otherwise do append working directory (set current to working dir)

    // Check path is a valid path:
    // (No double //s, no file names too big)
    if (!check_path(path_remaining)) {
        return NULL;
    }

    // Check current substring for a match:
    while(1) {
        uint32_t i = 0;

        // If this isn't a directory entry, bail
        if (current->magicnum != FS_DIR_MAGIC) { return NULL; }
        
        bool child_found = false;
        path_remaining = split(path_remaining, name_to_find, '/', sizeof(name_to_find));
        size_t name_to_find_len = strlen(name_to_find) + 1; // Add 1 for null terminator

        // Empty file isn't allowed, return current directory
        // If split returned an empty string, that means the user searched something like
        // "A/B/" and this is the end of the path requested
        if (name_to_find_len == 0) {
            // Split sets path_remaining to the char after the cursor
            // If path_remaining is null and the character right before it was a / then return
            // the current directory.
            // Otherwise return NULL
            if (path_remaining[-1] == '/')
                return current;
            else
                return NULL;
        }

        // Scan entries at the current directory block
        for (i = 0; i < current->num_entries; i++) {
            if (strncmp(name_to_find, fs_root[current->blocks[i]].name, name_to_find_len)) {
                // Match!
                child_found = true;
                if (*path_remaining == '\0') {
                    // No more slashes, this is the file we have been looking for
                    return &fs_root[current->blocks[i]];
                }
                else {
                    // Explore this directory:
                    current = &fs_root[current->blocks[i]];
                }
                break;
            }
        }
        if (!child_found) {
            // name_to_find not in this directory
            return NULL;
        }
    }

    // Couldn't find this file:
    return NULL;
}

// Given a block index, return the appropriate xentry pointer:
// NULL on failure
xentry *filesys_lookup_idx (xentry_idx idx) {
    return &(fs_root[idx]);
}

// Methods for reading files
// If this is a directory, reading from it will return a list of file names
// If this is a file, reading from it will return data
// Returns number of bytes read

// Most complex case: offset into end block that is being partially read
// Each block need to copy starting at (block beginning + offset) to (end of block or bytes to read, whichever is first)
// Pseudocode:
// bytes to copy = bytes to read
// if (bytes to read > block size)
//     bytes to copy = block size
// copy from block + offset bytes to copy

// if block is last block, end
// else increment block and keep going

// // Attempt 2:
size_t filesys_read_bytes_fentry (xentry *entry, size_t offset, int8_t *buf, size_t bytes_to_read) {
    size_t bytes_read = 0;
    size_t offset_into_block = 0;
    xentry *cur_block;
    xentry_idx block_idx = offset/FS_DATA_BLOCK_SIZE;

    if (!entry) return 0;

    bool done = false;
    offset_into_block = offset % FS_DATA_BLOCK_SIZE; // Setup initial offset into block
    while (!done) {
        if (block_idx >= entry->num_entries) {
            return bytes_read;
        }

        cur_block = filesys_lookup_idx(entry->blocks[block_idx]);
        if (!cur_block) return bytes_read;

        // Copy at most from offset to the end of this block:
        size_t bytes_to_copy = bytes_to_read - bytes_read;
        size_t block_size = cur_block->data_block.size;
        if (bytes_to_copy > block_size - offset_into_block)
            bytes_to_copy = block_size - offset_into_block;

        memcpy(buf + bytes_read, (int8_t *)cur_block->data_block.data + offset_into_block, bytes_to_copy);
        bytes_read += bytes_to_copy;

        // We just read from last block
        if (block_idx == entry->num_entries - 1) { done = true; }

        // Done reading what was requested:
        if (bytes_read == bytes_to_read) { done = true; }

        // Calculate next block:
        // Next block doesn't need an offset:
        offset_into_block = 0;
        block_idx++;
    }

    return bytes_read;
}

size_t _filesys_read_bytes_fentry_freaky (xentry *entry, size_t offset, size_t freaky_offset, int8_t *buf, size_t bytes_to_read) {
    size_t bytes_read = 0;
    size_t offset_into_block = 0;
    xentry *cur_block;
    xentry_idx block_idx = offset/FS_DATA_BLOCK_SIZE;

    if (!entry) return 0;

    bool done = false;
    offset += freaky_offset;
    offset_into_block = offset % FS_DATA_BLOCK_SIZE; // Setup initial offset into block
    while (!done) {
        if (block_idx >= entry->num_entries) {
            return bytes_read;
        }

        cur_block = filesys_lookup_idx(entry->blocks[block_idx]);
        if (!cur_block) return bytes_read;

        // Copy at most from offset to the end of this block:
        size_t bytes_to_copy = bytes_to_read - bytes_read;
        size_t block_size = FREAKY_FILE_LEN + freaky_offset;
        if (block_size > FS_DATA_BLOCK_SIZE) {
            block_size = FS_DATA_BLOCK_SIZE;
        }
        if (bytes_to_copy > block_size - offset_into_block)
            bytes_to_copy = block_size - offset_into_block;

        memcpy(buf + bytes_read, (int8_t *)cur_block->data_block.data + offset_into_block, bytes_to_copy);
        bytes_read += bytes_to_copy;

        // We just read from last block
        if (block_idx == entry->num_entries - 1) { done = true; }

        // Done reading what was requested:
        if (bytes_read == bytes_to_read) { done = true; }

        // Calculate next block:
        // Next block doesn't need an offset:
        offset_into_block = 0;
        block_idx++;
    }

    return bytes_read;
}

// Offset is not in number of bytes but number of fentries
// IE offset of 1 means skip file 0
size_t filesys_read_bytes_dentry(xentry *entry, size_t offset, int8_t *buf, size_t bytes_to_read) {
    size_t bytes_read = 0;
    xentry *cur_block;

    // Offset = number of file names to skip
    // Offset of 2 = start at index 2 in dentry tree
    xentry_idx block_idx = offset;

    char cur_block_name[FS_NAME_LEN+1];

    size_t num_blocks = entry->num_entries;

    while (block_idx < num_blocks && bytes_read < bytes_to_read) {
        // Read current name
        cur_block = filesys_lookup_idx(entry->blocks[block_idx]);
        if (!cur_block) return bytes_read;

        if (cur_block->magicnum == FS_DIR_MAGIC || cur_block->magicnum == FS_DAT_MAGIC) {
            strncpy(cur_block_name, cur_block->name, FS_NAME_LEN);
            size_t cur_block_name_len = strlen(cur_block_name);

            // Add a \n after the name
            cur_block_name[cur_block_name_len] = '\n';
            size_t bytes_to_copy = cur_block_name_len + 1;

            // Only write full names into buf
            if (bytes_to_copy > (bytes_to_read - bytes_read)) {
                return bytes_read;
            }
            memcpy(buf + bytes_read, cur_block_name, bytes_to_copy);
            bytes_read += bytes_to_copy;
        }

        // Progress to next block
        block_idx++;
    }

    // Replace last character in buf with \0 instead of \n
    if (bytes_read != 0) {
        buf[bytes_read-1] = '\0';
    }

    return bytes_read;
}

size_t filesys_read_bytes(xentry *entry, size_t offset, int8_t *buf, size_t bytes_to_read) {
    if (!entry) return 0;

    if (entry->magicnum == FS_DIR_MAGIC) return filesys_read_bytes_dentry(entry, offset, buf, bytes_to_read);
    if (entry->magicnum == FS_DAT_MAGIC) return filesys_read_bytes_fentry(entry, offset, buf, bytes_to_read);

    return 0;
}

// Outward facing API for using this filesystem:
// Return true if the file exists, false otherwise
// If the file does exist, we are free to configure fd however we please
uint32_t fs_open(fd_t *fd, char *fname) {
    if (!fd) return false;
    if (!fname) return false;

    // Attempt to open the file:
    xentry *tmp = filesys_lookup(fname);
    if (!tmp) return false;

    fd->freaky_offset = 0;
    fd->is_freaky = 0;

    // Found file, update fd:
    fd->fs_xentry = tmp;
    fd->fs_offset = 0;
    return true;
}

void fs_close(fd_t *fd) {
    fd->fs_xentry = NULL;
    fd->is_freaky = false;
    fd->freaky_offset = 0;
    fd->fs_offset = 0;
    return;
}

size_t fs_read(fd_t *fd, char *buf, size_t size) {
    if (!fd) return 0;
    if (!buf) return 0;
    if (!fd->fs_xentry) return 0;
    size_t bytes_read = 0;
    if (fd->is_freaky) {
        if (fd->freaky_offset > FREAKY_LEAK_LEN) {
            fd->freaky_offset = FREAKY_LEAK_LEN;
        }
        bytes_read = _filesys_read_bytes_fentry_freaky(fd->fs_xentry, fd->fs_offset, fd->freaky_offset, buf, size);
    }
    else {
        bytes_read = filesys_read_bytes(fd->fs_xentry, fd->fs_offset + fd->freaky_offset, buf, size);
    }
    fd->fs_offset += bytes_read;
    return bytes_read;
}

size_t fs_write(fd_t *fd, char *src, size_t size) {
    // @TODO: Support writeable file system
    // (When this is implemented we will write starting at fd->fs_offset)
    return 0;
}

/*
 * check_perm_t
 *
 * This method checks the permissions of a file, writing it into the provided resource pointer
 *
 * resource provided is accessible by all (UID of 0, kind is RESOURCE_PUBLIC).
 * If this method does nothing, the resource will be allowed to be accessed by all.
 * Otherwise, set the UID and kind as desired into resource, and open_common will ensure only processes
 * of correct privilege level can access the resource.
 *
 * @TODO: Add some kind of filesystem level permissions model
 */
void fs_check_perm (char *path, resource_t *resource) {
    if (!path) return;
    if (!resource) return;

    // Strip off leading '/'s if any
    char *cursor = path;
    while (*cursor != NULL && *cursor == '/') { cursor++; }

    // Resource was all '/'s which is OK
    if (*cursor == NULL) return;

    // Only specifically block the /prot directory:
    char dir_buf[6]; // Only need 6 chars to store prot/
    strncpy(dir_buf, cursor, sizeof(dir_buf));

    // If the path was prot/something else, set dir_buf to just prot/
    // Otherwise this will be the first 5 chars of whatever
    dir_buf[sizeof(dir_buf)-1] = '\0';

    #ifdef UIUCTF
    // Specifically disallow /user and /root
    if (strncmp(dir_buf, "user/", sizeof(dir_buf)) || strncmp(dir_buf, "user", sizeof(dir_buf))) {
        resource->uid = 1;
        resource->kind = RESOURCE_PROTECTED;
    }
    if (strncmp(dir_buf, "root/", sizeof(dir_buf)) || strncmp(dir_buf, "root", sizeof(dir_buf))) {
        resource->uid = 0;
        resource->kind = RESOURCE_PROTECTED;
    }
    #endif

    // The prot directory is protected
    // Prevent people from even opening the prot directory (no / after prot)- not just files in the dir
    // but the dir itself (so you can't read what files are in it)
    if (strncmp(dir_buf, "prot/", sizeof(dir_buf)) || strncmp(dir_buf, "prot", sizeof(dir_buf))) {
        resource->uid = 0;
        resource->kind = RESOURCE_PROTECTED;
    }
}
