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
static char sccsid[] = "@(#)delwin.c	8.1 (Berkeley) 6/4/93";
#endif	/* not lint */

#include <curses.h>
#include <stdlib.h>

/*
 * delwin --
 *	Delete a window and release it back to the system.
 */
int
delwin(win)
	register WINDOW *win;
{

	register WINDOW *wp, *np;

	if (win->orig == NULL) {
		/*
		 * If we are the original window, delete the space for all
		 * the subwindows, the line space and the window space.
		 */
		free(win->lspace);
		free(win->wspace);
		free(win->lines);
		wp = win->nextp;
		while (wp != win) {
			np = wp->nextp;
			delwin(wp);
			wp = np;
		}
	} else {
		/*
		 * If we are a subwindow, take ourselves out of the list.
		 * NOTE: if we are a subwindow, the minimum list is orig
		 * followed by this subwindow, so there are always at least
		 * two windows in the list.
		 */
		for (wp = win->nextp; wp->nextp != win; wp = wp->nextp)
			continue;
		wp->nextp = win->nextp;
	}
	free(win);
	return (OK);
}
