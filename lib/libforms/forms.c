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

#include <strhash.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <forms.h>
#include <err.h>

#include "internal.h"

extern FILE *yyin;
hash_table *root_table, *cbind;
OBJECT *cur_obj;
int done;

/* Default attribute commands */
struct attr_cmnd attr_cmnds[] = {
	{"box",		ATTR_BOX	},
	{"center",	ATTR_CENTER	},
	{"right",	ATTR_RIGHT	}
};

/* Internal bindings */

struct intbind {
	char *key;
	void *addr;
};

struct intbind internal_bindings[] = {
	{"draw_box", &draw_box},
	{"draw_shadow", &draw_shadow}
};  

/* Bind the internal function addresses */

void
bind_internals(hash_table *table)
{
	int i;

	for (i=0; i < (sizeof internal_bindings)/(sizeof (struct intbind)); i++)
		if (bind_tuple(table, internal_bindings[i].key,
					   TT_FUNC, internal_bindings[i].addr) == ST_ERROR)
			errx(-1, "Failed to bind internal tuples");
}

/*
 * Find the default device and open a display on it.
 */

DISPLAY *
default_open(DISPLAY *display)
{
	/* XXX -- not implemented, just calls ncurses */
	return (ncurses_open(display));

}

int
load_objects(const char *filename)
{
	FILE *fd;

	root_table = hash_create(0);
	if (!root_table)
		errx(-1, "Failed to allocate root bindings table");

	cbind = root_table;

	bind_internals(root_table);

	if (!(fd = fopen(filename, "r"))) {
		warn("Couldn't open file %s", filename);
		return (ST_ERROR);
	}

	yyin = fd;
	yyparse();

	if (fclose(fd)) {
		warn("Couldn't close file %s", filename);
		return (ST_ERROR);
	}

	return (ST_OK);
}

int
start_object(char *objname)
{
	TUPLE *tuple;
	OBJECT *object;

	tuple = get_tuple(root_table, objname, TT_OBJ_INST);
	if (!tuple)
		return (ST_NOBIND);

	object = (OBJECT *)tuple->addr;
	cur_obj = object;

	set_display(object->display);

	cur_obj->status |= O_VISIBLE;

	while (!done) {
		hash_traverse(root_table, &display_tuples, root_table);
		hash_traverse(root_table, &refresh_displays, root_table);
		process_object(cur_obj);
	}
	return (ST_DONE);
}

int
call_function(char *func, OBJECT *obj)
{
	TUPLE *tuple;

	tuple = tuple_search(obj, func, TT_FUNC);
	if (!tuple)
		return (0);

	(*tuple->addr)(obj);
	return (1);
}

set_display(DISPLAY *display)
{
	switch (display->type) {
		case DT_NCURSES:
			ncurses_set_display(display);
			break;
		default:
			break;
	}
}

void
display_object(OBJECT *obj)
{
	switch(obj->display->type) {
		case DT_NCURSES:
			ncurses_display_object(obj);
			break;
		default:
			break;
	}
}

void
process_object(OBJECT *obj)
{
	TUPLE *tuple;

	/* Call user routine, if there is one. */
	if (obj->UserProcFunc)
		if (call_function(obj->UserProcFunc, obj))
			return;

	/* Find the first non-compound object or a default override */
	while (obj->type == OT_COMPOUND) {
		tuple = tuple_search(obj, obj->object.compound->defobj, TT_OBJ_INST);
		obj = (OBJECT *)tuple->addr;
	}
	cur_obj = obj;

	switch(obj->display->type) {
		case DT_NCURSES:
			ncurses_process_object(obj);
			break;
		default:
			break;
	}
}

int
refresh_displays(char *key, void *data, void *arg)
{
	TUPLE *tuple = (TUPLE *)data;
	DISPLAY *display;

	if (tuple->type == TT_DISPLAY)
		display = (DISPLAY *)tuple->addr;
		switch (display->type) {
			case DT_NCURSES:
				ncurses_refresh_display(display);
				break;
			default:
				break;
		}

	return (1);
}

int
display_tuples(char *key, void *data, void *arg)
{
	TUPLE *tuple = (TUPLE *)data;
	OBJECT *obj;
	void (* fn)();

    switch(tuple->type) {
		case TT_OBJ_INST:
			obj = (OBJECT *)tuple->addr;

			/* Call user routine, if there is one. */
			if (obj->UserDrawFunc) {
				if (!call_function(obj->UserDrawFunc, obj))
					display_object(obj);
			} else
				display_object(obj);

			/* Display sub-objects */
			if (obj->bind)
				hash_traverse(obj->bind, &display_tuples, 0);
			break;
		default:
			break;
	}
	return (1);
}

AttrType
parse_default_attributes(char *string)
{
	int i;

	for (i=0; i < (sizeof attr_cmnds) / (sizeof (struct attr_cmnd)); i++)
		if (!strcmp(string, attr_cmnds[i].attr_name))
			return (attr_cmnds[i].attr_type);
	return (ATTR_UNKNOWN);
}
