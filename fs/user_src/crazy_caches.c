#include "userlib.h"

#define PROC_VIRT_ADDR ((0x08048000))
#define MMAP_VIRT_ADDR ((0x0D048000))

#define BIG_PAGE_SIZE (1 << 22)

char buf[1024];

void setup_code_buffer (int8_t *addr) {
	int i = 0;
	for (i = 0; i < 0x1000; i++) {
		addr[i] = '\xc3';
	}
}

int main () {
	//swrite(0, "[crazy_caches] Starting...\n");
	int *mmap_region = (int *)mmap();

	if (!mmap_region) {
		swrite(0, "[crazy_caches] Couldn't mmap!\n");
		return -1;
	}
	else {
		//snprintf(buf, sizeof(buf), "[crazy_caches] mmap: %0x\n", (int)mmap_region);
		//swrite(0, buf);

		while (1) {
			setup_code_buffer((int8_t*)mmap_region);
			//swrite(0, "[crazy_caches] Allocated code buffer in mmap region");
			while (*(int32_t *)mmap_region != 0xdeadc0de);
			//alert("Running the code!\n");

			int *code_region = (int *)mmap_region + 0x100;
			
			asm volatile (
				"movl %0, %%eax\n"
				"pushal\n"
				"call *%%eax\n"
				"popal\n"
				:
				: "r"(code_region)
				: "eax", "ebx", "ecx", "edx"
			);

		}
	}
	return 0;
}