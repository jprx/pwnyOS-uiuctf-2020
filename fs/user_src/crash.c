#include "syscall.h"

char input_buf[2];

int main () {
    // while (1) {
    //     swrite(0, "# Name a file!\n");
    //     sread(0, input_buf);

    //     swrite(0, "You asked for file: ");
    //     swrite(0, input_buf);
    //     swrite(0, "\n");

    //     int fd = open(input_buf);
    //     if (fd == -1) {
    //         alert("File doesn't exist!");
    //     }
    //     else {
    //         sread(fd, input_buf);
    //         swrite(0, "File contains:\n");
    //         swrite(0, input_buf);
    //     }
    //     close(fd);
    //     swrite(0,"\n");
    // }

    swrite(0, "Should I crash the computer? (y/n)\n");
    sread(0, input_buf);

    if (input_buf[0] == 'y' || input_buf[0] == 'Y') {
        alert("OK, crashing the computer!");
        int *ptr = (int *)0xdeadc0de;
        *ptr = 0x420;
    }
    else {
        swrite(0, "OK, Sparing the computer. Bye!\n");
    }

    return 0;
}
