/* input_scrub.c - Break up input buffers into whole numbers of lines.
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef lint
static char rcsid[] = "$FreeBSD: src/gnu/usr.bin/as/input-scrub.c,v 1.7 1999/08/27 23:34:18 peter Exp $";
#endif

#include <errno.h>		/* Need this to make errno declaration right */
#include "as.h"
#include "input-file.h"

/*
 * O/S independent module to supply buffers of sanitised source code
 * to rest of assembler. We get sanitized input data of arbitrary length.
 * We break these buffers on line boundaries, recombine pieces that
 * were broken across buffers, and return a buffer of full lines to
 * the caller.
 * The last partial line begins the next buffer we build and return to caller.
 * The buffer returned to caller is preceeded by BEFORE_STRING and followed
 * by AFTER_STRING, as sentinels. The last character before AFTER_STRING
 * is a newline.
 * Also looks after line numbers, for e.g. error messages.
 */

/*
 * We don't care how filthy our buffers are, but our callers assume
 * that the following sanitation has already been done.
 *
 * No comments, reduce a comment to a space.
 * Reduce a tab to a space unless it is 1st char of line.
 * All multiple tabs and spaces collapsed into 1 char. Tab only
 *   legal if 1st char of line.
 * # line file statements converted to .line x;.file y; statements.
 * Escaped newlines at end of line: remove them but add as many newlines
 *   to end of statement as you removed in the middle, to synch line numbers.
 */

#define BEFORE_STRING ("\n")
#define AFTER_STRING ("\0")	/* memcpy of 0 chars might choke. */
#define BEFORE_SIZE (1)
#define AFTER_SIZE  (1)

static char *buffer_start;	/*->1st char of full buffer area. */
static char *partial_where;	/*->after last full line in buffer. */
static int partial_size;	/* >=0. Number of chars in partial line in buffer. */
static char save_source[AFTER_SIZE];
/* Because we need AFTER_STRING just after last */
/* full line, it clobbers 1st part of partial */
/* line. So we preserve 1st part of partial */
/* line here. */
static unsigned int buffer_length;	/* What is the largest size buffer that */
/* input_file_give_next_buffer() could */
/* return to us? */

/* Saved information about the file that .include'd this one.  When we hit EOF,
   we automatically pop to that file. */

static char *next_saved_file;

/* We can have more than one source file open at once, though the info for all
   but the latest one are saved off in a struct input_save.  These files remain
   open, so we are limited by the number of open files allowed by the
   underlying OS. We may also sequentially read more than one source file in an
   assembly. */

/* We must track the physical file and line number for error messages. We also
   track a "logical" file and line number corresponding to (C?)  compiler
   source line numbers.  Whenever we open a file we must fill in
   physical_input_file. So if it is NULL we have not opened any files yet. */

static char *physical_input_file;
static char *logical_input_file;

typedef unsigned int line_numberT;	/* 1-origin line number in a source file. */
/* A line ends in '\n' or eof. */

static line_numberT physical_input_line;
static int logical_input_line;

/* Struct used to save the state of the input handler during include files */
struct input_save
  {
    char *buffer_start;
    char *partial_where;
    int partial_size;
    char save_source[AFTER_SIZE];
    unsigned int buffer_length;
    char *physical_input_file;
    char *logical_input_file;
    line_numberT physical_input_line;
    int logical_input_line;
    char *next_saved_file;	/* Chain of input_saves */
    char *input_file_save;	/* Saved state of input routines */
    char *saved_position;	/* Caller's saved position in buf */
  };

static char *input_scrub_push PARAMS ((char *saved_position));
static char *input_scrub_pop PARAMS ((char *arg));
static void as_1_char PARAMS ((unsigned int c, FILE * stream));

/* Push the state of input reading and scrubbing so that we can #include.
   The return value is a 'void *' (fudged for old compilers) to a save
   area, which can be restored by passing it to input_scrub_pop(). */
static char *
input_scrub_push (saved_position)
     char *saved_position;
{
  register struct input_save *saved;

  saved = (struct input_save *) xmalloc (sizeof *saved);

  saved->saved_position = saved_position;
  saved->buffer_start = buffer_start;
  saved->partial_where = partial_where;
  saved->partial_size = partial_size;
  saved->buffer_length = buffer_length;
  saved->physical_input_file = physical_input_file;
  saved->logical_input_file = logical_input_file;
  saved->physical_input_line = physical_input_line;
  saved->logical_input_line = logical_input_line;
  memcpy (saved->save_source, save_source, sizeof (save_source));
  saved->next_saved_file = next_saved_file;
  saved->input_file_save = input_file_push ();

  input_file_begin ();		/* Reinitialize! */
  logical_input_line = -1;
  logical_input_file = (char *) NULL;
  buffer_length = input_file_buffer_size ();

  buffer_start = xmalloc ((BEFORE_SIZE + buffer_length + buffer_length + AFTER_SIZE));
  memcpy (buffer_start, BEFORE_STRING, (int) BEFORE_SIZE);

  return ((char *) saved);
}				/* input_scrub_push() */

static char *
input_scrub_pop (arg)
     char *arg;
{
  register struct input_save *saved;
  char *saved_position;

  input_scrub_end ();		/* Finish off old buffer */

  saved = (struct input_save *) arg;

  input_file_pop (saved->input_file_save);
  saved_position = saved->saved_position;
  buffer_start = saved->buffer_start;
  buffer_length = saved->buffer_length;
  physical_input_file = saved->physical_input_file;
  logical_input_file = saved->logical_input_file;
  physical_input_line = saved->physical_input_line;
  logical_input_line = saved->logical_input_line;
  partial_where = saved->partial_where;
  partial_size = saved->partial_size;
  next_saved_file = saved->next_saved_file;
  memcpy (save_source, saved->save_source, sizeof (save_source));

  free (arg);
  return saved_position;
}


void
input_scrub_begin ()
{
  know (strlen (BEFORE_STRING) == BEFORE_SIZE);
  know (strlen (AFTER_STRING) == AFTER_SIZE || (AFTER_STRING[0] == '\0' && AFTER_SIZE == 1));

  input_file_begin ();

  buffer_length = input_file_buffer_size ();

  buffer_start = xmalloc ((BEFORE_SIZE + buffer_length + buffer_length + AFTER_SIZE));
  memcpy (buffer_start, BEFORE_STRING, (int) BEFORE_SIZE);

  /* Line number things. */
  logical_input_line = -1;
  logical_input_file = (char *) NULL;
  physical_input_file = NULL;	/* No file read yet. */
  next_saved_file = NULL;	/* At EOF, don't pop to any other file */
  do_scrub_begin ();
}

void
input_scrub_end ()
{
  if (buffer_start)
    {
      free (buffer_start);
      buffer_start = 0;
      input_file_end ();
    }
}

/* Start reading input from a new file. */

char *				/* Return start of caller's part of buffer. */
input_scrub_new_file (filename)
     char *filename;
{
  input_file_open (filename, !flagseen['f']);
  physical_input_file = filename[0] ? filename : "{standard input}";
  physical_input_line = 0;

  partial_size = 0;
  return (buffer_start + BEFORE_SIZE);
}


/* Include a file from the current file.  Save our state, cause it to
   be restored on EOF, and begin handling a new file.  Same result as
   input_scrub_new_file. */

char *
input_scrub_include_file (filename, position)
     char *filename;
     char *position;
{
  next_saved_file = input_scrub_push (position);
  return input_scrub_new_file (filename);
}

void
input_scrub_close ()
{
  input_file_close ();
}

char *
input_scrub_next_buffer (bufp)
     char **bufp;
{
  register char *limit;		/*->just after last char of buffer. */

  *bufp = buffer_start + BEFORE_SIZE;

  if (partial_size)
    {
      memcpy (buffer_start + BEFORE_SIZE, partial_where,
	      (unsigned int) partial_size);
      memcpy (buffer_start + BEFORE_SIZE, save_source, AFTER_SIZE);
    }
  limit = input_file_give_next_buffer (buffer_start + BEFORE_SIZE + partial_size);
  if (limit)
    {
      register char *p;		/* Find last newline. */

      for (p = limit; *--p != '\n';);;
      ++p;
      if (p <= buffer_start + BEFORE_SIZE)
	{
	  as_fatal ("Source line too long. Please change file %s then rebuild assembler.", __FILE__);
	}
      partial_where = p;
      partial_size = limit - p;
      memcpy (save_source, partial_where, (int) AFTER_SIZE);
      memcpy (partial_where, AFTER_STRING, (int) AFTER_SIZE);
    }
  else
    {
      partial_where = 0;
      if (partial_size > 0)
	{
	  as_warn ("Partial line at end of file ignored");
	}
      /* If we should pop to another file at EOF, do it. */
      if (next_saved_file)
	{
	  *bufp = input_scrub_pop (next_saved_file);	/* Pop state */
	  /* partial_where is now correct to return, since we popped it. */
	}
    }
  return (partial_where);
}				/* input_scrub_next_buffer() */

/*
 * The remaining part of this file deals with line numbers, error
 * messages and so on.
 */


int
seen_at_least_1_file ()		/* TRUE if we opened any file. */
{
  return (physical_input_file != NULL);
}

void
bump_line_counters ()
{
  ++physical_input_line;
  if (logical_input_line >= 0)
    ++logical_input_line;
}

/*
 *			new_logical_line()
 *
 * Tells us what the new logical line number and file are.
 * If the line_number is -1, we don't change the current logical line
 * number.  If it is -2, we decrement the logical line number (this is
 * to support the .appfile pseudo-op inserted into the stream by
 * do_scrub_next_char).
 * If the fname is NULL, we don't change the current logical file name.
 */
void
new_logical_line (fname, line_number)
     char *fname;		/* DON'T destroy it! We point to it! */
     int line_number;
{
  if (fname)
    {
      logical_input_file = fname;
    }				/* if we have a file name */

  if (line_number >= 0)
    logical_input_line = line_number;
  else if (line_number == -2 && logical_input_line > 0)
    --logical_input_line;
}				/* new_logical_line() */

/*
 *			a s _ w h e r e ()
 *
 * Return the current file name and line number.
 * namep should be char * const *, but there are compilers which screw
 * up declarations like that, and it's easier to avoid it.
 */
void
as_where (namep, linep)
     char **namep;
     unsigned int *linep;
{
  if (logical_input_file != NULL
      && (linep == NULL || logical_input_line >= 0))
    {
      *namep = logical_input_file;
      if (linep != NULL)
	*linep = logical_input_line;
    }
  else if (physical_input_file != NULL)
    {
      *namep = physical_input_file;
      if (linep != NULL)
	*linep = physical_input_line;
    }
  else
    {
      *namep = (char *) "*unknown*";
      if (linep != NULL)
	*linep = 0;
    }
}				/* as_where() */




/*
 *			a s _ h o w m u c h ()
 *
 * Output to given stream how much of line we have scanned so far.
 * Assumes we have scanned up to and including input_line_pointer.
 * No free '\n' at end of line.
 */
void
as_howmuch (stream)
     FILE *stream;		/* Opened for write please. */
{
  register char *p;		/* Scan input line. */
  /* register char c; JF unused */

  for (p = input_line_pointer - 1; *p != '\n'; --p)
    {
    }
  ++p;				/* p->1st char of line. */
  for (; p <= input_line_pointer; p++)
    {
      /* Assume ASCII. EBCDIC & other micro-computer char sets ignored. */
      /* c = *p & 0xFF; JF unused */
      as_1_char ((unsigned char) *p, stream);
    }
}

static void
as_1_char (c, stream)
     unsigned int c;
     FILE *stream;
{
  if (c > 127)
    {
      (void) putc ('%', stream);
      c -= 128;
    }
  if (c < 32)
    {
      (void) putc ('^', stream);
      c += '@';
    }
  (void) putc (c, stream);
}

/* end of input_scrub.c */
