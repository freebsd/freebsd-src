/*
 * Copyright (c) 1995 Paul Richards. 
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
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

struct form {
	int no_fields;
	int current_field;
	struct field *field;
	int nlines;
	int ncols;
	int y;
	int x;
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
	int disp_width;
	int width;
	int attr;
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
