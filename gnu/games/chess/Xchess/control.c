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


/* RCS Info: $Revision: 1.4 $ on $Date: 86/11/23 17:17:32 $
 *           $Source: /users/faustus/xchess/RCS/control.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Deal with input from the user.
 */

#include "xchess.h"

move *moves;
move *foremoves;
color nexttomove = WHITE;
bool noisyflag = false;

move *lastmove;
static move *thismove;

static void screen_move();

void
button_pressed(event, win)
	XEvent *event;
	windata *win;
{
	int x, y;
	XKeyEvent *ev = (XKeyEvent *) event;

	if (!oneboard && (win->color != nexttomove)) {
		message_add(win, "Wrong player!\n", true);
		return;
	}
	if (progflag && (nexttomove == (blackflag ? WHITE : BLACK))) {
		message_add(win, "Wait for the computer...\n", true);
		return;
	}
	if (loading_flag) {
		message_add(win, "You'd better not do that now...\n", true);
		return;
	}

	/* Figure out what piece he is pointing at. */
	x = ev->x / (SQUARE_WIDTH + BORDER_WIDTH);
	y = ev->y / (SQUARE_HEIGHT + BORDER_WIDTH);

	if (win->flipped) {
		y = SIZE - y - 1;
		x = SIZE - x - 1;
	}

	if ((x < 0) || (x >= SIZE) || (y < 0) || (y >= SIZE)) {
		fprintf(stderr, "Bad coords (%d, %d)\n", x, y);
		return;
	}

	if (oneboard && (chessboard->square[y][x].color != nexttomove)) {
		message_add(win, "Wrong player!\n", true);
		return;
	} else if (!oneboard && (chessboard->square[y][x].color !=
			win->color)) {
		message_add(win, "Can't move that\n", true);
		return;
	}

	thismove = alloc(move);
	thismove->fromx = x;
	thismove->fromy = y;
	thismove->piece.color = chessboard->square[y][x].color;
	thismove->piece.type = chessboard->square[y][x].type;

	if (debug)
		fprintf(stderr, "%s selected his %s at (%d, %d)...\n",
				colornames[(int) thismove->piece.color],
				piecenames[(int) thismove->piece.type],
				thismove->fromy, thismove->fromx);
	return;
}

void
button_released(event, win)
	XEvent *event;
	windata *win;
{
	int x, y;
	XKeyEvent *ev = (XKeyEvent *) event;

	if (!thismove) {
		/* fprintf(stderr, "Error: button hasn't been pressed\n"); */
		return;
	}
	if (loading_flag)
		return;

	/* Figure out what piece he is pointing at. */
	x = ev->x / (SQUARE_WIDTH + BORDER_WIDTH);
	y = ev->y / (SQUARE_HEIGHT + BORDER_WIDTH);

	if (win->flipped) {
		y = SIZE - y - 1;
		x = SIZE - x - 1;
	}

	if ((x < 0) || (x >= SIZE) || (y < 0) || (y >= SIZE)) {
		fprintf(stderr, "Bad coords (%d, %d)\n", x, y);
		return;
	}

	if ((thismove->fromx == x) && (thismove->fromy == y)) {
		message_add(win, "Hey, you touch it, you move it, buddy.\n",
				true);
		return;
	}
	if (chessboard->square[y][x].color == thismove->piece.color) {
		message_add(win, "Can't put one piece on top of another\n",
				true);
		return;
	}

	thismove->tox = x;
	thismove->toy = y;
	thismove->taken.color = chessboard->square[y][x].color;
	thismove->taken.type = chessboard->square[y][x].type;
	if (thismove->taken.color != NONE)
		thismove->type = CAPTURE;
	else if ((thismove->piece.type == KING) && (thismove->fromx == 4) &&
			(thismove->tox == 6) &&
			(thismove->toy == thismove->fromy))
		thismove->type = KCASTLE;
	else if ((thismove->piece.type == KING) && (thismove->tox == 2) &&
			(thismove->fromx == 4) &&
			(thismove->toy == thismove->fromy))
		thismove->type = QCASTLE;
	else
		thismove->type = MOVE;
	
	/* Now check the en-passant case... */
	if ((thismove->type == MOVE) && ((thismove->tox == thismove->fromx + 1)
			|| (thismove->tox == thismove->fromx - 1)) &&
			(thismove->piece.type == PAWN) && lastmove &&
			(lastmove->tox == lastmove->fromx) && (lastmove->fromx
			== thismove->tox) && ((lastmove->fromy + lastmove->toy)
			/ 2 == thismove->toy)) {
		thismove->type = CAPTURE;
		thismove->enpassant = true;
		thismove->taken = lastmove->piece;
	}

	if (!valid_move(thismove, chessboard)) {
		message_add(win, "Invalid move.\n", true);
		return;
	}

	if (debug)
		fprintf(stderr, "\t... and moved it to (%d, %d), type %s\n",
				thismove->toy, thismove->tox,
				movetypenames[(int) thismove->type]);
	move_piece(thismove);

	if (thismove->check) {
		message_add(win1, "Check.\n", true);
		if (!oneboard) {
			message_add(win2, "Check.\n", true);
		}
	}

	if (!moves)
		moves = lastmove = thismove;
	else
		lastmove = lastmove->next = thismove;

	if (progflag)
		program_send(thismove);

	thismove = NULL;
	nexttomove = ((nexttomove == WHITE) ? BLACK : WHITE);
	clock_switch();

	return;
}

void
prog_move(m)
	move *m;
{
	if (debug)
		fprintf(stderr, "program moves from (%d, %d) to (%d, %d)\n",
				m->fromy, m->fromx, m->toy, m->tox);
	move_piece(m);

	if (!moves)
		moves = lastmove = m;
	else
		lastmove = lastmove->next = m;

	nexttomove = ((nexttomove == WHITE) ? BLACK : WHITE);
	clock_switch();

	return;
}

void
move_piece(m)
	move *m;
{
	/* Update the screen... */
	screen_move(m);

	/* Move the piece on the board... */
	board_move(chessboard, m);

	/* And record it... */
	record_move(m);

	if (noisyflag) {
	    XBell(win1->display, 50);
	    XBell(win2->display, 50);
	}
	return;
}

static void
screen_move(m)
	move *m;
{
	piece pp;

	switch (m->type) {
	    case CAPTURE:
		jail_add(&m->taken);
		/* FALLTHRU */

	    case MOVE:
		win_erasepiece(m->fromy, m->fromx, WHITE);
		if (win_flashmove)
			win_flash(m, WHITE);
		win_drawpiece(&m->piece, m->toy, m->tox, WHITE);
		if (m->enpassant)
			win_erasepiece(m->toy + ((m->piece.color == WHITE) ?
					1 : -1), m->tox, WHITE);
		if (!oneboard) {
			win_erasepiece(m->fromy, m->fromx, BLACK);
			if (win_flashmove)
				win_flash(m, BLACK);
			win_drawpiece(&m->piece, m->toy, m->tox, BLACK);
			if (m->enpassant)
				win_erasepiece(m->toy + ((m->piece.color ==
					WHITE) ? 1 : -1), m->tox, WHITE);
		}
		if ((m->piece.type == PAWN) && (((m->piece.color == BLACK) &&
				(m->toy == 7)) || ((m->piece.color == WHITE) &&
				(m->toy == 0)))) {
			pp.color = m->piece.color;
			pp.type = QUEEN;
			win_drawpiece(&pp,  m->toy, m->tox, WHITE);
			if (!oneboard)
				win_drawpiece(&m->piece, m->toy, m->tox, BLACK);
		}
		break;
	    
	    case KCASTLE:
		if (m->piece.color == WHITE) {
			win_erasepiece(7, 4, WHITE);
			win_erasepiece(7, 7, WHITE);
			if (win_flashmove)
				win_flash(m, WHITE);
			win_drawpiece(&m->piece, 7, 6, WHITE);
			win_drawpiece(&chessboard->square[7][7], 7, 5, WHITE);
			if (!oneboard) {
				win_erasepiece(7, 4, BLACK);
				win_erasepiece(7, 7, BLACK);
				if (win_flashmove)
					win_flash(m, BLACK);
				win_drawpiece(&m->piece, 7, 6, BLACK);
				win_drawpiece(&chessboard->square[7][7], 7, 5,
						BLACK);
			}
		} else {
			win_erasepiece(0, 4, WHITE);
			win_erasepiece(0, 7, WHITE);
			if (win_flashmove)
				win_flash(m, WHITE);
			win_drawpiece(&m->piece, 0, 6, WHITE);
			win_drawpiece(&chessboard->square[0][7], 0, 5, WHITE);
			if (!oneboard) {
				win_erasepiece(0, 4, BLACK);
				win_erasepiece(0, 7, BLACK);
				if (win_flashmove)
					win_flash(m, BLACK);
				win_drawpiece(&m->piece, 0, 6, BLACK);
				win_drawpiece(&chessboard->square[0][7], 0, 5,
						BLACK);
			}
		}
		break;

	    case QCASTLE:
		if (m->piece.color == WHITE) {
			win_erasepiece(7, 4, WHITE);
			win_erasepiece(7, 0, WHITE);
			if (win_flashmove)
				win_flash(m, WHITE);
			win_drawpiece(&m->piece, 7, 2, WHITE);
			win_drawpiece(&chessboard->square[7][0], 7, 3, WHITE);
			if (!oneboard) {
				win_erasepiece(7, 4, BLACK);
				win_erasepiece(7, 0, BLACK);
				if (win_flashmove)
					win_flash(m, BLACK);
				win_drawpiece(&m->piece, 7, 2, BLACK);
				win_drawpiece(&chessboard->square[7][7], 7, 3,
						BLACK);
			}
		} else {
			win_erasepiece(0, 4, WHITE);
			win_erasepiece(0, 0, WHITE);
			if (win_flashmove)
				win_flash(m, WHITE);
			win_drawpiece(&m->piece, 0, 2, WHITE);
			win_drawpiece(&chessboard->square[0][0], 0, 3, WHITE);
			if (!oneboard) {
				win_erasepiece(0, 4, BLACK);
				win_erasepiece(0, 0, BLACK);
				if (win_flashmove)
					win_flash(m, BLACK);
				win_drawpiece(&m->piece, 0, 2, BLACK);
				win_drawpiece(&chessboard->square[0][7], 0, 3,
						BLACK);
			}
		}
		break;

	    default:
		fprintf(stderr, "Bad move type %d\n", m->type);
	}
	return;
}

/* Retract the last move made... */

void
replay()
{
	move *m = lastmove, bm;

	memset(&bm, 0, sizeof(bm));
	switch (m->type) {
	    case MOVE:
		bm.type = MOVE;
		bm.piece = m->piece;
		bm.fromx = m->tox;
		bm.fromy = m->toy;
		bm.tox = m->fromx;
		bm.toy = m->fromy;
		board_move(chessboard, &bm);
		screen_move(&bm);
		break;

	    case CAPTURE:
		bm.type = MOVE;
		bm.piece = m->piece;
		bm.fromx = m->tox;
		bm.fromy = m->toy;
		bm.tox = m->fromx;
		bm.toy = m->fromy;
		board_move(chessboard, &bm);
		screen_move(&bm);
		chessboard->square[m->toy][m->tox] = m->taken;
		bm.piece = m->taken;
		bm.fromx = bm.tox = m->tox;
		bm.fromy = bm.toy = m->toy;
		screen_move(&bm);
		jail_remove(&m->taken);
		break;

	    case KCASTLE:
		bm.type = MOVE;
		bm.piece.type = KING;
		bm.piece.color = m->piece.color;
		bm.fromx = 6;
		bm.tox = 4;
		bm.fromy = bm.toy = (m->piece.color == WHITE) ? 7 : 0;
		board_move(chessboard, &bm);
		screen_move(&bm);
		bm.type = MOVE;
		bm.piece.type = ROOK;
		bm.piece.color = m->piece.color;
		bm.fromx = 5;
		bm.tox = 7;
		bm.fromy = bm.toy = (m->piece.color == WHITE) ? 7 : 0;
		board_move(chessboard, &bm);
		screen_move(&bm);
		if (m->piece.color == WHITE)
			chessboard->white_cant_castle_k = false;
		else
			chessboard->black_cant_castle_k = false;
		break;

	    case QCASTLE:
		bm.type = MOVE;
		bm.piece.type = KING;
		bm.piece.color = m->piece.color;
		bm.fromx = 2;
		bm.tox = 4;
		bm.fromy = bm.toy = (m->piece.color == WHITE) ? 7 : 0;
		board_move(chessboard, &bm);
		screen_move(&bm);
		bm.type = MOVE;
		bm.piece.type = ROOK;
		bm.piece.color = m->piece.color;
		bm.fromx = 3;
		bm.tox = 0;
		bm.fromy = bm.toy = (m->piece.color == WHITE) ? 7 : 0;
		board_move(chessboard, &bm);
		screen_move(&bm);
		if (m->piece.color == WHITE)
			chessboard->white_cant_castle_q = false;
		else
			chessboard->black_cant_castle_q = false;
		break;
	}
	record_back();

	nexttomove = ((nexttomove == WHITE) ? BLACK : WHITE);
	clock_switch();

	if (!moves->next) {
		moves->next = foremoves;
		foremoves = moves;
		moves = lastmove = NULL;
	} else {
		for (m = moves; m->next; m = m->next)
			lastmove = m;
		lastmove->next->next = foremoves;
		foremoves = lastmove->next;
		lastmove->next = NULL;
	}

	if (progflag)
		program_undo();

	return;
}

/* Put back the last move undone. */

void
forward()
{
	prog_move(foremoves);
	foremoves = foremoves->next;
	return;
}

/* End the game. */

void
cleanup(s)
	char *s;
{
	if (progflag)
		program_end();
	record_end(s);
	XSync(win1->display, 0);
	if (!oneboard) {
	    XSync(win2->display, 0);
	}
	exit(0);
}

void
restart()
{
	moves = lastmove = thismove = NULL;
	nexttomove = WHITE;

	clock_init(win1, WHITE);
	clock_init(win1, BLACK);
	jail_init(win1);
	if (!oneboard) {
		clock_init(win2, WHITE);
		clock_init(win2, BLACK);
		jail_init(win2);
	}
	board_init(chessboard);
	win_restart();
	record_reset();
	if (progflag) {
		program_end();
		program_init(progname);
	}
	return;
}

