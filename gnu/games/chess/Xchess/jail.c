
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


/* RCS Info: $Revision: 1.3 $ on $Date: 86/11/26 12:09:54 $
 *           $Source: /users/faustus/xchess/RCS/jail.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 */

#include "xchess.h"

#include "pawn_small.bitmap"
#include "rook_small.bitmap"
#include "knight_small.bitmap"
#include "bishop_small.bitmap"
#include "queen_small.bitmap"
#include "king_small.bitmap"

#include "pawn_small_outline.bitmap"
#include "rook_small_outline.bitmap"
#include "knight_small_outline.bitmap"
#include "bishop_small_outline.bitmap"
#include "queen_small_outline.bitmap"
#include "king_small_outline.bitmap"

static bool pos[32];

static piecetype pcs[] = { KING, QUEEN, ROOK, ROOK, BISHOP, BISHOP, KNIGHT,
		KNIGHT, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN } ;

extern int piecepos();
extern char *bitsget();

void
jail_init(win)
	windata *win;
{
	int i;

	for (i = 0; i < 32; i++)
		pos[i] = false;
	jail_draw(win);
	return;
}

#define JAIL_HEADER	"Captured Pieces"

void
jail_draw(win)
	windata *win;
{
	int i;
	char *bits;
	Pixmap tmpPM;
	piece p;

	i = XTextWidth(win->large, JAIL_HEADER, strlen(JAIL_HEADER));
	XSetFont(win->display, DefaultGC(win->display, 0),
		 win->large->fid);
	XSetForeground(win->display, DefaultGC(win->display, 0),
		       win->textcolor.pixel);
	XSetBackground(win->display, DefaultGC(win->display, 0),
		       win->textback.pixel);
	
	XDrawImageString(win->display, win->jailwin,
			 DefaultGC(win->display, 0),
			 (JAIL_WIDTH - i) / 2, 20, JAIL_HEADER,
			strlen(JAIL_HEADER));

	XSetForeground(win->display, DefaultGC(win->display, 0),
		       win->blackpiece.pixel);
	XSetBackground(win->display, DefaultGC(win->display, 0),
		       win->textback.pixel);
	XSetFillStyle(win->display, DefaultGC(win->display, 0),
		      FillSolid);
	XSetFunction(win->display, DefaultGC(win->display, 0),
		     GXcopy);
	
	for (i = 0; i < 16; i++)
		if (pos[i]) {
			p.color = WHITE;
			p.type = pcs[i];
			bits = bitsget(&p);
			tmpPM = XCreateBitmapFromData(win->display,
				     win->jailwin, bits,
				     32, 32);
			
			XCopyPlane(win->display, tmpPM, win->jailwin,
				  DefaultGC(win->display, 0),
				  0, 0, 32, 32,
				  5 + (i % 8) * 32, 25 + (i / 8) * 32,
				  1);
		        XFreePixmap(win->display, tmpPM);
		} else {
		    XFillRectangle(win->display, win->jailwin,
				   DefaultGC(win->display, 0),
				   5 + (i % 8) * 32,
				   25 + (i / 8) * 32,
				   32, 32);
		}
	for (i = 0; i < 16; i++)
		if (pos[i + 16]) {
		    p.color = BLACK;
		    p.type = pcs[i];
		    bits = bitsget(&p);
		    tmpPM = XCreateBitmapFromData(win->display,
						  win->jailwin, bits,
						  32, 32);
			
		    XCopyPlane(win->display, tmpPM, win->jailwin,
			       DefaultGC(win->display, 0),
			       0, 0, 32, 32,
			       5 + (i % 8) * 32, 94 + (i / 8) * 32,
			       1);
		    XFreePixmap(win->display, tmpPM);
		} else {
		    XFillRectangle(win->display, win->jailwin,
				   DefaultGC(win->display, 0),
				   5 + (i % 8) * 32, 94 + (i / 8) * 32,
				   32, 32);
		}

	return;
}

void
jail_add(p)
	piece *p;
{
	int i = piecepos(p, false);
	char *bits;
	Pixmap tmpPM;
	
	pos[i] = true;

	bits = bitsget(p);

	XSetState(win1->display, DefaultGC(win1->display, 0),
		       win1->blackpiece.pixel,
		       win1->textback.pixel,
		       GXcopy,
		       AllPlanes);

	tmpPM = XCreateBitmapFromData(win1->display,
				      win1->jailwin, bits,
				      32, 32);
			
	XCopyPlane(win1->display, tmpPM, win1->jailwin,
		   DefaultGC(win1->display, 0),
		   0, 0, 32, 32,
		   5 + (i % 8) * 32, ((i >= 16) ? 30 : 25) + (i / 8) * 32,
		   1);
	XFreePixmap(win1->display, tmpPM);
	
	if (!oneboard) {
	    XSetState(win2->display, DefaultGC(win2->display, 0),
		      win2->blackpiece.pixel,
		      win2->textback.pixel,
		      GXcopy,
		      AllPlanes);


	    tmpPM = XCreateBitmapFromData(win2->display,
					  win2->jailwin, bits,
					  32, 32);
			
	    XCopyPlane(win2->display, tmpPM, win2->jailwin,
		       DefaultGC(win2->display, 0),
		       0, 0, 32, 32,
		       5 + (i % 8) * 32, ((i >= 16) ? 30 : 25) + (i / 8) * 32,
		       1);
	    XFreePixmap(win2->display, tmpPM);
	}

	return;
}

void
jail_remove(p)
	piece *p;
{
    int i = piecepos(p, true);

    pos[i] = false;


    XSetForeground(win1->display,
		   DefaultGC(win1->display, 0),
		   win1->blackpiece.pixel);
    XSetBackground(win1->display,
		   DefaultGC(win1->display, 0),
		   win1->textback.pixel);
    XSetFillStyle(win1->display,
		  DefaultGC(win1->display, 0),
		  FillSolid);

    XFillRectangle(win1->display, win1->jailwin,
		   DefaultGC(win1->display, 0),
		   5 + (i % 8) * 32,
		   ((i >= 16) ? 30 : 25) + (i / 8) * 32,
		   32, 32);

    if (!oneboard) {
	XSetForeground(win2->display,
		       DefaultGC(win2->display, 0),
		       win2->blackpiece.pixel);
	XSetBackground(win2->display,
		       DefaultGC(win2->display, 0),
		       win2->textback.pixel);
	XSetFillStyle(win2->display,
		      DefaultGC(win2->display, 0),
		      FillSolid);

	XFillRectangle(win2->display, win2->jailwin,
		       DefaultGC(win2->display, 0),
		       5 + (i % 8) * 32,
		       ((i >= 16) ? 30 : 25) + (i / 8) * 32,
		       32, 32);
    }

    return;
}

static char *bitsget(p)
	piece *p;
{
	char *bits;

	switch (p->type) {
	    case PAWN:
		bits = (p->color == WHITE) ? pawn_small_outline_bits :
				pawn_small_bits;
		break;

	    case ROOK:
		bits = (p->color == WHITE) ? rook_small_outline_bits :
				rook_small_bits;
		break;

	    case KNIGHT:
		bits = (p->color == WHITE) ? knight_small_outline_bits :
				knight_small_bits;
		break;

	    case BISHOP:
		bits = (p->color == WHITE) ? bishop_small_outline_bits :
				bishop_small_bits;
		break;

	    case QUEEN:
		bits = (p->color == WHITE) ? queen_small_outline_bits :
				queen_small_bits;
		break;

	    case KING:
		bits = (p->color == WHITE) ? king_small_outline_bits :
				king_small_bits;
		break;
	}
	return (bits);
}

static int
piecepos(p, there)
	piece *p;
	bool there;
{
	int i, base = (p->color == WHITE) ? 0 : 16;

	switch (p->type) {
	    case PAWN:
		for (i = base + 8; (i < base + 15) && pos[i]; i++)
			;
		if (there && !pos[i])
			i--;
		break;

	    case KING:
		/* Hmm... */
		i = base;
		break;

	    case QUEEN:
		i = base + 1;
		break;

	    case ROOK:
		i = base + 2;
		if ((there && pos[i + 1]) || (!there && pos[i]))
			i++;
		break;

	    case BISHOP:
		i = base + 4;
		if ((there && pos[i + 1]) || (!there && pos[i]))
			i++;
		break;

	    case KNIGHT:
		i = base + 6;
		if ((there && pos[i + 1]) || (!there && pos[i]))
			i++;
		break;
	}
	return (i);
}
