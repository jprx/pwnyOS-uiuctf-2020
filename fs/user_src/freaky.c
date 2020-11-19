#include "userlib.h"

char buf[1024];

int main () {
	int fd1, fd2;
	fd1 = 0;
	fd2 = 0;

	fd1 = open("/sandb0x/freaky_fds.txt");
	if (!fd1) { swrite(0, "Couldn't open freaky_fds.txt\n"); return -1; }
	sread(fd1, buf);
	if (buf[0] != 'V') { swrite(0, "Error reading 1\n"); return -1; }
	close(fd1);

	fd2 = open("/sandb0x/freaky_fds.txt");
	if (!fd2) { swrite(0, "Couldn't open freaky_fds.txt\n"); return -1; }
	sread(fd2, buf);
	if (buf[0] != 'V') { swrite(0, "Error reading 2\n"); return -1; }
	close(fd2);

	fd1 = open("/sandb0x/freaky_fds.txt");
	if (!fd1) { swrite(0, "Couldn't open fd1\n"); return -1; }
	sread(fd1, buf);
	if (buf[0] != 'V') { swrite(0, "Error reading fd1\n"); return -1; }

	fd2 = open("/sandb0x/freaky_fds.txt");
	if (!fd2) { swrite(0, "Couldn't open fs2\n"); return -1; }
	sread(fd2, buf);
	//if (buf[0] != 'w') { swrite(0, "Error reading fd2\n"); return -1; }
	swrite(0, buf);

	close(fd1);
	close(fd2);

	return 0;
}