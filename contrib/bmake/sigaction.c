/* NAME:
 *      sigact.c - fake sigaction(2)
 *
 * SYNOPSIS:
 *      #include "sigact.h"
 *
 *      int sigaction(int sig, struct sigaction *act,
 *                      struct sigaction *oact);
 *      int sigaddset(sigset_t *mask, int sig);
 *      int sigdelset(sigset_t *mask, int sig);
 *      int sigemptyset(sigset_t *mask);
 *      int sigfillset(sigset_t *mask);
 *      int sigismember(sigset_t *mask, int sig);
 *      int sigpending(sigset_t *set);
 *      int sigprocmask(int how, sigset_t *set, sigset_t *oset);
 *      int sigsuspend(sigset_t *mask);
 *
 *      SIG_HDLR (*Signal(int sig, SIG_HDLR (*disp)(int)))(int);
 *
 * DESCRIPTION:
 *      This is a fake sigaction implementation.  It uses
 *      sigsetmask(2) et al or sigset(2) and friends if
 *      available, otherwise it just uses signal(2).  If it
 *      thinks sigaction(2) really exists it compiles to "almost"
 *      nothing.
 *
 *      In any case it provides a Signal() function that is
 *      implemented in terms of sigaction().
 *      If not using signal(2) as part of the underlying
 *      implementation (USE_SIGNAL or USE_SIGMASK), and
 *      NO_SIGNAL is not defined, it also provides a signal()
 *      function that calls Signal().
 *
 *      The need for all this mucking about is the problems
 *      caused by mixing various signal handling mechanisms in
 *      the one process.  This module allows for a consistent
 *      POSIX compliant interface to whatever is actually
 *      available.
 *
 *      sigaction() allows the caller to examine and/or set the
 *      action to be associated with a given signal. "act" and
 *      "oact" are pointers to 'sigaction structs':
 *.nf
 *
 *      struct sigaction
 *      {
 *             SIG_HDLR  (*sa_handler)();
 *             sigset_t  sa_mask;
 *             int       sa_flags;
 *      };
 *.fi
 *
 *      SIG_HDLR is normally 'void' in the POSIX implementation
 *      and for most current systems.  On some older UNIX
 *      systems, signal handlers do not return 'void', so
 *      this implementation keeps 'sa_handler' inline with the
 *      hosts normal signal handling conventions.
 *      'sa_mask' controls which signals will be blocked while
 *      the selected signal handler is active.  It is not used
 *      in this implementation.
 *      'sa_flags' controls various semantics such as whether
 *      system calls should be automagically restarted
 *      (SA_RESTART) etc.  It is not used in this
 *      implementation.
 *      Either "act" or "oact" may be NULL in which case the
 *      appropriate operation is skipped.
 *
 *      sigaddset() adds "sig" to the sigset_t pointed to by "mask".
 *
 *      sigdelset() removes "sig" from the sigset_t pointed to
 *      by "mask".
 *
 *      sigemptyset() makes the sigset_t pointed to by "mask" empty.
 *
 *      sigfillset() makes the sigset_t pointed to by "mask"
 *      full ie. match all signals.
 *
 *      sigismember() returns true if "sig" is found in "*mask".
 *
 *      sigpending() is supposed to return "set" loaded with the
 *      set of signals that are blocked and pending for the
 *      calling process.  It does nothing in this impementation.
 *
 *      sigprocmask() is used to examine and/or change the
 *      signal mask for the calling process.  Either "set" or
 *      "oset" may be NULL in which case the appropriate
 *      operation is skipped.  "how" may be one of SIG_BLOCK,
 *      SIG_UNBLOCK or SIG_SETMASK.  If this package is built
 *      with USE_SIGNAL, then this routine achieves nothing.
 *
 *      sigsuspend() sets the signal mask to "*mask" and waits
 *      for a signal to be delivered after which the previous
 *      mask is restored.
 *
 *
 * RETURN VALUE:
 *      0==success, -1==failure
 *
 * BUGS:
 *      Since we fake most of this, don't expect fancy usage to
 *      work.
 *
 * AUTHOR:
 *      Simon J. Gerraty <sjg@crufty.net>
 */
/* COPYRIGHT:
 *      @(#)Copyright (c) 1992-2021, Simon J. Gerraty
 *
 *      This is free software.  It comes with NO WARRANTY.
 *      Permission to use, modify and distribute this source code
 *      is granted subject to the following conditions.
 *      1/ that that the above copyright notice and this notice
 *      are preserved in all copies and that due credit be given
 *      to the author.
 *      2/ that any changes to this code are clearly commented
 *      as such so that the author does get blamed for bugs
 *      other than his own.
 *
 *      Please send copies of changes and bug-fixes to:
 *      sjg@crufty.net
 *
 */
#ifndef lint
static char *RCSid = "$Id: sigact.c,v 1.8 2021/10/14 19:39:17 sjg Exp $";
#endif

#undef _ANSI_SOURCE		/* causes problems */

#include <signal.h>
#include <sys/cdefs.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
# ifdef NO_SIGSET
#   undef HAVE_SIGSET
# endif
# ifndef HAVE_SIGACTION
#   ifdef HAVE_SIGSETMASK
#     define USE_SIGMASK
#   else
#     ifdef HAVE_SIGSET
#       define USE_SIGSET
#     else
#       define USE_SIGNAL
#     endif
#   endif
# endif
#endif

/*
 * some systems have a faulty sigaction() implementation!
 * Allow us to bypass it.
 * Or they may have installed sigact.h as signal.h which is why
 * we have SA_NOCLDSTOP defined.
 */
#if !defined(SA_NOCLDSTOP) || defined(_SIGACT_H) || defined(USE_SIGNAL) || defined(USE_SIGSET) || defined(USE_SIGMASK)

/*
 * if we haven't been told,
 * try and guess what we should implement with.
 */
#if !defined(USE_SIGSET) && !defined(USE_SIGMASK) && !defined(USE_SIGNAL)
# if defined(sigmask) || defined(BSD) || defined(_BSD) && !defined(BSD41)
#   define USE_SIGMASK
# else
#   ifndef NO_SIGSET
#     define USE_SIGSET
#   else
#     define USE_SIGNAL
#   endif
# endif
#endif
/*
 * if we still don't know, we're in trouble
 */
#if !defined(USE_SIGSET) && !defined(USE_SIGMASK) && !defined(USE_SIGNAL)
error must know what to implement with
#endif

#include "sigact.h"

/*
 * in case signal() has been mapped to our Signal().
 */
#undef signal


int
sigaction(int sig,
    const struct sigaction *act,
    struct sigaction *oact)
{
	SIG_HDLR(*oldh) ();

	if (act) {
#ifdef USE_SIGSET
		oldh = sigset(sig, act->sa_handler);
#else
		oldh = signal(sig, act->sa_handler);
#endif
	} else {
		if (oact) {
#ifdef USE_SIGSET
			oldh = sigset(sig, SIG_IGN);
#else
			oldh = signal(sig, SIG_IGN);
#endif
			if (oldh != SIG_IGN && oldh != SIG_ERR) {
#ifdef USE_SIGSET
				(void) sigset(sig, oldh);
#else
				(void) signal(sig, oldh);
#endif
			}
		}
	}
	if (oact) {
		oact->sa_handler = oldh;
	}
	return 0;		/* hey we're faking it */
}

#ifndef HAVE_SIGADDSET
int
sigaddset(sigset_t *mask, int sig)
{
	*mask |= sigmask(sig);
	return 0;
}


int
sigdelset(sigset_t *mask, int sig)
{
	*mask &= ~(sigmask(sig));
	return 0;
}


int
sigemptyset(sigset_t *mask)
{
	*mask = 0;
	return 0;
}


int
sigfillset(sigset_t *mask)
{
	*mask = ~0;
	return 0;
}


int
sigismember(const sigset_t *mask, int sig)
{
	return ((*mask) & sigmask(sig));
}
#endif

#ifndef HAVE_SIGPENDING
int
sigpending(sigset_t *set)
{
	return 0;		/* faking it! */
}
#endif

#ifndef HAVE_SIGPROCMASK
int
sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
#ifdef USE_SIGSET
	int i;
#endif
	static sigset_t sm;
	static int once = 0;

	if (!once) {
		/*
	         * initally we clear sm,
	         * there after, it represents the last
	         * thing we did.
	         */
		once++;
#ifdef USE_SIGMASK
		sm = sigblock(0);
#else
		sm = 0;
#endif
	}
	if (oset)
		*oset = sm;
	if (set) {
		switch (how) {
		case SIG_BLOCK:
			sm |= *set;
			break;
		case SIG_UNBLOCK:
			sm &= ~(*set);
			break;
		case SIG_SETMASK:
			sm = *set;
			break;
		}
#ifdef USE_SIGMASK
		(void) sigsetmask(sm);
#else
#ifdef USE_SIGSET
		for (i = 1; i < NSIG; i++) {
			if (how == SIG_UNBLOCK) {
				if (*set & sigmask(i))
					sigrelse(i);
			} else
				if (sm & sigmask(i)) {
					sighold(i);
				}
		}
#endif
#endif
	}
	return 0;
}
#endif

#ifndef HAVE_SIGSUSPEND
int
sigsuspend(sigset_t *mask)
{
#ifdef USE_SIGMASK
	sigpause(*mask);
#else
	int i;

#ifdef USE_SIGSET

	for (i = 1; i < NSIG; i++) {
		if (*mask & sigmask(i)) {
			/* not the same sigpause() as above! */
			sigpause(i);
			break;
		}
	}
#else				/* signal(2) only */
	SIG_HDLR(*oldh) ();

	/*
         * make sure that signals in mask will not
         * be ignored.
         */
	for (i = 1; i < NSIG; i++) {
		if (*mask & sigmask(i)) {
			if ((oldh = signal(i, SIG_DFL)) != SIG_ERR &&
			    oldh != SIG_IGN &&
			    oldh != SIG_DFL)
				(void) signal(i, oldh);	/* restore handler */
		}
	}
	pause();		/* wait for a signal */
#endif
#endif
	return 0;
}
#endif
#endif				/* ! SA_NOCLDSTOP */

#if 0
#if !defined(SIG_HDLR)
#define SIG_HDLR void
#endif
#if !defined(SIG_ERR)
#define SIG_ERR	(SIG_HDLR (*)())-1
#endif

#if !defined(USE_SIGNAL) && !defined(USE_SIGMASK) && !defined(NO_SIGNAL)
/*
 * ensure we avoid signal mayhem
 */

extern void (*Signal (int sig, void (*handler) (int)))(int);

SIG_HDLR(*signal(int sig, SIG_HDLR(*handler)(int))
{
	return (Signal(sig, handler));
}
#endif
#endif

/* This lot (for GNU-Emacs) goes at the end of the file. */
/*
 * Local Variables:
 * version-control:t
 * comment-column:40
 * End:
 */
