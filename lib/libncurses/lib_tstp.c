
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_tstp.c
**
**	The routine tstp().
**
*/

#include "curses.priv.h"
#include "terminfo.h"
#ifdef SVR4_ACTION
#define _POSIX_SOURCE
#endif
#include <signal.h>

void tstp(int dummy)
{
sigaction_t act, oact;
sigset_t mask, omask;

	T(("tstp() called"));

	/*
	 * The user may have changed the prog_mode tty bits, so save them.
	 */
	def_prog_mode();

	/*
	 * Block window change and timer signals.  The latter
	 * is because applications use timers to decide when
	 * to repaint the screen.
	 */
	(void)sigemptyset(&mask);
	(void)sigaddset(&mask, SIGALRM);
#ifdef SIGWINCH
	(void)sigaddset(&mask, SIGWINCH);
#endif
	(void)sigprocmask(SIG_BLOCK, &mask, &omask);

	endwin();

	sigemptyset(&mask);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef  SA_RESTART
	act.sa_flags |= SA_RESTART;
#endif
	sigaction(SIGTSTP, &act, &oact);
	kill(getpid(), SIGTSTP);

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

