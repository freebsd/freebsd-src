/* Derived from:  Id: linux_genassym.c,v 1.8 1998/07/29 15:50:41 bde Exp */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i386/svr4/svr4_genassym.c,v 1.9 2003/06/10 05:05:54 obrien Exp $");

#include <sys/param.h>
#include <sys/assym.h>
#include <sys/systm.h>

#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_ucontext.h>

/* XXX: This bit sucks rocks, but gets rid of compiler errors.  Maybe I should
 * fix the include files instead... */
#define SVR4_MACHDEP_JUST_REGS
#include <i386/svr4/svr4_machdep.h>

ASSYM(SVR4_SIGF_HANDLER, offsetof(struct svr4_sigframe, sf_handler));
ASSYM(SVR4_SIGF_UC, offsetof(struct svr4_sigframe, sf_uc));
ASSYM(SVR4_UC_FS, offsetof(struct svr4_ucontext,
    uc_mcontext.greg[SVR4_X86_FS]));
ASSYM(SVR4_UC_GS, offsetof(struct svr4_ucontext,
    uc_mcontext.greg[SVR4_X86_GS]));
ASSYM(SVR4_UC_EFLAGS, offsetof(struct svr4_ucontext,
    uc_mcontext.greg[SVR4_X86_EFL]));
