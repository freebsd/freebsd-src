/* $Id: linux_genassym.c,v 1.7 1998/02/01 18:47:56 bde Exp $ */

#include <sys/param.h>

#include <i386/linux/linux.h>

#define	offsetof(type, member)	((size_t)(&((type *)0)->member))
#define	OS(s, m)	((u_int)offsetof(struct s, m))

int	main __P((void));
int	printf __P((const char *, ...));

int
main()
{
	printf("#define\tLINUX_SIGF_HANDLER %u\n",
	    OS(linux_sigframe, sf_handler));
	printf("#define\tLINUX_SIGF_SC %u\n", OS(linux_sigframe, sf_sc));
	printf("#define\tLINUX_SC_FS %u\n", OS(linux_sigcontext, sc_fs));
	printf("#define\tLINUX_SC_GS %u\n", OS(linux_sigcontext, sc_gs));
	printf("#define\tLINUX_SC_EFLAGS %u\n",
	    OS(linux_sigcontext, sc_eflags));

	return (0);
}
