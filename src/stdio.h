#ifndef STDIO_H
#define STDIO_H
#include "file.h"

#define STDIO_MOUNT (("/dev/stdio"))

// Standard I/O is done with only 1 file descriptor- just read and write from it
extern mount_t stdio_mount;

#endif