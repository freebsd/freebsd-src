/*
 * Copyright (c) 1995
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Paul Richards.
 * 4. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Low level ncurses support routines.
 */

#include <forms.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ttyent.h>

#include "internal.h"

/* ncurses key mappings */
/* XXX -- need to look at implementing key mapping properly */
#define K_UPL 	KEY_UP
#define K_DOWNL		KEY_DOWN
#define K_RIGHTL	9
#define K_LEFTL		8
#define K_NEXTL		10
#define K_LEFT		KEY_LEFT
#define K_RIGHT		KEY_RIGHT
#define K_HOME		KEY_HOME
#define K_END		KEY_END
#define K_BS		263
#define K_DEL		330
#define K_ACCEPT	K_NEXTL

/* Function declarations */
DISPLAY *ncurses_open(DISPLAY *);
void ncurses_moveto(OBJECT *);
int ncurses_print_srtring(OBJECT *, char *);

extern OBJECT *cur_obj;

DISPLAY *
ncurses_open(DISPLAY *display)
{
	struct ttyent *tty;
	NCURSDEV *device = display->device.ncurses;
	FILE *in, *out;

	if (device) {
		tty = getttynam(device->ttyname);
		if (!tty)
			return (0);

		in = fopen(device->input, "r");
		out = fopen(device->output, "w+");
		if ((!in) || (!out))
			return (0);
		device->screen = newterm(tty->ty_type, in, out);
	} else {
		display->device.ncurses = malloc(sizeof (NCURSDEV));
		device = display->device.ncurses;
		if (!device)
			return (0);
		device->screen = newterm(getenv("TERM"), stdout, stdin);
	}

	if (!device->screen)
		return (0);

	start_color();
	cbreak();
	noecho();

	/* If we got here by default, set device type to ncurses */
	if (display->type == DT_ANY)
		display->type = DT_NCURSES;


	display->height = LINES;
	display->width = COLS;

	return (display);
}

ncurses_set_display(DISPLAY *display)
{
	set_term(display->device.ncurses->screen);
}

ncurses_open_window(OBJECT *object)
{
	object->window.ncurses->win =
		newwin(object->height, object->width, object->y, object->x);
	if (!object->window.ncurses->win)
		errx(-1, "Couldn't open window (%d)", lineno);
	if (keypad(object->window.ncurses->win, TRUE) == ERR)
		errx(-1, "Keypad call failed (%d)", lineno);
}

void
ncurses_refresh_display(DISPLAY *display)
{
	doupdate();
}

/*
 * Parse an attribute string. For ncurses we look up the display
 * specific bindings table for the attribute strings. For ncurses
 * the attribute entry is a simple integer which is passed to wattron.
 */

int
ncurses_parse_attrs(OBJECT *object, char *string)
{
	hash_table *htable;
	TUPLE *tuple;
	int inc = 1;
	char *attribute = 0;
	AttrType attr_type;
	int len, y, x;
	int skip = 0;
		
	if ((!string) || (*string != '\\'))
		return (0);

	do {
		if (*(string + inc) == '\\')
			return (skip);

		while ((!isspace(*(string + inc))) 
			   && (*(string+inc) != '\\')
			   && (*(string + inc) != '\0'))
			inc++;

		attribute = malloc(inc);
		if (!attribute)
			errx(-1, "Failed to allocate memory for attribute when parsing string");
		strncpy(attribute, string+1, inc-1);
		attribute[inc-1] = 0;

#ifdef no
		/* Skip trailing space after the attribute string */
		while (isspace(*(string + inc)))
			inc++;
#endif

		attr_type = parse_default_attributes(attribute);
		free(attribute);

		switch (attr_type) {
			case ATTR_CENTER:
				len = calc_string_width(string+inc);
				getyx(object->window.ncurses->win, y, x);
				wmove(object->window.ncurses->win, y,
					  (object->width - x - len)/2);
				break;
			case ATTR_RIGHT:
				len = calc_string_width(string+inc);
				getyx(object->window.ncurses->win, y, x);
				wmove(object->window.ncurses->win, y, COLS-len);
				break;
			case ATTR_UNKNOWN:
			default:
				/*
				 * If no bindings table is found just skip over the attribute
				 * string. i.e. ignore the attribute but keep printing the text.
				 */
				if (object->display && object->display->bind) {
					tuple = get_tuple(object->display->bind, attribute, TT_ATTR);
					if (tuple)
						wattron(object->window.ncurses->win, COLOR_PAIR((int)*(tuple->addr)));
				}
				break;
		}

		
		skip += inc;
		string += inc;
		inc = 0;
	} while (*(string + inc++) == '\\');

	return (skip);
}

int
ncurses_print_string(OBJECT *object, char *string)
{
	int len, skip;
	int y = object->y;
	int x;
	int height = object->height;

	/* If it's a null string, clear the area using spaces. */
	if (!string)
		len = -1;
	else
		len = strlen(string);

	wmove(object->window.ncurses->win, y, x);

	while (height--) {
		x = object->x;
		if (wmove(object->window.ncurses->win, y++, x) == ERR)
			return (ERR);
		while (x++ < (object->x + object->width)) {
			if (len-- > 0) {
				/* Print input objects without processing \'s */
				if ((*string == '\\') && (object->type != OT_INPUT)) {
					skip = ncurses_parse_attrs(object, string);
					len -= skip;
					string += skip;
				}
				if (waddch(object->window.ncurses->win, *string++) == ERR)
					return (ERR);
			} else if (waddch(object->window.ncurses->win, ' ') == ERR)
					return (ERR);
		}
	}
	return (OK);
}	

void
ncurses_display_object(OBJECT *object)
{
	ncurses_set_attributes(object, 0);

	switch (object->type) {
		case OT_ACTION:
			ncurses_display_action(object);
			break;
		case OT_COMPOUND:
			ncurses_display_compound(object);
			break;
		case OT_FUNCTION:
			ncurses_display_function(object);
			break;
		case OT_INPUT:
			ncurses_display_input(object);
			break;
		case OT_MENU:
			ncurses_display_menu(object);
			break;
		case OT_TEXT:
			ncurses_display_text(object);
			break;
		default:
			break;
	}
}


void
ncurses_process_object(OBJECT *object)
{
	ncurses_set_attributes(object, 1);

	switch (object->type) {
		case OT_ACTION:
			ncurses_process_action(object);
			break;
		case OT_FUNCTION:
			break;
		case OT_INPUT:
			ncurses_process_input(object);
			break;
		case OT_MENU:
			ncurses_process_menu(object);
			break;
		case OT_TEXT:
			break;
		default:
			break;
	}
}

void
ncurses_draw_box(OBJECT *object)
{
	int y, x;
	chtype box, border;

	ncurses_parse_attrs(object, object->highlight);
	wattron(object->window.ncurses->win, A_BOLD);

	mvwaddch(object->window.ncurses->win, object->y, object->x,
				ACS_ULCORNER);

	mvwaddch(object->window.ncurses->win,
						 object->y + object->height - 1,
						 object->x,
				ACS_LLCORNER);

	for (y=object->y + 1; y < (object->y + object->height) - 1; y++) {
		mvwaddch(object->window.ncurses->win, y, object->x,
					ACS_VLINE);
	}
	for (x=object->x + 1; x < (object->x + object->width) - 1; x++) {
		mvwaddch(object->window.ncurses->win, object->y, x,
					ACS_HLINE);
	}

	ncurses_parse_attrs(object, object->attributes);
	wattroff(object->window.ncurses->win, A_BOLD);

	mvwaddch(object->window.ncurses->win, object->y,
						 object->x + object->width-1,
				ACS_URCORNER);

	mvwaddch(object->window.ncurses->win,
						 object->y + object->height-1,
						 object->x + object->width-1,
				ACS_LRCORNER);

	for (y=object->y + 1; y < (object->y + object->height) - 1; y++) {
		mvwaddch(object->window.ncurses->win, y, object->x + object->width - 1,
					 ACS_VLINE);
	}
	for (x=object->x + 1; x < (object->x + object->width) - 1; x++) {
		mvwaddch(object->window.ncurses->win, object->y + object->height - 1, x,
					ACS_HLINE);
	}

	wnoutrefresh(object->window.ncurses->win);
}

void
ncurses_draw_shadow(OBJECT *object)
{
	int i;

	for (i=object->y + 1; i < (object->y + object->height); i++) {
		wattron(object->window.ncurses->win, A_INVIS);
		mvwaddch(object->window.ncurses->win, i, object->x + object->width, ' ');
		waddch(object->window.ncurses->win,
					mvwinch(object->window.ncurses->win,
							i, object->x + object->width) & A_CHARTEXT);
		waddch(object->window.ncurses->win,
					mvwinch(object->window.ncurses->win,
							i, object->x + object->width+1) & A_CHARTEXT);
	}
	for (i=object->x + 1; i < (object->x + object->width + 2); i++) {
		wattrset(object->window.ncurses->win, A_INVIS);
		wmove(object->window.ncurses->win, object->y+object->height, i);
		waddstr(object->window.ncurses->win, " ");
		wattrset(object->window.ncurses->win, COLOR_PAIR(2)|A_BOLD);
		waddch(object->window.ncurses->win,
					mvwinch(object->window.ncurses->win,
							object->y+object->height, i) & A_CHARTEXT);
	}
	wnoutrefresh(object->window.ncurses->win);
}

void
ncurses_display_compound(OBJECT *object)
{
	int y, x;

		for (y=object->y; y < (object->y + object->height); y++)
			for (x=object->x; x < (object->x + object->width); x++)
				waddch(object->window.ncurses->win,
					mvwinch(object->window.ncurses->win, y, x) & A_CHARTEXT);
}

void
ncurses_display_action(OBJECT *object)
{
	ncurses_print_string(object, object->object.action->text);
	wnoutrefresh(object->window.ncurses->win);
}

void
ncurses_display_function(OBJECT *object)
{
	TUPLE *tuple;
	void (* fn)();

	tuple = tuple_search(object, object->object.function->fn, TT_FUNC);
	if (!tuple)
		return;
	fn = (FUNCP)tuple->addr;
	if (fn)
		(*fn)(object);
	wnoutrefresh(object->window.ncurses->win);
}

void
ncurses_display_text(OBJECT *object)
{
	ncurses_print_string(object, object->object.text->text);
	wnoutrefresh(object->window.ncurses->win);
}

void
ncurses_display_input(OBJECT *object)
{
	if (object->object.input->lbl_flag)
		ncurses_print_string(object, object->object.input->label);
	else
		ncurses_print_string(object, object->object.input->input);
	wnoutrefresh(object->window.ncurses->win);
}

void
ncurses_process_action(OBJECT *object)
{
	TUPLE *tuple;
	int ch;

	ncurses_display_action(object);
	wmove(object->window.ncurses->win, object->y, object->x);

	for (;;) {
		ch = wgetch(object->window.ncurses->win);

		if (ch == K_ACCEPT) {
			tuple = tuple_search(object,
						  object->object.action->action, TT_FUNC);

			if (!tuple) {
				ncurses_print_status("No function bound to action");
				continue;
			} else {
				(*tuple->addr)(object);
				return;
			}
		} else {
			ch = ncurses_bind_key(object, ch);

			if (ch == ST_ERROR) {
				beep();
				continue;
			} else
				return;
		}
	}
}

void
ncurses_process_input(OBJECT *object)
{
	int len;
	int disp_off=0, abspos=0, cursor = 0;
	int ch;

#define DISPOFF ((len < object->width) ? 0 : len - object->width)
#define CURSPOS ((len < object->width) ? len : object->width)

	ncurses_display_input(object);

	len = strlen(object->object.input->input);

	cursor = CURSPOS;
	abspos = cursor;

	for (;;) {

		wmove(object->window.ncurses->win, object->y, object->x+cursor);

		ch = wgetch(object->window.ncurses->win);
		ch = ncurses_bind_key(object, ch);

		/*
		 * If there was a valid motion command then we've
		 * moved to a new object so just return. If the motion
		 * command was invalid then just go around and get another
		 * keystroke. Otherwise, it was not a motion command.
		 */

		if (ch == ST_OK)
			return;
		else if (ch == ST_ERROR)
			continue;

		ncurses_print_status("");

		if (object->object.input->lbl_flag) {
			object->object.input->lbl_flag = 0;
		}
		if ((ch == K_HOME) || (ch == '')) {
				disp_off = 0;
				cursor = 0;
				abspos = 0;
		} else if ((ch == K_END) || (ch == '')) {
				disp_off = DISPOFF;
				abspos = len;
				cursor = CURSPOS;
		} else if (ch == K_DEL) {
			if (!(len-abspos))
				beep();
			else {
				bcopy(object->object.input->input+abspos+1,
						object->object.input->input+abspos,
						len - abspos);
				--len;
			}
		} else if ((ch == K_LEFT) || (ch == K_BS) || (ch == '')) {
			if (!abspos)
				beep();
			else {
				if (ch == K_BS) {
					bcopy(object->object.input->input+abspos,
							object->object.input->input+abspos-1,
							len-abspos+1);
					--len;
				}
				--abspos;
				--cursor;
				if ((disp_off) && (cursor < 0)) {
					--disp_off;
					++cursor;
				}
			}
		}else if (ch == '') {
			bzero(object->object.input->input, len);
			len = 0;
			abspos = 0;
			cursor = 0;
			disp_off = 0;
		} else if ((ch == K_RIGHT) || (ch == '')) {
			if (abspos == len)
				beep();
			else {
				++abspos;
				if (++cursor >= object->width) {
					++disp_off;
					--cursor;
				}
			}
		} else if ((isprint(ch)) && (len < object->object.input->limit)){ 
			bcopy(object->object.input->input+abspos,
					 object->object.input->input+abspos+1, len-abspos+1);
			object->object.input->input[abspos++] = ch;
			len++;
			if (++cursor > object->width) {
				++disp_off;
				--cursor;
			}
		} else 
				beep();
		ncurses_print_string(object, object->object.input->input+disp_off);
	}
}

void
ncurses_print_status(char *msg)
{
	if (wmove(stdscr, LINES-1, 0) == ERR) {
		endwin();
		exit(1);
	}

	wclrtoeol(stdscr);

	wstandout(stdscr);
	if (wprintw(stdscr, "%s",
				msg) == ERR) {
		endwin();
		exit(1);
	}
	wstandend(stdscr);
	wrefresh(stdscr);
}

int
ncurses_bind_key(OBJECT *object, unsigned int ch)
{
	struct Tuple *tuple=0;

	/* XXX -- check for keymappings here --- not yet done */

	if (ch == K_UPL) {
		if (object->lup) {
			tuple = tuple_search(object, object->lup, TT_OBJ_INST);
			if (!tuple)
				ncurses_print_status("Field to move up to does not exist");
		} else
			ncurses_print_status("Can't move up from this object");
	} else if (ch == K_DOWNL) {
		if (object->ldown) {
			tuple = tuple_search(object, object->ldown, TT_OBJ_INST);
			if (!tuple)
				ncurses_print_status("Field to move down to does not exist");
		} else
			ncurses_print_status("Can't move down from this object");
	} else if (ch == K_LEFTL) {
		if (object->lleft) {
			tuple = tuple_search(object, object->lleft, TT_OBJ_INST);
			if (!tuple)
				ncurses_print_status("Field to move left to does not exist");
		} else
			ncurses_print_status("Can't move left from this object");
	} else if (ch == K_RIGHTL) {
		if (object->lright) {
			tuple = tuple_search(object, object->lright, TT_OBJ_INST);
			if (!tuple)
				ncurses_print_status("Field to move right to does not exist");
		} else
			ncurses_print_status("Can't move right from this object");
	} else if (ch == K_NEXTL) {
		if (object->lnext) {
			tuple = tuple_search(object, object->lnext, TT_OBJ_INST);
			if (!tuple)
				ncurses_print_status("Field to move to next does not exist");
		} else
			ncurses_print_status("There is no next object from this object");
	} else
		/* No motion keys pressed */
		return (ch);

	if (tuple) {
		cur_obj = (OBJECT *)tuple->addr;
		return (ST_OK);
	} else {
		beep();
		return (ST_ERROR);
	}
}

void
ncurses_display_menu(OBJECT *object)
{
	if (ncurses_print_string(object,
		object->object.menu->options[object->object.menu->selected]) == ERR)
		ncurses_print_status("Illegal scroll in print_string");
	wnoutrefresh(object->window.ncurses->win);
}

void
ncurses_process_menu(OBJECT *object)
{
	int ch;

	for (;;) {

		ncurses_display_menu(object);
		wmove(object->window.ncurses->win, object->y, object->x);

		ch = wgetch(object->window.ncurses->win);

		switch (ch) {
			case ' ':
				ncurses_print_status("");
				object->object.menu->selected++;
				if (object->object.menu->selected >= object->object.menu->no_options)
					object->object.menu->selected = 0;
				ch = ST_OK;
				break;
			default:
				ch = ncurses_bind_key(object, ch);
				break;
		}

		if (ch == ST_OK)
			return;
		else if (ch == ST_ERROR) {
			beep();
			continue;
		} else {
			ncurses_print_status("Hit the space bar to toggle through options");
			beep();
			continue;
		}
	}
}

ncurses_set_attributes(OBJECT *object, int hl)
{
	int y, x;
	char *attr = 0;

	if (hl && object->highlight)
		attr = object->highlight;
	else if (object->attributes)
		attr = object->attributes;
	if (attr) {
		wattrset(object->window.ncurses->win, A_NORMAL);
		ncurses_parse_attrs(object, attr);
	}
}
