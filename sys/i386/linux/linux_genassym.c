/* $FreeBSD$ */

#include <sys/param.h>
#include <sys/assym.h>

#include <i386/linux/linux.h>

ASSYM(LINUX_SIGF_HANDLER, offsetof(struct linux_sigframe, sf_handler));
ASSYM(LINUX_SIGF_SC, offsetof(struct linux_sigframe, sf_sc));
ASSYM(LINUX_SC_GS, offsetof(struct linux_sigcontext, sc_gs));
ASSYM(LINUX_SC_EFLAGS, offsetof(struct linux_sigcontext, sc_eflags));
ASSYM(LINUX_RT_SIGF_HANDLER, offsetof(struct linux_rt_sigframe, sf_handler));
ASSYM(LINUX_RT_SIGF_UC, offsetof(struct linux_rt_sigframe, sf_sc));


