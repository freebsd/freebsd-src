#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/amd64/linux32/linux32_genassym.c,v 1.1.18.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/assym.h>
#include <sys/systm.h>

#include <amd64/linux32/linux.h>

ASSYM(LINUX_SIGF_HANDLER, offsetof(struct l_sigframe, sf_handler));
ASSYM(LINUX_SIGF_SC, offsetof(struct l_sigframe, sf_sc));
ASSYM(LINUX_SC_ES, offsetof(struct l_sigcontext, sc_es));
ASSYM(LINUX_SC_DS, offsetof(struct l_sigcontext, sc_ds));
ASSYM(LINUX_RT_SIGF_HANDLER, offsetof(struct l_rt_sigframe, sf_handler));
ASSYM(LINUX_RT_SIGF_UC, offsetof(struct l_rt_sigframe, sf_sc));
ASSYM(LINUX_RT_SIGF_SC, offsetof(struct l_ucontext, uc_mcontext));
