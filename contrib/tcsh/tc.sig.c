/* $Header: /src/pub/tcsh/tc.sig.c,v 3.25 2000/07/04 19:46:24 christos Exp $ */
/*
 * tc.sig.c: Signal routine emulations
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "sh.h"

RCSID("$Id: tc.sig.c,v 3.25 2000/07/04 19:46:24 christos Exp $")

#include "tc.wait.h"

#ifndef BSDSIGS

/* this stack is used to queue signals
 * we can handle up to MAX_CHLD outstanding children now;
 */
#define MAX_CHLD 50

# ifdef UNRELSIGS
static struct mysigstack {
    int     s_w;		/* wait report			 */
    int     s_errno;		/* errno returned;		 */
    pid_t   s_pid;		/* pid returned			 */
}       stk[MAX_CHLD];
static int stk_ptr = -1;


/* queue child signals
 */
static sigret_t
sig_ch_queue()
{
#  ifdef JOBDEBUG
    xprintf("queue SIGCHLD\n");
    flush();
#  endif /* JOBDEBUG */
    stk_ptr++;
    stk[stk_ptr].s_pid = (pid_t) wait(&stk[stk_ptr].s_w);
    stk[stk_ptr].s_errno = errno;
    (void) signal(SIGCHLD, sig_ch_queue);
#  ifndef SIGVOID
    return(0);
#  endif /* SIGVOID */
}

/* process all awaiting child signals
 */
static sigret_t
sig_ch_rel()
{
    while (stk_ptr > -1)
	pchild(SIGCHLD);
#  ifdef JOBDEBUG
    xprintf("signal(SIGCHLD, pchild);\n");
#  endif /* JOBDEBUG */
    (void) signal(SIGCHLD, pchild);
#  ifndef SIGVOID
    return(0);
#  endif /* SIGVOID */
}


/* libc.a contains these functions in SYSVREL >= 3. */
sigret_t
(*xsigset(a, b)) ()
    int     a;
    signalfun_t  b;
{
    return (signal(a, b));
}

/* release signal
 *	release all queued signals and
 *	set the default signal handler
 */
void
sigrelse(what)
    int     what;
{
    if (what == SIGCHLD)
	sig_ch_rel();

#  ifdef COHERENT
    (void) signal(what, what == SIGINT ? pintr : SIG_DFL);
#  endif /* COHERENT */
}

/* hold signal
 * only works with child and interrupt
 */
void
xsighold(what)
    int     what;
{
    if (what == SIGCHLD)
	(void) signal(SIGCHLD, sig_ch_queue);

#  ifdef COHERENT
    (void) signal(what, SIG_IGN);
#  endif /* COHERENT */
}

/* ignore signal
 */
void
xsigignore(a)
    int     a;
{
    (void) signal(a, SIG_IGN);
}

/* atomically release one signal
 */
void
xsigpause(what)
    int     what;
{
    /* From: Jim Mattson <mattson%cs@ucsd.edu> */
    if (what == SIGCHLD)
	pchild(SIGCHLD);
}


/* return either awaiting processes or do a wait now
 */
pid_t
ourwait(w)
    int    *w;
{
    pid_t pid;

#  ifdef JOBDEBUG
    xprintf(CGETS(25, 1, "our wait %d\n"), stk_ptr);
    flush();
#  endif /* JOBDEBUG */

    if (stk_ptr == -1) {
	/* stack empty return signal from stack */
	pid = (pid_t) wait(w);
#  ifdef JOBDEBUG
	xprintf("signal(SIGCHLD, pchild);\n");
#  endif /* JOBDEBUG */
	(void) signal(SIGCHLD, pchild);
	return (pid);
    }
    else {
	/* return signal from stack */
	errno = stk[stk_ptr].s_errno;
	*w = stk[stk_ptr].s_w;
	stk_ptr--;
	return (stk[stk_ptr + 1].s_pid);
    }
} /* end ourwait */

#  ifdef COHERENT
#   undef signal
sigret_t
(*xsignal(a, b)) ()
    int     a;
    signalfun_t  b;
{
    if (a == SIGCHLD)
	return SIG_DFL;
    else
	return (signal(a, b));
}
#  endif /* COHERENT */

# endif /* UNRELSIGS */

# ifdef SXA
/*
 * SX/A is SYSVREL3 but does not have sys5-sigpause().
 * I've heard that sigpause() is not defined in SYSVREL3.
 */
/* This is not need if you make tcsh by BSD option's cc. */
void
sigpause(what)
{
    if (what == SIGCHLD) {
	(void) bsd_sigpause(bsd_sigblock((sigmask_t) 0) & ~sigmask(SIGBSDCHLD));
    }
    else if (what == 0) {
	pause();
    }
    else {
	xprintf("sigpause(%d)\n", what);
	pause();
    }
}
# endif /* SXA */

#endif /* !BSDSIGS */

#ifdef NEEDsignal
/* turn into bsd signals */
sigret_t
(*xsignal(s, a)) ()
    int     s;
    signalfun_t a;
{
    sigvec_t osv, sv;

    (void) mysigvec(s, NULL, &osv);
    sv = osv;
    sv.sv_handler = a;
#ifdef SIG_STK
    sv.sv_onstack = SIG_STK;
#endif /* SIG_STK */
#ifdef SV_BSDSIG
    sv.sv_flags = SV_BSDSIG;
#endif /* SV_BSDSIG */

    if (mysigvec(s, &sv, NULL) < 0)
	return (BADSIG);
    return (osv.sv_handler);
}

#endif /* NEEDsignal */

#ifdef POSIXSIGS
/*
 * Support for signals.
 */

extern int errno;

/* Set and test a bit.  Bits numbered 1 to 32 */

#define SETBIT(x, y)	x |= sigmask(y)
#define ISSET(x, y)	((x & sigmask(y)) != 0)

#ifdef DEBUG
# define SHOW_SIGNALS	1	/* to assist in debugging signals */
#endif /* DEBUG */

#ifdef SHOW_SIGNALS
char   *show_sig_mask();
#endif /* SHOW_SIGNALS */

#ifndef __PARAGON__
/*
 * sigsetmask(mask)
 *
 * Set a new signal mask.  Return old mask.
 */
sigmask_t
sigsetmask(mask)
    sigmask_t     mask;
{
    sigset_t set, oset;
    int     m;
    register int i;

    (void) sigemptyset(&set);
    (void) sigemptyset(&oset);

    for (i = 1; i <= MAXSIG; i++)
	if (ISSET(mask, i))
	    (void) sigaddset(&set, i);

    if ((sigprocmask(SIG_SETMASK, &set, &oset)) == -1) {
	xprintf("sigsetmask(0x%x) - sigprocmask failed, errno %d",
		mask, errno);
    }

    m = 0;
    for (i = 1; i <= MAXSIG; i++)
	if (sigismember(&oset, i))
	    SETBIT(m, i);

    return (m);
}
#endif /* __PARAGON__ */

#ifndef __DGUX__
/*
 * sigblock(mask)
 *
 * Add "mask" set of signals to the present signal mask.
 * Return old mask.
 */
sigmask_t
sigblock(mask)
    sigmask_t     mask;
{
    sigset_t set, oset;
    int     m;
    register int i;

    (void) sigemptyset(&set);
    (void) sigemptyset(&oset);

    /* Get present set of signals. */
    if ((sigprocmask(SIG_SETMASK, NULL, &set)) == -1)
	stderror(ERR_SYSTEM, "sigprocmask", strerror(errno));

    /* Add in signals from mask. */
    for (i = 1; i <= MAXSIG; i++)
	if (ISSET(mask, i))
	    (void) sigaddset(&set, i);

    if ((sigprocmask(SIG_SETMASK, &set, &oset)) == -1)
	stderror(ERR_SYSTEM, "sigprocmask", strerror(errno));

    /* Return old mask to user. */
    m = 0;
    for (i = 1; i <= MAXSIG; i++)
	if (sigismember(&oset, i))
	    SETBIT(m, i);

    return (m);
}
#endif /* __DGUX__ */


/*
 * bsd_sigpause(mask)
 *
 * Set new signal mask and wait for signal;
 * Old mask is restored on signal.
 */
void
bsd_sigpause(mask)
    sigmask_t     mask;
{
    sigset_t set;
    register int i;

    (void) sigemptyset(&set);

    for (i = 1; i <= MAXSIG; i++)
	if (ISSET(mask, i))
	    (void) sigaddset(&set, i);
    (void) sigsuspend(&set);
}

/*
 * bsd_signal(sig, func)
 *
 * Emulate bsd style signal()
 */
sigret_t (*bsd_signal(sig, func)) ()
        int sig;
        signalfun_t func;
{
        struct sigaction act, oact;
        sigset_t set;
        signalfun_t r_func;

        if (sig < 0 || sig > MAXSIG) {
                xprintf(CGETS(25, 2,
			"error: bsd_signal(%d) signal out of range\n"), sig);
                return((signalfun_t) SIG_IGN);
        }

        (void) sigemptyset(&set);

        act.sa_handler = (signalfun_t) func; /* user function */
        act.sa_mask = set;                      /* signal mask */
        act.sa_flags = 0;                       /* no special actions */

        if (sigaction(sig, &act, &oact)) {
                xprintf(CGETS(25, 3,
			"error: bsd_signal(%d) - sigaction failed, errno %d\n"),
			sig, errno);
                return((signalfun_t) SIG_IGN);
        }

        r_func = (signalfun_t) oact.sa_handler;
        return(r_func);
}
#endif /* POSIXSIG */


#ifdef SIGSYNCH
static long Synch_Cnt = 0;

sigret_t
synch_handler(sno)
int sno;
{
    if (sno != SIGSYNCH)
	abort();
    Synch_Cnt++;
}
#endif /* SIGSYNCH */
