/* $Id$ */
/* Derived from:  Id: linux_genassym.c,v 1.8 1998/07/29 15:50:41 bde Exp */

#include <sys/param.h>

struct proc;

#include <svr4/svr4.h>
#include <svr4/svr4_signal.h>
#include <svr4/svr4_ucontext.h>

/* XXX: This bit sucks rocks, but gets rid of compiler errors.  Maybe I should
 * fix the include files instead... */
#define SVR4_MACHDEP_JUST_REGS
#include <i386/svr4/svr4_machdep.h>

#define	offsetof(type, member)	((size_t)(&((type *)0)->member))
#define	OS(s, m)	((u_int)offsetof(struct s, m))

int	main __P((void));
int	printf __P((const char *, ...));

int
main()
{
	printf("#define\tSVR4_SIGF_HANDLER %u\n",
	    OS(svr4_sigframe, sf_handler));
	printf("#define\tSVR4_SIGF_UC %u\n", OS(svr4_sigframe, sf_uc));
	printf("#define\tSVR4_UC_FS %u\n",
			OS(svr4_ucontext, uc_mcontext.greg[SVR4_X86_FS]));
	printf("#define\tSVR4_UC_GS %u\n",
			OS(svr4_ucontext, uc_mcontext.greg[SVR4_X86_GS]));
	printf("#define\tSVR4_UC_EFLAGS %u\n",
			OS(svr4_ucontext, uc_mcontext.greg[SVR4_X86_EFL]));
	return (0);
}
