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
static char sccsid[] = "@(#)cr_put.c	8.2 (Berkeley) 1/9/94";
#endif	/* not lint */

#include <curses.h>
#include <string.h>

#define	HARDTABS	8

/*
 * Terminal driving and line formatting routines.  Basic motion optimizations
 * are done here as well as formatting lines (printing of control characters,
 * line numbering and the like).
 */

/* Stub function for the users. */
int
mvcur(ly, lx, y, x)
	int ly, lx, y, x;
{
	return (__mvcur(ly, lx, y, x, 0));
}

static void	fgoto __P((int));
static int	plod __P((int, int));
static void     plodput __P((int));
static int	tabcol __P((int, int));

static int outcol, outline, destcol, destline;

/*
 * Sync the position of the output cursor.  Most work here is rounding for
 * terminal boundaries getting the column position implied by wraparound or
 * the lack thereof and rolling up the screen to get destline on the screen.
 */
int
__mvcur(ly, lx, y, x, in_refresh)
	int ly, lx, y, x, in_refresh;
{
#ifdef DEBUG
	__CTRACE("mvcur: moving cursor from (%d, %d) to (%d, %d)\n",
	    ly, lx, y, x);
#endif
	destcol = x;
	destline = y;
	outcol = lx;
	outline = ly;
	fgoto(in_refresh);
	return (OK);
}	
        
static void
fgoto(in_refresh)
	int in_refresh;
{
	register int c, l;
	register char *cgp;

	if (destcol >= COLS) {
		destline += destcol / COLS;
		destcol %= COLS;
	}
	if (outcol >= COLS) {
		l = (outcol + 1) / COLS;
		outline += l;
		outcol %= COLS;
		if (AM == 0) {
			while (l > 0) {
				if (__pfast)
					if (CR)
						tputs(CR, 0, __cputchar);
					else
						putchar('\r');
				if (NL)
					tputs(NL, 0, __cputchar);
				else
					putchar('\n');
				l--;
			}
			outcol = 0;
		}
		if (outline > LINES - 1) {
			destline -= outline - (LINES - 1);
			outline = LINES - 1;
		}
	}
	if (destline >= LINES) {
		l = destline;
		destline = LINES - 1;
		if (outline < LINES - 1) {
			c = destcol;
			if (__pfast == 0 && !CA)
				destcol = 0;
			fgoto(in_refresh);
			destcol = c;
		}
		while (l >= LINES) {
			/* The following linefeed (or simulation thereof) is
			 * supposed to scroll up the screen, since we are on
			 * the bottom line.  We make the assumption that
			 * linefeed will scroll.  If ns is in the capability
			 * list this won't work.  We should probably have an
			 * sc capability but sf will generally take the place
			 * if it works.
			 * 
			 * Superbee glitch: in the middle of the screen have
			 * to use esc B (down) because linefeed screws up in
			 * "Efficient Paging" (what a joke) mode (which is
			 * essential in some SB's because CRLF mode puts
			 * garbage in at end of memory), but you must use
			 * linefeed to scroll since down arrow won't go past
			 * memory end. I turned this off after recieving Paul
			 * Eggert's Superbee description which wins better.
			 */
			if (NL /* && !XB */ && __pfast)
				tputs(NL, 0, __cputchar);
			else
				putchar('\n');
			l--;
			if (__pfast == 0)
				outcol = 0;
		}
	}
	if (destline < outline && !(CA || UP))
		destline = outline;
	if (CA) {
		cgp = tgoto(CM, destcol, destline);

		/*
		 * Need this condition due to inconsistent behavior
		 * of backspace on the last column.
		 */
		if (outcol != COLS - 1 && plod(strlen(cgp), in_refresh) > 0)
			plod(0, in_refresh);
		else 
			tputs(cgp, 0, __cputchar);
	} else
		plod(0, in_refresh);
	outline = destline;
	outcol = destcol;
}
/*
 * Move (slowly) to destination.
 * Hard thing here is using home cursor on really deficient terminals.
 * Otherwise just use cursor motions, hacking use of tabs and overtabbing
 * and backspace.
 */

static int plodcnt, plodflg;

static void
plodput(c)
	int c;
{
	if (plodflg)
		--plodcnt;
	else
		putchar(c);
}

static int
plod(cnt, in_refresh)
	int cnt, in_refresh;
{
	register int i, j, k, soutcol, soutline;

	plodcnt = plodflg = cnt;
	soutcol = outcol;
	soutline = outline;
	/*
	 * Consider homing and moving down/right from there, vs. moving
	 * directly with local motions to the right spot.
	 */
	if (HO) {
		/*
		 * i is the cost to home and tab/space to the right to get to
		 * the proper column.  This assumes ND space costs 1 char.  So
		 * i + destcol is cost of motion with home.
		 */
		if (GT)
			i = (destcol / HARDTABS) + (destcol % HARDTABS);
		else
			i = destcol;

		/* j is cost to move locally without homing. */
		if (destcol >= outcol) {	/* if motion is to the right */
			j = destcol / HARDTABS - outcol / HARDTABS;
			if (GT && j)
				j += destcol % HARDTABS;
			else
				j = destcol - outcol;
		} else
			/* leftward motion only works if we can backspace. */
			if (outcol - destcol <= i && (BS || BC))
				/* Cheaper to backspace. */
				i = j = outcol - destcol;
			else
				/* Impossibly expensive. */
				j = i + 1;

		/* k is the absolute value of vertical distance. */
		k = outline - destline;
		if (k < 0)
			k = -k;
		j += k;

		/* Decision.  We may not have a choice if no UP. */
		if (i + destline < j || (!UP && destline < outline)) {
			/*
			 * Cheaper to home.  Do it now and pretend it's a
			 * regular local motion.
			 */
			tputs(HO, 0, plodput);
			outcol = outline = 0;
		} else if (LL) {
			/*
			 * Quickly consider homing down and moving from there.
			 * Assume cost of LL is 2.
			 */
			k = (LINES - 1) - destline;
			if (i + k + 2 < j && (k <= 0 || UP)) {
				tputs(LL, 0, plodput);
				outcol = 0;
				outline = LINES - 1;
			}
		}
	} else
		/* No home and no up means it's impossible. */
		if (!UP && destline < outline)
			return (-1);
	if (GT)
		i = destcol % HARDTABS + destcol / HARDTABS;
	else
		i = destcol;
#ifdef notdef
	if (BT && outcol > destcol &&
	    (j = (((outcol+7) & ~7) - destcol - 1) >> 3)) {
		j *= (k = strlen(BT));
		if ((k += (destcol&7)) > 4)
			j += 8 - (destcol&7);
		else
			j += k;
	}
	else
#endif
		j = outcol - destcol;

	/*
	 * If we will later need a \n which will turn into a \r\n by the
	 * system or the terminal, then don't bother to try to \r.
	 */
	if ((NONL || !__pfast) && outline < destline)
		goto dontcr;

	/*
	 * If the terminal will do a \r\n and there isn't room for it, then
	 * we can't afford a \r.
	 */
	if (NC && outline >= destline)
		goto dontcr;

	/*
	 * If it will be cheaper, or if we can't back up, then send a return
	 * preliminarily.
	 */
	if (j > i + 1 || outcol > destcol && !BS && !BC) {
		/*
		 * BUG: this doesn't take the (possibly long) length of CR
		 * into account.
		 */
		if (CR)
			tputs(CR, 0, plodput);
		else
			plodput('\r');
		if (NC) {
			if (NL)
				tputs(NL, 0, plodput);
			else
				plodput('\n');
			outline++;
		}
		outcol = 0;
	}

dontcr:	while (outline < destline) {
		outline++;
		if (NL)
			tputs(NL, 0, plodput);
		else
			plodput('\n');
		if (plodcnt < 0)
			goto out;
		if (NONL || __pfast == 0)
			outcol = 0;
	}
	if (BT)
		k = strlen(BT);
	while (outcol > destcol) {
		if (plodcnt < 0)
			goto out;
#ifdef notdef
		if (BT && outcol - destcol > k + 4) {
			tputs(BT, 0, plodput);
			outcol--;
			outcol &= ~7;
			continue;
		}
#endif
		outcol--;
		if (BC)
			tputs(BC, 0, plodput);
		else
			plodput('\b');
	}
	while (outline > destline) {
		outline--;
		tputs(UP, 0, plodput);
		if (plodcnt < 0)
			goto out;
	}
	if (GT && destcol - outcol > 1) {
		for (;;) {
			i = tabcol(outcol, HARDTABS);
			if (i > destcol)
				break;
			if (TA)
				tputs(TA, 0, plodput);
			else
				plodput('\t');
			outcol = i;
		}
		if (destcol - outcol > 4 && i < COLS && (BC || BS)) {
			if (TA)
				tputs(TA, 0, plodput);
			else
				plodput('\t');
			outcol = i;
			while (outcol > destcol) {
				outcol--;
				if (BC)
					tputs(BC, 0, plodput);
				else
					plodput('\b');
			}
		}
	}
	while (outcol < destcol) {
		/*
		 * Move one char to the right.  We don't use ND space because
		 * it's better to just print the char we are moving over.
		 */
		if (in_refresh)
			if (plodflg)	/* Avoid a complex calculation. */
				plodcnt--;
			else {
				i = curscr->lines[outline]->line[outcol].ch;
				if ((curscr->lines[outline]->line[outcol].attr
				     & __STANDOUT) ==
				    (curscr->flags & __WSTANDOUT))
					putchar(i);
				else
					goto nondes;
			}
		else
nondes:			if (ND)
				tputs(ND, 0, plodput);
			else
				plodput(' ');
		outcol++;
		if (plodcnt < 0)
			goto out;
	}

out:	if (plodflg) {
		outcol = soutcol;
		outline = soutline;
	}
	return (plodcnt);
}

/*
 * Return the column number that results from being in column col and
 * hitting a tab, where tabs are set every ts columns.  Work right for
 * the case where col > COLS, even if ts does not divide COLS.
 */
static int
tabcol(col, ts)
	int col, ts;
{
	int offset;

	if (col >= COLS) {
		offset = COLS * (col / COLS);
		col -= offset;
	} else
		offset = 0;
	return (col + ts - (col % ts) + offset);
}
