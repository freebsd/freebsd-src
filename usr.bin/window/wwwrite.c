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
static char sccsid[] = "@(#)wwwrite.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "ww.h"
#include "tt.h"
#include "char.h"

#define UPDATE() \
	if (!w->ww_noupdate && w->ww_cur.r >= 0 && w->ww_cur.r < wwnrow && \
	    wwtouched[w->ww_cur.r]) \
		wwupdate1(w->ww_cur.r, w->ww_cur.r + 1)

/*
 * To support control character expansion, we save the old
 * p and q values in r and s, and point p at the beginning
 * of the expanded string, and q at some safe place beyond it
 * (p + 10).  At strategic points in the loops, we check
 * for (r && !*p) and restore the saved values back into
 * p and q.  Essentially, we implement a stack of depth 2,
 * to avoid recursion, which might be a better idea.
 */
wwwrite(w, p, n)
register struct ww *w;
register char *p;
int n;
{
	char hascursor;
	char *savep = p;
	char *q = p + n;
	char *r = 0;
	char *s;

#ifdef lint
	s = 0;			/* define it before possible use */
#endif
	if (hascursor = w->ww_hascursor)
		wwcursor(w, 0);
	while (p < q && !w->ww_stopped && (!wwinterrupt() || w->ww_nointr)) {
		if (r && !*p) {
			p = r;
			q = s;
			r = 0;
			continue;
		}
		if (w->ww_wstate == 0 &&
		    (isprt(*p) || w->ww_unctrl && isunctrl(*p))) {
			register i;
			register union ww_char *bp;
			int col, col1;

			if (w->ww_insert) {	/* this is very slow */
				if (*p == '\t') {
					p++;
					w->ww_cur.c += 8 -
						(w->ww_cur.c - w->ww_w.l & 7);
					goto chklf;
				}
				if (!isprt(*p)) {
					r = p + 1;
					s = q;
					p = unctrl(*p);
					q = p + 10;
				}
				wwinschar(w, w->ww_cur.r, w->ww_cur.c,
					*p++, w->ww_modes);
				goto right;
			}

			bp = &w->ww_buf[w->ww_cur.r][w->ww_cur.c];
			i = w->ww_cur.c;
			while (i < w->ww_w.r && p < q)
				if (!*p && r) {
					p = r;
					q = s;
					r = 0;
				} else if (*p == '\t') {
					register tmp = 8 - (i - w->ww_w.l & 7);
					p++;
					i += tmp;
					bp += tmp;
				} else if (isprt(*p)) {
					bp++->c_w = *p++
						| w->ww_modes << WWC_MSHIFT;
					i++;
				} else if (w->ww_unctrl && isunctrl(*p)) {
					r = p + 1;
					s = q;
					p = unctrl(*p);
					q = p + 10;
				} else
					break;
			col = MAX(w->ww_cur.c, w->ww_i.l);
			col1 = MIN(i, w->ww_i.r);
			w->ww_cur.c = i;
			if (w->ww_cur.r >= w->ww_i.t
			    && w->ww_cur.r < w->ww_i.b) {
				register union ww_char *ns = wwns[w->ww_cur.r];
				register char *smap = &wwsmap[w->ww_cur.r][col];
				register char *win = w->ww_win[w->ww_cur.r];
				int nchanged = 0;

				bp = w->ww_buf[w->ww_cur.r];
				for (i = col; i < col1; i++)
					if (*smap++ == w->ww_index) {
						nchanged++;
						ns[i].c_w = bp[i].c_w
							^ win[i] << WWC_MSHIFT;
					}
				if (nchanged > 0)
					wwtouched[w->ww_cur.r] |= WWU_TOUCHED;
			}
		chklf:
			if (w->ww_cur.c >= w->ww_w.r)
				goto crlf;
		} else switch (w->ww_wstate) {
		case 0:
			switch (*p++) {
			case '\n':
				if (w->ww_mapnl)
		crlf:
					w->ww_cur.c = w->ww_w.l;
		lf:
				UPDATE();
				if (++w->ww_cur.r >= w->ww_w.b) {
					w->ww_cur.r = w->ww_w.b - 1;
					if (w->ww_w.b < w->ww_b.b) {
						(void) wwscroll1(w, w->ww_i.t,
							w->ww_i.b, 1, 0);
						w->ww_buf++;
						w->ww_b.t--;
						w->ww_b.b--;
					} else
						wwdelline(w, w->ww_b.t);
				}
				break;
			case '\b':
				if (--w->ww_cur.c < w->ww_w.l) {
					w->ww_cur.c = w->ww_w.r - 1;
					goto up;
				}
				break;
			case '\r':
				w->ww_cur.c = w->ww_w.l;
				break;
			case ctrl('g'):
				ttputc(ctrl('g'));
				break;
			case ctrl('['):
				w->ww_wstate = 1;
				break;
			}
			break;
		case 1:
			w->ww_wstate = 0;
			switch (*p++) {
			case '@':
				w->ww_insert = 1;
				break;
			case 'A':
		up:
				UPDATE();
				if (--w->ww_cur.r < w->ww_w.t) {
					w->ww_cur.r = w->ww_w.t;
					if (w->ww_w.t > w->ww_b.t) {
						(void) wwscroll1(w, w->ww_i.t,
							w->ww_i.b, -1, 0);
						w->ww_buf--;
						w->ww_b.t++;
						w->ww_b.b++;
					} else
						wwinsline(w, w->ww_b.t);
				}
				break;
			case 'B':
				goto lf;
			case 'C':
		right:
				w->ww_cur.c++;
				goto chklf;
			case 'E':
				w->ww_buf -= w->ww_w.t - w->ww_b.t;
				w->ww_b.t = w->ww_w.t;
				w->ww_b.b = w->ww_b.t + w->ww_b.nr;
				w->ww_cur.r = w->ww_w.t;
				w->ww_cur.c = w->ww_w.l;
				wwclreos(w, w->ww_w.t, w->ww_w.l);
				break;
			case 'H':
				UPDATE();
				w->ww_cur.r = w->ww_w.t;
				w->ww_cur.c = w->ww_w.l;
				break;
			case 'J':
				wwclreos(w, w->ww_cur.r, w->ww_cur.c);
				break;
			case 'K':
				wwclreol(w, w->ww_cur.r, w->ww_cur.c);
				break;
			case 'L':
				UPDATE();
				wwinsline(w, w->ww_cur.r);
				break;
			case 'M':
				wwdelline(w, w->ww_cur.r);
				break;
			case 'N':
				wwdelchar(w, w->ww_cur.r, w->ww_cur.c);
				break;
			case 'O':
				w->ww_insert = 0;
				break;
			case 'P':
				wwinschar(w, w->ww_cur.r, w->ww_cur.c, ' ', 0);
				break;
			case 'X':
				wwupdate();
				break;
			case 'Y':
				UPDATE();
				w->ww_wstate = 2;
				break;
			case 'Z':
				wwupdate();
				xxflush(0);
				break;
			case 's':
				w->ww_wstate = 4;
				break;
			case 'r':
				w->ww_wstate = 5;
				break;
			}
			break;
		case 2:
			w->ww_cur.r = w->ww_w.t +
				(unsigned)(*p++ - ' ') % w->ww_w.nr;
			w->ww_wstate = 3;
			break;
		case 3:
			w->ww_cur.c = w->ww_w.l +
				(unsigned)(*p++ - ' ') % w->ww_w.nc;
			w->ww_wstate = 0;
			break;
		case 4:
			w->ww_modes |= *p++ & wwavailmodes;
			w->ww_wstate = 0;
			break;
		case 5:
			w->ww_modes &= ~*p++;
			w->ww_wstate = 0;
			break;
		}
	}
	if (hascursor)
		wwcursor(w, 1);
	wwnwwr++;
	wwnwwra += n;
	n = p - savep;
	wwnwwrc += n;
	return n;
}
