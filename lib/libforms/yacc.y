%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <forms.h>

extern struct form *form;

struct field *cur_field;
int id,next, up, down, left, right;
%}

%start file

%union {
	int ival;
	char *sval;
}

%token <sval> STRING
%token <ival> NUMBER
%token <ival> INPUT
%token <ival> TEXT
%token <ival> FORM

%type <sval> String

%%

file : /* Empty */
	| file form
	| file fields
	| error
	;

form : FORM NUMBER NUMBER NUMBER NUMBER 
	{
		form = (struct form *) malloc(sizeof(struct form));
		if (!form)
			return(-1);
		form->x = $2;
		form->y = $3;
		form->height = $4;
		form->width = $5;
		form->fields = 0;
	}
	;

fields : input
	| text
	;

input : INPUT NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER String NUMBER NUMBER NUMBER NUMBER String
	{
		if (alloc_field() == -1)
			return(-1);
		cur_field->field_id = $2;
		/*
		 * These will hold addresses but store 
		 * the field id's in them temporarily
		 */
		cur_field->next = (struct field *) $3;
		cur_field->up = (struct field *) $4;
		cur_field->down = (struct field *) $5;
		cur_field->left = (struct field *) $6;
		cur_field->right = (struct field *) $7;
		cur_field->type = FORM_FTYPE_INPUT;
		cur_field->entry.input.y_prompt = $8;
		cur_field->entry.input.x_prompt = $9;
		cur_field->entry.input.prompt_width = $10;
		cur_field->entry.input.prompt_attr = FORM_DEFAULT_ATTR;
		cur_field->entry.input.prompt = (char *) malloc($10+1);
		if (!cur_field->entry.input.prompt)
			return(-1);
		strncpy(cur_field->entry.input.prompt, $11, $10);
		cur_field->entry.input.prompt[$10] = 0;
		cur_field->entry.input.y_field = $12;
		cur_field->entry.input.x_field = $13;
		cur_field->entry.input.field_width = $14;
		cur_field->entry.input.max_field_width = $15;
		cur_field->entry.input.field = (char *) malloc(strlen($16));
		cur_field->entry.input.field_attr = FORM_DEFAULT_ATTR;
		if (!cur_field->entry.input.field)
			return(-1);
		strcpy(cur_field->entry.input.field, $16);
	}
	;

text : TEXT NUMBER NUMBER String
	{
		if (alloc_field() == -1)
			return(-1);
		cur_field->field_id = 0;
		cur_field->type = FORM_FTYPE_TEXT;
		cur_field->entry.text.y = $2;
		cur_field->entry.text.x = $3;
		cur_field->entry.text.attr = FORM_DEFAULT_ATTR;
		cur_field->entry.text.text = (char *) malloc(strlen($4));
		if (!cur_field->entry.text.text)
			return (-1);
		strcpy(cur_field->entry.text.text, $4);
	}
	;

String : STRING
	{
		char *t, *old, *new;

		t = strdup($1);
		free($1);
		/*
		 * Deal with any escaped characters,
		 * only works for " and \ really.
		 */
		for (old=t, new=t; *old; old++, new++) {
			if (*old == '\\')
				old++;
			*new = *old;
		}
		*new = '\0';
		$$ = t;
	}

%%
void
yyerror(char *s)
{
	printf("%s\n", s);
	exit(1);
}

int
alloc_field()
{
	if (!form->fields) {
		form->fields = (struct field *) malloc(sizeof(struct field));
		if (!form->fields)
			return(-1);
		cur_field = form->fields;
	} else {
		cur_field->link = (struct field *) malloc(sizeof(struct field));
		if (!cur_field->link)
			return(-1);
		cur_field = cur_field->link;
	}

	return(0);
}
