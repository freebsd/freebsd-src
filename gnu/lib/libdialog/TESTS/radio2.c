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

/* Hook functions */

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
  /* prompt	title								checked		fire */
static dialogMenuItem menu6[] = {
  { "Tom",	"Tom's a dynamic shoe salesman from Tulsa, OK!",		getBachelor,	setBachelor },
  { "Dick",	"Dick's a retired engine inspector from McDonnell-Douglas!",	getBachelor,	setBachelor },
  { "Harry",	"Harry's a professional female impersonator from Las Vegas!",	getBachelor,	setBachelor },
  { "-----",	"----------------------------------",			NULL, NULL, NULL, NULL, ' ', ' ', ' ' },
  { "Jane",	"Jane's a twice-divorced housewife from Moose, Oregon!",	getBachelette,	setBachelette },
  { "Sally",	"Sally's a shy Human Resources Manager for IBM!",		getBachelette,	setBachelette },
  { "Mary",	"Mary's an energetic serial killer on the lam!",		getBachelette,	setBachelette },
};

/* End of hook functions */

/* Kick it off, James! */
int
main(int argc, unsigned char *argv[])
{
  int retval;

  init_dialog();

  retval = dialog_radiolist("this is dialog_radiolist() in action, test #2",
			    "Welcome to \"The Love Blender!\" - America's favorite game show\n"
			    "where YOU, the contestant, get to choose which of these two\n"
			    "fine specimens of humanity will go home together, whether they\n"
			    "like it or not!", -1, -1, 7, -7, &menu6, NULL);
  dialog_clear();
  fprintf(stderr, "I'm sure that %s and %s will be very happy together!\n", bachelor, bachelette);

  end_dialog();
  return 0;
}
