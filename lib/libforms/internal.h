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

#define F_UP		keymap[0]
#define F_DOWN		keymap[1]
#define F_RIGHT	keymap[2]
#define F_LEFT		keymap[3]
#define F_NEXT		keymap[4]
#define F_CLEFT	keymap[5]
#define F_CRIGHT	keymap[6]
#define F_CHOME	keymap[7]
#define F_CEND		keymap[8]
#define F_CBS		keymap[9]
#define F_CDEL		keymap[10]
#define F_ACCEPT	F_NEXT

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
