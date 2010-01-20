/* closeout.c - close standard output

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2004 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "closeout.h"

#include <stdio.h>
#include <errno.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)

#include "error.h"
#include "exitfail.h"
#include "quotearg.h"
#include "unlocked-io.h"
#include "__fpending.h"

static const char *file_name;

/* Set the file name to be reported in the event an error is detected
   by close_stdout.  */
void
close_stdout_set_file_name (const char *file)
{
  file_name = file;
}

/* Close standard output, exiting with status 'exit_failure' on failure.
   If a program writes *anything* to stdout, that program should `fflush'
   stdout and make sure that it succeeds before exiting.  Otherwise,
   suppose that you go to the extreme of checking the return status
   of every function that does an explicit write to stdout.  The last
   printf can succeed in writing to the internal stream buffer, and yet
   the fclose(stdout) could still fail (due e.g., to a disk full error)
   when it tries to write out that buffered data.  Thus, you would be
   left with an incomplete output file and the offending program would
   exit successfully.

   FIXME: note the fflush suggested above is implicit in the fclose
   we actually do below.  Consider doing only the fflush and/or using
   setvbuf to inhibit buffering.

   Besides, it's wasteful to check the return value from every call
   that writes to stdout -- just let the internal stream state record
   the failure.  That's what the ferror test is checking below.

   It's important to detect such failures and exit nonzero because many
   tools (most notably `make' and other build-management systems) depend
   on being able to detect failure in other tools via their exit status.  */

void
close_stdout (void)
{
  int e = ferror (stdout) ? 0 : -1;

  /* If the stream's error bit is clear and there is nothing to flush,
     then return right away.  */
  if (e && __fpending (stdout) == 0)
    return;

  if (fclose (stdout) != 0)
    e = errno;

  if (0 <= e)
    {
      char const *write_error = _("write error");
      if (file_name)
	error (exit_failure, e, "%s: %s", quotearg_colon (file_name),
	       write_error);
      else
	error (exit_failure, e, "%s", write_error);
    }
}
