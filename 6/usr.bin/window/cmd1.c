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
static char sccsid[] = "@(#)cmd1.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "@(#)$FreeBSD$";
#endif /* not lint */

#include "defs.h"
#include "char.h"

c_window()
{
	int col, row, xcol, xrow;
	int id;

	if ((id = findid()) < 0)
		return;
	if (!terse)
		wwputs("New window (upper left corner): ", cmdwin);
	col = 0;
	row = 1;
	wwadd(boxwin, framewin->ww_back);
	for (;;) {
		wwbox(boxwin, row - 1, col - 1, 3, 3);
		wwsetcursor(row, col);
		while (wwpeekc() < 0)
			wwiomux();
		switch (getpos(&row, &col, row > 1, 0,
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
	if (!terse)
		wwputs("\nNew window (lower right corner): ", cmdwin);
	xcol = col;
	xrow = row;
	for (;;) {
		wwbox(boxwin, row - 1, col - 1,
			xrow - row + 3, xcol - col + 3);
		wwsetcursor(xrow, xcol);
		while (wwpeekc() < 0)
			wwiomux();
		switch (getpos(&xrow, &xcol, row, col, wwnrow - 1, wwncol - 1))
		{
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
	(void) openwin(id, row, col, xrow-row+1, xcol-col+1, default_nline,
		(char *) 0, 1, 1, default_shellfile, default_shell);
}

getpos(row, col, minrow, mincol, maxrow, maxcol)
register int *row, *col;
int minrow, mincol;
int maxrow, maxcol;
{
	static int scount;
	int count;
	int c;
	int oldrow = *row, oldcol = *col;

	while ((c = wwgetc()) >= 0) {
		switch (c) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			scount = scount * 10 + c - '0';
			continue;
		}
		count = scount ? scount : 1;
		scount = 0;
		switch (c) {
		case 'h':
			if ((*col -= count) < mincol)
				*col = mincol;
			break;
		case 'H':
			*col = mincol;
			break;
		case 'l':
			if ((*col += count) > maxcol)
				*col = maxcol;
			break;
		case 'L':
			*col = maxcol;
			break;
		case 'j':
			if ((*row += count) > maxrow)
				*row = maxrow;
			break;
		case 'J':
			*row = maxrow;
			break;
		case 'k':
			if ((*row -= count) < minrow)
				*row = minrow;
			break;
		case 'K':
			*row = minrow;
			break;
		case ctrl('['):
			if (!terse)
				wwputs("\nCancelled.  ", cmdwin);
			return 3;
		case '\r':
			return 2;
		default:
			if (!terse)
				wwputs("\nType [hjklHJKL] to move, return to enter position, escape to cancel.", cmdwin);
			wwbell();
		}
	}
	return oldrow != *row || oldcol != *col;
}
