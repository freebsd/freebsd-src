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
 * $FreeBSD: src/gnu/lib/libdialog/TESTS/yesno.c,v 1.5 2000/01/10 11:52:10 phantom Exp $
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
  int retval;

  init_dialog();

  retval = dialog_yesno("This is dialog_yesno() in action",
			"Have you stopped deliberately putting bugs into your code?", -1, -1);
  dialog_clear();
  fprintf(stderr, "returned value for dialog_yesno was %d\n", retval);

  end_dialog();
  return 0;
}
