
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


/* RCS Info: $Revision: 1.4 $ on $Date: 86/11/23 17:17:15 $
 *           $Source: /users/faustus/xchess/RCS/board.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Stuff to deal with the board.
 */

#include "xchess.h"

board *chessboard;

void
board_setup()
{
	chessboard = alloc(board);
	board_init(chessboard);
	return;
}

void
board_init(b)
	board *b;
{
	int i, j;

	for (i = 0; i < 2; i++)
		for (j = 0; j < SIZE; j++)
			b->square[i][j].color = BLACK;
	for (i = 2; i < 6; i++)
		for (j = 0; j < SIZE; j++)
			b->square[i][j].color = NONE;
	for (i = 6; i < 8; i++)
		for (j = 0; j < SIZE; j++)
			b->square[i][j].color = WHITE;
	for (i = 0; i < SIZE; i++)
		b->square[1][i].type = b->square[6][i].type =
				PAWN;
	b->square[0][0].type = b->square[7][0].type = ROOK;
	b->square[0][1].type = b->square[7][1].type = KNIGHT;
	b->square[0][2].type = b->square[7][2].type = BISHOP;
	b->square[0][3].type = b->square[7][3].type = QUEEN;
	b->square[0][4].type = b->square[7][4].type = KING;
	b->square[0][5].type = b->square[7][5].type = BISHOP;
	b->square[0][6].type = b->square[7][6].type = KNIGHT;
	b->square[0][7].type = b->square[7][7].type = ROOK;
	b->black_cant_castle_k = false;
	b->black_cant_castle_q = false;
	b->white_cant_castle_k = false;
	b->white_cant_castle_q = false;

	return;
}

void
board_drawall()
{
	int i, j;

	for (i = 0; i < SIZE; i++)
		for (j = 0; j < SIZE; j++)
			if (chessboard->square[i][j].color != NONE) {
				win_drawpiece(&chessboard->square[i][j], i,
						j, WHITE);
				if (!oneboard)
					win_drawpiece(&chessboard->square[i][j],
							i, j, BLACK);
			}
	return;
}

void
board_move(b, m)
	board *b;
	move *m;
{
	switch (m->type) {

	    case MOVE:
	    case CAPTURE:
		b->square[m->fromy][m->fromx].color = NONE;
		b->square[m->toy][m->tox].color = m->piece.color;
		b->square[m->toy][m->tox].type = m->piece.type;
		if ((m->piece.type == PAWN) && (((m->piece.color == BLACK) &&
				(m->toy == 7)) || ((m->piece.color == WHITE) &&
				(m->toy == 0))))
			b->square[m->toy][m->tox].type = QUEEN;
		if (m->enpassant)
			b->square[m->toy + ((m->piece.color == WHITE) ? 1 :
					-1)][m->tox].color = NONE;
		break;

	    case KCASTLE:
		if (m->piece.color == WHITE) {
			b->square[7][5].color = m->piece.color;
			b->square[7][5].type = ROOK;
			b->square[7][6].color = m->piece.color;
			b->square[7][6].type = KING;
			b->square[7][4].color = NONE;
			b->square[7][7].color = NONE;
		} else {
			b->square[0][5].color = m->piece.color;
			b->square[0][5].type = ROOK;
			b->square[0][6].color = m->piece.color;
			b->square[0][6].type = KING;
			b->square[0][4].color = NONE;
			b->square[0][7].color = NONE;
		}
		break;

	    case QCASTLE:
		if (m->piece.color == WHITE) {
			b->square[7][3].color = m->piece.color;
			b->square[7][3].type = ROOK;
			b->square[7][2].color = m->piece.color;
			b->square[7][2].type = KING;
			b->square[7][4].color = NONE;
			b->square[7][0].color = NONE;
		} else {
			b->square[0][3].color = m->piece.color;
			b->square[0][3].type = ROOK;
			b->square[0][2].color = m->piece.color;
			b->square[0][2].type = KING;
			b->square[0][4].color = NONE;
			b->square[0][0].color = NONE;
		}
		break;

	    default:
		fprintf(stderr, "Bad move type %d\n", m->type);
	}

	if (m->piece.type == KING) {
		if (m->piece.color == WHITE)
			b->white_cant_castle_q =
					b->white_cant_castle_k= true;
		else
			b->black_cant_castle_q =
					b->black_cant_castle_k= true;
	} else if (m->piece.type == ROOK) {
		if (m->piece.color == WHITE) {
			if (m->fromx == 0)
				b->white_cant_castle_q = true;
			else if (m->fromx == 7) 
				b->white_cant_castle_k = true;
		} else {
			if (m->fromx == 0)
				b->black_cant_castle_q = true;
			else if (m->fromx == 7) 
				b->black_cant_castle_k = true;
		}
	}

	return;
}

