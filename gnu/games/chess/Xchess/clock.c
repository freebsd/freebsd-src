
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


/* RCS Info: $Revision: 1.4 $ on $Date: 86/11/26 12:09:47 $
 *           $Source: /users/faustus/xchess/RCS/clock.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Do stuff with the clocks.  The way things work is as follows.  We call
 * clock_init to draw the clocks initially, but they don't actually start
 * running until we call clock_switch for the first time.
 */

#include "xchess.h"

int movesperunit = 0;
int timeunit = 0;
bool clock_started = false;
int whiteseconds, blackseconds;

static bool white_running = true;
static long lastwhite, lastblack;
static bool firstmove = true;

extern void dohands(), hilight();

#define PI 3.1415926535897932384

void
clock_draw(win, col)
	windata *win;
	color col;
{
	int i;
	char buf[BSIZE];
	int x = CLOCK_WIDTH / 2, y = CLOCK_WIDTH / 2;
	int xp, yp;
	int rad = CLOCK_WIDTH / 2 - 10;
	Window w = ((col == WHITE) ? win->wclockwin : win->bclockwin);

	/* Draw a clock face and the hands. */
	XCircle(w, x, y, rad, 0.0, 0.0, 1, 1, win->textcolor.pixel, GXcopy, 
			AllPlanes);
	rad -= 8;

	XSetFont(win->display, DefaultGC(win->display, 0),
		 win->small->fid);
	XSetForeground(win->display, DefaultGC(win->display, 0),
		       win->textcolor.pixel);
	XSetBackground(win->display, DefaultGC(win->display, 0),
		       win->textback.pixel);
	for (i = 1; i <= 12; i++) {
		xp = x + rad * cos(PI * 3 / 2 + i * PI / 6) - 4;
		yp = y + rad * sin(PI * 3 / 2 + i * PI / 6) - 5;
		sprintf(buf, "%d", i);
		XDrawString(win->display, w, DefaultGC(win->display, 0),
			    xp, yp, buf, strlen(buf));
	}

	dohands(win, col);

	if (white_running) {
		hilight(win, WHITE, true);
		hilight(win, BLACK, false);
	} else {
		hilight(win, WHITE, false);
		hilight(win, BLACK, true);
	}
	return;
}

void
clock_init(win, col)
	windata *win;
	color col;
{
	whiteseconds = blackseconds = 0;
	clock_started = false;
	firstmove = true;
	clock_draw(win, col);

	return;
}

void
clock_update()
{
	int now = time((long *) NULL);
	int i;

	if (!clock_started) {
		lastwhite = lastblack = now;
		return;
	}
	
	if (white_running) {
		whiteseconds += now - lastwhite;
		lastwhite = now;
		dohands(win1, WHITE);
		if (!oneboard)
			dohands(win2, WHITE);
		if (timeunit) {
			i = whiteseconds / timeunit;
			if ((i > 0) && (whiteseconds > i * timeunit) &&
					(whiteseconds < i * timeunit + 10) &&
					(movesperunit * i > movenum)) {
				message_add(win1,
					"White has exceeded his time limit\n",
						true);
				if (!oneboard) {
					message_add(win2,
					"White has exceeded his time limit\n",
						true);
				}
				timeunit = 0;
			}
		}
	} else {
		blackseconds += now - lastblack;
		lastblack = now;
		dohands(win1, BLACK);
		if (!oneboard)
			dohands(win2, BLACK);
		if (timeunit) {
			i = blackseconds / timeunit;
			if ((i > 0) && (blackseconds > i * timeunit) &&
					(blackseconds < i * timeunit + 10) &&
					(movesperunit * i > movenum)) {
				message_add(win1,
					"Black has exceeded his time limit\n",
						true);
				if (!oneboard) {
					message_add(win2,
					"Black has exceeded his time limit\n",
						true);
				}
				timeunit = 0;
			}
		}
	}
	return;
}

void
clock_switch()
{
	if (firstmove) {
		clock_started = true;
		firstmove = false;
		lastwhite = lastblack = time((long *) NULL);
	}
	if (white_running) {
		white_running = false;
		lastblack = time((long *) NULL);
		hilight(win1, WHITE, false);
		hilight(win1, BLACK, true);
		if (!oneboard) {
			hilight(win2, WHITE, false);
			hilight(win2, BLACK, true);
		}
	} else {
		white_running = true;
		lastwhite = time((long *) NULL);
		hilight(win1, WHITE, true);
		hilight(win1, BLACK, false);
		if (!oneboard) {
			hilight(win2, WHITE, true);
			hilight(win2, BLACK, false);
		}
	}
	return;
}

static void
dohands(win, col)
	windata *win;
	color col;
{
	int cx = CLOCK_WIDTH / 2, cy = CLOCK_WIDTH / 2;
	double *h = (col == WHITE) ? win->whitehands : win->blackhands;
	Window w = (col == WHITE) ? win->wclockwin : win->bclockwin; 
	long secs = (col == WHITE) ? whiteseconds : blackseconds;
	int rad, x, y, i;

	/* First erase the old hands. */
	XSetState(win->display, DefaultGC(win->display, 0),
		  win->textback.pixel, win->textback.pixel,
		  GXcopy, AllPlanes);

	rad = CLOCK_WIDTH / 2 - 30;
	for (i = 0; i < 3; i++) {
		x = cx + rad * sin(PI - h[i]);
		y = cy + rad * cos(PI - h[i]);
		XSetLineAttributes(win->display,
				   DefaultGC(win->display, 0),
				   i, LineSolid, 0, 0);
		XDrawLine(win->display, w, DefaultGC(win->display, 0),
			  cx, cy, x, y);
		rad -= 8;
	}

	h[0] = (secs % 60) * 2 * PI / 60;
	h[1] = ((secs / 60) % 60) * 2 * PI / 60;
	h[2] = ((secs / 3600) % 12) * 2 * PI / 12;

	/* Now draw the new ones. */

	XSetState(win->display, DefaultGC(win->display, 0),
		  win->textcolor.pixel, win->textback.pixel,
		  GXcopy, AllPlanes);

	rad = CLOCK_WIDTH / 2 - 30;
	for (i = 0; i < 3; i++) {
		x = cx + rad * sin(PI - h[i]);
		y = cy + rad * cos(PI - h[i]);
		XSetLineAttributes(win->display,
				   DefaultGC(win->display, 0),
				   i, LineSolid, 0, 0);
		XDrawLine(win->display, w, DefaultGC(win->display, 0),
			  cx, cy, x, y);
		rad -= 8;
	}
	XFlush(win->display);
	return;
}

static void
hilight(win, col, on)
	windata *win;
	color col;
	bool on;
{
	Window w = (col == WHITE) ? win->wclockwin : win->bclockwin;
	char *s = (col == WHITE) ? " WHITE " : " BLACK ";
	int x;


	x = XTextWidth(win->large, s, strlen(s));
	if (on)
		XSetState(win->display, DefaultGC(win->display, 0),
			  win->textback.pixel,
			  win->textcolor.pixel,
			  GXcopy,
			  AllPlanes);
	else
		XSetState(win->display, DefaultGC(win->display, 0),
			  win->textcolor.pixel,
			  win->textback.pixel,
			  GXcopy, AllPlanes);

	XSetLineAttributes(win->display, DefaultGC(win->display, 0),
		      BORDER_WIDTH, LineSolid, CapButt, JoinMiter);
	XSetFont(win->display, DefaultGC(win->display, 0),
		 win->large->fid);
	
	XDrawLine(win->display, w, DefaultGC(win->display, 0),
		  0, CLOCK_HEIGHT - 26,
		  CLOCK_WIDTH, CLOCK_HEIGHT - 26);
	
	XDrawImageString(win->display, w, DefaultGC(win->display, 0),
			 (CLOCK_WIDTH - x) / 2, CLOCK_HEIGHT,
			 s, strlen(s));

	if (on)
		XSetState(win->display, DefaultGC(win->display, 0),
			  win->textcolor.pixel,
			  win->textback.pixel,
			  GXcopy, AllPlanes);
	return;
}

