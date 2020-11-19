#ifndef SANDBOX_H
#define SANDBOX_H

// Any place an exploit is added should wrap an ifdef around this:
#define UIUCTF ((1))

#ifdef UIUCTF
// Sandbox levels of access for UIUCTF:
extern uint32_t sandbox_level;

// When true passwords have a timing attack
extern bool password_timing_side_channel;

// Entry level
#define SANDBOX_NONE ((0))
#define SANDBOX_1 ((245464))
#define SANDBOX_2 ((543213))
// #define SANDBOX_3 ((7072130))

// Lowest ID allowed to login when in sandbox mode:
#define SANDBOX_RESTRICTED_LOGIN_ID ((2))

// Freaky file descriptors:
#define FREAKY_LEAK_LEN ((40))
#define FREAKY_FILE_LEN ((243))

#endif

#endif
