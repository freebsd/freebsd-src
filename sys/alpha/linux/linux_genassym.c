/* $FreeBSD: src/sys/alpha/linux/linux_genassym.c,v 1.15.2.1 2000/11/04 07:30:08 obrien Exp $ */

#include <stddef.h>

#include <sys/param.h>
#include <sys/assym.h>

#include <alpha/linux/linux.h>

ASSYM(LINUX_SIGF_HANDLER, offsetof(struct linux_sigframe, sf_handler));
ASSYM(LINUX_SIGF_SC, offsetof(struct linux_sigframe, sf_sc));
/* ASSYM(LINUX_SC_GS, offsetof(struct linux_sigcontext, sc_gs)); */
/* ASSYM(LINUX_SC_EFLAGS, offsetof(struct linux_sigcontext, sc_eflags)); */
