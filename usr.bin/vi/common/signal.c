/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char sccsid[] = "@(#)signal.c	8.34 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"

static void	h_alrm __P((int));
static void	h_hup __P((int));
static void	h_int __P((int));
static void	h_term __P((int));
static void	h_winch __P((int));
static void	sig_sync __P((int, u_int));

/*
 * There are seven normally asynchronous actions about which vi cares:
 * SIGALRM, SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGTSTP and SIGWINCH.
 *
 * The assumptions:
 *	1: The DB routines are not reentrant.
 *	2: The curses routines may not be reentrant.
 *
 * SIGALRM, SIGHUP, SIGTERM
 *	Used for file recovery.  The DB routines can't be reentered, so
 *	the vi routines that call DB block all three signals (see line.c).
 *	This means that DB routines can be called at interrupt time.
 *
 * SIGALRM
 *	Used to paint busy messages on the screen.  The curses routines
 *	can't be reentered, so this function of SIGALRM can only be used
 *	in sections of code that do not use any curses functions (see
 *	busy_on, busy_off in signal.c).  This means that curses can be
 *	called at interrupt time.
 *
 * SIGQUIT
 *	Disabled by the signal initialization routines.  Historically,
 *	^\ switched vi into ex mode, and we continue that practice.
 *
 * SIGWINCH:
 *	The interrupt routine sets a global bit which is checked by the
 *  	key-read routine, so there are no reentrancy issues.  This means
 *	that the screen will not resize until vi runs out of keys, but
 *	that doesn't seem like a problem.
 *
 * SIGINT and SIGTSTP are a much more difficult issue to resolve.  Vi has
 * to permit the user to interrupt long-running operations.  Generally, a
 * search, substitution or read/write is done on a large file, or, the user
 * creates a key mapping with an infinite loop.  This problem will become
 * worse as more complex semantics are added to vi.  There are four major
 * solutions on the table, each of which have minor permutations.
 *
 * 1:	Run in raw mode.
 *
 *	The up side is that there's no asynchronous behavior to worry about,
 *	and obviously no reentrancy problems.  The down side is that it's easy
 *	to misinterpret characters (e.g. :w big_file^Mi^V^C is going to look
 *	like an interrupt) and it's easy to get into places where we won't see
 *	interrupt characters (e.g. ":map a ixx^[hxxaXXX" infinitely loops in
 *	historic implementations of vi).  Periodically reading the terminal
 *	input buffer might solve the latter problem, but it's not going to be
 *	pretty.
 *
 *	Also, we're going to be checking for ^C's and ^Z's both, all over
 *	the place -- I hate to litter the source code with that.  For example,
 *	the historic version of vi didn't permit you to suspend the screen if
 *	you were on the colon command line.  This isn't right.  ^Z isn't a vi
 *	command, it's a terminal event.  (Dammit.)
 *
 * 2:	Run in cbreak mode.  There are two problems in this area.  First, the
 *	current curses implementations (both System V and Berkeley) don't give
 *	you clean cbreak modes. For example, the IEXTEN bit is left on, turning
 *	on DISCARD and LNEXT.  To clarify, what vi WANTS is 8-bit clean, with
 *	the exception that flow control and signals are turned on, and curses
 *	cbreak mode doesn't give you this.
 *
 *	We can either set raw mode and twiddle the tty, or cbreak mode and
 *	twiddle the tty.  I chose to use raw mode, on the grounds that raw
 *	mode is better defined and I'm less likely to be surprised by a curses
 *	implementation down the road.  The twiddling consists of setting ISIG,
 *	IXON/IXOFF, and disabling some of the interrupt characters (see the
 *	comments in svi/svi_screen.c).  This is all found in historic System
 *	V (SVID 3) and POSIX 1003.1-1992, so it should be fairly portable.
 *
 *	The second problem is that vi permits you to enter literal signal
 *	characters, e.g. ^V^C.  There are two possible solutions.  First, you
 *	can turn off signals when you get a ^V, but that means that a network
 *	packet containing ^V and ^C will lose, since the ^C may take effect
 *	before vi reads the ^V.  (This is particularly problematic if you're
 *	talking over a protocol that recognizes signals locally and sends OOB
 *	packets when it sees them.)  Second, you can turn the ^C into a literal
 *	character in vi, but that means that there's a race between entering
 *	^V<character>^C, i.e. the sequence may end up being ^V^C<character>.
 *	Also, the second solution doesn't work for flow control characters, as
 *	they aren't delivered to the program as signals.
 *
 *	Generally, this is what historic vi did.  (It didn't have the curses
 *	problems because it didn't use curses.)  It entered signals following
 *	^V characters into the input stream, (which is why there's no way to
 *	enter a literal flow control character).
 *
 * 3:	Run in mostly raw mode; turn signals on when doing an operation the
 *	user might want to interrupt, but leave them off most of the time.
 *
 *	This works well for things like file reads and writes.  This doesn't
 *	work well for trying to detect infinite maps.  The problem is that
 *	you can write the code so that you don't have to turn on interrupts
 *	per keystroke, but the code isn't pretty and it's hard to make sure
 *	that an optimization doesn't cover up an infinite loop.  This also
 *	requires interaction or state between the vi parser and the key
 *	reading routines, as an infinite loop may still be returning keys
 *	to the parser.
 *
 *	Also, if the user inserts an interrupt into the tty queue while the
 *	interrupts are turned off, the key won't be treated as an interrupt,
 *	and requiring the user to pound the keyboard to catch an interrupt
 *	window is nasty.
 *
 * 4:	Run in mostly raw mode, leaving signals on all of the time.  Done
 *	by setting raw mode, and twiddling the tty's termios ISIG bit.
 *
 *	This works well for the interrupt cases, because the code only has
 *	to check to see if the interrupt flag has been set, and can otherwise
 *	ignore signals.  It's also less likely that we'll miss a case, and we
 *	don't have to worry about synchronizing between the vi parser and the
 *	key read routines.
 *
 *	The down side is that we have to turn signals off if the user wants
 *	to enter a literal character (e.g. ^V^C).  If the user enters the
 *	combination fast enough, or as part of a single network packet,
 *	the text input routines will treat it as a signal instead of as a
 *	literal character.  To some extent, we have this problem already,
 *	since we turn off flow control so that the user can enter literal
 *	XON/XOFF characters.
 *
 *	This is probably the easiest to code, and provides the smoothest
 *	programming interface.
 *
 * There are a couple of other problems to consider.
 *
 * First, System V's curses doesn't handle SIGTSTP correctly.  If you use the
 * newterm() interface, the TSTP signal will leave you in raw mode, and the
 * final endwin() will leave you in the correct shell mode.  If you use the
 * initscr() interface, the TSTP signal will return you to the correct shell
 * mode, but the final endwin() will leave you in raw mode.  There you have
 * it: proof that drug testing is not making any significant headway in the
 * computer industry.  The 4BSD curses is deficient in that it does not have
 * an interface to the terminal keypad.  So, regardless, we have to do our
 * own SIGTSTP handling.
 *
 * The problem with this is that if we do our own SIGTSTP handling, in either
 * models #3 or #4, we're going to have to call curses routines at interrupt
 * time, which means that we might be reentering curses, which is something we
 * don't want to do.
 *
 * Second, SIGTSTP has its own little problems.  It's broadcast to the entire
 * process group, not sent to a single process.  The scenario goes something
 * like this: the shell execs the mail program, which execs vi.  The user hits
 * ^Z, and all three programs get the signal, in some random order.  The mail
 * program goes to sleep immediately (since it probably didn't have a SIGTSTP
 * handler in place).  The shell gets a SIGCHLD, does a wait, and finds out
 * that the only child in its foreground process group (of which it's aware)
 * is asleep.  It then optionally resets the terminal (because the modes aren't
 * how it left them), and starts prompting the user for input.  The problem is
 * that somewhere in the middle of all of this, vi is resetting the terminal,
 * and getting ready to send a SIGTSTP to the process group in order to put
 * itself to sleep.  There's a solution to all of this: when vi starts, it puts
 * itself into its own process group, and then only it (and possible child
 * processes) receive the SIGTSTP.  This permits it to clean up the terminal
 * and switch back to the original process group, where it sends that process
 * group a SIGTSTP, putting everyone to sleep and waking the shell.
 *
 * Third, handing SIGTSTP asynchronously is further complicated by the child
 * processes vi may fork off.  If vi calls ex, ex resets the terminal and
 * starts running some filter, and SIGTSTP stops them both, vi has to know
 * when it restarts that it can't repaint the screen until ex's child has
 * finished running.  This is solveable, but it's annoying.
 *
 * Well, somebody had to make a decision, and this is the way it's going to be
 * (unless I get talked out of it).  SIGINT is handled asynchronously, so
 * that we can pretty much guarantee that the user can interrupt any operation
 * at any time.  SIGTSTP is handled synchronously, so that we don't have to
 * reenter curses and so that we don't have to play the process group games.
 * ^Z is recognized in the standard text input and command modes.  (^Z should
 * also be recognized during operations that may potentially take a long time.
 * The simplest solution is probably to twiddle the tty, install a handler for
 * SIGTSTP, and then restore normal tty modes when the operation is complete.)
 */

/*
 * sig_init --
 *	Initialize signals.
 */
int
sig_init(sp)
	SCR *sp;
{
	GS *gp;
	struct sigaction act;

	/* Initialize the signals. */
	gp = sp->gp;
	(void)sigemptyset(&gp->blockset);

	/*
	 * Use sigaction(2), not signal(3), since we don't always want to
	 * restart system calls.  The example is when waiting for a command
	 * mode keystroke and SIGWINCH arrives.  Try to set the restart bit
	 * (SA_RESTART) on SIGALRM anyway, it should result in a lot fewer
	 * interruptions.  We also block every other signal that we can block
	 * when a signal arrives.  This is because the signal functions call
	 * other nvi functions, which aren't guaranteed to be reentrant.
	 */

#ifndef	SA_RESTART
#define	SA_RESTART	0
#endif
#define	SETSIG(signal, flags, handler) {				\
	if (sigaddset(&gp->blockset, signal))				\
		goto err;						\
	act.sa_handler = handler;					\
	sigfillset(&act.sa_mask);					\
	act.sa_flags = flags;						\
	if (sigaction(signal, &act, NULL))				\
		goto err;						\
}
	SETSIG(SIGALRM, SA_RESTART, h_alrm);
	SETSIG(SIGHUP, 0, h_hup);
	SETSIG(SIGINT, 0, h_int);
	SETSIG(SIGTERM, 0, h_term);
	SETSIG(SIGWINCH, 0, h_winch);
	return (0);

err:	msgq(sp, M_SYSERR, "signal init");
	return (1);
}

/*
 * sig_end --
 *	End signal setup.
 */
void
sig_end()
{
	/*
	 * POSIX 1003.1-1990 requires that fork (and, presumably, vfork) clear
	 * pending alarms, and that the exec functions clear pending signals.
	 * In addition, after an exec, the child continues to ignore signals
	 * ignored in the parent, and the child's action for signals caught in
	 * the parent is set to the default action.  So, as we currently don't
	 * ignore any signals, there's no cleanup to be done.  This routine is
	 * left here as a stub function.
	 */
	 return;
}

/*
 * busy_on --
 *	Set a busy message timer.
 */
int
busy_on(sp, msg)
	SCR *sp;
	char const *msg;
{
	struct itimerval value;
	struct timeval tod;

	/*
	 * Give the oldest busy message precedence, since it's
	 * the longer running operation.
	 */
	if (sp->busy_msg != NULL)
		return (1);

	/* Get the current time of day, and create a target time. */
	if (gettimeofday(&tod, NULL))
		return (1);
#define	USER_PATIENCE_USECS	(8 * 100000L)
	sp->busy_tod.tv_sec = tod.tv_sec;
	sp->busy_tod.tv_usec = tod.tv_usec + USER_PATIENCE_USECS;

	/* We depend on this being an atomic instruction. */
	sp->busy_msg = msg;

	/*
	 * Busy messages turn around fast.  Reset the timer regardless
	 * of its current state.
	 */
	value.it_value.tv_sec = 0;
	value.it_value.tv_usec = USER_PATIENCE_USECS;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &value, NULL))
		msgq(sp, M_SYSERR, "timer: setitimer");
	return (0);
}

/*
 * busy_off --
 *	Turn off a busy message timer.
 */
void
busy_off(sp)
	SCR *sp;
{
	/* We depend on this being an atomic instruction. */
	sp->busy_msg = NULL;
}

/*
 * rcv_on --
 *	Turn on recovery timer.
 */
int
rcv_on(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct itimerval value;
	struct timeval tod;

	/* Get the current time of day. */
	if (gettimeofday(&tod, NULL))
		return (1);

	/* Create target time of day. */
	ep->rcv_tod.tv_sec = tod.tv_sec + RCV_PERIOD;
	ep->rcv_tod.tv_usec = 0;

	/*
	 * If there's a busy message happening, we're done, the
	 * interrupt handler will start our timer as necessary.
	 */
	if (sp->busy_msg != NULL)
		return (0);

	value.it_value.tv_sec = RCV_PERIOD;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &value, NULL)) {
		msgq(sp, M_SYSERR, "timer: setitimer");
		return (1);
	}
	return (0);
}

/*
 * h_alrm --
 *	Handle SIGALRM.
 *
 * There are two uses of the ITIMER_REAL timer (SIGALRM) in nvi.  The first
 * is to push the recovery information out to disk at periodic intervals.
 * The second is to display a "busy" message if an operation takes more time
 * that users are willing to wait before seeing something happen.  The SCR
 * structure has a wall clock timer structure for each of these.  Since the
 * busy timer has a much faster timeout than the recovery timer, most of the
 * code ignores the recovery timer unless it's the only thing running.
 *
 * XXX
 * It would be nice to reimplement this with two timers, a la POSIX 1003.1,
 * but not many systems offer them yet.
 */
static void
h_alrm(signo)
	int signo;
{
	struct itimerval value;
	struct timeval ntod, tod;
	SCR *sp;
	EXF *ep;
	int sverrno;

	sverrno = errno;

	/* XXX: Get the current time of day; if this fails, we're dead. */
	if (gettimeofday(&tod, NULL))
		goto ret;

	/*
	 * Fire any timers that are past due, or any that are due
	 * in a tenth of a second or less.
	 */
	for (ntod.tv_sec = 0, sp = __global_list->dq.cqh_first;
	    sp != (void *)&__global_list->dq; sp = sp->q.cqe_next) {

		/* Check the busy timer if the msg pointer is set. */
		if (sp->busy_msg == NULL)
			goto skip_busy;
		if (sp->busy_tod.tv_sec > tod.tv_sec ||
		    sp->busy_tod.tv_sec == tod.tv_sec &&
		    sp->busy_tod.tv_usec > tod.tv_usec &&
		    sp->busy_tod.tv_usec - tod.tv_usec > 100000L) {
			if (ntod.tv_sec == 0 ||
			    ntod.tv_sec > sp->busy_tod.tv_sec ||
			    ntod.tv_sec == sp->busy_tod.tv_sec &&
			    ntod.tv_usec > sp->busy_tod.tv_usec)
				ntod = sp->busy_tod;
		} else {
			(void)sp->s_busy(sp, sp->busy_msg);
			sp->busy_msg = NULL;
		}

		/*
		 * Sync the file if the recovery timer has fired.  If
		 * the sync fails, we don't reschedule future sync's.
		 */
skip_busy:	ep = sp->ep;
		if (ep->rcv_tod.tv_sec < tod.tv_sec ||
		    ep->rcv_tod.tv_sec == tod.tv_sec &&
		    ep->rcv_tod.tv_usec < tod.tv_usec + 100000L) {
			if (rcv_sync(sp, ep, 0))
				continue;
			ep->rcv_tod = tod;
			ep->rcv_tod.tv_sec += RCV_PERIOD;
		}
		if (ntod.tv_sec == 0 ||
		    ntod.tv_sec > ep->rcv_tod.tv_sec ||
		    ntod.tv_sec == ep->rcv_tod.tv_sec &&
		    ntod.tv_usec > ep->rcv_tod.tv_usec)
			ntod = ep->rcv_tod;
	}

	if (ntod.tv_sec == 0)
		goto ret;

	/* XXX: Set the timer; if this fails, we're dead. */
	value.it_value.tv_sec = ntod.tv_sec - tod.tv_sec;
	value.it_value.tv_usec = ntod.tv_usec - tod.tv_usec;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	(void)setitimer(ITIMER_REAL, &value, NULL);

ret:	errno = sverrno;
}

/*
 * h_hup --
 *	Handle SIGHUP.
 */
static void
h_hup(signo)
	int signo;
{
	sig_sync(SIGHUP, RCV_EMAIL);
	/* NOTREACHED */
}

/*
 * h_int --
 *	Handle SIGINT.
 *
 * XXX
 * This isn't right if windows are independent of each other.
 */
static void
h_int(signo)
	int signo;
{
	F_SET(__global_list, G_SIGINT);
}

/*
 * h_term --
 *	Handle SIGTERM.
 */
static void
h_term(signo)
	int signo;
{
	sig_sync(SIGTERM, 0);
	/* NOTREACHED */
}

/*
 * h_winch --
 *	Handle SIGWINCH.
 *
 * XXX
 * This isn't right if windows are independent of each other.
 */
static void
h_winch(signo)
	int signo;
{
	F_SET(__global_list, G_SIGWINCH);
}


/*
 * sig_sync --
 *
 *	Sync the files based on a signal.
 */
static void
sig_sync(signo, flags)
	int signo;
	u_int flags;
{
	SCR *sp;

	/*
	 * Walk the lists of screens, sync'ing the files; only sync
	 * each file once.
	 */
	for (sp = __global_list->dq.cqh_first;
	    sp != (void *)&__global_list->dq; sp = sp->q.cqe_next)
		rcv_sync(sp, sp->ep, RCV_ENDSESSION | RCV_PRESERVE | flags);
	for (sp = __global_list->hq.cqh_first;
	    sp != (void *)&__global_list->hq; sp = sp->q.cqe_next)
		rcv_sync(sp, sp->ep, RCV_ENDSESSION | RCV_PRESERVE | flags);

	/*
	 * Die with the proper exit status.  Don't bother using
	 * sigaction(2) 'cause we want the default behavior.
	 */
	(void)signal(signo, SIG_DFL);
	(void)kill(getpid(), signo);
	/* NOTREACHED */

	exit (1);
}
