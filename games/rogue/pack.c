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
static char sccsid[] = "@(#)pack.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 * pack.c
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

char *curse_message = "you can't, it appears to be cursed";

extern short levitate;

object *
add_to_pack(obj, pack, condense)
object *obj, *pack;
{
	object *op;

	if (condense) {
		if (op = check_duplicate(obj, pack)) {
			free_object(obj);
			return(op);
		} else {
			obj->ichar = next_avail_ichar();
		}
	}
	if (pack->next_object == 0) {
		pack->next_object = obj;
	} else {
		op = pack->next_object;

		while (op->next_object) {
			op = op->next_object;
		}
		op->next_object = obj;
	}
	obj->next_object = 0;
	return(obj);
}

take_from_pack(obj, pack)
object *obj, *pack;
{
	while (pack->next_object != obj) {
		pack = pack->next_object;
	}
	pack->next_object = pack->next_object->next_object;
}

/* Note: *status is set to 0 if the rogue attempts to pick up a scroll
 * of scare-monster and it turns to dust.  *status is otherwise set to 1.
 */

object *
pick_up(row, col, status)
short *status;
{
	object *obj;

	*status = 1;

	if (levitate) {
		message("you're floating in the air!", 0);
		return((object *) 0);
	}
	obj = object_at(&level_objects, row, col);
	if (!obj) {
		message("pick_up(): inconsistent", 1);
		return(obj);
	}
	if (	(obj->what_is == SCROL) &&
			(obj->which_kind == SCARE_MONSTER) &&
			obj->picked_up) {
		message("the scroll turns to dust as you pick it up", 0);
		dungeon[row][col] &= (~OBJECT);
		vanish(obj, 0, &level_objects);
		*status = 0;
		if (id_scrolls[SCARE_MONSTER].id_status == UNIDENTIFIED) {
			id_scrolls[SCARE_MONSTER].id_status = IDENTIFIED;
		}
		return((object *) 0);
	}
	if (obj->what_is == GOLD) {
		rogue.gold += obj->quantity;
		dungeon[row][col] &= ~(OBJECT);
		take_from_pack(obj, &level_objects);
		print_stats(STAT_GOLD);
		return(obj);	/* obj will be free_object()ed in caller */
	}
	if (pack_count(obj) >= MAX_PACK_COUNT) {
		message("pack too full", 1);
		return((object *) 0);
	}
	dungeon[row][col] &= ~(OBJECT);
	take_from_pack(obj, &level_objects);
	obj = add_to_pack(obj, &rogue.pack, 1);
	obj->picked_up = 1;
	return(obj);
}

drop()
{
	object *obj, *new;
	short ch;
	char desc[DCOLS];

	if (dungeon[rogue.row][rogue.col] & (OBJECT | STAIRS | TRAP)) {
		message("there's already something there", 0);
		return;
	}
	if (!rogue.pack.next_object) {
		message("you have nothing to drop", 0);
		return;
	}
	if ((ch = pack_letter("drop what?", ALL_OBJECTS)) == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		message("no such item.", 0);
		return;
	}
	if (obj->in_use_flags & BEING_WIELDED) {
		if (obj->is_cursed) {
			message(curse_message, 0);
			return;
		}
		unwield(rogue.weapon);
	} else if (obj->in_use_flags & BEING_WORN) {
		if (obj->is_cursed) {
			message(curse_message, 0);
			return;
		}
		mv_aquatars();
		unwear(rogue.armor);
		print_stats(STAT_ARMOR);
	} else if (obj->in_use_flags & ON_EITHER_HAND) {
		if (obj->is_cursed) {
			message(curse_message, 0);
			return;
		}
		un_put_on(obj);
	}
	obj->row = rogue.row;
	obj->col = rogue.col;

	if ((obj->quantity > 1) && (obj->what_is != WEAPON)) {
		obj->quantity--;
		new = alloc_object();
		*new = *obj;
		new->quantity = 1;
		obj = new;
	} else {
		obj->ichar = 'L';
		take_from_pack(obj, &rogue.pack);
	}
	place_at(obj, rogue.row, rogue.col);
	(void) strcpy(desc, "dropped ");
	get_desc(obj, desc+8);
	message(desc, 0);
	(void) reg_move();
}

object *
check_duplicate(obj, pack)
object *obj, *pack;
{
	object *op;

	if (!(obj->what_is & (WEAPON | FOOD | SCROL | POTION))) {
		return(0);
	}
	if ((obj->what_is == FOOD) && (obj->which_kind == FRUIT)) {
		return(0);
	}
	op = pack->next_object;

	while (op) {
		if ((op->what_is == obj->what_is) && 
			(op->which_kind == obj->which_kind)) {

			if ((obj->what_is != WEAPON) ||
			((obj->what_is == WEAPON) &&
			((obj->which_kind == ARROW) ||
			(obj->which_kind == DAGGER) ||
			(obj->which_kind == DART) ||
			(obj->which_kind == SHURIKEN)) &&
			(obj->quiver == op->quiver))) {
				op->quantity += obj->quantity;
				return(op);
			}
		}
		op = op->next_object;
	}
	return(0);
}

next_avail_ichar()
{
	register object *obj;
	register i;
	boolean ichars[26];

	for (i = 0; i < 26; i++) {
		ichars[i] = 0;
	}
	obj = rogue.pack.next_object;
	while (obj) {
		ichars[(obj->ichar - 'a')] = 1;
		obj = obj->next_object;
	}
	for (i = 0; i < 26; i++) {
		if (!ichars[i]) {
			return(i + 'a');
		}
	}
	return('?');
}

wait_for_ack()
{
	while (rgetchar() != ' ') ;
}

pack_letter(prompt, mask)
char *prompt;
unsigned short mask;
{
	short ch;
	unsigned short tmask = mask;

	if (!mask_pack(&rogue.pack, mask)) {
		message("nothing appropriate", 0);
		return(CANCEL);
	}
	for (;;) {

		message(prompt, 0);

		for (;;) {
			ch = rgetchar();
			if (!is_pack_letter(&ch, &mask)) {
				sound_bell();
			} else {
				break;
			}
		}

		if (ch == LIST) {
			check_message();
			mask = tmask;
			inventory(&rogue.pack, mask);
		} else {
			break;
		}
		mask = tmask;
	}
	check_message();
	return(ch);
}

take_off()
{
	char desc[DCOLS];
	object *obj;

	if (rogue.armor) {
		if (rogue.armor->is_cursed) {
			message(curse_message, 0);
		} else {
			mv_aquatars();
			obj = rogue.armor;
			unwear(rogue.armor);
			(void) strcpy(desc, "was wearing ");
			get_desc(obj, desc+12);
			message(desc, 0);
			print_stats(STAT_ARMOR);
			(void) reg_move();
		}
	} else {
		message("not wearing any", 0);
	}
}

wear()
{
	short ch;
	register object *obj;
	char desc[DCOLS];

	if (rogue.armor) {
		message("your already wearing some", 0);
		return;
	}
	ch = pack_letter("wear what?", ARMOR);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		message("no such item.", 0);
		return;
	}
	if (obj->what_is != ARMOR) {
		message("you can't wear that", 0);
		return;
	}
	obj->identified = 1;
	(void) strcpy(desc, "wearing ");
	get_desc(obj, desc + 8);
	message(desc, 0);
	do_wear(obj);
	print_stats(STAT_ARMOR);
	(void) reg_move();
}

unwear(obj)
object *obj;
{
	if (obj) {
		obj->in_use_flags &= (~BEING_WORN);
	}
	rogue.armor = (object *) 0;
}

do_wear(obj)
object *obj;
{
	rogue.armor = obj;
	obj->in_use_flags |= BEING_WORN;
	obj->identified = 1;
}

wield()
{
	short ch;
	register object *obj;
	char desc[DCOLS];

	if (rogue.weapon && rogue.weapon->is_cursed) {
		message(curse_message, 0);
		return;
	}
	ch = pack_letter("wield what?", WEAPON);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		message("No such item.", 0);
		return;
	}
	if (obj->what_is & (ARMOR | RING)) {
		sprintf(desc, "you can't wield %s",
			((obj->what_is == ARMOR) ? "armor" : "rings"));
		message(desc, 0);
		return;
	}
	if (obj->in_use_flags & BEING_WIELDED) {
		message("in use", 0);
	} else {
		unwield(rogue.weapon);
		(void) strcpy(desc, "wielding ");
		get_desc(obj, desc + 9);
		message(desc, 0);
		do_wield(obj);
		(void) reg_move();
	}
}

do_wield(obj)
object *obj;
{
	rogue.weapon = obj;
	obj->in_use_flags |= BEING_WIELDED;
}

unwield(obj)
object *obj;
{
	if (obj) {
		obj->in_use_flags &= (~BEING_WIELDED);
	}
	rogue.weapon = (object *) 0;
}

call_it()
{
	short ch;
	register object *obj;
	struct id *id_table;
	char buf[MAX_TITLE_LENGTH+2];

	ch = pack_letter("call what?", (SCROL | POTION | WAND | RING));

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		message("no such item.", 0);
		return;
	}
	if (!(obj->what_is & (SCROL | POTION | WAND | RING))) {
		message("surely you already know what that's called", 0);
		return;
	}
	id_table = get_id_table(obj);

	if (get_input_line("call it:","",buf,id_table[obj->which_kind].title,1,1)) {
		id_table[obj->which_kind].id_status = CALLED;
		(void) strcpy(id_table[obj->which_kind].title, buf);
	}
}

pack_count(new_obj)
object *new_obj;
{
	object *obj;
	short count = 0;

	obj = rogue.pack.next_object;

	while (obj) {
		if (obj->what_is != WEAPON) {
			count += obj->quantity;
		} else if (!new_obj) {
			count++;
		} else if ((new_obj->what_is != WEAPON) ||
			((obj->which_kind != ARROW) &&
			(obj->which_kind != DAGGER) &&
			(obj->which_kind != DART) &&
			(obj->which_kind != SHURIKEN)) ||
			(new_obj->which_kind != obj->which_kind) ||
			(obj->quiver != new_obj->quiver)) {
			count++;
		}
		obj = obj->next_object;
	}
	return(count);
}

boolean
mask_pack(pack, mask)
object *pack;
unsigned short mask;
{
	while (pack->next_object) {
		pack = pack->next_object;
		if (pack->what_is & mask) {
			return(1);
		}
	}
	return(0);
}

is_pack_letter(c, mask)
short *c;
unsigned short *mask;
{
	if (((*c == '?') || (*c == '!') || (*c == ':') || (*c == '=') ||
		(*c == ')') || (*c == ']') || (*c == '/') || (*c == ','))) {
		switch(*c) {
		case '?':
			*mask = SCROL;
			break;
		case '!':
			*mask = POTION;
			break;
		case ':':
			*mask = FOOD;
			break;
		case ')':
			*mask = WEAPON;
			break;
		case ']':
			*mask = ARMOR;
			break;
		case '/':
			*mask = WAND;
			break;
		case '=':
			*mask = RING;
			break;
		case ',':
			*mask = AMULET;
			break;
		}
		*c = LIST;
		return(1);
	}
	return(((*c >= 'a') && (*c <= 'z')) || (*c == CANCEL) || (*c == LIST));
}

has_amulet()
{
	return(mask_pack(&rogue.pack, AMULET));
}

kick_into_pack()
{
	object *obj;
	char desc[DCOLS];
	short n, stat;

	if (!(dungeon[rogue.row][rogue.col] & OBJECT)) {
		message("nothing here", 0);
	} else {
		if (obj = pick_up(rogue.row, rogue.col, &stat)) {
			get_desc(obj, desc);
			if (obj->what_is == GOLD) {
				message(desc, 0);
				free_object(obj);
			} else {
				n = strlen(desc);
				desc[n] = '(';
				desc[n+1] = obj->ichar;
				desc[n+2] = ')';
				desc[n+3] = 0;
				message(desc, 0);
			}
		}
		if (obj || (!stat)) {
			(void) reg_move();
		}
	}
}
