/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Timothy C. Stoehr.
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
#if 0
static char sccsid[] = "@(#)room.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/rogue/room.c,v 1.7 1999/11/30 03:49:26 billf Exp $";
#endif /* not lint */

/*
 * room.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include "rogue.h"

room rooms[MAXROOMS];
boolean rooms_visited[MAXROOMS];

extern short blind;
extern boolean detect_monster, jump, passgo, no_skull, ask_quit, flush;
extern char *nick_name, *fruit, *save_file, *press_space;

#define NOPTS 8

struct option {
	const char *prompt;
	boolean is_bool;
	char **strval;
	boolean *bval;
} options[NOPTS] = {
	{
		"Flush typeahead during battle (\"flush\"): ",
		1, (char **) 0, &flush
	},
	{
		"Show position only at end of run (\"jump\"): ",
		1, (char **) 0, &jump
	},
	{
		"Follow turnings in passageways (\"passgo\"): ",
		1, (char **) 0, &passgo
	},
	{
		"Don't print skull when killed (\"noskull\" or \"notombstone\"): ",
		1, (char **) 0, &no_skull
	},
	{
		"Ask player before saying 'Okay, bye-bye!' (\"askquit\"): ",
		1, (char **) 0, &ask_quit
	},
	{
		"Name (\"name\"): ",
		0, &nick_name
	},
	{
		"Fruit (\"fruit\"): ",
		0, &fruit
	},
	{
		"Save file (\"file\"): ",
		0, &save_file
	}
};

light_up_room(rn)
int rn;
{
	short i, j;

	if (!blind) {
		for (i = rooms[rn].top_row;
			i <= rooms[rn].bottom_row; i++) {
			for (j = rooms[rn].left_col;
				j <= rooms[rn].right_col; j++) {
				if (dungeon[i][j] & MONSTER) {
					object *monster;

					if (monster = object_at(&level_monsters, i, j)) {
						dungeon[monster->row][monster->col] &= (~MONSTER);
						monster->trail_char =
							get_dungeon_char(monster->row, monster->col);
						dungeon[monster->row][monster->col] |= MONSTER;
					}
				}
				mvaddch(i, j, get_dungeon_char(i, j));
			}
		}
		mvaddch(rogue.row, rogue.col, rogue.fchar);
	}
}

light_passage(row, col)
int row, col;
{
	short i, j, i_end, j_end;

	if (blind) {
		return;
	}
	i_end = (row < (DROWS-2)) ? 1 : 0;
	j_end = (col < (DCOLS-1)) ? 1 : 0;

	for (i = ((row > MIN_ROW) ? -1 : 0); i <= i_end; i++) {
		for (j = ((col > 0) ? -1 : 0); j <= j_end; j++) {
			if (can_move(row, col, row+i, col+j)) {
				mvaddch(row+i, col+j, get_dungeon_char(row+i, col+j));
			}
		}
	}
}

darken_room(rn)
short rn;
{
	short i, j;

	for (i = rooms[rn].top_row + 1; i < rooms[rn].bottom_row; i++) {
		for (j = rooms[rn].left_col + 1; j < rooms[rn].right_col; j++) {
			if (blind) {
				mvaddch(i, j, ' ');
			} else {
				if (!(dungeon[i][j] & (OBJECT | STAIRS)) &&
					!(detect_monster && (dungeon[i][j] & MONSTER))) {
					if (!imitating(i, j)) {
						mvaddch(i, j, ' ');
					}
					if ((dungeon[i][j] & TRAP) && (!(dungeon[i][j] & HIDDEN))) {
						mvaddch(i, j, '^');
					}
				}
			}
		}
	}
}

get_dungeon_char(row, col)
int row, col;
{
	unsigned short mask = dungeon[row][col];

	if (mask & MONSTER) {
		return(gmc_row_col(row, col));
	}
	if (mask & OBJECT) {
		object *obj;

		obj = object_at(&level_objects, row, col);
		return(get_mask_char(obj->what_is));
	}
	if (mask & (TUNNEL | STAIRS | HORWALL | VERTWALL | FLOOR | DOOR)) {
		if ((mask & (TUNNEL| STAIRS)) && (!(mask & HIDDEN))) {
			return(((mask & STAIRS) ? '%' : '#'));
		}
		if (mask & HORWALL) {
			return('-');
		}
		if (mask & VERTWALL) {
			return('|');
		}
		if (mask & FLOOR) {
			if (mask & TRAP) {
				if (!(dungeon[row][col] & HIDDEN)) {
					return('^');
				}
			}
			return('.');
		}
		if (mask & DOOR) {
			if (mask & HIDDEN) {
				if (((col > 0) && (dungeon[row][col-1] & HORWALL)) ||
					((col < (DCOLS-1)) && (dungeon[row][col+1] & HORWALL))) {
					return('-');
				} else {
					return('|');
				}
			} else {
				return('+');
			}
		}
	}
	return(' ');
}

get_mask_char(mask)
unsigned short mask;
{
		switch(mask) {
		case SCROL:
			return('?');
		case POTION:
			return('!');
		case GOLD:
			return('*');
		case FOOD:
			return(':');
		case WAND:
			return('/');
		case ARMOR:
			return(']');
		case WEAPON:
			return(')');
		case RING:
			return('=');
		case AMULET:
			return(',');
		default:
			return('~');	/* unknown, something is wrong */
		}
}

gr_row_col(row, col, mask)
short *row, *col;
unsigned short mask;
{
	short rn;
	short r, c;

	do {
		r = get_rand(MIN_ROW, DROWS-2);
		c = get_rand(0, DCOLS-1);
		rn = get_room_number(r, c);
	} while ((rn == NO_ROOM) ||
		(!(dungeon[r][c] & mask)) ||
		(dungeon[r][c] & (~mask)) ||
		(!(rooms[rn].is_room & (R_ROOM | R_MAZE))) ||
		((r == rogue.row) && (c == rogue.col)));

	*row = r;
	*col = c;
}

gr_room()
{
	short i;

	do {
		i = get_rand(0, MAXROOMS-1);
	} while (!(rooms[i].is_room & (R_ROOM | R_MAZE)));

	return(i);
}

party_objects(rn)
{
	short i, j, nf = 0;
	object *obj;
	short n, N, row, col;
	boolean found;

	N = ((rooms[rn].bottom_row - rooms[rn].top_row) - 1) *
		((rooms[rn].right_col - rooms[rn].left_col) - 1);
	n =  get_rand(5, 10);
	if (n > N) {
		n = N - 2;
	}
	for (i = 0; i < n; i++) {
		for (j = found = 0; ((!found) && (j < 250)); j++) {
			row = get_rand(rooms[rn].top_row+1,
					   rooms[rn].bottom_row-1);
			col = get_rand(rooms[rn].left_col+1,
					   rooms[rn].right_col-1);
			if ((dungeon[row][col] == FLOOR) || (dungeon[row][col] == TUNNEL)) {
				found = 1;
			}
		}
		if (found) {
			obj = gr_object();
			place_at(obj, row, col);
			nf++;
		}
	}
	return(nf);
}

get_room_number(row, col)
int row, col;
{
	short i;

	for (i = 0; i < MAXROOMS; i++) {
		if ((row >= rooms[i].top_row) && (row <= rooms[i].bottom_row) &&
			(col >= rooms[i].left_col) && (col <= rooms[i].right_col)) {
			return(i);
		}
	}
	return(NO_ROOM);
}

is_all_connected()
{
	short i, starting_room;

	for (i = 0; i < MAXROOMS; i++) {
		rooms_visited[i] = 0;
		if (rooms[i].is_room & (R_ROOM | R_MAZE)) {
			starting_room = i;
		}
	}

	visit_rooms(starting_room);

	for (i = 0; i < MAXROOMS; i++) {
		if ((rooms[i].is_room & (R_ROOM | R_MAZE)) && (!rooms_visited[i])) {
			return(0);
		}
	}
	return(1);
}

visit_rooms(rn)
int rn;
{
	short i;
	short oth_rn;

	rooms_visited[rn] = 1;

	for (i = 0; i < 4; i++) {
		oth_rn = rooms[rn].doors[i].oth_room;
		if ((oth_rn >= 0) && (!rooms_visited[oth_rn])) {
			visit_rooms(oth_rn);
		}
	}
}

draw_magic_map()
{
	short i, j, ch, och;
	unsigned short mask = (HORWALL | VERTWALL | DOOR | TUNNEL | TRAP | STAIRS |
			MONSTER);
	unsigned short s;

	for (i = 0; i < DROWS; i++) {
		for (j = 0; j < DCOLS; j++) {
			s = dungeon[i][j];
			if (s & mask) {
				if (((ch = mvinch(i, j)) == ' ') ||
					((ch >= 'A') && (ch <= 'Z')) || (s & (TRAP | HIDDEN))) {
					och = ch;
					dungeon[i][j] &= (~HIDDEN);
					if (s & HORWALL) {
						ch = '-';
					} else if (s & VERTWALL) {
						ch = '|';
					} else if (s & DOOR) {
						ch = '+';
					} else if (s & TRAP) {
						ch = '^';
					} else if (s & STAIRS) {
						ch = '%';
					} else if (s & TUNNEL) {
						ch = '#';
					} else {
						continue;
					}
					if ((!(s & MONSTER)) || (och == ' ')) {
						addch(ch);
					}
					if (s & MONSTER) {
						object *monster;

						if (monster = object_at(&level_monsters, i, j)) {
							monster->trail_char = ch;
						}
					}
				}
			}
		}
	}
}

dr_course(monster, entering, row, col)
object *monster;
boolean entering;
short row, col;
{
	short i, j, k, rn;
	short r, rr;

	monster->row = row;
	monster->col = col;

	if (mon_sees(monster, rogue.row, rogue.col)) {
		monster->trow = NO_ROOM;
		return;
	}
	rn = get_room_number(row, col);

	if (entering) {		/* entering room */
		/* look for door to some other room */
		r = get_rand(0, MAXROOMS-1);
		for (i = 0; i < MAXROOMS; i++) {
			rr = (r + i) % MAXROOMS;
			if ((!(rooms[rr].is_room & (R_ROOM | R_MAZE))) || (rr == rn)) {
				continue;
			}
			for (k = 0; k < 4; k++) {
				if (rooms[rr].doors[k].oth_room == rn) {
					monster->trow = rooms[rr].doors[k].oth_row;
					monster->tcol = rooms[rr].doors[k].oth_col;
					if ((monster->trow == row) &&
						(monster->tcol == col)) {
						continue;
					}
					return;
				}
			}
		}
		/* look for door to dead end */
		for (i = rooms[rn].top_row; i <= rooms[rn].bottom_row; i++) {
			for (j = rooms[rn].left_col; j <= rooms[rn].right_col; j++) {
				if ((i != monster->row) && (j != monster->col) &&
					(dungeon[i][j] & DOOR)) {
					monster->trow = i;
					monster->tcol = j;
					return;
				}
			}
		}
		/* return monster to room that he came from */
		for (i = 0; i < MAXROOMS; i++) {
			for (j = 0; j < 4; j++) {
				if (rooms[i].doors[j].oth_room == rn) {
					for (k = 0; k < 4; k++) {
						if (rooms[rn].doors[k].oth_room == i) {
							monster->trow = rooms[rn].doors[k].oth_row;
							monster->tcol = rooms[rn].doors[k].oth_col;
							return;
						}
					}
				}
			}
		}
		/* no place to send monster */
		monster->trow = NO_ROOM;
	} else {		/* exiting room */
		if (!get_oth_room(rn, &row, &col)) {
			monster->trow = NO_ROOM;
		} else {
			monster->trow = row;
			monster->tcol = col;
		}
	}
}

get_oth_room(rn, row, col)
short rn, *row, *col;
{
	short d = -1;

	if (*row == rooms[rn].top_row) {
		d = UPWARD/2;
	} else if (*row == rooms[rn].bottom_row) {
		d = DOWN/2;
	} else if (*col == rooms[rn].left_col) {
		d = LEFT/2;
	} else if (*col == rooms[rn].right_col) {
		d = RIGHT/2;
	}
	if ((d != -1) && (rooms[rn].doors[d].oth_room >= 0)) {
		*row = rooms[rn].doors[d].oth_row;
		*col = rooms[rn].doors[d].oth_col;
		return(1);
	}
	return(0);
}

edit_opts()
{
	char save[NOPTS+1][DCOLS];
	short i, j;
	short ch;
	boolean done = 0;
	char buf[MAX_OPT_LEN + 2];

	for (i = 0; i < NOPTS+1; i++) {
		for (j = 0; j < DCOLS; j++) {
			save[i][j] = mvinch(i, j);
		}
		if (i < NOPTS) {
			opt_show(i);
		}
	}
	opt_go(0);
	i = 0;

	while (!done) {
		refresh();
		ch = rgetchar();
CH:
		switch(ch) {
		case '\033':
			done = 1;
			break;
		case '\012':
		case '\015':
			if (i == (NOPTS - 1)) {
				mvaddstr(NOPTS, 0, press_space);
				refresh();
				wait_for_ack();
				done = 1;
			} else {
				i++;
				opt_go(i);
			}
			break;
		case '-':
			if (i > 0) {
				opt_go(--i);
			} else {
				sound_bell();
			}
			break;
		case 't':
		case 'T':
		case 'f':
		case 'F':
			if (options[i].is_bool) {
				*(options[i].bval) = (((ch == 't') || (ch == 'T')) ? 1 : 0);
				opt_show(i);
				opt_go(++i);
				break;
			}
		default:
			if (options[i].is_bool) {
				sound_bell();
				break;
			}
			j = 0;
			if ((ch == '\010') || ((ch >= ' ') && (ch <= '~'))) {
				opt_erase(i);
				do {
					if ((ch >= ' ') && (ch <= '~') && (j < MAX_OPT_LEN)) {
						buf[j++] = ch;
						buf[j] = '\0';
						addch(ch);
					} else if ((ch == '\010') && (j > 0)) {
						buf[--j] = '\0';
						move(i, j + strlen(options[i].prompt));
						addch(' ');
						move(i, j + strlen(options[i].prompt));
					}
					refresh();
					ch = rgetchar();
				} while ((ch != '\012') && (ch != '\015') && (ch != '\033'));
				if (j != 0) {
					(void) strcpy(*(options[i].strval), buf);
				}
				opt_show(i);
				goto CH;
			} else {
				sound_bell();
			}
			break;
		}
	}

	for (i = 0; i < NOPTS+1; i++) {
		move(i, 0);
		for (j = 0; j < DCOLS; j++) {
			addch(save[i][j]);
		}
	}
}

opt_show(i)
int i;
{
	const char *s;
	struct option *opt = &options[i];

	opt_erase(i);

	if (opt->is_bool) {
		s = *(opt->bval) ? "True" : "False";
	} else {
		s = *(opt->strval);
	}
	addstr(s);
}

opt_erase(i)
int i;
{
	struct option *opt = &options[i];

	mvaddstr(i, 0, opt->prompt);
	clrtoeol();
}

opt_go(i)
int i;
{
	move(i, strlen(options[i].prompt));
}

do_shell()
{
#ifdef UNIX
	const char *sh;

	md_ignore_signals();
	if (!(sh = md_getenv("SHELL"))) {
		sh = "/bin/sh";
	}
	move(LINES-1, 0);
	refresh();
	stop_window();
	printf("\nCreating new shell...\n");
	md_shell(sh);
	start_window();
	wrefresh(curscr);
	md_heed_signals();
#endif
}
