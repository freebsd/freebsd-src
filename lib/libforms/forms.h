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

#define F_END 0
#define F_TEXT 1
#define F_ACTION 2
#define F_INPUT 3
#define F_MENU 4

#define F_DEFATTR 0
#define F_SELATTR A_REVERSE

#define F_DONE 1
#define F_CANCEL -1

struct col_pair {
	int f;
	int b;
};

struct form {
	int no_fields;
	int start_field;
	int current_field;
	struct field *field;
	int height;
	int width;
	int y;
	int x;
	int attr;
	struct col_pair *color_table;
	WINDOW *window;
};

struct text_field {
	char *text;
};

struct action_field {
	char *text;
	void (* fn)();
};

struct input_field {
	int lbl_flag;
	char *label;
	char *input;
	int limit;
};

struct menu_field {
	int no_options;
	int selected;
	char **options;
};

struct help_link {
};

struct field {
	int type;
	int y;
	int x;
	int width;
	int attr;
	int selattr;
	int next;
	int up;
	int down;
	int left;
	int right;
	union {
		struct text_field *text;
		struct action_field *action;
		struct input_field *input;
		struct menu_field *menu;
	}field;
	/*
	struct help_link help;
	*/
};

/* Externally visible keymap table for user-definable keymaps */
extern unsigned int keymap[];

/* Externally visible function declarations */
int update_form(struct form *);
int initfrm(struct form *);
void endfrm(struct form *);
void exit_form(void);
void cancel_form(void);
