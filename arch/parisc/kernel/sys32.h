#ifndef _PARISC64_KERNEL_SYS32_H
#define _PARISC64_KERNEL_SYS32_H

/* Call a kernel syscall which will use kernel space instead of user
 * space for its copy_to/from_user.
 */
#define KERNEL_SYSCALL(ret, syscall, args...) \
{ \
    mm_segment_t old_fs = get_fs(); \
    set_fs(KERNEL_DS); \
    ret = syscall(args); \
    set_fs (old_fs); \
}

struct timeval32 {
	int tv_sec;
	int tv_usec;
};

typedef __u32 __sighandler_t32;

#include <linux/signal.h>
typedef struct {
	unsigned int sig[_NSIG_WORDS * 2];
} sigset_t32;

struct sigaction32 {
	__sighandler_t32 sa_handler;
	unsigned int sa_flags;
	sigset_t32 sa_mask;		/* mask last for extensibility */
};

#endif
