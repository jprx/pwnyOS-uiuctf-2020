#ifndef PROC_FS_H
#define PROC_FS_H
#include "file.h"
#include "types.h"

// A simple pseudofilesystem mounted at /proc that can do process related stuff
uint32_t proc_open (fd_t *fd, char *fname);
void proc_close (fd_t *fd);
size_t proc_read (fd_t *fd, char *buf, size_t size);
size_t proc_write (fd_t *fd, char *src, size_t size);

#endif
