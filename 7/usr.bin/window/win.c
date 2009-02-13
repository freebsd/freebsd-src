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
static char sccsid[] = "@(#)win.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "defs.h"
#include "char.h"

/*
 * Higher level routines for dealing with windows.
 *
 * There are two types of windows: user window, and information window.
 * User windows are the ones with a pty and shell.  Information windows
 * are for displaying error messages, and other information.
 *
 * The windows are doubly linked in overlapping order and divided into
 * two groups: foreground and normal.  Information
 * windows are always foreground.  User windows can be either.
 * Addwin() adds a window to the list at the top of one of the two groups.
 * Deletewin() deletes a window.  Front() moves a window to the front
 * of its group.  Wwopen(), wwadd(), and wwdelete() should never be called
 * directly.
 */

/*
 * Open a user window.
 */
struct ww *
openwin(id, row, col, nrow, ncol, nline, label, haspty, hasframe, shf, sh)
char *label;
char haspty, hasframe;
char *shf, **sh;
{
	register struct ww *w;

	if (id < 0 && (id = findid()) < 0)
		return 0;
	if (row + nrow <= 0 || row > wwnrow - 1
	    || col + ncol <= 0 || col > wwncol - 1) {
		error("Illegal window position.");
		return 0;
	}
	w = wwopen(haspty ? WWO_PTY : WWO_SOCKET, nrow, ncol, row, col, nline);
	if (w == 0) {
		error("Can't open window: %s.", wwerror());
		return 0;
	}
	w->ww_id = id;
	window[id] = w;
	w->ww_hasframe = hasframe;
	w->ww_alt = w->ww_w;
	if (label != 0 && setlabel(w, label) < 0)
		error("No memory for label.");
	wwcursor(w, 1);
	/*
	 * We have to do this little maneuver to make sure
	 * addwin() puts w at the top, so we don't waste an
	 * insert and delete operation.
	 */
	setselwin((struct ww *)0);
	addwin(w, 0);
	setselwin(w);
	if (wwspawn(w, shf, sh) < 0) {
		error("Can't execute %s: %s.", shf, wwerror());
		closewin(w);
		return 0;
	}
	return w;
}

findid()
{
	register i;

	for (i = 0; i < NWINDOW && window[i] != 0; i++)
		;
	if (i >= NWINDOW) {
		error("Too many windows.");
		return -1;
	}
	return i;
}

struct ww *
findselwin()
{
	register struct ww *w, *s = 0;
	register i;

	for (i = 0; i < NWINDOW; i++)
		if ((w = window[i]) != 0 && w != selwin &&
		    (s == 0 ||
		     !isfg(w) && (w->ww_order < s->ww_order || isfg(s))))
			s = w;
	return s;
}

/*
 * Close a user window.  Close all if w == 0.
 */
closewin(w)
register struct ww *w;
{
	char didit = 0;
	register i;

	if (w != 0) {
		closewin1(w);
		didit++;
	} else
		for (i = 0; i < NWINDOW; i++) {
			if ((w = window[i]) == 0)
				continue;
			closewin1(w);
			didit++;
		}
	if (didit) {
		if (selwin == 0)
			if (lastselwin != 0) {
				setselwin(lastselwin);
				lastselwin = 0;
			} else if (w = findselwin())
				setselwin(w);
		if (lastselwin == 0 && selwin)
			if (w = findselwin())
				lastselwin = w;
		reframe();
	}
}

/*
 * Open an information (display) window.
 */
struct ww *
openiwin(nrow, label)
char *label;
{
	register struct ww *w;

	if ((w = wwopen(0, nrow, wwncol, 2, 0, 0)) == 0)
		return 0;
	w->ww_mapnl = 1;
	w->ww_hasframe = 1;
	w->ww_nointr = 1;
	w->ww_noupdate = 1;
	w->ww_unctrl = 1;
	w->ww_id = -1;
	w->ww_center = 1;
	(void) setlabel(w, label);
	addwin(w, 1);
	reframe();
	return w;
}

/*
 * Close an information window.
 */
closeiwin(w)
struct ww *w;
{
	closewin1(w);
	reframe();
}

closewin1(w)
register struct ww *w;
{
	if (w == selwin)
		selwin = 0;
	if (w == lastselwin)
		lastselwin = 0;
	if (w->ww_id >= 0 && w->ww_id < NWINDOW)
		window[w->ww_id] = 0;
	if (w->ww_label)
		str_free(w->ww_label);
	deletewin(w);
	wwclose(w);
}

/*
 * Move the window to the top of its group.
 * Don't do it if already fully visible.
 * Wwvisible() doesn't work for tinted windows.
 * But anything to make it faster.
 * Always reframe() if doreframe is true.
 */
front(w, doreframe)
register struct ww *w;
char doreframe;
{
	if (w->ww_back != (isfg(w) ? framewin : fgwin) && !wwvisible(w)) {
		deletewin(w);
		addwin(w, isfg(w));
		doreframe = 1;
	}
	if (doreframe)
		reframe();
}

/*
 * Add a window at the top of normal windows or foreground windows.
 * For normal windows, we put it behind the current window.
 */
addwin(w, fg)
register struct ww *w;
char fg;
{
	if (fg) {
		wwadd(w, framewin);
		if (fgwin == framewin)
			fgwin = w;
	} else
		wwadd(w, selwin != 0 && selwin != w && !isfg(selwin)
				? selwin : fgwin);
}

/*
 * Delete a window.
 */
deletewin(w)
register struct ww *w;
{
	if (fgwin == w)
		fgwin = w->ww_back;
	wwdelete(w);
}

reframe()
{
	register struct ww *w;

	wwunframe(framewin);
	for (w = wwhead.ww_back; w != &wwhead; w = w->ww_back)
		if (w->ww_hasframe) {
			wwframe(w, framewin);
			labelwin(w);
		}
}

labelwin(w)
register struct ww *w;
{
	int mode = w == selwin ? WWM_REV : 0;

	if (!w->ww_hasframe)
		return;
	if (w->ww_id >= 0) {
		char buf[2];

		buf[0] = w->ww_id + '1';
		buf[1] = 0;
		wwlabel(w, framewin, 1, buf, mode);
	}
	if (w->ww_label) {
		int col;

		if (w->ww_center) {
			col = (w->ww_w.nc - strlen(w->ww_label)) / 2;
			col = MAX(3, col);
		} else
			col = 3;
		wwlabel(w, framewin, col, w->ww_label, mode);
	}
}

stopwin(w)
	register struct ww *w;
{
	if (w->ww_pty >= 0 && w->ww_ispty && wwstoptty(w->ww_pty) < 0)
		error("Can't stop output: %s.", wwerror());
	else
		w->ww_stopped = 1;
}

startwin(w)
	register struct ww *w;
{
	if (w->ww_pty >= 0 && w->ww_ispty && wwstarttty(w->ww_pty) < 0)
		error("Can't start output: %s.", wwerror());
	else
		w->ww_stopped = 0;
}

sizewin(w, nrow, ncol)
register struct ww *w;
{
	struct ww *back = w->ww_back;

	w->ww_alt.nr = w->ww_w.nr;
	w->ww_alt.nc = w->ww_w.nc;
	wwdelete(w);
	if (wwsize(w, nrow, ncol) < 0)
		error("Can't resize window: %s.", wwerror());
	wwadd(w, back);
	reframe();
}

waitnl(w)
struct ww *w;
{
	(void) waitnl1(w, "[Type any key to continue]");
}

more(w, always)
register struct ww *w;
char always;
{
	int c;
	char uc = w->ww_unctrl;

	if (!always && w->ww_cur.r < w->ww_w.b - 2)
		return 0;
	c = waitnl1(w, "[Type escape to abort, any other key to continue]");
	w->ww_unctrl = 0;
	wwputs("\033E", w);
	w->ww_unctrl = uc;
	return c == ctrl('[') ? 2 : 1;
}

waitnl1(w, prompt)
register struct ww *w;
char *prompt;
{
	char uc = w->ww_unctrl;

	w->ww_unctrl = 0;
	front(w, 0);
	wwprintf(w, "\033Y%c%c\033sA%s\033rA ",
		w->ww_w.nr - 1 + ' ', ' ', prompt);	/* print on last line */
	wwcurtowin(w);
	while (wwpeekc() < 0)
		wwiomux();
	w->ww_unctrl = uc;
	return wwgetc();
}
