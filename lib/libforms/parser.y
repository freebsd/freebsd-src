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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <forms.h>
#include <err.h>

#include "internal.h"

char *cpstr(char *);

extern int yyleng;
int lineno = 1;
int charno = 1;
int off;

char *fieldname;
char *defname;
char *formname;
char *startname;
char *colortable;
int formattr;
char *text;
char *label;
char *function;
char *up, *down, *left, *right, *next;
int height, width;
int y, x;
int width;
int limit;
int attr;
int selattr;
int type;
int lbl_flag;
int selected, no_options=0;

extern FILE *outf;

struct MenuList {
	char *option;
	struct MenuList *next;
};

struct MenuList *cur_menu;
struct MenuList *menu_list;
struct MenuList *menu;

struct pair_node {
	char *foreground;
	char *background;
	struct pair_node *next;
};
struct pair_node *pair_list;
struct pair_node *cur_pair;
struct pair_node *pair;

struct color_table {
	char *tablename;
	struct pair_node *pairs;
	struct color_table *next;
};

struct color_table *color_table;
struct color_table *cur_table;
struct color_table *color_tables;

struct Form *form;
struct Field *field_inst_list;
struct Field *field;
struct Field *cur_field;
%}

%union {
	int ival;
	char *sval;
}

%token <ival> FORM
%token <ival> COLORTABLE
%token <ival> COLOR
%token <ival> BLACK
%token <ival> RED
%token <ival> GREEN
%token <ival> YELLOW
%token <ival> BLUE
%token <ival> MAGENTA
%token <ival> CYAN
%token <ival> WHITE
%token <ival> PAIR
%token <sval> NAME
%token <sval> STRING
%token <ival> AT
%token <ival> AS
%token <ival> HEIGHT
%token <ival> EQUALS
%token <ival> NUMBER
%token <ival> WIDTH
%token <ival> STARTFIELD
%token <ival> COMMA
%token <ival> LBRACE
%token <ival> RBRACE
%token <ival> TEXT
%token <ival> ATTR
%token <ival> SELATTR
%token <ival> DEFAULT
%token <ival> LABEL
%token <ival> LIMIT
%token <ival> SELECTED
%token <ival> OPTIONS
%token <ival> ACTION
%token <ival> FUNC
%token <ival> LINK
%token <ival> UP
%token <ival> DOWN
%token <ival> LEFT
%token <ival> RIGHT
%token <ival> NEXT
%token <ival> DEF

%type <sval> a_color

%start spec

%%

spec: /* empty */
	| spec fields
	| spec forms
	| spec colours
	;

colours: COLOR NAME 
		{
			color_table = malloc(sizeof (struct color_table));
			if (!color_table) {
				fprintf(stderr, "Couldn't allocate memory for a color table\n");
				exit (1);
			}
			color_table->tablename = cpstr($2);
		}
	LBRACE color_pairs RBRACE
		{
			color_table->pairs = pair_list;
			cur_pair = 0;
			form_bind_tuple(color_table->tablename, FT_COLTAB, color_table);
		}
	;

color_pairs: /* empty */
	| color_pairs pair
	;

pair: PAIR EQUALS a_color
		{
			pair = malloc(sizeof (struct pair_node));
			if (!pair) {
				fprintf(stderr, "Couldn't allocate memory for a color pair\n");
				exit(1);
			}
			pair->foreground = cpstr($3);
		}
	COMMA a_color
		{
			pair->background = cpstr($6);
			if (!cur_pair) {
				pair_list = pair;
				cur_pair = pair;
			} else {
				cur_pair->next = pair;
				cur_pair = pair;
			}
		}
	;

a_color: BLACK
		{ $$ = "COLOR_BLACK"; }
	| RED
		{ $$ = "COLOR_RED"; }
	| GREEN
		{ $$ = "COLOR_GREEN"; }
	| YELLOW
		{ $$ = "COLOR_YELLOW"; }
	| BLUE
		{ $$ = "COLOR_BLUE"; }
	| MAGENTA
		{ $$ = "COLOR_MAGENTA"; }
	| CYAN
		{ $$ = "COLOR_CYAN"; }
	| WHITE
		{ $$ = "COLOR_WHITE"; }
	;

forms:  FORM NAME 
		 { formname = cpstr($2); }
	AT coord 
		{
			form = malloc(sizeof (struct Form));
			if (!form) {
				fprintf(stderr,"Failed to allocate memory for form\n");
				exit(1);
			}
			form->y = y;
			form->x = x;
		}
	LBRACE formspec RBRACE
		{
			form->startfield = startname;
			form->colortable = colortable;
			form->height = height;
			form->width = width;
			form->attr = formattr;
			form->fieldlist = field_inst_list;
			field_inst_list = 0;
			form_bind_tuple(formname, FT_FORM, form);
		}
	;

formspec: height width startfield colortable formattr fieldlocs
	;

startfield:	/* empty */
		{	startname = 0; 
			printf("Warning: No start field specified for form %s\n", formname);
		}
	| STARTFIELD EQUALS NAME
		{ startname = cpstr($3); }
	;

colortable: /*empty */
		{ colortable = 0; }
	| COLORTABLE EQUALS NAME
		{ colortable = cpstr($3); }
	;

formattr: /* empty */
		{ formattr = 0; }
	| ATTR EQUALS NUMBER
		{ formattr = $3; }
	;

fieldlocs:	/* empty */
	| fieldlocs field_at
	;

field_at: NAME 
		 { fieldname = cpstr($1); }
	field_def AT coord
		{ 
			field = malloc(sizeof (struct Field));
			if (!field) {
				fprintf(stderr,"Failed to allocate memory for form field\n");
				exit(1);
			}
			if (!defname)
				field->defname = fieldname;
			else
				field->defname = defname;
			field->y = y;
			field->x = x;
		}
	links
		{
			field->fup = up;
			field->fdown = down;
			field->fleft = left;
			field->fright = right;
			field->fnext = next;
			if (!field_inst_list)
				field_inst_list = field;
			up = 0;
			down = 0;
			left = 0;
			right = 0;
			next = 0;
			if (!cur_field)
				cur_field = field;
			else {
				cur_field->next = field;
				cur_field = field;
			}
			form_bind_tuple(fieldname, FT_FIELD_INST, field);
		}
	;

fields: NAME 
		{ defname = cpstr($1); }
	field_spec
		{ define_field(defname); }
	;

field_def: /* empty */
		{ defname = 0; }
	| LBRACE NAME 
		{ defname = cpstr($2); }
	  RBRACE
	| field_spec
		{ defname = fieldname; define_field(defname); }
	;

field_spec: LBRACE height width attr selattr type RBRACE
	;

links: /* empty */
	| links COMMA conns
	;

conns: UP EQUALS NAME
		{ up = cpstr($3); }
	| DOWN EQUALS NAME
		{ down = cpstr($3); }
	| LEFT EQUALS NAME
		{ left = cpstr($3); }
	| RIGHT EQUALS NAME
		{ right = cpstr($3); }
	| NEXT EQUALS NAME
		{ next = cpstr($3); }
	;

type: textfield
	| inputfield
	| menufield
	| actionfield
	;

textfield:	TEXT EQUALS STRING
			{ type = FF_TEXT; text = cpstr($3); }
	;

inputfield:	inputspec
			{ type = FF_INPUT; }
	;

inputspec:	LABEL EQUALS STRING limit
			{ lbl_flag = 1; label = cpstr($3); }
	| DEFAULT EQUALS STRING limit
			{ lbl_flag = 0; label = cpstr($3); }
	;

limit: /* empty */
	| LIMIT EQUALS NUMBER
			{ limit = $3; }

menufield: SELECTED EQUALS NUMBER OPTIONS EQUALS menuoptions
			{ type = FF_MENU; selected = $3; }
	;

menuoptions: menuoption
	| menuoptions COMMA menuoption
	;

menuoption: STRING
			{	
				menu = malloc(sizeof(struct MenuList));
				if (!menu) {
						err(1, "Couldn't allocate memory for menu option\n");
				}
				menu->option = cpstr($1);
				if (!cur_menu) {  
					menu_list = menu;
					cur_menu = menu;
				} else {
					cur_menu->next = menu; 
					cur_menu = menu;
				}
			}
;

actionfield: ACTION EQUALS STRING FUNC EQUALS NAME
			{ type = FF_ACTION; text = cpstr($3); function = cpstr($6); }
	;

height: /* empty */
		{ height = 0; }
	| HEIGHT EQUALS NUMBER
		{ height = $3; }
	;

width: /* empty */
			{ width = 0; }
	| WIDTH EQUALS NUMBER
			{ width = $3; }
	;

attr: /* empty */
			{ attr = 0; }
	| ATTR EQUALS NUMBER
			{ attr = $3; }
	;

selattr: /* empty */
			{ selattr = 0; }
	| SELATTR EQUALS NUMBER
			{ selattr = $3; }
	;

coord: NUMBER COMMA NUMBER
		{ y = $1; x = $3; }
	;

%%

void
yyerror (char *error)
{
	fprintf(stderr, "%s at line %d\n",error, lineno);
	exit(1);
}

char *
cpstr(char *ostr)
{
	char *nstr;

	nstr = malloc(strlen(ostr)+1);
	if (!nstr) {
		fprintf(stderr, "Couldn't allocate memory for string\n");
		exit(1);
	}
	strcpy(nstr, ostr);
	return (nstr);
}

/* Calculate a default height for a field */

void
calc_field_height(struct Field *field, char *string)
{

	int len;

	len = strlen(string);

	if (!field->width) {
		/*
		 * This is a failsafe, this routine shouldn't be called
		 * with a width of 0, the width should be determined
		 * first.
		 */
		field->height = 1;
		return;
	}

	if (len < field->width) {
		field->height = 1;
		return;
	} else
		field->height = len / field->width;

	if ((field->height*field->width) < len)
		field->height++;

	return;
}

void
define_field(char *defname)
{
	struct Field *field;
	struct MenuList *menu_options;
	int no_options;

	field = malloc(sizeof (struct Field));
	if (!field) {
		fprintf(stderr,"Failed to allocate memory for form field\n");
		exit(1);
	}
	field->defname = defname;
	field->type = type;
	field->height = height;
	field->width = width;
	field->attr = attr;
	field->selattr = selattr;
	switch (type) {
		case FF_TEXT:
			field->field.text = malloc(sizeof (struct TextField));
			if (!field->field.text) {
				fprintf(stderr,
						"Failed to allocate memory for text field\n");
				exit (1);
			}
			field->field.text->text = text;
			break;
		case FF_INPUT:
			field->field.input = malloc(sizeof (struct InputField));
			if (!field->field.input) {
				fprintf(stderr,
						"Failed to allocate memory for input field\n");
				exit (1);
			}
			field->field.input->lbl_flag = lbl_flag;
			field->field.input->label = label;
			field->field.input->limit = limit;
			break;
		case FF_MENU:
			printf("field type %s = %d\n", defname,field->type);
			field->field.menu = malloc(sizeof (struct MenuField));
			if (!field->field.menu) {
				fprintf(stderr,
						"Failed to allocate memory for menu field\n");
				exit (1);
			}
			field->field.menu->selected = selected;
			menu_options = menu_list;
			field->field.menu->no_options = 0;
			field->field.menu->options = 0;
			for (; menu_options; menu_options = menu_options->next) {
				no_options = add_menu_option(field->field.menu,
											 menu_options->option);
				if (!no_options)
					err(1, "Couldn't add menu option");
			}
			field->field.menu->no_options = no_options;
			cur_menu = 0;
			break;
		case FF_ACTION:
			field->field.action = malloc(sizeof (struct ActionField));
			if (!field->field.action) {
				fprintf(stderr,
						"Failed to allocate memory for action field\n");
				exit (1);
			}
			field->field.action->text = text;
			field->field.action->fn = (void *) function;
			break;
		default:
			break;
	}
	form_bind_tuple(defname, FT_FIELD_DEF, field);
	width=0;
	height = 0;
	attr=0;
	selattr=0;
	limit=0;
}
