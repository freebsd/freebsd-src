
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


/* RCS Info: $Revision: 1.3 $ on $Date: 86/11/23 17:18:35 $
 *           $Source: /users/faustus/xchess/RCS/valid.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Validate a move.
 */

#include "xchess.h"

extern bool ischeck(), couldmove();

bool
valid_move(m, b)
	move *m;
	board *b;
{
	board tb;

	/* First check that the piece can make the move at all... */
	if (!couldmove(m, b))
		return (false);

	/* Now see if the king is in check now. */
	bcopy((char *) b, (char *) &tb, sizeof (board));
	board_move(&tb, m);
	if (ischeck(&tb, m->piece.color))
		return (false);
	
	if (ischeck(&tb, ((m->piece.color == WHITE) ? BLACK : WHITE)))
		m->check = true;
	
	return (true);
}

static bool
couldmove(m, b)
	move *m;
	board *b;
{
	int x, y;

	switch (m->type) {
	    case KCASTLE:
		if ((m->piece.color == WHITE) && (b->white_cant_castle_k) ||
				(m->piece.color == BLACK) && 
				(b->black_cant_castle_k))
			return (false);
		if ((b->square[m->fromy][5].color != NONE) ||
				(b->square[m->fromy][6].color != NONE))
			return (false);
		if (ischeck(b, m->piece.color))
			return (false);
		break;

	    case QCASTLE:
		if ((m->piece.color == WHITE) && (b->white_cant_castle_q) ||
				(m->piece.color == BLACK) && 
				(b->black_cant_castle_q))
			return (false);
		if ((b->square[m->fromy][1].color != NONE) ||
				(b->square[m->fromy][2].color != NONE) ||
				(b->square[m->fromy][3].color != NONE))
			return (false);
		if (ischeck(b, m->piece.color))
			return (false);
		break;

	    case MOVE:
	    case CAPTURE:
		/* There is one special case here, that of taking a pawn
		 * en passant.  In this case we change the move field to
		 * CAPTURE if it's ok.
		 */
		switch (m->piece.type) {
		    case PAWN:
			if ((m->type == MOVE) && (m->fromx == m->tox)) {
				/* A normal move. */
				if ((m->piece.color == WHITE) && (m->fromy ==
						m->toy + 1))
					break;
				if ((m->piece.color == WHITE) && (m->fromy ==
						6) && (m->toy == 4) &&
						(b->square[5][m->fromx].color
						== NONE))
					break;
				if ((m->piece.color == BLACK) && (m->fromy ==
						m->toy - 1))
					break;
				if ((m->piece.color == BLACK) && (m->fromy ==
						1) && (m->toy == 3) &&
						(b->square[2][m->fromx].color
						== NONE))
					break;
				return (false);
			} else if (m->type == CAPTURE) {
				if ((((m->piece.color == WHITE) && (m->fromy ==
					    m->toy + 1)) || ((m->piece.color ==
					    BLACK) && (m->fromy == m->toy -
					    1))) && ((m->fromx == m->tox + 1) ||
					    (m->fromx == m->tox - 1)))
					break;
				/* Now maybe it's enpassant...  We've already
				 * checked for some of these things in the
				 * calling routine.
				 */
				if (m->enpassant) {
					if (b->square[(m->piece.color == WHITE)
						    ? 3 : 4][m->tox].color == 
						    ((m->piece.color == WHITE) ?
						    BLACK : WHITE))
						break;
				}
				return (false);
			}
			return (false);

		    case ROOK:
			if (m->fromx == m->tox) {
				for (y = m->fromy + ((m->fromy > m->toy) ? -1 :
						1); y != m->toy; y += ((m->fromy
						> m->toy) ? -1 : 1))
					if (b->square[y][m->tox].color != NONE)
						return (false);
				break;
			}
			if (m->fromy == m->toy) {
				for (x = m->fromx + ((m->fromx > m->tox) ? -1 :
						1); x != m->tox; x += ((m->fromx
						> m->tox) ? -1 : 1))
					if (b->square[m->toy][x].color != NONE)
						return (false);
				break;
			}
			return (false);

		    case KNIGHT:
			x = m->fromx - m->tox;
			y = m->fromy - m->toy;
			if ((((x == 2) || (x == -2)) &&
					((y == 1) || (y == -1))) ||
					(((x == 1) || (x == -1)) &&
					((y == 2) || (y == -2))))
				break;
			return (false);

		    case BISHOP:
			x = m->fromx - m->tox;
			y = m->fromy - m->toy;
			if ((x != y) && (x != - y))
				return (false);
			for (x = m->fromx + ((m->fromx > m->tox) ? -1 : 1), y =
					m->fromy + ((m->fromy > m->toy) ? -1 :
					1); x != m->tox;
					x += ((m->fromx > m->tox) ? -1 : 1),
					y += ((m->fromy > m->toy) ? -1 : 1))
				if (b->square[y][x].color != NONE)
					return (false);
			break;

		    case QUEEN:
			if (m->fromx == m->tox) {
				for (y = m->fromy + ((m->fromy > m->toy) ? -1 :
						1); y != m->toy; y += ((m->fromy
						> m->toy) ? -1 : 1))
					if (b->square[y][m->tox].color != NONE)
						return (false);
				break;
			}
			if (m->fromy == m->toy) {
				for (x = m->fromx + ((m->fromx > m->tox) ? -1 :
						1); x != m->tox; x += ((m->fromx
						> m->tox) ? -1 : 1))
					if (b->square[m->toy][x].color != NONE)
						return (false);
				break;
			}
			x = m->fromx - m->tox;
			y = m->fromy - m->toy;
			if ((x != y) && (x != - y))
				return (false);
			for (x = m->fromx + ((m->fromx > m->tox) ? -1 : 1), y =
					m->fromy + ((m->fromy > m->toy) ? -1 :
					1); x != m->tox;
					x += ((m->fromx > m->tox) ? -1 : 1),
					y += ((m->fromy > m->toy) ? -1 : 1))
				if (b->square[y][x].color != NONE)
					return (false);
			break;

		    case KING:
			x = m->fromx - m->tox;
			y = m->fromy - m->toy;
			if ((x >= -1) && (x <= 1) && (y >= -1) && (y <= 1))
				break;
			return (false);
		}
		break;
	}
	return (true);
}

/* Say whether either king is in check...  If move is non-NULL, say whether he
 * in in check after the move takes place.  We do this in a rather stupid way.
 */

static bool
ischeck(b, col)
	board *b;
	color col;
{
	int x, y, kx, ky;
	move ch;

	for (x = 0; x < SIZE; x++)
		for (y = 0; y < SIZE; y++)
			if ((b->square[y][x].color == col) &&
				    (b->square[y][x].type == KING)) {
				kx = x;
				ky = y;
			}

	for (x = 0; x < SIZE; x++)
		for (y = 0; y < SIZE; y++)
			if (b->square[y][x].color == ((col == WHITE) ?
					BLACK : WHITE)) {
				ch.type = CAPTURE;
				ch.piece.color = b->square[y][x].color;
				ch.piece.type = b->square[y][x].type;
				ch.fromx = x;
				ch.fromy = y;
				ch.tox = kx;
				ch.toy = ky;
				ch.enpassant = false;
				if (couldmove(&ch, b))
					return (true);
			}

	return (false);
}

