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

#include <hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <forms.h>
#include <err.h>
#include <ncurses.h>

#include "internal.h"

extern FILE *yyin;

hash_table *global_bindings;

unsigned int f_keymap[] = {
	KEY_UP,         /* F_UP */
	KEY_DOWN,       /* F_DOWN */
	9,          /* F_RIGHT */
	8,          /* F_LEFT */
	10,         /* F_NEXT */
	KEY_LEFT,       /* F_CLEFT */
	KEY_RIGHT,      /* F_CRIGHT */
	KEY_HOME,       /* F_CHOME */
	KEY_END,        /* F_CEND */
	263,            /* F_CBS */
	330,            /* F_CDEL */
	10          /* F_ACCEPT */
};

int
form_load(const char *filename)
{
	FILE *fd;

	global_bindings = hash_create(0);

	if (!global_bindings)
		return (FS_ERROR);

	if (!(fd = fopen(filename, "r"))) {
		warn("Couldn't open forms file %s", filename);
		return (FS_ERROR);
	}

	yyin = fd;
	yyparse();

	if (fclose(fd)) {
		warn("Couldn't close forms file %s", filename);
		return (FS_ERROR);
	}

	hash_stats(global_bindings, 1);

	return (FS_OK);
}

int
find_editable(char *key, void *data, void *arg)
{
	struct Tuple *tuple = (struct Tuple *)data;
	struct Field *field;

	if (tuple->type != FT_FIELD_INST)
		return (1);

	field = (struct Field *)tuple->addr;

	if ((field->type == FF_INPUT) ||
		(field->type == FF_MENU) ||
		(field->type == FF_ACTION)) {
		arg = field;
		return (0);
	} else
		return (1);
}

struct Form *
form_start(char *formname)
{
	struct Tuple *tuple;
	struct Form *form;
	struct Field *field = 0;
	struct Field *start = 0;

	tuple = form_get_tuple(global_bindings, formname, FT_FORM);

	if (!tuple) {
		warnx("No such form");
		return (0);
	}

	form = tuple->addr;

	/* Initialise form */
	if (!form->height)
		form->height = LINES;
	if (!form->width)
		form->width = COLS;

	form->window = newwin(form->height, form->width, form->y, form->x);
	if (!form->window) {
		warnx("Couldn't open window, closing form");
		return (0);
	}

	/* Initialise the field instances */

	hash_traverse(form->bindings, init_field, field);

	tuple = form_get_tuple(form->bindings, form->startfield, FT_FIELD_INST);

	if (!tuple) {
		warnx("No start field specified");
		/* Search for an editable field */
		hash_traverse(form->bindings, &find_editable, start);
		form->current_field = start;
	} else	
		form->current_field = (struct Field *)tuple->addr;

	if (!form->current_field)
		errx(1, "No suitable start field found, aborting");

	form->prev_field = form->current_field;

	form->status = FS_RUNNING;

	return (form);
}

int
form_bind_tuple(hash_table *htable, char *name, TupleType type, void *addr)
{
	struct Tuple *tuple;

	tuple = malloc(sizeof (struct Tuple));
	if (!tuple) {
		warn("Couldn't allocate memory for new tuple");
		return (FS_ERROR);
	}

	tuple->name = name;
	tuple->type = type;
	tuple->addr = addr;
	tuple->next = 0;

	if (!htable)
		return (FS_ERROR);
	else {
		/* Check there isn't already a tuple of this type with this name */
		if (form_get_tuple(htable, name, type)) {
			warn("Duplicate tuple name, %s, skipping", name);
			return (FS_ERROR);
		} else
			hash_search(htable, tuple->name, tuple, NULL);
	}

	return (0);
}

int
tuple_match_any(char *key, struct Tuple *tuple, TupleType *type)
{
	if (tuple->type != *type) {
		type = 0;
		return (1);
	} else {
		type = (TupleType *)tuple;
		return (0);
	}
}

struct Tuple *
form_get_tuple(hash_table *htable, char *key, TupleType type)
{
	void *arg = &type;

	/*
	 * If a key is specified then search for that key,
	 * otherwise, search the whole table for the first
	 * tuple of the required type.
	 */

	if (key)
		return(hash_search(htable, key, NULL, NULL));
	else {
		hash_traverse(htable, &tuple_match_any, arg);
		return (arg);
	}
}

int
show_field(char *key, void *data, void *arg)
{
	struct Tuple *tuple = (struct Tuple *)data;
	struct Field *field = (struct Field *)tuple->addr;

	display_field(arg, field);

	return (1);

}

int
form_show(char *formname)
{
	struct Tuple *tuple;
	struct Form *form;
	int x, y;

	tuple = form_get_tuple(global_bindings, formname, FT_FORM);
	if (!tuple)
		return (FS_NOBIND);

	form = tuple->addr;

	/* Clear form */
	wattrset(form->window, form->attr);
	for (y=0; y < form->height; y++)
		for (x=0; x < form->width; x++)
			mvwaddch(form->window, y, x, ' ');

	hash_traverse(form->bindings, show_field, form->window);

	return (FS_OK);
}

unsigned int
do_key_bind(struct Form *form, unsigned int ch)
{
	struct Field *field = form->current_field;
	struct Tuple *tuple=0;

	/* XXX -- check for keymappings here --- not yet done */

	if (ch == FK_UP) {
		if (field->fup) {
			tuple = form_get_tuple(form->bindings, field->fup, FT_FIELD_INST);
			if (!tuple)
				print_status("Field to move up to does not exist");
		} else
			print_status("Can't move up from this field");
	} else if (ch == FK_DOWN) {
		if (field->fdown) {
			tuple = form_get_tuple(form->bindings, field->fdown, FT_FIELD_INST);
			if (!tuple)
				print_status("Field to move down to does not exist");
		} else
			print_status("Can't move down from this field");
	} else if (ch == FK_LEFT) {
		if (field->fleft) {
			tuple = form_get_tuple(form->bindings, field->fleft, FT_FIELD_INST);
			if (!tuple)
				print_status("Field to move left to does not exist");
		} else
			print_status("Can't move left from this field");
	} else if (ch == FK_RIGHT) {
		if (field->fright) {
			tuple = form_get_tuple(form->bindings, field->fright, FT_FIELD_INST);
			if (!tuple)
				print_status("Field to move right to does not exist");
		} else
			print_status("Can't move right from this field");
	} else if (ch == FK_NEXT) {
		if (field->fnext) {
			tuple = form_get_tuple(form->bindings, field->fnext, FT_FIELD_INST);
			if (!tuple)
				print_status("Field to move to next does not exist");
		} else
			print_status("Can't move next from this field");
	} else
		/* No motion keys pressed */
		return (ch);

	if (tuple) {
		form->prev_field = form->current_field;
		form->current_field = tuple->addr;
		return (FS_OK);
	} else {
		beep();
		return (FS_ERROR);
	}
}

#ifdef DEBUG_NOT_YET
void
debug_dump_bindings(hash_table *htable)
{
	struct Tuple *binds;

	binds = form_get_tuple(htable, 0, FT_ANY);
	while (binds) {
		printf("%s, %d, %x\n", binds->name, binds->type, (int)binds->addr);
		binds = form_next_tuple(0, FT_ANY, binds->next);
	}
}

void debug_dump_form(struct Form *form)
{
	struct Field *field;

	field = form->fieldlist;

	for ( ; field; field = field->next) {
		printf("%s, %x, next = %x\n", field->defname, (int)field, (int)field->next);
	}
}
#endif
