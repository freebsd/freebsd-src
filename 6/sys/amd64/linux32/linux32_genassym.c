#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/assym.h>
#include <sys/systm.h>

#include <amd64/linux32/linux.h>

ASSYM(LINUX_SIGF_HANDLER, offsetof(struct l_sigframe, sf_handler));
ASSYM(LINUX_SIGF_SC, offsetof(struct l_sigframe, sf_sc));
ASSYM(LINUX_SC_GS, offsetof(struct l_sigcontext, sc_gs));
ASSYM(LINUX_SC_FS, offsetof(struct l_sigcontext, sc_fs));
ASSYM(LINUX_SC_ES, offsetof(struct l_sigcontext, sc_es));
ASSYM(LINUX_SC_DS, offsetof(struct l_sigcontext, sc_ds));
ASSYM(LINUX_RT_SIGF_HANDLER, offsetof(struct l_rt_sigframe, sf_handler));
ASSYM(LINUX_RT_SIGF_UC, offsetof(struct l_rt_sigframe, sf_sc));
