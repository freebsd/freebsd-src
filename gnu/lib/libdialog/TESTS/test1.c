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
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dialog.h>

static enum { nowhere, berlin, rome, ny } where;

#define IVAL	0

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

static dialogMenuItem menu1[] = {
  { "Berlin", "Go visit Germany's new capitol", NULL, _menu1_berlin_action },
  { "Rome", "Go visit the Roman ruins", NULL, _menu1_rome_action },
  { "New York", "Go visit the streets of New York", NULL, _menu1_ny_action },
};

int
getBool(dialogMenuItem *self)
{
  if (self->data && *((int *)self->data))
    return TRUE;
  return FALSE;
}

int setBool(dialogMenuItem *self)
{
  if (self->data) {
    *((int *)self->data) = !*((int *)self->data);
    return DITEM_SUCCESS;
  }
  return DITEM_FAILURE;
}

static int german_book, italian_book, slang_book;
static int spending;

int clearBooks(dialogMenuItem *self)
{
  german_book = italian_book = slang_book = FALSE;
  return DITEM_REDRAW;
}

int buyBooks(dialogMenuItem *self)
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

int check(dialogMenuItem *self)
{
  return ((int)self->data == spending);
}

int spend(dialogMenuItem *self)
{
  spending = (int)self->data;
  return DITEM_REDRAW;
}

static dialogMenuItem menu2[] = {
  { "German", "Buy book on learning German", getBool, setBool, &german_book},
  { "Italian", "Buy book on learning Italian", getBool, setBool, &italian_book },
  { "Slang", "Buy book on commonly used insults", getBool, setBool, &slang_book },
  { "Clear", "Clear book list", NULL, clearBooks, NULL },
};

static dialogMenuItem menu3[] = {
  { "Buy!", NULL, NULL, buyBooks },
  { "No Way!", NULL, NULL, NULL },
  { "German", "Buy books on learning German", getBool, setBool, &german_book},
  { "Italian", "Buy books on learning Italian", getBool, setBool, &italian_book },
  { "Slang", "Buy books on commonly used insults", getBool, setBool, &slang_book },
  { "Clear", "Clear book list", NULL, clearBooks, NULL },
};

static dialogMenuItem menu4[] = {
  { "German", "Buy books on learning German", getBool, setBool, &german_book},
  { "Italian", "Buy books on learning Italian", getBool, setBool, &italian_book },
  { "Slang", "Buy books on commonly used insults", getBool, setBool, &slang_book },
  { "-----", "----------------------------------", NULL, NULL, NULL, ' ', ' ', ' ' },
  { "1000", "Spend $1,000", check, spend, (void *)1000, '(', '*', ')' },
  { "500", "Spend $500", check, spend, (void *)500, '(', '*', ')' },
  { "100", "Spend $100", check, spend, (void *)100, '(', '*', ')' },
};

static dialogMenuItem menu5[] = {
  { "1000", "Spend $1,000", check, spend, (void *)1000 },
  { "500", "Spend $500", check, spend, (void *)500 },
  { "100", "Spend $100", check, spend, (void *)100 },
};

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

  retval = dialog_msgbox("This is dialog_msgbox() in action with pause on", "This is a multiple\nline message.",
			 -1, -1, 1);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_msgbox was %d\n", retval);
  sleep(IVAL);

  retval = dialog_msgbox("This is dialog_msgbox() in action", "This is a multiple\nline message.",
			 -1, -1, 0);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_msgbox was %d\n", retval);
  sleep(IVAL);

  retval = dialog_prgbox("This is dialog_prgbox() in action", "cal", 14, 40, TRUE, TRUE);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_prgbox was %d\n", retval);
  sleep(IVAL);

  retval = dialog_textbox("This is dialog_textbox() in action", "/etc/passwd", 10, 50);
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
  fprintf(stderr, "returned value for dialog_checklist was %d\n", retval);
  sleep(IVAL);

  retval = dialog_checklist("this is dialog_checklist() in action, trial #2",
			    "Same as before, but now we relabel the buttons and override the OK action.",
			    -1, -1, 4, -4, menu3 + 2, (char *)TRUE);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_checklist was %d\n", retval);
  sleep(IVAL);

  retval = dialog_checklist("this is dialog_checklist() in action, trial #3",
			    "Now we show off some of the button 'styles' one could use.",
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

  end_dialog();
  return 0;
}
