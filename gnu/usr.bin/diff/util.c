/* Support routines for GNU DIFF.
   Copyright (C) 1988, 1989, 1992 Free Software Foundation, Inc.

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

/* Use when a system call returns non-zero status.
   TEXT should normally be the file name.  */

void
perror_with_name (text)
     char *text;
{
  int e = errno;
  fprintf (stderr, "%s: ", program);
  errno = e;
  perror (text);
}

/* Use when a system call returns non-zero status and that is fatal.  */

void
pfatal_with_name (text)
     char *text;
{
  int e = errno;
  print_message_queue ();
  fprintf (stderr, "%s: ", program);
  errno = e;
  perror (text);
  exit (2);
}

/* Print an error message from the format-string FORMAT
   with args ARG1 and ARG2.  */

void
error (format, arg, arg1)
     char *format;
     char *arg;
     char *arg1;
{
  fprintf (stderr, "%s: ", program);
  fprintf (stderr, format, arg, arg1);
  fprintf (stderr, "\n");
}

/* Print an error message containing the string TEXT, then exit.  */

void
fatal (m)
     char *m;
{
  print_message_queue ();
  error ("%s", m, 0);
  exit (2);
}

/* Like printf, except if -l in effect then save the message and print later.
   This is used for things like "binary files differ" and "Only in ...".  */

void
message (format, arg1, arg2)
     char *format, *arg1, *arg2;
{
  if (paginate_flag)
    {
      struct msg *new = (struct msg *) xmalloc (sizeof (struct msg));
      if (msg_chain_end == 0)
	msg_chain = msg_chain_end = new;
      else
	{
	  msg_chain_end->next = new;
	  msg_chain_end = new;
	}
      new->format = format;
      new->arg1 = concat (arg1, "", "");
      new->arg2 = concat (arg2, "", "");
      new->next = 0;
    }
  else
    {
      if (sdiff_help_sdiff)
	putchar (' ');
      printf (format, arg1, arg2);
    }
}

/* Output all the messages that were saved up by calls to `message'.  */

void
print_message_queue ()
{
  struct msg *m;

  for (m = msg_chain; m; m = m->next)
    printf (m->format, m->arg1, m->arg2);
}

/* Call before outputting the results of comparing files NAME0 and NAME1
   to set up OUTFILE, the stdio stream for the output to go to.

   Usually, OUTFILE is just stdout.  But when -l was specified
   we fork off a `pr' and make OUTFILE a pipe to it.
   `pr' then outputs to our stdout.  */

static char *current_name0;
static char *current_name1;
static int current_depth;

void
setup_output (name0, name1, depth)
     char *name0, *name1;
     int depth;
{
  current_name0 = name0;
  current_name1 = name1;
  current_depth = depth;
  outfile = 0;
}

void
begin_output ()
{
  char *name;

  if (outfile != 0)
    return;

  /* Construct the header of this piece of diff.  */
  name = (char *) xmalloc (strlen (current_name0) + strlen (current_name1)
			   + strlen (switch_string) + 15);

  strcpy (name, "diff");
  strcat (name, switch_string);
  strcat (name, " ");
  strcat (name, current_name0);
  strcat (name, " ");
  strcat (name, current_name1);

  if (paginate_flag)
    {
      int pipes[2];
      int desc;

      /* For a `pr' and make OUTFILE a pipe to it.  */
      if (pipe (pipes) < 0)
	pfatal_with_name ("pipe");

      fflush (stdout);

      desc = vfork ();
      if (desc < 0)
	pfatal_with_name ("vfork");

      if (desc == 0)
	{
	  close (pipes[1]);
	  if (pipes[0] != fileno (stdin))
	    {
	      if (dup2 (pipes[0], fileno (stdin)) < 0)
		pfatal_with_name ("dup2");
	      close (pipes[0]);
	    }

	  if (execl (PR_FILE_NAME, PR_FILE_NAME, "-f", "-h", name, 0) < 0)
	    pfatal_with_name (PR_FILE_NAME);
	}
      else
	{
	  close (pipes[0]);
	  outfile = fdopen (pipes[1], "w");
	} 
    }
  else
    {

      /* If -l was not specified, output the diff straight to `stdout'.  */

      outfile = stdout;

      /* If handling multiple files (because scanning a directory),
	 print which files the following output is about.  */
      if (current_depth > 0)
	printf ("%s\n", name);
    }

  free (name);

  /* A special header is needed at the beginning of context output.  */
  switch (output_style)
    {
    case OUTPUT_CONTEXT:
      print_context_header (files, 0);
      break;

    case OUTPUT_UNIFIED:
      print_context_header (files, 1);
      break;

    default:
      break;
    }
}

/* Call after the end of output of diffs for one file.
   Close OUTFILE and get rid of the `pr' subfork.  */

void
finish_output ()
{
  if (outfile != 0 && outfile != stdout)
    {
      fclose (outfile);
      wait (0);
    }

  outfile = 0;
}

/* Compare two lines (typically one from each input file)
   according to the command line options.
   Return 1 if the lines differ, like `bcmp'.  */

int
line_cmp (s1, len1, s2, len2)
     const char *s1, *s2;
     int len1, len2;
{
  register const unsigned char *t1, *t2;
  register unsigned char end_char = line_end_char;

  /* Check first for exact identity.
     If that is true, return 0 immediately.
     This detects the common case of exact identity
     faster than complete comparison would.  */

  if (len1 == len2 && bcmp (s1, s2, len1) == 0)
    return 0;

  /* Not exactly identical, but perhaps they match anyway
     when case or whitespace is ignored.  */

  if (ignore_case_flag || ignore_space_change_flag || ignore_all_space_flag)
    {
      t1 = (const unsigned char *) s1;
      t2 = (const unsigned char *) s2;

      while (1)
	{
	  register unsigned char c1 = *t1++;
	  register unsigned char c2 = *t2++;

	  /* Ignore horizontal whitespace if -b or -w is specified.  */

	  if (ignore_all_space_flag)
	    {
	      /* For -w, just skip past any white space.  */
	      while (Is_space (c1)) c1 = *t1++;
	      while (Is_space (c2)) c2 = *t2++;
	    }
	  else if (ignore_space_change_flag)
	    {
	      /* For -b, advance past any sequence of whitespace in line 1
		 and consider it just one Space, or nothing at all
		 if it is at the end of the line.  */
	      if (c1 == ' ' || c1 == '\t')
		{
		  while (1)
		    {
		      c1 = *t1++;
		      if (c1 == end_char)
			break;
		      if (c1 != ' ' && c1 != '\t')
			{
			  --t1;
			  c1 = ' ';
			  break;
			}
		    }
		}

	      /* Likewise for line 2.  */
	      if (c2 == ' ' || c2 == '\t')
		{
		  while (1)
		    {
		      c2 = *t2++;
		      if (c2 == end_char)
			break;
		      if (c2 != ' ' && c2 != '\t')
			{
			  --t2;
			  c2 = ' ';
			  break;
			}
		    }
		}
	    }

	  /* Upcase all letters if -i is specified.  */

	  if (ignore_case_flag)
	    {
	      if (islower (c1))
		c1 = toupper (c1);
	      if (islower (c2))
		c2 = toupper (c2);
	    }

	  if (c1 != c2)
	    break;
	  if (c1 == end_char)
	    return 0;
	}
    }

  return (1);
}

/* Find the consecutive changes at the start of the script START.
   Return the last link before the first gap.  */

struct change *
find_change (start)
     struct change *start;
{
  return start;
}

struct change *
find_reverse_change (start)
     struct change *start;
{
  return start;
}

/* Divide SCRIPT into pieces by calling HUNKFUN and
   print each piece with PRINTFUN.
   Both functions take one arg, an edit script.

   HUNKFUN is called with the tail of the script
   and returns the last link that belongs together with the start
   of the tail.

   PRINTFUN takes a subscript which belongs together (with a null
   link at the end) and prints it.  */

void
print_script (script, hunkfun, printfun)
     struct change *script;
     struct change * (*hunkfun) ();
     void (*printfun) ();
{
  struct change *next = script;

  while (next)
    {
      struct change *this, *end;

      /* Find a set of changes that belong together.  */
      this = next;
      end = (*hunkfun) (next);

      /* Disconnect them from the rest of the changes,
	 making them a hunk, and remember the rest for next iteration.  */
      next = end->link;
      end->link = NULL;
#ifdef DEBUG
      debug_script (this);
#endif

      /* Print this hunk.  */
      (*printfun) (this);

      /* Reconnect the script so it will all be freed properly.  */
      end->link = next;
    }
}

/* Print the text of a single line LINE,
   flagging it with the characters in LINE_FLAG (which say whether
   the line is inserted, deleted, changed, etc.).  */

void
print_1_line (line_flag, line)
     const char *line_flag;
     const char * const *line;
{
  const char *text = line[0], *limit = line[1]; /* Help the compiler.  */
  FILE *out = outfile; /* Help the compiler some more.  */
  const char *flag_format = 0;

  /* If -T was specified, use a Tab between the line-flag and the text.
     Otherwise use a Space (as Unix diff does).
     Print neither space nor tab if line-flags are empty.  */

  if (line_flag != NULL && line_flag[0] != 0)
    {
      flag_format = tab_align_flag ? "%s\t" : "%s ";
      fprintf (out, flag_format, line_flag);
    }

  output_1_line (text, limit, flag_format, line_flag);

  if ((line_flag == NULL || line_flag[0] != 0) && limit[-1] != '\n'
      && line_end_char == '\n')
    fprintf (out, "\n\\ No newline at end of file\n");
}

/* Output a line from TEXT up to LIMIT.  Without -t, output verbatim.
   With -t, expand white space characters to spaces, and if FLAG_FORMAT
   is nonzero, output it with argument LINE_FLAG after every
   internal carriage return, so that tab stops continue to line up.  */

void
output_1_line (text, limit, flag_format, line_flag)
     const char *text, *limit, *flag_format, *line_flag;
{
  if (!tab_expand_flag)
    fwrite (text, sizeof (char), limit - text, outfile);
  else
    {
      register FILE *out = outfile;
      register char c;
      register const char *t = text;
      register unsigned column = 0;

      while (t < limit)
	switch ((c = *t++))
	  {
	  case '\t':
	    {
	      unsigned spaces = TAB_WIDTH - column % TAB_WIDTH;
	      column += spaces;
	      do
		putc (' ', out);
	      while (--spaces);
	    }
	    break;

	  case '\r':
	    putc (c, out);
	    if (flag_format && t < limit && *t != '\n')
	      fprintf (out, flag_format, line_flag);
	    column = 0;
	    break;

	  case '\b':
	    if (column == 0)
	      continue;
	    column--;
	    putc (c, out);
	    break;

	  default:
	    if (textchar[(unsigned char) c])
	      column++;
	    /* fall into */
	  case '\f':
	  case '\v':
	    putc (c, out);
	    break;
	  }
    }
}

int
change_letter (inserts, deletes)
     int inserts, deletes;
{
  if (!inserts)
    return 'd';
  else if (!deletes)
    return 'a';
  else
    return 'c';
}

/* Translate an internal line number (an index into diff's table of lines)
   into an actual line number in the input file.
   The internal line number is LNUM.  FILE points to the data on the file.

   Internal line numbers count from 0 starting after the prefix.
   Actual line numbers count from 1 within the entire file.  */

int
translate_line_number (file, lnum)
     struct file_data *file;
     int lnum;
{
  return lnum + file->prefix_lines + 1;
}

void
translate_range (file, a, b, aptr, bptr)
     struct file_data *file;
     int a, b;
     int *aptr, *bptr;
{
  *aptr = translate_line_number (file, a - 1) + 1;
  *bptr = translate_line_number (file, b + 1) - 1;
}

/* Print a pair of line numbers with SEPCHAR, translated for file FILE.
   If the two numbers are identical, print just one number.

   Args A and B are internal line numbers.
   We print the translated (real) line numbers.  */

void
print_number_range (sepchar, file, a, b)
     char sepchar;
     struct file_data *file;
     int a, b;
{
  int trans_a, trans_b;
  translate_range (file, a, b, &trans_a, &trans_b);

  /* Note: we can have B < A in the case of a range of no lines.
     In this case, we should print the line number before the range,
     which is B.  */
  if (trans_b > trans_a)
    fprintf (outfile, "%d%c%d", trans_a, sepchar, trans_b);
  else
    fprintf (outfile, "%d", trans_b);
}

/* Look at a hunk of edit script and report the range of lines in each file
   that it applies to.  HUNK is the start of the hunk, which is a chain
   of `struct change'.  The first and last line numbers of file 0 are stored in
   *FIRST0 and *LAST0, and likewise for file 1 in *FIRST1 and *LAST1. 
   Note that these are internal line numbers that count from 0.

   If no lines from file 0 are deleted, then FIRST0 is LAST0+1.

   Also set *DELETES nonzero if any lines of file 0 are deleted
   and set *INSERTS nonzero if any lines of file 1 are inserted.
   If only ignorable lines are inserted or deleted, both are
   set to 0.  */

void
analyze_hunk (hunk, first0, last0, first1, last1, deletes, inserts)
     struct change *hunk;
     int *first0, *last0, *first1, *last1;
     int *deletes, *inserts;
{
  int f0, l0, f1, l1, show_from, show_to;
  int i;
  int nontrivial = !(ignore_blank_lines_flag || ignore_regexp_list);
  struct change *next;

  show_from = show_to = 0;

  f0 = hunk->line0;
  f1 = hunk->line1;

  for (next = hunk; next; next = next->link)
    {
      l0 = next->line0 + next->deleted - 1;
      l1 = next->line1 + next->inserted - 1;
      show_from += next->deleted;
      show_to += next->inserted;

      for (i = next->line0; i <= l0 && ! nontrivial; i++)
	if (!ignore_blank_lines_flag || files[0].linbuf[i][0] != '\n')
	  {
	    struct regexp_list *r;
	    const char *line = files[0].linbuf[i];
	    int len = files[0].linbuf[i + 1] - line;

	    for (r = ignore_regexp_list; r; r = r->next)
	      if (0 <= re_search (&r->buf, line, len, 0, len, 0))
		break;	/* Found a match.  Ignore this line.  */
	    /* If we got all the way through the regexp list without
	       finding a match, then it's nontrivial.  */
	    if (r == NULL)
	      nontrivial = 1;
	  }

      for (i = next->line1; i <= l1 && ! nontrivial; i++)
	if (!ignore_blank_lines_flag || files[1].linbuf[i][0] != '\n')
	  {
	    struct regexp_list *r;
	    const char *line = files[1].linbuf[i];
	    int len = files[1].linbuf[i + 1] - line;

	    for (r = ignore_regexp_list; r; r = r->next)
	      if (0 <= re_search (&r->buf, line, len, 0, len, 0))
		break;	/* Found a match.  Ignore this line.  */
	    /* If we got all the way through the regexp list without
	       finding a match, then it's nontrivial.  */
	    if (r == NULL)
	      nontrivial = 1;
	  }
    }

  *first0 = f0;
  *last0 = l0;
  *first1 = f1;
  *last1 = l1;

  /* If all inserted or deleted lines are ignorable,
     tell the caller to ignore this hunk.  */

  if (!nontrivial)
    show_from = show_to = 0;

  *deletes = show_from;
  *inserts = show_to;
}

/* malloc a block of memory, with fatal error message if we can't do it. */

VOID *
xmalloc (size)
     unsigned size;
{
  register VOID *value;

  if (size == 0)
    size = 1;

  value = (VOID *) malloc (size);

  if (!value)
    fatal ("virtual memory exhausted");
  return value;
}

/* realloc a block of memory, with fatal error message if we can't do it. */

VOID *
xrealloc (old, size)
     VOID *old;
     unsigned int size;
{
  register VOID *value;

  if (size == 0)
    size = 1;

  value = (VOID *) realloc (old, size);

  if (!value)
    fatal ("virtual memory exhausted");
  return value;
}

/* Concatenate three strings, returning a newly malloc'd string.  */

char *
concat (s1, s2, s3)
     char *s1, *s2, *s3;
{
  int len = strlen (s1) + strlen (s2) + strlen (s3);
  char *new = (char *) xmalloc (len + 1);
  strcpy (new, s1);
  strcat (new, s2);
  strcat (new, s3);
  return new;
}

void
debug_script (sp)
     struct change *sp;
{
  fflush (stdout);
  for (; sp; sp = sp->link)
    fprintf (stderr, "%3d %3d delete %d insert %d\n",
	     sp->line0, sp->line1, sp->deleted, sp->inserted);
  fflush (stderr);
}

#if !HAVE_MEMCHR
char *
memchr (s, c, n)
     char *s;
     int c;
     size_t n;
{
  unsigned char *p = (unsigned char *) s, *lim = p + n;
  for (;  p < lim;  p++)
    if (*p == c)
      return (char *) p;
  return 0;
}
#endif
