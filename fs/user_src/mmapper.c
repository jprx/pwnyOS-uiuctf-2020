#include "userlib.h"

char buf[1024];

#define PROC_VIRT_ADDR ((0x08048000))
#define MMAP_VIRT_ADDR ((0x0D048000))

int main () {

	int *probable_region = (int *)MMAP_VIRT_ADDR;
	//*probable_region = 0x420;

	int *mmap_region = (int *)mmap();

	*probable_region = 0x420;

	if (!mmap_region) {
		swrite(0, "Couldn't mmap");
	}
	else {
		snprintf(buf, sizeof(buf), "mmap: %0x\n", (int)mmap_region);
		swrite(0, buf);

		snprintf(buf, sizeof(buf), "mmap: %0x\n", (int)*mmap_region);
		swrite(0, buf);
	}
	return 0;
}