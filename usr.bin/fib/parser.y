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
char *formattr;
char *text;
char *label;
char *function;
char *up, *down, *left, *right, *next;
int height, width;
int y, x;
int width;
int limit;
char *attr;
char *selattr;
int type;
int lbl_flag;
int selected, no_options=0;

extern FILE *outf;

struct field_list {
	char *fieldname;
	char *defname;
	char *attr;
	char *selattr;
	char *up;
	char *down;
	char *left;
	char *right;
	char *next;
	struct field field;
	struct field_list *next_field;
};

struct field_list *cur_field_def;
struct field_list *field_defs;
struct field_list *field;
struct field_list *cur_form_field;
struct field_list *form_field_list;

struct form_list {
	char *formname;
	struct form form;
	struct form_list *next;
	struct field_list *fields;
	char *startfield;
	char *formattr;
	char *colortable;
};

struct form_list *cur_form;
struct form_list *form_list;
struct form_list *form;

struct menu_list {
	char *option;
	struct menu_list *next;
};

struct menu_list *cur_menu;
struct menu_list *menu_list;
struct menu_list *menu;

struct color_list {
	char *foreground;
	char *background;
	struct color_list *next;
};
struct color_list *pair;
struct color_list *cur_pair;
struct color_list *color_list;

struct color_table {
	char *tablename;
	struct color_list *pairs;
	struct color_table *next;
};

struct color_table *color_table;
struct color_table *cur_table;
struct color_table *color_tables;

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
			color_table->pairs = color_list;
			cur_pair = 0;
			if (!cur_table) {
				color_tables = color_table;
				cur_table = color_table;
			} else {
				cur_table->next = color_table;
				cur_table = color_table;
			}
		}
	;

color_pairs: /* empty */
	| color_pairs pair
	;

pair: PAIR EQUALS a_color
		{
			pair = malloc(sizeof (struct color_list));
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
				color_list = pair;
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
	AT coord LBRACE formspec RBRACE
		{
			form = malloc(sizeof (struct form_list));
			if (!form) {
				fprintf(stderr,"Failed to allocate memory for form\n");
				exit(1);
			}
			form->form.y = y;
			form->form.x = x;
			form->startfield = startname;
			form->formname = formname;
			form->colortable = colortable;
			form->form.height = height;
			form->form.width = width;
			form->formattr = formattr;
			form->fields = form_field_list;
			cur_form_field = 0;
			if (!cur_form) {
				form_list = form;
				cur_form = form;
			} else {
				cur_form->next = form;
				cur_form = form;
			}
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
	| ATTR EQUALS NAME
		{ formattr = cpstr($3); }
	;

fieldlocs:	/* empty */
	| fieldlocs field_at
	;

field_at: NAME 
		 { fieldname = cpstr($1); }
	field_def AT coord
		{ 
			field = malloc(sizeof (struct field_list));
			if (!field) {
				fprintf(stderr,"Failed to allocate memory for form field\n");
				exit(1);
			}
			field->fieldname = fieldname;
			if (!defname)
				field->defname = fieldname;
			else
				field->defname = defname;
			field->field.y = y;
			field->field.x = x;
		}
	links
		{
			field->up = up;
			field->down = down;
			field->left = left;
			field->right = right;
			field->next = next;
			up = 0;
			down = 0;
			left = 0;
			right = 0;
			next = 0;
			if (!cur_form_field) {
				form_field_list = field;
				cur_form_field = field;
			} else {
				cur_form_field->next_field = field;
				cur_form_field = field;
			}
		}
	;

fields: NAME 
		{ defname = cpstr($1); }
	field_spec
		{ define_field(defname, 0); }
	;

field_def: /* empty */
		{ defname = 0; }
	| LBRACE NAME 
		{ defname = cpstr($2); }
	  RBRACE
	| field_spec
		{ defname = fieldname; define_field(defname, 0); }
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
			{ type = F_TEXT; text = cpstr($3); }
	;

inputfield:	inputspec
			{ type = F_INPUT; }
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
			{ type = F_MENU; selected = $3; }
	;

menuoptions: menuoption
	| menuoptions COMMA menuoption
	;

menuoption: STRING
			{	menu = malloc(sizeof(struct menu_list));
				if (!menu) {
					fprintf(stderr,
							"Couldn't allocate memory for menu option\n");
					exit (1);
				}
				menu->option = cpstr($1);
				++no_options;
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
			{ type = F_ACTION; text = cpstr($3); function = cpstr($6); }
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
	| ATTR EQUALS NAME
			{ attr = cpstr($3); }
	;

selattr: /* empty */
			{ selattr = 0; }
	| SELATTR EQUALS NAME
			{ selattr = cpstr($3); }
	;

coord: NUMBER COMMA NUMBER
		{ y = $1; x = $3; }
	;

%%

yyerror (char *error)
{
	fprintf(stderr, "%s at line %d\n",error, lineno);
	exit(1);
}

yywrap()
{
	return(1);
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

output_field(struct field_list *fields, char *fieldname, FILE *outf)
{
	struct menu_list *menu;
	int i, lim, len;

	switch(fields->field.type) {
		case F_TEXT:
			if (!fieldname) {
				if (!fields->field.width)
					fields->field.width = strlen(fields->field.field.text->text);
				if (!fields->field.height)
					calc_field_height(&fields->field, fields->field.field.text->text);

				fprintf(outf, "struct text_field %s = {\"%s\"};\n",
					fields->defname,
					fields->field.field.text->text);
			}
			break;
		case F_INPUT:
			/* Force height to one regardless */
			fields->field.height = 1;
			if (!fields->field.width && !fields->field.field.input->limit) {
				fields->field.width = strlen(fields->field.field.input->label);
				fields->field.field.input->limit = fields->field.width;
			} else if (!fields->field.width)
				fields->field.width = fields->field.field.input->limit;
			else if (!fields->field.field.input->limit)
				fields->field.field.input->limit = fields->field.width;
			if (fields->field.field.input->limit < fields->field.width)
				fields->field.width = fields->field.field.input->limit;
			fprintf(outf, "struct input_field %s = {%d, \"%s\", 0, %d};\n",
					(fieldname)?fieldname:fields->defname,
					(fields->field.field.input->lbl_flag ? 1 : 0),
					fields->field.field.input->label,
					fields->field.field.input->limit);
			break;
		case F_MENU:
			/* Force height to one regardless */
			fields->field.height = 1;
			if (!fieldname) {
				fprintf(outf, "char *%s_options[] = {",
					fields->defname);
				menu = (struct menu_list *)fields->field.field.menu->options;
				lim = 0;
				for (i=0; i < fields->field.field.menu->no_options - 1; i++) {
					fprintf(outf, "\"%s\", ", menu->option);
					len = strlen(menu->option);
					if (len > lim)
						lim = len;
					menu = menu->next;
				}
				if (!fields->field.width)
					fields->field.width = lim;
				fprintf(outf, "\"%s\"};\n", menu->option);
			}
			fprintf(outf, "struct menu_field %s = {%d, %d, %s_options};\n",
					(fieldname)?fieldname:fields->defname,
					fields->field.field.menu->no_options,
					fields->field.field.menu->selected,
					fields->defname);
			break;
		case F_ACTION:
			if (!fields->field.width)
				fields->field.width = strlen(fields->field.field.action->text);
			if (!fields->field.height)
				calc_field_height(&fields->field, fields->field.field.action->text);
			fprintf(outf, "struct action_field %s = {\"%s\", &%s};\n",
					(fieldname)?fieldname:fields->defname,
					fields->field.field.action->text,
					fields->field.field.action->fn);
			break;
		default:
			break;
	}
}

output_forms(FILE *outf)
{
	struct form_list *forms;
	struct field_list *fields;

	/* Output the general field definitions */
	for (fields = field_defs; fields; fields=fields->next_field)
		output_field(fields, 0, outf);

	for(forms=form_list; forms; forms = forms->next) {
		parse_form(forms, outf);
	}
}

output_coltab(struct color_table *coltab, FILE *outf)
{
	struct color_list *pairs;

	/* Output the color pair table */

	fprintf(outf, "struct col_pair %s_coltab[] = {\n",coltab->tablename);

	if (color_list) {
		for(pairs=coltab->pairs; pairs; pairs=pairs->next)
			fprintf(outf, "\t{%s, %s},\n",
					pairs->foreground, pairs->background);
	} else {
		/* Output a default color table */
		fprintf(outf, "\t{7, 0}\n};");
	}
	fprintf(outf, "\t{-1, -1}\n};\n\n");
}

parse_form(struct form_list *form, FILE *outf)
{
	struct field_list *fields;
	struct field_list *defs;
	struct field_list *info;
	struct color_table *coltab;
	char *fieldname;
	int no_fields = 0;

	/* If there's a color table defined find it and output it */
	if (form->colortable) {
		for (coltab = color_tables; coltab; coltab = coltab->next)
			if (!strcmp(form->colortable, coltab->tablename)) {
				output_coltab(coltab, outf);
				break;
			}
		if (!coltab) {
			fprintf(stderr, "Color table for form %s not found\n",
					form->formname);
			form->colortable = 0;
		}
	}
			
	/*
	 * Run through the specific instances of the fields referenced by
	 * this form, filling in the link structures and outputing a field
	 * definition for this particular instance.
	 */
	for(fields=form->fields; fields; fields=fields->next_field) {
		
		/* Fill in link values */

		fields->field.up = field_id(fields->up, form->fields);
		fields->field.down = field_id(fields->down, form->fields);
		fields->field.left = field_id(fields->left, form->fields);
		fields->field.right = field_id(fields->right, form->fields);
		fields->field.next = field_id(fields->next, form->fields);

		/* Search field list for  definition of this field */

		for(defs=field_defs; defs; defs=defs->next_field) {
			if (!strcmp(fields->defname, defs->defname))
				break;
		}

		if (!defs) {
			fprintf(stderr,
					"%s not found in field definitions, field not output\n",
					fields->defname);
			fields->fieldname = 0;
		} else {
			fields->field.type = defs->field.type;
			fields->field.height = defs->field.height;
			fields->field.width = defs->field.width;
			fields->attr = defs->attr;
			fields->selattr = defs->selattr;
			if (strcmp(fields->defname, fields->fieldname))
				output_field(defs, fields->fieldname, outf);
		}
	}

	/* Output the field table for this form */

	fprintf(outf, "struct field %s_fields[] = {\n", form->formname);

	for(fields=form->fields; fields; fields=fields->next_field) {
		if (fields->fieldname) {
			++no_fields;
			fprintf(outf, "\t{%d, %d, %d, %d, %d, %s, %s, ",
				fields->field.type, fields->field.y, fields->field.x,
				fields->field.height, fields->field.width,
				(fields->attr ? fields->attr : "F_DEFATTR"),
				(fields->selattr ? fields->selattr : "F_SELATTR"));
			fprintf(outf, "%d, %d, %d, %d, %d, ",
				fields->field.next,
				fields->field.up,
				fields->field.down,
				fields->field.left,
				fields->field.right);
			fprintf(outf, "(struct text_field *)&%s},\n",
				fields->fieldname);
		}
	}

	fprintf(outf, "\t{%d, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}\n};\n", F_END);

	/* If no start field set then find first non-text field */
	if (!form->startfield) 
		for (fields = form->fields; fields; fields = fields->next_field)
			if (fields->field.type != F_TEXT) {
				form->startfield = fields->fieldname;
				break;
			}

	fprintf(outf,
			"struct form %s = {%d, %d, %d, %s_fields, %d, %d, %d, %d, %s, ",
			form->formname, no_fields,
			field_id(form->startfield, form->fields),
			field_id(form->startfield, form->fields),
			form->formname, form->form.height, form->form.width,
			form->form.y, form->form.x,
			(form->formattr ? form->formattr : "F_DEFATTR"));
	if (form->colortable)
		fprintf(outf, "%s_coltab, ", form->colortable);
	else
		fprintf(outf, "0, ");
	fprintf(outf, "0};\n\n");
			
}

/*
 * Search a field list for a particular fieldname
 * and return an integer id for that field based on
 * it's position in the list [0..n]. Return -1 if
 * there's no match.
 */

int
field_id(char *fieldname, struct field_list *field_list)
{
	struct field_list *fields;
	int id=0;

	/* If the strings null then fail immediately */

	if (!fieldname)
		return (-1);

	for (fields = field_list; fields; fields = fields->next_field, id++) {
		if (!strcmp(fieldname, fields->fieldname))
			return (id);
	}

	fprintf(stderr, "Field %s, not found in specification\n", fieldname);
	return (-1);
}

/* Calculate a default height for a field */

calc_field_height(struct field *field, char *string)
{

	int len;

	len = strlen(string);

	if (!width) {
		/*
		 * This is a failsafe, this routine shouldn't be called
		 * with a width of 0, the width should be determined
		 * first.
		 */
		height = 1;
		return;
	}

	if (len < field->width) {
		field->height = 1;
		return;
	} else
		field->height = len / width;

	if ((field->height*width) < len)
		field->height++;

	return;
}

/* Add a field to the pre-defined fields list */

define_field(char *defname, char *fieldname)
{
	field = malloc(sizeof (struct field_list));
	if (!field) {
		fprintf(stderr,"Failed to allocate memory for form field\n");
		exit(1);
	}
	field->fieldname = fieldname;
	field->defname = defname;
	field->field.type = type;
	field->field.height = height;
	field->field.width = width;
	field->attr = attr;
	field->selattr = selattr;
	switch (type) {
		case F_TEXT:
			field->field.field.text = malloc(sizeof (struct text_field));
			if (!field->field.field.text) {
				fprintf(stderr,
						"Failed to allocate memory for text field\n");
				exit (1);
			}
			field->field.field.text->text = text;
			break;
		case F_INPUT:
			field->field.field.input = malloc(sizeof (struct input_field));
			if (!field->field.field.input) {
				fprintf(stderr,
						"Failed to allocate memory for input field\n");
				exit (1);
			}
			field->field.field.input->lbl_flag = lbl_flag;
			field->field.field.input->label = label;
			field->field.field.input->limit = limit;
			break;
		case F_MENU:
			field->field.field.menu = malloc(sizeof (struct menu_field));
			if (!field->field.field.menu) {
				fprintf(stderr,
						"Failed to allocate memory for menu field\n");
				exit (1);
			}
			field->field.field.menu->selected = selected;
			field->field.field.menu->no_options = no_options;
			no_options = 0;
			field->field.field.menu->options = (char **)menu_list;
			cur_menu = 0;
			break;
		case F_ACTION:
			field->field.field.action = malloc(sizeof (struct action_field));
			if (!field->field.field.action) {
				fprintf(stderr,
						"Failed to allocate memory for action field\n");
				exit (1);
			}
			field->field.field.action->text = text;
			field->field.field.action->fn = (void *) function;
			break;
		default:
			break;
	}
	if (!cur_field_def) {
		field_defs = field;
		cur_field_def = field;
	} else {
		cur_field_def->next_field = field;
		cur_field_def = field;
	}
	width=0;
	height = 0;
	attr=0;
	selattr=0;
	limit=0;
}
