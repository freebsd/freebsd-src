/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI signal.c,v 2.2 1996/04/08 19:33:06 bostic Exp
 *
 * $FreeBSD$
 */

#include "doscmd.h"

static void	(*handler[NSIG])(struct sigframe *);
static char	signal_stack[16 * 1024];
#define PSS(w) { char s; printf(w " @ %08x\n", (signal_stack + sizeof signal_stack) - &s); }

struct sigframe	*saved_sigframe;
regcontext_t *saved_regcontext;
int		saved_valid = 0; 

static void
sanity_check(struct sigframe *sf)
{
#if 0
    static sigset_t oset;
    int i;

    for (i = 1; i < 32; ++i) {
	if (sigismember(&sf->sf_sc.sc_mask, i) != sigismember(&oset, i))
	    fprintf(debugf, "Signal %s %s being blocked\n",
		    sys_signame[i],
		    sigismember(&sf->sf_sc.sc_mask, i) ? "now" : "no longer");
    }
    oset = sf->sf_sc.sc_mask;
#endif

    if (dead)
	fatal("attempting to return to vm86 while dead");
}

#if defined(__FreeBSD__) || defined(USE_VM86)
static void
generichandler(struct sigframe sf)
{
    if (sf.sf_uc.uc_mcontext.mc_eflags & PSL_VM) {
	saved_sigframe = &sf;
	saved_regcontext = (regcontext_t *)&(sf.sf_uc.uc_mcontext);
	saved_valid = 1;
	if (handler[sf.sf_signum])
	    (*handler[sf.sf_signum])(&sf);
	saved_valid = 0;
	sanity_check(&sf);
    } else {
	if (handler[sf.sf_signum])
	    (*handler[sf.sf_signum])(&sf);
    }
}
#else
#error BSD/OS sigframe/trapframe kernel interface not currently supported.
#endif

void
setsignal(int s, void (*h)(struct sigframe *))
{
    static int first = 1;
    struct sigaction sa;
    sigset_t set;

    if (first) {
	struct sigaltstack sstack;

	sstack.ss_sp = signal_stack;
	sstack.ss_size = sizeof signal_stack;
	sstack.ss_flags = 0;
	sigaltstack (&sstack, NULL);
	first = 0;
    }

    if (s >= 0 && s < NSIG) {
	handler[s] = h;

	sa.sa_handler = (__sighandler_t *)generichandler;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGIO);
	sigaddset(&sa.sa_mask, SIGALRM);
	sa.sa_flags = SA_ONSTACK;
	sigaction(s, &sa, NULL);

	sigaddset(&set, s);
	sigprocmask(SIG_UNBLOCK, &set, 0);
    }
}
