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
static char sccsid[] = "@(#)svi_line.c	8.22 (Berkeley) 3/24/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <curses.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "svi_screen.h"

#if defined(DEBUG) && 0
#define	TABCH	'-'
#define	TABSTR	"--------------------"
#else
#define	TABSTR	"                    "
#define	TABCH	' '
#endif

/*
 * svi_line --
 *	Update one line on the screen.
 */
int
svi_line(sp, ep, smp, yp, xp)
	SCR *sp;
	EXF *ep;
	SMAP *smp;
	size_t *xp, *yp;
{
	CHNAME const *cname;
	SMAP *tsmp;
	size_t chlen, cols_per_screen, cno_cnt, len, scno, skip_screens;
	size_t offset_in_char, offset_in_line;
	size_t oldy, oldx;
	int ch, is_cached, is_infoline, is_partial, is_tab, listset;
	char *p, nbuf[10];

#if defined(DEBUG) && 0
	TRACE(sp, "svi_line: row %u: line: %u off: %u\n",
	    smp - HMAP, smp->lno, smp->off);
#endif

	/*
	 * Assume that, if the cache entry for the line is filled in, the
	 * line is already on the screen, and all we need to do is return
	 * the cursor position.  If the calling routine doesn't need the
	 * cursor position, we can just return.
	 */
	is_cached = SMAP_CACHE(smp);
	if (yp == NULL && is_cached)
		return (0);

	/*
	 * A nasty side effect of this routine is that it returns the screen
	 * position for the "current" character.  Not pretty, but this is the
	 * only routine that really knows what's out there.
	 *
	 * Move to the line.  This routine can be called by svi_sm_position(),
	 * which uses it to fill in the cache entry so it can figure out what
	 * the real contents of the screen are.  Because of this, we have to
	 * return to whereever we started from.
	 */
	getyx(stdscr, oldy, oldx);
	MOVE(sp, smp - HMAP, 0);

	/* Get the character map. */
	cname = sp->gp->cname;

	/* Get a copy of the line. */
	p = file_gline(sp, ep, smp->lno, &len);

	/*
	 * Special case if we're printing the info/mode line.  Skip printing
	 * the leading number, as well as other minor setup.  If painting the
	 * line between two screens, it's always in reverse video.  The only
	 * time this code paints the mode line is when the user is entering
	 * text for a ":" command, so we can put the code here instead of
	 * dealing with the empty line logic below.  This is a kludge, but it's
	 * pretty much confined to this module.
	 *
	 * Set the number of screens to skip until a character is displayed.
	 * Left-right screens are special, because we don't bother building
	 * a buffer to be skipped over.
	 *
	 * Set the number of columns for this screen.
	 */
	cols_per_screen = sp->cols;
	if (is_infoline = ISINFOLINE(sp, smp)) {
		listset = 0;
		if (O_ISSET(sp, O_LEFTRIGHT))
			skip_screens = 0;
		else
			skip_screens = smp->off - 1;
	} else {
		listset = O_ISSET(sp, O_LIST);
		skip_screens = smp->off - 1;

		/*
		 * If O_NUMBER is set and it's line number 1 or the line exists
		 * and this is the first screen of a folding line or any left-
		 * right line, display the line number.
		 */
		if (O_ISSET(sp, O_NUMBER)) {
			cols_per_screen -= O_NUMBER_LENGTH;
			if ((smp->lno == 1 || p != NULL) && skip_screens == 0) {
				(void)snprintf(nbuf,
				    sizeof(nbuf), O_NUMBER_FMT, smp->lno);
				ADDSTR(nbuf);
			}
		}
	}

	/*
	 * Special case non-existent lines and the first line of an empty
	 * file.  In both cases, the cursor position is 0, but corrected
	 * for the O_NUMBER field if it was displayed.
	 */
	if (p == NULL || len == 0) {
		/* Fill in the cursor. */
		if (yp != NULL && smp->lno == sp->lno) {
			*yp = smp - HMAP;
			*xp = sp->cols - cols_per_screen;
		}

		/* If the line is on the screen, quit. */
		if (is_cached)
			goto ret;

		/* Set line cacheing information. */
		smp->c_sboff = smp->c_eboff = 0;
		smp->c_scoff = smp->c_eclen = 0;

		/* Lots of special cases for empty lines. */
		if (skip_screens == 0)
			if (p == NULL) {
				if (smp->lno == 1) {
					if (listset) {
						ch = '$';
						goto empty;
					}
				} else {
					ch = '~';
					goto empty;
				}
			} else
				if (listset) {
					ch = '$';
empty:					ADDCH(ch);
				}

		clrtoeol();
		MOVEA(sp, oldy, oldx);
		return (0);
	}

	/*
	 * If we wrote a line that's this or a previous one, we can do this
	 * much more quickly -- we cached the starting and ending positions
	 * of that line.  The way it works is we keep information about the
	 * lines displayed in the SMAP.  If we're painting the screen in
	 * the forward, this saves us from reformatting the physical line for
	 * every line on the screen.  This wins big on binary files with 10K
	 * lines.
	 *
	 * Test for the first screen of the line, then the current screen line,
	 * then the line behind us, then do the hard work.  Note, it doesn't
	 * do us any good to have a line in front of us -- it would be really
	 * hard to try and figure out tabs in the reverse direction, i.e. how
	 * many spaces a tab takes up in the reverse direction depends on
	 * what characters preceded it.
	 */
	if (smp->off == 1) {
		smp->c_sboff = offset_in_line = 0;
		smp->c_scoff = offset_in_char = 0;
		p = &p[offset_in_line];
	} else if (is_cached) {
		offset_in_line = smp->c_sboff;
		offset_in_char = smp->c_scoff;
		p = &p[offset_in_line];
		if (skip_screens != 0)
			cols_per_screen = sp->cols;
	} else if (smp != HMAP &&
	    SMAP_CACHE(tsmp = smp - 1) && tsmp->lno == smp->lno) {
		if (tsmp->c_eclen != tsmp->c_ecsize) {
			offset_in_line = tsmp->c_eboff;
			offset_in_char = tsmp->c_eclen;
		} else {
			offset_in_line = tsmp->c_eboff + 1;
			offset_in_char = 0;
		}

		/* Put starting info for this line in the cache. */
		smp->c_sboff = offset_in_line;
		smp->c_scoff = offset_in_char;
		p = &p[offset_in_line];
		if (skip_screens != 0)
			cols_per_screen = sp->cols;
	} else {
		offset_in_line = 0;
		offset_in_char = 0;

		/* This is the loop that skips through screens. */
		if (skip_screens == 0) {
			smp->c_sboff = offset_in_line;
			smp->c_scoff = offset_in_char;
		} else for (scno = 0; offset_in_line < len; ++offset_in_line) {
			scno += chlen =
			    (ch = *(u_char *)p++) == '\t' && !listset ?
			    TAB_OFF(sp, scno) : cname[ch].len;
			if (scno < cols_per_screen)
				continue;
			/*
			 * Reset cols_per_screen to second and subsequent line
			 * length.
			 */
			scno -= cols_per_screen;
			cols_per_screen = sp->cols;

			/*
			 * If crossed the last skipped screen boundary, start
			 * displaying the characters.
			 */
			if (--skip_screens)
				continue;

			/* Put starting info for this line in the cache. */
			if (scno) {
				smp->c_sboff = offset_in_line;
				smp->c_scoff = offset_in_char = chlen - scno;
				--p;
			} else {
				smp->c_sboff = ++offset_in_line;
				smp->c_scoff = 0;
			}
			break;
		}
	}

	/*
	 * Set the number of characters to skip before reaching the cursor
	 * character.  Offset by 1 and use 0 as a flag value.  Svi_line is
	 * called repeatedly with a valid pointer to a cursor position.
	 * Don't fill anything in unless it's the right line and the right
	 * character, and the right part of the character...
	 */
	if (yp == NULL ||
	    smp->lno != sp->lno || sp->cno < offset_in_line ||
	    offset_in_line + cols_per_screen < sp->cno) {
		cno_cnt = 0;
		/* If the line is on the screen, quit. */
		if (is_cached)
			goto ret;
	} else
		cno_cnt = (sp->cno - offset_in_line) + 1;

	/* This is the loop that actually displays characters. */
	for (is_partial = 0, scno = 0;
	    offset_in_line < len; ++offset_in_line, offset_in_char = 0) {
		if ((ch = *(u_char *)p++) == '\t' && !listset) {
			scno += chlen = TAB_OFF(sp, scno) - offset_in_char;
			is_tab = 1;
		} else {
			scno += chlen = cname[ch].len - offset_in_char;
			is_tab = 0;
		}

		/*
		 * Only display up to the right-hand column.  Set a flag if
		 * the entire character wasn't displayed for use in setting
		 * the cursor.  If reached the end of the line, set the cache
		 * info for the screen.  Don't worry about there not being
		 * characters to display on the next screen, its lno/off won't
		 * match up in that case.
		 */
		if (scno >= cols_per_screen) {
			smp->c_ecsize = chlen;
			chlen -= scno - cols_per_screen;
			smp->c_eclen = chlen;
			smp->c_eboff = offset_in_line;
			if (scno > cols_per_screen)
				is_partial = 1;

			/* Terminate the loop. */
			offset_in_line = len;
		}

		/*
		 * If the caller wants the cursor value, and this was the
		 * cursor character, set the value.  There are two ways to
		 * put the cursor on a character -- if it's normal display
		 * mode, it goes on the last column of the character.  If
		 * it's input mode, it goes on the first.  In normal mode,
		 * set the cursor only if the entire character was displayed.
		 */
		if (cno_cnt &&
		    --cno_cnt == 0 && (F_ISSET(sp, S_INPUT) || !is_partial)) {
			*yp = smp - HMAP;
			if (F_ISSET(sp, S_INPUT))
				*xp = scno - chlen;
			else
				*xp = scno - 1;
			if (O_ISSET(sp, O_NUMBER) &&
			    !is_infoline && smp->off == 1)
				*xp += O_NUMBER_LENGTH;

			/* If the line is on the screen, quit. */
			if (is_cached)
				goto ret;
		}

		/* If the line is on the screen, don't display anything. */
		if (is_cached)
			continue;

		/*
		 * Display the character.  If it's a tab and tabs aren't some
		 * ridiculous length, do it fast.  (We do tab expansion here
		 * because curses doesn't have a way to set the tab length.)
		 */
		if (is_tab) {
			if (chlen <= sizeof(TABSTR) - 1) {
				ADDNSTR(TABSTR, chlen);
			} else
				while (chlen--)
					ADDCH(TABCH);
		} else
			ADDNSTR(cname[ch].name + offset_in_char, chlen);
	}

	if (scno < cols_per_screen) {
		/* If didn't paint the whole line, update the cache. */
		smp->c_ecsize = smp->c_eclen = cname[ch].len;
		smp->c_eboff = len - 1;

		/*
		 * If not the info/mode line, and O_LIST set, and at the
		 * end of the line, and the line ended on this screen,
		 * add a trailing $.
		 */
		if (listset) {
			++scno;
			ADDCH('$');
		}

		/* If still didn't paint the whole line, clear the rest. */
		if (scno < cols_per_screen)
			clrtoeol();
	}

ret:	MOVEA(sp, oldy, oldx);
	return (0);
}

/*
 * svi_number --
 *	Repaint the numbers on all the lines.
 */
int
svi_number(sp, ep)
	SCR *sp;
	EXF *ep;
{
	SMAP *smp;
	size_t oldy, oldx;
	char *lp, nbuf[10];

	/*
	 * Try and avoid getting the last line in the file, by getting the
	 * line after the last line in the screen -- if it exists, we know
	 * we have to to number all the lines in the screen.  Get the one
	 * after the last instead of the last, so that the info line doesn't
	 * fool us.
	 *
	 * If that test fails, we have to check each line for existence.
	 *
	 * XXX
	 * The problem is that file_lline will lie, and tell us that the
	 * info line is the last line in the file.
	 */
	lp = file_gline(sp, ep, TMAP->lno + 1, NULL);

	getyx(stdscr, oldy, oldx);
	for (smp = HMAP; smp <= TMAP; ++smp) {
		if (smp->off != 1)
			continue;
		if (ISINFOLINE(sp, smp))
			break;
		if (smp->lno != 1 && lp == NULL &&
		    file_gline(sp, ep, smp->lno, NULL) == NULL)
			break;
		MOVE(sp, smp - HMAP, 0);
		(void)snprintf(nbuf, sizeof(nbuf), O_NUMBER_FMT, smp->lno);
		ADDSTR(nbuf);
	}
	MOVEA(sp, oldy, oldx);
	return (0);
}
