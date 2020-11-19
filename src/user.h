#ifndef USER_H
#define USER_H

#include "types.h"
#include "typeable.h"

// User-related methods

/*
 * Protection Strategy
 *
 * There are two categories of file- protected and public.
 * Protected resources can be only accessed with a UID equal to the owner UID (or equal to 0)
 * Public resources can be accessed by any UID
 */

#define USERNAME_LEN ((32))
#define MAX_NUM_USERS ((8))

// UIDs of accounts on the system
#define ROOT_USER ((0))
#define USER_USER ((1))
#define SANDBOX_USER ((2))

/*
 * uid
 * 0 = root
 * -1 = invalid user
 * The rest are associated with user_t accounts
 */
typedef int32_t uid_t;

typedef struct user_t {
    char name[USERNAME_LEN];

    // Yeah this is secure DW about it
    // @TODO: Hash these
    // Better yet, read it dynamically so password hashes aren't sitting around in kernel memory
    char password[USERNAME_LEN];

    // Valid user?
    // Need this to prevent entering no username and no password to login as the next available user_t
    // because the end of the users list is just a bunch of users with name "".
    bool valid;
} user_t;

// Any protected resource:
typedef enum {RESOURCE_PROTECTED, RESOURCE_PUBLIC} resource_kind_t;
typedef struct resource_t {
    // UID associated with this process:
    uid_t uid;

    // Type of resource (PROTECTED or PUBLIC):
    resource_kind_t kind;
} resource_t;

/*
 * users
 * The global array of all users
 * UIDs are indeces into this array
 */
extern user_t users[MAX_NUM_USERS];

/*
 * load_users_from_file
 *
 * Given a path, treat is a user initialization file and read it into the users array, configuring it correctly hopefully.
 */
bool load_users_from_file (char *file);

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
 * Returns true on success, false on failure.
 * Returns number of users loaded, -1 on failure
 */
int load_users(const char *name_db);

/*
 * login_window
 * Doesn't return until the user successfully logs in
 *
 * Draws a VGA login window to the screen and uses the users array to check against passwords.
 */
uid_t login_window(bool animate);

/*
 * login
 * Attempt to login to the system
 *
 * Returns UID on success, -1 (invalid UID) on failure
 */
uid_t login(char *username, char *password);

/*
 * switch_user
 * Attempt to modify a process's permissions by logging them in as the requested user.
 *
 * Returns 0 on success, -1 on no such name, -2 on wrong password.
 */
struct pcb_t;
uid_t switch_user(struct pcb_t *proc, char *username, char *password);

/*
 * display_user_colorscheme
 *
 * Display the VGA colorscheme associated with current privilege level.
 * This is to make getting super user more exciting and fun!
 */
void display_user_colorscheme(uid_t uid);

/*
 * create_resource
 * Setup a resource object
 * Returns true on success, false on failure.
 */
bool create_resource (resource_t *res, uid_t uid, resource_kind_t access_level);

/*
 * access_ok
 * Check user access against a resource
 * Returns true on OK, false on NOT ALLOWED
 */
bool access_ok (uid_t uid, resource_t *res);

/*
 * system_resource
 *
 * The resource identifier associated with any system-related (kernel) objects
 * Example: right to modify interrupt tables, right to crash (reboot) system, etc.
 * In the future this could serve as a semaphore of sorts as well.
 */
extern resource_t system_resource;

#endif