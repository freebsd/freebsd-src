/* Output routines for ed-script format.
   Copyright (C) 1988, 89, 91, 92 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU DIFF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "diff.h"

int change_letter ();
int translate_line_number ();
static void print_rcs_hunk ();
static void print_ed_hunk ();
static void pr_forward_ed_hunk ();
void translate_range ();
struct change *find_change ();
struct change *find_reverse_change ();

/* Print our script as ed commands.  */

void
print_ed_script (script)
    struct change *script;
{
  print_script (script, find_reverse_change, print_ed_hunk);
}

/* Print a hunk of an ed diff */

static void
print_ed_hunk (hunk)
     struct change *hunk; 
{
  int f0, l0, f1, l1;
  int deletes, inserts;

#if 0
  hunk = flip_script (hunk);
#endif
#ifdef DEBUG
  debug_script (hunk);
#endif

  /* Determine range of line numbers involved in each file.  */
  analyze_hunk (hunk, &f0, &l0, &f1, &l1, &deletes, &inserts);
  if (!deletes && !inserts)
    return;

  begin_output ();

  /* Print out the line number header for this hunk */
  print_number_range (',', &files[0], f0, l0);
  fprintf (outfile, "%c\n", change_letter (inserts, deletes));

  /* Print new/changed lines from second file, if needed */
  if (inserts)
    {
      int i;
      int inserting = 1;
      for (i = f1; i <= l1; i++)
	{
	  /* Resume the insert, if we stopped.  */
	  if (! inserting)
	    fprintf (outfile, "%da\n",
		     i - f1 + translate_line_number (&files[0], f0) - 1);
	  inserting = 1;

	  /* If the file's line is just a dot, it would confuse `ed'.
	     So output it with a double dot, and set the flag LEADING_DOT
	     so that we will output another ed-command later
	     to change the double dot into a single dot.  */

	  if (files[1].linbuf[i][0] == '.'
	      && files[1].linbuf[i][1] == '\n')
	    {
	      fprintf (outfile, "..\n");
	      fprintf (outfile, ".\n");
	      /* Now change that double dot to the desired single dot.  */
	      fprintf (outfile, "%ds/^\\.\\././\n",
		       i - f1 + translate_line_number (&files[0], f0));
	      inserting = 0;
	    }
	  else
	    /* Line is not `.', so output it unmodified.  */
	    print_1_line ("", &files[1].linbuf[i]);
	}

      /* End insert mode, if we are still in it.  */
      if (inserting)
	fprintf (outfile, ".\n");
    }
}

/* Print change script in the style of ed commands,
   but print the changes in the order they appear in the input files,
   which means that the commands are not truly useful with ed.  */

void
pr_forward_ed_script (script)
     struct change *script;
{
  print_script (script, find_change, pr_forward_ed_hunk);
}

static void
pr_forward_ed_hunk (hunk)
     struct change *hunk;
{
  int i;
  int f0, l0, f1, l1;
  int deletes, inserts;

  /* Determine range of line numbers involved in each file.  */
  analyze_hunk (hunk, &f0, &l0, &f1, &l1, &deletes, &inserts);
  if (!deletes && !inserts)
    return;

  begin_output ();

  fprintf (outfile, "%c", change_letter (inserts, deletes));
  print_number_range (' ', files, f0, l0);
  fprintf (outfile, "\n");

  /* If deletion only, print just the number range.  */

  if (!inserts)
    return;

  /* For insertion (with or without deletion), print the number range
     and the lines from file 2.  */

  for (i = f1; i <= l1; i++)
    print_1_line ("", &files[1].linbuf[i]);

  fprintf (outfile, ".\n");
}

/* Print in a format somewhat like ed commands
   except that each insert command states the number of lines it inserts.
   This format is used for RCS.  */

void
print_rcs_script (script)
     struct change *script;
{
  print_script (script, find_change, print_rcs_hunk);
}

/* Print a hunk of an RCS diff */

static void
print_rcs_hunk (hunk)
     struct change *hunk;
{
  int i;
  int f0, l0, f1, l1;
  int deletes, inserts;
  int tf0, tl0, tf1, tl1;

  /* Determine range of line numbers involved in each file.  */
  analyze_hunk (hunk, &f0, &l0, &f1, &l1, &deletes, &inserts);
  if (!deletes && !inserts)
    return;

  begin_output ();

  translate_range (&files[0], f0, l0, &tf0, &tl0);

  if (deletes)
    {
      fprintf (outfile, "d");
      /* For deletion, print just the starting line number from file 0
	 and the number of lines deleted.  */
      fprintf (outfile, "%d %d\n",
	       tf0,
	       (tl0 >= tf0 ? tl0 - tf0 + 1 : 1));	     
    }

  if (inserts)
    {
      fprintf (outfile, "a");

      /* Take last-line-number from file 0 and # lines from file 1.  */
      translate_range (&files[1], f1, l1, &tf1, &tl1);
      fprintf (outfile, "%d %d\n",
	       tl0,
	       (tl1 >= tf1 ? tl1 - tf1 + 1 : 1));	     

      /* Print the inserted lines.  */
      for (i = f1; i <= l1; i++)
	print_1_line ("", &files[1].linbuf[i]);
    }
}
