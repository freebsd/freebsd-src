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
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dialog.h>

/* Kick it off, James! */
int
main(int argc, char **argv)
{
  int rval1, rval2;

  init_dialog();

  rval1 = dialog_yesno("This is dialog_yesno() in action",
		       "Have you stopped deliberately putting bugs into your code?", -1, -1);
  dialog_clear();
  rval2 = dialog_noyes("This is dialog_noyes() in action",
		       "Have you stopped beating your wife?", -1, -1);
  dialog_clear();
  end_dialog();
  fprintf(stderr, "returned value for dialog_yesno was %d\n", rval1);
  fprintf(stderr, "returned value for dialog_noyes was %d\n", rval2);
  return 0;
}
