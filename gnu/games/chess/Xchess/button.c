
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


/* RCS Info: $Revision: 1.4 $ on $Date: 86/11/26 12:09:41 $
 *           $Source: /users/faustus/xchess/RCS/button.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Do stuff with the buttons.
 * The configuration we're using is:	Draw	Back	Pause
 *					Resign	Fwd	Flip
 *					Reset	Save	Easy (Switch)
 */

#include "xchess.h"

typedef enum choice { NOCHOICE, DRAW, RESIGN, REPLAY, SWITCH, FORE, SAVE,
		STOP, FLIP, RESTART, EASY } choice;

static struct but {
	char *label;
	int x, y;
	int width, height;
	choice which;
} buts[] = {
	{ "Draw", 0, 20, 108, 29, DRAW } ,
	{ "Back", 109, 20, 108, 29, REPLAY } ,
	{ "Pause", 219, 20, 108, 29, STOP } ,
	{ "Resign", 0, 50, 108, 29, RESIGN } ,
	{ "Fwd", 109, 50, 108, 29, FORE } ,
	{ "Flip", 219, 50, 108, 29, FLIP } ,
	{ "Reset", 0, 80, 108, 29, RESTART } ,
	{ "Save", 109, 80, 108, 29, SAVE } ,
#define EASY_OFFSET 8
	{ "Switch", 219, 80, 108, 29, SWITCH } 
/*	{ "NoEasy", 219, 80, 108, 29, EASY }*/
} ;
static int easy = 1;

void
button_draw(win)
	windata *win;
{
	int i, x, numbuts = sizeof (buts) / sizeof (struct but);

	XSetState(win->display, DefaultGC(win->display, 0),
		  win->border.pixel, WhitePixel(win->display, 0),
		  GXcopy, AllPlanes);
	XSetLineAttributes(win->display, DefaultGC(win->display, 0),
			   BORDER_WIDTH, LineSolid, CapButt,
			   JoinMiter);
	
	XDrawLine(win->display, win->buttonwin,
		  DefaultGC(win->display, 0),
		  0, 29, BUTTON_WIDTH, 29);
	XDrawLine(win->display, win->buttonwin,
		  DefaultGC(win->display, 0),
		  0, 60, BUTTON_WIDTH, 60);
	XDrawLine(win->display, win->buttonwin,
		  DefaultGC(win->display, 0),
		  108, 0, 108, BUTTON_HEIGHT);
	XDrawLine(win->display, win->buttonwin,
		  DefaultGC(win->display, 0),
		  219, 0, 219, BUTTON_HEIGHT);

	XSetFont(win->display, DefaultGC(win->display, 0), win->large->fid);
	XSetForeground(win->display, DefaultGC(win->display, 0),
		       win->textcolor.pixel); 
	XSetBackground(win->display, DefaultGC(win->display, 0),
		       win->textback.pixel); 

	for (i = 0; i < numbuts; i++) {
		x = (buts[i].width -
		     XTextWidth(win->large, buts[i].label,
				strlen(buts[i].label))) / 2;

		XDrawImageString(win->display, win->buttonwin,
				 DefaultGC(win->display, 0),
				 buts[i].x + x, buts[i].y, buts[i].label,
				 strlen(buts[i].label));
	}
	return;
}

void
button_service(win, event)
	windata *win;
	XEvent *event;
{
	XKeyEvent *ev = &event->xkey;
	choice c;
	int i, numbuts = sizeof (buts) / sizeof (struct but);
	char *s;

	ev->y += 15;
	for (i = 0; i < numbuts; i++)
		if ((ev->x >= buts[i].x) && (ev->x <= buts[i].x +
				buts[i].width) && (ev->y >= buts[i].y) &&
				(ev->y <= buts[i].y + buts[i].height)) {
			c = buts[i].which;
			break;
		}
	if ((i == numbuts) || (c == NOCHOICE)) {
		message_add(win, "Bad choice.\n", true);
		return;
	}

	if (loading_flag && (c != STOP)) {
		message_add(win, "You can only use PAUSE now\n", true);
		return;
	}

	switch (c) {
	    case DRAW:
		if (!oneboard) {
			message_add(win, "Just a sec...\n", false);
			if (!pop_question(((win == win1) ? win2 : win1),
"The other player wants\nto call the game a draw.\nDo you agree?\n")) {
				message_add(win,
				"The other player declines the draw\n", false);
				return;
			}
		}
		message_add(win1, "Draw agreed.\n", false);
		if (!oneboard)
			message_add(win2, "Draw agreed.\n", false);
		cleanup("Draw agreed.");
		break;

	    case RESIGN:
		if (!pop_question(win, "Are you sure\nyou want to resign?"))
			return;
		if ((oneboard && !progflag) || (nexttomove == win->color)) {
			if (nexttomove == WHITE)
				s = "White resigns.";
			else
				s = "Black resigns.";
			if (oneboard) {
				message_add(win, s, false);
				message_add(win, "\n", false);
			} else {
				message_add(win1, s, false);
				message_add(win, "\n", false);
				message_add(win2, s, false);
				message_add(win, "\n", false);
			}
			sleep(5);
			cleanup(s);
		} else {
			message_add(win, "It's not your turn.\n", true);
		}
		break;

	    case REPLAY:
		if (!oneboard) {
			message_add(win, "Just a sec...\n", false);
			if (!pop_question(((win == win1) ? win2 : win1),
"The other player wants\nto take back his last move.\nDo you let him?\n")) {
				message_add(win,
				"The other player refuses...\n", false);
				return;
			}
		}
		if (!moves) {
			message_add(win, "Can't back up...\n", true);
			break;
		}
		message_add(win1, "Replaying...\n", false);
		if (!oneboard)
			message_add(win2, "Replaying...\n", false);
		replay();
		if (progflag)
		    replay();
		break;

	    case FORE:
		if (!oneboard) {
			message_add(win, "Just a sec...\n", false);
			if (!pop_question(((win == win1) ? win2 : win1),
"The other player wants\nto do a 'fore'.\nIs that ok with you?\n")) {
				message_add(win,
				"The other player refuses...\n", false);
				return;
			}
		}
		if (!foremoves) {
			message_add(win, "Can't go forward...\n", true);
			break;
		}
		message_add(win1, "Moving forward...\n", false);
		if (!oneboard)
			message_add(win2, "Moving forward...\n", false);
		forward();
		break;

	    case SWITCH:
		message_add(win, "You can't switch yet.\n", false);
		break;

	    case SAVE:
		if (saveflag) {
			message_add(win, 
				"Game is already being logged in file '", true);
			message_add(win, record_file, true);
			message_add(win, "'.\n", true);
		} else {
			message_add(win, "Saving game to file '", false);
			message_add(win, record_file, false);
			message_add(win, "'.\n", false);
			record_save();
		}
		break;

	    case STOP:
		if (loading_flag) {
			loading_paused = (loading_paused ? false : true);
			message_add(win, loading_paused ?
				"Stopped.\nHit 'Pause' again to restart.\n" :
				"Restarted.\n", false);
		} else if (clock_started) {
			if (!oneboard) {
				message_add(win, "Just a sec...\n", false);
				if (!pop_question(((win == win1) ? win2 : win1),
"The other player wants\nto stop the clock.\nDo you let him?\n")) {
					message_add(win,
					"The other player refuses to pause.\n",
					false);
					return;
				}
			}
			message_add(win1, 
			"Clock stopped.\nHit 'Pause' again to restart.\n",
					false);
			if (!oneboard)
				message_add(win2, 
			"Clock stopped.\nHit 'Pause' again to restart.\n", 
					false);
			clock_started = false;
		} else {
			if (!oneboard) {
				message_add(win, "Just a sec...\n", false);
				if (!pop_question(((win == win1) ? win2 : win1),
"The other player wants\nto start the clock again.\nIs that ok?\n")) {
					message_add(win,
				"The other player refuses to resume.\n",
					false);
					return;
				}
			}
			message_add(win1, "Clock restarted.\n", false);
			if (!oneboard)
				message_add(win2, "Clock restarted.\n", false);
			clock_started = true;
		}
		break;

	    case FLIP:
		message_add(win, "Flipping window...\n", false);
		win->flipped = win->flipped ? false : true;
		win_redraw(win, (XEvent *) NULL);
		break;

	    case RESTART:
		if (!oneboard) {
			message_add(win, "Just a sec...\n", false);
			if (!pop_question(((win == win1) ? win2 : win1),
"The other player wants\nto restart the game.\nDo you agree?\n")) {
				message_add(win,
				"The other player refuses to reset\n", false);
				return;
			}
		}
		message_add(win, "Restarting game.\n", false);
		restart();
		break;
	    case EASY:
		if (oneboard) {
			int x;
			if (easy)
				 easy = 0;
			else
				easy = 1;

			if (easy)
				buts[EASY_OFFSET].label = " Easy ";
			else
				buts[EASY_OFFSET].label = "NoEasy";

			program_easy(easy);

			x = (buts[EASY_OFFSET].width -
				   XTextWidth(win->large,
					 buts[EASY_OFFSET].label,
					 strlen(buts[EASY_OFFSET].label))) / 2;

			XSetFont(win->display, DefaultGC(win->display,
							 0), win->large->fid);
			XSetForeground(win->display,
				       DefaultGC(win->display, 0),
				       win->textcolor.pixel);
			XSetBackground(win->display,
				       DefaultGC(win->display, 0),
				       win->textback.pixel); 

			XDrawImageString(win->display,
					 win->buttonwin,
					 DefaultGC(win->display, 0),
					 buts[EASY_OFFSET].x + x,
					 buts[EASY_OFFSET].y,
					 buts[EASY_OFFSET].label,
					 strlen(buts[EASY_OFFSET].label));
		}
		break;
	}
	return;
}

