#ifndef __libgxx_sys_wait_h

#include <_G_config.h>

extern "C" {
#ifdef __sys_wait_h_recursive
#include_next <sys/wait.h>
#else
#define __sys_wait_h_recursive


#if _G_HAVE_SYS_WAIT
#ifdef VMS
#include "GNU_CC_INCLUDE:[sys]wait.h"
#else
#include_next <sys/wait.h>
#endif
#else /* !_G_HAVE_SYS_WAIT */
/* Traditional definitions. */
#define WEXITSTATUS(status) (((x) >> 8) & 0xFF)
#define WIFSTOPPED(x) (((x) & 0xFF) == 0177)
#define WIFEXITED(x) (! WIFSTOPPED(x) && WTERMSIG(x) == 0)
#define WIFSIGNALED(x) (! WIFSTOPPED(x) && WTERMSIG(x) != 0)
#define WTERMSIG(status) ((x) & 0x7F)
#define WSTOPSIG(status) (((x) >> 8) & 0xFF)
#endif /* !_G_HAVE_SYS_WAIT */

#define __libgxx_sys_wait_h 1

struct rusage;
extern _G_pid_t wait _G_ARGS((int*));
extern _G_pid_t waitpid _G_ARGS((_G_pid_t, int*, int));
extern _G_pid_t wait3 _G_ARGS((int*, int options, struct rusage*));
#ifndef __386BSD__
extern _G_pid_t wait4 _G_ARGS((int, int*, int, struct rusage*));
#else
extern _G_pid_t wait4 _G_ARGS((_G_pid_t, int*, int, struct rusage*));
#endif
#endif
}

#endif
