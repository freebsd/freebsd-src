/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)wwdelchar.c	3.16 (Berkeley) 6/6/90";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

wwdelchar(w, row, col)
register struct ww *w;
{
	register i;
	int nvis;

	/*
	 * First, shift the line.
	 */
	{
		register union ww_char *p, *q;

		p = &w->ww_buf[row][col];
		q = p + 1;
		for (i = w->ww_b.r - col; --i > 0;)
			*p++ = *q++;
		p->c_w = ' ';
	}

	/*
	 * If can't see it, just return.
	 */
	if (row < w->ww_i.t || row >= w->ww_i.b
	    || w->ww_i.r <= 0 || w->ww_i.r <= col)
		return;

	if (col < w->ww_i.l)
		col = w->ww_i.l;

	/*
	 * Now find out how much is actually changed, and fix wwns.
	 */
	{
		register union ww_char *buf;
		register char *win;
		register union ww_char *ns;
		register char *smap;
		char touched;

		nvis = 0;
		smap = &wwsmap[row][col];
		for (i = col; i < w->ww_i.r && *smap++ != w->ww_index; i++)
			;
		if (i >= w->ww_i.r)
			return;
		col = i;
		buf = w->ww_buf[row];
		win = w->ww_win[row];
		ns = wwns[row];
		smap = &wwsmap[row][i];
		touched = wwtouched[row];
		for (; i < w->ww_i.r; i++) {
			if (*smap++ != w->ww_index)
				continue;
			touched |= WWU_TOUCHED;
			if (win[i])
				ns[i].c_w =
					buf[i].c_w ^ win[i] << WWC_MSHIFT;
			else {
				nvis++;
				ns[i] = buf[i];
			}
		}
		wwtouched[row] = touched;
	}

	/*
	 * Can/Should we use delete character?
	 */
	if (tt.tt_delchar != 0 && nvis > (wwncol - col) / 2) {
		register union ww_char *p, *q;

		xxdelchar(row, col);
		p = &wwos[row][col];
		q = p + 1;
		for (i = wwncol - col; --i > 0;)
			*p++ = *q++;
		p->c_w = ' ';
	}
}
