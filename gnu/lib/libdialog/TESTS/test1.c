/*
 * small test-driver for new dialog functionality
 *
 * Copyright (c) 1995, Jordan Hubbard
 *
 * All rights reserved.
 *
 * This manual page may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of the software described herein
 * nor does the author assume any responsibility for damages incurred with
 * its use.
 *
 * $Id: test1.c,v 1.1 1995/12/23 01:10:32 jkh Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dialog.h>

#define IVAL	0	/* Time so sleep between stages */


/* Private routines and the menu declarations that use them reside in this section */

/* Callbacks for menu1 */
static enum { nowhere, berlin, rome, ny } where;

static int
_menu1_berlin_action(dialogMenuItem *self)
{
  WINDOW *w;
  int st = DITEM_FAILURE;

  w = dupwin(newscr);
  if (where == berlin) {
    dialog_mesgbox("excuse me?", "But you're already *in* Berlin!", -1, -1);
  }
  else {
    where = berlin;
    dialog_mesgbox("whoosh!", "Welcome to Berlin!  Have a beer!", -1, -1);
  }
  touchwin(w);
  wrefresh(w);
  delwin(w);
  return st;
}

static int
_menu1_rome_action(dialogMenuItem *self)
{
  WINDOW *w;
  int st = DITEM_FAILURE;

  w = dupwin(newscr);
  if (where == rome) {
    dialog_mesgbox("The wine must be getting to you..", "You're already in Rome!", -1, -1);
  }
  else {
    where = rome;
    dialog_mesgbox("whoosh!", "Welcome to Rome!  Have a coffee!", -1, -1);
  }
  touchwin(w);
  wrefresh(w);
  delwin(w);
  return st;
}

static int
_menu1_ny_action(dialogMenuItem *self)
{
  WINDOW *w;
  int st = DITEM_FAILURE;

  w = dupwin(newscr);
  if (where == ny) {
    dialog_mesgbox("Say what?", "You're already there!", -1, -1);
  }
  else {
    where = ny;
    dialog_mesgbox("whoosh!", "Welcome to New York!  Now go someplace else!", -1, -1);
  }
  touchwin(w);
  wrefresh(w);
  delwin(w);
  return st;
}

/* menu1 - show off the "fire" action hook */
static dialogMenuItem menu1[] = {
  { "Berlin", "Go visit Germany's new capitol", NULL, _menu1_berlin_action },
  { "Rome", "Go visit the Roman ruins", NULL, _menu1_rome_action },
  { "New York", "Go visit the streets of New York", NULL, _menu1_ny_action },
};


/* Callbacks for menu2 */
static int
getBool(dialogMenuItem *self)
{
  if (self->data && *((int *)self->data))
    return TRUE;
  return FALSE;
}

static int
setBool(dialogMenuItem *self)
{
  if (self->data) {
    *((int *)self->data) = !*((int *)self->data);
    return DITEM_SUCCESS;
  }
  return DITEM_FAILURE;
}

static int german_book, italian_book, slang_book;
static int spending;

static int
clearBooks(dialogMenuItem *self)
{
  german_book = italian_book = slang_book = FALSE;
  return DITEM_REDRAW;
}

/* menu2 - A more advanced way of using checked and fire hooks to manipulate the backing-variables directly */
static dialogMenuItem menu2[] = {
  { "German", "Buy book on learning German", getBool, setBool, &german_book},
  { "Italian", "Buy book on learning Italian", getBool, setBool, &italian_book },
  { "Slang", "Buy book on commonly used insults", getBool, setBool, &slang_book },
  { "Clear", "Clear book list", NULL, clearBooks, NULL, ' ', ' ', ' ' },
};


/* Callbacks for menu3 */
static int
buyBooks(dialogMenuItem *self)
{
  char foo[256];

  strcpy(foo, "Ok, you're buying books on");
  if (german_book)
    strcat(foo, " german");
  if (italian_book)
    strcat(foo, " italian");
  if (slang_book)
    strcat(foo, " slang");
  dialog_mesgbox("Cash Register", foo, -1, -1);
  return DITEM_SUCCESS;
}

/* menu3 - Look mom!  We can finally use our own OK and Cancel buttons! */
static dialogMenuItem menu3[] = {
  { "Buy!", NULL, NULL, buyBooks },	/* This is the new "OK" button with own fire action */
  { "No Way!", NULL, NULL, NULL },	/* This is the new "Cancel" button with defaults */
  { "German", "Buy books on learning German",	getBool, setBool, &german_book},	/* Actual items start here */
  { "Italian", "Buy books on learning Italian",	getBool, setBool, &italian_book },
  { "Slang", "Buy books on commonly used insults", getBool, setBool, &slang_book },
  { "Clear", "Clear book list",			NULL,	clearBooks, NULL },
};


/* Callbacks for menu4 and menu5 */
static int
check(dialogMenuItem *self)
{
  return ((int)self->data == spending);
}

static int
spend(dialogMenuItem *self)
{
  spending = (int)self->data;
  return DITEM_REDRAW;
}

/* menu4 - Show off a simulated compound menu (group at top is checklist, group at bottom radio) */
static dialogMenuItem menu4[] = {
  { "German", "Buy books on learning German", getBool, setBool, &german_book},
  { "Italian", "Buy books on learning Italian", getBool, setBool, &italian_book },
  { "Slang", "Buy books on commonly used insults", getBool, setBool, &slang_book },
  { "-----", "----------------------------------", NULL, NULL, NULL, ' ', ' ', ' ' },
  { "1000", "Spend $1,000", check, spend, (void *)1000, '(', '*', ')' },
  { "500", "Spend $500", check, spend, (void *)500, '(', '*', ')' },
  { "100", "Spend $100", check, spend, (void *)100, '(', '*', ')' },
};

/* menu5 - Show a simple radiolist menu that inherits the radio appearance by default */
static dialogMenuItem menu5[] = {
  { "1000", "Spend $1,000", check, spend, (void *)1000 },
  { "500", "Spend $500", check, spend, (void *)500 },
  { "100", "Spend $100", check, spend, (void *)100 },
};


/* Callbacks for menu6 */
static char bachelor[10], bachelette[10];

static int
getBachelor(dialogMenuItem *self)
{
  return !strcmp(bachelor, self->prompt);
}

static int
setBachelor(dialogMenuItem *self)
{
  strcpy(bachelor, self->prompt);
  return DITEM_REDRAW;
}

static int
getBachelette(dialogMenuItem *self)
{
  return !strcmp(bachelette, self->prompt);
}

static int
setBachelette(dialogMenuItem *self)
{
  strcpy(bachelette, self->prompt);
  return DITEM_REDRAW;
}

/* menu6- More complex radiolist menu that creates two groups in a single menu */
static dialogMenuItem menu6[] = {
  { "Tom", "Tom's a dynamic shoe salesman from Tulsa, OK!", getBachelor, setBachelor },
  { "Dick", "Dick's a retired engine inspector from McDonnell-Douglas!", getBachelor, setBachelor },
  { "Harry", "Harry's a professional female impersonator from Las Vegas!", getBachelor, setBachelor },
  { "-----", "----------------------------------", NULL, NULL, NULL, ' ', ' ', ' ' },
  { "Jane", "Jane's a twice-divorced housewife from Moose, Oregon!", getBachelette, setBachelette },
  { "Sally", "Sally's a shy Human Resources Manager for IBM!", getBachelette, setBachelette },
  { "Mary", "Mary's an energetic serial killer on the lam!", getBachelette, setBachelette },
};

/* End of hook functions */

/* Kick it off, James! */
int
main(int argc, unsigned char *argv[])
{
  int retval;

  init_dialog();

  /* Do the yes/no first */
  retval = dialog_yesno("This is dialog_yesno() in action",
			"Have you stopped deliberately putting bugs into your code?", -1, -1);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_yesno was %d\n", retval);
  sleep(IVAL);

  retval = dialog_msgbox("This is dialog_msgbox() in action with pause on", "Hi there.  Please press return now.",
			 -1, -1, 1);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_msgbox was %d\n", retval);
  sleep(IVAL);

  retval = dialog_msgbox("This is dialog_msgbox() in action with pause off",
			 "It also contains\n"
			 "a multiple line\n"
			 "message.",
			 -1, -1, 0);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_msgbox was %d\n", retval);
  sleep(IVAL);

  retval = dialog_prgbox("This is dialog_prgbox() in action with cal(1)", "cal", 14, 50, TRUE, TRUE);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_prgbox was %d\n", retval);
  sleep(IVAL);

  retval = dialog_textbox("This is dialog_textbox() in action with /etc/passwd", "/etc/passwd", 10, 60);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_textbox was %d\n", retval);
  sleep(IVAL);

  retval = dialog_menu("this is dialog_menu() in action, trial #1",
		       "this simple menu shows off some of the straight-forward features\n"
		       "of the new menu system's action dispatch hooks", -1, -1, 3, -3, &menu1, NULL, NULL, NULL);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_menu was %d\n", retval);
  sleep(IVAL);

  retval = dialog_checklist("this is dialog_checklist() in action, trial #1",
			    "this checklist menu shows off some of the straight-forward features\n"
			    "of the new menu system's check & fire dispatch hooks", -1, -1, 4, -4, &menu2, NULL);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_checklist was %d (%d %d %d)\n", retval, german_book, italian_book, slang_book);
  sleep(IVAL);

  retval = dialog_checklist("this is dialog_checklist() in action, trial #2",
			    "Same as before, but now we relabel the buttons and override the OK action.",
			    -1, -1, 4, -4, menu3 + 2, (char *)TRUE);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_checklist was %d\n", retval);
  sleep(IVAL);

  retval = dialog_checklist("this is dialog_checklist() in action, trial #3",
			    "Now we show off some of the button 'styles' one can create.",
			    -1, -1, 7, -7, menu4, NULL);
  dialog_clear();
  fprintf(stderr, "spent $%d on %s%s%s books\n", spending, german_book ? " german" : "",
	  italian_book ? " italian" : "", slang_book ? " slang" : "");
  sleep(IVAL);

  retval = dialog_radiolist("this is dialog_radiolist() in action, trial #1",
			    "this radio menu shows off some of the straight-forward features\n"
			    "of the new menu system's check & fire dispatch hooks", -1, -1, 3, -3, &menu5, NULL);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_radiolist was %d (money set to %d)\n", retval, spending);
  sleep(IVAL);

  retval = dialog_radiolist("this is dialog_radiolist() in action, trial #2",
			    "Welcome to \"The Love Blender!\" - America's favorite game show\n"
			    "where YOU, the contestant, get to choose which of these two\n"
			    "fine specimens of humanity will go home together, whether they\n"
			    "like it or not!", -1, -1, 7, -7, &menu6, NULL);
  dialog_clear();
  fprintf(stderr, "I'm sure that %s and %s will be very happy together!\n", bachelor, bachelette);
  sleep(IVAL);

  end_dialog();
  return 0;
}
