/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
static char sccsid[] = "@(#)wwscroll.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

wwscroll(w, n)
register struct ww *w;
int n;
{
	register dir;
	register top;

	if (n == 0)
		return;
	dir = n < 0 ? -1 : 1;
	top = w->ww_b.t - n;
	if (top > w->ww_w.t)
		top = w->ww_w.t;
	else if (top + w->ww_b.nr < w->ww_w.b)
		top = w->ww_w.b - w->ww_b.nr;
	n = abs(top - w->ww_b.t);
	if (n < w->ww_i.nr) {
		while (--n >= 0) {
			(void) wwscroll1(w, w->ww_i.t, w->ww_i.b, dir, 0);
			w->ww_buf += dir;
			w->ww_b.t -= dir;
			w->ww_b.b -= dir;
		}
	} else {
		w->ww_buf -= top - w->ww_b.t;
		w->ww_b.t = top;
		w->ww_b.b = top + w->ww_b.nr;
		wwredrawwin(w);
	}
}

/*
 * Scroll one line, between 'row1' and 'row2', in direction 'dir'.
 * Don't adjust ww_scroll.
 * And don't redraw 'leaveit' lines.
 */
wwscroll1(w, row1, row2, dir, leaveit)
register struct ww *w;
int row1, row2, dir;
int leaveit;
{
	register i;
	int row1x, row2x;
	int nvis;
	int nvismax;
	int scrolled = 0;

	/*
	 * See how many lines on the screen are affected.
	 * And calculate row1x, row2x, and left at the same time.
	 */
	for (i = row1; i < row2 && w->ww_nvis[i] == 0; i++)
		;
	if (i >= row2)			/* can't do any fancy stuff */
		goto out;
	row1x = i;
	for (i = row2 - 1; i >= row1 && w->ww_nvis[i] == 0; i--)
		;
	if (i <= row1x)
		goto out;		/* just one line is easy */
	row2x = i + 1;

	/*
	 * See how much of this window is visible.
	 */
	nvismax = wwncol * (row2x - row1x);
	nvis = 0;
	for (i = row1x; i < row2x; i++)
		nvis += w->ww_nvis[i];

	/*
	 * If it's a good idea to scroll and the terminal can, then do it.
	 */
	if (nvis < nvismax / 2)
		goto no_scroll;		/* not worth it */
	if ((dir > 0 ? tt.tt_scroll_down == 0 : tt.tt_scroll_up == 0) ||
	    (tt.tt_scroll_top != row1x || tt.tt_scroll_bot != row2x - 1) &&
	    tt.tt_setscroll == 0)
		if (tt.tt_delline == 0 || tt.tt_insline == 0)
			goto no_scroll;
	xxscroll(dir, row1x, row2x);
	scrolled = 1;
	/*
	 * Fix up the old screen.
	 */
	{
		register union ww_char *tmp;
		register union ww_char **cpp, **cqq;

		if (dir > 0) {
			cpp = &wwos[row1x];
			cqq = cpp + 1;
			tmp = *cpp;
			for (i = row2x - row1x; --i > 0;)
				*cpp++ = *cqq++;
			*cpp = tmp;
		} else {
			cpp = &wwos[row2x];
			cqq = cpp - 1;
			tmp = *cqq;
			for (i = row2x - row1x; --i > 0;)
				*--cpp = *--cqq;
			*cqq = tmp;
		}
		for (i = wwncol; --i >= 0;)
			tmp++->c_w = ' ';
	}

no_scroll:
	/*
	 * Fix the new screen.
	 */
	if (nvis == nvismax) {
		/*
		 * Can shift whole lines.
		 */
		if (dir > 0) {
			{
				register union ww_char *tmp;
				register union ww_char **cpp, **cqq;

				cpp = &wwns[row1x];
				cqq = cpp + 1;
				tmp = *cpp;
				for (i = row2x - row1x; --i > 0;)
					*cpp++ = *cqq++;
				*cpp = tmp;
			}
			if (scrolled) {
				register char *p, *q;

				p = &wwtouched[row1x];
				q = p + 1;
				for (i = row2x - row1x; --i > 0;)
					*p++ = *q++;
				*p |= WWU_TOUCHED;
			} else {
				register char *p;

				p = &wwtouched[row1x];
				for (i = row2x - row1x; --i >= 0;)
					*p++ |= WWU_TOUCHED;
			}
			wwredrawwin1(w, row1, row1x, dir);
			wwredrawwin1(w, row2x - 1, row2 - leaveit, dir);
		} else {
			{
				register union ww_char *tmp;
				register union ww_char **cpp, **cqq;

				cpp = &wwns[row2x];
				cqq = cpp - 1;
				tmp = *cqq;
				for (i = row2x - row1x; --i > 0;)
					*--cpp = *--cqq;
				*cqq = tmp;
			}
			if (scrolled) {
				register char *p, *q;

				p = &wwtouched[row2x];
				q = p - 1;
				for (i = row2x - row1x; --i > 0;)
					*--p = *--q;
				*q |= WWU_TOUCHED;
			} else {
				register char *p;

				p = &wwtouched[row1x];
				for (i = row2x - row1x; --i >= 0;)
					*p++ |= WWU_TOUCHED;
			}
			wwredrawwin1(w, row1 + leaveit, row1x + 1, dir);
			wwredrawwin1(w, row2x, row2, dir);
		}
	} else {
		if (scrolled) {
			register char *p;

			p = &wwtouched[row1x];
			for (i = row2x - row1x; --i >= 0;)
				*p++ |= WWU_TOUCHED;
		}
out:
		if (dir > 0)
			wwredrawwin1(w, row1, row2 - leaveit, dir);
		else
			wwredrawwin1(w, row1 + leaveit, row2, dir);
	}
	return scrolled;
}
