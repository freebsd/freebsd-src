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
static char sccsid[] = "@(#)cmd7.c	3.7 (Berkeley) 6/6/90";
#endif /* not lint */

#include "defs.h"

/*
 * Window size.
 */

c_size(w)
register struct ww *w;
{
	int col, row;

	if (!terse)
		wwputs("New window size (lower right corner): ", cmdwin);
	col = MIN(w->ww_w.r, wwncol) - 1;
	row = MIN(w->ww_w.b, wwnrow) - 1;
	wwadd(boxwin, framewin->ww_back);
	for (;;) {
		wwbox(boxwin, w->ww_w.t - 1, w->ww_w.l - 1,
			row - w->ww_w.t + 3, col - w->ww_w.l + 3);
		wwsetcursor(row, col);
		while (wwpeekc() < 0)
			wwiomux();
		switch (getpos(&row, &col, w->ww_w.t, w->ww_w.l,
			wwnrow - 1, wwncol - 1)) {
		case 3:
			wwunbox(boxwin);
			wwdelete(boxwin);
			return;
		case 2:
			wwunbox(boxwin);
			break;
		case 1:
			wwunbox(boxwin);
		case 0:
			continue;
		}
		break;
	}
	wwdelete(boxwin);
	if (!terse)
		wwputc('\n', cmdwin);
	wwcurtowin(cmdwin);
	sizewin(w, row - w->ww_w.t + 1, col - w->ww_w.l + 1);
}
