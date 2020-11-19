#include "userlib.h"

#define CODE_SIZE ((4096))

// Code buf as ASCII
char ascii_code_buf[2 * CODE_SIZE];

// Code buf as code
char code_buf[CODE_SIZE];

bool found_done () {
	char *cursor = ascii_code_buf;
	while (*cursor) {
		if (strncmp (cursor, "done", 5)) {
			*cursor = '\0';
			return true;
		}
		cursor++;
	}

	return false;
}

bool found_exit () {
	char *cursor = ascii_code_buf;
	while (*cursor) {
		if (strncmp (cursor, "exit", 5)) {
			*cursor = '\0';
			return true;
		}
		cursor++;
	}

	return false;
}

// Converts numeric ASCII codes '1' '2' 'A' to numbers 1 2 A
// Undefined behaviour for non-hex ASCII codes
char ascii_to_num (char c) {
	if (c >= 0x30 && c <= 0x39) return (c - '0') & 0x0F;
	else return (c - 'A' + 10) & 0x0F;
}

void show_regs (uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
	char buf[128];
	snprintf(buf, sizeof(buf), "eax: %0x\nebx: %0x\necx: %0x\nedx: %0x\n", a, b, c, d);
	swrite(0, buf);
}

void setup_code_buf () {
	char *cursor = ascii_code_buf;

	// This is safe bc if *cursor fails, then it won't call *(cursor+1)
	uint32_t idx = 0;
	// Ret
	for (idx = 0; idx < sizeof(code_buf); idx++) {
		code_buf[idx] = '\xc3';
	}
	idx = 0;
	while (*cursor && *(cursor+1)) {
		code_buf[idx] = (ascii_to_num(*cursor) << 4) | ascii_to_num(*(cursor+1));

		idx++;
		cursor+=2;
		while (*cursor == ' ' || *cursor == '\n') { cursor++; }
	}
}

char addrbuf[128];

int main () {
	swrite(0, "Welcome to binexec!\nType some shellcode in hex and I'll run it!\n");
	swrite(0, "\nType the word 'done' and press enter when you are ready\n");
	swrite(0, "Type 'exit' and press enter to quit the program\n");
	snprintf(addrbuf, sizeof(addrbuf), "Address where I'm gonna run your code: %0x\n", &code_buf[0]);
	swrite(0, addrbuf);
	while (1) {
		int idx = 0;

		while (idx < sizeof(ascii_code_buf) - 1 && !found_done()) {
			size_t bytes_read = read(0, &ascii_code_buf[0] + idx, sizeof(ascii_code_buf) - idx);
			if (bytes_read != 0) {
				idx += bytes_read - 1;
			}
			if (found_exit()) {
				return 0;
			}
		}

		swrite(0, "Running...\n");

		setup_code_buf();

		asm volatile (
			"movl %0, %%eax\n"
			"pushal\n"
			"call *%%eax\n"
			"pushl %%edx\n"
			"pushl %%ecx\n"
			"pushl %%ebx\n"
			"pushl %%eax\n"
			"call show_regs\n"
			"addl $16, %%esp\n"
			"popal\n"
			:
			: "r"(&code_buf)
			: "eax", "ebx", "ecx", "edx"
		);

		// int (*their_code)() = (int(*)())code_buf;
		// their_code();

		// Clear buf
		memset(ascii_code_buf, '\0', sizeof(ascii_code_buf));
		swrite(0, "Done! Type more code\n");
		snprintf(addrbuf, sizeof(addrbuf), "Address where I'm gonna run your code: %0x\n", &code_buf[0]);
		swrite(0, addrbuf);
	}
}
