/****************************************************************************
 * Copyright (c) 1998,1999 Free Software Foundation, Inc.                   *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/


/*
**	lib_tstp.c
**
**	The routine _nc_signal_handler().
**
*/

#include <curses.priv.h>

#include <signal.h>
#include <SigAction.h>

#if defined(SVR4_ACTION) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

MODULE_ID("$Id: lib_tstp.c,v 1.21 2000/05/20 23:28:56 tom Exp $")

#if defined(SIGTSTP) && (HAVE_SIGACTION || HAVE_SIGVEC)
#define USE_SIGTSTP 1
#else
#define USE_SIGTSTP 0
#endif

/*
 * Note: This code is fragile!  Its problem is that different OSs
 * handle restart of system calls interrupted by signals differently.
 * The ncurses code needs signal-call restart to happen -- otherwise,
 * interrupted wgetch() calls will return FAIL, probably making the
 * application think the input stream has ended and it should
 * terminate.  In particular, you know you have this problem if, when
 * you suspend an ncurses-using lynx with ^Z and resume, it dies
 * immediately.
 *
 * Default behavior of POSIX sigaction(2) is not to restart
 * interrupted system calls, but Linux's sigaction does it anyway (at
 * least, on and after the 1.1.47 I (esr) use).  Thus this code works
 * OK under Linux.  The 4.4BSD sigaction(2) supports a (non-portable)
 * SA_RESTART flag that forces the right behavior.  Thus, this code
 * should work OK under BSD/OS, NetBSD, and FreeBSD (let us know if it
 * does not).
 *
 * Stock System Vs (and anything else using a strict-POSIX
 * sigaction(2) without SA_RESTART) may have a problem.  Possible
 * solutions:
 *
 *    sigvec      restarts by default (SV_INTERRUPT flag to not restart)
 *    signal      restarts by default in SVr4 (assuming you link with -lucb)
 *                and BSD, but not SVr3.
 *    sigset      restarts, but is only available under SVr4/Solaris.
 *
 * The signal(3) call is mandated by the ANSI standard, and its
 * interaction with sigaction(2) is described in the POSIX standard
 * (3.3.4.2, page 72,line 934).  According to section 8.1, page 191,
 * however, signal(3) itself is not required by POSIX.1.  And POSIX is
 * silent on whether it is required to restart signals.
 *
 * So.  The present situation is, we use sigaction(2) with no
 * guarantee of restart anywhere but on Linux and BSD.  We could
 * switch to signal(3) and collar Linux, BSD, and SVr4.  Any way
 * we slice it, System V UNIXes older than SVr4 will probably lose
 * (this may include XENIX).
 *
 * This implementation will probably be changed to use signal(3) in
 * the future.  If nothing else, it's simpler...
 */

#if USE_SIGTSTP
static void tstp(int dummy GCC_UNUSED)
{
	sigset_t mask, omask;
	sigaction_t act, oact;

#ifdef SIGTTOU
	int sigttou_blocked;
#endif

	T(("tstp() called"));

	/*
	 * The user may have changed the prog_mode tty bits, so save them.
	 *
	 * But first try to detect whether we still are in the foreground
	 * process group - if not, an interactive shell may already have
	 * taken ownership of the tty and modified the settings when our
	 * parent was stopped before us, and we would likely pick up the
	 * settings already modified by the shell.
	 */
	if (SP != 0 && !SP->_endwin) /* don't do this if we're not in curses */
#if HAVE_TCGETPGRP
	if (tcgetpgrp(STDIN_FILENO) == getpgrp())
#endif
	    def_prog_mode();

	/*
	 * Block window change and timer signals.  The latter
	 * is because applications use timers to decide when
	 * to repaint the screen.
	 */
	(void)sigemptyset(&mask);
	(void)sigaddset(&mask, SIGALRM);
#if USE_SIGWINCH
	(void)sigaddset(&mask, SIGWINCH);
#endif
	(void)sigprocmask(SIG_BLOCK, &mask, &omask);

#ifdef SIGTTOU
	sigttou_blocked = sigismember(&omask, SIGTTOU);
	if (!sigttou_blocked) {
	    (void)sigemptyset(&mask);
	    (void)sigaddset(&mask, SIGTTOU);
	    (void)sigprocmask(SIG_BLOCK, &mask, NULL);
	}
#endif

	/*
	 * End window mode, which also resets the terminal state to the
	 * original (pre-curses) modes.
	 */
	endwin();

	/* Unblock SIGTSTP. */
	(void)sigemptyset(&mask);
	(void)sigaddset(&mask, SIGTSTP);
#ifdef SIGTTOU
	if (!sigttou_blocked) {
            /* Unblock this too if it wasn't blocked on entry */
	    (void)sigaddset(&mask, SIGTTOU);
	}
#endif
	(void)sigprocmask(SIG_UNBLOCK, &mask, NULL);

	/* Now we want to resend SIGSTP to this process and suspend it */
	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */
	sigaction(SIGTSTP, &act, &oact);
	kill(getpid(), SIGTSTP);

	/* Process gets suspended...time passes...process resumes */

	T(("SIGCONT received"));
	sigaction(SIGTSTP, &oact, NULL);
	flushinp();

	/*
	 * If the user modified the tty state while suspended, he wants
	 * those changes to stick.  So save the new "default" terminal state.
	 */
	def_shell_mode();

	/*
	 * This relies on the fact that doupdate() will restore the
	 * program-mode tty state, and issue enter_ca_mode if need be.
	 */
	doupdate();

	/* Reset the signals. */
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
}
#endif	/* USE_SIGTSTP */

static void cleanup(int sig)
{
	static int nested;

	/*
	 * Actually, doing any sort of I/O from within an signal handler is
	 * "unsafe".  But we'll _try_ to clean up the screen and terminal
	 * settings on the way out.
	 */
	if (!nested++
	 && (sig == SIGINT
	  || sig == SIGQUIT)) {
#if HAVE_SIGACTION || HAVE_SIGVEC
		sigaction_t act;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = SIG_IGN;
		if (sigaction(sig, &act, (sigaction_t *)0) == 0)
#else
		if (signal(sig, SIG_IGN) != SIG_ERR)
#endif
		{
		    SCREEN *scan = _nc_screen_chain;
		    while(scan)
		    {
			if (SP != 0
			&& SP->_ofp != 0
			&& isatty(fileno(SP->_ofp))) {
			    SP->_cleanup = TRUE;
			    SP->_outch = _nc_outch;
			}
			set_term(scan);
			endwin();
			if (SP)
			    SP->_endwin = FALSE; /* in case we have an atexit! */
			scan = scan->_next_screen;
		    }
		}
	}
	exit(EXIT_FAILURE);
}

#if USE_SIGWINCH
static void sigwinch(int sig GCC_UNUSED)
{
    SCREEN *scan = _nc_screen_chain;
    while(scan)
    {
	scan->_sig_winch = TRUE;
	scan = scan->_next_screen;
    }
}
#endif /* USE_SIGWINCH */

/*
 * If the given signal is still in its default state, set it to the given
 * handler.
 */
#if HAVE_SIGACTION || HAVE_SIGVEC
static int CatchIfDefault(int sig, sigaction_t *act)
{
	sigaction_t old_act;

	if (sigaction(sig, (sigaction_t *)0, &old_act) == 0
	 && (old_act.sa_handler == SIG_DFL
#if USE_SIGWINCH
	    || (sig == SIGWINCH && old_act.sa_handler == SIG_IGN)
#endif
	    )) {
		(void)sigaction(sig, act, (sigaction_t *)0);
		return TRUE;
	}
	return FALSE;
}
#else
static int CatchIfDefault(int sig, RETSIGTYPE (*handler)(int))
{
	void	(*ohandler)(int);

	ohandler = signal(sig, SIG_IGN);
	if (ohandler == SIG_DFL
#if USE_SIGWINCH
	    || (sig == SIGWINCH && ohandler == SIG_IGN)
#endif
	) {
		signal(sig, handler);
		return TRUE;
	} else {
		signal(sig, ohandler);
		return FALSE;
	}
}
#endif

/*
 * This is invoked once at the beginning (e.g., from 'initscr()'), to
 * initialize the signal catchers, and thereafter when spawning a shell (and
 * returning) to disable/enable the SIGTSTP (i.e., ^Z) catcher.
 *
 * If the application has already set one of the signals, we'll not modify it
 * (during initialization).
 *
 * The XSI document implies that we shouldn't keep the SIGTSTP handler if
 * the caller later changes its mind, but that doesn't seem correct.
 */
void _nc_signal_handler(bool enable)
{
#if USE_SIGTSTP		/* Xenix 2.x doesn't have SIGTSTP, for example */
static sigaction_t act, oact;
static int ignore;

	if (!ignore)
	{
		if (!enable)
		{
			act.sa_handler = SIG_IGN;
			sigaction(SIGTSTP, &act, &oact);
		}
		else if (act.sa_handler)
		{
			sigaction(SIGTSTP, &oact, NULL);
		}
		else	/*initialize */
		{
			sigemptyset(&act.sa_mask);
			act.sa_flags = 0;
#if USE_SIGWINCH
			act.sa_handler = sigwinch;
			CatchIfDefault(SIGWINCH, &act);
#endif

#ifdef SA_RESTART
			act.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */
			act.sa_handler = cleanup;
			CatchIfDefault(SIGINT,  &act);
			CatchIfDefault(SIGTERM, &act);

			act.sa_handler = tstp;
			if (!CatchIfDefault(SIGTSTP, &act))
				ignore = TRUE;
		}
	}
#else /* !USE_SIGTSTP */
	if (enable)
	{
#if HAVE_SIGACTION || HAVE_SIGVEC
		static sigaction_t act;
		sigemptyset(&act.sa_mask);
#if USE_SIGWINCH
		act.sa_handler = sigwinch;
		CatchIfDefault(SIGWINCH, &act);
#endif
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */
		act.sa_handler = cleanup;
		CatchIfDefault(SIGINT,  &act);
		CatchIfDefault(SIGTERM, &act);

#else /* !(HAVE_SIGACTION || HAVE_SIGVEC) */

		CatchIfDefault(SIGINT,  cleanup);
		CatchIfDefault(SIGTERM, cleanup);
#if USE_SIGWINCH
		CatchIfDefault(SIGWINCH, sigwinch);
#endif
#endif /* !(HAVE_SIGACTION || HAVE_SIGVEC) */
	}
#endif /* !USE_SIGTSTP */
}
