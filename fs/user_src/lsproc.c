#include "userlib.h"

char printbuf[4096];

int main () {
	int fd = open("/proc/all");
	if (fd < 0 || !fd) return -1;
	sread(fd, printbuf);
	swrite(0, printbuf);
	close(fd);
    return 0;
}