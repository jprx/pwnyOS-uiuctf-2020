#include "userlib.h"

char cmd[256];
char outbuf[2048];
char cmdbuf[2048 + 256];

// Paths should be up to 2048 chars long at max
char cur_dir[2048];

// Spare path variable for testing files and stuff
char spare_path[2048];

/* Returns true if 2 strings are identical, false otherwise */
#define strtest(s1, s2) ((strncmp(((s1)), ((s2)), sizeof(((s2))))))

//#define TERM_CLEAR_SCREEN ((0x18))
#define TERM_CLEAR_SCREEN (("\x18"))

// @TODO: Fix .data section ELF loading
// Right now the .data and .bss sections are loaded at offsets relative to their position in the
// ELF file itself, so since the kernel just lays the ELF out in memory. This means data and bss
// aren't exactly at the right spot. For initialized data this is an issue (for uninitialized not so much).
// By putting this as static const & tagging it with the .text attribute, I am forcing this data into the .text
// segment so it is loaded at the appropriate address.
//
// TLDR Fix ELF loading in kernel
static const char motd[] __attribute__((section(".text"))) = ""\
"  _____             _____ _    _ \n"\
" |  __ \\     /\\    / ____| |  | |\n"\
" | |__) |   /  \\  | (___ | |__| |\n"\
" |  _  /   / /\\ \\  \\___ \\|  __  |\n"\
" | | \\ \\  / ____ \\ ____) | |  | |\n"\
" |_|  \\_\\/_/    \\_\\_____/|_|  |_|\n"\
"      Resurrected-Again SHell  \n";

static const char helptext[] __attribute__((section(".text"))) = ""\
"rash help\n"\
"\n"\
"help- Print the help menu\n"\
"exit- Quit the shell\n"\
"cd directory- Change working directory to directory\n"\
"ls- List files in current directory\n"\
"alert message- Show a nicely formatted popup window with a message\n"\
"reboot- Reboot the system\n"\
"su user- Switch to account named 'user'\n"\
"whoami- Print current username and UID\n"\
"cat file- Read a file and print it\n"\
"run file- Run a file as a program\n"\
"\n"\
"To execute a program, just type its name\n"\
"    Example: to run rash, type rash\n"\
"";

void switch_term_user (char *requested_username) {
    // Attempt to switch to root user

    // Prompt for password:
    // But first issue an envconfig command to set characters to be hidden
    char pwbuf[64];
    swrite(0, "Password: ");
    env_config(ENV_CONFIG_HIDE_CHARS, true);
    sread(0, pwbuf);
    env_config(ENV_CONFIG_HIDE_CHARS, false);

    int retcode = switch_user(requested_username, pwbuf);
    if (0 == retcode) {
        snprintf(outbuf, sizeof(outbuf), "Logged in as %s!\n", requested_username);
        swrite(0, outbuf);
        return;
    }
    else if (-1 == retcode) {
        // No such user
        snprintf(outbuf, sizeof(outbuf), "User %s doesn't exist\n", requested_username);
        swrite(0, outbuf);
        return;
    }
    else if (-2 == retcode) {
        // Wrong password
        snprintf(outbuf, sizeof(outbuf), "Incorrect password\n");
        swrite(0, outbuf);
        return;
    }
}

int main () {
    int i = 0;
    snprintf(cur_dir, sizeof(cur_dir), "/");

    //snprintf(motd, sizeof(motd), "RASH\n");
    write(0, (char *)motd, sizeof(motd));

    while (true) {
        memset(cmd, '\0', sizeof(cmd));
        swrite(0, cur_dir);
        swrite(0, " $ ");
        sread(0, cmd);

        // Special commands:
        if (strtest(cmd, "exit")) {
            // Clear screen before exit:
            //swrite(0, TERM_CLEAR_SCREEN);
            return 0;
        }
        if (strtest(cmd, "motd")) {
            swrite(0, TERM_CLEAR_SCREEN);
            write(0, (char *)motd, sizeof(motd));
            continue;
        }
        if (strtest(cmd, "help")) {
            write(0, (char *)helptext, sizeof(helptext));
            continue;
        }
        if (strtest(cmd, "ls")) {
            int ls_fd = open(cur_dir);
            memset(outbuf, '\0', sizeof(outbuf));
            sread(ls_fd, outbuf);
            swrite(0, outbuf);
            swrite(0,"\n");
            close(ls_fd);
            continue;
        }
        if (strtest(cmd, "reboot")) {
            // Shut it down
            reboot();

            // If we are still here, print a message saying we couldn't do it:
            swrite(0, "Permission denied: Not allowed to reboot\n");
            continue;
        }
        if (strtest(cmd, "shutdown")) {
            // Shut it down
            shutdown();

            // If we are still here, print a message saying we couldn't do it:
            swrite(0, "Permission denied: Not allowed to shutdown\n");
            continue;
        }
        if (strtest(cmd, "whoami")) {
            // Print who I am!
            memset(outbuf, '\0', sizeof(outbuf));

            int32_t uid = 0;

            int retval = get_user(outbuf, sizeof(outbuf), &uid);

            if (retval != -5) {
                swrite(0, outbuf);
                snprintf(outbuf, sizeof(outbuf), " (%x)\n", uid);
                swrite(0, outbuf);
            }
            continue;
        }
        if (strtest(cmd, "su") || strtest(cmd, "su ")) {
            char requested_username[5] = "root";
            switch_term_user(requested_username);
            continue;
        }
        if (strtest(cmd, "pwd")) {
            swrite(0, cur_dir);
            swrite(0, "\n");
            continue;
        }
        if (strncmp_prefix(cmd, "su ", 3)) {
            char *requested_username = (char *)cmd + 3;
            switch_term_user(requested_username);
            continue;
        }
        if (strncmp_prefix(cmd, "sudo ", 5)) {
            swrite(0, "sudo is not supported, try using 'su' to change permissions\n");
            continue;
        }
        if (strncmp_prefix(cmd, "alert ", 6)) {
            // Alert whatever else the user typed
            alert(cmd + 6);
            continue;
        }
        if (strncmp_prefix(cmd, "cd ..", 5)) {
            // cd .. or anything like it (cd ../.. or the like)
            // Just go up 1 directory no matter what
            char *cursor = cur_dir;
            size_t len = strlen(cur_dir);
            cursor = &(cur_dir[len-1]);
            while (*(cursor-1) != '/' && cursor > cur_dir) { cursor--; }

            // Can't cd .. when you're in /
            if (cursor != cur_dir) {
                *cursor = '\0';
            }
            else {
                swrite(0, "No such directory\n");
            }

            continue;
        }
        if (strncmp_prefix(cmd, "cd ", 3)) {
            // Trying to CD
            size_t cmd_total_memory = 0;
            size_t new_dir_len = 0;

            bool relative = true;

            char *new_dir = &cmd[3];
            if (!*new_dir) continue;

            // Get rid of all '/'s at beginning unless it is just a "/"
            if (!strtest(new_dir, "/")) {
                // If any '/'s exist at the beginning, not a relative path
                while (*new_dir == '/') { relative = false; new_dir++; }
            }
            else {
                // Simple
                cur_dir[1] = '\0';
                continue;
            }

            // Ensure there is a '/' at the end of the chosen directory
            // If not, add one
            {
                cmd_total_memory = strlen(cmd) + 1; // Add 1 to account for null terminator
                new_dir_len = strlen(new_dir); // char at new_dir_len is null terminator
                if (new_dir[new_dir_len-1] != '/') {
                    // Check overall command length against size of cmd buffer
                    // No reason this can ever be greater, but let's still check anyways
                    if (cmd_total_memory >= sizeof(cmd)) {
                        swrite(0, "Path too long!\n");
                        continue;
                    }

                    // Convert null terminator to '/' and add a new null terminator after
                    // This is safe because cmd is at most the size of the buffer - 1
                    new_dir[new_dir_len] = '/';
                    new_dir[new_dir_len + 1] = '\0';
                }
            }

            if (relative) {
                int offset = strncpy(spare_path, cur_dir, sizeof(cur_dir));
                strncpy(spare_path + offset, new_dir, strlen(new_dir) + 1);
            }
            else {
                // Absolute path
                spare_path[0] = '/';
                strncpy(spare_path + 1, new_dir, strlen(new_dir) + 1);
            }

            int cd_fd = open(spare_path);
            if (cd_fd >= 0) {
                strncpy(cur_dir, spare_path, strlen(spare_path) + 1); // Need +1 for null terminator
                close(cd_fd);
            }
            else {
                if (cd_fd == -2) {
                    alert("Permission denied!");
                }
                else {
                    swrite(0, "No such directory\n");
                }
            }
            continue;
        }

        // If command is empty do nothing
        // This check is needed since later on the command is prepended with the current working directory
        // And it will be interpreted as trying to execute the current directory (most likely just printing all
        // files in the directory, since non-ELF files are opened and read out by default)
        if (strtest(cmd, "")) {
            continue;
        }

        // Which op are we using?
        bool should_exec = true;
        bool should_read = false;
        bool should_truncate = false;

        if (strncmp_prefix(cmd, "run ", 4)) {
            should_exec = true;
            should_read = false;
            should_truncate = true;
        }
        else if (strncmp_prefix(cmd, "cat ", 4)) {
            should_exec = false;
            should_read = true;
            should_truncate = true;
        }
        else {
            should_truncate = false;
        }

        char *cmd_ptr = cmd;
        if (should_truncate) {
            cmd_ptr += 4;
        }

        // Try to execute the program:
        // First construct absolute path using working directory:
        int offset = strncpy(cmdbuf, cur_dir, sizeof(cur_dir));
        strncpy(cmdbuf + offset, cmd_ptr, strlen(cmd_ptr) + 1);

        int fd = open(cmdbuf);
        if (fd < 0) {
            // Try again but appending a /bin in front
            char *cursor = cmdbuf;

            // Find end of cmdbuf
            size_t len = strlen(cmdbuf);
            cursor = &cmdbuf[len-1];
            while (*(cursor-1) != '/' && cursor > cmdbuf) cursor--;
            if (*cursor != NULL) {
                snprintf(spare_path, sizeof(spare_path), "/bin/%s", cursor);
                strncpy(cmdbuf, spare_path, sizeof(cmdbuf));
                fd = open(cmdbuf);
            }
        }

        if (fd < 0) {
            if (fd == -2) {
                alert("Permission denied!");
            }
            else {
                swrite(0, "File not found\n");
            }
        }
        else {
            int retcode = 0;

            if (should_exec) {
                retcode = execute(cmdbuf);
            }

            if (should_read) {
                int bytes_read = 0;

                do {
                    bytes_read = sread(fd, outbuf);
                    if (bytes_read == sizeof(outbuf)) {
                        bytes_read--;
                    }
                    outbuf[bytes_read] = '\0';
                    swrite(0, outbuf);
                } while (bytes_read != 0);

                swrite(0,"\n");
            }

            if (retcode != 0) {
                snprintf(outbuf, sizeof(outbuf), "Process exited with return code: %x\n", retcode);
                swrite(0, outbuf);
            }
        }
        close(fd);

        i++;
    }
}