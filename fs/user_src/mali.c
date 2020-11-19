#include "userlib.h"

#define TEST_TO_RUN 8

// A malicious program
// None of these attempted things should work in a secure kernel
// If any of these "tests" passes, you've got a bug
char buf[1024];

int main () {
	char *kern_mem = (char *)0x400000;
	int tmp = 0;

	// Try to poke kernel memory:
	switch (TEST_TO_RUN) {
		case 0:
		swrite(0, "Test 0: Kernel Write\n");
		*kern_mem = 0;
		break;

		case 1:
		// Try to read kernel memory:
		swrite(0, "Test 1: Kernel Read\n");
		tmp = *kern_mem;
		break;

		// Use kernel addresses in syscalls:
		case 2:
		swrite(0, "Test 2: Open kernel address\n");
		open(kern_mem);
		break;

		case 3:
		swrite(0, "Test 3: Read into kernel address from stdio\n");
		read(0,kern_mem,100);
		break;

		case 4:
		swrite(0, "Test 4: Write from kernel address to stdio\n");
		write(0,kern_mem,100);
		break;

		case 5:
		swrite(0, "Test 5: Switch User with kernel as username\n");
		switch_user(kern_mem, "password");
		break;

		case 6:
		swrite(0, "Test 6: Switch User with kernel as password\n");
		switch_user("user", kern_mem);
		break;

		case 7:
		swrite(0, "Test 7: Get User with kernel as username buffer\n");
		get_user(kern_mem, 100, &tmp);
		break;

		case 8:
		swrite(0, "Test 8: Get User with kernel as uid pointer\n");
		get_user("user", 5, (int *)kern_mem);
		break;

		default:
		swrite(0, "No test case associated with this number\n");
		return 0;
		break;
	}

	tmp++;
	swrite(0, "Test case passed! You've got a bug\n");
	
	return 0;
}