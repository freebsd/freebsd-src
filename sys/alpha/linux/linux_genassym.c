#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/alpha/linux/linux_genassym.c,v 1.19.22.1 2008/10/02 02:57:24 kensmith Exp $");

#include <sys/param.h>
#include <sys/assym.h>

#include <alpha/linux/linux.h>

ASSYM(LINUX_SIGF_HANDLER, offsetof(struct l_sigframe, sf_handler));
ASSYM(LINUX_SIGF_SC, offsetof(struct l_sigframe, sf_sc));
