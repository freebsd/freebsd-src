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
static const char sccsid[] = "@(#)sex_window.c	8.7 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"

/*
 * sex_window --
 *	Set the window size.
 */
int
sex_window(sp, sigwinch)
	SCR *sp;
	int sigwinch;
{
	struct winsize win;
	size_t col, row;
	int rval, user_set;
	ARGS *argv[2], a, b;
	char *s, buf[2048];

	/*
	 * Get the screen rows and columns.  If the values are wrong, it's
	 * not a big deal -- as soon as the user sets them explicitly the
	 * environment will be set and the screen package will use the new
	 * values.
	 *
	 * Try TIOCGWINSZ.
	 */
	row = col = 0;
#ifdef TIOCGWINSZ
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) != -1) {
		row = win.ws_row;
		col = win.ws_col;
	}
#endif
	/* If here because of a signal, TIOCGWINSZ is all we trust. */
	if (sigwinch) {
		if (row == 0 || col == 0) {
			msgq(sp, M_SYSERR, "TIOCGWINSZ");
			return (1);
		}

		/*
		 * !!!
		 * SunOS systems deliver SIGWINCH when windows are uncovered
		 * as well as when they change size.  In addition, we call
		 * here when continuing after being suspended since the window
		 * may have changed size.  Since we don't want to background
		 * all of the screens just because the window was uncovered,
		 * ignore the signal if there's no change.
		 */
		if (row == O_VAL(sp, O_LINES) && col == O_VAL(sp, O_COLUMNS))
			return (1);

		goto sigw;
	}

	/*
	 * !!!
	 * If TIOCGWINSZ failed, or had entries of 0, try termcap.  This
	 * routine is called before any termcap or terminal information
	 * has been set up.  If there's no TERM environmental variable set,
	 * let it go, at least ex can run.
	 */
	if (row == 0 || col == 0) {
		if ((s = getenv("TERM")) == NULL)
			goto noterm;
#ifdef SYSV_CURSES
		if (row == 0)
			if ((rval = tigetnum("lines")) < 0)
				msgq(sp, M_SYSERR, "tigetnum: lines");
			else
				row = rval;
		if (col == 0)
			if ((rval = tigetnum("cols")) < 0)
				msgq(sp, M_SYSERR, "tigetnum: cols");
			else
				col = rval;
#else
		switch (tgetent(buf, s)) {
		case -1:
			msgq(sp, M_SYSERR, "tgetent: %s", s);
			return (1);
		case 0:
			msgq(sp, M_ERR, "%s: unknown terminal type", s);
			return (1);
		}
		if (row == 0)
			if ((rval = tgetnum("li")) < 0)
				msgq(sp, M_ERR,
				    "no \"li\" capability for %s", s);
			else
				row = rval;
		if (col == 0)
			if ((rval = tgetnum("co")) < 0)
				msgq(sp, M_ERR,
				    "no \"co\" capability for %s", s);
			else
				col = rval;
#endif
	}

	/* If nothing else, well, it's probably a VT100. */
noterm:	if (row == 0)
		row = 24;
	if (col == 0)
		col = 80;

	/* POSIX 1003.2 requires the environment to override. */
	if ((s = getenv("LINES")) != NULL)
		row = strtol(s, NULL, 10);
	if ((s = getenv("COLUMNS")) != NULL)
		col = strtol(s, NULL, 10);

sigw:	a.bp = buf;
	b.bp = NULL;
	b.len = 0;
	argv[0] = &a;
	argv[1] = &b;;

	/*
	 * Tell the options code that the screen size has changed.
	 * Since the user didn't do the set, clear the set bits.
	 */
	user_set = F_ISSET(&sp->opts[O_LINES], OPT_SET);
	a.len = snprintf(buf, sizeof(buf), "lines=%u", row);
	if (opts_set(sp, NULL, argv))
		return (1);
	if (user_set)
		F_CLR(&sp->opts[O_LINES], OPT_SET);

	user_set = F_ISSET(&sp->opts[O_COLUMNS], OPT_SET);
	a.len = snprintf(buf, sizeof(buf), "columns=%u", col);
	if (opts_set(sp, NULL, argv))
		return (1);
	if (user_set)
		F_CLR(&sp->opts[O_COLUMNS], OPT_SET);

	F_SET(sp, S_RESIZE);
	return (0);
}
