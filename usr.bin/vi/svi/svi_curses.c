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
static char sccsid[] = "@(#)svi_curses.c	8.3 (Berkeley) 8/7/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
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
#include "svi_screen.h"

/*
 * svi_curses_init --
 *	Initialize curses.
 */
int
svi_curses_init(sp)
	SCR *sp;
{
	struct termios t;
	char *p;

#ifdef SYSV_CURSES
	/*
	 * The SunOS/System V initscr() isn't reentrant.  Don't even think
	 * about trying to use it.  It fails in subtle ways (e.g. select(2)
	 * on fileno(stdin) stops working).  We don't care about the SCREEN
	 * reference returned by newterm, we never have more than one SCREEN
	 * at a time.
	 */
	errno = 0;
	if (newterm(O_STR(sp, O_TERM), stdout, stdin) == NULL) {
		msgq(sp, errno ? M_SYSERR : M_ERR, "newterm failed");
		return (1);
	}
#else
	/*
	 * Initscr() doesn't provide useful error values or messages.  The
	 * reasonable guess is that either malloc failed or the terminal was
	 * unknown or lacking some essential feature.  Try and guess so the
	 * user isn't even more pissed off because of the error message.
	 */
	errno = 0;
	if (initscr() == NULL) {
		char kbuf[2048];
		msgq(sp, errno ? M_SYSERR : M_ERR, "initscr failed");
		if ((p = getenv("TERM")) == NULL || !strcmp(p, "unknown"))
			msgq(sp, M_ERR,
	"No TERM environment variable set, or TERM set to \"unknown\"");
		else if (tgetent(kbuf, p) != 1)
			msgq(sp, M_ERR,
"%s: unknown terminal type, or terminal lacks necessary features", p);
		else
			msgq(sp, M_ERR,
		    "%s: terminal type lacks necessary features", p);
		return (1);
	}
#endif
	/*
	 * We use raw mode.  What we want is 8-bit clean, however, signals
	 * and flow control should continue to work.  Admittedly, it sounds
	 * like cbreak, but it isn't.  Using cbreak() can get you additional
	 * things like IEXTEN, which turns on things like DISCARD and LNEXT.
	 *
	 * !!!
	 * If raw isn't turning off echo and newlines, something's wrong.
	 * However, it doesn't hurt.
	 */
	noecho();			/* No character echo. */
	nonl();				/* No CR/NL translation. */
	raw();				/* 8-bit clean. */
	idlok(stdscr, 1);		/* Use hardware insert/delete line. */

	/*
	 * XXX
	 * Historic implementations of curses handled SIGTSTP signals
	 * in one of three ways.  They either:
	 *
	 *	1: Set their own handler, regardless.
	 *	2: Did not set a handler if a handler was already installed.
	 *	3: Set their own handler, but then called any previously set
	 *	   handler after completing their own cleanup.
	 *
	 * We don't try and figure out which behavior is in place, we
	 * just set it to SIG_DFL after initializing the curses interface.
	 */
	(void)signal(SIGTSTP, SIG_DFL);

	/*
	 * If flow control was on, turn it back on.  Turn signals on.  ISIG
	 * turns on VINTR, VQUIT, VDSUSP and VSUSP.  See signal.c:sig_init()
	 * for a discussion of what's going on here.  To sum up, sig_init()
	 * already installed a handler for VINTR.  We're going to disable the
	 * other three.
	 *
	 * XXX
	 * We want to use ^Y as a vi scrolling command.  If the user has the
	 * DSUSP character set to ^Y (common practice) clean it up.  As it's
	 * equally possible that the user has VDSUSP set to 'a', we disable
	 * it regardless.  It doesn't make much sense to suspend vi at read,
	 * so I don't think anyone will care.  Alternatively, we could look
	 * it up in the table of legal command characters and turn it off if
	 * it matches one.  VDSUSP wasn't in POSIX 1003.1-1990, so we test for
	 * it.
	 *
	 * XXX
	 * We don't check to see if the user had signals enabled to start with.
	 * If they didn't, it's unclear what we're supposed to do here, but it
	 * is also pretty unlikely.
	 */
	if (!tcgetattr(STDIN_FILENO, &t)) {
		if (sp->gp->original_termios.c_iflag & IXON)
			t.c_iflag |= IXON;
		if (sp->gp->original_termios.c_iflag & IXOFF)
			t.c_iflag |= IXOFF;

		t.c_lflag |= ISIG;
#ifdef VDSUSP
		t.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
		t.c_cc[VQUIT] = _POSIX_VDISABLE;
		t.c_cc[VSUSP] = _POSIX_VDISABLE;

		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t);
	}

	/* Put the cursor keys into application mode. */
	svi_keypad(sp, 1);

	/*
	 * The first screen in the list gets it all.  All other screens
	 * are hidden and lose their maps.
	 */
	svi_dtoh(sp, "Window resize");

	/* Initialize terminal values. */
	SVP(sp)->srows = O_VAL(sp, O_LINES);

	/*
	 * Initialize screen values.
	 *
	 * Small windows: see svi/svi_refresh.c:svi_refresh, section 3b.
	 *
	 * Setup:
	 *	t_minrows is the minimum rows to display
	 *	t_maxrows is the maximum rows to display (rows - 1)
	 *	t_rows is the rows currently being displayed
	 */
	sp->rows = SVP(sp)->srows;
	sp->cols = O_VAL(sp, O_COLUMNS);
	sp->woff = 0;
	sp->t_rows = sp->t_minrows = O_VAL(sp, O_WINDOW);
	if (sp->t_rows > sp->rows - 1) {
		sp->t_minrows = sp->t_rows = sp->rows - 1;
		msgq(sp, M_INFO,
		    "Windows option value is too large, max is %u", sp->t_rows);
	}
	sp->t_maxrows = sp->rows - 1;

	/* Create the screen map. */
	CALLOC(sp, HMAP, SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	if (HMAP == NULL) {
		if (endwin() == ERR)
			msgq(sp, M_SYSERR, "endwin");
		return (1);
	}
	TMAP = HMAP + (sp->t_rows - 1);

	F_SET(SVP(sp), SVI_CUR_INVALID);	/* Cursor is invalid. */
	F_SET(SVP(sp), SVI_CURSES_INIT);	/* It's initialized. */

	return (0);
}

/*
 * svi_curses_end --
 *	Move to the bottom of the screen, end curses.
 */
int
svi_curses_end(sp)
	SCR *sp;
{
	/*
	 * XXX
	 * By the time we get here, the screen private area (SVI_PRIVATE)
	 * is probably gone.  Don't use it, and don't call any routines
	 * that do.
	 *
	 * Restore the cursor keys to normal mode.
	 */
	svi_keypad(sp, 0);

	/* Move to the bottom of the screen. */
	if (move(INFOLINE(sp), 0) == OK) {
		clrtoeol();
		refresh();
	}

	/* End curses window. */
	if (endwin() == ERR)
		msgq(sp, M_SYSERR, "endwin");

	return (0);
}
