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
static char sccsid[] = "@(#)overlay.c	8.1 (Berkeley) 6/4/93";
#endif	/* not lint */

#include <ctype.h>
#include <curses.h>

/*
 * overlay --
 *	Writes win1 on win2 non-destructively.
 */
int
overlay(win1, win2)
	register WINDOW *win1, *win2;
{

	register int x, y, y1, y2, endy, endx, starty, startx;
	register __LDATA *sp, *end;

#ifdef DEBUG
	__CTRACE("overlay: (%0.2o, %0.2o);\n", win1, win2);
#endif
	starty = max(win1->begy, win2->begy);
	startx = max(win1->begx, win2->begx);
	endy = min(win1->maxy + win1->begy, win2->maxy + win2->begx);
	endx = min(win1->maxx + win1->begx, win2->maxx + win2->begx);
#ifdef DEBUG
	__CTRACE("overlay: from (%d,%d) to (%d,%d)\n",
	    starty, startx, endy, endx);
#endif
	if (starty >= endy || startx >= endx)
		return (OK);
	y1 = starty - win1->begy;
	y2 = starty - win2->begy;
	for (y = starty; y < endy; y++, y1++, y2++) {
		end = &win1->lines[y1]->line[endx - win1->begx];
		x = startx - win2->begx;
		for (sp = &win1->lines[y1]->line[startx - win1->begx]; 
		     sp < end; sp++) {
			if (!isspace(sp->ch)) {
				wmove(win2, y2, x);
				__waddch(win2, sp);
			}
			x++;
		}
	}
	return (OK);
}
