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

#define F_UP		f_keymap[0]
#define F_DOWN		f_keymap[1]
#define F_RIGHT		f_keymap[2]
#define F_LEFT		f_keymap[3]
#define F_NEXT		f_keymap[4]
#define F_CLEFT		f_keymap[5]
#define F_CRIGHT	f_keymap[6]
#define F_CHOME		f_keymap[7]
#define F_CEND		f_keymap[8]
#define F_CBS		f_keymap[9]
#define F_CDEL		f_keymap[10]
#define F_ACCEPT	f_keymap[11]

/* Private function declarations */
static void show_form(struct form *);
static void disp_text(struct form *, int);
static void disp_menu(struct form *, int);
static void disp_action(struct form *, int);
static void disp_input(struct form *, int);
static void field_menu(struct form *);
static void field_input(struct form *);
static void field_action(struct form *);
static int print_string(WINDOW *, int, int, int, char *);
static void print_status(char *);
static int next_field(struct form *form, int);
