/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)ring.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * ring.c
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

char *left_or_right = "left or right hand?";
char *no_ring = "there's no ring on that hand";
short stealthy;
short r_rings;
short add_strength;
short e_rings;
short regeneration;
short ring_exp;
short auto_search;
boolean r_teleport;
boolean r_see_invisible;
boolean sustain_strength;
boolean maintain_armor;

extern char *curse_message;
extern boolean wizard;

put_on_ring()
{
	short ch;
	char desc[DCOLS];
	object *ring;

	if (r_rings == 2) {
		message("wearing two rings already", 0);
		return;
	}
	if ((ch = pack_letter("put on what?", RING)) == CANCEL) {
		return;
	}
	if (!(ring = get_letter_object(ch))) {
		message("no such item.", 0);
		return;
	}
	if (!(ring->what_is & RING)) {
		message("that's not a ring", 0);
		return;
	}
	if (ring->in_use_flags & (ON_LEFT_HAND | ON_RIGHT_HAND)) {
		message("that ring is already being worn", 0);
		return;
	}
	if (r_rings == 1) {
		ch = (rogue.left_ring ? 'r' : 'l');
	} else {
		message(left_or_right, 0);
		do {
			ch = rgetchar();
		} while ((ch != CANCEL) && (ch != 'l') && (ch != 'r') && (ch != '\n') &&
			 	(ch != '\r'));
	}
	if ((ch != 'l') && (ch != 'r')) {
		check_message();
		return;
	}
	if (((ch == 'l') && rogue.left_ring)||((ch == 'r') && rogue.right_ring)) {
		check_message();
		message("there's already a ring on that hand", 0);
		return;
	}
	if (ch == 'l') {
		do_put_on(ring, 1);
	} else {
		do_put_on(ring, 0);
	}
	ring_stats(1);
	check_message();
	get_desc(ring, desc);
	message(desc, 0);
	(void) reg_move();
}

/*
 * Do not call ring_stats() from within do_put_on().  It will cause
 * serious problems when do_put_on() is called from read_pack() in restore().
 */

do_put_on(ring, on_left)
object *ring;
boolean on_left;
{
	if (on_left) {
		ring->in_use_flags |= ON_LEFT_HAND;
		rogue.left_ring = ring;
	} else {
		ring->in_use_flags |= ON_RIGHT_HAND;
		rogue.right_ring = ring;
	}
}

remove_ring()
{
	boolean left = 0, right = 0;
	short ch;
	char buf[DCOLS];
	object *ring;

	if (r_rings == 0) {
		inv_rings();
	} else if (rogue.left_ring && !rogue.right_ring) {
		left = 1;
	} else if (!rogue.left_ring && rogue.right_ring) {
		right = 1;
	} else {
		message(left_or_right, 0);
		do {
			ch = rgetchar();
		} while ((ch != CANCEL) && (ch != 'l') && (ch != 'r') &&
			(ch != '\n') && (ch != '\r'));
		left = (ch == 'l');
		right = (ch == 'r');
		check_message();
	}
	if (left || right) {
		if (left) {
			if (rogue.left_ring) {
				ring = rogue.left_ring;
			} else {
				message(no_ring, 0);
			}
		} else {
			if (rogue.right_ring) {
				ring = rogue.right_ring;
			} else {
				message(no_ring, 0);
			}
		}
		if (ring->is_cursed) {
			message(curse_message, 0);
		} else {
			un_put_on(ring);
			(void) strcpy(buf, "removed ");
			get_desc(ring, buf + 8);
			message(buf, 0);
			(void) reg_move();
		}
	}
}

un_put_on(ring)
object *ring;
{
	if (ring && (ring->in_use_flags & ON_LEFT_HAND)) {
		ring->in_use_flags &= (~ON_LEFT_HAND);
		rogue.left_ring = 0;
	} else if (ring && (ring->in_use_flags & ON_RIGHT_HAND)) {
		ring->in_use_flags &= (~ON_RIGHT_HAND);
		rogue.right_ring = 0;
	}
	ring_stats(1);
}

gr_ring(ring, assign_wk)
object *ring;
boolean assign_wk;
{
	ring->what_is = RING;
	if (assign_wk) {
		ring->which_kind = get_rand(0, (RINGS - 1));
	}
	ring->class = 0;

	switch(ring->which_kind) {
	/*
	case STEALTH:
		break;
	case SLOW_DIGEST:
		break;
	case REGENERATION:
		break;
	case R_SEE_INVISIBLE:
		break;
	case SUSTAIN_STRENGTH:
		break;
	case R_MAINTAIN_ARMOR:
		break;
	case SEARCHING:
		break;
	*/
	case R_TELEPORT:
		ring->is_cursed = 1;
		break;
	case ADD_STRENGTH:
	case DEXTERITY:
		while ((ring->class = (get_rand(0, 4) - 2)) == 0) ;
		ring->is_cursed = (ring->class < 0);
		break;
	case ADORNMENT:
		ring->is_cursed = coin_toss();
		break;
	}
}

inv_rings()
{
	char buf[DCOLS];

	if (r_rings == 0) {
		message("not wearing any rings", 0);
	} else {
		if (rogue.left_ring) {
			get_desc(rogue.left_ring, buf);
			message(buf, 0);
		}
		if (rogue.right_ring) {
			get_desc(rogue.right_ring, buf);
			message(buf, 0);
		}
	}
	if (wizard) {
		sprintf(buf, "ste %d, r_r %d, e_r %d, r_t %d, s_s %d, a_s %d, reg %d, r_e %d, s_i %d, m_a %d, aus %d",
			stealthy, r_rings, e_rings, r_teleport, sustain_strength,
			add_strength, regeneration, ring_exp, r_see_invisible,
			maintain_armor, auto_search);
		message(buf, 0);
	}
}

ring_stats(pr)
boolean pr;
{
	short i;
	object *ring;

	stealthy = 0;
	r_rings = 0;
	e_rings = 0;
	r_teleport = 0;
	sustain_strength = 0;
	add_strength = 0;
	regeneration = 0;
	ring_exp = 0;
	r_see_invisible = 0;
	maintain_armor = 0;
	auto_search = 0;

	for (i = 0; i < 2; i++) {
		if (!(ring = ((i == 0) ? rogue.left_ring : rogue.right_ring))) {
			continue;
		}
		r_rings++;
		e_rings++;
		switch(ring->which_kind) {
		case STEALTH:
			stealthy++;
			break;
		case R_TELEPORT:
			r_teleport = 1;
			break;
		case REGENERATION:
			regeneration++;
			break;
		case SLOW_DIGEST:
			e_rings -= 2;
			break;
		case ADD_STRENGTH:
			add_strength += ring->class;
			break;
		case SUSTAIN_STRENGTH:
			sustain_strength = 1;
			break;
		case DEXTERITY:
			ring_exp += ring->class;
			break;
		case ADORNMENT:
			break;
		case R_SEE_INVISIBLE:
			r_see_invisible = 1;
			break;
		case MAINTAIN_ARMOR:
			maintain_armor = 1;
			break;
		case SEARCHING:
			auto_search += 2;
			break;
		}
	}
	if (pr) {
		print_stats(STAT_STRENGTH);
		relight();
	}
}
