/* Context-format output routines for GNU DIFF.
   Copyright (C) 1988,1989,1991,1992,1993,1994,1998 Free Software Foundation, Inc.

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

static struct change *find_hunk PARAMS((struct change *));
static void find_function PARAMS((struct file_data const *, int, char const **, size_t *));
static void mark_ignorable PARAMS((struct change *));
static void pr_context_hunk PARAMS((struct change *));
static void pr_unidiff_hunk PARAMS((struct change *));
static void print_context_label PARAMS ((char const *, struct file_data *, char const *));
static void print_context_number_range PARAMS((struct file_data const *, int, int));
static void print_unidiff_number_range PARAMS((struct file_data const *, int, int));

/* Last place find_function started searching from.  */
static int find_function_last_search;

/* The value find_function returned when it started searching there.  */
static int find_function_last_match;

/* Print a label for a context diff, with a file name and date or a label.  */

static void
print_context_label (mark, inf, label)
     char const *mark;
     struct file_data *inf;
     char const *label;
{
  if (label)
    printf_output ("%s %s\n", mark, label);
  else
    {
      char const *ct = ctime (&inf->stat.st_mtime);
      if (!ct)
	ct = "?\n";
      /* See Posix.2 section 4.17.6.1.4 for this format.  */
      printf_output ("%s %s\t%s", mark, inf->name, ct);
    }
}

/* Print a header for a context diff, with the file names and dates.  */

void
print_context_header (inf, unidiff_flag)
     struct file_data inf[];
     int unidiff_flag;
{
  if (unidiff_flag)
    {
      print_context_label ("---", &inf[0], file_label[0]);
      print_context_label ("+++", &inf[1], file_label[1]);
    }
  else
    {
      print_context_label ("***", &inf[0], file_label[0]);
      print_context_label ("---", &inf[1], file_label[1]);
    }
}

/* Print an edit script in context format.  */

void
print_context_script (script, unidiff_flag)
     struct change *script;
     int unidiff_flag;
{
  if (ignore_blank_lines_flag || ignore_regexp_list)
    mark_ignorable (script);
  else
    {
      struct change *e;
      for (e = script; e; e = e->link)
	e->ignore = 0;
    }

  find_function_last_search = - files[0].prefix_lines;
  find_function_last_match = find_function_last_search - 1;

  if (unidiff_flag)
    print_script (script, find_hunk, pr_unidiff_hunk);
  else
    print_script (script, find_hunk, pr_context_hunk);
}

/* Print a pair of line numbers with a comma, translated for file FILE.
   If the second number is not greater, use the first in place of it.

   Args A and B are internal line numbers.
   We print the translated (real) line numbers.  */

static void
print_context_number_range (file, a, b)
     struct file_data const *file;
     int a, b;
{
  int trans_a, trans_b;
  translate_range (file, a, b, &trans_a, &trans_b);

  /* Note: we can have B < A in the case of a range of no lines.
     In this case, we should print the line number before the range,
     which is B.  */
  if (trans_b > trans_a)
    printf_output ("%d,%d", trans_a, trans_b);
  else
    printf_output ("%d", trans_b);
}

/* Print a portion of an edit script in context format.
   HUNK is the beginning of the portion to be printed.
   The end is marked by a `link' that has been nulled out.

   Prints out lines from both files, and precedes each
   line with the appropriate flag-character.  */

static void
pr_context_hunk (hunk)
     struct change *hunk;
{
  int first0, last0, first1, last1, show_from, show_to, i;
  struct change *next;
  char const *prefix;
  char const *function;
  size_t function_length;

  /* Determine range of line numbers involved in each file.  */

  analyze_hunk (hunk, &first0, &last0, &first1, &last1, &show_from, &show_to);

  if (!show_from && !show_to)
    return;

  /* Include a context's width before and after.  */

  i = - files[0].prefix_lines;
  first0 = max (first0 - context, i);
  first1 = max (first1 - context, i);
  last0 = min (last0 + context, files[0].valid_lines - 1);
  last1 = min (last1 + context, files[1].valid_lines - 1);

  /* If desired, find the preceding function definition line in file 0.  */
  function = 0;
  if (function_regexp_list)
    find_function (&files[0], first0, &function, &function_length);

  begin_output ();

  /* If we looked for and found a function this is part of,
     include its name in the header of the diff section.  */
  printf_output ("***************");

  if (function)
    {
      printf_output (" ");
      write_output (function, min (function_length - 1, 40));
    }

  printf_output ("\n*** ");
  print_context_number_range (&files[0], first0, last0);
  printf_output (" ****\n");

  if (show_from)
    {
      next = hunk;

      for (i = first0; i <= last0; i++)
	{
	  /* Skip past changes that apply (in file 0)
	     only to lines before line I.  */

	  while (next && next->line0 + next->deleted <= i)
	    next = next->link;

	  /* Compute the marking for line I.  */

	  prefix = " ";
	  if (next && next->line0 <= i)
	    /* The change NEXT covers this line.
	       If lines were inserted here in file 1, this is "changed".
	       Otherwise it is "deleted".  */
	    prefix = (next->inserted > 0 ? "!" : "-");

	  print_1_line (prefix, &files[0].linbuf[i]);
	}
    }

  printf_output ("--- ");
  print_context_number_range (&files[1], first1, last1);
  printf_output (" ----\n");

  if (show_to)
    {
      next = hunk;

      for (i = first1; i <= last1; i++)
	{
	  /* Skip past changes that apply (in file 1)
	     only to lines before line I.  */

	  while (next && next->line1 + next->inserted <= i)
	    next = next->link;

	  /* Compute the marking for line I.  */

	  prefix = " ";
	  if (next && next->line1 <= i)
	    /* The change NEXT covers this line.
	       If lines were deleted here in file 0, this is "changed".
	       Otherwise it is "inserted".  */
	    prefix = (next->deleted > 0 ? "!" : "+");

	  print_1_line (prefix, &files[1].linbuf[i]);
	}
    }
}

/* Print a pair of line numbers with a comma, translated for file FILE.
   If the second number is smaller, use the first in place of it.
   If the numbers are equal, print just one number.

   Args A and B are internal line numbers.
   We print the translated (real) line numbers.  */

static void
print_unidiff_number_range (file, a, b)
     struct file_data const *file;
     int a, b;
{
  int trans_a, trans_b;
  translate_range (file, a, b, &trans_a, &trans_b);

  /* Note: we can have B < A in the case of a range of no lines.
     In this case, we should print the line number before the range,
     which is B.  */
  if (trans_b <= trans_a)
    printf_output (trans_b == trans_a ? "%d" : "%d,0", trans_b);
  else
    printf_output ("%d,%d", trans_a, trans_b - trans_a + 1);
}

/* Print a portion of an edit script in unidiff format.
   HUNK is the beginning of the portion to be printed.
   The end is marked by a `link' that has been nulled out.

   Prints out lines from both files, and precedes each
   line with the appropriate flag-character.  */

static void
pr_unidiff_hunk (hunk)
     struct change *hunk;
{
  int first0, last0, first1, last1, show_from, show_to, i, j, k;
  struct change *next;
  char const *function;
  size_t function_length;

  /* Determine range of line numbers involved in each file.  */

  analyze_hunk (hunk, &first0, &last0, &first1, &last1, &show_from, &show_to);

  if (!show_from && !show_to)
    return;

  /* Include a context's width before and after.  */

  i = - files[0].prefix_lines;
  first0 = max (first0 - context, i);
  first1 = max (first1 - context, i);
  last0 = min (last0 + context, files[0].valid_lines - 1);
  last1 = min (last1 + context, files[1].valid_lines - 1);

  /* If desired, find the preceding function definition line in file 0.  */
  function = 0;
  if (function_regexp_list)
    find_function (&files[0], first0, &function, &function_length);

  begin_output ();

  printf_output ("@@ -");
  print_unidiff_number_range (&files[0], first0, last0);
  printf_output (" +");
  print_unidiff_number_range (&files[1], first1, last1);
  printf_output (" @@");

  /* If we looked for and found a function this is part of,
     include its name in the header of the diff section.  */

  if (function)
    {
      write_output (" ", 1);
      write_output (function, min (function_length - 1, 40));
    }
  write_output ("\n", 1);

  next = hunk;
  i = first0;
  j = first1;

  while (i <= last0 || j <= last1)
    {

      /* If the line isn't a difference, output the context from file 0. */

      if (!next || i < next->line0)
	{
	  write_output (tab_align_flag ? "\t" : " ", 1);
	  print_1_line (0, &files[0].linbuf[i++]);
	  j++;
	}
      else
	{
	  /* For each difference, first output the deleted part. */

	  k = next->deleted;
	  while (k--)
	    {
	      write_output ("-", 1);
	      if (tab_align_flag)
		write_output ("\t", 1);
	      print_1_line (0, &files[0].linbuf[i++]);
	    }

	  /* Then output the inserted part. */

	  k = next->inserted;
	  while (k--)
	    {
	      write_output ("+", 1);
	      if (tab_align_flag)
		write_output ("\t", 1);
	      print_1_line (0, &files[1].linbuf[j++]);
	    }

	  /* We're done with this hunk, so on to the next! */

	  next = next->link;
	}
    }
}

/* Scan a (forward-ordered) edit script for the first place that more than
   2*CONTEXT unchanged lines appear, and return a pointer
   to the `struct change' for the last change before those lines.  */

static struct change *
find_hunk (start)
     struct change *start;
{
  struct change *prev;
  int top0, top1;
  int thresh;

  do
    {
      /* Compute number of first line in each file beyond this changed.  */
      top0 = start->line0 + start->deleted;
      top1 = start->line1 + start->inserted;
      prev = start;
      start = start->link;
      /* Threshold distance is 2*CONTEXT between two non-ignorable changes,
	 but only CONTEXT if one is ignorable.  */
      thresh = ((prev->ignore || (start && start->ignore))
		? context
		: 2 * context + 1);
      /* It is not supposed to matter which file we check in the end-test.
	 If it would matter, crash.  */
      if (start && start->line0 - top0 != start->line1 - top1)
	abort ();
    } while (start
	     /* Keep going if less than THRESH lines
		elapse before the affected line.  */
	     && start->line0 < top0 + thresh);

  return prev;
}

/* Set the `ignore' flag properly in each change in SCRIPT.
   It should be 1 if all the lines inserted or deleted in that change
   are ignorable lines.  */

static void
mark_ignorable (script)
     struct change *script;
{
  while (script)
    {
      struct change *next = script->link;
      int first0, last0, first1, last1, deletes, inserts;

      /* Turn this change into a hunk: detach it from the others.  */
      script->link = 0;

      /* Determine whether this change is ignorable.  */
      analyze_hunk (script, &first0, &last0, &first1, &last1, &deletes, &inserts);
      /* Reconnect the chain as before.  */
      script->link = next;

      /* If the change is ignorable, mark it.  */
      script->ignore = (!deletes && !inserts);

      /* Advance to the following change.  */
      script = next;
    }
}

/* Find the last function-header line in FILE prior to line number LINENUM.
   This is a line containing a match for the regexp in `function_regexp'.
   Store the address of the line text into LINEP and the length of the
   line into LENP.
   Do not store anything if no function-header is found.  */

static void
find_function (file, linenum, linep, lenp)
     struct file_data const *file;
     int linenum;
     char const **linep;
     size_t *lenp;
{
  int i = linenum;
  int last = find_function_last_search;
  find_function_last_search = i;

  while (--i >= last)
    {
      /* See if this line is what we want.  */
      struct regexp_list *r;
      char const *line = file->linbuf[i];
      size_t len = file->linbuf[i + 1] - line;

      for (r = function_regexp_list; r; r = r->next)
	if (0 <= re_search (&r->buf, line, len, 0, len, 0))
	  {
	    *linep = line;
	    *lenp = len;
	    find_function_last_match = i;
	    return;
	  }
    }
  /* If we search back to where we started searching the previous time,
     find the line we found last time.  */
  if (find_function_last_match >= - file->prefix_lines)
    {
      i = find_function_last_match;
      *linep = file->linbuf[i];
      *lenp = file->linbuf[i + 1] - *linep;
      return;
    }
  return;
}
