/*
 * Copyright (c) 1987, 1993
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
static char sccsid[] = "@(#)addbytes.c	8.2 (Berkeley) 1/9/94";
#endif	/* not lint */

#include <curses.h>

#define	SYNCH_IN	{y = win->cury; x = win->curx;}
#define	SYNCH_OUT	{win->cury = y; win->curx = x;}

/*
 * waddbytes --
 *	Add the character to the current position in the given window.
 */
int
__waddbytes(win, bytes, count, so)
	register WINDOW *win;
	register const char *bytes;
	register int count;
	int so;
{
	static char blanks[] = "        ";
	register int c, newx, x, y;
	char stand;
	__LINE *lp;

	SYNCH_IN;

#ifdef DEBUG
	__CTRACE("ADDBYTES('%c') at (%d, %d)\n", c, y, x);
#endif
	while (count--) {
		c = *bytes++;
		switch (c) {
		case '\t':
			SYNCH_OUT;
			if (waddbytes(win, blanks, 8 - (x % 8)) == ERR)
				return (ERR);
			SYNCH_IN;
			break;

		default:
#ifdef DEBUG
	__CTRACE("ADDBYTES(%0.2o, %d, %d)\n", win, y, x);
#endif
			
			lp = win->lines[y];
			if (lp->flags & __ISPASTEOL) {
				lp->flags &= ~__ISPASTEOL;
newline:			if (y == win->maxy - 1) {
					if (win->flags & __SCROLLOK) {
						SYNCH_OUT;
						scroll(win);
						SYNCH_IN;
						lp = win->lines[y];
					        x = 0;
					} 
					else
						return ERR;
				} else {
					y++;
					lp = win->lines[y];
					x = 0;
				}
				if (c == '\n')
					break;
			}
				
			stand = '\0';
			if (win->flags & __WSTANDOUT || so)
				stand |= __STANDOUT;
#ifdef DEBUG
	__CTRACE("ADDBYTES: 1: y = %d, x = %d, firstch = %d, lastch = %d\n",
	    y, x, *win->lines[y]->firstchp, *win->lines[y]->lastchp);
#endif
			if (lp->line[x].ch != c || 
			    !(lp->line[x].attr & stand)) {
				newx = x + win->ch_off;
				if (!(lp->flags & __ISDIRTY)) {
					lp->flags |= __ISDIRTY;
					*lp->firstchp = *lp->lastchp = newx;
				}
				else if (newx < *lp->firstchp)
					*lp->firstchp = newx;
				else if (newx > *lp->lastchp)
					*lp->lastchp = newx;
#ifdef DEBUG
	__CTRACE("ADDBYTES: change gives f/l: %d/%d [%d/%d]\n",
	    *lp->firstchp, *lp->lastchp,
	    *lp->firstchp - win->ch_off,
	    *lp->lastchp - win->ch_off);
#endif
			}
			lp->line[x].ch = c;
			if (stand)
				lp->line[x].attr |= __STANDOUT;
			else
				lp->line[x].attr &= ~__STANDOUT;
			if (x == win->maxx - 1)
				lp->flags |= __ISPASTEOL;
			else
				x++;
#ifdef DEBUG
	__CTRACE("ADDBYTES: 2: y = %d, x = %d, firstch = %d, lastch = %d\n",
	    y, x, *win->lines[y]->firstchp, *win->lines[y]->lastchp);
#endif
			break;
		case '\n':
			SYNCH_OUT;
			wclrtoeol(win);
			SYNCH_IN;
			if (!NONL)
				x = 0;
			goto newline;
		case '\r':
			x = 0;
			break;
		case '\b':
			if (--x < 0)
				x = 0;
			break;
		}
	}
	SYNCH_OUT;
	return (OK);
}
