#include "user.h"
#include "util.h"
#include "file.h"
#include "filesystem.h"
#include "exception.h"
#include "vga.h"
#include "typeable.h"
#include "process.h"
#include "gui.h"
#include "sandbox.h"
#include "system.h"

/* The system resource */
/*
 * system_resource
 *
 * The resource identifier associated with any system-related (kernel) objects
 * Example: right to modify interrupt tables, right to crash (reboot) system, etc.
 */
resource_t system_resource = {
    .uid = 0,
    .kind = RESOURCE_PROTECTED,
};

/* Fields for login_window */
// Username field
typeable username_typeable = {
    .putc=typeable_putc_default,
    .clear=typeable_clear_default,
    .read=typeable_read_default,
    .enter=typeable_enter_default,
    .x=(VGA_WIDTH/2) - (47/2) + 12,
    .y=(VGA_HEIGHT/2) - 1,
    .frame={
        .x = (VGA_WIDTH/2) - (47/2) + 12,
        .y = (VGA_HEIGHT/2) - 1,
        .w = USERNAME_LEN-1,
        .h = 1
    },
    .buf[0] = 0
};

char username_buf[USERNAME_LEN];

// Password field
#define PASSWORD_LEN ((USERNAME_LEN))
typeable password_typeable = {
    .putc=typeable_putc_private,
    .clear=typeable_clear_default,
    .read=typeable_read_default,
    .enter=typeable_enter_default,
    .x=(VGA_WIDTH/2) - (47/2) + 12,
    .y=(VGA_HEIGHT/2) + 1,
    .frame={
        .x = (VGA_WIDTH/2) - (47/2) + 12,
        .y = (VGA_HEIGHT/2) + 1,
        .w = PASSWORD_LEN-1,
        .h = 1
    },
    .buf[0] = 0
};

char password_buf[PASSWORD_LEN];

// Resolution mode of 2 requires a different fudge factor than the other modes
#if (RESOLUTION_MODE == 2)
#define GUI_TYPEABLE_FUDGE_W ((5 * FONT_WIDTH))
#define GUI_TYPEABLE_FUDGE_H ((FONT_HEIGHT + (FONT_HEIGHT/4)))
#define GUI_LOGIN_UI_FUDGE_W ((-5))
#else
#define GUI_TYPEABLE_FUDGE_W ((3 * FONT_WIDTH))
#define GUI_TYPEABLE_FUDGE_H ((FONT_HEIGHT))
#define GUI_LOGIN_UI_FUDGE_W ((0))
#endif

/* Fields for high-res GUI */
// I am so sorry for this disguisting positioning math
// I will do better in the future I promise
typeable gui_username_typeable = {
    .putc=typeable_putc_default,
    .clear=typeable_clear_default,
    .read=typeable_read_default,
    .enter=typeable_enter_default,
    .x=(((SCREEN_WIDTH/2) - 250 + 25 + 9 * FONT_WIDTH/6 + 15) * GUI_FONT_SCALAR + GUI_TYPEABLE_FUDGE_W) / FONT_WIDTH,
    .y=(((SCREEN_HEIGHT/2) - 150 + 80 - 5 - FONT_HEIGHT/24) * GUI_FONT_SCALAR + GUI_TYPEABLE_FUDGE_H) / FONT_HEIGHT,
    .frame={
        .x = (((SCREEN_WIDTH/2) - 250 + 25 + 9 * FONT_WIDTH/6 + 15) * GUI_FONT_SCALAR + GUI_TYPEABLE_FUDGE_W) / FONT_WIDTH,
        .y = (((SCREEN_HEIGHT/2) - 150 + 80 - 5 - FONT_HEIGHT/24) * GUI_FONT_SCALAR + GUI_TYPEABLE_FUDGE_H) / FONT_HEIGHT,
        .w = USERNAME_LEN-1,
        .h = 1
    },
    .buf[0] = 0
};

typeable gui_password_typeable = {
    .putc=typeable_putc_private,
    .clear=typeable_clear_default,
    .read=typeable_read_default,
    .enter=typeable_enter_default,
    .x=(((SCREEN_WIDTH/2) - 250 + 25 + 9 * FONT_WIDTH/6 + 15) * GUI_FONT_SCALAR + GUI_TYPEABLE_FUDGE_W) / FONT_WIDTH,
    .y=(((SCREEN_HEIGHT/2) - 150 + 140 - FONT_HEIGHT/12) * GUI_FONT_SCALAR + GUI_TYPEABLE_FUDGE_H) / FONT_HEIGHT,
    .frame={
        .x = (((SCREEN_WIDTH/2) - 250 + 25 + 9 * FONT_WIDTH/6 + 15) * GUI_FONT_SCALAR + GUI_TYPEABLE_FUDGE_W) / FONT_WIDTH,
        .y = (((SCREEN_HEIGHT/2) - 150 + 140 - FONT_HEIGHT/12) * GUI_FONT_SCALAR + GUI_TYPEABLE_FUDGE_H) / FONT_HEIGHT,
        .w = PASSWORD_LEN-1,
        .h = 1
    },
    .buf[0] = 0
};

/*
 * users
 * The global array of all users
 * UIDs are indeces into this array
 */
user_t users[MAX_NUM_USERS];

/*
 * load_users_from_file
 *
 * Given a path, treat is a user initialization file and read it into the users array, configuring it correctly hopefully.
 */
bool load_users_from_file (char *file) {
    fd_t users_fd;

    // Buffer of the users file:
    char userfile_buf[2 * USERNAME_LEN * MAX_NUM_USERS + 1];

    if (!fs_open(&users_fd, file)) {
        crash_reason("Couldn't load username file");
    }
    fs_read(&users_fd, userfile_buf, sizeof(userfile_buf));

    bool retcode = load_users(userfile_buf);
    fs_close(&users_fd);
    return retcode;
}

/*
 * load_users
 * Load all users into the users array
 *
 * Calls to any user access methods are indeterminite until this is called!
 *
 * name_db is a pointer to a string containing username/ password pairs (from filesystem probably).
 * They should be of the form:
 * FILE START:
 * name1
 * password1
 * name2
 * password2
 * etc.
 *
 * In order of UID! Also, names and passwords both better not be longer than USERNAME_LEN or I'm gonna lose it.
 *
 * Returns number of users loaded, -1 on failure
 */
int load_users(const char *name_db) {
    uid_t cur_user = 0;
    const char *cur_line = name_db;

    char username_tmp[USERNAME_LEN];
    char password_tmp[USERNAME_LEN];

    bool reading_password = false;

    // Mark all users as invalid:
    for (cur_user = 0; cur_user < MAX_NUM_USERS; cur_user++) {
        users[cur_user].valid = false;
    }
    cur_user = 0;

    // If cur_user grows larger than number of users, throw an error
    while (cur_user < MAX_NUM_USERS) {
        // Plaintext password gang
        uint32_t chars_read = 0;
        while (cur_line[chars_read] != '\0' && cur_line[chars_read] != '\n' && chars_read < USERNAME_LEN - 1) {
            if (reading_password) password_tmp[chars_read] = cur_line[chars_read];
            else username_tmp[chars_read] = cur_line[chars_read];
            chars_read++;
        }
        if (reading_password) password_tmp[chars_read] = '\0';
        else username_tmp[chars_read] = '\0';

        if (reading_password) {
            // We just read the super secure plaintext password
            // Store this user in the users array
            strncpy((char *)&users[cur_user].name, username_tmp, USERNAME_LEN);
            strncpy((char *)&users[cur_user].password, password_tmp, USERNAME_LEN);
            users[cur_user].valid = true;
            cur_user++;
        }

        if (cur_line[chars_read] == '\n' || chars_read == USERNAME_LEN-1) {
            cur_line = &cur_line[chars_read+1];
        }
        else {
            // End of file
            // Didn't get a password!
            if (!reading_password) return -1;

            // Return number of users:
            return cur_user;
        }
        reading_password = !reading_password;
    }

    return -1;
}

/*
 * _vga_login_window
 * Doesn't return until the user successfully logs in
 *
 * Draws a VGA login window to the screen and uses the users array to check against passwords.
 *
 * Helper method used by login_window.
 */
static inline uid_t _vga_login_window (bool animate) {
    int box_w_target = 48; //36, 21;
    int box_h_target = 7;
    int box_w = 2;
    int box_h = 2;
    int dummy = 0;

    typeable *prev_typeable = current_typeable;

    // Little bit of animation for ya
    if (animate) {
        for (box_w = 2; box_w < box_w_target; box_w++) {
            // Use vga_clear_box for everything but titlebar
            vga_clear_box (0, 1, VGA_WIDTH, VGA_HEIGHT-1);
            vga_setattr(0,1,0xFF);
            vga_box_title_centered(box_w, box_h, "");
            while (dummy < 1900000) { dummy++; }
            dummy = 0;
        }
        for (box_h = box_h; box_h < box_h_target; box_h++) {
            // Use vga_clear_box for everything but titlebar
            vga_clear_box (0, 1, VGA_WIDTH, VGA_HEIGHT-1);
            vga_setattr(0,1,0xFF);
            vga_box_title_centered(box_w, box_h, "");
            while (dummy < 5000000) { dummy++; }
            dummy = 0;
        }
    }
    else {
        box_w = box_w_target;
        box_h = box_h_target;
    }

    // Use vga_clear_box for everything but titlebar
    vga_clear_box (0, 1, VGA_WIDTH, VGA_HEIGHT-1);
    vga_setattr(0,1,0xFF);

    vga_box_title_centered(box_w,box_h,"Login");
    vga_printf_xy((VGA_WIDTH/2) - (box_w/2) + 3, (VGA_HEIGHT/2) - 1, "Username: ");
    vga_printf_xy((VGA_WIDTH/2) - (box_w/2) + 3, (VGA_HEIGHT/2) + 1, "Password: ");

    set_current_typeable(&username_typeable);
    current_typeable_clear();
    set_current_typeable(&password_typeable);
    current_typeable_clear();

    sti();

    bool access_granted = false;
    while (!access_granted) {
        set_current_typeable(&username_typeable);
        current_typeable_read(username_buf, USERNAME_LEN);
        set_current_typeable(&password_typeable);
        current_typeable_read(password_buf, PASSWORD_LEN);

        set_current_typeable(prev_typeable);

        uid_t uid = login(username_buf, password_buf);
        if (uid >= 0) return uid;

        if (uid == -1) {
            vga_popup(32, 5, "Unknown User:", 0xF4);
            vga_printf_centered((VGA_HEIGHT/2), "%s", username_buf);
        }
        if (uid == -2) {
            // Invalid password
            vga_popup(32, 5, "Incorrect Password!", 0xF4);
        }

        current_typeable_read(NULL, 0);
        vga_popup_clear();
        password_typeable.clear(&password_typeable);
    }

    set_current_typeable(prev_typeable);
    return -1;
}

static inline uid_t _gui_login_window () {
    typeable *prev_typeable = current_typeable;

    // Login window:
    blur_region(framebuffer,
        framebuffer,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        (SCREEN_WIDTH/2) - 250,
        (SCREEN_HEIGHT/2) - 150,
        500,
        200,
        BORDER_RADIUS,
        DARK_MODE_LIGHTEN_AMOUNT);

    // Username region:
    blur_region(
        framebuffer,
        framebuffer,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        (SCREEN_WIDTH/2) - 250 + 25 + 9 * FONT_WIDTH/6 + 15,
        (SCREEN_HEIGHT/2) - 150 + 80 - 5 + GUI_LOGIN_UI_FUDGE_W,
        300,
        FONT_HEIGHT/6 + FONT_HEIGHT/12,
        15,
        LIGHT_MODE_LIGHTEN_AMOUNT);

    // Password region:
    blur_region(
        framebuffer,
        framebuffer,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        (SCREEN_WIDTH/2) - 250 + 25 + 9 * FONT_WIDTH/6 + 15,
        (SCREEN_HEIGHT/2) - 150 + 140 - FONT_HEIGHT/12 + (FONT_HEIGHT/24) + GUI_LOGIN_UI_FUDGE_W,
        300,
        FONT_HEIGHT/6 + FONT_HEIGHT/12,
        15,
        LIGHT_MODE_LIGHTEN_AMOUNT);

    // Copy to video memory, ignoring the statusbar:
    //size_t offset = (SCREEN_WIDTH * FONT_HEIGHT/6 * 2);
    //memcpy(videomem + offset, framebuffer + offset, SCREEN_SIZE * sizeof(*videomem) - offset);

    // Commit the changes to framebuffer to the screen
    // (This draws the blurred regions)
    gui_redraw();

    // All strings are drawn directly to video memory (so framebuffer is what video mem is replaced with when chars are erased)
    gui_draw_string((SCREEN_WIDTH/2)-(FONT_WIDTH * 5)/6, (SCREEN_HEIGHT/2) - 150, "Login", 3, 0xFFFFFF);
    gui_draw_string((SCREEN_WIDTH/2) - 250 + 25, (SCREEN_HEIGHT/2) - 150 + 80 - 5 + GUI_LOGIN_UI_FUDGE_W + (FONT_HEIGHT/24), "Username:", 6, 0xFFFFFF);
    gui_draw_string((SCREEN_WIDTH/2) - 250 + 25, (SCREEN_HEIGHT/2) - 150 + 140 - 5 + GUI_LOGIN_UI_FUDGE_W + (FONT_HEIGHT/24), "Password:", 6, 0xFFFFFF);

    char *bottom_notice = "Welcome to your Island Adventure! Please enjoy your stay.";
    gui_draw_string((SCREEN_WIDTH/2) - (FONT_WIDTH/6) * (strlen(bottom_notice)/2), (SCREEN_HEIGHT/2) + 250, bottom_notice, 6, 0);

    set_current_typeable(&gui_username_typeable);
    current_typeable_clear();
    set_current_typeable(&gui_password_typeable);
    current_typeable_clear();
    sti();

    bool access_granted = false;
    while (!access_granted) {
        set_current_typeable(&gui_username_typeable);
        current_typeable_read(username_buf, USERNAME_LEN);
        set_current_typeable(&gui_password_typeable);
        current_typeable_read(password_buf, PASSWORD_LEN);

        set_current_typeable(prev_typeable);

        uid_t uid = login(username_buf, password_buf);
        if (uid >= 0) {
            // Clear the login window:
            gui_set_region_background((SCREEN_WIDTH/2) - 250,
                (SCREEN_HEIGHT/2) - 150,
                500,
                200);
            gui_redraw();

            return uid;
        }

        if (uid == -1) {
            vga_popup(32, 5, "Unknown User", 0xF4);
        }
        if (uid == -2) {
            // Invalid password
            vga_popup(32, 5, "Incorrect Password!", 0xF4);
        }

        current_typeable_read(NULL, 0);
        vga_popup_clear();
        gui_password_typeable.clear(&gui_password_typeable);
    }

    set_current_typeable(prev_typeable);
    return -1;
}

/*
 * login_window
 * Doesn't return until the user successfully logs in
 *
 * Draws a login window to the screen and uses the users array to check against passwords.
 * Uses either _vga_login_window or _gui_login_window helper methods, depending on the graphics version.
 * animate flag is ignored in GUI mode.
 */
uid_t login_window (bool animate) {
    if (vga_use_highres_gui) {
        return _gui_login_window();
    }
    else {
        return _vga_login_window(animate);
    }
}

/*
 * login
 * Attempt to login to the system
 *
 * Returns UID on success, -1 (invalid UID) on failure, or -2 for invalid password
 */
uid_t login(char *username, char *password) {
    uid_t i;
    for (i = 0; i < MAX_NUM_USERS; i++) {
#ifdef UIUCTF
        if ((sandbox_level != SANDBOX_NONE) && i < SANDBOX_RESTRICTED_LOGIN_ID) {
            continue;
        }

        // Disallow root user
        //if (i == ROOT_USER) continue;

        if (users[i].valid) {
            if (strncmp(users[i].name, username, USERNAME_LEN)) {
                if (password_timing_side_channel == true) {
                    if (strncmp(users[i].password, password, USERNAME_LEN)) {
                        // Return new UID:
                        return i;
                    }
                    else {
                        // Hit em with the timing side channel
                        _strncmp_timing_side_channel(users[i].password, password, USERNAME_LEN);
                        return -2;
                    }
                }
                else {
                    if (strncmp(users[i].password, password, USERNAME_LEN)) {
                        // Return new UID:
                        return i;
                    }
                    else {
                        return -2;
                    }
                }
            }
        }
#else
        if (users[i].valid) {
            if (strncmp(users[i].name, username, USERNAME_LEN)) {
                if (strncmp(users[i].password, password, USERNAME_LEN)) {
                    // Return new UID:
                    return i;
                }
                else {
                    return -2;
                }
            }
        }
#endif
    }
    return -1;
}

/*
 * switch_user
 * Attempt to modify a process's permissions by logging them in as the requested user.
 *
 * Returns 0 on success, -1 on no such user, -2 on wrong password.
 */
uid_t switch_user(pcb_t *proc, char *username, char *password) {
    if (!proc) return -1;

    uid_t new_uid = login(username, password);
    if (new_uid < 0) {
        // Failed to login, return the login error code
        return new_uid;
    }

    proc->uid = new_uid;
    return 0;
}

/*
 * sysswitchuser
 * Syscall wrapper around switch_user
 *
 * Attempt to authenticate as a different user.
 * Returns 0 on success, -1 on no such name, -2 on wrong password.
 */
int32_t sysswitchuser(char *username, char *password) {
    int retcode = switch_user(current_proc, username, password);

    if (retcode >= 0) {
        display_user_colorscheme(current_proc->uid);
    }

    return retcode;
}

/*
 * sysgetuser
 *
 * Retrieves login information about the current user.
 * Returns 0 on success, -1 on failure
 */
int32_t sysgetuser (char *name, size_t name_size, uid_t *id) {
    if (!current_proc) return -1;
    if (!name) return -1;

    size_t min_bytes = name_size;
    if (USERNAME_LEN < min_bytes) { min_bytes = USERNAME_LEN; }

    strncpy(name, users[current_proc->uid].name, min_bytes);
    *id = current_proc->uid;
    return 0;
}

/*
 * sys_remote_switchuser
 *
 * Elevate a remote process to have the permissions of this current process.
 *
 * Does nothing if they are at our have higher privilege level than us.
 * Privilege level: lower UID = more privileged
 */
int32_t sys_remote_switchuser(uint32_t pid) {
    if (pid < 0 || pid > MAX_PROCESSES) return -1;
    if (!processes[pid].in_use) return -1;
    if (processes[pid].uid <= current_proc->uid) return -2;

    // We are gonna elevate privileges so forget the sandbox level:
    #ifdef UIUCTF
    if (processes[pid].uid == SANDBOX_USER) {
        sandbox_level = SANDBOX_NONE;
        current_typeable_printf("uiuctf{translation_l00kas1d3_what_aga1n?}");
    }
    #endif

    processes[pid].uid = current_proc->uid;

    // This is technically not correct, we should call this on the process when it gets
    // scheduled, but whatever this works:
    display_user_colorscheme(current_proc->uid);

    return 0;
}

/*
 * display_user_colorscheme
 *
 * Display the VGA colorscheme associated with current privilege level.
 * This is to make getting super user more exciting and fun!
 */
void display_user_colorscheme(uid_t uid_in) {
    if (vga_use_highres_gui) {
        char *mode;
        if (uid_in == ROOT_USER)
            mode = "SUPERUSER";
        else if (sandbox_level == SANDBOX_NONE)
            mode = "Regular User";
        else
            mode = "Sandbox Mode";
        gui_menubar(users[uid_in].name, mode);
        gui_redraw_menubar();
    }
    else {
        vga_clear_box(0,0,VGA_WIDTH,1); // Clear statusbar
        if (uid_in == ROOT_USER) {
            vga_setcolor(0x0F);
            vga_statusbar(0, 0x04);
            vga_printf_xy(VGA_WIDTH - 10,0,"SUPERUSER");
        }
        else {
            vga_setcolor(0xF0);
            vga_statusbar(0, 0x0F);
            vga_printf_xy(VGA_WIDTH - 10,0," Usermode");
        }
        vga_printf_centered(0,"jprx Kernel");
        vga_printf_xy(1,0,users[uid_in].name);
    }
}

/*
 * create_resource
 * Setup a resource object
 * Returns true on success, false on failure.
 */
bool create_resource (resource_t *res, uid_t uid, resource_kind_t access_level) {
    if (!res) return false;

    // Hmm, maybe we should check something here?
    // NAH :D (@CTF idea)
    res->uid = uid;
    res->kind = access_level;
    return true;
}

/*
 * access_ok
 * Check user access against a resource
 * Returns true on OK, false on NOT ALLOWED
 */
bool access_ok (uid_t uid, resource_t *res) {
    if (!res) return false;

    if (res->kind == RESOURCE_PUBLIC) return true;
    if (uid == 0) return true; // Kernel/ superuser can do whatever
        
    // For RESOURCE_PROTECTED we need an exact UID match
    return uid == res->uid;
}
