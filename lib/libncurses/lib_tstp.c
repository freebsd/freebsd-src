
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
sigset_t mask;

	T(("tstp() called"));

	endwin();

	sigemptyset(&mask);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGTSTP, &act, &oact);
	kill(getpid(), SIGTSTP);

	T(("SIGCONT received"));
	sigaction(SIGTSTP, &oact, NULL);
	reset_prog_mode();
	flushinp();
	if (enter_ca_mode)
		putp(enter_ca_mode);
	doupdate();
}

