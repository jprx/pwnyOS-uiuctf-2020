#include "userlib.h"

#define BIG_NUM ((1000000000))

int main () {
    //int k;
    int i;
    while (1) {
        for (i = 0; i < BIG_NUM; i++);

        // After 4 iterations, k will hold 0x41414141, which watchdog0 will
        // detect as a "hacking number"
        // k = ((k << 8) | 0x41);
        swrite(0, "!");
    }
}