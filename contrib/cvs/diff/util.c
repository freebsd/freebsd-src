/* Support routines for GNU DIFF.
   Copyright (C) 1988, 1989, 1992, 1993, 1994, 1997, 1998 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "diff.h"

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifndef strerror
extern char *strerror ();
#endif

/* Queue up one-line messages to be printed at the end,
   when -l is specified.  Each message is recorded with a `struct msg'.  */

struct msg
{
  struct msg *next;
  char const *format;
  char const *arg1;
  char const *arg2;
  char const *arg3;
  char const *arg4;
};

/* Head of the chain of queues messages.  */

static struct msg *msg_chain;

/* Tail of the chain of queues messages.  */

static struct msg **msg_chain_end = &msg_chain;

/* Use when a system call returns non-zero status.
   TEXT should normally be the file name.  */

void
perror_with_name (text)
     char const *text;
{
  int e = errno;

  if (callbacks && callbacks->error)
    (*callbacks->error) ("%s: %s", text, strerror (e));
  else
    {
      fprintf (stderr, "%s: ", diff_program_name);
      errno = e;
      perror (text);
    }
}

/* Use when a system call returns non-zero status and that is fatal.  */

void
pfatal_with_name (text)
     char const *text;
{
  int e = errno;
  print_message_queue ();
  if (callbacks && callbacks->error)
    (*callbacks->error) ("%s: %s", text, strerror (e));
  else
    {
      fprintf (stderr, "%s: ", diff_program_name);
      errno = e;
      perror (text);
    }
  DIFF_ABORT (2);
}

/* Print an error message from the format-string FORMAT
   with args ARG1 and ARG2.  */

void
diff_error (format, arg, arg1)
     char const *format, *arg, *arg1;
{
  if (callbacks && callbacks->error)
    (*callbacks->error) (format, arg, arg1);
  else
    {
      fprintf (stderr, "%s: ", diff_program_name);
      fprintf (stderr, format, arg, arg1);
      fprintf (stderr, "\n");
    }
}

/* Print an error message containing the string TEXT, then exit.  */

void
fatal (m)
     char const *m;
{
  print_message_queue ();
  diff_error ("%s", m, 0);
  DIFF_ABORT (2);
}

/* Like printf, except if -l in effect then save the message and print later.
   This is used for things like "binary files differ" and "Only in ...".  */

void
message (format, arg1, arg2)
     char const *format, *arg1, *arg2;
{
  message5 (format, arg1, arg2, 0, 0);
}

void
message5 (format, arg1, arg2, arg3, arg4)
     char const *format, *arg1, *arg2, *arg3, *arg4;
{
  if (paginate_flag)
    {
      struct msg *new = (struct msg *) xmalloc (sizeof (struct msg));
      new->format = format;
      new->arg1 = concat (arg1, "", "");
      new->arg2 = concat (arg2, "", "");
      new->arg3 = arg3 ? concat (arg3, "", "") : 0;
      new->arg4 = arg4 ? concat (arg4, "", "") : 0;
      new->next = 0;
      *msg_chain_end = new;
      msg_chain_end = &new->next;
    }
  else
    {
      if (sdiff_help_sdiff)
	write_output (" ", 1);
      printf_output (format, arg1, arg2, arg3, arg4);
    }
}

/* Output all the messages that were saved up by calls to `message'.  */

void
print_message_queue ()
{
  struct msg *m;

  for (m = msg_chain; m; m = m->next)
    printf_output (m->format, m->arg1, m->arg2, m->arg3, m->arg4);
}

/* Call before outputting the results of comparing files NAME0 and NAME1
   to set up OUTFILE, the stdio stream for the output to go to.

   Usually, OUTFILE is just stdout.  But when -l was specified
   we fork off a `pr' and make OUTFILE a pipe to it.
   `pr' then outputs to our stdout.  */

static char const *current_name0;
static char const *current_name1;
static int current_depth;

static int output_in_progress = 0;

void
setup_output (name0, name1, depth)
     char const *name0, *name1;
     int depth;
{
  current_name0 = name0;
  current_name1 = name1;
  current_depth = depth;
}

#if HAVE_FORK && defined (PR_PROGRAM)
static pid_t pr_pid;
#endif

void
begin_output ()
{
  char *name;

  if (output_in_progress)
    return;
  output_in_progress = 1;

  /* Construct the header of this piece of diff.  */
  name = xmalloc (strlen (current_name0) + strlen (current_name1)
		  + strlen (switch_string) + 7);
  /* Posix.2 section 4.17.6.1.1 specifies this format.  But there is a
     bug in the first printing (IEEE Std 1003.2-1992 p 251 l 3304):
     it says that we must print only the last component of the pathnames.
     This requirement is silly and does not match historical practice.  */
  sprintf (name, "diff%s %s %s", switch_string, current_name0, current_name1);

  if (paginate_flag && callbacks && callbacks->write_output)
    fatal ("can't paginate when using library callbacks");

  if (paginate_flag)
    {
      /* Make OUTFILE a pipe to a subsidiary `pr'.  */

#ifdef PR_PROGRAM

# if HAVE_FORK
      int pipes[2];

      if (pipe (pipes) != 0)
	pfatal_with_name ("pipe");

      fflush (stdout);

      pr_pid = vfork ();
      if (pr_pid < 0)
	pfatal_with_name ("vfork");

      if (pr_pid == 0)
	{
	  close (pipes[1]);
	  if (pipes[0] != STDIN_FILENO)
	    {
	      if (dup2 (pipes[0], STDIN_FILENO) < 0)
		pfatal_with_name ("dup2");
	      close (pipes[0]);
	    }

	  execl (PR_PROGRAM, PR_PROGRAM, "-f", "-h", name, 0);
	  pfatal_with_name (PR_PROGRAM);
	}
      else
	{
	  close (pipes[0]);
	  outfile = fdopen (pipes[1], "w");
	  if (!outfile)
	    pfatal_with_name ("fdopen");
	}
# else /* ! HAVE_FORK */
      char *command = xmalloc (4 * strlen (name) + strlen (PR_PROGRAM) + 10);
      char *p;
      char const *a = name;
      sprintf (command, "%s -f -h ", PR_PROGRAM);
      p = command + strlen (command);
      SYSTEM_QUOTE_ARG (p, a);
      *p = 0;
      outfile = popen (command, "w");
      if (!outfile)
	pfatal_with_name (command);
      free (command);
# endif /* ! HAVE_FORK */
#else
      fatal ("This port does not support the --paginate option to diff.");
#endif
    }
  else
    {

      /* If -l was not specified, output the diff straight to `stdout'.  */

      /* If handling multiple files (because scanning a directory),
	 print which files the following output is about.  */
      if (current_depth > 0)
	printf_output ("%s\n", name);
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
   If -l was given, close OUTFILE and get rid of the `pr' subfork.  */

void
finish_output ()
{
  if (paginate_flag && outfile != 0 && outfile != stdout)
    {
#ifdef PR_PROGRAM
      int wstatus, w;
      if (ferror (outfile))
	fatal ("write error");
# if ! HAVE_FORK
      wstatus = pclose (outfile);
# else /* HAVE_FORK */
      if (fclose (outfile) != 0)
	pfatal_with_name ("write error");
      while ((w = waitpid (pr_pid, &wstatus, 0)) < 0 && errno == EINTR)
	;
      if (w < 0)
	pfatal_with_name ("waitpid");
# endif /* HAVE_FORK */
      if (wstatus != 0)
	fatal ("subsidiary pr failed");
#else
      fatal ("internal error in finish_output");
#endif
    }

  output_in_progress = 0;
}

/* Write something to the output file.  */

void
write_output (text, len)
     char const *text;
     size_t len;
{
  if (callbacks && callbacks->write_output)
    (*callbacks->write_output) (text, len);
  else if (len == 1)
    putc (*text, outfile);
  else
    fwrite (text, sizeof (char), len, outfile);
}

/* Printf something to the output file.  */

#if __STDC__
#define VA_START(args, lastarg) va_start(args, lastarg)
#else /* ! __STDC__ */
#define VA_START(args, lastarg) va_start(args)
#endif /* __STDC__ */

void
#if __STDC__
printf_output (const char *format, ...)
#else
printf_output (format, va_alist)
     char const *format;
     va_dcl
#endif
{
  va_list args;

  VA_START (args, format);
  if (callbacks && callbacks->write_output)
    {
      /* We implement our own limited printf-like functionality (%s, %d,
	 and %c only).  Callers who want something fancier can use
	 sprintf.  */
      const char *p = format;
      char *q;
      char *str;
      int num;
      int ch;
      char buf[100];

      while ((q = strchr (p, '%')) != NULL)
	{
	  static const char msg[] =
	    "\ninternal error: bad % in printf_output\n";
	  (*callbacks->write_output) (p, q - p);

	  switch (q[1])
	    {
	    case 's':
	      str = va_arg (args, char *);
	      (*callbacks->write_output) (str, strlen (str));
	      break;
	    case 'd':
	      num = va_arg (args, int);
	      sprintf (buf, "%d", num);
	      (*callbacks->write_output) (buf, strlen (buf));
	      break;
	    case 'c':
	      ch = va_arg (args, int);
	      buf[0] = ch;
	      (*callbacks->write_output) (buf, 1);
	      break;
	    default:
	      (*callbacks->write_output) (msg, sizeof (msg) - 1);
	      /* Don't just keep going, because q + 1 might point to the
		 terminating '\0'.  */
	      goto out;
	    }
	  p = q + 2;
	}
      (*callbacks->write_output) (p, strlen (p));
    }
  else
    vfprintf (outfile, format, args);
 out:
  va_end (args);
}

/* Flush the output file.  */

void
flush_output ()
{
  if (callbacks && callbacks->flush_output)
    (*callbacks->flush_output) ();
  else
    fflush (outfile);
}

/* Compare two lines (typically one from each input file)
   according to the command line options.
   For efficiency, this is invoked only when the lines do not match exactly
   but an option like -i might cause us to ignore the difference.
   Return nonzero if the lines differ.  */

int
line_cmp (s1, s2)
     char const *s1, *s2;
{
  register unsigned char const *t1 = (unsigned char const *) s1;
  register unsigned char const *t2 = (unsigned char const *) s2;

  while (1)
    {
      register unsigned char c1 = *t1++;
      register unsigned char c2 = *t2++;

      /* Test for exact char equality first, since it's a common case.  */
      if (c1 != c2)
	{
	  /* Ignore horizontal white space if -b or -w is specified.  */

	  if (ignore_all_space_flag)
	    {
	      /* For -w, just skip past any white space.  */
	      while (ISSPACE (c1) && c1 != '\n') c1 = *t1++;
	      while (ISSPACE (c2) && c2 != '\n') c2 = *t2++;
	    }
	  else if (ignore_space_change_flag)
	    {
	      /* For -b, advance past any sequence of white space in line 1
		 and consider it just one Space, or nothing at all
		 if it is at the end of the line.  */
	      if (ISSPACE (c1))
		{
		  while (c1 != '\n')
		    {
		      c1 = *t1++;
		      if (! ISSPACE (c1))
			{
			  --t1;
			  c1 = ' ';
			  break;
			}
		    }
		}

	      /* Likewise for line 2.  */
	      if (ISSPACE (c2))
		{
		  while (c2 != '\n')
		    {
		      c2 = *t2++;
		      if (! ISSPACE (c2))
			{
			  --t2;
			  c2 = ' ';
			  break;
			}
		    }
		}

	      if (c1 != c2)
		{
		  /* If we went too far when doing the simple test
		     for equality, go back to the first non-white-space
		     character in both sides and try again.  */
		  if (c2 == ' ' && c1 != '\n'
		      && (unsigned char const *) s1 + 1 < t1
		      && ISSPACE(t1[-2]))
		    {
		      --t1;
		      continue;
		    }
		  if (c1 == ' ' && c2 != '\n'
		      && (unsigned char const *) s2 + 1 < t2
		      && ISSPACE(t2[-2]))
		    {
		      --t2;
		      continue;
		    }
		}
	    }

	  /* Lowercase all letters if -i is specified.  */

	  if (ignore_case_flag)
	    {
	      if (ISUPPER (c1))
		c1 = tolower (c1);
	      if (ISUPPER (c2))
		c2 = tolower (c2);
	    }

	  if (c1 != c2)
	    break;
	}
      if (c1 == '\n')
	return 0;
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
     struct change * (*hunkfun) PARAMS((struct change *));
     void (*printfun) PARAMS((struct change *));
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
      end->link = 0;
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
     char const *line_flag;
     char const * const *line;
{
  char const *text = line[0], *limit = line[1]; /* Help the compiler.  */
  char const *flag_format = 0;

  /* If -T was specified, use a Tab between the line-flag and the text.
     Otherwise use a Space (as Unix diff does).
     Print neither space nor tab if line-flags are empty.  */

  if (line_flag && *line_flag)
    {
      flag_format = tab_align_flag ? "%s\t" : "%s ";
      printf_output (flag_format, line_flag);
    }

  output_1_line (text, limit, flag_format, line_flag);

  if ((!line_flag || line_flag[0]) && limit[-1] != '\n')
    printf_output ("\n\\ No newline at end of file\n");
}

/* Output a line from TEXT up to LIMIT.  Without -t, output verbatim.
   With -t, expand white space characters to spaces, and if FLAG_FORMAT
   is nonzero, output it with argument LINE_FLAG after every
   internal carriage return, so that tab stops continue to line up.  */

void
output_1_line (text, limit, flag_format, line_flag)
     char const *text, *limit, *flag_format, *line_flag;
{
  if (!tab_expand_flag)
    write_output (text, limit - text);
  else
    {
      register unsigned char c;
      register char const *t = text;
      register unsigned column = 0;
      /* CC is used to avoid taking the address of the register
         variable C.  */
      char cc;

      while (t < limit)
	switch ((c = *t++))
	  {
	  case '\t':
	    {
	      unsigned spaces = TAB_WIDTH - column % TAB_WIDTH;
	      column += spaces;
	      do
		write_output (" ", 1);
	      while (--spaces);
	    }
	    break;

	  case '\r':
	    write_output ("\r", 1);
	    if (flag_format && t < limit && *t != '\n')
	      printf_output (flag_format, line_flag);
	    column = 0;
	    break;

	  case '\b':
	    if (column == 0)
	      continue;
	    column--;
	    write_output ("\b", 1);
	    break;

	  default:
	    if (ISPRINT (c))
	      column++;
	    cc = c;
	    write_output (&cc, 1);
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
     struct file_data const *file;
     int lnum;
{
  return lnum + file->prefix_lines + 1;
}

void
translate_range (file, a, b, aptr, bptr)
     struct file_data const *file;
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
     int sepchar;
     struct file_data *file;
     int a, b;
{
  int trans_a, trans_b;
  translate_range (file, a, b, &trans_a, &trans_b);

  /* Note: we can have B < A in the case of a range of no lines.
     In this case, we should print the line number before the range,
     which is B.  */
  if (trans_b > trans_a)
    printf_output ("%d%c%d", trans_a, sepchar, trans_b);
  else
    printf_output ("%d", trans_b);
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
  int l0, l1, show_from, show_to;
  int i;
  int trivial = ignore_blank_lines_flag || ignore_regexp_list;
  struct change *next;

  show_from = show_to = 0;

  *first0 = hunk->line0;
  *first1 = hunk->line1;

  next = hunk;
  do
    {
      l0 = next->line0 + next->deleted - 1;
      l1 = next->line1 + next->inserted - 1;
      show_from += next->deleted;
      show_to += next->inserted;

      for (i = next->line0; i <= l0 && trivial; i++)
	if (!ignore_blank_lines_flag || files[0].linbuf[i][0] != '\n')
	  {
	    struct regexp_list *r;
	    char const *line = files[0].linbuf[i];
	    int len = files[0].linbuf[i + 1] - line;

	    for (r = ignore_regexp_list; r; r = r->next)
	      if (0 <= re_search (&r->buf, line, len, 0, len, 0))
		break;	/* Found a match.  Ignore this line.  */
	    /* If we got all the way through the regexp list without
	       finding a match, then it's nontrivial.  */
	    if (!r)
	      trivial = 0;
	  }

      for (i = next->line1; i <= l1 && trivial; i++)
	if (!ignore_blank_lines_flag || files[1].linbuf[i][0] != '\n')
	  {
	    struct regexp_list *r;
	    char const *line = files[1].linbuf[i];
	    int len = files[1].linbuf[i + 1] - line;

	    for (r = ignore_regexp_list; r; r = r->next)
	      if (0 <= re_search (&r->buf, line, len, 0, len, 0))
		break;	/* Found a match.  Ignore this line.  */
	    /* If we got all the way through the regexp list without
	       finding a match, then it's nontrivial.  */
	    if (!r)
	      trivial = 0;
	  }
    }
  while ((next = next->link) != 0);

  *last0 = l0;
  *last1 = l1;

  /* If all inserted or deleted lines are ignorable,
     tell the caller to ignore this hunk.  */

  if (trivial)
    show_from = show_to = 0;

  *deletes = show_from;
  *inserts = show_to;
}

/* Concatenate three strings, returning a newly malloc'd string.  */

char *
concat (s1, s2, s3)
     char const *s1, *s2, *s3;
{
  size_t len = strlen (s1) + strlen (s2) + strlen (s3);
  char *new = xmalloc (len + 1);
  sprintf (new, "%s%s%s", s1, s2, s3);
  return new;
}

/* Yield the newly malloc'd pathname
   of the file in DIR whose filename is FILE.  */

char *
dir_file_pathname (dir, file)
     char const *dir, *file;
{
  char const *p = filename_lastdirchar (dir);
  return concat (dir, "/" + (p && !p[1]), file);
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
