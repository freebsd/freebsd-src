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
static char sccsid[] = "@(#)cmd.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "defs.h"
#include "char.h"

docmd()
{
	register char c;
	register struct ww *w;
	char out = 0;

	while (!out && !quit) {
		if ((c = wwgetc()) < 0) {
			if (terse)
				wwsetcursor(0, 0);
			else {
				wwputs("Command: ", cmdwin);
				wwcurtowin(cmdwin);
			}
			do
				wwiomux();
			while ((c = wwgetc()) < 0);
		}
		if (!terse)
			wwputc('\n', cmdwin);
		switch (c) {
		default:
			if (c != escapec)
				break;
		case 'h': case 'j': case 'k': case 'l':
		case 'y': case 'p':
		case ctrl('y'):
		case ctrl('e'):
		case ctrl('u'):
		case ctrl('d'):
		case ctrl('b'):
		case ctrl('f'):
		case ctrl('s'):
		case ctrl('q'):
		case ctrl('['):
			if (selwin == 0) {
				error("No window.");
				continue;
			}
		}
		switch (c) {
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			if ((w = window[c - '1']) == 0) {
				error("%c: No such window.", c);
				break;
			}
			setselwin(w);
			if (checkproc(selwin) >= 0)
				 out = 1;
			break;
		case '%':
			if ((w = getwin()) != 0)
				setselwin(w);
			break;
		case ctrl('^'):
			if (lastselwin != 0) {
				setselwin(lastselwin);
				if (checkproc(selwin) >= 0)
					out = 1;
			} else
				error("No previous window.");
			break;
		case 'c':
			if ((w = getwin()) != 0)
				closewin(w);
			break;
		case 'w':
			c_window();
			break;
		case 'm':
			if ((w = getwin()) != 0)
				c_move(w);
			break;
		case 'M':
			if ((w = getwin()) != 0)
				movewin(w, w->ww_alt.t, w->ww_alt.l);
			break;
		case 's':
			if ((w = getwin()) != 0)
				c_size(w);
			break;
		case 'S':
			if ((w = getwin()) != 0)
				sizewin(w, w->ww_alt.nr, w->ww_alt.nc);
			break;
		case 'y':
			c_yank();
			break;
		case 'p':
			c_put();
			break;
		case ':':
			c_colon();
			break;
		case 'h':
			(void) wwwrite(selwin, "\b", 1);
			break;
		case 'j':
			(void) wwwrite(selwin, "\n", 1);
			break;
		case 'k':
			(void) wwwrite(selwin, "\033A", 2);
			break;
		case 'l':
			(void) wwwrite(selwin, "\033C", 2);
			break;
		case ctrl('e'):
			wwscroll(selwin, 1);
			break;
		case ctrl('y'):
			wwscroll(selwin, -1);
			break;
		case ctrl('d'):
			wwscroll(selwin, selwin->ww_w.nr / 2);
			break;
		case ctrl('u'):
			wwscroll(selwin, - selwin->ww_w.nr / 2);
			break;
		case ctrl('f'):
			wwscroll(selwin, selwin->ww_w.nr);
			break;
		case ctrl('b'):
			wwscroll(selwin, - selwin->ww_w.nr);
			break;
		case ctrl('s'):
			stopwin(selwin);
			break;
		case ctrl('q'):
			startwin(selwin);
			break;
		case ctrl('l'):
			wwredraw();
			break;
		case '?':
			c_help();
			break;
		case ctrl('['):
			if (checkproc(selwin) >= 0)
				out = 1;
			break;
		case ctrl('z'):
			wwsuspend();
			break;
		case 'q':
			c_quit();
			break;
		/* debugging stuff */
		case '&':
			if (debug) {
				c_debug();
				break;
			}
		default:
			if (c == escapec) {
				if (checkproc(selwin) >= 0) {
					(void) write(selwin->ww_pty,
						&escapec, 1);
					out = 1;
				}
			} else {
				if (!terse)
					wwbell();
				error("Type ? for help.");
			}
		}
	}
	if (!quit)
		setcmd(0);
}

struct ww *
getwin()
{
	register int c;
	struct ww *w = 0;

	if (!terse)
		wwputs("Which window? ", cmdwin);
	wwcurtowin(cmdwin);
	while ((c = wwgetc()) < 0)
		wwiomux();
	if (debug && c == 'c')
		w = cmdwin;
	else if (debug && c == 'f')
		w = framewin;
	else if (debug && c == 'b')
		w = boxwin;
	else if (c >= '1' && c < NWINDOW + '1')
		w = window[c - '1'];
	else if (c == '+')
		w = selwin;
	else if (c == '-')
		w = lastselwin;
	if (w == 0)
		wwbell();
	if (!terse)
		wwputc('\n', cmdwin);
	return w;
}

checkproc(w)
struct ww *w;
{
	if (w->ww_state != WWS_HASPROC) {
		error("No process in window.");
		return -1;
	}
	return 0;
}

setcmd(new)
char new;
{
	if (new && !incmd) {
		if (!terse)
			wwadd(cmdwin, &wwhead);
		if (selwin != 0)
			wwcursor(selwin, 1);
		wwcurwin = 0;
	} else if (!new && incmd) {
		if (!terse) {
			wwdelete(cmdwin);
			reframe();
		}
		if (selwin != 0)
			wwcursor(selwin, 0);
		wwcurwin = selwin;
	}
	incmd = new;
}

setterse(new)
char new;
{
	if (incmd)
		if (new && !terse) {
			wwdelete(cmdwin);
			reframe();
		} else if (!new && terse)
			wwadd(cmdwin, &wwhead);
	terse = new;
}

/*
 * Set the current window.
 */
setselwin(w)
struct ww *w;
{
	if (selwin == w)
		return;
	if (selwin != 0)
		lastselwin = selwin;
	if ((selwin = w) != 0)
		front(selwin, 1);
}
