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
static char sccsid[] = "@(#)save.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 * save.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#include <stdio.h>
#include "rogue.h"

short write_failed = 0;
char *save_file = (char *) 0;

extern boolean detect_monster;
extern short cur_level, max_level;
extern char hunger_str[];
extern char login_name[];
extern short party_room;
extern short foods;
extern boolean is_wood[];
extern short cur_room;
extern boolean being_held;
extern short bear_trap;
extern short halluc;
extern short blind;
extern short confused;
extern short levitate;
extern short haste_self;
extern boolean see_invisible;
extern boolean detect_monster;
extern boolean wizard;
extern boolean score_only;
extern short m_moves;

extern boolean msg_cleared;

save_game()
{
	char fname[64];

	if (!get_input_line("file name?", save_file, fname, "game not saved",
			0, 1)) {
		return;
	}
	check_message();
	message(fname, 0);
	save_into_file(fname);
}

save_into_file(sfile)
char *sfile;
{
	FILE *fp;
	int file_id;
	char name_buffer[80];
	char *hptr;
	struct rogue_time rt_buf;

	if (sfile[0] == '~') {
		if (hptr = md_getenv("HOME")) {
			(void) strcpy(name_buffer, hptr);
			(void) strcat(name_buffer, sfile+1);
			sfile = name_buffer;
		}
	}
	if (	((fp = fopen(sfile, "w")) == NULL) ||
			((file_id = md_get_file_id(sfile)) == -1)) {
		message("problem accessing the save file", 0);
		return;
	}
	md_ignore_signals();
	write_failed = 0;
	(void) xxx(1);
	r_write(fp, (char *) &detect_monster, sizeof(detect_monster));
	r_write(fp, (char *) &cur_level, sizeof(cur_level));
	r_write(fp, (char *) &max_level, sizeof(max_level));
	write_string(hunger_str, fp);
	write_string(login_name, fp);
	r_write(fp, (char *) &party_room, sizeof(party_room));
	write_pack(&level_monsters, fp);
	write_pack(&level_objects, fp);
	r_write(fp, (char *) &file_id, sizeof(file_id));
	rw_dungeon(fp, 1);
	r_write(fp, (char *) &foods, sizeof(foods));
	r_write(fp, (char *) &rogue, sizeof(fighter));
	write_pack(&rogue.pack, fp);
	rw_id(id_potions, fp, POTIONS, 1);
	rw_id(id_scrolls, fp, SCROLS, 1);
	rw_id(id_wands, fp, WANDS, 1);
	rw_id(id_rings, fp, RINGS, 1);
	r_write(fp, (char *) traps, (MAX_TRAPS * sizeof(trap)));
	r_write(fp, (char *) is_wood, (WANDS * sizeof(boolean)));
	r_write(fp, (char *) &cur_room, sizeof(cur_room));
	rw_rooms(fp, 1);
	r_write(fp, (char *) &being_held, sizeof(being_held));
	r_write(fp, (char *) &bear_trap, sizeof(bear_trap));
	r_write(fp, (char *) &halluc, sizeof(halluc));
	r_write(fp, (char *) &blind, sizeof(blind));
	r_write(fp, (char *) &confused, sizeof(confused));
	r_write(fp, (char *) &levitate, sizeof(levitate));
	r_write(fp, (char *) &haste_self, sizeof(haste_self));
	r_write(fp, (char *) &see_invisible, sizeof(see_invisible));
	r_write(fp, (char *) &detect_monster, sizeof(detect_monster));
	r_write(fp, (char *) &wizard, sizeof(wizard));
	r_write(fp, (char *) &score_only, sizeof(score_only));
	r_write(fp, (char *) &m_moves, sizeof(m_moves));
	md_gct(&rt_buf);
	rt_buf.second += 10;		/* allow for some processing time */
	r_write(fp, (char *) &rt_buf, sizeof(rt_buf));
	fclose(fp);

	if (write_failed) {
		(void) md_df(sfile);	/* delete file */
	} else {
		clean_up("");
	}
}

restore(fname)
char *fname;
{
	FILE *fp;
	struct rogue_time saved_time, mod_time;
	char buf[4];
	char tbuf[40];
	int new_file_id, saved_file_id;

	if (	((new_file_id = md_get_file_id(fname)) == -1) ||
			((fp = fopen(fname, "r")) == NULL)) {
		clean_up("cannot open file");
	}
	if (md_link_count(fname) > 1) {
		clean_up("file has link");
	}
	(void) xxx(1);
	r_read(fp, (char *) &detect_monster, sizeof(detect_monster));
	r_read(fp, (char *) &cur_level, sizeof(cur_level));
	r_read(fp, (char *) &max_level, sizeof(max_level));
	read_string(hunger_str, fp);

	(void) strcpy(tbuf, login_name);
	read_string(login_name, fp);
	if (strcmp(tbuf, login_name)) {
		clean_up("you're not the original player");
	}

	r_read(fp, (char *) &party_room, sizeof(party_room));
	read_pack(&level_monsters, fp, 0);
	read_pack(&level_objects, fp, 0);
	r_read(fp, (char *) &saved_file_id, sizeof(saved_file_id));
	if (new_file_id != saved_file_id) {
		clean_up("sorry, saved game is not in the same file");
	}
	rw_dungeon(fp, 0);
	r_read(fp, (char *) &foods, sizeof(foods));
	r_read(fp, (char *) &rogue, sizeof(fighter));
	read_pack(&rogue.pack, fp, 1);
	rw_id(id_potions, fp, POTIONS, 0);
	rw_id(id_scrolls, fp, SCROLS, 0);
	rw_id(id_wands, fp, WANDS, 0);
	rw_id(id_rings, fp, RINGS, 0);
	r_read(fp, (char *) traps, (MAX_TRAPS * sizeof(trap)));
	r_read(fp, (char *) is_wood, (WANDS * sizeof(boolean)));
	r_read(fp, (char *) &cur_room, sizeof(cur_room));
	rw_rooms(fp, 0);
	r_read(fp, (char *) &being_held, sizeof(being_held));
	r_read(fp, (char *) &bear_trap, sizeof(bear_trap));
	r_read(fp, (char *) &halluc, sizeof(halluc));
	r_read(fp, (char *) &blind, sizeof(blind));
	r_read(fp, (char *) &confused, sizeof(confused));
	r_read(fp, (char *) &levitate, sizeof(levitate));
	r_read(fp, (char *) &haste_self, sizeof(haste_self));
	r_read(fp, (char *) &see_invisible, sizeof(see_invisible));
	r_read(fp, (char *) &detect_monster, sizeof(detect_monster));
	r_read(fp, (char *) &wizard, sizeof(wizard));
	r_read(fp, (char *) &score_only, sizeof(score_only));
	r_read(fp, (char *) &m_moves, sizeof(m_moves));
	r_read(fp, (char *) &saved_time, sizeof(saved_time));

	if (fread(buf, sizeof(char), 1, fp) > 0) {
		clear();
		clean_up("extra characters in file");
	}

	md_gfmt(fname, &mod_time);	/* get file modification time */

	if (has_been_touched(&saved_time, &mod_time)) {
		clear();
		clean_up("sorry, file has been touched");
	}
	if ((!wizard) && !md_df(fname)) {
		clean_up("cannot delete file");
	}
	msg_cleared = 0;
	ring_stats(0);
	fclose(fp);
}

write_pack(pack, fp)
object *pack;
FILE *fp;
{
	object t;

	while (pack = pack->next_object) {
		r_write(fp, (char *) pack, sizeof(object));
	}
	t.ichar = t.what_is = 0;
	r_write(fp, (char *) &t, sizeof(object));
}

read_pack(pack, fp, is_rogue)
object *pack;
FILE *fp;
boolean is_rogue;
{
	object read_obj, *new_obj;

	for (;;) {
		r_read(fp, (char *) &read_obj, sizeof(object));
		if (read_obj.ichar == 0) {
			pack->next_object = (object *) 0;
			break;
		}
		new_obj = alloc_object();
		*new_obj = read_obj;
		if (is_rogue) {
			if (new_obj->in_use_flags & BEING_WORN) {
					do_wear(new_obj);
			} else if (new_obj->in_use_flags & BEING_WIELDED) {
					do_wield(new_obj);
			} else if (new_obj->in_use_flags & (ON_EITHER_HAND)) {
				do_put_on(new_obj,
					((new_obj->in_use_flags & ON_LEFT_HAND) ? 1 : 0));
			}
		}
		pack->next_object = new_obj;
		pack = new_obj;
	}
}

rw_dungeon(fp, rw)
FILE *fp;
boolean rw;
{
	short i, j;
	char buf[DCOLS];

	for (i = 0; i < DROWS; i++) {
		if (rw) {
			r_write(fp, (char *) dungeon[i], (DCOLS * sizeof(dungeon[0][0])));
			for (j = 0; j < DCOLS; j++) {
				buf[j] = mvinch(i, j);
			}
			r_write(fp, buf, DCOLS);
		} else {
			r_read(fp, (char *) dungeon[i], (DCOLS * sizeof(dungeon[0][0])));
			r_read(fp, buf, DCOLS);
			for (j = 0; j < DCOLS; j++) {
				mvaddch(i, j, buf[j]);
			}
		}
	}
}

rw_id(id_table, fp, n, wr)
struct id id_table[];
FILE *fp;
int n;
boolean wr;
{
	short i;

	for (i = 0; i < n; i++) {
		if (wr) {
			r_write(fp, (char *) &(id_table[i].value), sizeof(short));
			r_write(fp, (char *) &(id_table[i].id_status),
				sizeof(unsigned short));
			write_string(id_table[i].title, fp);
		} else {
			r_read(fp, (char *) &(id_table[i].value), sizeof(short));
			r_read(fp, (char *) &(id_table[i].id_status),
				sizeof(unsigned short));
			read_string(id_table[i].title, fp);
		}
	}
}

write_string(s, fp)
char *s;
FILE *fp;
{
	short n;

	n = strlen(s) + 1;
	xxxx(s, n);
	r_write(fp, (char *) &n, sizeof(short));
	r_write(fp, s, n);
}

read_string(s, fp)
char *s;
FILE *fp;
{
	short n;

	r_read(fp, (char *) &n, sizeof(short));
	r_read(fp, s, n);
	xxxx(s, n);
}

rw_rooms(fp, rw)
FILE *fp;
boolean rw;
{
	short i;

	for (i = 0; i < MAXROOMS; i++) {
		rw ? r_write(fp, (char *) (rooms + i), sizeof(room)) :
			r_read(fp, (char *) (rooms + i), sizeof(room));
	}
}

r_read(fp, buf, n)
FILE *fp;
char *buf;
int n;
{
	if (fread(buf, sizeof(char), n, fp) != n) {
		clean_up("read() failed, don't know why");
	}
}

r_write(fp, buf, n)
FILE *fp;
char *buf;
int n;
{
	if (!write_failed) {
		if (fwrite(buf, sizeof(char), n, fp) != n) {
			message("write() failed, don't know why", 0);
			sound_bell();
			write_failed = 1;
		}
	}
}

boolean
has_been_touched(saved_time, mod_time)
struct rogue_time *saved_time, *mod_time;
{
	if (saved_time->year < mod_time->year) {
		return(1);
	} else if (saved_time->year > mod_time->year) {
		return(0);
	}
	if (saved_time->month < mod_time->month) {
		return(1);
	} else if (saved_time->month > mod_time->month) {
		return(0);
	}
	if (saved_time->day < mod_time->day) {
		return(1);
	} else if (saved_time->day > mod_time->day) {
		return(0);
	}
	if (saved_time->hour < mod_time->hour) {
		return(1);
	} else if (saved_time->hour > mod_time->hour) {
		return(0);
	}
	if (saved_time->minute < mod_time->minute) {
		return(1);
	} else if (saved_time->minute > mod_time->minute) {
		return(0);
	}
	if (saved_time->second < mod_time->second) {
		return(1);
	}
	return(0);
}
