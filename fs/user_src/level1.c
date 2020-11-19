#include "userlib.h"

char buf[2];

int main () {
	swrite(0, "**********************\n");
	swrite(0, "* Welcome to pwnyOS! *\n");
	swrite(0, "**********************\n");
	swrite(0, "\n");
	swrite(0, "Congrats on solving the first challenge!\nYour Island Adventure truly begins here- here's where things get real.\n\n");
	swrite(0, "You're currently in a sandbox that restricts your syscalls.\n\n");
	swrite(0, "binexec is a program that will run any shellcode you give it-\nuse it to call the SANDBOX_SPECIAL syscall and escape this sandbox!\n\n");
	swrite(0, "For info on what a syscall is and how to use them, check out the documentation on uiuc.tf.\n\n");
	swrite(0, "Oh, and of course, here's your flag: uiuctf{t1ming_s1d3_chann3l_g4ng}\n\n");
	swrite(0, "Good luck!\nPress Enter to Continue.\n");
	sread(0, buf);
	return 0;
}