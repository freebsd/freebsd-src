/* $FreeBSD: src/sys/alpha/linux/linux_genassym.c,v 1.17 2002/09/17 07:22:23 peter Exp $ */

#include <sys/param.h>
#include <sys/assym.h>

#include <alpha/linux/linux.h>

ASSYM(LINUX_SIGF_HANDLER, offsetof(struct l_sigframe, sf_handler));
ASSYM(LINUX_SIGF_SC, offsetof(struct l_sigframe, sf_sc));
