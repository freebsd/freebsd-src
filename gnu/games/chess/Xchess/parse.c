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


/* RCS Info: $Revision: 1.2 $ on $Date: 86/11/23 17:17:59 $
 *           $Source: /users/faustus/xchess/RCS/parse.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Parse a sequence of chess moves...
 */

#include "xchess.h"

bool loading_flag = false;
bool loading_paused = false;

static char *line;

/* Load a record file in.  This returns a number of things -- the board, the
 * list of moves, and whose turn it is.
 */

void
load_game(file)
	char *file;
{
	FILE *fp;
	char buf[BSIZE];
	bool eflag;
	move *m;
	board *tmpboard = alloc(board);

	if (eq(file, "xchess.game") && saveflag) {
		message_add(win1,
			"Oops, I just overwrote the\nfile xchess.game...\n",
				true);
		message_add(win1, "I hope you had another copy.\n", true);
		return;
	}
	if (!(fp = fopen(file, "r"))) {
		perror(file);
		return;
	}

	/* Get a few lines... */
	fgets(buf, BSIZE, fp);
	message_add(win1, buf, false);
	if (!oneboard)
		message_add(win2, buf, false);

	fgets(buf, BSIZE, fp);
	message_add(win1, buf, false);
	if (!oneboard)
		message_add(win2, buf, false);
	
	fgets(buf, BSIZE, fp);
	if (eq(buf, "\tenglish\n"))
		eflag = true;
	else if (eq(buf, "\talgebraic\n"))
		eflag = false;
	else {
		fprintf(stderr, "Can't decide whether this is english...\n");
		return;
	}

	board_init(tmpboard);
	line = NULL;
	m = parse_file(fp, tmpboard, eflag);
	tfree(tmpboard);

	/* Now apply these moves to the board we were given... */
	loading_flag = true;
	while (m) {
		if (!quickflag)
			XSync(win1->display, 0);
		win_process(true);
		if (!quickflag)
			sleep(1);
		if (!loading_paused) {
			prog_move(m);
			m = m->next;
		}
	}
	loading_flag = false;
	if (line)
		message_add(win1, line, false);

	while (fgets(buf, BSIZE, fp))
		message_add(win1, buf, false);
	
	fclose(fp);

	return;
}

/* Given a starting position (usually the beginning board configuration),
 * read in a file of moves.
 */

move *
parse_file(fp, b, english)
	FILE *fp;
	board *b;
	bool english;
{
	move *mvs = NULL, *end = NULL;
	char buf[BSIZE], *s, *t;

	while (fgets(buf, BSIZE, fp)) {
		if (*buf == '#')
			continue;
		s = buf;

		/* The move number... */
		if (!(t = gettok(&s)))
			break;
		if (!isdigit(*t)) {
			line = copy(buf);
			break;
		}

		if (!(t = gettok(&s)))
			break;
		if (end)
			end = end->next = (english ? parse_move(b, t, WHITE) :
					parse_imove(b, t, WHITE));
		else
			mvs = end = (english ? parse_move(b, t, WHITE) :
					parse_imove(b, t, WHITE));
		if (!end) {
			fprintf(stderr, "Can't parse %s\n", buf);
			return (NULL);
		}
		board_move(b, end);

		if (!(t = gettok(&s)))
			break;
		if (end)
			end = end->next = (english ? parse_move(b, t, BLACK) :
					parse_imove(b, t, BLACK));
		else
			mvs = end = (english ? parse_move(b, t, BLACK) :
					parse_imove(b, t, BLACK));
		if (!end) {
			fprintf(stderr, "Can't parse %s\n", buf);
			return (NULL);
		}
		board_move(b, end);
	}

	return (mvs);
}

/* Parse a move.  The move format accepted is as follows -
 *	move:		spec-spec
 *	capture:	specxspec
 *	kcastle:	2 o's
 *	qcastle:	3 o's
 * A spec is either piece/pos, piece, or just pos.  A pos consists of a column
 * name followed by a row number.  If the column name is kr, kn, kb, k, q,
 * qb, qn, or qr, then the row number is according to the english system,
 * or if it is a-h then it is according to the international system.
 * 
 *** As of now the spec must include the position.
 */

move *
parse_move(b, str, w)
	board *b;
	char *str;
	color w;
{
	move *m = alloc(move);
	char *s;
	char spec1[16], spec2[16];
	int i, j;

if (debug) fprintf(stderr, "parsing %s\n", str);

	/* Check for castles. */
	for (s = str, i = 0; *s; s++)
		if ((*s == 'o') || (*s == 'O'))
			i++;
	if (i == 2) {
		m->type = KCASTLE;
		m->piece.type = KING;
		m->piece.color = w;
		return (m);
	} else if (i == 3) {
		m->type = QCASTLE;
		m->piece.type = KING;
		m->piece.color = w;
		return (m);
	}
	if (index(str, '-'))
		m->type = MOVE;
	else if (index(str, 'x'))
		m->type = CAPTURE;
	else
		return (NULL);
	for (i = 0; str[i]; i++)
		if ((str[i] == 'x') || (str[i] == '-'))
			break;
		else
			spec1[i] = str[i];
	spec1[i] = '\0';
	for (i++, j = 0; str[i]; i++, j++)
		if ((str[i] == 'x') || (str[i] == '-'))
			break;
		else
			spec2[j] = str[i];
	spec2[j] = '\0';

	/* Now decode the specifications. */
	s = spec1;
	switch (*s) {
	    case 'p': case 'P':
		m->piece.type = PAWN; break;
	    case 'r': case 'R':
		m->piece.type = ROOK; break;
	    case 'n': case 'N':
		m->piece.type = KNIGHT; break;
	    case 'b': case 'B':
		m->piece.type = BISHOP; break;
	    case 'q': case 'Q':
		m->piece.type = QUEEN; break;
	    case 'k': case 'K':
		m->piece.type = KING; break;
	    default:
		return (NULL);
	}
	m->piece.color = w;
	s += 2;

	/* Now get the {q,k}{,b,n,r}n string... */
	if ((s[0] == 'q') && (s[1] == 'r'))
		m->fromx = 0, s += 2;
	else if ((s[0] == 'q') && (s[1] == 'n'))
		m->fromx = 1, s += 2;
	else if ((s[0] == 'q') && (s[1] == 'b'))
		m->fromx = 2, s += 2;
	else if ((s[0] == 'q') && isdigit(s[1]))
		m->fromx = 3, s += 1;
	else if ((s[0] == 'k') && isdigit(s[1]))
		m->fromx = 4, s += 1;
	else if ((s[0] == 'k') && (s[1] == 'b'))
		m->fromx = 5, s += 2;
	else if ((s[0] == 'k') && (s[1] == 'n'))
		m->fromx = 6, s += 2;
	else if ((s[0] == 'k') && (s[1] == 'r'))
		m->fromx = 7, s += 2;
	m->fromy = ((w == WHITE) ? (SIZE - atoi(s)) : (atoi(s) - 1));

	if ((b->square[m->fromy][m->fromx].color != w) ||
		     (b->square[m->fromy][m->fromx].type != m->piece.type)) {
		fprintf(stderr, "Error: bad stuff\n");
		return (NULL);
	}

	s = spec2;
	if (m->type == CAPTURE) {
		switch (*s) {
		    case 'p': case 'P':
			m->taken.type = PAWN; break;
		    case 'r': case 'R':
			m->taken.type = ROOK; break;
		    case 'n': case 'N':
			m->taken.type = KNIGHT; break;
		    case 'b': case 'B':
			m->taken.type = BISHOP; break;
		    case 'q': case 'Q':
			m->taken.type = QUEEN; break;
		    case 'k': case 'K':
			m->taken.type = KING; break;
		    default:
			return (NULL);
		}
		m->taken.color = ((w == WHITE) ? BLACK : WHITE);
		s += 2;
	}

	/* Now get the {q,k}{,b,n,r}n string... */
	if ((s[0] == 'q') && (s[1] == 'r'))
		m->tox = 0, s += 2;
	else if ((s[0] == 'q') && (s[1] == 'n'))
		m->tox = 1, s += 2;
	else if ((s[0] == 'q') && (s[1] == 'b'))
		m->tox = 2, s += 2;
	else if ((s[0] == 'q') && isdigit(s[1]))
		m->tox = 3, s += 1;
	else if ((s[0] == 'k') && isdigit(s[1]))
		m->tox = 4, s += 1;
	else if ((s[0] == 'k') && (s[1] == 'b'))
		m->tox = 5, s += 2;
	else if ((s[0] == 'k') && (s[1] == 'n'))
		m->tox = 6, s += 2;
	else if ((s[0] == 'k') && (s[1] == 'r'))
		m->tox = 7, s += 2;
	m->toy = ((w == WHITE) ? (SIZE - atoi(s)) : (atoi(s) - 1));

	if ((m->type == CAPTURE) && ((b->square[m->toy][m->tox].color !=
			m->taken.color) || (b->square[m->toy][m->tox].type !=
			m->taken.type))) {
		fprintf(stderr, "Error: bad stuff\n");
		return (NULL);
	}

	return (m);
}

/* Parse an algebraic notation move.  This is a lot easier... */

move *
parse_imove(b, buf, w)
	board *b;
	char *buf;
	color w;
{
	char *s;
	move *m = alloc(move);
	int n;

if (debug) fprintf(stderr, "(alg) parsing %s\n", buf);

	for (s = buf, n = 0; *s; s++)
		if ((*s == 'o') || (*s == 'O'))
			n++;
	s = buf;

	if (n == 2)
		m->type = KCASTLE;
	else if (n == 3)
		m->type = QCASTLE;
	else {
		m->fromx = *s++ - 'a';
		m->fromy = SIZE - (*s++ - '0');
		m->tox = *s++ - 'a';
		m->toy = SIZE - (*s++ - '0');
		m->piece = b->square[m->fromy][m->fromx];
		m->taken = b->square[m->toy][m->tox];
		if (m->taken.color == NONE)
		    m->type = MOVE;
		else
		    m->type = CAPTURE;
		/* for pawns we must account for en passant */
		if (m->piece.type == PAWN) {
		    if (m->type == MOVE && m->fromx != m->tox) {
			m->enpassant = 1;
			m->type = CAPTURE;
		    }
		}
	    }

	if (m->piece.color != w) {
		fprintf(stderr, "Error: parse_imove: piece of wrong color!\n");
		return (NULL);
	}
	if ((m->piece.type == KING) && (m->fromy == m->toy) && (m->fromx == 4)
			&& (m->tox == 6))
		m->type = KCASTLE;
	else if ((m->piece.type == KING) && (m->fromy == m->toy) &&
			(m->fromx == 4) && (m->tox == 2))
		m->type = QCASTLE;
	
	return (m);
}

