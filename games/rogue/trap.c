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
static char sccsid[] = "@(#)trap.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD$";
#endif /* not lint */

/*
 * trap.c
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

trap traps[MAX_TRAPS];
boolean trap_door = 0;
short bear_trap = 0;

const char *const trap_strings[TRAPS * 2] = {
	"trap door",
			"you fell down a trap",
	"bear trap",
			"you are caught in a bear trap",
	"teleport trap",
			"teleport",
	"poison dart trap",
			"a small dart just hit you in the shoulder",
	"sleeping gas trap",
			"a strange white mist envelops you and you fall asleep",
	"rust trap",
			"a gush of water hits you on the head"
};

extern short cur_level, party_room;
extern const char *new_level_message;
extern boolean interrupted;
extern short ring_exp;
extern boolean sustain_strength;
extern short blind;

trap_at(row, col)
int row, col;
{
	short i;

	for (i = 0; ((i < MAX_TRAPS) && (traps[i].trap_type != NO_TRAP)); i++) {
		if ((traps[i].trap_row == row) && (traps[i].trap_col == col)) {
			return(traps[i].trap_type);
		}
	}
	return(NO_TRAP);
}

trap_player(row, col)
short row, col;
{
	short t;

	if ((t = trap_at(row, col)) == NO_TRAP) {
		return;
	}
	dungeon[row][col] &= (~HIDDEN);
	if (rand_percent(rogue.exp + ring_exp)) {
		message("the trap failed", 1);
		return;
	}
	switch(t) {
	case TRAP_DOOR:
		trap_door = 1;
		new_level_message = trap_strings[(t*2)+1];
		break;
	case BEAR_TRAP:
		message(trap_strings[(t*2)+1], 1);
		bear_trap = get_rand(4, 7);
		break;
	case TELE_TRAP:
		mvaddch(rogue.row, rogue.col, '^');
		tele();
		break;
	case DART_TRAP:
		message(trap_strings[(t*2)+1], 1);
		rogue.hp_current -= get_damage("1d6", 1);
		if (rogue.hp_current <= 0) {
			rogue.hp_current = 0;
		}
		if ((!sustain_strength) && rand_percent(40) &&
			(rogue.str_current >= 3)) {
			rogue.str_current--;
		}
		print_stats(STAT_HP | STAT_STRENGTH);
		if (rogue.hp_current <= 0) {
			killed_by((object *) 0, POISON_DART);
		}
		break;
	case SLEEPING_GAS_TRAP:
		message(trap_strings[(t*2)+1], 1);
		take_a_nap();
		break;
	case RUST_TRAP:
		message(trap_strings[(t*2)+1], 1);
		rust((object *) 0);
		break;
	}
}

add_traps()
{
	short i, n, tries = 0;
	short row, col;

	if (cur_level <= 2) {
		n = 0;
	} else if (cur_level <= 7) {
		n = get_rand(0, 2);
	} else if (cur_level <= 11) {
		n = get_rand(1, 2);
	} else if (cur_level <= 16) {
		n = get_rand(2, 3);
	} else if (cur_level <= 21) {
		n = get_rand(2, 4);
	} else if (cur_level <= (AMULET_LEVEL + 2)) {
		n = get_rand(3, 5);
	} else {
		n = get_rand(5, MAX_TRAPS);
	}
	for (i = 0; i < n; i++) {
		traps[i].trap_type = get_rand(0, (TRAPS - 1));

		if ((i == 0) && (party_room != NO_ROOM)) {
			do {
				row = get_rand((rooms[party_room].top_row+1),
						(rooms[party_room].bottom_row-1));
				col = get_rand((rooms[party_room].left_col+1),
						(rooms[party_room].right_col-1));
				tries++;
			} while (((dungeon[row][col] & (OBJECT|STAIRS|TRAP|TUNNEL)) ||
					(dungeon[row][col] == NOTHING)) && (tries < 15));
			if (tries >= 15) {
				gr_row_col(&row, &col, (FLOOR | MONSTER));
			}
		} else {
			gr_row_col(&row, &col, (FLOOR | MONSTER));
		}
		traps[i].trap_row = row;
		traps[i].trap_col = col;
		dungeon[row][col] |= (TRAP | HIDDEN);
	}
}

id_trap()
{
	short dir, row, col, d, t;

	message("direction? ", 0);

	while (!is_direction(dir = rgetchar(), &d)) {
		sound_bell();
	}
	check_message();

	if (dir == CANCEL) {
		return;
	}
	row = rogue.row;
	col = rogue.col;

	get_dir_rc(d, &row, &col, 0);

	if ((dungeon[row][col] & TRAP) && (!(dungeon[row][col] & HIDDEN))) {
		t = trap_at(row, col);
		message(trap_strings[t*2], 0);
	} else {
		message("no trap there", 0);
	}
}

show_traps()
{
	short i, j;

	for (i = 0; i < DROWS; i++) {
		for (j = 0; j < DCOLS; j++) {
			if (dungeon[i][j] & TRAP) {
				mvaddch(i, j, '^');
			}
		}
	}
}

search(n, is_auto)
short n;
boolean is_auto;
{
	short s, i, j, row, col, t;
	short shown = 0, found = 0;
	static boolean reg_search;

	for (i = -1; i <= 1; i++) {
		for (j = -1; j <= 1; j++) {
			row = rogue.row + i;
			col = rogue.col + j;
			if ((row < MIN_ROW) || (row >= (DROWS-1)) ||
					(col < 0) || (col >= DCOLS)) {
				continue;
			}
			if (dungeon[row][col] & HIDDEN) {
				found++;
			}
		}
	}
	for (s = 0; s < n; s++) {
		for (i = -1; i <= 1; i++) {
			for (j = -1; j <= 1; j++) {
				row = rogue.row + i;
				col = rogue.col + j ;
				if ((row < MIN_ROW) || (row >= (DROWS-1)) ||
						(col < 0) || (col >= DCOLS)) {
					continue;
				}
				if (dungeon[row][col] & HIDDEN) {
					if (rand_percent(17 + (rogue.exp + ring_exp))) {
						dungeon[row][col] &= (~HIDDEN);
						if ((!blind) && ((row != rogue.row) ||
								(col != rogue.col))) {
							mvaddch(row, col, get_dungeon_char(row, col));
						}
						shown++;
						if (dungeon[row][col] & TRAP) {
							t = trap_at(row, col);
							message(trap_strings[t*2], 1);
						}
					}
				}
				if (((shown == found) && (found > 0)) || interrupted) {
					return;
				}
			}
		}
		if ((!is_auto) && (reg_search = !reg_search)) {
			(void) reg_move();
		}
	}
}
