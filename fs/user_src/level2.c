#include "userlib.h"

char buf[2];

int main () {
    clear_screen();
    swrite(0, "Congrats on solving the second challenge!\n\n");
    swrite(0, "You're still in a sandbox, but now you've got a shell at least.\n\n");
    swrite(0, "From here you can do pretty much anything but directly login as anythingbut a sandbox user, even if you've got their password.\n\n");
    swrite(0, "A bunch of syscalls are disabled as well, if you find a syscall you\ncan't use, you'll see:\n\nSandbox Policy Prevents that!\n\n");
    swrite(0, "Find a way to login as the un-sandboxed account 'user' to continue.\n\n");
    swrite(0, "Here's your flag: uiuctf{5ysc4ll_g4ng_sysc4ll_g4ng}\n\n");
    swrite(0, "Good luck!\nPress Enter to Continue.\n");
    sread(0, buf);
    clear_screen();
    return 0;
}