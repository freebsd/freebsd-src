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

#include <ncurses.h>

#define FF_UNKNOWN 0
#define FF_TEXT 1
#define FF_ACTION 2
#define FF_INPUT 3
#define FF_MENU 4

#define F_DEFATTR 0
#define F_SELATTR A_REVERSE

/* Status values */

#define FS_ERROR	-1
#define FS_OK		0
#define FS_EXIT		1
#define FS_CANCEL	2
#define FS_NOBIND	3
#define FS_RUNNING  4


typedef enum {
	FT_ANY,
	FT_FORM, 
	FT_COLTAB,
	FT_FIELD_INST,
	FT_FIELD_DEF,
	FT_FUNC
} TupleType;

struct Tuple {
	char *name;
	int type;
	void *addr;
	struct Tuple *next;
};

struct col_pair {
	int f;
	int b;
};

struct Form {
	int status;
	int no_fields;
	char *startfield;
	struct Field *current_field;
	struct Field *prev_field;
	int height;
	int width;
	int y;
	int x;
	int attr;
	char *colortable;
	WINDOW *window;
	hash_table *bindings;
};

struct TextField {
	char *text;
};

struct ActionField {
	char *text;
	char *fn;
};

struct InputField {
	int lbl_flag;
	char *label;
	char *input;
	int limit;
};

struct MenuField {
	int selected;
	int no_options;
	char **options;
};

struct help_link {
};

struct Field {
	char *defname;
	char *enter;
	char *leave;
	int type;
	int y;
	int x;
	int height;
	int width;
	int attr;
	int selattr;
	char *fnext;
	char *fup;
	char *fdown;
	char *fleft;
	char *fright;
	char *f_keymap;
	union {
		struct TextField *text;
		struct ActionField *action;
		struct InputField *input;
		struct MenuField *menu;
	}field;
	/*
	struct help_link help;
	*/
};

/* Externally visible keymap table for user-definable keymaps */
extern unsigned int keymap[];

/* Externally visible function declarations */
struct Form *form_start(char *);
struct Tuple *form_get_tuple(hash_table *, char *, TupleType);
int form_bind_tuple(hash_table *, char *, TupleType, void *);
void print_status(char *);
void exit_form(struct Form *form);
void cancel_form(struct Form *form);
void print_status(char *);
int add_menu_option(struct MenuField *, char *);
