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
static const char sccsid[] = "@(#)svi_smap.c	8.47 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "../vi/vcmd.h"
#include "svi_screen.h"

static int	svi_deleteln __P((SCR *, int));
static int	svi_insertln __P((SCR *, int));
static int	svi_sm_delete __P((SCR *, EXF *, recno_t));
static int	svi_sm_down __P((SCR *, EXF *,
		    MARK *, recno_t, enum sctype, SMAP *));
static int	svi_sm_erase __P((SCR *));
static int	svi_sm_insert __P((SCR *, EXF *, recno_t));
static int	svi_sm_reset __P((SCR *, EXF *, recno_t));
static int	svi_sm_up __P((SCR *, EXF *,
		    MARK *, recno_t, enum sctype, SMAP *));

/*
 * svi_change --
 *	Make a change to the screen.
 */
int
svi_change(sp, ep, lno, op)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	enum operation op;
{
	SMAP *p;
	size_t oldy, oldx;

	/* Appending is the same as inserting, if the line is incremented. */
	if (op == LINE_APPEND) {
		++lno;
		op = LINE_INSERT;
	}

	/* Ignore the change if the line is after the map. */
	if (lno > TMAP->lno)
		return (0);

	/*
	 * If the line is before the map, and it's a decrement, decrement
	 * the map.  If it's an increment, increment the map.  Otherwise,
	 * ignore it.
	 */
	if (lno < HMAP->lno) {
		switch (op) {
		case LINE_APPEND:
			abort();
			/* NOTREACHED */
		case LINE_DELETE:
			for (p = HMAP; p <= TMAP; ++p)
				--p->lno;
			if (sp->lno >= lno)
				--sp->lno;
			F_SET(sp, S_RENUMBER);
			break;
		case LINE_INSERT:
			for (p = HMAP; p <= TMAP; ++p)
				++p->lno;
			if (sp->lno >= lno)
				++sp->lno;
			F_SET(sp, S_RENUMBER);
			break;
		case LINE_RESET:
			break;
		}
		return (0);
	}

	F_SET(SVP(sp), SVI_SCREENDIRTY);

	/* Invalidate the cursor, if it's on this line. */
	if (sp->lno == lno)
		F_SET(SVP(sp), SVI_CUR_INVALID);

	/* Invalidate the line size cache. */
	SVI_SCR_CFLUSH(SVP(sp));

	getyx(stdscr, oldy, oldx);

	switch (op) {
	case LINE_DELETE:
		if (svi_sm_delete(sp, ep, lno))
			return (1);
		F_SET(sp, S_RENUMBER);
		break;
	case LINE_INSERT:
		if (svi_sm_insert(sp, ep, lno))
			return (1);
		F_SET(sp, S_RENUMBER);
		break;
	case LINE_RESET:
		if (svi_sm_reset(sp, ep, lno))
			return (1);
		break;
	default:
		abort();
	}

	MOVEA(sp, oldy, oldx);

	return (0);
}

/*
 * svi_sm_fill --
 *	Fill in the screen map, placing the specified line at the
 *	right position.  There isn't any way to tell if an SMAP
 *	entry has been filled in, so this routine had better be
 *	called with P_FILL set before anything else is done.
 *
 * !!!
 * Unexported interface: if lno is OOBLNO, P_TOP means that the HMAP
 * slot is already filled in, P_BOTTOM means that the TMAP slot is
 * already filled in, and we just finish up the job.
 */
int
svi_sm_fill(sp, ep, lno, pos)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	enum position pos;
{
	SMAP *p, tmp;

	/* Flush all cached information from the SMAP. */
	for (p = HMAP; p <= TMAP; ++p)
		SMAP_FLUSH(p);

	/* If the map is filled, the screen must be redrawn. */
	F_SET(sp, S_REDRAW);

	switch (pos) {
	case P_FILL:
		tmp.lno = 1;
		tmp.off = 1;

		/* See if less than half a screen from the top. */
		if (svi_sm_nlines(sp, ep,
		    &tmp, lno, HALFTEXT(sp)) <= HALFTEXT(sp)) {
			lno = 1;
			goto top;
		}

		/* See if less than half a screen from the bottom. */
		if (file_lline(sp, ep, &tmp.lno))
			return (1);
		if (!O_ISSET(sp, O_LEFTRIGHT))
			tmp.off = svi_opt_screens(sp, ep, tmp.lno, NULL);
		if (svi_sm_nlines(sp, ep,
		    &tmp, lno, HALFTEXT(sp)) <= HALFTEXT(sp)) {
			TMAP->lno = tmp.lno;
			if (!O_ISSET(sp, O_LEFTRIGHT))
				TMAP->off = tmp.off;
			goto bottom;
		}
		goto middle;
	case P_TOP:
		if (lno != OOBLNO) {
top:			HMAP->lno = lno;
			HMAP->off = 1;
		}
		/* If we fail, just punt. */
		for (p = HMAP; p < TMAP; ++p)
			if (svi_sm_next(sp, ep, p, p + 1))
				goto err;
		break;
	case P_MIDDLE:
		/* If we fail, guess that the file is too small. */
middle:		p = HMAP + (TMAP - HMAP) / 2;
		for (p->lno = lno, p->off = 1; p > HMAP; --p)
			if (svi_sm_prev(sp, ep, p, p - 1)) {
				lno = 1;
				goto top;
			}

		/* If we fail, just punt. */
		p = HMAP + (TMAP - HMAP) / 2;
		for (; p < TMAP; ++p)
			if (svi_sm_next(sp, ep, p, p + 1))
				goto err;
		break;
	case P_BOTTOM:
		if (lno != OOBLNO) {
			TMAP->lno = lno;
			if (!O_ISSET(sp, O_LEFTRIGHT))
				TMAP->off = svi_opt_screens(sp, ep, lno, NULL);
		}
		/* If we fail, guess that the file is too small. */
bottom:		for (p = TMAP; p > HMAP; --p)
			if (svi_sm_prev(sp, ep, p, p - 1)) {
				lno = 1;
				goto top;
			}
		break;
	default:
		abort();
	}
	return (0);

	/*
	 * Try and put *something* on the screen.  If this fails,
	 * we have a serious hard error.
	 */
err:	HMAP->lno = 1;
	HMAP->off = 1;
	for (p = HMAP; p < TMAP; ++p)
		if (svi_sm_next(sp, ep, p, p + 1))
			return (1);
	return (0);
}

/*
 * For the routines svi_sm_reset, svi_sm_delete and svi_sm_insert: if the
 * screen only contains one line, or, if the line is the entire screen, this
 * gets fairly exciting.  Skip the fun and simply return if there's only one
 * line in the screen, or just call fill.  Fill may not be entirely accurate,
 * i.e. we may be painting the screen with something not even close to the
 * cursor, but it's not like we're into serious performance issues here, and
 * the refresh routine will fix it for us.
 */
#define	TOO_WEIRD {							\
	if (cnt_orig >= sp->t_rows) {					\
		if (cnt_orig == 1)					\
			return (0);					\
		if (file_gline(sp, ep, lno, NULL) == NULL)		\
			if (file_lline(sp, ep, &lno))			\
				return (1);				\
		F_SET(sp, S_REDRAW);					\
		return (svi_sm_fill(sp, ep, lno, P_TOP));		\
	}								\
}

/*
 * svi_sm_delete --
 *	Delete a line out of the SMAP.
 */
static int
svi_sm_delete(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	SMAP *p, *t;
	size_t cnt_orig;

	/*
	 * Find the line in the map, and count the number of screen lines
	 * which display any part of the deleted line.
	 */
	for (p = HMAP; p->lno != lno; ++p);
	if (O_ISSET(sp, O_LEFTRIGHT))
		cnt_orig = 1;
	else
		for (cnt_orig = 1, t = p + 1;
		    t <= TMAP && t->lno == lno; ++cnt_orig, ++t);

	TOO_WEIRD;

	/* Delete that many lines from the screen. */
	MOVE(sp, p - HMAP, 0);
	if (svi_deleteln(sp, cnt_orig))
		return (1);

	/* Shift the screen map up. */
	memmove(p, p + cnt_orig, (((TMAP - p) - cnt_orig) + 1) * sizeof(SMAP));

	/* Decrement the line numbers for the rest of the map. */
	for (t = TMAP - cnt_orig; p <= t; ++p)
		--p->lno;

	/* Display the new lines. */
	for (p = TMAP - cnt_orig;;) {
		if (p < TMAP && svi_sm_next(sp, ep, p, p + 1))
			return (1);
		/* svi_sm_next() flushed the cache. */
		if (svi_line(sp, ep, ++p, NULL, NULL))
			return (1);
		if (p == TMAP)
			break;
	}
	return (0);
}

/*
 * svi_sm_insert --
 *	Insert a line into the SMAP.
 */
static int
svi_sm_insert(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	SMAP *p, *t;
	size_t cnt_orig, cnt;

	/*
	 * Find the line in the map, find out how many screen lines
	 * needed to display the line.
	 */
	for (p = HMAP; p->lno != lno; ++p);
	if (O_ISSET(sp, O_LEFTRIGHT))
		cnt_orig = 1;
	else
		cnt_orig = svi_opt_screens(sp, ep, lno, NULL);

	TOO_WEIRD;

	/*
	 * The lines left in the screen override the number of screen
	 * lines in the inserted line.
	 */
	cnt = (TMAP - p) + 1;
	if (cnt_orig > cnt)
		cnt_orig = cnt;

	/* Push down that many lines. */
	MOVE(sp, p - HMAP, 0);
	if (svi_insertln(sp, cnt_orig))
		return (1);

	/* Shift the screen map down. */
	memmove(p + cnt_orig, p, (((TMAP - p) - cnt_orig) + 1) * sizeof(SMAP));

	/* Increment the line numbers for the rest of the map. */
	for (t = p + cnt_orig; t <= TMAP; ++t)
		++t->lno;

	/* Fill in the SMAP for the new lines, and display. */
	for (cnt = 1, t = p; cnt <= cnt_orig; ++t, ++cnt) {
		t->lno = lno;
		t->off = cnt;
		SMAP_FLUSH(t);
		if (svi_line(sp, ep, t, NULL, NULL))
			return (1);
	}
	return (0);
}

/*
 * svi_sm_reset --
 *	Reset a line in the SMAP.
 */
static int
svi_sm_reset(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	SMAP *p, *t;
	size_t cnt_orig, cnt_new, cnt, diff;

	/*
	 * See if the number of on-screen rows taken up by the old display
	 * for the line is the same as the number needed for the new one.
	 * If so, repaint, otherwise do it the hard way.
	 */
	for (p = HMAP; p->lno != lno; ++p);
	if (O_ISSET(sp, O_LEFTRIGHT)) {
		t = p;
		cnt_orig = cnt_new = 1;
	} else {
		for (cnt_orig = 0,
		    t = p; t <= TMAP && t->lno == lno; ++cnt_orig, ++t);
		cnt_new = svi_opt_screens(sp, ep, lno, NULL);
	}

	TOO_WEIRD;

	if (cnt_orig == cnt_new) {
		do {
			SMAP_FLUSH(p);
			if (svi_line(sp, ep, p, NULL, NULL))
				return (1);
		} while (++p < t);
		return (0);
	}

	if (cnt_orig < cnt_new) {
		/* Get the difference. */
		diff = cnt_new - cnt_orig;

		/*
		 * The lines left in the screen override the number of screen
		 * lines in the inserted line.
		 */
		cnt = (TMAP - p) + 1;
		if (diff > cnt)
			diff = cnt;

		/* Push down the extra lines. */
		MOVE(sp, p - HMAP, 0);
		if (svi_insertln(sp, diff))
			return (1);

		/* Shift the screen map down. */
		memmove(p + diff, p, (((TMAP - p) - diff) + 1) * sizeof(SMAP));

		/* Fill in the SMAP for the replaced line, and display. */
		for (cnt = 1, t = p; cnt_new-- && t <= TMAP; ++t, ++cnt) {
			t->lno = lno;
			t->off = cnt;
			SMAP_FLUSH(t);
			if (svi_line(sp, ep, t, NULL, NULL))
				return (1);
		}
	} else {
		/* Get the difference. */
		diff = cnt_orig - cnt_new;

		/* Delete that many lines from the screen. */
		MOVE(sp, p - HMAP, 0);
		if (svi_deleteln(sp, diff))
			return (1);

		/* Shift the screen map up. */
		memmove(p, p + diff, (((TMAP - p) - diff) + 1) * sizeof(SMAP));

		/* Fill in the SMAP for the replaced line, and display. */
		for (cnt = 1, t = p; cnt_new--; ++t, ++cnt) {
			t->lno = lno;
			t->off = cnt;
			SMAP_FLUSH(t);
			if (svi_line(sp, ep, t, NULL, NULL))
				return (1);
		}

		/* Display the new lines at the bottom of the screen. */
		for (t = TMAP - diff;;) {
			if (t < TMAP && svi_sm_next(sp, ep, t, t + 1))
				return (1);
			/* svi_sm_next() flushed the cache. */
			if (svi_line(sp, ep, ++t, NULL, NULL))
				return (1);
			if (t == TMAP)
				break;
		}
	}
	return (0);
}

/*
 * svi_sm_scroll
 *	Scroll the SMAP up/down count logical lines.  Different
 *	semantics based on the vi command, *sigh*.
 */
int
svi_sm_scroll(sp, ep, rp, count, scmd)
	SCR *sp;
	EXF *ep;
	MARK *rp;
	recno_t count;
	enum sctype scmd;
{
	SMAP *smp;

	/*
	 * Invalidate the cursor.  The line is probably going to change,
	 * (although for ^E and ^Y it may not).  In any case, the scroll
	 * routines move the cursor to draw things.
	 */
	F_SET(SVP(sp), SVI_CUR_INVALID);

	/* Find the cursor in the screen. */
	if (svi_sm_cursor(sp, ep, &smp))
		return (1);

	switch (scmd) {
	case CNTRL_B:
	case CNTRL_U:
	case CNTRL_Y:
	case Z_CARAT:
		if (svi_sm_down(sp, ep, rp, count, scmd, smp))
			return (1);
		break;
	case CNTRL_D:
	case CNTRL_E:
	case CNTRL_F:
	case Z_PLUS:
		if (svi_sm_up(sp, ep, rp, count, scmd, smp))
			return (1);
		break;
	default:
		abort();
	}

	/*
	 * !!!
	 * If we're at the start of a line, go for the first non-blank.
	 * This makes it look like the old vi, even though we're moving
	 * around by logical lines, not physical ones.
	 *
	 * XXX
	 * In the presence of a long line, which has more than a screen
	 * width of leading spaces, this code can cause a cursor warp.
	 * Live with it.
	 */
	if (scmd != CNTRL_E && scmd != CNTRL_Y &&
	    rp->cno == 0 && nonblank(sp, ep, rp->lno, &rp->cno))
		return (1);

	return (0);
}

/*
 * svi_sm_up --
 *	Scroll the SMAP up count logical lines.
 */
static int
svi_sm_up(sp, ep, rp, count, scmd, smp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
	enum sctype scmd;
	recno_t count;
	SMAP *smp;
{
	int cursor_set, echanged, zset;
	SMAP s1, s2;

	/*
	 * Check to see if movement is possible.
	 *
	 * Get the line after the map.  If that line is a new one (and if
	 * O_LEFTRIGHT option is set, this has to be true), and the next
	 * line doesn't exist, and the cursor doesn't move, or the cursor
	 * isn't even on the screen, or the cursor is already at the last
	 * line in the map, it's an error.  If that test succeeded because
	 * the cursor wasn't at the end of the map, test to see if the map
	 * is mostly empty.
	 */
	if (svi_sm_next(sp, ep, TMAP, &s1))
		return (1);
	if (s1.lno > TMAP->lno && !file_gline(sp, ep, s1.lno, NULL)) {
		if (scmd == CNTRL_E || scmd == Z_PLUS || smp == TMAP) {
			v_eof(sp, ep, NULL);
			return (1);
		}
		if (svi_sm_next(sp, ep, smp, &s1))
			return (1);
		if (s1.lno > smp->lno && !file_gline(sp, ep, s1.lno, NULL)) {
			v_eof(sp, ep, NULL);
			return (1);
		}
	}

	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 2b.
	 *
	 * If it's a small screen, and the movement isn't larger than a
	 * screen, i.e some context will remain, open up the screen and
	 * display by scrolling.  In this case, the cursor moves to the
	 * first line displayed.  Otherwise, erase/compress and repaint,
	 * and move the cursor to the first line in the screen.  Note,
	 * the ^F command is always in the latter case, for historical
	 * reasons.
	 */
	cursor_set = 0;
	if (ISSMALLSCREEN(sp)) {
		if (count >= sp->t_maxrows || scmd == CNTRL_F) {
			s1 = TMAP[0];
			if (svi_sm_erase(sp))
				return (1);
			for (; count--; s1 = s2) {
				if (svi_sm_next(sp, ep, &s1, &s2))
					return (1);
				if (s2.lno != s1.lno &&
				    !file_gline(sp, ep, s2.lno, NULL))
					break;
			}
			TMAP[0] = s2;
			if (svi_sm_fill(sp, ep, OOBLNO, P_BOTTOM))
				return (1);
			return (svi_sm_position(sp, ep, rp, 0, P_TOP));
		}
		for (; count &&
		    sp->t_rows != sp->t_maxrows; --count, ++sp->t_rows) {
			if (svi_sm_next(sp, ep, TMAP, &s1))
				return (1);
			if (TMAP->lno != s1.lno &&
			    !file_gline(sp, ep, s1.lno, NULL))
				break;
			*++TMAP = s1;
			/* svi_sm_next() flushed the cache. */
			if (svi_line(sp, ep, TMAP, NULL, NULL))
				return (1);

			if (scmd != CNTRL_E && !cursor_set) {
				cursor_set = 1;
				rp->lno = TMAP->lno;
				rp->cno = TMAP->c_sboff;
			}
		}
		if (count == 0)
			return (0);
	}

	for (echanged = zset = 0; count; --count) {
		/* Decide what would show up on the screen. */
		if (svi_sm_next(sp, ep, TMAP, &s1))
			return (1);

		/* If the line doesn't exist, we're done. */
		if (TMAP->lno != s1.lno && !file_gline(sp, ep, s1.lno, NULL))
			break;

		/* Scroll the screen cursor up one logical line. */
		if (svi_sm_1up(sp, ep))
			return (1);
		switch (scmd) {
		case CNTRL_E:
			if (smp > HMAP)
				--smp;
			else
				echanged = 1;
			break;
		case Z_PLUS:
			if (zset) {
				if (smp > HMAP)
					--smp;
			} else {
				smp = TMAP;
				zset = 1;
			}
			/* FALLTHROUGH */
		default:
			break;
		}
	}

	if (cursor_set)
		return(0);

	switch (scmd) {
	case CNTRL_E:
		/*
		 * On a ^E that was forced to change lines, try and keep the
		 * cursor as close as possible to the last position, but also
		 * set it up so that the next "real" movement will return the
		 * cursor to the closest position to the last real movement.
		 */
		if (echanged) {
			rp->lno = smp->lno;
			rp->cno =
			    svi_cm_private(sp, ep, smp->lno, smp->off, sp->rcm);
		}
		return (0);
	case CNTRL_F:
		/*
		 * If there are more lines, the ^F command is always
		 * positioned at the first line of the screen.
		 */
		if (!count) {
			smp = HMAP;
			break;
		}
		/* FALLTHROUGH */
	case CNTRL_D:
		/*
		 * The ^D and ^F commands move the cursor towards EOF
		 * if there are more lines to move.  Check to be sure
		 * the lines actually exist.  (They may not if the
		 * file is smaller than the screen.)
		 */
		for (; count; --count, ++smp)
			if (smp == TMAP ||
			    !file_gline(sp, ep, smp[1].lno, NULL))
				break;
		break;
	case Z_PLUS:
		 /* The z+ command moves the cursor to the first new line. */
		break;
	default:
		abort();
	}

	if (!SMAP_CACHE(smp) && svi_line(sp, ep, smp, NULL, NULL))
		return (1);
	rp->lno = smp->lno;
	rp->cno = smp->c_sboff;
	return (0);
}

/*
 * svi_sm_1up --
 *	Scroll the SMAP up one.
 */
int
svi_sm_1up(sp, ep)
	SCR *sp;
	EXF *ep;
{
	/*
	 * Delete the top line of the screen.  Shift the screen map up.
	 * Display a new line at the bottom of the screen.
	 */
	MOVE(sp, 0, 0);
	if (svi_deleteln(sp, 1))
		return (1);

	/* One-line screens can fail. */
	if (HMAP == TMAP) {
		if (svi_sm_next(sp, ep, TMAP, TMAP))
			return (1);
	} else {
		memmove(HMAP, HMAP + 1, (sp->rows - 1) * sizeof(SMAP));
		if (svi_sm_next(sp, ep, TMAP - 1, TMAP))
			return (1);
	}
	/* svi_sm_next() flushed the cache. */
	if (svi_line(sp, ep, TMAP, NULL, NULL))
		return (1);
	return (0);
}

/*
 * svi_deleteln --
 *	Delete a line a la curses, make sure to put the information
 *	line and other screens back.
 */
static int
svi_deleteln(sp, cnt)
	SCR *sp;
	int cnt;
{
	size_t oldy, oldx;

	getyx(stdscr, oldy, oldx);
	while (cnt--) {
		deleteln();
		MOVE(sp, INFOLINE(sp) - 1, 0);
		insertln();
		MOVEA(sp, oldy, oldx);
	}
	return (0);
}

/*
 * svi_sm_down --
 *	Scroll the SMAP down count logical lines.
 */
static int
svi_sm_down(sp, ep, rp, count, scmd, smp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
	recno_t count;
	SMAP *smp;
	enum sctype scmd;
{
	SMAP s1, s2;
	int cursor_set, ychanged, zset;

	/* Check to see if movement is possible. */
	if (HMAP->lno == 1 && HMAP->off == 1 &&
	    (scmd == CNTRL_Y || scmd == Z_CARAT || smp == HMAP)) {
		v_sof(sp, NULL);
		return (1);
	}

	/*
	 * Small screens: see svi/svi_refresh.c:svi_refresh, section 2b.
	 *
	 * If it's a small screen, and the movement isn't larger than a
	 * screen, i.e some context will remain, open up the screen and
	 * display by scrolling.  In this case, the cursor moves to the
	 * first line displayed.  Otherwise, erase/compress and repaint,
	 * and move the cursor to the first line in the screen.  Note,
	 * the ^B command is always in the latter case, for historical
	 * reasons.
	 */
	cursor_set = scmd == CNTRL_Y;
	if (ISSMALLSCREEN(sp)) {
		if (count >= sp->t_maxrows || scmd == CNTRL_B) {
			s1 = HMAP[0];
			if (svi_sm_erase(sp))
				return (1);
			for (; count--; s1 = s2) {
				if (svi_sm_prev(sp, ep, &s1, &s2))
					return (1);
				if (s2.lno == 1 && s2.off == 1)
					break;
			}
			HMAP[0] = s2;
			if (svi_sm_fill(sp, ep, OOBLNO, P_TOP))
				return (1);
			return (svi_sm_position(sp, ep, rp, 0, P_BOTTOM));
		}
		for (; count &&
		    sp->t_rows != sp->t_maxrows; --count, ++sp->t_rows) {
			if (HMAP->lno == 1 || HMAP->off == 1)
				break;
			++TMAP;
			if (svi_sm_1down(sp, ep))
				return (1);
			if (scmd != CNTRL_Y && !cursor_set) {
				cursor_set = 1;
				if (svi_sm_position(sp, ep, rp, 0, P_BOTTOM))
					return (1);
			}
		}
		if (count == 0)
			return (0);
	}

	for (ychanged = zset = 0; count; --count) {
		/* If the line doesn't exist, we're done. */
		if (HMAP->lno == 1 && HMAP->off == 1)
			break;

		/* Scroll the screen and cursor down one logical line. */
		if (svi_sm_1down(sp, ep))
			return (1);
		switch (scmd) {
		case CNTRL_Y:
			if (smp < TMAP)
				++smp;
			else
				ychanged = 1;
			break;
		case Z_CARAT:
			if (zset) {
				if (smp < TMAP)
					++smp;
			} else {
				smp = HMAP;
				zset = 1;
			}
			/* FALLTHROUGH */
		default:
			break;
		}
	}

	if (scmd != CNTRL_Y && cursor_set)
		return(0);

	switch (scmd) {
	case CNTRL_B:
		/*
		 * If there are more lines, the ^B command is always
		 * positioned at the last line of the screen.
		 */
		if (!count) {
			smp = TMAP;
			break;
		}
		/* FALLTHROUGH */
	case CNTRL_U:
		/*
		 * The ^B and ^U commands move the cursor towards SOF
		 * if there are more lines to move.
		 */
		if (count < smp - HMAP)
			smp -= count;
		else
			smp = HMAP;
		break;
	case CNTRL_Y:
		/*
		 * On a ^Y that was forced to change lines, try and keep the
		 * cursor as close as possible to the last position, but also
		 * set it up so that the next "real" movement will return the
		 * cursor to the closest position to the last real movement.
		 */
		if (ychanged) {
			rp->lno = smp->lno;
			rp->cno =
			    svi_cm_private(sp, ep, smp->lno, smp->off, sp->rcm);
		}
		return (0);
	case Z_CARAT:
		 /* The z^ command moves the cursor to the first new line. */
		break;
	default:
		abort();
	}

	if (!SMAP_CACHE(smp) && svi_line(sp, ep, smp, NULL, NULL))
		return (1);
	rp->lno = smp->lno;
	rp->cno = smp->c_sboff;
	return (0);
}

/*
 * svi_sm_erase --
 *	Erase the small screen area for the scrolling functions.
 */
static int
svi_sm_erase(sp)
	SCR *sp;
{
	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();
	for (; sp->t_rows > sp->t_minrows; --sp->t_rows, --TMAP) {
		MOVE(sp, TMAP - HMAP, 0);
		clrtoeol();
	}
	return (0);
}

/*
 * svi_sm_1down --
 *	Scroll the SMAP down one.
 */
int
svi_sm_1down(sp, ep)
	SCR *sp;
	EXF *ep;
{
	/*
	 * Clear the bottom line of the screen, insert a line at the top
	 * of the screen.  Shift the screen map down, display a new line
	 * at the top of the screen.
	 */
	MOVE(sp, sp->t_rows, 0);
	clrtoeol();
	MOVE(sp, 0, 0);
	if (svi_insertln(sp, 1))
		return (1);
	memmove(HMAP + 1, HMAP, (sp->rows - 1) * sizeof(SMAP));
	if (svi_sm_prev(sp, ep, HMAP + 1, HMAP))
		return (1);
	/* svi_sm_prev() flushed the cache. */
	if (svi_line(sp, ep, HMAP, NULL, NULL))
		return (1);
	return (0);
}

/*
 * svi_insertln --
 *	Insert a line a la curses, make sure to put the information
 *	line and other screens back.
 */
static int
svi_insertln(sp, cnt)
	SCR *sp;
	int cnt;
{
	size_t oldy, oldx;

	getyx(stdscr, oldy, oldx);
	while (cnt--) {
		MOVE(sp, INFOLINE(sp) - 1, 0);
		deleteln();
		MOVEA(sp, oldy, oldx);
		insertln();
	}
	return (0);
}

/*
 * svi_sm_next --
 *	Fill in the next entry in the SMAP.
 */
int
svi_sm_next(sp, ep, p, t)
	SCR *sp;
	EXF *ep;
	SMAP *p, *t;
{
	size_t lcnt;

	SMAP_FLUSH(t);
	if (O_ISSET(sp, O_LEFTRIGHT)) {
		t->lno = p->lno + 1;
		t->off = p->off;
	} else {
		lcnt = svi_opt_screens(sp, ep, p->lno, NULL);
		if (lcnt == p->off) {
			t->lno = p->lno + 1;
			t->off = 1;
		} else {
			t->lno = p->lno;
			t->off = p->off + 1;
		}
	}
	return (0);
}

/*
 * svi_sm_prev --
 *	Fill in the previous entry in the SMAP.
 */
int
svi_sm_prev(sp, ep, p, t)
	SCR *sp;
	EXF *ep;
	SMAP *p, *t;
{
	SMAP_FLUSH(t);
	if (O_ISSET(sp, O_LEFTRIGHT)) {
		t->lno = p->lno - 1;
		t->off = p->off;
	} else if (p->off != 1) {
		t->lno = p->lno;
		t->off = p->off - 1;
	} else {
		t->lno = p->lno - 1;
		t->off = svi_opt_screens(sp, ep, t->lno, NULL);
	}
	return (t->lno == 0);
}

/*
 * svi_sm_cursor --
 *	Return the SMAP entry referenced by the cursor.
 */
int
svi_sm_cursor(sp, ep, smpp)
	SCR *sp;
	EXF *ep;
	SMAP **smpp;
{
	SMAP *p;

	/* See if the cursor is not in the map. */
	if (sp->lno < HMAP->lno || sp->lno > TMAP->lno)
		return (1);

	/* Find the first occurence of the line. */
	for (p = HMAP; p->lno != sp->lno; ++p);

	/* Fill in the map information until we find the right line. */
	for (; p <= TMAP; ++p) {
		/* Short lines are common and easy to detect. */
		if (p != TMAP && (p + 1)->lno != p->lno) {
			*smpp = p;
			return (0);
		}
		if (!SMAP_CACHE(p) && svi_line(sp, ep, p, NULL, NULL))
			return (1);
		if (p->c_eboff >= sp->cno) {
			*smpp = p;
			return (0);
		}
	}

	/* It was past the end of the map after all. */
	return (1);
}

/*
 * svi_sm_position --
 *	Return the line/column of the top, middle or last line on the screen.
 *	(The vi H, M and L commands.)  Here because only the screen routines
 *	know what's really out there.
 */
int
svi_sm_position(sp, ep, rp, cnt, pos)
	SCR *sp;
	EXF *ep;
	MARK *rp;
	u_long cnt;
	enum position pos;
{
	SMAP *smp;
	recno_t last;

	switch (pos) {
	case P_TOP:
		/*
		 * !!!
		 * Historically, an invalid count to the H command failed.
		 * We do nothing special here, just making sure that H in
		 * an empty screen works.
		 */
		if (cnt > TMAP - HMAP)
			goto sof;
		smp = HMAP + cnt;
		if (cnt && file_gline(sp, ep, smp->lno, NULL) == NULL) {
sof:			msgq(sp, M_BERR, "Movement past the end-of-screen");
			return (1);
		}
		break;
	case P_MIDDLE:
		/*
		 * !!!
		 * Historically, a count to the M command was ignored.
		 * If the screen isn't filled, find the middle of what's
		 * real and move there.
		 */
		if (file_gline(sp, ep, TMAP->lno, NULL) == NULL) {
			if (file_lline(sp, ep, &last))
				return (1);
			for (smp = TMAP; smp->lno > last && smp > HMAP; --smp);
			if (smp > HMAP)
				smp -= (smp - HMAP) / 2;
		} else
			smp = (HMAP + (TMAP - HMAP) / 2) + cnt;
		break;
	case P_BOTTOM:
		/*
		 * !!!
		 * Historically, an invalid count to the L command failed.
		 * If the screen isn't filled, find the bottom of what's
		 * real and try to offset from there.
		 */
		if (cnt > TMAP - HMAP)
			goto eof;
		smp = TMAP - cnt;
		if (file_gline(sp, ep, smp->lno, NULL) == NULL) {
			if (file_lline(sp, ep, &last))
				return (1);
			for (; smp->lno > last && smp > HMAP; --smp);
			if (cnt > smp - HMAP) {
eof:				msgq(sp, M_BERR,
				    "Movement past the beginning-of-screen");
				return (1);
			}
			smp -= cnt;
		}
		break;
	default:
		abort();
	}

	/* Make sure that the cached information is valid. */
	if (!SMAP_CACHE(smp) && svi_line(sp, ep, smp, NULL, NULL))
		return (1);
	rp->lno = smp->lno;
	rp->cno = smp->c_sboff;

	return (0);
}

/*
 * svi_sm_nlines --
 *	Return the number of screen lines from an SMAP entry to the
 *	start of some file line, less than a maximum value.
 */
recno_t
svi_sm_nlines(sp, ep, from_sp, to_lno, max)
	SCR *sp;
	EXF *ep;
	SMAP *from_sp;
	recno_t to_lno;
	size_t max;
{
	recno_t lno, lcnt;

	if (O_ISSET(sp, O_LEFTRIGHT))
		return (from_sp->lno > to_lno ?
		    from_sp->lno - to_lno : to_lno - from_sp->lno);

	if (from_sp->lno == to_lno)
		return (from_sp->off - 1);

	if (from_sp->lno > to_lno) {
		lcnt = from_sp->off - 1;	/* Correct for off-by-one. */
		for (lno = from_sp->lno; --lno >= to_lno && lcnt <= max;)
			lcnt += svi_opt_screens(sp, ep, lno, NULL);
	} else {
		lno = from_sp->lno;
		lcnt = (svi_opt_screens(sp, ep, lno, NULL) - from_sp->off) + 1;
		for (; ++lno < to_lno && lcnt <= max;)
			lcnt += svi_opt_screens(sp, ep, lno, NULL);
	}
	return (lcnt);
}
