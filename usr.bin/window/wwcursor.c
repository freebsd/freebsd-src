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
static char sccsid[] = "@(#)wwcursor.c	3.12 (Berkeley) 6/6/90";
#endif /* not lint */

#include "ww.h"

wwcursor(w, on)
register struct ww *w;
{
	register char *win;

	if (on) {
		if (w->ww_hascursor)
			return;
		w->ww_hascursor = 1;
	} else {
		if (!w->ww_hascursor)
			return;
		w->ww_hascursor = 0;
	}
	if (wwcursormodes != 0) {
		win = &w->ww_win[w->ww_cur.r][w->ww_cur.c];
		*win ^= wwcursormodes;
		if (w->ww_cur.r < w->ww_i.t || w->ww_cur.r >= w->ww_i.b
		    || w->ww_cur.c < w->ww_i.l || w->ww_cur.c >= w->ww_i.r)
			return;
		if (wwsmap[w->ww_cur.r][w->ww_cur.c] == w->ww_index) {
			if (*win == 0)
				w->ww_nvis[w->ww_cur.r]++;
			else if (*win == wwcursormodes)
				w->ww_nvis[w->ww_cur.r]--;
			wwns[w->ww_cur.r][w->ww_cur.c].c_m ^= wwcursormodes;
			wwtouched[w->ww_cur.r] |= WWU_TOUCHED;
		}
	}
}

wwsetcursormodes(new)
register new;
{
	register i;
	register struct ww *w;
	register old = wwcursormodes;

	new &= wwavailmodes;
	if (new == wwcursormodes)
		return;
	for (i = 0; i < NWW; i++)
		if (wwindex[i] != 0 && (w = wwindex[i])->ww_hascursor) {
			wwcursor(w, 0);
			wwcursormodes = new;
			wwcursor(w, 1);
			wwcursormodes = old;
		}
	wwcursormodes = new;
}
