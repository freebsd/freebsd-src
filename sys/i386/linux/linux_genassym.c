/* $Id: linux_genassym.c,v 1.2 1996/03/02 21:00:10 peter Exp $ */
#include <stdio.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <i386/linux/linux.h>

#define	offsetof(type, member)	((size_t)(&((type *)0)->member))
#define	OS(s, m)		((u_int)offsetof(struct s, m))

int	main __P((void));

int
main()
{
#if	0
	struct linux_sigframe *linux_sigf = (struct linux_sigframe *)0;
	struct linux_sigcontext *linux_sc = (struct linux_sigcontext *)0;

	printf("#define\tLINUX_SIGF_HANDLER %d\n", &linux_sigf->sf_handler);
	printf("#define\tLINUX_SIGF_SC %d\n", &linux_sigf->sf_sc);
	printf("#define\tLINUX_SC_FS %d\n", &linux_sc->sc_fs);
	printf("#define\tLINUX_SC_GS %d\n", &linux_sc->sc_gs);
	printf("#define\tLINUX_SC_EFLAGS %d\n", &linux_sc->sc_eflags);
#else
	printf("#define\tLINUX_SIGF_HANDLER %u\n",
		OS(linux_sigframe, sf_handler));
	printf("#define\tLINUX_SIGF_SC %u\n", OS(linux_sigframe, sf_sc));
	printf("#define\tLINUX_SC_FS %u\n", OS(linux_sigcontext, sc_fs));
	printf("#define\tLINUX_SC_GS %u\n", OS(linux_sigcontext, sc_gs));
	printf("#define\tLINUX_SC_EFLAGS %u\n",
		OS(linux_sigcontext, sc_eflags));
#endif
	return (0);
}
