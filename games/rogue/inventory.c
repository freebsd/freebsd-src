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
static char sccsid[] = "@(#)inventory.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD$";
#endif /* not lint */

/*
 * inventory.c
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

boolean is_wood[WANDS];
const char *press_space = " --press space to continue--";

const char *const wand_materials[WAND_MATERIALS] = {
	"steel ",
	"bronze ",
	"gold ",
	"silver ",
	"copper ",
	"nickel ",
	"cobalt ",
	"tin ",
	"iron ",
	"magnesium ",
	"chrome ",
	"carbon ",
	"platinum ",
	"silicon ",
	"titanium ",

	"teak ",
	"oak ",
	"cherry ",
	"birch ",
	"pine ",
	"cedar ",
	"redwood ",
	"balsa ",
	"ivory ",
	"walnut ",
	"maple ",
	"mahogany ",
	"elm ",
	"palm ",
	"wooden "
};

const char *const gems[GEMS] = {
	"diamond ",
	"stibotantalite ",
	"lapi-lazuli ",
	"ruby ",
	"emerald ",
	"sapphire ",
	"amethyst ",
	"quartz ",
	"tiger-eye ",
	"opal ",
	"agate ",
	"turquoise ",
	"pearl ",
	"garnet "
};

const char *const syllables[MAXSYLLABLES] = {
	"blech ",
	"foo ",
	"barf ",
	"rech ",
	"bar ",
	"blech ",
	"quo ",
	"bloto ",
	"oh ",
	"caca ",
	"blorp ",
	"erp ",
	"festr ",
	"rot ",
	"slie ",
	"snorf ",
	"iky ",
	"yuky ",
	"ooze ",
	"ah ",
	"bahl ",
	"zep ",
	"druhl ",
	"flem ",
	"behil ",
	"arek ",
	"mep ",
	"zihr ",
	"grit ",
	"kona ",
	"kini ",
	"ichi ",
	"tims ",
	"ogr ",
	"oo ",
	"ighr ",
	"coph ",
	"swerr ",
	"mihln ",
	"poxi "
};

#define COMS 48

struct id_com_s {
	short com_char;
	const char *com_desc;
};

const struct id_com_s com_id_tab[COMS] = {
	'?',	"?       prints help",
	'r',	"r       read scroll",
	'/',	"/       identify object",
	'e',	"e       eat food",
	'h',	"h       left ",
	'w',	"w       wield a weapon",
	'j',	"j       down",
	'W',	"W       wear armor",
	'k',	"k       up",
	'T',	"T       take armor off",
	'l',	"l       right",
	'P',	"P       put on ring",
	'y',	"y       up & left",
	'R',	"R       remove ring",
	'u',	"u       up & right",
	'd',	"d       drop object",
	'b',	"b       down & left",
	'c',	"c       call object",
	'n',	"n       down & right",
	'\0',	"<SHIFT><dir>: run that way",
	')',	")       print current weapon",
	'\0',	"<CTRL><dir>: run till adjacent",
	']',	"]       print current armor",
	'f',	"f<dir>  fight till death or near death",
	'=',	"=       print current rings",
	't',	"t<dir>  throw something",
	'\001',	"^A      print Hp-raise average",
	'm',	"m<dir>  move onto without picking up",
	'z',	"z<dir>  zap a wand in a direction",
	'o',	"o       examine/set options",
	'^',	"^<dir>  identify trap type",
	'\022',	"^R      redraw screen",
	'&',	"&       save screen into 'rogue.screen'",
	's',	"s       search for trap/secret door",
	'\020',	"^P      repeat last message",
	'>',	">       go down a staircase",
	'\033',	"^[      cancel command",
	'<',	"<       go up a staircase",
	'S',	"S       save game",
	'.',	".       rest for a turn",
	'Q',	"Q       quit",
	',',	",       pick something up",
	'!',	"!       shell escape",
	'i',	"i       inventory",
	'F',	"F<dir>  fight till either of you dies",
	'I',	"I       inventory single item",
	'v',	"v       print version number",
	'q',	"q       quaff potion"
};

extern boolean wizard;
extern char *m_names[], *more;

inventory(pack, mask)
const object *pack;
unsigned short mask;
{
	object *obj;
	short i = 0, j, maxlen = 0, n;
	char descs[MAX_PACK_COUNT+1][DCOLS];
	short row, col;

	obj = pack->next_object;

	if (!obj) {
		message("your pack is empty", 0);
		return;
	}
	while (obj) {
		if (obj->what_is & mask) {
			descs[i][0] = ' ';
			descs[i][1] = obj->ichar;
			descs[i][2] = ((obj->what_is & ARMOR) && obj->is_protected)
				? '}' : ')';
			descs[i][3] = ' ';
			get_desc(obj, descs[i]+4);
			if ((n = strlen(descs[i])) > maxlen) {
				maxlen = n;
			}
		i++;
		}
		obj = obj->next_object;
	}
	(void) strcpy(descs[i++], press_space);
	if (maxlen < 27) maxlen = 27;
	col = DCOLS - (maxlen + 2);

	for (row = 0; ((row < i) && (row < DROWS)); row++) {
		if (row > 0) {
			for (j = col; j < DCOLS; j++) {
				descs[row-1][j-col] = mvinch(row, j);
			}
			descs[row-1][j-col] = 0;
		}
		mvaddstr(row, col, descs[row]);
		clrtoeol();
	}
	refresh();
	wait_for_ack();

	move(0, 0);
	clrtoeol();

	for (j = 1; ((j < i) && (j < DROWS)); j++) {
		mvaddstr(j, col, descs[j-1]);
	}
}

id_com()
{
	int ch = 0;
	short i, j, k;

	while (ch != CANCEL) {
		check_message();
		message("Character you want help for (* for all):", 0);

		refresh();
		ch = getchar();

		switch(ch) {
		case LIST:
			{
				char save[(((COMS / 2) + (COMS % 2)) + 1)][DCOLS];
				short rows = (((COMS / 2) + (COMS % 2)) + 1);
				boolean need_two_screens;

				if (rows > LINES) {
					need_two_screens = 1;
					rows = LINES;
				}
				k = 0;

				for (i = 0; i < rows; i++) {
					for (j = 0; j < DCOLS; j++) {
						save[i][j] = mvinch(i, j);
					}
				}
MORE:
				for (i = 0; i < rows; i++) {
					move(i, 0);
					clrtoeol();
				}
				for (i = 0; i < (rows-1); i++) {
					if (i < (LINES-1)) {
						if (((i + i) < COMS) && ((i+i+k) < COMS)) {
							mvaddstr(i, 0, com_id_tab[i+i+k].com_desc);
						}
						if (((i + i + 1) < COMS) && ((i+i+k+1) < COMS)) {
							mvaddstr(i, (DCOLS/2),
										com_id_tab[i+i+k+1].com_desc);
						}
					}
				}
				mvaddstr(rows - 1, 0, need_two_screens ? more : press_space);
				refresh();
				wait_for_ack();

				if (need_two_screens) {
					k += ((rows-1) * 2);
					need_two_screens = 0;
					goto MORE;
				}
				for (i = 0; i < rows; i++) {
					move(i, 0);
					for (j = 0; j < DCOLS; j++) {
						addch(save[i][j]);
					}
				}
			}
			break;
		default:
			if (!pr_com_id(ch)) {
				if (!pr_motion_char(ch)) {
					check_message();
					message("unknown character", 0);
				}
			}
			ch = CANCEL;
			break;
		}
	}
}

pr_com_id(ch)
int ch;
{
	int i;

	if (!get_com_id(&i, ch)) {
		return(0);
	}
	check_message();
	message(com_id_tab[i].com_desc, 0);
	return(1);
}

get_com_id(index, ch)
int *index;
short ch;
{
	short i;

	for (i = 0; i < COMS; i++) {
		if (com_id_tab[i].com_char == ch) {
			*index = i;
			return(1);
		}
	}
	return(0);
}

pr_motion_char(ch)
int ch;
{
	if (	(ch == 'J') ||
			(ch == 'K') ||
			(ch == 'L') ||
			(ch == 'H') ||
			(ch == 'Y') ||
			(ch == 'U') ||
			(ch == 'N') ||
			(ch == 'B') ||
			(ch == '\012') ||
			(ch == '\013') ||
			(ch == '\010') ||
			(ch == '\014') ||
			(ch == '\025') ||
			(ch == '\031') ||
			(ch == '\016') ||
			(ch == '\002')) {
		char until[18], buf[DCOLS];
		int n;

		if (ch <= '\031') {
			ch += 96;
			(void) strcpy(until, "until adjascent");
		} else {
			ch += 32;
			until[0] = '\0';
		}
		(void) get_com_id(&n, ch);
		sprintf(buf, "run %s %s", com_id_tab[n].com_desc + 8, until);
		check_message();
		message(buf, 0);
		return(1);
	} else {
		return(0);
	}
}

mix_colors()
{
	short i, j, k;
	char *t;

	for (i = 0; i <= 32; i++) {
		j = get_rand(0, (POTIONS - 1));
		k = get_rand(0, (POTIONS - 1));
		t = id_potions[j].title;
		id_potions[j].title = id_potions[k].title;
		id_potions[k].title = t;
	}
}

make_scroll_titles()
{
	short i, j, n;
	short sylls, s;

	for (i = 0; i < SCROLS; i++) {
		sylls = get_rand(2, 5);
		(void) strcpy(id_scrolls[i].title, "'");

		for (j = 0; j < sylls; j++) {
			s = get_rand(1, (MAXSYLLABLES-1));
			(void) strcat(id_scrolls[i].title, syllables[s]);
		}
		n = strlen(id_scrolls[i].title);
		(void) strcpy(id_scrolls[i].title+(n-1), "' ");
	}
}

get_desc(obj, desc)
const object *obj;
char *desc;
{
	const char *item_name;
	struct id *id_table;
	char more_info[32];
	short i;

	if (obj->what_is == AMULET) {
		(void) strcpy(desc, "the amulet of Yendor ");
		return;
	}
	item_name = name_of(obj);

	if (obj->what_is == GOLD) {
		sprintf(desc, "%d pieces of gold", obj->quantity);
		return;
	}

	if (obj->what_is != ARMOR) {
		if (obj->quantity == 1) {
			(void) strcpy(desc, "a ");
		} else {
			sprintf(desc, "%d ", obj->quantity);
		}
	}
	if (obj->what_is == FOOD) {
		if (obj->which_kind == RATION) {
			if (obj->quantity > 1) {
				sprintf(desc, "%d rations of ", obj->quantity);
			} else {
				(void) strcpy(desc, "some ");
			}
		} else {
			(void) strcpy(desc, "a ");
		}
		(void) strcat(desc, item_name);
		goto ANA;
	}
	id_table = get_id_table(obj);

	if (wizard) {
		goto ID;
	}
	if (obj->what_is & (WEAPON | ARMOR | WAND | RING)) {
		goto CHECK;
	}

	switch(id_table[obj->which_kind].id_status) {
	case UNIDENTIFIED:
CHECK:
		switch(obj->what_is) {
		case SCROL:
			(void) strcat(desc, item_name);
			(void) strcat(desc, "entitled: ");
			(void) strcat(desc, id_table[obj->which_kind].title);
			break;
		case POTION:
			(void) strcat(desc, id_table[obj->which_kind].title);
			(void) strcat(desc, item_name);
			break;
		case WAND:
		case RING:
			if (obj->identified ||
			(id_table[obj->which_kind].id_status == IDENTIFIED)) {
				goto ID;
			}
			if (id_table[obj->which_kind].id_status == CALLED) {
				goto CALL;
			}
			(void) strcat(desc, id_table[obj->which_kind].title);
			(void) strcat(desc, item_name);
			break;
		case ARMOR:
			if (obj->identified) {
				goto ID;
			}
			(void) strcpy(desc, id_table[obj->which_kind].title);
			break;
		case WEAPON:
			if (obj->identified) {
				goto ID;
			}
			(void) strcat(desc, name_of(obj));
			break;
		}
		break;
	case CALLED:
CALL:	switch(obj->what_is) {
		case SCROL:
		case POTION:
		case WAND:
		case RING:
			(void) strcat(desc, item_name);
			(void) strcat(desc, "called ");
			(void) strcat(desc, id_table[obj->which_kind].title);
			break;
		}
		break;
	case IDENTIFIED:
ID:		switch(obj->what_is) {
		case SCROL:
		case POTION:
			(void) strcat(desc, item_name);
			(void) strcat(desc, id_table[obj->which_kind].real);
			break;
		case RING:
			if (wizard || obj->identified) {
				if ((obj->which_kind == DEXTERITY) ||
					(obj->which_kind == ADD_STRENGTH)) {
					sprintf(more_info, "%s%d ", ((obj->class > 0) ? "+" : ""),
						obj->class);
					(void) strcat(desc, more_info);
				}
			}
			(void) strcat(desc, item_name);
			(void) strcat(desc, id_table[obj->which_kind].real);
			break;
		case WAND:
			(void) strcat(desc, item_name);
			(void) strcat(desc, id_table[obj->which_kind].real);
			if (wizard || obj->identified) {
				sprintf(more_info, "[%d]", obj->class);
				(void) strcat(desc, more_info);
			}
			break;
		case ARMOR:
			sprintf(desc, "%s%d ", ((obj->d_enchant >= 0) ? "+" : ""),
			obj->d_enchant);
			(void) strcat(desc, id_table[obj->which_kind].title);
			sprintf(more_info, "[%d] ", get_armor_class(obj));
			(void) strcat(desc, more_info);
			break;
		case WEAPON:
			sprintf(desc+strlen(desc), "%s%d,%s%d ",
			((obj->hit_enchant >= 0) ? "+" : ""), obj->hit_enchant,
			((obj->d_enchant >= 0) ? "+" : ""), obj->d_enchant);
			(void) strcat(desc, name_of(obj));
			break;
		}
		break;
	}
ANA:
	if (!strncmp(desc, "a ", 2)) {
		if (is_vowel(desc[2])) {
			for (i = strlen(desc) + 1; i > 1; i--) {
				desc[i] = desc[i-1];
			}
			desc[1] = 'n';
		}
	}
	if (obj->in_use_flags & BEING_WIELDED) {
		(void) strcat(desc, "in hand");
	} else if (obj->in_use_flags & BEING_WORN) {
		(void) strcat(desc, "being worn");
	} else if (obj->in_use_flags & ON_LEFT_HAND) {
		(void) strcat(desc, "on left hand");
	} else if (obj->in_use_flags & ON_RIGHT_HAND) {
		(void) strcat(desc, "on right hand");
	}
}

get_wand_and_ring_materials()
{
	short i, j;
	boolean used[WAND_MATERIALS];

	for (i = 0; i < WAND_MATERIALS; i++) {
		used[i] = 0;
	}
	for (i = 0; i < WANDS; i++) {
		do {
			j = get_rand(0, WAND_MATERIALS-1);
		} while (used[j]);
		used[j] = 1;
		(void) strcpy(id_wands[i].title, wand_materials[j]);
		is_wood[i] = (j > MAX_METAL);
	}
	for (i = 0; i < GEMS; i++) {
		used[i] = 0;
	}
	for (i = 0; i < RINGS; i++) {
		do {
			j = get_rand(0, GEMS-1);
		} while (used[j]);
		used[j] = 1;
		(void) strcpy(id_rings[i].title, gems[j]);
	}
}

single_inv(ichar)
short ichar;
{
	short ch;
	char desc[DCOLS];
	object *obj;

	ch = ichar ? ichar : pack_letter("inventory what?", ALL_OBJECTS);

	if (ch == CANCEL) {
		return;
	}
	if (!(obj = get_letter_object(ch))) {
		message("no such item.", 0);
		return;
	}
	desc[0] = ch;
	desc[1] = ((obj->what_is & ARMOR) && obj->is_protected) ? '}' : ')';
	desc[2] = ' ';
	desc[3] = 0;
	get_desc(obj, desc+3);
	message(desc, 0);
}

struct id *
get_id_table(obj)
const object *obj;
{
	switch(obj->what_is) {
	case SCROL:
		return(id_scrolls);
	case POTION:
		return(id_potions);
	case WAND:
		return(id_wands);
	case RING:
		return(id_rings);
	case WEAPON:
		return(id_weapons);
	case ARMOR:
		return(id_armors);
	}
	return((struct id *) 0);
}

inv_armor_weapon(is_weapon)
boolean is_weapon;
{
	if (is_weapon) {
		if (rogue.weapon) {
			single_inv(rogue.weapon->ichar);
		} else {
			message("not wielding anything", 0);
		}
	} else {
		if (rogue.armor) {
			single_inv(rogue.armor->ichar);
		} else {
			message("not wearing anything", 0);
		}
	}
}

id_type()
{
	const char *id;
	int ch;
	char buf[DCOLS];

	message("what do you want identified?", 0);

	ch = rgetchar();

	if ((ch >= 'A') && (ch <= 'Z')) {
		id = m_names[ch-'A'];
	} else if (ch < 32) {
		check_message();
		return;
	} else {
		switch(ch) {
		case '@':
			id = "you";
			break;
		case '%':
			id = "staircase";
			break;
		case '^':
			id = "trap";
			break;
		case '+':
			id = "door";
			break;
		case '-':
		case '|':
			id = "wall of a room";
			break;
		case '.':
			id = "floor";
			break;
		case '#':
			id = "passage";
			break;
		case ' ':
			id = "solid rock";
			break;
		case '=':
			id = "ring";
			break;
		case '?':
			id = "scroll";
			break;
		case '!':
			id = "potion";
			break;
		case '/':
			id = "wand or staff";
			break;
		case ')':
			id = "weapon";
			break;
		case ']':
			id = "armor";
			break;
		case '*':
			id = "gold";
			break;
		case ':':
			id = "food";
			break;
		case ',':
			id = "the Amulet of Yendor";
			break;
		default:
			id = "unknown character";
			break;
		}
	}
	check_message();
	sprintf(buf, "'%c': %s", ch, id);
	message(buf, 0);
}
