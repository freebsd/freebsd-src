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
static char sccsid[] = "@(#)wwupdate.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

wwupdate1(top, bot)
{
	int i;
	register j;
	char *touched;
	struct ww_update *upd;
	char check_clreos = 0;
	int scan_top, scan_bot;

	wwnupdate++;
	{
		register char *t1 = wwtouched + top, *t2 = wwtouched + bot;
		register n;

		while (!*t1++)
			if (t1 == t2)
				return;
		while (!*--t2)
			;
		scan_top = top = t1 - wwtouched - 1;
		scan_bot = bot = t2 - wwtouched + 1;
		if (scan_bot - scan_top > 1 &&
		    (tt.tt_clreos != 0 || tt.tt_clear != 0)) {
			int st = tt.tt_clreos != 0 ? scan_top : 0;

			/*
			 * t1 is one past the first touched row,
			 * t2 is on the last touched row.
			 */
			for (t1--, n = 1; t1 < t2;)
				if (*t1++)
					n++;
			/*
			 * If we can't clreos then we try for clearing
			 * the whole screen.
			 */
			if (check_clreos = n * 10 > (wwnrow - st) * 9) {
				scan_top = st;
				scan_bot = wwnrow;
			}
		}
	}
	if (tt.tt_clreol == 0 && !check_clreos)
		goto simple;
	for (i = scan_top, touched = &wwtouched[i], upd = &wwupd[i];
	     i < scan_bot;
	     i++, touched++, upd++) {
		register gain = 0;
		register best_gain = 0;
		register best_col;
		register union ww_char *ns, *os;

		if (wwinterrupt())
			return;
		if (!check_clreos && !*touched)
			continue;
		wwnupdscan++;
		j = wwncol;
		ns = &wwns[i][j];
		os = &wwos[i][j];
		while (--j >= 0) {
			/*
			 * The cost of clearing is:
			 *	ncol - nblank + X
			 * The cost of straight update is, more or less:
			 *	ncol - nsame
			 * We clear if  nblank - nsame > X
			 * X is the clreol overhead.
			 * So we make gain = nblank - nsame.
			 */
			if ((--ns)->c_w == (--os)->c_w)
				gain--;
			else
				best_gain--;
			if (ns->c_w == ' ')
				gain++;
			if (gain > best_gain) {
				best_col = j;
				best_gain = gain;
			}
		}
		upd->best_gain = best_gain;
		upd->best_col = best_col;
		upd->gain = gain;
	}
	if (check_clreos) {
		register struct ww_update *u;
		register gain = 0;
		register best_gain = 0;
		int best_row;
		register simple_gain = 0;
		char didit = 0;

		/*
		 * gain is the advantage of clearing all the lines.
		 * best_gain is the advantage of clearing to eos
		 * at best_row and u->best_col.
		 * simple_gain is the advantage of using only clreol.
		 * We use g > best_gain because u->best_col can be
		 * undefined when u->best_gain is 0 so we can't use it.
		 */
		for (j = scan_bot - 1, u = wwupd + j; j >= top; j--, u--) {
			register g = gain + u->best_gain;

			if (g > best_gain) {
				best_gain = g;
				best_row = j;
			}
			gain += u->gain;
			if (tt.tt_clreol != 0 && u->best_gain > 4)
				simple_gain += u->best_gain - 4;
		}
		if (tt.tt_clreos == 0) {
			if (gain > simple_gain && gain > 4) {
				xxclear();
				i = top = scan_top;
				bot = scan_bot;
				j = 0;
				didit = 1;
			}
		} else
			if (best_gain > simple_gain && best_gain > 4) {
				i = best_row;
				xxclreos(i, j = wwupd[i].best_col);
				bot = scan_bot;
				didit = 1;
			}
		if (didit) {
			wwnupdclreos++;
			wwnupdclreosline += wwnrow - i;
			u = wwupd + i;
			while (i < scan_bot) {
				register union ww_char *os = &wwos[i][j];

				for (j = wwncol - j; --j >= 0;)
					os++->c_w = ' ';
				wwtouched[i++] |= WWU_TOUCHED;
				u++->best_gain = 0;
				j = 0;
			}
		} else
			wwnupdclreosmiss++;
	}
simple:
	for (i = top, touched = &wwtouched[i], upd = &wwupd[i]; i < bot;
	     i++, touched++, upd++) {
		register union ww_char *os, *ns;
		char didit;

		if (!*touched)
			continue;
		*touched = 0;
		wwnupdline++;
		didit = 0;
		if (tt.tt_clreol != 0 && upd->best_gain > 4) {
			wwnupdclreol++;
			xxclreol(i, j = upd->best_col);
			for (os = &wwos[i][j], j = wwncol - j; --j >= 0;)
				os++->c_w = ' ';
			didit = 1;
		}
		ns = wwns[i];
		os = wwos[i];
		for (j = 0; j < wwncol;) {
			register char *p, *q;
			char m;
			int c;
			register n;
			char buf[512];			/* > wwncol */
			union ww_char lastc;

			for (; j++ < wwncol && ns++->c_w == os++->c_w;)
				;
			if (j > wwncol)
				break;
			p = buf;
			m = ns[-1].c_m;
			c = j - 1;
			os[-1] = ns[-1];
			*p++ = ns[-1].c_c;
			n = 5;
			q = p;
			while (j < wwncol && ns->c_m == m) {
				*p++ = ns->c_c;
				if (ns->c_w == os->c_w) {
					if (--n <= 0)
						break;
					os++;
					ns++;
				} else {
					n = 5;
					q = p;
					lastc = *os;
					*os++ = *ns++;
				}
				j++;
			}
			n = q - buf;
			if (!wwwrap || i != wwnrow - 1 || c + n != wwncol)
				xxwrite(i, c, buf, n, m);
			else if (tt.tt_inschar || tt.tt_insspace) {
				if (n > 1) {
					q[-2] = q[-1];
					n--;
				} else
					c--;
				xxwrite(i, c, buf, n, m);
				c += n - 1;
				if (tt.tt_inschar)
					xxinschar(i, c, ns[-2].c_c,
						ns[-2].c_m);
				else {
					xxinsspace(i, c);
					xxwrite(i, c, &ns[-2].c_c, 1,
						ns[-2].c_m);
				}
			} else {
				if (--n)
					xxwrite(i, c, buf, n, m);
				os[-1] = lastc;
				*touched = WWU_TOUCHED;
			}
			didit = 1;
		}
		if (!didit)
			wwnupdmiss++;
	}
}
