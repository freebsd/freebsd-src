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
static char sccsid[] = "@(#)svi_screen.c	8.69 (Berkeley) 3/16/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"
#include "excmd.h"
#include "svi_screen.h"
#include "sex/sex_screen.h"

static void	d_to_h __P((SCR *, char *));
static int	svi_initscr_kluge __P((SCR *, struct termios *));

/*
 * svi_screen_init --
 *	Initialize a screen.
 */
int
svi_screen_init(sp)
	SCR *sp;
{
	/* Initialize support routines. */
	sp->s_bell		= svi_bell;
	sp->s_bg		= svi_bg;
	sp->s_busy		= svi_busy;
	sp->s_change		= svi_change;
	sp->s_clear		= svi_clear;
	sp->s_colpos		= svi_cm_public;
	sp->s_column		= svi_column;
	sp->s_confirm		= svi_confirm;
	sp->s_crel		= svi_crel;
	sp->s_down		= svi_sm_down;
	sp->s_edit		= svi_screen_edit;
	sp->s_end		= svi_screen_end;
	sp->s_ex_cmd		= svi_ex_cmd;
	sp->s_ex_run		= svi_ex_run;
	sp->s_ex_write		= svi_ex_write;
	sp->s_fg		= svi_fg;
	sp->s_fill		= svi_sm_fill;
	sp->s_get		= svi_get;
	sp->s_key_read		= sex_key_read;
	sp->s_optchange		= svi_optchange;
	sp->s_position		= svi_sm_position;
	sp->s_rabs		= svi_rabs;
	sp->s_rcm		= svi_rcm;
	sp->s_refresh		= svi_refresh;
	sp->s_split		= svi_split;
	sp->s_suspend		= svi_suspend;
	sp->s_up		= svi_sm_up;

	return (0);
}

/*
 * svi_screen_copy --
 *	Copy to a new screen.
 */
int
svi_screen_copy(orig, sp)
	SCR *orig, *sp;
{
	SVI_PRIVATE *osvi, *nsvi;

	/* Create the private screen structure. */
	CALLOC_RET(orig, nsvi, SVI_PRIVATE *, 1, sizeof(SVI_PRIVATE));
	sp->svi_private = nsvi;

/* INITIALIZED AT SCREEN CREATE. */
	/* Invalidate the line size cache. */
	SVI_SCR_CFLUSH(nsvi);

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig == NULL) {
	} else {
		osvi = SVP(orig);
		nsvi->srows = osvi->srows;
		if (osvi->VB != NULL && (nsvi->VB = strdup(osvi->VB)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}

		F_SET(nsvi, F_ISSET(osvi, SVI_NO_VBELL));
	}
	return (0);
}

/*
 * svi_screen_end --
 *	End a screen.
 */
int
svi_screen_end(sp)
	SCR *sp;
{
	int rval;

	rval = 0;

	/*
	 * XXX
	 * If this is the last screen, end curses
	 * while we still have screen information.
	 */
	if (sp->q.cqe_prev == (void *)&sp->gp->dq &&
	    sp->q.cqe_next == (void *)&sp->gp->dq &&
	    sp->gp->hq.cqh_first == (void *)&sp->gp->hq &&
	    svi_curses_end(sp))
		rval = 1;

	/* Free screen map. */
	if (HMAP != NULL)
		FREE(HMAP, SIZE_HMAP(sp) * sizeof(SMAP));

	/* Free visual bell string. */
	if (SVP(sp)->VB != NULL)
		FREE(SVP(sp)->VB, strlen(SVP(sp)->VB) + 1);

	/* Free private memory. */
	FREE(SVP(sp), sizeof(SVI_PRIVATE));
	sp->svi_private = NULL;

	return (rval);
}

/*
 * We use a single curses "window" for each vi screen.  The model would be
 * simpler with two windows (one for the text, and one for the modeline)
 * because scrolling the text window down would work correctly then, not
 * affecting the mode line.  As it is we have to play games to make it look
 * right.  The reason for this choice is that it would be difficult for
 * curses to optimize the movement, i.e. detect that the downward scroll
 * isn't going to change the modeline, set the scrolling region on the
 * terminal and only scroll the first part of the text window.  (Even if
 * curses did detect it, the set-scrolling-region terminal commands can't
 * be used by curses because it's indeterminate where the cursor ends up
 * after they are sent.)
 */
/*
 * svi_screen_edit --
 *	Main vi curses screen loop.
 */
int
svi_screen_edit(sp, ep)
	SCR *sp;
	EXF *ep;
{
	SCR *tsp;
	int force;

	/* Get refresh to init curses. */
	F_SET(sp, S_RESIZE);

	for (;;) {
		/* Reset the cursor. */
		F_SET(SVP(sp), SVI_CUR_INVALID);

		/*
		 * Run vi.  If vi fails, svi data structures may be
		 * corrupted, be extremely careful what you free up.
		 */
		if (vi(sp, sp->ep)) {
			if (F_ISSET(ep, F_RCV_ON)) {
				F_SET(ep, F_RCV_NORM);
				(void)rcv_sync(sp, sp->ep);
			}
			(void)file_end(sp, sp->ep, 1);	/* End the file. */
			(void)svi_curses_end(sp);	/* End curses. */
			(void)screen_end(sp);		/* End the screen. */
			return (1);
		}

		force = 0;
		switch (F_ISSET(sp, S_MAJOR_CHANGE)) {
		case S_EXIT_FORCE:
			force = 1;
			/* FALLTHROUGH */
		case S_EXIT:
			F_CLR(sp, S_EXIT_FORCE | S_EXIT);
			if (file_end(sp, sp->ep, force))/* End the file. */
				break;
			(void)svi_join(sp, &tsp);	/* Find a new screen. */
			if (tsp == NULL)
				(void)svi_swap(sp, &tsp, NULL);
			(void)screen_end(sp);		/* End the screen. */
			if ((sp = tsp) == NULL)		/* Switch. */
				return (0);
			break;
		case 0:					/* Exit vi mode. */
			(void)svi_curses_end(sp);	/* End curses. */
			d_to_h(sp, "Exit from vi");
			return (0);
		case S_FSWITCH:				/* File switch. */
			F_CLR(sp, S_FSWITCH);
			F_SET(sp, S_REFORMAT);
			break;
		case S_SSWITCH:				/* Screen switch. */
			F_CLR(sp, S_SSWITCH);
			sp = sp->nextdisp;
			break;
		default:
			abort();
		}
	}
	/* NOTREACHED */
}

/*
 * svi_curses_init --
 *	Initialize curses.
 */
int
svi_curses_init(sp)
	SCR *sp;
{
	struct termios t;
	int ixoff, ixon;
	char *p, kbuf[2048];

	/*
	 * Vi wants raw mode, excepting flow control characters.  Both
	 * cbreak and raw modes have problems for us.  In cbreak mode,
	 * we have to turn all the signals off explicitly.  In raw mode,
	 * we have to turn flow control back on.  Raw chosen for no strong
	 * reason.  In both cases we have to periodically turn various
	 * signals on.
	 */
	if (tcgetattr(STDIN_FILENO, &t)) {
		msgq(sp, M_SYSERR, "tcgetattr");
		return (1);
	}
	ixon = t.c_iflag & IXON;
	ixoff = t.c_iflag & IXOFF;

	/*
	 * The initscr() in SunOS curses flushes the terminal driver queue.
	 * I have no idea if this stark raving insanity appears elsewhere,
	 * but since the SunOS curses is likely derived from the System III
	 * or System V versions, here's the workaround.
	 */
	if (svi_initscr_kluge(sp, &t))
		return (1);

	/*
	 * Start the screen.  Initscr() doesn't provide useful error values
	 * or messages.  Generally, either malloc failed or the terminal
	 * was unknown or lacked some necesssary feature.  Try and guess so
	 * the user isn't even more pissed off because of the error message.
	 */
	errno = 0;
	if (initscr() == NULL) {
		if (errno)
			msgq(sp, M_SYSERR, "Initscr failed");
		else
			msgq(sp, M_ERR, "Initscr failed.");
		if ((p = getenv("TERM")) == NULL || !strcmp(p, "unknown"))
			msgq(sp, M_ERR,
	"No TERM environment variable set, or TERM set to \"unknown\".");
		else if (tgetent(kbuf, p) != 1)
			msgq(sp, M_ERR,
"%s: unknown terminal type, or terminal lacks necessary features.", p);
		else
			msgq(sp, M_ERR,
		    "%s: terminal type lacks necessary features.", p);
		return (1);
	}

	/*
	 * !!!
	 * If raw isn't turning off echo and newlines, something's wrong.
	 * However, just in case...
	 */
	noecho();			/* No character echo. */
	nonl();				/* No CR/NL translation. */
	raw();				/* No special characters. */
	idlok(stdscr, 1);		/* Use hardware insert/delete line. */

	/*
	 * Put the cursor keys into application mode.  Historic versions
	 * of curses had no way to do this, and the newer versions (SunOS,
	 * System V) only enable it through the wgetch() interface.
	 */
	svi_keypad(sp, 1);

	/*
	 * XXX
	 * Major compatibility kluge.  When we call the curses raw() routine,
	 * XON/XOFF flow control is turned off.  Old terminals like to have
	 * it, so if it's originally set for the tty, we turn it back on.  For
	 * some unknown reason, this causes System V curses to NOT correctly
	 * restore the terminal modes when SIGTSTP is received.
	 */
	if ((ixon || ixoff) &&
	    !tcgetattr(STDIN_FILENO, &sp->gp->s5_curses_botch)) {
		t = sp->gp->s5_curses_botch;
		if (ixon)
			t.c_iflag |= IXON;
		if (ixoff)
			t.c_iflag |= IXOFF;
		F_SET(sp->gp, G_CURSES_S5CB);
		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t);
	}

	/*
	 * The first screen in the list gets it all.  All other screens
	 * are hidden and lose their maps.
	 */
	d_to_h(sp, "Window resize");

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
	CALLOC_RET(sp, HMAP, SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	TMAP = HMAP + (sp->t_rows - 1);

	F_SET(sp->gp, G_CURSES_INIT);		/* Curses initialized. */
	F_SET(SVP(sp), SVI_CUR_INVALID);	/* Cursor is invalid. */
	return (0);
}

/*
 * svi_crel --
 *	Change the relative size of the current screen.
 */
int
svi_crel(sp, count)
	SCR *sp;
	long count;
{
	/* Can't grow beyond the size of the window. */
	if (count > O_VAL(sp, O_WINDOW))
		count = O_VAL(sp, O_WINDOW);

	sp->t_minrows = sp->t_rows = count;
	if (sp->t_rows > sp->rows - 1)
		sp->t_minrows = sp->t_rows = sp->rows - 1;
	TMAP = HMAP + (sp->t_rows - 1);
	F_SET(sp, S_REDRAW);
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
	/* We get called before anything has been initialized. */
	if (!F_ISSET(sp->gp, G_CURSES_INIT))
		return (0);
	F_CLR(sp->gp, G_CURSES_INIT);

	/* Move to the bottom of the screen. */
	if (move(INFOLINE(sp), 0) == OK) {
		clrtoeol();
		refresh();
	}

	/*
	 * XXX
	 * See comment in svi_curses_init().
	 */
	if (F_ISSET(sp->gp, G_CURSES_S5CB))
		(void)tcsetattr(STDIN_FILENO,
		    TCSASOFT | TCSADRAIN, &sp->gp->s5_curses_botch);
	F_CLR(sp->gp, G_CURSES_S5CB);

	/* Restore the cursor keys to normal mode. */
	svi_keypad(sp, 0);
	endwin();
	return (0);
}

/*
 * svi_initscr_kluge --
 *	Read all of the waiting keys before calling initscr().
 */
static int
svi_initscr_kluge(sp, tp)
	SCR *sp;
	struct termios *tp;
{
	struct termios rawt;
	int rval;

	/*
	 * Turn off canonical input processing so that we get partial
	 * lines as well as complete ones.  Also, set the MIN/TIME
	 * values -- System V and SMI systems overload VMIN and VTIME,
	 * such that VMIN is the same as the VEOF element, and VTIME is
	 * the same as the VEOL element.  This means, that if VEOF was
	 * ^D, the default VMIN is 4.  Majorly stupid.
	 */
	rawt = *tp;
	rawt.c_cc[VMIN] = 1;
	rawt.c_cc[VTIME] = 0;
	rawt.c_lflag &= ~ICANON;
	if (tcsetattr(STDIN_FILENO, TCSASOFT | TCSANOW, &rawt))
		return (1);
	rval = term_key_queue(sp);
	if (tcsetattr(STDIN_FILENO, TCSASOFT | TCSANOW, tp) || rval)
		return (1);
	return (0);
}

/*
 * d_to_h --
 *	Move all but the current screen to the hidden queue.
 */
static void
d_to_h(sp, emsg)
	SCR *sp;
	char *emsg;
{
	SCR *tsp;
	int hidden;

	for (hidden = 0;
	    (tsp = sp->gp->dq.cqh_first) != (void *)&sp->gp->dq; ++hidden) {
		if (_HMAP(tsp) != NULL) {
			FREE(_HMAP(tsp), SIZE_HMAP(tsp) * sizeof(SMAP));
			_HMAP(tsp) = NULL;
		}
		CIRCLEQ_REMOVE(&sp->gp->dq, tsp, q);
		CIRCLEQ_INSERT_TAIL(&sp->gp->hq, tsp, q);
	}
	CIRCLEQ_REMOVE(&sp->gp->hq, sp, q);
	CIRCLEQ_INSERT_TAIL(&sp->gp->dq, sp, q);
	if (hidden > 1)
		msgq(sp, M_INFO,
	    "%s backgrounded %d screens; use :display to list the screens.",
		    emsg, hidden - 1);
}
