/*
 * Copyright (c) 1981, 1993
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
static char sccsid[] = "@(#)refresh.c	8.1 (Berkeley) 7/20/93";
#endif /* not lint */

#include <curses.h>
#include <string.h>

static int curwin;
static short ly, lx;

static void	domvcur __P((int, int, int, int));
static int	makech __P((WINDOW *, int));
static void	quickch __P((WINDOW *));	
static void	scrolln __P((WINDOW *, int, int, int, int, int));

/*
 * wrefresh --
 *	Make the current screen look like "win" over the area coverd by
 *	win.
 */
int
wrefresh(win)
	register WINDOW *win;
{
	register __LINE *wlp;
	register int retval;
	register short wy;
	int dnum;
	
	/* Initialize loop parameters. */
	ly = curscr->cury;
	lx = curscr->curx;
	wy = 0;
	curwin = (win == curscr);

	if (!curwin)
		for (wy = 0; wy < win->maxy; wy++) {
			wlp = win->lines[wy];
			if (wlp->flags & __ISDIRTY)
				wlp->hash = 
				   __hash((char *) wlp->line, win->maxx * __LDATASIZE);
		}

	if (win->flags & __CLEAROK || curscr->flags & __CLEAROK || curwin) {
		if ((win->flags & __FULLWIN) || curscr->flags & __CLEAROK) {
			tputs(CL, 0, __cputchar);
			ly = 0;
			lx = 0;
			if (!curwin) {
				curscr->flags &= ~__CLEAROK;
				curscr->cury = 0;
				curscr->curx = 0;
				werase(curscr);
			}
			__touchwin(win);
		}
		win->flags &= ~__CLEAROK;
	}
	if (!CA) {
		if (win->curx != 0)
			putchar('\n');
		if (!curwin)
			werase(curscr);
	}
#ifdef DEBUG
	__CTRACE("wrefresh: (%0.2o): curwin = %d\n", win, curwin);
	__CTRACE("wrefresh: \tfirstch\tlastch\n");
#endif

#ifndef NOQCH
	if ((win->flags & __FULLWIN) && !curwin) {
		/*
		 * Invoke quickch() only if more than a quarter of the lines
		 * in the window are dirty.
		 */
		for (wy = 0, dnum = 0; wy < win->maxy; wy++)
			if (win->lines[wy]->flags & (__ISDIRTY | __FORCEPAINT))
				dnum++;
		if (!__noqch && dnum > (int) win->maxy / 4)
			quickch(win);
	}
#endif

#ifdef DEBUG
{ int i, j;
		__CTRACE("#####################################\n");
		for (i = 0; i < curscr->maxy; i++) {
			__CTRACE("C: %d:", i);
			__CTRACE(" 0x%x \n", curscr->lines[i]->hash);
			for (j = 0; j < curscr->maxx; j++) 
				__CTRACE("%c", 
			           curscr->lines[i]->line[j].ch);
			__CTRACE("\n");
			for (j = 0; j < curscr->maxx; j++) 
				__CTRACE("%x", 
			           curscr->lines[i]->line[j].attr);
			__CTRACE("\n");
			__CTRACE("W: %d:", i);
			__CTRACE(" 0x%x \n", win->lines[i]->hash);
			__CTRACE(" 0x%x ", win->lines[i]->flags);
			for (j = 0; j < win->maxx; j++) 
				__CTRACE("%c", 
			           win->lines[i]->line[j].ch);
			__CTRACE("\n");
			for (j = 0; j < win->maxx; j++) 
				__CTRACE("%x", 
			           win->lines[i]->line[j].attr);
			__CTRACE("\n");
		}
}
#endif /* DEBUG */

	for (wy = 0; wy < win->maxy; wy++) {
#ifdef DEBUG
		__CTRACE("%d\t%d\t%d\n",
		    wy, *win->lines[wy]->firstchp, *win->lines[wy]->lastchp);
#endif
		if (!curwin)
			curscr->lines[wy]->hash = win->lines[wy]->hash;
		if (win->lines[wy]->flags & (__ISDIRTY | __FORCEPAINT)) {
			if (makech(win, wy) == ERR)
				return (ERR);
			else {
				if (*win->lines[wy]->firstchp >= win->ch_off)
					*win->lines[wy]->firstchp = win->maxx +
					    win->ch_off;
				if (*win->lines[wy]->lastchp < win->maxx +
				    win->ch_off)
					*win->lines[wy]->lastchp = win->ch_off;
				if (*win->lines[wy]->lastchp < 
				    *win->lines[wy]->firstchp) {
#ifdef DEBUG
					__CTRACE("wrefresh: line %d notdirty \n", wy);
#endif
					win->lines[wy]->flags &= ~__ISDIRTY;
				}
			}

		}
#ifdef DEBUG
		__CTRACE("\t%d\t%d\n", *win->lines[wy]->firstchp, 
			*win->lines[wy]->lastchp);
#endif
	}
	
#ifdef DEBUG
	__CTRACE("refresh: ly=%d, lx=%d\n", ly, lx);
#endif

	if (win == curscr)
		domvcur(ly, lx, win->cury, win->curx);
	else {
		if (win->flags & __LEAVEOK) {
			curscr->cury = ly;
			curscr->curx = lx;
			ly -= win->begy;
			lx -= win->begx;
			if (ly >= 0 && ly < win->maxy && lx >= 0 &&
			    lx < win->maxx) {
				win->cury = ly;
				win->curx = lx;
			} else
				win->cury = win->curx = 0;
		} else {
			domvcur(ly, lx, win->cury + win->begy,
			    win->curx + win->begx);
			curscr->cury = win->cury + win->begy;
			curscr->curx = win->curx + win->begx;
		}
	}
	retval = OK;

	(void)fflush(stdout);
	return (retval);
}

/*
 * makech --
 *	Make a change on the screen.
 */
static int
makech(win, wy)
	register WINDOW *win;
	int wy;
{
	static __LDATA blank = {' ', 0};
	register int nlsp, clsp;		/* Last space in lines. */
	register int wx, lch, y;
	register __LDATA *nsp, *csp, *cp, *cep;
	u_int force;
	char *ce;

	/* Is the cursor still on the end of the last line? */
	if (wy > 0 && win->lines[wy - 1]->flags & __ISPASTEOL) {
		domvcur(ly, lx, ly + 1, 0);
		ly++;
		lx = 0;
	}
	wx = *win->lines[wy]->firstchp - win->ch_off;
	if (wx < 0)
		wx = 0;
	else if (wx >= win->maxx)
		return (OK);
	lch = *win->lines[wy]->lastchp - win->ch_off;
	if (lch < 0)
		return (OK);
	else if (lch >= (int) win->maxx)
		lch = win->maxx - 1;
	y = wy + win->begy;

	if (curwin)
		csp = &blank;
	else
		csp = &curscr->lines[wy + win->begy]->line[wx + win->begx];

	nsp = &win->lines[wy]->line[wx];
	force = win->lines[wy]->flags & __FORCEPAINT;
	win->lines[wy]->flags &= ~__FORCEPAINT;
	if (CE && !curwin) {
		for (cp = &win->lines[wy]->line[win->maxx - 1]; 
		     cp->ch == ' ' && cp->attr == 0; cp--)
			if (cp <= win->lines[wy]->line)
				break;
		nlsp = cp - win->lines[wy]->line;
	}
	if (!curwin)
		ce = CE;
	else
		ce = NULL;

	if (force) {
		if (CM)
			tputs(tgoto(CM, lx, ly), 0, __cputchar);
		else {
			tputs(HO, 0, __cputchar);
			__mvcur(0, 0, ly, lx, 1);
		}
	}
	while (wx <= lch) {
		if (!force && memcmp(nsp, csp, sizeof(__LDATA)) == 0) {
			if (wx <= lch) {
				while (wx <= lch &&
				       memcmp(nsp, csp, sizeof(__LDATA)) == 0) {
					    nsp++;
					    if (!curwin)
						    csp++;
					    ++wx;
				    }
				continue;
			}
			break;
		}
		domvcur(ly, lx, y, wx + win->begx);

#ifdef DEBUG
		__CTRACE("makech: 1: wx = %d, ly= %d, lx = %d, newy = %d, newx = %d, force =%d\n", 
		    wx, ly, lx, y, wx + win->begx, force);
#endif
		ly = y;
		lx = wx + win->begx;
		while ((force || memcmp(nsp, csp, sizeof(__LDATA)) != 0) 
		    && wx <= lch) {

			if (ce != NULL && win->maxx + win->begx == 
			    curscr->maxx && wx >= nlsp && nsp->ch == ' ') {
				/* Check for clear to end-of-line. */
				cep = &curscr->lines[wy]->line[win->maxx - 1];
				while (cep->ch == ' ' && cep->attr == 0)
					if (cep-- <= csp)
						break;
				clsp = cep - curscr->lines[wy]->line - 
				       win->begx * __LDATASIZE;
#ifdef DEBUG
			__CTRACE("makech: clsp = %d, nlsp = %d\n", clsp, nlsp);
#endif
				if ((clsp - nlsp >= strlen(CE) 
				    && clsp < win->maxx * __LDATASIZE) ||
				    wy == win->maxy - 1) {
#ifdef DEBUG
					__CTRACE("makech: using CE\n");
#endif
					tputs(CE, 0, __cputchar);
					lx = wx + win->begx;
					while (wx++ <= clsp) {
						csp->ch = ' ';
						csp->attr = 0;
						csp++;
					}
					return (OK);
				}
				ce = NULL;
			}

			/* Enter/exit standout mode as appropriate. */
			if (SO && (nsp->attr & __STANDOUT) !=
			    (curscr->flags & __WSTANDOUT)) {
				if (nsp->attr & __STANDOUT) {
					tputs(SO, 0, __cputchar);
					curscr->flags |= __WSTANDOUT;
				} else {
					tputs(SE, 0, __cputchar);
					curscr->flags &= ~__WSTANDOUT;
				}
			}

			wx++;
			if (wx >= win->maxx && wy == win->maxy - 1 && !curwin)
				if (win->flags & __SCROLLOK) {
					if (curscr->flags & __WSTANDOUT
					    && win->flags & __ENDLINE)
						if (!MS) {
							tputs(SE, 0,
							    __cputchar);
							curscr->flags &=
							    ~__WSTANDOUT;
						}
					if (!(win->flags & __SCROLLWIN)) {
						if (!curwin) {
							csp->attr = nsp->attr;
							putchar(csp->ch = nsp->ch);
						} else
							putchar(nsp->ch);
					}
					if (wx + win->begx < curscr->maxx) {
						domvcur(ly, wx + win->begx, 
						    win->begy + win->maxy - 1,
						    win->begx + win->maxx - 1);
					}
					ly = win->begy + win->maxy - 1;
					lx = win->begx + win->maxx - 1;
					return (OK);
				} 
			if (wx < win->maxx || wy < win->maxy - 1 || 
			    !(win->flags & __SCROLLWIN)) {
				if (!curwin) {
					csp->attr = nsp->attr;
					putchar(csp->ch = nsp->ch);
					csp++;
				} else 
					putchar(nsp->ch);
			}
#ifdef DEBUG
			__CTRACE("makech: putchar(%c)\n", nsp->ch & 0177);
#endif
			if (UC && (nsp->attr & __STANDOUT)) {
				putchar('\b');
				tputs(UC, 0, __cputchar);
			}
			nsp++;
#ifdef DEBUG
		__CTRACE("makech: 2: wx = %d, lx = %d\n", wx, lx);
#endif
		}
		if (lx == wx + win->begx)	/* If no change. */
			break;
		lx = wx + win->begx;
		if (lx >= COLS && AM)
			lx = COLS - 1;
		else if (wx >= win->maxx) {
			domvcur(ly, lx, ly, win->maxx + win->begx - 1);
			lx = win->maxx + win->begx - 1;
		}

#ifdef DEBUG
		__CTRACE("makech: 3: wx = %d, lx = %d\n", wx, lx);
#endif
	}
	return (OK);
}

/*
 * domvcur --
 *	Do a mvcur, leaving standout mode if necessary.
 */
static void
domvcur(oy, ox, ny, nx)
	int oy, ox, ny, nx;
{
	if (curscr->flags & __WSTANDOUT && !MS) {
		tputs(SE, 0, __cputchar);
		curscr->flags &= ~__WSTANDOUT;
	}

	__mvcur(oy, ox, ny, nx, 1);
}

/*
 * Quickch() attempts to detect a pattern in the change of the window
 * in order to optimize the change, e.g., scroll n lines as opposed to 
 * repainting the screen line by line.
 */

static void
quickch(win)
	WINDOW *win;
{
#define THRESH		(int) win->maxy / 4

	register __LINE *clp, *tmp1, *tmp2;
	register int bsize, curs, curw, starts, startw, i, j;
	int n, target, cur_period, bot, top, sc_region;
	__LDATA buf[1024];
	u_int blank_hash;

	/* 
	 * Find how many lines from the top of the screen are unchanged.
	 */
	for (top = 0; top < win->maxy; top++) 
		if (win->lines[top]->flags & __FORCEPAINT ||
		    win->lines[top]->hash != curscr->lines[top]->hash 
		    || memcmp(win->lines[top]->line, 
		    curscr->lines[top]->line, 
		    win->maxx * __LDATASIZE) != 0)
			break;
		else
			win->lines[top]->flags &= ~__ISDIRTY;
       /*
	* Find how many lines from bottom of screen are unchanged. 
	*/
	for (bot = win->maxy - 1; bot >= 0; bot--)
		if (win->lines[bot]->flags & __FORCEPAINT ||
		    win->lines[bot]->hash != curscr->lines[bot]->hash 
		    || memcmp(win->lines[bot]->line, 
		    curscr->lines[bot]->line, 
		    win->maxx * __LDATASIZE) != 0)
			break;
		else
			win->lines[bot]->flags &= ~__ISDIRTY;

#ifdef NO_JERKINESS
	/*
	 * If we have a bottom unchanged region return.  Scrolling the
	 * bottom region up and then back down causes a screen jitter.
	 * This will increase the number of characters sent to the screen
	 * but it looks better.
	 */
	if (bot < win->maxy - 1)
		return;
#endif /* NO_JERKINESS */

	/*
	 * Search for the largest block of text not changed.
	 * Invariants of the loop:
	 * - Startw is the index of the beginning of the examined block in win.
         * - Starts is the index of the beginning of the examined block in 
	 *    curscr.
	 * - Curs is the index of one past the end of the exmined block in win.
	 * - Curw is the index of one past the end of the exmined block in 
	 *   curscr.
	 * - bsize is the current size of the examined block.
         */
	for (bsize = bot - top; bsize >= THRESH; bsize--) {
		for (startw = top; startw <= bot - bsize; startw++)
			for (starts = top; starts <= bot - bsize; 
			     starts++) {
				for (curw = startw, curs = starts;
				     curs < starts + bsize; curw++, curs++)
					if (win->lines[curw]->flags &
					    __FORCEPAINT ||
					    (win->lines[curw]->hash !=
					    curscr->lines[curs]->hash ||
				            memcmp(win->lines[curw]->line, 
					    curscr->lines[curs]->line, 
					    win->maxx * __LDATASIZE) != 0))
						break;
				if (curs == starts + bsize)
					goto done;
			}
	}
 done:
	/* Did not find anything */
	if (bsize < THRESH)	
		return;

#ifdef DEBUG
	__CTRACE("quickch:bsize=%d,starts=%d,startw=%d,curw=%d,curs=%d,top=%d,bot=%d\n", 
		bsize, starts, startw, curw, curs, top, bot);
#endif

	/* 
	 * Make sure that there is no overlap between the bottom and top 
	 * regions and the middle scrolled block.
	 */
	if (bot < curs)
		bot = curs - 1;
	if (top > starts)
		top = starts;

	n = startw - starts;

#ifdef DEBUG
		__CTRACE("#####################################\n");
		for (i = 0; i < curscr->maxy; i++) {
			__CTRACE("C: %d:", i);
			__CTRACE(" 0x%x \n", curscr->lines[i]->hash);
			for (j = 0; j < curscr->maxx; j++) 
				__CTRACE("%c", 
			           curscr->lines[i]->line[j].ch);
			__CTRACE("\n");
			for (j = 0; j < curscr->maxx; j++) 
				__CTRACE("%x", 
			           curscr->lines[i]->line[j].attr);
			__CTRACE("\n");
			__CTRACE("W: %d:", i);
			__CTRACE(" 0x%x \n", win->lines[i]->hash);
			__CTRACE(" 0x%x ", win->lines[i]->flags);
			for (j = 0; j < win->maxx; j++) 
				__CTRACE("%c", 
			           win->lines[i]->line[j].ch);
			__CTRACE("\n");
			for (j = 0; j < win->maxx; j++) 
				__CTRACE("%x", 
			           win->lines[i]->line[j].attr);
			__CTRACE("\n");
		}
#endif 
	
	/* So we don't have to call __hash() each time */
	for (i = 0; i < win->maxx; i++) {
		buf[i].ch = ' ';
		buf[i].attr = 0;
	}
	blank_hash = __hash((char *) buf, win->maxx * __LDATASIZE);

	/*
	 * Perform the rotation to maintain the consistency of curscr.
	 * This is hairy since we are doing an *in place* rotation.
	 * Invariants of the loop:
	 * - I is the index of the current line.
	 * - Target is the index of the target of line i.
	 * - Tmp1 points to current line (i).
	 * - Tmp2 and points to target line (target);
	 * - Cur_period is the index of the end of the current period. 
	 *   (see below).
	 *
	 * There are 2 major issues here that make this rotation non-trivial:
	 * 1.  Scrolling in a scrolling region bounded by the top
	 *     and bottom regions determined (whose size is sc_region).
	 * 2.  As a result of the use of the mod function, there may be a 
	 *     period introduced, i.e., 2 maps to 4, 4 to 6, n-2 to 0, and
	 *     0 to 2, which then causes all odd lines not to be rotated.
	 *     To remedy this, an index of the end ( = beginning) of the 
	 *     current 'period' is kept, cur_period, and when it is reached, 
	 *     the next period is started from cur_period + 1 which is 
	 *     guaranteed not to have been reached since that would mean that
	 *     all records would have been reached. (think about it...).
	 * 
	 * Lines in the rotation can have 3 attributes which are marked on the
	 * line so that curscr is consistent with the visual screen.
	 * 1.  Not dirty -- lines inside the scrolled block, top region or
	 *                  bottom region.
	 * 2.  Blank lines -- lines in the differential of the scrolling 
	 *		      region adjacent to top and bot regions 
	 *                    depending on scrolling direction.
	 * 3.  Dirty line -- all other lines are marked dirty.
	 */
	sc_region = bot - top + 1;
	i = top;
	tmp1 = curscr->lines[top];
	cur_period = top;
	for (j = top; j <= bot; j++) {
		target = (i - top + n + sc_region) % sc_region + top;
		tmp2 = curscr->lines[target];
		curscr->lines[target] = tmp1;
		/* Mark block as clean and blank out scrolled lines. */
		clp = curscr->lines[target];
#ifdef DEBUG
		__CTRACE("quickch: n=%d startw=%d curw=%d i = %d target=%d ",
			n, startw, curw, i, target);
#endif
		if ((target >= startw && target < curw) || target < top 
		    || target > bot) {
#ifdef DEBUG
			__CTRACE("-- notdirty");
#endif
			win->lines[target]->flags &= ~__ISDIRTY;
		} else if ((n > 0 && target >= top && target < top + n) ||
		           (n < 0 && target <= bot && target > bot + n)) {
			if (clp->hash != blank_hash ||  memcmp(clp->line, 
			    buf, win->maxx * __LDATASIZE) !=0) {
				(void)memcpy(clp->line,  buf,
				    win->maxx * __LDATASIZE);
#ifdef DEBUG
				__CTRACE("-- blanked out: dirty");
#endif
				clp->hash = blank_hash;
				__touchline(win, target, 0, win->maxx - 1, 0);
			} else {
				__touchline(win, target, 0, win->maxx - 1, 0);
#ifdef DEBUG
				__CTRACE(" -- blank line already: dirty");
#endif
			}
		} else {
#ifdef DEBUG
			__CTRACE(" -- dirty");
#endif
			__touchline(win, target, 0, win->maxx - 1, 0);
		}
#ifdef DEBUG
		__CTRACE("\n");
#endif
		if (target == cur_period) {
			i = target + 1;
			tmp1 = curscr->lines[i];
			cur_period = i;
		} else {
			tmp1 = tmp2;
			i = target;
		}
	}
#ifdef DEBUG
		__CTRACE("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
		for (i = 0; i < curscr->maxy; i++) {
			__CTRACE("C: %d:", i);
			for (j = 0; j < curscr->maxx; j++) 
				__CTRACE("%c", 
			           curscr->lines[i]->line[j].ch);
			__CTRACE("\n");
			__CTRACE("W: %d:", i);
			for (j = 0; j < win->maxx; j++) 
				__CTRACE("%c", 
			           win->lines[i]->line[j].ch);
			__CTRACE("\n");
		}
#endif
	if (n != 0) {
		WINDOW *wp;
		scrolln(win, starts, startw, curs, bot, top);
		/*
		 * Need to repoint any subwindow lines to the rotated
		 * line structured. 
		 */
		for (wp = win->nextp; wp != win; wp = wp->nextp)
			__set_subwin(win, wp);
	}
}

/*
 * Scrolln performs the scroll by n lines, where n is starts - startw.
 */
static void
scrolln(win, starts, startw, curs, bot, top)
	WINDOW *win;
	int starts, startw, curs, bot, top;
{
	int i, oy, ox, n;

	oy = curscr->cury;
	ox = curscr->curx;
	n = starts - startw;

	if (n > 0) {
		__mvcur(oy, ox, top, 0, 1);
		/* Scroll up the block */
		if (DL)
			tputs(__tscroll(DL, n), 0, __cputchar);
		else
			for(i = 0; i < n; i++)
				tputs(dl, 0, __cputchar);

		/* 
		 * Push down the bottom region.
		 */
		__mvcur(top, 0, bot - n + 1, 0, 1);
		if (AL) 
			tputs(__tscroll(AL, n), 0, __cputchar);
		else
			for(i = 0; i < n; i++)
				tputs(al, 0, __cputchar);
		__mvcur(bot - n + 1, 0, oy, ox, 1);
	} else {
		/* Preserve the bottom lines */
		__mvcur(oy, ox, bot + n + 1, 0, 1);	/* n < 0 */
		if (DL)
			tputs(__tscroll(DL, -n), 0, __cputchar);
		else
		       	for(i = n; i < 0; i++)
				tputs(dl, 0, __cputchar);
		__mvcur(bot + n + 1, 0, top, 0, 1);

		/* Scroll the block down */
		if (AL) 
			tputs(__tscroll(AL, -n), 0, __cputchar);
		else
			for(i = n; i < 0; i++)
				tputs(al, 0, __cputchar);
		__mvcur(top, 0, oy, ox, 1);
	}		
}
