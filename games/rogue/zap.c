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
static char sccsid[] = "@(#)zap.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/rogue/zap.c,v 1.3 1999/11/30 03:49:29 billf Exp $";
#endif /* not lint */

/*
 * zap.c
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

boolean wizard = 0;

extern boolean being_held, score_only, detect_monster;
extern short cur_room;

zapp()
{
	short wch;
	boolean first_miss = 1;
	object *wand;
	short dir, d, row, col;
	object *monster;

	while (!is_direction(dir = rgetchar(), &d)) {
		sound_bell();
		if (first_miss) {
			message("direction? ", 0);
			first_miss = 0;
		}
	}
	check_message();
	if (dir == CANCEL) {
		return;
	}
	if ((wch = pack_letter("zap with what?", WAND)) == CANCEL) {
		return;
	}
	check_message();

	if (!(wand = get_letter_object(wch))) {
		message("no such item.", 0);
		return;
	}
	if (wand->what_is != WAND) {
		message("you can't zap with that", 0);
		return;
	}
	if (wand->class <= 0) {
		message("nothing happens", 0);
	} else {
		wand->class--;
		row = rogue.row; col = rogue.col;
		if ((wand->which_kind == COLD) || (wand->which_kind == FIRE)) {
			bounce((short) wand->which_kind, d, row, col, 0);
		} else {
			monster = get_zapped_monster(d, &row, &col);
			if (wand->which_kind == DRAIN_LIFE) {
				wdrain_life(monster);
			} else if (monster) {
				wake_up(monster);
				s_con_mon(monster);
				zap_monster(monster, wand->which_kind);
				relight();
			}
		}
	}
	(void) reg_move();
}

object *
get_zapped_monster(dir, row, col)
short dir;
short *row, *col;
{
	short orow, ocol;

	for (;;) {
		orow = *row; ocol = *col;
		get_dir_rc(dir, row, col, 0);
		if (((*row == orow) && (*col == ocol)) ||
		   (dungeon[*row][*col] & (HORWALL | VERTWALL)) ||
		   (dungeon[*row][*col] == NOTHING)) {
			return(0);
		}
		if (dungeon[*row][*col] & MONSTER) {
			if (!imitating(*row, *col)) {
				return(object_at(&level_monsters, *row, *col));
			}
		}
	}
}

zap_monster(monster, kind)
object *monster;
unsigned short kind;
{
	short row, col;
	object *nm;
	short tc;

	row = monster->row;
	col = monster->col;

	switch(kind) {
	case SLOW_MONSTER:
		if (monster->m_flags & HASTED) {
			monster->m_flags &= (~HASTED);
		} else {
			monster->slowed_toggle = 0;
			monster->m_flags |= SLOWED;
		}
		break;
	case HASTE_MONSTER:
		if (monster->m_flags & SLOWED) {
			monster->m_flags &= (~SLOWED);
		} else {
			monster->m_flags |= HASTED;
		}
		break;
	case TELE_AWAY:
		tele_away(monster);
		break;
	case INVISIBILITY:
		monster->m_flags |= INVISIBLE;
		break;
	case POLYMORPH:
		if (monster->m_flags & HOLDS) {
			being_held = 0;
		}
		nm = monster->next_monster;
		tc = monster->trail_char;
		(void) gr_monster(monster, get_rand(0, MONSTERS-1));
		monster->row = row;
		monster->col = col;
		monster->next_monster = nm;
		monster->trail_char = tc;
		if (!(monster->m_flags & IMITATES)) {
			wake_up(monster);
		}
		break;
	case MAGIC_MISSILE:
		rogue_hit(monster, 1);
		break;
	case CANCELLATION:
		if (monster->m_flags & HOLDS) {
			being_held = 0;
		}
		if (monster->m_flags & STEALS_ITEM) {
			monster->drop_percent = 0;
		}
		monster->m_flags &= (~(FLIES | FLITS | SPECIAL_HIT | INVISIBLE |
			FLAMES | IMITATES | CONFUSES | SEEKS_GOLD | HOLDS));
		break;
	case DO_NOTHING:
		message("nothing happens", 0);
		break;
	}
}

tele_away(monster)
object *monster;
{
	short row, col;

	if (monster->m_flags & HOLDS) {
		being_held = 0;
	}
	gr_row_col(&row, &col, (FLOOR | TUNNEL | STAIRS | OBJECT));
	mvaddch(monster->row, monster->col, monster->trail_char);
	dungeon[monster->row][monster->col] &= ~MONSTER;
	monster->row = row; monster->col = col;
	dungeon[row][col] |= MONSTER;
	monster->trail_char = mvinch(row, col);
	if (detect_monster || rogue_can_see(row, col)) {
		mvaddch(row, col, gmc(monster));
	}
}

wizardize()
{
	char buf[100];

	if (wizard) {
		wizard = 0;
		message("not wizard anymore", 0);
	} else {
		if (get_input_line("wizard's password:", "", buf, "", 0, 0)) {
			(void) xxx(1);
			xxxx(buf, strlen(buf));
			if (!strncmp(buf, "\247\104\126\272\115\243\027", 7)) {
				wizard = 1;
				score_only = 1;
				message("Welcome, mighty wizard!", 0);
			} else {
				message("sorry", 0);
			}
		}
	}
}

wdrain_life(monster)
object *monster;
{
	short hp;
	object *lmon, *nm;

	hp = rogue.hp_current / 3;
	rogue.hp_current = (rogue.hp_current + 1) / 2;

	if (cur_room >= 0) {
		lmon = level_monsters.next_monster;
		while (lmon) {
			nm = lmon->next_monster;
			if (get_room_number(lmon->row, lmon->col) == cur_room) {
				wake_up(lmon);
				(void) mon_damage(lmon, hp);
			}
			lmon = nm;
		}
	} else {
		if (monster) {
			wake_up(monster);
			(void) mon_damage(monster, hp);
		}
	}
	print_stats(STAT_HP);
	relight();
}

bounce(ball, dir, row, col, r)
short ball, dir, row, col, r;
{
	short orow, ocol;
	char buf[DCOLS];
	const char *s;
	short i, ch, new_dir = -1, damage;
	static short btime;

	if (++r == 1) {
		btime = get_rand(3, 6);
	} else if (r > btime) {
		return;
	}

	if (ball == FIRE) {
		s = "fire";
	} else {
		s = "ice";
	}
	if (r > 1) {
		sprintf(buf, "the %s bounces", s);
		message(buf, 0);
	}
	orow = row;
	ocol = col;
	do {
		ch = mvinch(orow, ocol);
		standout();
		mvaddch(orow, ocol, ch);
		get_dir_rc(dir, &orow, &ocol, 1);
	} while (!(	(ocol <= 0) ||
				(ocol >= DCOLS-1) ||
				(dungeon[orow][ocol] == NOTHING) ||
				(dungeon[orow][ocol] & MONSTER) ||
				(dungeon[orow][ocol] & (HORWALL | VERTWALL)) ||
				((orow == rogue.row) && (ocol == rogue.col))));
	standend();
	refresh();
	do {
		orow = row;
		ocol = col;
		ch = mvinch(row, col);
		mvaddch(row, col, ch);
		get_dir_rc(dir, &row, &col, 1);
	} while (!(	(col <= 0) ||
				(col >= DCOLS-1) ||
				(dungeon[row][col] == NOTHING) ||
				(dungeon[row][col] & MONSTER) ||
				(dungeon[row][col] & (HORWALL | VERTWALL)) ||
				((row == rogue.row) && (col == rogue.col))));

	if (dungeon[row][col] & MONSTER) {
		object *monster;

		monster = object_at(&level_monsters, row, col);

		wake_up(monster);
		if (rand_percent(33)) {
			sprintf(buf, "the %s misses the %s", s, mon_name(monster));
			message(buf, 0);
			goto ND;
		}
		if (ball == FIRE) {
			if (!(monster->m_flags & RUSTS)) {
				if (monster->m_flags & FREEZES) {
					damage = monster->hp_to_kill;
				} else if (monster->m_flags & FLAMES) {
					damage = (monster->hp_to_kill / 10) + 1;
				} else {
					damage = get_rand((rogue.hp_current / 3), rogue.hp_max);
				}
			} else {
				damage = (monster->hp_to_kill / 2) + 1;
			}
			sprintf(buf, "the %s hits the %s", s, mon_name(monster));
			message(buf, 0);
			(void) mon_damage(monster, damage);
		} else {
			damage = -1;
			if (!(monster->m_flags & FREEZES)) {
				if (rand_percent(33)) {
					message("the monster is frozen", 0);
					monster->m_flags |= (ASLEEP | NAPPING);
					monster->nap_length = get_rand(3, 6);
				} else {
					damage = rogue.hp_current / 4;
				}
			} else {
				damage = -2;
			}
			if (damage != -1) {
				sprintf(buf, "the %s hits the %s", s, mon_name(monster));
				message(buf, 0);
				(void) mon_damage(monster, damage);
			}
		}
	} else if ((row == rogue.row) && (col == rogue.col)) {
		if (rand_percent(10 + (3 * get_armor_class(rogue.armor)))) {
			sprintf(buf, "the %s misses", s);
			message(buf, 0);
			goto ND;
		} else {
			damage = get_rand(3, (3 * rogue.exp));
			if (ball == FIRE) {
				damage = (damage * 3) / 2;
				damage -= get_armor_class(rogue.armor);
			}
			sprintf(buf, "the %s hits", s);
			rogue_damage(damage, (object *) 0,
					((ball == FIRE) ? KFIRE : HYPOTHERMIA));
			message(buf, 0);
		}
	} else {
		short nrow, ncol;

ND:		for (i = 0; i < 10; i++) {
			dir = get_rand(0, DIRS-1);
			nrow = orow;
			ncol = ocol;
			get_dir_rc(dir, &nrow, &ncol, 1);
			if (((ncol >= 0) && (ncol <= DCOLS-1)) &&
				(dungeon[nrow][ncol] != NOTHING) &&
				(!(dungeon[nrow][ncol] & (VERTWALL | HORWALL)))) {
				new_dir = dir;
				break;
			}
		}
		if (new_dir != -1) {
			bounce(ball, new_dir, orow, ocol, r);
		}
	}
}
