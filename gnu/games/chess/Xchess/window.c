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


/* RCS Info: $Revision: 1.5 $ on $Date: 86/11/26 12:11:15 $
 *           $Source: /users/faustus/xchess/RCS/window.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Deal with the two (or one) windows.
 */

#include "xchess.h"
#include <X11/Xutil.h>
#include <sys/time.h>

#include "pawn.bitmap"
#include "rook.bitmap"
#include "knight.bitmap"
#include "bishop.bitmap"
#include "queen.bitmap"
#include "king.bitmap"

#include "pawn_outline.bitmap"
#include "rook_outline.bitmap"
#include "knight_outline.bitmap"
#include "bishop_outline.bitmap"
#include "queen_outline.bitmap"
#include "king_outline.bitmap"

#include "pawn_mask.bitmap"
#include "rook_mask.bitmap"
#include "knight_mask.bitmap"
#include "bishop_mask.bitmap"
#include "queen_mask.bitmap"
#include "king_mask.bitmap"

#include "shade.bitmap"

#include "xchess.cur"
#include "xchess_mask.cur"

#include "xchess.icon"

windata *win1, *win2;
bool win_flashmove = false;

extern bool setup();
extern void service(), drawgrid(), icon_refresh();

bool
win_setup(disp1, disp2)
	char *disp1, *disp2;
{
	win1 = alloc(windata);
	if (!oneboard)
		win2 = alloc(windata);

	if (!setup(disp1, win1) || (!oneboard && !setup(disp2, win2)))
		return (false);

	if (blackflag) {
		win1->color = BLACK;
		win1->flipped = true;
	} else
		win1->color = WHITE;
	win_drawboard(win1);

	if (!oneboard) {
		win2->color = BLACK;
		win2->flipped = true;
		win_drawboard(win2);
	}
	
	return(true);
}

/* Draw the chess board... */

void
win_drawboard(win)
	windata *win;
{
	int i, j;

	drawgrid(win);

	/* Now toss on the squares... */
	for (i = 0; i < SIZE; i++)
		for (j = 0; j < SIZE; j++)
			win_erasepiece(j, i, win->color);

	return;
}

/* Draw one piece. */

void
win_drawpiece(p, y, x, wnum)
	piece *p;
	int y, x;
	color wnum;
{
    char *bits, *maskbits, *outline;
    windata *win;
    char buf[BSIZE];
    XImage *tmpImage;
    Pixmap tmpPM, maskPM;
    XGCValues gc;

    if (oneboard || (wnum == win1->color))
	win = win1;
    else
	win = win2;

    if (win->flipped) {
	y = SIZE - y - 1;
	x = SIZE - x - 1;
    }

    /*
      if (debug)
      fprintf(stderr, "draw a %s at (%d, %d) on board %d\n",
      piecenames[(int) p->type], y, x, wnum);
      */

    if ((x < 0) || (x > 7) || (y < 0) || (y > 7)) exit(1);

    switch (p->type) {
    case PAWN:
	bits = pawn_bits;
	maskbits = pawn_mask_bits;
	outline = pawn_outline_bits;
	break;

    case ROOK:
	bits = rook_bits;
	maskbits = rook_mask_bits;
	outline = rook_outline_bits;
	break;

    case KNIGHT:
	bits = knight_bits;
	maskbits = knight_mask_bits;
	outline = knight_outline_bits;
	break;

    case BISHOP:
	bits = bishop_bits;
	maskbits = bishop_mask_bits;
	outline = bishop_outline_bits;
	break;

    case QUEEN:
	bits = queen_bits;
	maskbits = queen_mask_bits;
	outline = queen_outline_bits;
	break;

    case KING:
	bits = king_bits;
	maskbits = king_mask_bits;
	outline = king_outline_bits;
	break;

    default:
	fprintf(stderr,
		"Internal Error: win_drawpiece: bad piece type %d\n",
		p->type);
    }

    /* There are two things we can do... If this is a black and white
     * display, we have to shade the square and use an outline if the piece
     * is white.  We also have to use a mask...  Since we don't want
     * to use up too many bitmaps, create the mask bitmap, put the bits,
     * and then destroy it.
     */
    if (win->bnw && (p->color == WHITE))
	bits = outline;
    if (win->bnw && !iswhite(win, x, y)) {
	XSetState(win->display, DefaultGC(win->display, 0),
		  BlackPixel(win->display, 0),
		  WhitePixel(win->display, 0), GXcopy, AllPlanes);
	
	tmpPM = XCreateBitmapFromData(win->display, win->boardwin,
				shade_bits, SQUARE_WIDTH, SQUARE_HEIGHT);

	XCopyPlane(win->display, tmpPM, win->boardwin, DefaultGC(win->display, 0),
		   0, 0, SQUARE_WIDTH, SQUARE_HEIGHT,
		   x * (SQUARE_WIDTH + BORDER_WIDTH),
		   y * (SQUARE_HEIGHT + BORDER_WIDTH), 1);

	XFreePixmap(win->display, tmpPM);
	
	XSetFunction(win->display, DefaultGC(win->display, 0),
		     GXandInverted);
	maskPM = XCreateBitmapFromData(win->display, win->boardwin,
				      maskbits, SQUARE_WIDTH, SQUARE_HEIGHT);
	XCopyPlane(win->display, maskPM, win->boardwin, DefaultGC(win->display, 0),
		   0, 0, SQUARE_WIDTH, SQUARE_HEIGHT,
		   x * (SQUARE_WIDTH + BORDER_WIDTH),
		   y * (SQUARE_HEIGHT + BORDER_WIDTH), 1);
	XFreePixmap(win->display, maskPM);

	XSetFunction(win->display, DefaultGC(win->display, 0),
		     GXor);
	tmpPM = XCreateBitmapFromData(win->display, win->boardwin,
				bits, SQUARE_WIDTH, SQUARE_HEIGHT);
	XCopyPlane(win->display, tmpPM, win->boardwin, DefaultGC(win->display, 0),
		   0, 0, SQUARE_WIDTH, SQUARE_HEIGHT,
		   x * (SQUARE_WIDTH + BORDER_WIDTH),
		   y * (SQUARE_HEIGHT + BORDER_WIDTH), 1);
	XFreePixmap(win->display, tmpPM);

	XSetFunction(win->display, DefaultGC(win->display, 0), GXcopy);

    } else if (win->bnw){
	XSetState(win->display, DefaultGC(win->display, 0),
		  BlackPixel(win->display, 0),
		  WhitePixel(win->display, 0), GXcopy, AllPlanes);

	tmpPM = XCreateBitmapFromData(win->display, win->boardwin,
				bits, SQUARE_WIDTH, SQUARE_HEIGHT);
	XCopyPlane(win->display, tmpPM, win->boardwin, DefaultGC(win->display, 0),
		   0, 0, SQUARE_WIDTH, SQUARE_HEIGHT,
		   x * (SQUARE_WIDTH + BORDER_WIDTH),
		   y * (SQUARE_HEIGHT + BORDER_WIDTH), 1);
	XFreePixmap(win->display, tmpPM);
    } else {
	XSetState(win->display, DefaultGC(win->display, 0),
		 ((p->color == WHITE) ? win->whitepiece.pixel :
		  			win->blackpiece.pixel),
		  (iswhite(win, x, y) ? win->whitesquare.pixel :
		   			win->blacksquare.pixel),
		  GXcopy, AllPlanes);
	tmpPM = XCreateBitmapFromData(win->display, win->boardwin,
				bits, SQUARE_WIDTH, SQUARE_HEIGHT);
	XCopyPlane(win->display, tmpPM, win->boardwin, DefaultGC(win->display, 0),
		   0, 0, SQUARE_WIDTH, SQUARE_HEIGHT,
		   x * (SQUARE_WIDTH + BORDER_WIDTH),
		   y * (SQUARE_HEIGHT + BORDER_WIDTH), 1);
	XFreePixmap(win->display, tmpPM);
    }

    if (!record_english) {
	gc.foreground = win->textcolor.pixel;
	if (iswhite(win, x, y) || win->bnw)
	    gc.background = win->whitesquare.pixel;
	else
	    gc.background = win->blacksquare.pixel;

	gc.font = win->small->fid;
	    
	XChangeGC(win->display, DefaultGC(win->display, 0),
		  GCForeground | GCBackground | GCFont, &gc);
	    
	if (!x) {
	    sprintf(buf, " %d", SIZE - y);
	    XDrawImageString(win->display, win->boardwin,
			     DefaultGC(win->display, 0),
			     1, (y + 1) * (SQUARE_HEIGHT + 
					   BORDER_WIDTH) - BORDER_WIDTH + 
			     win->small->max_bounds.ascent - 1, buf, 2);
	}
	if (y == SIZE - 1) {
	    sprintf(buf, "%c", 'A' + x);
	    XDrawImageString(win->display, win->boardwin,
			     DefaultGC(win->display, 0),
			     x * (SQUARE_WIDTH + BORDER_WIDTH) + 1,
			     SIZE * (SQUARE_HEIGHT + BORDER_WIDTH) - BORDER_WIDTH + 
			     win->small->max_bounds.ascent - 1, buf, 1);
	}
    }
    return;
}

void
win_erasepiece(y, x, wnum)
	int y, x;
	color wnum;
{
    windata *win;
    char buf[BSIZE];
    XGCValues gc;
    Pixmap tmpPM;
    
    if (oneboard || (wnum == win1->color))
	win = win1;
    else
	win = win2;
		
    if (win->flipped) {
	y = SIZE - y - 1;
	x = SIZE - x - 1;
    }

    /*
      if (debug)
      fprintf(stderr, "erase square (%d, %d) on board %d\n", y, x,
      wnum);
      */

    if ((x < 0) || (x > 7) || (y < 0) || (y > 7)) exit(1);

    if (win->bnw && !iswhite(win, x, y)) {
	XSetState(win->display, DefaultGC(win->display, 0),
		  BlackPixel(win->display, 0),
		  WhitePixel(win->display, 0), GXcopy, AllPlanes);
	tmpPM = XCreateBitmapFromData(win->display, win->boardwin,
				shade_bits, SQUARE_WIDTH, SQUARE_HEIGHT);

	XCopyPlane(win->display, tmpPM, win->boardwin, DefaultGC(win->display, 0),
		   0, 0, SQUARE_WIDTH, SQUARE_HEIGHT,
		   x * (SQUARE_WIDTH + BORDER_WIDTH),
		   y * (SQUARE_HEIGHT + BORDER_WIDTH), 1);

	XFreePixmap(win->display, tmpPM);
    } else {
	XSetFillStyle(win->display, DefaultGC(win->display, 0),
		      FillSolid);
	XSetForeground(win->display, DefaultGC(win->display, 0),
		       iswhite(win, x, y) ? win->whitesquare.pixel :
		       win->blacksquare.pixel);
	XFillRectangle(win->display, win->boardwin,
		       DefaultGC(win->display, 0),
		       x * (SQUARE_WIDTH + BORDER_WIDTH),
		       y * (SQUARE_HEIGHT + BORDER_WIDTH),
		       SQUARE_WIDTH, SQUARE_HEIGHT);
    }

    if (!record_english) {
	gc.foreground = win->textcolor.pixel;
	if (iswhite(win, x, y) || win->bnw)
	    gc.background = win->whitesquare.pixel;
	else
	    gc.background = win->blacksquare.pixel;

	gc.font = win->small->fid;
	    
	XChangeGC(win->display, DefaultGC(win->display, 0),
		  GCForeground | GCBackground | GCFont, &gc);
	    
	if (!x) {
	    sprintf(buf, " %d", SIZE - y);
	    XDrawImageString(win->display, win->boardwin,
			     DefaultGC(win->display, 0),
			     1, (y + 1) * (SQUARE_HEIGHT + 
					   BORDER_WIDTH) - BORDER_WIDTH + 
			     win->small->max_bounds.ascent - 1, buf, 2);
	}
	if (y == SIZE - 1) {
	    sprintf(buf, "%c", 'A' + x);
	    XDrawImageString(win->display, win->boardwin,
			     DefaultGC(win->display, 0),
			     x * (SQUARE_WIDTH + BORDER_WIDTH) + 1,
			     SIZE * (SQUARE_HEIGHT + BORDER_WIDTH) - BORDER_WIDTH + 
			     win->small->max_bounds.ascent - 1, buf, 1);
	}
    }
    

    return;
}

void
win_flash(m, wnum)
	move *m;
	color wnum;
{
	windata *win;
	int sx, sy, ex, ey, i;

	if (oneboard || (wnum == win1->color))
		win = win1;
	else
		win = win2;
		
	if (win->flipped) {
		sx = SIZE - m->fromx - 1;
		sy = SIZE - m->fromy - 1;
		ex = SIZE - m->tox - 1;
		ey = SIZE - m->toy - 1;
	} else {
		sx = m->fromx;
		sy = m->fromy;
		ex = m->tox;
		ey = m->toy;
	}
	sx = sx * (SQUARE_WIDTH + BORDER_WIDTH) + SQUARE_WIDTH / 2;
	sy = sy * (SQUARE_HEIGHT + BORDER_WIDTH) + SQUARE_HEIGHT / 2;
	ex = ex * (SQUARE_WIDTH + BORDER_WIDTH) + SQUARE_WIDTH / 2;
	ey = ey * (SQUARE_HEIGHT + BORDER_WIDTH) + SQUARE_HEIGHT / 2;

	XSetFunction(win->display, DefaultGC(win->display, 0), GXinvert);
	XSetLineAttributes(win->display, DefaultGC(win->display, 0),
		0, LineSolid, 0, 0);
	for (i = 0; i < num_flashes * 2; i++) {
	    XDrawLine(win->display,win->boardwin,
		      DefaultGC(win->display, 0),
		      sx, sy, ex, ey);
	}
	
	XSetFunction(win->display, DefaultGC(win->display, 0), GXcopy);
	return;
}

/* Handle input from the players. */

void
win_process(quick)
	bool quick;
{
	int i, rfd = 0, wfd = 0, xfd = 0;
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = (quick ? 0 : 500000);

	if (XPending(win1->display))
		service(win1);
	if (!oneboard) {
	    if (XPending(win1->display))
		service(win2);
	}

	if (oneboard)
		rfd = 1 << win1->display->fd;
	else
		rfd = (1 << win1->display->fd) | (1 << win2->display->fd);
	if (!(i = select(32, &rfd, &wfd, &xfd, &timeout)))
		return;
	if (i == -1) {
		perror("select");
		exit(1);
	}
	if (rfd & (1 << win1->display->fd))
		service(win1);
	if (!oneboard && (rfd & (1 << win2->display->fd)))
		service(win2);

	return;
}

static void
service(win)
	windata *win;
{
	XEvent ev;

	while(XPending(win->display)) {
		XNextEvent(win->display, &ev);
		if (TxtFilter(win->display, &ev))
			continue;

		if (ev.xany.window == win->boardwin) {
			switch (ev.type) {
			    case ButtonPress:
				button_pressed(&ev, win);
				break;

			    case ButtonRelease:
				button_released(&ev, win);
				break;

			    case Expose:
				/* Redraw... */
				win_redraw(win, &ev);
				break;

			    case 0:
			    case NoExpose:
				break;
			    default:
				fprintf(stderr, "Bad event type %d\n", ev.type);
				exit(1);
			}
		} else if (ev.xany.window == win->wclockwin) {
			switch (ev.type) {
			    case Expose:
				clock_draw(win, WHITE);
				break;

			    case 0:
			    case NoExpose:
				break;
			    default:
				fprintf(stderr, "Bad event type %d\n", ev.type);
				exit(1);
			}
		} else if (ev.xany.window == win->bclockwin) {
			switch (ev.type) {
			    case Expose:
				clock_draw(win, BLACK);
				break;

			    case 0:
			    case NoExpose:
				break;
			    default:
				fprintf(stderr, "Bad event type %d\n", ev.type);
				exit(1);
			}
		} else if (ev.xany.window == win->jailwin) {
			switch (ev.type) {
			    case Expose:
				jail_draw(win);
				break;

			    case 0:
			    case NoExpose:
				break;
			    default:
				fprintf(stderr, "Bad event type %d\n", ev.type);
				exit(1);
			}
		} else if (ev.xany.window == win->buttonwin) {
			switch (ev.type) {
			    case ButtonPress:
				button_service(win, &ev);
				break;

			    case Expose:
				button_draw(win);
				break;

			    case 0:
			    case NoExpose:
				break;
			    default:
				fprintf(stderr, "Bad event type %d\n", ev.type);
				exit(1);
			}
		} else if (ev.xany.window == win->icon) {
			icon_refresh(win);
		} else if (ev.xany.window == win->basewin) {
			message_send(win, &ev);
		} else {
			fprintf(stderr, "Internal Error: service: bad win\n");
			fprintf(stderr, "window = %d, event = %d\n", ev.xany.window,
					ev.type);
		}
	}
	return;
}

void
win_redraw(win, event)
	windata *win;
	XEvent *event;
{
	XExposeEvent *ev = &event->xexpose;
	int x1, y1, x2, y2, i, j;

	drawgrid(win);
	if (ev) {
		x1 = ev->x / (SQUARE_WIDTH + BORDER_WIDTH);
		y1 = ev->y / (SQUARE_HEIGHT + BORDER_WIDTH);
		x2 = (ev->x + ev->width) / (SQUARE_WIDTH + BORDER_WIDTH);
		y2 = (ev->y + ev->height) / (SQUARE_HEIGHT + BORDER_WIDTH);
	} else {
		x1 = 0;
		y1 = 0;
		x2 = SIZE - 1;
		y2 = SIZE - 1;
	}

	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if (x2 < 0) x2 = 0;
	if (y2 < 0) y2 = 0;
	if (x1 > SIZE - 1) x1 = SIZE - 1;
	if (y1 > SIZE - 1) y1 = SIZE - 1;
	if (x2 > SIZE - 1) x2 = SIZE - 1;
	if (y2 > SIZE - 1) y2 = SIZE - 1;

	if (win->flipped) {
		y1 = SIZE - y2 - 1;
		y2 = SIZE - y1 - 1;
		x1 = SIZE - x2 - 1;
		x2 = SIZE - x1 - 1;
	}

	for (i = x1; i <= x2; i++) 
		for (j = y1; j <= y2; j++) {
			if (chessboard->square[j][i].color == NONE)
				win_erasepiece(j, i, WHITE);
			else
				win_drawpiece(&chessboard->square[j][i], j, i,
						WHITE);
			if (!oneboard) {
				if (chessboard->square[j][i].color == NONE)
					win_erasepiece(j, i, BLACK);
				else
					win_drawpiece(&chessboard->square[j][i],
							j, i, BLACK);
			}
		}
	
	return;
}

static bool
setup(dispname, win)
	char *dispname;
	windata *win;
{
	char buf[BSIZE], *s;
	Pixmap bm, bmask;
	Cursor cur;
	extern char *program, *recfile;
	XSizeHints xsizes;
	

	if (!(win->display = XOpenDisplay(dispname)))
		return (false);
	

	/* Now get boolean defaults... */
	if ((s = XGetDefault(win->display, program, "noisy")) && eq(s, "on"))
		noisyflag = true;
	if ((s = XGetDefault(win->display, program, "savemoves")) && eq(s, "on"))
		saveflag = true;
	if ((s = XGetDefault(win->display, program, "algebraic")) && eq(s, "on"))
		record_english = false;
	if ((s = XGetDefault(win->display, program, "blackandwhite")) && eq(s, "on"))
		bnwflag = true;
	if ((s = XGetDefault(win->display, program, "quickrestore")) && eq(s, "on"))
		quickflag = true;
	if ((s = XGetDefault(win->display, program, "flash")) && eq(s, "on"))
		win_flashmove = true;
	
	/* ... numeric variables ... */
	if (s = XGetDefault(win->display, program, "numflashes"))
		num_flashes = atoi(s);
	if (s = XGetDefault(win->display, program, "flashsize"))
		flash_size = atoi(s);
	
	/* ... and strings. */
	if (s = XGetDefault(win->display, program, "progname"))
		progname = s;
	if (s = XGetDefault(win->display, program, "proghost"))
		proghost = s;
	if (s = XGetDefault(win->display, program, "recordfile"))
		recfile = s;
	if (s = XGetDefault(win->display, program, "blackpiece"))
		black_piece_color = s;
	if (s = XGetDefault(win->display, program, "whitepiece"))
		white_piece_color = s;
	if (s = XGetDefault(win->display, program, "blacksquare"))
		black_square_color = s;
	if (s = XGetDefault(win->display, program, "whitesquare"))
		white_square_color = s;
	if (s = XGetDefault(win->display, program, "bordercolor"))
		border_color = s;
	if (s = XGetDefault(win->display, program, "textcolor"))
		text_color = s;
	if (s = XGetDefault(win->display, program, "textback"))
		text_back = s;
	if (s = XGetDefault(win->display, program, "errortext"))
		error_text = s;
	if (s = XGetDefault(win->display, program, "playertext"))
		player_text = s;
	if (s = XGetDefault(win->display, program, "cursorcolor"))
		cursor_color = s;

	if ((DisplayPlanes(win->display, 0) == 1) || bnwflag)
		win->bnw = true;
	
	/* Allocate colors... */
	if (win->bnw) {
		win->blackpiece.pixel = BlackPixel (win->display, 0);
		win->whitepiece.pixel = WhitePixel (win->display, 0);
		win->blacksquare.pixel = BlackPixel (win->display, 0);
		win->whitesquare.pixel = WhitePixel (win->display, 0);
		win->border.pixel = BlackPixel (win->display, 0);
		win->textcolor.pixel = BlackPixel (win->display, 0);
		win->textback.pixel = WhitePixel (win->display, 0);
		win->playertext.pixel = BlackPixel (win->display, 0);
		win->errortext.pixel = BlackPixel (win->display, 0);
		win->cursorcolor.pixel = BlackPixel (win->display, 0) ;
	} else {
	    if (!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     black_piece_color, &win->blackpiece) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     white_piece_color, &win->whitepiece) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     black_square_color, &win->blacksquare) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     white_square_color, &win->whitesquare) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     border_color, &win->border) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     text_color, &win->textcolor) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     text_back, &win->textback) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     error_text, &win->errortext) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     player_text, &win->playertext) ||  
		!XParseColor(win->display,
			     DefaultColormap(win->display, 0),
			     cursor_color, &win->cursorcolor) ||
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->blackpiece) ||  
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->whitepiece) ||  
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->blacksquare) ||  
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->whitesquare) ||   
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->border) ||  
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->textcolor) ||  
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->textback) ||  
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->errortext) ||  
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->playertext) ||  
		!XAllocColor(win->display,
			     DefaultColormap(win->display, 0),
			     &win->cursorcolor))   
		fprintf(stderr, "Can't get colors...\n");
	}

	/* Get fonts... */
	if ((win->small = XLoadQueryFont(win->display,SMALL_FONT)) ==
	    NULL)
		fprintf(stderr, "Can't get small font...\n");

	if ((win->medium = XLoadQueryFont(win->display,MEDIUM_FONT))
	    == NULL)
	    fprintf(stderr, "Can't get medium font...\n");

	if ((win->large = XLoadQueryFont(win->display,LARGE_FONT)) ==
	    NULL)
	    fprintf(stderr, "Can't get large font...\n");

	
	/* Create the windows... */

	win->basewin =
	    XCreateSimpleWindow(win->display,DefaultRootWindow(win->display),
			  BASE_XPOS, BASE_YPOS, 
			  BASE_WIDTH, BASE_HEIGHT, 0,
			  BlackPixel(win->display, 0),
			  WhitePixel(win->display, 0)); 
	win->boardwin = XCreateSimpleWindow(win->display,win->basewin,
					    BOARD_XPOS, BOARD_YPOS, 
					    BOARD_WIDTH, BOARD_HEIGHT,
					    BORDER_WIDTH,
					    win->border.pixel,
					    WhitePixel(win->display, 0));
	win->recwin = XCreateSimpleWindow(win->display,win->basewin,
					  RECORD_XPOS, RECORD_YPOS,
					  RECORD_WIDTH, RECORD_HEIGHT,
					  BORDER_WIDTH, win->border.pixel,
					  win->textback.pixel);
	win->jailwin = XCreateSimpleWindow(win->display,win->basewin,
					   JAIL_XPOS, JAIL_YPOS,
					   JAIL_WIDTH, JAIL_HEIGHT,
					   BORDER_WIDTH,
					   win->border.pixel,
					   win->textback.pixel);
	win->wclockwin = XCreateSimpleWindow(win->display,win->basewin,
					     WCLOCK_XPOS, WCLOCK_YPOS,
					     CLOCK_WIDTH, CLOCK_HEIGHT,
					     BORDER_WIDTH, win->border.pixel,
					     win->textback.pixel);
	win->bclockwin = XCreateSimpleWindow(win->display,win->basewin,
					     BCLOCK_XPOS, BCLOCK_YPOS,
					     CLOCK_WIDTH, CLOCK_HEIGHT,
					     BORDER_WIDTH, win->border.pixel,
					     win->textback.pixel);
	win->messagewin = XCreateSimpleWindow(win->display,win->basewin,
					      MESS_XPOS, MESS_YPOS,
					      MESS_WIDTH, MESS_HEIGHT,
					      BORDER_WIDTH, win->border.pixel,
					      win->textback.pixel);
	win->buttonwin = XCreateSimpleWindow(win->display,win->basewin,
					     BUTTON_XPOS, BUTTON_YPOS,
					     BUTTON_WIDTH, BUTTON_HEIGHT,
					     BORDER_WIDTH, win->border.pixel,
					     win->textback.pixel);
	
	/* Let's define an icon... */
	win->iconpixmap = XCreatePixmapFromBitmapData(win->display,
						      win->basewin, icon_bits,
						      icon_width, icon_height,
						      win->blacksquare.pixel,
						      win->whitesquare.pixel,
						      1);
	xsizes.flags = PSize | PMinSize | PPosition;
	xsizes.min_width = BASE_WIDTH;
	xsizes.min_height = BASE_HEIGHT;
	xsizes.x = BASE_XPOS;
	xsizes.y = BASE_YPOS;
	XSetStandardProperties(win->display, win->basewin,
			       program, program, win->iconpixmap,
			       0, NULL, &xsizes);
	
	bm = XCreateBitmapFromData(win->display,
				   win->basewin, xchess_bits,
				   xchess_width, xchess_height);
	bmask = XCreateBitmapFromData(win->display,
				   win->basewin, xchess_mask_bits,
				   xchess_width, xchess_height);
	cur = XCreatePixmapCursor(win->display, bm, bmask,
			    &win->cursorcolor,
			    &WhitePixel(win->display, 0),
			    xchess_x_hot, xchess_y_hot);
	XFreePixmap(win->display, bm);
	XFreePixmap(win->display, bmask);
	
	XDefineCursor(win->display,win->basewin, cur);

	XMapSubwindows(win->display,win->basewin);
	XMapRaised(win->display,win->basewin);

	XSelectInput(win->display,win->basewin, KeyPressMask);
	XSelectInput(win->display,win->boardwin,
		     ButtonPressMask | ButtonReleaseMask | ExposureMask);
	XSelectInput(win->display,win->recwin,
		     ButtonReleaseMask | ExposureMask);
	XSelectInput(win->display,win->jailwin, ExposureMask);
	XSelectInput(win->display,win->wclockwin, ExposureMask);
	XSelectInput(win->display,win->bclockwin, ExposureMask);
	XSelectInput(win->display,win->messagewin,
		     ButtonReleaseMask | ExposureMask);
	XSelectInput(win->display,win->buttonwin,
		     ButtonPressMask | ExposureMask);
	
	message_init(win);
	record_init(win);
	button_draw(win);
	jail_init(win);
	clock_init(win, WHITE);
	clock_init(win, BLACK);
	if (timeunit) {
		if (timeunit > 1800)
			sprintf(buf, "%d moves every %.2lg hours.\n",
				movesperunit, ((double) timeunit) / 3600);
		else if (timeunit > 30)
			sprintf(buf, "%d moves every %.2lg minutes.\n",
				movesperunit, ((double) timeunit) / 60);
		else
			sprintf(buf, "%d moves every %d seconds.\n",
				movesperunit, timeunit);
		message_add(win, buf, false);
	}
	return (true);
}

static void
drawgrid(win)
	windata *win;
{
	int i;
	XGCValues gc;

	gc.function = GXcopy;
	gc.plane_mask = AllPlanes;
	gc.foreground = win->border.pixel;
	gc.line_width = 0;
	gc.line_style = LineSolid;
	
	XChangeGC(win->display,
		  DefaultGC(win->display, 0),
		  GCFunction | GCPlaneMask | GCForeground |
		  GCLineWidth | GCLineStyle, &gc);
	
	/* Draw the lines... horizontal, */
	for (i = 1; i < SIZE; i++)
		XDrawLine(win->display, win->boardwin,
			  DefaultGC(win->display, 0), 0,
			  i * (SQUARE_WIDTH + BORDER_WIDTH) -
			      BORDER_WIDTH / 2,
			  SIZE * (SQUARE_WIDTH + BORDER_WIDTH),
			  i * (SQUARE_WIDTH + BORDER_WIDTH) -
			      BORDER_WIDTH / 2);

	/* and vertical... */
	for (i = 1; i < SIZE; i++)
		XDrawLine(win->display, win->boardwin,
			  DefaultGC(win->display, 0),
			  i * (SQUARE_WIDTH + BORDER_WIDTH) -
				BORDER_WIDTH / 2, 0,
			  i * (SQUARE_WIDTH + BORDER_WIDTH) -
			        BORDER_WIDTH / 2, 
			  SIZE * (SQUARE_WIDTH + BORDER_WIDTH));
	return;
}

void
win_restart()
{
	win1->flipped = false;
	win_redraw(win1, (XEvent *) NULL);
	if (!oneboard) {
		win2->flipped = true;
		win_redraw(win2, (XEvent *) NULL);
	}
	return;
}

static void
icon_refresh(win)
	windata *win;
{
	XCopyArea(win->display, win->iconpixmap, win->icon,
		  DefaultGC(win->display, 0),
		  0, 0, icon_width, icon_height, 0, 0);
	return;
}

