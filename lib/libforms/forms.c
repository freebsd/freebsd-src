/*
 * Copyright (c) 1995 Paul Richards. 
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <forms.h>
#include "internal.h"

unsigned int keymap[] = {
	KEY_UP,			/* F_UP */
	KEY_DOWN,		/* F_DOWN */
	9,			/* F_RIGHT */
	8,			/* F_LEFT */
	10,			/* F_NEXT */
	KEY_LEFT,		/* F_CLEFT */
	KEY_RIGHT,		/* F_CRIGHT */
	KEY_HOME,		/* F_CHOME */
	KEY_END,		/* F_CEND */
	263,		/* F_CBS */
	330		/* F_CDEL */
};

int done=0;
	
int
initfrm(struct form *form)
{

	struct field *field = &form->field[0];

	cbreak();
	noecho();

	form->window = newwin(form->nlines, form->ncols, form->y, form->x);
	if (!form->window) {
		print_status("Couldn't open window, closing form");
		return (ERR);
	}
	form->no_fields = 0;

	keypad(form->window, TRUE);

	while (field->type != F_END) {
		if (field->type == F_INPUT) {
			field->field.input->input = malloc(field->width);
			if (!field->field.input->input){
				print_status("Couldn't allocate memory, closing form");
				endfrm(form);
				return (ERR);
			}
			/*
			 * If it's a label then clear the input string
			 * otherwise copy the default string to the input string.
			 */
			if (field->field.input->lbl_flag)
				field->field.input->input[0] = '\0';
			else if (field->field.input->label)
				strcpy(field->field.input->input, field->field.input->label);
		} else if ((field->type != F_TEXT) && (field->type != F_MENU) &&
		   (field->type != F_ACTION)) {
			print_status("Unknown field type, closing form");
			endfrm(form);
			return (ERR);
		}
		form->no_fields++;
		field = &form->field[form->no_fields];
	}
	show_form(form);
	return (OK);
}

void
endfrm(struct form *form)
{

	struct field *field = &form->field[0];
	int i;

	delwin(form->window);

	for (i=0; i < form->no_fields; i++) {
		if (field->type == F_INPUT)
			free(field->field.input->input);
		field = &form->field[i];
	}
}

int
update_form(struct form *form)
{

	switch (form->field[form->current_field].type) {
		case F_MENU:
			field_menu(form);
			break;
		case F_INPUT:
			field_input(form);
			break;
		case F_ACTION:
			field_action(form);
			break;
		case F_TEXT:
		default:
	}

	show_form(form);

	return (done);
}

static void
show_form(struct form *form)
{
	int i;

	for (i=0; i < form->no_fields; i++) {
		wattrset(form->window, 0);
		wmove(form->window, form->field[i].y, form->field[i].x);
		switch (form->field[i].type) {
			case F_TEXT:
				disp_text(form, i);
				break;
			case F_MENU:
				disp_menu(form, i);
				break;
			case F_INPUT:
				disp_input(form,i);
				break;
			case F_ACTION:
				disp_action(form,i);
				break;
			case F_END:
			default:
				break;
		}
	}
	wrefresh(form->window);
}

static void
disp_text(struct form *form, int index)
{

	struct field *field = &form->field[index];

	wattron(form->window, field->attr);

	if (print_string(form->window, field->y, field->x,
	             field->disp_width, field->field.text->text) == ERR)
		print_status("Illegal scroll in print_string");
}

static void
disp_input(struct form *form, int index)
{

	struct field *field = &form->field[index];

	wattron(form->window, field->attr);

	if (field->field.input->lbl_flag) {
		if (print_string(form->window, field->y, field->x,
						 field->disp_width, field->field.input->label) == ERR)
			print_status("Illegal scroll in print_string");
	} else 
		if (print_string(form->window, field->y, field->x,
						 field->disp_width, field->field.input->input) == ERR)
			print_status("Illegal scroll in print_string");
		
}

static void
disp_menu(struct form *form, int index)
{
	struct field *field = &form->field[index];

	wattron(form->window, field->attr);

	if (print_string(form->window, field->y, field->x,
			field->disp_width,
			field->field.menu->options[field->field.menu->selected]) == ERR)
		print_status("Illegal scroll in print_string");
}

static void
disp_action(struct form *form, int index)
{
	struct field *field = &form->field[index];


	wattron(form->window, field->attr);

	if (print_string(form->window, field->y, field->x,
				field->disp_width,
				field->field.action->text) == ERR)
		print_status("Illegal scroll in print_string");

}

static void
field_action(struct form *form)
{

	struct field *field = &form->field[form->current_field];
	int ch;

	for (;;) {
		wattron(form->window, F_SELATTR);
		disp_action(form, form->current_field);
		ch = wgetch(form->window);
		if (ch == F_ACCEPT) {
			(*field->field.action->fn)();
			return;
		} else if (!next_field(form, ch))
			beep();
		else
			return;
	}
}

static void
field_menu(struct form *form)
{
	struct field *field = &form->field[form->current_field];
	int ch;

	for (;;) {
		wattron(form->window, F_SELATTR);
		disp_menu(form, form->current_field);
		wmove(form->window, field->y, field->x);
		switch (ch = wgetch(form->window)) {
			case ' ':
				print_status("");
				field->field.menu->selected++;
				if (field->field.menu->selected >= field->field.menu->no_options)
					field->field.menu->selected = 0;
				break;
			default:
				if (!next_field(form, ch)) {
					print_status("Hit the space bar to toggle through options");
					beep();
				} else
					return;
		}
	}
}

static int
next_field(struct form *form, int ch)
{

	struct field *field = &form->field[form->current_field];

	if (ch == F_UP) {
		if (field->up == -1) {
			print_status("Can't go up from here");
			return (0);
		} else
			form->current_field = field->up;
	} else if (ch == F_DOWN) {
		if (field->down == -1) {
			print_status("Can't go down from here");
			return (0);
		} else
			form->current_field = field->down;
	} else if (ch == F_NEXT) {
		if (field->next == -1) {
			print_status("Can't go to next from here");
			return (0);
		} else
			form->current_field = field->next;
	} else if (ch == F_RIGHT) {
		if (field->right == -1) {
			print_status("Can't go right from here");
			return (0);
		} else
			form->current_field = field->right;
	} else if (ch == F_LEFT) {
		if (field->left == -1) {
			print_status("Can't go left from here");
			return (0);
		} else
			form->current_field = field->left;
	} else
		return (0);

	print_status("");
	field->attr = F_DEFATTR;
	return (1);
}

static int
print_string(WINDOW *window, int y, int x,
             int disp_width, char *string)
{
	int len;

	if (!string)
		len = -1;

	len = strlen(string);

	if (wmove(window, y, x) == ERR)
		return (ERR);
	while (disp_width--) {
		if (len-- > 0) {
			if (waddch(window, *string++) == ERR)
				return (ERR);
		} else
			if (waddch(window, ' ') == ERR)
				return (ERR);
	}
	return (OK);
}	

static void
print_status(char *msg)
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


void
field_input(struct form *form)
{
	struct field *field = &form->field[form->current_field];
	int len;
	int ch;
	int disp_off=0, abspos=0, cursor = 0;

#define DISPOFF ((len < field->disp_width) ? 0 : len - field->disp_width)
#define CURSPOS ((len < field->disp_width) ? len : field->disp_width)

	len = strlen(field->field.input->input);
	wattron(form->window, F_SELATTR);
	disp_input(form, form->current_field);

	cursor = CURSPOS;
	abspos = cursor;

	for(;;) {

		wmove(form->window, field->y, field->x+cursor);
		wrefresh(form->window);

		ch = wgetch(form->window);
		if (next_field(form, ch)) {
			print_string(form->window, field->y, field->x,
						field->disp_width,
						field->field.input->input+DISPOFF);
			return;
		}
		if (field->field.input->lbl_flag) {
			field->field.input->lbl_flag = 0;
		}
		if (ch == F_CHOME) {
				disp_off = 0;
				cursor = 0;
				abspos = 0;
		} else if (ch == F_CEND) {
				disp_off = DISPOFF;
				abspos = len;
				cursor = CURSPOS;
		} else if (ch == F_CDEL) {
			if (!(len-abspos))
				beep();
			else {
				bcopy(field->field.input->input+abspos+1,
						field->field.input->input+abspos,
						len - abspos);
				--len;
			}
		} else if ((ch == F_CLEFT) || (ch == F_CBS)) {
			if (!abspos)
				beep();
			else {
				if (ch == F_CBS) {
					bcopy(field->field.input->input+abspos,
							field->field.input->input+abspos-1,
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
		} else if (ch == F_CRIGHT) {
			if (abspos == len)
				beep();
			else {
				++abspos;
				if (cursor++ == field->disp_width) {
					++disp_off;
					--cursor;
				}
			}
		} else if ((isprint(ch)) && (len < field->width)){ 
			bcopy(field->field.input->input+abspos,
					 field->field.input->input+abspos+1, len-abspos+1);
			field->field.input->input[abspos++] = ch;
			len++;
			if (++cursor > field->disp_width) {
				++disp_off;
				--cursor;
			}
		} else {
				beep();
		}
		print_string(form->window, field->y, field->x,
					 field->disp_width, field->field.input->input+disp_off);
	}
	/* Not Reached */
}

void
exit_form(void)
{
	done = F_DONE;
}

void
cancel_form(void)
{
	done = F_CANCEL;
}
