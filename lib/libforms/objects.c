/*-
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
#include <err.h>
#include <ncurses.h>
#include <forms.h>
#include <string.h>
#include <stdlib.h>

#include "internal.h"

extern hash_table *global_bindings;

extern int lineno;

int done=0;

__inline void
inherit_parent(OBJECT *obj, OBJECT *parent)
{
	obj->parent = parent;
	obj->status = parent->status;
	if (!obj->y)
		obj->y = parent->y;
	if (!obj->x)
		obj->x = parent->x;
	if (!obj->height)
		obj->height = parent->height;
	if (!obj->width)
		obj->width = parent->width;
	if (!obj->attributes && parent->attributes)
		obj->attributes = strdup(parent->attributes);
	if (!obj->highlight && parent->highlight)
		obj->highlight = strdup(parent->highlight);
	if (!obj->lnext && parent->lnext)
		obj->lnext = strdup(parent->lnext);
	if (!obj->lup && parent->lup)
		obj->lup = strdup(parent->lup);
	if (!obj->ldown && parent->ldown)
		obj->ldown = strdup(parent->ldown);
	if (!obj->lleft && parent->lleft)
		obj->lleft = strdup(parent->lleft);
	if (!obj->lright && parent->lright)
		obj->lright = strdup(parent->lright);
	obj->display = parent->display;
	obj->window = parent->window;
}

/*
 * Inherit any unspecified properties from the parent. Not
 * all properties get passed down to children.
 */

int
inherit_props(char *key, void *data, void *arg)
{
	TUPLE *tuple = (TUPLE *)data;
	OBJECT *parent = (OBJECT *)arg;
	OBJECT *obj = (OBJECT *)tuple->addr;


	inherit_parent(obj, parent);

	if (obj->bind)
		hash_traverse(obj->bind, &inherit_props, obj);

	return (1);
}

/*
 * Propagate unspecified properties from the parent
 * to any attached sub-objects.
 */

void
inherit_properties(OBJECT *obj, OBJECT *parent)
{
	inherit_parent(obj, parent);
	hash_traverse(obj->bind, &inherit_props, obj);
}

__inline void
clone_object(OBJECT *object, OBJECT *def)
{
	int i;

	/* XXX - Should really check if strdup's succeed */

	object->type = def->type;
	object->status = def->status;
	object->parent = def->parent;
	/*
	 * Only copy sizes for fixed size objects,
	 * otherwise inherit from parent. Always
	 * inherit x and y.
	 */
	if (def->type != OT_COMPOUND && def->type != OT_FUNCTION) {
		object->height = def->height;
		object->width = def->width;
	}
	if (def->attributes)
		object->attributes = strdup(def->attributes);
	if (def->highlight)
		object->highlight = strdup(def->highlight);
	if (def->lnext)
		object->lnext = strdup(def->lnext);
	if (def->lup)
		object->lup = strdup(def->lup);
	if (def->ldown)
		object->ldown = strdup(def->ldown);
	if (def->lleft)
		object->lleft = strdup(def->lleft);
	if (def->lright)
		object->lright = strdup(def->lright);
	if (def->UserDrawFunc)
		object->UserDrawFunc = strdup(def->UserDrawFunc);
	if (def->UserProcFunc)
		object->UserProcFunc = strdup(def->UserProcFunc);
	if (def->OnEntry)
		object->OnEntry = strdup(def->OnEntry);
	if (def->OnExit)
		object->OnExit = strdup(def->OnExit);

	object->display = def->display;
	object->window = def->window;

	switch (object->type) {
		case OT_ACTION:
			if (def->object.action) {
				object->object.action = malloc(sizeof (ACTION_OBJECT));
				if (!object->object.action)
					errx(-1,
				   	"Failed to allocate memory for copy of action object");
				if (def->object.action->text)
					object->object.action->text =
				   		strdup(def->object.action->text);
				if (def->object.action->action)
					object->object.action->action =
				   		strdup(def->object.action->action);
			}
			break;
		case OT_COMPOUND:
			if (def->object.compound) {
				object->object.compound = malloc(sizeof (COMPOUND_OBJECT));
				if (!object->object.compound)
					errx(-1,
				   	"Failed to allocate memory for copy of compound object");
				if (def->object.compound->defobj)
					object->object.compound->defobj =
				   		strdup(def->object.compound->defobj);
			}
			break;
		case OT_FUNCTION:
			object->object.function = malloc(sizeof (FUNCTION_OBJECT));
			if (!object->object.function)
				errx(-1, "Failed to allocate memory for copy of function object");
			object->object.function->fn = strdup(def->object.function->fn);
			break;
		case OT_INPUT:
			object->object.input = malloc(sizeof (INPUT_OBJECT));
			if (!object->object.input)
				errx(-1, "Failed to allocate memory for copy of input object");
			object->object.input->lbl_flag = def->object.input->lbl_flag;
			object->object.input->label = strdup(def->object.input->label);
			object->object.input->input = strdup(def->object.input->input);
			object->object.input->limit = def->object.input->limit;
			break;
		case OT_MENU:
			object->object.menu = malloc(sizeof (MENU_OBJECT));
			if (!object->object.menu)
				errx(-1, "Failed to allocate memory for copy of menu object");
			object->object.menu->selected = def->object.menu->selected;
			for (i=0; i < def->object.menu->no_options; i++) {
				object->object.menu->no_options =
					add_menu_option(object->object.menu,
						def->object.menu->options[i]);
				if (!object->object.menu->no_options)
					errx(-1, "Failed to allocate memory for copy of menu option");
			}
			break;
		case OT_TEXT:
			object->object.text = malloc(sizeof (TEXT_OBJECT));
			if (!object->object.text)
				errx(-1, "Failed to allocate memory for copy of text object");
			object->object.text->text = strdup(def->object.text->text);
			break;
		default:
			break;
	}
}

/*
 * Recursively clone objects in the bindings table.
 */

int
copy_bound_objects(char *key, void *data, void *arg)
{
	TUPLE *tuple = (TUPLE *)data;
	OBJECT *object = (OBJECT *)arg;
	OBJECT *clone, *src;

	if (tuple->type != TT_OBJ_DEF && tuple->type != TT_OBJ_INST)
		errx(-1, "Invalid tuple type in definition");

	src = (OBJECT *)tuple->addr;


	clone = malloc(sizeof (OBJECT));
	if (!clone)
		errx(-1, "Failed to allocate memory for clone object");
	clone->bind = hash_create(0);
	if (!clone->bind)
		errx(-1, "Failed to create hash table for definition copy");

	clone_object(clone, src);
	bind_tuple(object->bind, key, tuple->type, (FUNCP)clone);

	if (src->bind)
		hash_traverse(src->bind, &copy_bound_objects, clone);
	return (1);
}

int
use_defined_object(OBJECT *object, char *src)
{
	TUPLE *tuple;
	OBJECT *def;

	tuple = tuple_search(object, src, TT_OBJ_DEF);
	if (!tuple)
		return (ST_NOBIND);
	else
		def = (OBJECT *)tuple->addr;

	/* Clone root object */
	clone_object(object, def);

	/* Now recursively clone sub-objects */
	hash_traverse(def->bind, &copy_bound_objects, object);
}


/*
 * Calculate length of printable part of the string,
 * stripping out the attribute modifiers.
 */

int
calc_string_width(char *string)
{
	int len, width=0;

	if (!string)
		return (0);

	len = strlen(string);

	while (len) {
		if (*string != '\\') {
			width++;
			len--;
			string++;
			continue;
		} else {
			string++;
			len--;
			if (*string == '\\') {
				string++;
				width++;
				len--;
				continue;
			} else {
				while (!isspace(*string)) {
					string++;
					len--;
				}
				while (isspace(*string)) {
					string ++;
					len--;
				}
			}
		}
	}

	return (width);
}

/* Calculate a default height for a object */

void
calc_object_height(OBJECT *object, char *string)
{

	int len;

	len = calc_string_width(string);

	if (!object->width) {
		/*
		 * This is a failsafe, this routine shouldn't be called
		 * with a width of 0, the width should be determined
		 * first.
		 */
		object->height = 1;
		return;
	}

	if (len < object->width) {
		object->height = 1;
		return;
	} else
		object->height = len / object->width;

	if ((object->height*object->width) < len)
		object->height++;

	return;
}

int
add_menu_option(MENU_OBJECT *menu, char *option)
{
	char **tmp;
	int len;

	tmp = (char **)realloc(menu->options, (menu->no_options + 1) * sizeof(char**));
	if (!tmp)
		return (0);
	else
		menu->options = tmp;

	len = strlen(option) + 1;
	menu->options[menu->no_options] = (char *)malloc(len);
	if (!menu->options[menu->no_options])
		return (0);
	strncpy(menu->options[menu->no_options], option, len);

	return (++menu->no_options);
}

 
/* Default object functions */
 
void  
draw_box(OBJECT *object) 
{
	/* Gross hack for now */ 
	ncurses_draw_box(object);
}
 
void
draw_shadow(OBJECT *object)
{ 
	/* Gross hack for now */
	ncurses_draw_shadow(object);
}

