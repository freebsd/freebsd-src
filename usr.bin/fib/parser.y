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

char *startname;
char *formname;
char *text;
char *label;
char *fieldname;
char *function;
char *up, *down, *left, *right, *next;
char *linkname;
int height, width;
int y, x;
int width;
int limit;
char *attr;
int type;
int lbl_flag;
int selected, no_options=0;
FILE *outf;

struct field_list {
	char *fieldname;
	char *defname;
	char *attr;
	struct field field;
	struct field_list *next;
};

struct field_list *cur_field;
struct field_list *field_list;
struct field_list *field;
struct field_list *cur_form_field;
struct field_list *form_field_list;

struct link_list {
	char *linkname;
	char *fieldname;
	char *up;
	char *down;
	char *left;
	char *right;
	char *next;
	struct link_list *lnext;
};

struct link_list *cur_link;
struct link_list *link_list;
struct link_list *link;

struct form_list {
	char *formname;
	struct form form;
	struct form_list *next;
	struct field_list *fields;
	char *startfield;
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
%}

%union {
	int ival;
	char *sval;
}

%token <ival> FORM
%token <sval> NAME
%token <sval> STRING
%token <ival> AT
%token <ival> AS
%token <ival> HEIGHT
%token <ival> EQUALS
%token <ival> NUMBER
%token <ival> WIDTH
%token <ival> STARTFIELD
%token <ival> COLON
%token <ival> LBRACE
%token <ival> RBRACE
%token <ival> FIELD
%token <ival> TEXT
%token <ival> ATTR
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

%start spec

%%

spec: fields links forms
		{
			outf = fopen("frm.tab.h", "w");
			if (!outf) {
				perror("Couldn't open output file:");
				exit (1);
			}
			output_forms(outf);
			if (fclose(outf) == EOF) {
				perror("Error closing output file:");
				exit (1);
			}
		}
	;

fields: field
	| fields field
	;

links: /* empty */
	| link
	| links link
	;

link: LINK NAME 
		{
			linkname = cpstr($2); 
			fieldname = 0;
			up = 0;
			down = 0;
			left = 0;
			right = 0;
			next = 0;
		}
	alias LBRACE	connspec RBRACE
		{
			link = malloc(sizeof (struct link_list));
			if (!link) {
				fprintf(stderr,"Failed to allocate memory for link\n");
				exit(1);
			}
			link->linkname = linkname;
			link->fieldname = fieldname;
			link->up = up;
			link->down = down;
			link->left = left;
			link->right = right;
			link->next = next;
			if (!cur_link) {
				link_list = link;
				cur_link = link;
			} else {
				cur_link->lnext = link;
				cur_link = link;
			}
		}	
	;

alias: /* empty */
	| AS NAME
		{ fieldname = cpstr($2); }
	;

connspec: conns
	| connspec conns
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

forms: form
	| forms form
	;

form: FORM NAME 
		{ formname = cpstr($2); }
	AT coord 
		{
			form = malloc(sizeof (struct form_list));
			if (!form) {
				fprintf(stderr,"Failed to allocate memory for form\n");
				exit(1);
			}
			form->form.y = y;
			form->form.x = x;
		}
	LBRACE formspec RBRACE
		{
			form->startfield = startname;
			form->formname = formname;
			form->form.height = height;
			form->form.width = width;
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

formspec: height width startfield fieldlocs
	;

height:	HEIGHT EQUALS NUMBER
		{ height = $3; }
	;

width:	WIDTH EQUALS NUMBER
		{ width = $3; }
	;

startfield:	/* empty */
		{	startname = 0; 
			printf("Warning: No start field specified for form %s\n", formname);
		}
	| STARTFIELD EQUALS NAME
		{ startname = cpstr($3); }
	;

fieldlocs:	/* empty */
	| afield
	| fieldlocs afield
	;

afield: FIELD NAME 
		{ fieldname = cpstr($2); }
	AT coord
		{
			field = malloc(sizeof (struct field_list));
			if (!field) {
				fprintf(stderr,"Failed to allocate memory for form field\n");
				exit(1);
			}
			field->fieldname = fieldname;
			field->field.y = y;
			field->field.x = x;
			if (!cur_form_field) {
				form_field_list = field;
				cur_form_field = field;
			} else {
				cur_form_field->next = field;
				cur_form_field = field;
			}
		}
	;

coord: NUMBER COLON NUMBER
		{ y = $1; x = $3; }
	;

field: FIELD NAME 
		{ fieldname = cpstr($2); }
	LBRACE fieldspec RBRACE
		{
			field = malloc(sizeof (struct field_list));
			if (!field) {
				fprintf(stderr,"Failed to allocate memory for field\n");
				exit(1);
			}
			field->fieldname = fieldname;
			field->field.type = type;
			field->field.width = width;
			field->attr = attr;
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
			if (!cur_field) {
				field_list = field;
				cur_field = field;
			} else {
				cur_field->next = field;
				cur_field = field;
			}
			width=0;
			attr=0;
			limit=0;
		}
	;

fieldspec: width attr type

type: textfield
	| inputfield
	| menufield
	| actionfield
	| error
		{ fprintf(stderr, "Uknown field type at %d\n", lineno); }
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
	| menuoptions COLON menuoption
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
				fprintf(outf, "struct text_field %s = {\"%s\"};\n",
					fields->fieldname,
					fields->field.field.text->text);
			}
			break;
		case F_INPUT:
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
					(fieldname)?fieldname:fields->fieldname,
					(fields->field.field.input->lbl_flag ? 1 : 0),
					fields->field.field.input->label,
					fields->field.field.input->limit);
			break;
		case F_MENU:
			if (!fieldname) {
				fprintf(outf, "char *%s_options[] = {",
					fields->fieldname);
				menu = (struct menu_list *)fields->field.field.menu->options;
				lim = 0;
				for (i=0; i < fields->field.field.menu->no_options - 1; i++) {
					fprintf(outf, "\"%s\", ", menu->option);
					menu = menu->next;
					len = strlen(menu->option);
					if (len > lim)
						lim = len;
				}
				if (!fields->field.width)
					fields->field.width = lim;
				fprintf(outf, "\"%s\"};\n", menu->option);
			}
			fprintf(outf, "struct menu_field %s = {%d, %d, %s_options};\n",
					(fieldname)?fieldname:fields->fieldname,
					fields->field.field.menu->no_options,
					fields->field.field.menu->selected,
					fields->fieldname);
			break;
		case F_ACTION:
			if (!fields->field.width)
				fields->field.width = strlen(fields->field.field.action->text);
			fprintf(outf, "struct action_field %s = {\"%s\", &%s};\n",
					(fieldname)?fieldname:fields->fieldname,
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
	for (fields = field_list; fields; fields=fields->next)
		output_field(fields, 0, outf);

	for(forms=form_list; forms; forms = forms->next) {
		parse_form(forms, outf);
	}
}

parse_form(struct form_list *form, FILE *outf)
{
	struct field_list *fields;
	struct field_list *def;
	struct field_list *info;
	struct link_list *links;
	char *fieldname;
	int no_fields = 0;

	/*
	 * Run through the specific instances of the fields referenced by
	 * this form, filling in the link structures and outputing a field
	 * definition for this particular instance.
	 */
	for(fields=form->fields; fields; fields=fields->next) {
		
		fieldname = fields->fieldname;

		/* Search links list for an entry for this field */
		for (links = link_list; links; links = links->lnext) {
			if (!strcmp(links->linkname, fields->fieldname)) {
				/* Check for an alias */
				if (links->fieldname)
					fieldname = links->fieldname;
				fields->fieldname = links->linkname;
				fields->field.up = field_id(links->up, form->fields);
				fields->field.down = field_id(links->down, form->fields);
				fields->field.left = field_id(links->left, form->fields);
				fields->field.right = field_id(links->right, form->fields);
				fields->field.next = field_id(links->next, form->fields);
				break;
			}
		}

		if (!links) {
			/* No links for this field */
			fields->field.up = -1;
			fields->field.down = -1;
			fields->field.left = -1;
			fields->field.right = -1;
			fields->field.next = -1;
		}

		for (def = field_list; def; def=def->next)
			if (!strcmp(fieldname, def->fieldname))
				break;
		if (!def) {
			fprintf(stderr,
					"%s not found in field definitions, field not output\n",
					fieldname);
		} else {
			fields->field.type = def->field.type;
			fields->field.width = def->field.width;
			fields->field.attr = def->field.attr;

			output_field(def, fields->fieldname, outf);

			if ((fields->field.type == F_TEXT) 
				|| (fields->field.type == F_ACTION))
				fields->defname = def->fieldname;
			else
				fields->defname = fields->fieldname;
		}
	}

	/* Output the field table for this form */

	fprintf(outf, "struct field %s_fields[] = {\n", form->formname);

	for(fields=form->fields; fields; fields=fields->next) {
		++no_fields;
		fprintf(outf, "\t{%d, %d, %d, %d, %s, ",
			fields->field.type, fields->field.y, fields->field.x,
			fields->field.width, (fields->attr ? fields->attr : "F_DEFATTR"));
		fprintf(outf, "%d, %d, %d, %d, %d, ",
			fields->field.next,
			fields->field.up,
			fields->field.down,
			fields->field.left,
			fields->field.right);
		fprintf(outf, "(struct text_field *)&%s},\n",
			fields->defname);
	}

	fprintf(outf, "\t{%d, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}\n};\n", F_END);

	/* If no start field set then find first non-text field */
	if (!form->startfield) 
		for (fields = form->fields; fields; fields = fields->next)
			if (fields->field.type != F_TEXT) {
				form->startfield = fields->fieldname;
				break;
			}

	fprintf(outf,
			"struct form %s = {%d, %d, %s_fields, %d, %d, %d, %d, 0};\n\n",
			form->formname, no_fields,
			field_id(form->startfield, form->fields),
			form->formname, form->form.height, form->form.width,
			form->form.y, form->form.x);
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

	for (fields = field_list; fields; fields = fields->next, id++) {
		if (!strcmp(fieldname, fields->fieldname))
			return (id);
	}

	fprintf(stderr, "Field %s, not found in specification\n", fieldname);
	return (-1);
}
