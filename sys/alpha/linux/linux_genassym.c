/* $FreeBSD: src/sys/alpha/linux/linux_genassym.c,v 1.15.2.2 2001/11/05 19:08:22 marcel Exp $ */

#include <stddef.h>

#include <sys/param.h>
#include <sys/assym.h>

#include <alpha/linux/linux.h>

ASSYM(LINUX_SIGF_HANDLER, offsetof(struct l_sigframe, sf_handler));
ASSYM(LINUX_SIGF_SC, offsetof(struct l_sigframe, sf_sc));
