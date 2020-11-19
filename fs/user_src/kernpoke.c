#include "syscall.h"
#include "userlib.h"

char buf[1024];

void main () {
    swrite(0, "Poking kernel memory!\n");
    int *kern_ptr = (int *)0x402e28;
    int x = *kern_ptr;
    snprintf(buf, sizeof(buf), "Got %x\n", x);
    swrite(0, buf);
}
