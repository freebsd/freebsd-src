%{
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ncurses.h>
#include <forms.h>
#include <err.h>

#include "internal.h"

extern int yyleng;
int lineno = 1;

OBJECT *parent;
extern hash_table *cbind;


/* Some variables for holding temporary values as we parse objects */

OBJECT *object, *tmpobj;
DISPLAY *display;

int tmp, len;
char *tmpstr, *objname, *dispname, *useobj;
TUPLE *tmptuple;
TupleType t_type;

%}

%union {
	int ival;
	char *sval;
}

%token <ival> ACTION
%token <ival> ACTIVE
%token <ival> AS
%token <ival> AT
%token <ival> ATTR
%token <ival> ATTRTABLE
%token <ival> CALLFUNC
%token <ival> COLORPAIRS
%token <ival> DEFAULT
%token <ival> A_DISPLAY
%token <ival> DOWN
%token <ival> FORMS
%token <ival> FUNCTION
%token <ival> HANDLER
%token <ival> HEIGHT
%token <ival> INPUT
%token <ival> INPUTFILE
%token <ival> LABEL
%token <ival> LEFT
%token <ival> LIMIT
%token <ival> MENU
%token <ival> NCURSES
%token <ival> NEXT
%token <ival> AN_OBJECT
%token <ival> ON
%token <ival> ONENTRY
%token <ival> ONEXIT
%token <ival> OPTIONS
%token <ival> OUTPUTFILE
%token <ival> RIGHT
%token <ival> HIGHLIGHT
%token <ival> SELECTED
%token <ival> TEXT
%token <ival> TTYNAME
%token <ival> TYPE
%token <ival> UP
%token <ival> USE
%token <ival> USERDRAWFUNC
%token <ival> USERPROCFUNC
%token <ival> VERSION
%token <ival> WIDTH
%token <ival> WINDOW

%token <ival> BLACK
%token <ival> RED
%token <ival> GREEN
%token <ival> YELLOW
%token <ival> BLUE
%token <ival> MAGENTA
%token <ival> CYAN
%token <ival> WHITE

%token <ival> COMMA
%token <ival> SEMICOLON
%token <ival> LBRACE
%token <ival> RBRACE

%token <sval> NAME
%token <ival> NUMBER
%token <sval> STRING

%type <ival> color

%start forms

%%

forms: FORMS VERSION NAME spec
	{
#ifdef DEBUG
		printf("Forms language version %s\n", $3);
#endif
	}
	;

spec: /* empty */
	| spec display 
	| spec window
	| spec object
	;

display: A_DISPLAY NAME
		{
			dispname = $2;
			display = malloc(sizeof (DISPLAY));
			if (!display)
				errx(-1,
					"Failed to allocate memory for display (%d)", lineno);
		}
	LBRACE HEIGHT NUMBER 
		{
			display->virt_height = $6;
		}
	WIDTH NUMBER 
		{
			display->virt_width = $9;
		}
	disp_type disp_attr_table RBRACE
		{
			if (!display)
				errx(-1, "Failed to open display (%d)", lineno);
			bind_tuple(root_table, dispname, TT_DISPLAY, (FUNCP)display);
			dispname = 0;
		}
	;

disp_type: /* empty */
		{
			display->type = DT_ANY;
			display->device = 0;
			display = default_open(display);
		}
	| TYPE NCURSES device_ncurses
		{ display->type = DT_NCURSES; }
	;

device_ncurses: /* empty */
		{
			/* Use ncurses drivers but on a default tty */
			display->device.ncurses = 0;
			display = ncurses_open(display);
		}
	| LBRACE device_ncurses_tty 
		{
			display = ncurses_open(display);
		}
	  device_ncurses_colors RBRACE
	;

device_ncurses_tty: /* empty */
		{
			/* Use ncurses drivers but on a default tty */
			display->device.ncurses = 0;
		}
	| TTYNAME STRING INPUTFILE STRING OUTPUTFILE STRING
		{
			display->device.ncurses = (NCURSDEV *)malloc(sizeof (NCURSDEV));
			if (!display->device.ncurses)
				errx(-1, "Failed to allocate memory for ncurses device (%d)", lineno);
			display->device.ncurses->ttyname = $2;
			display->device.ncurses->input = $4;
			display->device.ncurses->output = $6;
		}
	;

device_ncurses_colors: /* empty */
	| COLORPAIRS LBRACE color_pairs RBRACE
	;

color_pairs: /* empty */
	| color_pairs color_pair
	;

color_pair: NUMBER color color
	{
		if (display)
			init_pair($1, $2, $3);
	}
	;

color: BLACK
		{ $$ = COLOR_BLACK; }
	| RED
		{ $$ = COLOR_RED; }
	| GREEN	
		{ $$ = COLOR_GREEN; }
	| YELLOW
		{ $$ = COLOR_YELLOW; }
	| BLUE
		{ $$ = COLOR_BLUE; }
	| MAGENTA
		{ $$ = COLOR_MAGENTA; }
	| CYAN
		{ $$ = COLOR_CYAN; }
	| WHITE
		{ $$ = COLOR_WHITE; }
	;		

disp_attr_table: /* empty */
		{ display->bind = 0; }
	| ATTRTABLE 
		{
			display->bind = hash_create(0);
			if (!display->bind)
				errx(-1, "Failed to allocate memory for display bindings (%d)", lineno);
		}
	LBRACE disp_attrs RBRACE
	;

disp_attrs: /* empty */
	| disp_attrs disp_attr
	;

disp_attr: NAME NUMBER
		{ bind_tuple(display->bind, $1, TT_ATTR, (FUNCP)$2); }
	;

window: WINDOW NAME ON NAME AT NUMBER COMMA NUMBER LBRACE
		{
			objname = $2;
			dispname = $4;
			object = malloc(sizeof (OBJECT));
			if (!object)
				errx(-1, "Failed to allocate memory for window (%d)", lineno);

			object->y = $6;
			object->x = $8;
			object->status = O_VISIBLE;
			object->bind = hash_create(0);
			if (!object->bind)
				errx(-1, "Failed to allocate memory for window's bindings (%d)", lineno);
		}
 object_params
		{
			tmptuple = tuple_search(object, dispname, TT_DISPLAY);
			if (!tmptuple)
				errx(-1, "Couldn't find binding for display (%d)", lineno);
			free(dispname);
			object->display = (struct Display *)tmptuple->addr;

			switch (object->display->type) {
				case DT_NCURSES:
				default:
					object->window.ncurses = malloc(sizeof (NCURSES_WINDOW));	
					if (!object->window.ncurses)
						errx(-1, "Failed to allocate memory for ncurses window, (%d)", lineno);
					ncurses_open_window(object);
					break;
			}
			object->parent = 0;
			if (!object->height)
				object->height = display->height;
			if (!object->width)
				object->width = display->width;
			bind_tuple(root_table, objname, TT_OBJ_INST, (FUNCP)object);
			parent = object;
			cbind = parent->bind;
		}
	object RBRACE
		{
			parent = 0;
			cbind = root_table;
		}
	;

objects: /* empty */
	| objects object
	;

object: NAME
		 { 
			objname = $1;
			object = malloc(sizeof (OBJECT));
			if (!object)
				errx(-1, "Failed to allocate memory for object (%d)", lineno);

			object->bind = hash_create(0);
			if (!object->bind)
				errx(-1, "Failed to allocate memory for ",
						 "object's bindings (%d)", lineno);
		}
	at LBRACE use_copy
		{
			if (useobj) {
				/* Need to declare parent to see previous scope levels */
				object->parent = parent;
				if (use_defined_object(object, useobj) == ST_NOBIND)
					errx(-1, "Object, %s, not found in scope (%d)",
						 useobj, lineno);
			}
		}
	object_params
		{
			/*
			 * If this is a function object convert it from
			 * a definition to an instance (see 'at' below).
			 */
			if (object->type == OT_FUNCTION)
				t_type = TT_OBJ_INST;

			/*
			 * If this is an instance object and it doesn't
			 * have a parent then there's a syntax error since
			 * instances can only be specified inside windows.
			 */
			if (parent)
				inherit_properties(object, parent);
			else if (t_type != TT_OBJ_DEF)
				errx(-1, "Object, %s, has no parent (%d)", objname, lineno);

			/* Propagate defobj up through nested compounds */
			if (t_type == TT_OBJ_INST &&
				parent && parent->type == OT_COMPOUND &&
				!parent->object.compound->defobj)
				parent->object.compound->defobj =
					strdup(objname);

			/* Add object and go down to next object */
			bind_tuple(cbind, objname, t_type, (FUNCP)object);
			parent = object;
			cbind = object->bind;
		}
	objects RBRACE
		{
			parent = object->parent;
			if (parent)
				cbind = parent->bind;
			else
				cbind = root_table;
			object = parent;
		}
	;

at: /* empty */
		{
			/*
			 * If there's no 'at' part specified then this is
			 * either a definition rather than an instance of
			 * an object or it's a function. Set it to a definition,
			 * we deal with the function case above.
			 */
			t_type = TT_OBJ_DEF;
			object->y = 0;
			object->x = 0;
		}
	| AT NUMBER COMMA NUMBER
		{
			t_type = TT_OBJ_INST;
			object->y = $2;
			object->x = $4;
		}
	;

use_copy: /* empty */
		{ useobj = 0; }
	| USE NAME 
		{ useobj = $2; }
	;

object_params: user_draw_func user_proc_func height width attributes highlight on_entry on_exit links object_type
	;

object_type: /* empty */
		{
			/* If we haven't inherited a type assume it's a compound */
			if (!object->type) {
				object->type = OT_COMPOUND;
				object->object.compound = malloc(sizeof (COMPOUND_OBJECT));
				if (!object->object.compound)
					errx(-1, "Failed to allocate memory for object, (%d)\n",
						 lineno);
				object->object.compound->defobj = 0;
			}
		}
	| object_action
	| object_compound
	| object_function
	| object_input
	| object_menu
	| object_text
	;

links: /* empty */
	| links  conns
	;

conns: UP NAME
		{ 
			if (object->lup)
				free(object->lup);
			object->lup = $2;
		}
	| DOWN NAME
		{ 
			if (object->ldown)
				free(object->ldown);
			object->ldown = $2;
		}
	| LEFT NAME
		{ 
			if (object->lleft)
				free(object->lleft);
			object->lleft = $2;
		}
	| RIGHT NAME
		{ 
			if (object->lright)
				free(object->lright);
			object->lright = $2;
		}
	| NEXT NAME
		{ 
			if (object->lnext)
				free(object->lnext);
			object->lnext = $2;
		}
	;

/*
 * Parse the action object type.
 */

object_action: ACTION NAME LABEL STRING
	{
		object->type = OT_ACTION;
		object->object.action = malloc(sizeof (ACTION_OBJECT));
		if (!object->object.action)
			errx(-1, "Failed to allocate memory for object, (%d)\n", lineno);
		object->object.action->action = $2;
		object->object.action->text = $4;
		if (!object->width)
			object->width = calc_string_width(object->object.text->text);
		if (!object->height)
			calc_object_height(object, object->object.text->text);
	}
	;

/*
 * Parse the compound object type.
 */

object_compound: ACTIVE NAME
	{
		object->type = OT_COMPOUND;
		object->object.compound = malloc(sizeof (COMPOUND_OBJECT));
		if (!object->object.compound)
			errx(-1, "Failed to allocate memory for object, (%d)\n", lineno);
		object->object.compound->defobj = $2;
	}
	;

/*
 * Parse the function object type
 */

object_function: CALLFUNC NAME 
		{
			object->type = OT_FUNCTION;
			object->object.function = malloc(sizeof (FUNCTION_OBJECT));
			if (!object->object.function)
				errx(-1, "Failed to allocate memory for object (%d)", lineno);
			object->object.function->fn = $2;
		}
	;

/*
 * Parse the input object type
 */

object_input: INPUT
		{
			object->type = OT_INPUT;
			object->object.input = malloc(sizeof (INPUT_OBJECT));
			if (!object->object.input)
				errx(-1, "Failed to allocate memory for object (%d)", lineno);
		}
	input_params limit
		{
			/* Force height to 1 regardless */
			object->height = 1;
			if (!object->width && !object->object.input->limit) {
				if (!object->object.input->label)
					errx(-1, "Unable to determine size of input object (%d)",
						lineno);
				object->width = calc_string_width(object->object.input->label);
				object->object.input->limit = object->width;
			} else if (!object->width)
				object->width = object->object.input->limit;
			else if (!object->object.input->limit)
				object->object.input->limit = object->width;
			if (object->object.input->limit < object->width)
				object->width = object->object.input->limit;

			object->object.input->input = 
				malloc(object->object.input->limit + 1);
			if (!object->object.input->input)
				errx(-1, "Failed to allocate memory for object (%d)", lineno);

			/*
			 * If it's a label then clear the input string
			 * otherwise copy the default there.
			 */

			if (object->object.input->lbl_flag)
				object->object.input->input[0] = '\0';
			else if (object->object.input->label) {
				tmp = strlen(object->object.input->label);
				strncpy(object->object.input->input,
					object->object.input->label,
					tmp);
				object->object.input->input[tmp] = 0;
			}
		}
	;

input_params: /* empty */ 
		{
			object->object.input->lbl_flag = 0;
			object->object.input->label = 0;
		}
	| STRING
		{
			object->object.input->lbl_flag = 0;
			object->object.input->label = $1;
		}
	| DEFAULT STRING
		{
			object->object.input->lbl_flag = 0;
			object->object.input->label = $2;
		}
	| LABEL STRING
		{
			object->object.input->lbl_flag = 1;
			object->object.input->label = $2;
		}
	;

limit: /* empty */
		{ object->object.input->limit = 0; }
	| LIMIT NUMBER 
		{ object->object.input->limit = $2; }
	;

/*
 * Parse the menu object type
 */

object_menu: OPTIONS LBRACE 
		{
			object->type = OT_MENU;
			object->object.menu = malloc(sizeof (MENU_OBJECT));
			if (!object->object.menu)
				errx(-1, "Failed to allocate memory for object (%d)", lineno);
			object->object.menu->no_options = 0;
			object->object.menu->options = 0;
			len = 0;
		}
	menuoptions 
		{
			object->height = 1;
			if (!object->width)
				object->width = len;
		}
	RBRACE option_selected
	;

menuoptions: menuoption
	| menuoptions  menuoption
	;

menuoption: STRING
			{
				tmpstr = $1;
				object->object.menu->no_options = 
					add_menu_option(object->object.menu, tmpstr);
				if (!object->object.menu->no_options)
					errx(-1, "Failed to allocate memory for option (%d)", lineno);
				tmp = calc_string_width(tmpstr);
				if (tmp > len)
					len = tmp;
				free(tmpstr);
			}
	;

option_selected: /* empty */
		{ object->object.menu->selected = 0; }
	| SELECTED NUMBER 
		{ object->object.menu->selected = $2; }
	;

/*
 * Parse the text object type
 */

object_text: TEXT STRING 
		{
			object->type = OT_TEXT;
			object->object.text = malloc(sizeof (TEXT_OBJECT));
			if (!object->object.text)
				errx(-1, "Failed to allocate memory for object (%d)", lineno);
			object->object.text->text = $2;
			if (!object->width)
				object->width = calc_string_width(object->object.text->text);
			if (!object->height)
				calc_object_height(object, object->object.text->text);
		}
	;

user_draw_func: /* empty */
	| USERDRAWFUNC NAME 
		{
			if (object->UserDrawFunc)
				free(object->UserDrawFunc);
			object->UserDrawFunc = $2;
		}
	;

user_proc_func: /* empty */
	| USERPROCFUNC NAME 
		{
			if (object->UserProcFunc)
				free(object->UserProcFunc);
			object->UserProcFunc = $2;
		}
	;

height: /* empty */
	| HEIGHT NUMBER 
		{ object->height = $2; }
	;

width: /* empty */
	| WIDTH NUMBER 
			{ object->width = $2; }
	;

attributes: /* empty */
	| ATTR STRING 
		{
			if (object->attributes)
				free(object->attributes);
			object->attributes = $2;
		}
	;

highlight: /* empty */
	| HIGHLIGHT STRING 
		{
			if (object->highlight)
				free(object->highlight);
			object->highlight = $2;
		}
	;

on_entry:	/* empty */
	| ONENTRY NAME 
		{
			if (object->OnEntry)
				free(object->OnEntry);
			object->OnEntry = $2;
		}
	;

on_exit:	/* empty */
	| ONEXIT NAME 
		{
			if (object->OnExit)
				free(object->OnExit);
			object->OnExit = $2;
		}
	;

%%

void
yyerror (char *error)
{
	errx(-1, "%s at line %d\n", error, lineno);
}
