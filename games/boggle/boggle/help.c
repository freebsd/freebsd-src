/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Barry Brachman.
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
static char sccsid[] = "@(#)help.c	8.1 (Berkeley) 6/11/93";
#endif /* not lint */

#include <curses.h>
#include <stdio.h>

#include "bog.h"
#include "extern.h"

int
help()
{
	extern int nlines;
	int eof, i;
	FILE *fp;
	WINDOW *win;
	char buf[BUFSIZ];

	if ((fp = fopen(HELPFILE, "r")) == NULL)
		return(-1);
	win = newwin(0, 0, 0, 0);
	clearok(win, 1);

	eof = 0;
	if (ungetc(getc(fp), fp) == EOF) {
		wprintw(win, "There doesn't seem to be any help.");
		eof = 1;			/* Nothing there... */
	}

	while (!eof) {
		for (i = 0; i < nlines - 3; i++) {
			if (fgets(buf, sizeof(buf), fp) == (char *) NULL) {
				eof = 1;
				break;
			}
			if (buf[0] == '.' && buf[1] == '\n')
				break;
			wprintw(win, "%s", buf);
		}
		if (eof || ungetc(getc(fp), fp) == EOF) {
			eof = 1;
			break;
		}
		wmove(win, nlines - 1, 0);
		wprintw(win,
		    "Type <space> to continue, anything else to quit...");
		wrefresh(win);
		if ((inputch() & 0177) != ' ')
			break;
		wclear(win);
	}

	fclose(fp);
	if (eof) {
		wmove(win, nlines - 1, 0);
		wprintw(win, "Hit any key to continue...");
		wrefresh(win);
		inputch();
	}
	delwin(win);
	clearok(stdscr, 1);
	refresh();
	return(0);
}
