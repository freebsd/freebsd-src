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
static char sccsid[] = "@(#)wwdelete.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "ww.h"

/*
 * Pull w free from the cover list.
 */
wwdelete(w)
register struct ww *w;
{
	register i;

	for (i = w->ww_i.t; i < w->ww_i.b; i++) {
		register j;
		register char *smap = wwsmap[i];
		register union ww_char *ns = wwns[i];
		register int nchanged = 0;

		for (j = w->ww_i.l; j < w->ww_i.r; j++)
			if (smap[j] == w->ww_index) {
				smap[j] = WWX_NOBODY;
				ns[j].c_w = ' ';
				nchanged++;
			}
		if (nchanged > 0)
			wwtouched[i] |= WWU_TOUCHED;
	}

	{
		register struct ww *wp;

		for (wp = w->ww_forw; wp != &wwhead; wp = wp->ww_forw)
			wp->ww_order--;
	}

	if (w->ww_forw != &wwhead)
		wwdelete1(w->ww_forw,
			w->ww_i.t, w->ww_i.b, w->ww_i.l, w->ww_i.r);

	w->ww_back->ww_forw = w->ww_forw;
	w->ww_forw->ww_back = w->ww_back;
	w->ww_forw = w->ww_back = 0;
}

wwdelete1(w, t, b, l, r)
register struct ww *w;
{
	int i;
	int tt, bb, ll, rr;
	char hasglass;

again:
	hasglass = 0;
	tt = MAX(t, w->ww_i.t);
	bb = MIN(b, w->ww_i.b);
	ll = MAX(l, w->ww_i.l);
	rr = MIN(r, w->ww_i.r);
	if (tt >= bb || ll >= rr) {
		if ((w = w->ww_forw) == &wwhead)
			return;
		goto again;
	}
	for (i = tt; i < bb; i++) {
		register j;
		register char *smap = wwsmap[i];
		register union ww_char *ns = wwns[i];
		register char *win = w->ww_win[i];
		register union ww_char *buf = w->ww_buf[i];
		int nvis = w->ww_nvis[i];
		int nchanged = 0;

		for (j = ll; j < rr; j++) {
			if (smap[j] != WWX_NOBODY)
				continue;
			if (win[j] & WWM_GLS) {
				hasglass = 1;
				continue;
			}
			smap[j] = w->ww_index;
			ns[j].c_w = buf[j].c_w ^ win[j] << WWC_MSHIFT;
			nchanged++;
			if (win[j] == 0)
				nvis++;
		}
		if (nchanged > 0)
			wwtouched[i] |= WWU_TOUCHED;
		w->ww_nvis[i] = nvis;
	}
	if ((w = w->ww_forw) == &wwhead)
		return;
	if (hasglass)
		goto again;
	if (tt > t)
		wwdelete1(w, t, tt, l, r);
	if (bb < b)
		wwdelete1(w, bb, b, l, r);
	if (ll > l)
		wwdelete1(w, tt, bb, l, ll);
	if (rr < r)
		wwdelete1(w, tt, bb, rr, r);
}
