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
static char sccsid[] = "@(#)insch.c	8.1 (Berkeley) 6/4/93";
#endif	/* not lint */

#include <curses.h>
#include <string.h>

/*
 * winsch --
 *	Do an insert-char on the line, leaving (cury, curx) unchanged.
 */
int
winsch(register WINDOW *win, char ch)
{

	register __LDATA *end, *temp1, *temp2;

	end = &win->lines[win->cury]->line[win->curx];
	temp1 = &win->lines[win->cury]->line[win->maxx - 1];
	temp2 = temp1 - 1;
	while (temp1 > end) {
		(void)memcpy(temp1, temp2, sizeof(__LDATA));
		temp1--, temp2--;
	}
	temp1->ch = ch;
	temp1->attr &= ~__STANDOUT;
	__touchline(win, win->cury, win->curx, win->maxx - 1, 0);
	if (win->cury == LINES - 1 && 
	    (win->lines[LINES - 1]->line[COLS - 1].ch != ' ' ||
	    win->lines[LINES -1]->line[COLS - 1].attr != 0))
		if (win->flags & __SCROLLOK) {
			wrefresh(win);
			scroll(win);
			win->cury--;

		} else
			return (ERR);
	return (OK);
}
