/*
 * small test-driver for new dialog functionality
 *
 * Copyright (c) 1995, Jordan Hubbard
 *
 * All rights reserved.
 *
 * This source code may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of the software nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 * $Id: test1.c,v 1.2 1995/12/23 14:53:07 jkh Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dialog.h>

/* Start of hook functions */
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
  /* prompt	title					checked		fire */
static dialogMenuItem menu1[] = {
  { "Berlin",	"Go visit Germany's new capitol",	NULL,	_menu1_berlin_action	},
  { "Rome",	"Go visit the Roman ruins",		NULL,	_menu1_rome_action	},
  { "New York",	"Go visit the streets of New York",	NULL,	_menu1_ny_action	},
};

/* End of hook functions */

/* Kick it off, James! */
int
main(int argc, unsigned char *argv[])
{
  int retval;

  init_dialog();

  use_helpfile("menu2.c");
  use_helpline("Type F1 to view the source for this demo");
  retval = dialog_menu("this is dialog_menu() in action, test #2",
		       "this simple menu shows off some of the straight-forward features\n"
		       "of the new menu system's action dispatch hooks as well as a helpline\n"
		       "and a helpfile.  Select Cancel to leave",
		       -1, -1, 3, -3, &menu1, NULL, NULL, NULL);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_menu was %d\n", retval);

  end_dialog();
  return 0;
}
