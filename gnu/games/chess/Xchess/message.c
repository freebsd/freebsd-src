
/* This file contains code for X-CHESS.
   Copyright (C) 1986 Free Software Foundation, Inc.

This file is part of X-CHESS.

X-CHESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the X-CHESS General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
X-CHESS, but only under the conditions described in the
X-CHESS General Public License.   A copy of this license is
supposed to have been given to you along with X-CHESS so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


/* RCS Info: $Revision: 1.4 $ on $Date: 86/11/26 12:10:22 $
 *           $Source: /users/faustus/xchess/RCS/message.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Do stuff with the message window.  Font 0 is the normal font, font 1
 * is large, and font 2 is normal red.
 */

#include "xchess.h"

#define MESSAGE_HEADER	"\n1  XChess Messages0\n"

void
message_init(win)
	windata *win;
{
	TxtGrab(win->display, win->messagewin, "xchess", win->medium, 
			win->textback.pixel, win->textcolor.pixel,
				win->cursorcolor.pixel);
	TxtAddFont(win->display, win->messagewin, 1, win->large, win->textcolor.pixel);
	TxtAddFont(win->display, win->messagewin, 2, win->medium, win->errortext.pixel);
	TxtAddFont(win->display, win->messagewin, 3, win->medium, win->playertext.pixel);

	TxtWriteStr(win->display, win->messagewin, MESSAGE_HEADER);
	return;
}

void
message_add(win, string, err)
	windata *win;
	char *string;
	bool err;
{
	if (err) {
		TxtWriteStr(win->display, win->messagewin, "2");
		TxtWriteStr(win->display, win->messagewin, string);
		TxtWriteStr(win->display, win->messagewin, "0");
		XBell(win->display, 50);
	} else
		TxtWriteStr(win->display, win->messagewin, string);

	XSync(win->display, 0);
	return;
}

void
message_send(win, event)
	windata *win;
	XEvent *event;
{
	XKeyEvent *ev = &event->xkey;
	KeySym keysym;
	windata *ow = (win == win1) ? win2 : win1;
	char buf[BSIZE], *s;
	int i;

	i = XLookupString(ev, buf, sizeof(buf) - 1, &keysym, &s);
	buf[i] = '\0';
	for (s = buf; *s; s++)
		if (*s == '\r')
			*s = '\n';
		else if (*s == '\177')
			*s = '';

	TxtWriteStr(win->display, win->messagewin, "3");
	TxtWriteStr(win->display, win->messagewin, buf);
	TxtWriteStr(win->display, win->messagewin, "0");
	XSync(win->display, 0);
	if (ow) {
		TxtWriteStr(ow->display, ow->messagewin, "3");
		TxtWriteStr(ow->display, ow->messagewin, buf);
		TxtWriteStr(ow->display, ow->messagewin, "0");
		XSync(ow->display, 0);
	}
	return;
}

