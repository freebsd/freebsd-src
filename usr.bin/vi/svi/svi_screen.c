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
static char sccsid[] = "@(#)svi_screen.c	8.91 (Berkeley) 8/14/94";
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
#include "../vi/vcmd.h"
#include "svi_screen.h"
#include "../sex/sex_screen.h"

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
	sp->s_fmap		= svi_fmap;
	sp->s_position		= svi_sm_position;
	sp->s_rabs		= svi_rabs;
	sp->s_rcm		= svi_rcm;
	sp->s_refresh		= svi_refresh;
	sp->s_scroll		= svi_sm_scroll;
	sp->s_split		= svi_split;
	sp->s_suspend		= svi_suspend;
	sp->s_window		= sex_window;

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

		F_SET(nsvi, F_ISSET(osvi, SVI_CURSES_INIT));
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
	SVI_PRIVATE *svp;

	svp = SVP(sp);

	/* Free the screen map. */
	if (HMAP != NULL)
		FREE(HMAP, SIZE_HMAP(sp) * sizeof(SMAP));

	/* Free the visual bell string. */
	if (svp->VB != NULL)
		free(svp->VB);

	/* Free private memory. */
	FREE(svp, sizeof(SVI_PRIVATE));
	sp->svi_private = NULL;

	return (0);
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
	int ecurses, escreen, force, rval;

	escreen = ecurses = rval = 0;

	/* Initialize curses. */
	if (svi_curses_init(sp)) {
		escreen = 1;
		goto err;
	}
	ecurses = 1;

	/*
	 * The resize bit is probably set, as a result of the terminal being
	 * set.  We clear it as we just finished initializing the screen.
	 * However, we will want to fill in the map from scratch, so provide
	 * a line number just in case, and set the reformat flag.
	 */
	HMAP->lno = 1;
	F_CLR(sp, S_RESIZE);
	F_SET(sp, S_REFORMAT);

	/*
	 * The historic 4BSD curses had an uneasy relationship with termcap.
	 * Termcap used a static buffer to hold the terminal information,
	 * which was was then used by the curses functions.  We want to use
	 * it too, for lots of random things, but we've put it off until after
	 * svi_curses_init:initscr() was called.  Do it now.
	 */
	if (svi_term_init(sp))
		goto err;

	for (;;) {
		/* Reset the cursor. */
		F_SET(SVP(sp), SVI_CUR_INVALID);

		/*
		 * Run vi.  If vi fails, svi data structures may be
		 * corrupted, be extremely careful what you free up.
		 */
		if (vi(sp, sp->ep)) {
			(void)rcv_sync(sp, sp->ep,
			    RCV_EMAIL | RCV_ENDSESSION | RCV_PRESERVE);
			escreen = 1;
			goto err;
		}

		force = 0;
		switch (F_ISSET(sp, S_MAJOR_CHANGE)) {
		case S_EXIT_FORCE:
			force = 1;
			/* FALLTHROUGH */
		case S_EXIT:
			F_CLR(sp, S_EXIT_FORCE | S_EXIT);
			if (file_end(sp, sp->ep, force))/* File end. */
				break;
			/*
			 * !!!
			 * NB: sp->frp may now be NULL, if it was a tmp file.
			 */
			(void)svi_join(sp, &tsp);	/* Find a new screen. */
			if (tsp == NULL)
				(void)svi_swap(sp, &tsp, NULL);
			if (tsp == NULL) {
				escreen = 1;
				goto ret;
			}
			(void)screen_end(sp);		/* Screen end. */
			sp = tsp;
			break;
		case 0:					/* Exit vi mode. */
			svi_dtoh(sp, "Exit from vi");
			goto ret;
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

	if (0) {
err:		rval = 1;
	}

ret:	if (svi_term_end(sp))			/* Terminal end (uses sp). */
		rval = 1;
	if (ecurses && svi_curses_end(sp))	/* Curses end (uses sp). */
		rval = 1;
	if (escreen && screen_end(sp))		/* Screen end. */
		rval = 1;
	return (rval);
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
 * svi_dtoh --
 *	Move all but the current screen to the hidden queue.
 */
void
svi_dtoh(sp, emsg)
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
	    "%s backgrounded %d screens; use :display to list the screens",
		    emsg, hidden - 1);
}
