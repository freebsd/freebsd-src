/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 */

#ifndef lint
static char sccsid[] = "@(#)input-scrub.c	6.4 (Berkeley) 5/8/91";
#endif /* not lint */

/* input_scrub.c - layer between app and the rest of the world
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "as.h"
#include "read.h"
#include "input-file.h"

/*
 * O/S independent module to supply buffers of sanitised source code
 * to rest of assembler. We get raw input data of some length.
 * Also looks after line numbers, for e.g. error messages.
 * This module used to do the sanitising, but now a pre-processor program
 * (app) does that job so this module is degenerate.
 * Now input is pre-sanitised, so we only worry about finding the
 * last partial line. A buffer of full lines is returned to caller.
 * The last partial line begins the next buffer we build and return to caller.
 * The buffer returned to caller is preceeded by BEFORE_STRING and followed
 * by AFTER_STRING. The last character before AFTER_STRING is a newline.
 */

/*
 * We expect the following sanitation has already been done.
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
#define AFTER_STRING ("\0")	/* bcopy of 0 chars might choke. */
#define BEFORE_SIZE (1)
#define AFTER_SIZE  (1)

static char *	buffer_start;	/* -> 1st char of full buffer area. */
static char *	partial_where;	/* -> after last full line in buffer. */
static int	partial_size;	/* >=0. Number of chars in partial line in buffer. */
static char	save_source [AFTER_SIZE];
				/* Because we need AFTER_STRING just after last */
				/* full line, it clobbers 1st part of partial */
				/* line. So we preserve 1st part of partial */
				/* line here. */
static int	buffer_length;	/* What is the largest size buffer that */
				/* input_file_give_next_buffer() could */
				/* return to us? */

static void as_1_char ();

/*
We never have more than one source file open at once.
We may, however, read more than 1 source file in an assembly.
NULL means we have no file open right now.
*/


/*
We must track the physical file and line number for error messages.
We also track a "logical" file and line number corresponding to (C?)
compiler source line numbers.
Whenever we open a file we must fill in physical_input_file. So if it is NULL
we have not opened any files yet.
*/

static
char *		physical_input_file,
     *		logical_input_file;



typedef unsigned int line_numberT;	/* 1-origin line number in a source file. */
				/* A line ends in '\n' or eof. */

static
line_numberT	physical_input_line,
		logical_input_line;

void
input_scrub_begin ()
{
  know( strlen(BEFORE_STRING) == BEFORE_SIZE );
  know( strlen( AFTER_STRING) ==  AFTER_SIZE );

  input_file_begin ();

  buffer_length = input_file_buffer_size ();

  buffer_start = xmalloc ((long)(BEFORE_SIZE + buffer_length + buffer_length + AFTER_SIZE));
  bcopy (BEFORE_STRING, buffer_start, (int)BEFORE_SIZE);

  /* Line number things. */
  logical_input_line = 0;
  logical_input_file = (char *)NULL;
  physical_input_file = NULL;	/* No file read yet. */
  do_scrub_begin();
}

void
input_scrub_end ()
{
  input_file_end ();
}

char *				/* Return start of caller's part of buffer. */
input_scrub_new_file (filename)
     char *	filename;
{
  input_file_open (filename, !flagseen['f']);
  physical_input_file = filename[0] ? filename : "{standard input}";
  physical_input_line = 0;

  partial_size = 0;
  return (buffer_start + BEFORE_SIZE);
}

char *
input_scrub_next_buffer (bufp)
char **bufp;
{
  register char *	limit;	/* -> just after last char of buffer. */

#ifdef DONTDEF
  if(preprocess) {
    if(save_buffer) {
      *bufp = save_buffer;
      save_buffer = 0;
    }
    limit = input_file_give_next_buffer(buffer_start+BEFORE_SIZE);
    if (!limit) {
      partial_where = 0;
      if(partial_size)
        as_warn("Partial line at end of file ignored");
      return partial_where;
    }

    if(partial_size)
      bcopy(save_source, partial_where,(int)AFTER_SIZE);
    do_scrub(partial_where,partial_size,buffer_start+BEFORE_SIZE,limit-(buffer_start+BEFORE_SIZE),&out_string,&out_length);
    limit=out_string + out_length;
    for(p=limit;*--p!='\n';)
      ;
    p++;
    if(p<=buffer_start+BEFORE_SIZE)
      as_fatal("Source line too long.  Please change file '%s' and re-make the assembler.",__FILE__);

    partial_where = p;
    partial_size = limit-p;
    bcopy(partial_where, save_source,(int)AFTER_SIZE);
    bcopy(AFTER_STRING, partial_where, (int)AFTER_SIZE);

    save_buffer = *bufp;
    *bufp = out_string;

    return partial_where;
  }

  /* We're not preprocessing.  Do the right thing */
#endif
  if (partial_size)
    {
      bcopy (partial_where, buffer_start + BEFORE_SIZE, (int)partial_size);
      bcopy (save_source, buffer_start + BEFORE_SIZE, (int)AFTER_SIZE);
    }
  limit = input_file_give_next_buffer (buffer_start + BEFORE_SIZE + partial_size);
  if (limit)
    {
      register char *	p;	/* Find last newline. */

      for (p = limit;   * -- p != '\n';   )
	{
	}
      ++ p;
      if (p <= buffer_start + BEFORE_SIZE)
	{
	  as_fatal ("Source line too long. Please change file %s then rebuild assembler.", __FILE__);
	}
      partial_where = p;
      partial_size = limit - p;
      bcopy (partial_where, save_source,  (int)AFTER_SIZE);
      bcopy (AFTER_STRING, partial_where, (int)AFTER_SIZE);
    }
  else
    {
      partial_where = 0;
      if (partial_size > 0)
	{
	  as_warn( "Partial line at end of file ignored" );
	}
    }
  return (partial_where);
}

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
  ++ physical_input_line;
  ++ logical_input_line;
}

/*
 *			new_logical_line()
 *
 * Tells us what the new logical line number and file are.
 * If the line_number is <0, we don't change the current logical line number.
 * If the fname is NULL, we don't change the current logical file name.
 */
void
new_logical_line (fname, line_number)
     char *	fname;		/* DON'T destroy it! We point to it! */
     int	line_number;
{
  if ( fname )
    {
      logical_input_file = fname;
    }
  if ( line_number >= 0 )
    {
      logical_input_line = line_number;
    }
}

/*
 *			a s _ w h e r e ( )
 *
 * Write a line to stderr locating where we are in reading
 * input source files.
 * As a sop to the debugger of AS, pretty-print the offending line.
 */
void
as_where()
{
  char *p;
  line_numberT line;

  if (physical_input_file)
    {				/* we tried to read SOME source */
      if (input_file_is_open())
	{			/* we can still read lines from source */
#ifdef DONTDEF
	  fprintf (stderr," @ physical line %ld., file \"%s\"",
		   (long) physical_input_line, physical_input_file);
	  fprintf (stderr," @ logical line %ld., file \"%s\"\n",
		   (long) logical_input_line, logical_input_file);
	  (void)putc(' ', stderr);
	  as_howmuch (stderr);
	  (void)putc('\n', stderr);
#else
		p = logical_input_file ? logical_input_file : physical_input_file;
		line = logical_input_line ? logical_input_line : physical_input_line;
		fprintf(stderr,"%s:%u:", p, line);
#endif
	}
      else
	{
#ifdef DONTDEF
	  fprintf (stderr," After reading source.\n");
#else
	p = logical_input_file ? logical_input_file : physical_input_file;
	line = logical_input_line ? logical_input_line : physical_input_line;
	fprintf (stderr,"%s:unknown:", p);
#endif
	}
    }
  else
    {
#ifdef DONTDEF
      fprintf (stderr," Before reading source.\n");
#else
#endif
    }
}

/*
 * Support for source file debugging.  These functions handle
 * logical lines and logical files.
 */
static char *saved_file;
static int saved_len;
static line_numberT saved_line;

void
filestab()
{
  char *file;
  int len;

  if (!physical_input_file ||
      !input_file_is_open())
    return;

  file = logical_input_file ? logical_input_file : physical_input_file;

  if (saved_file == 0 || strcmp(file, saved_file) != 0)
    {
      stabs(file);
      len = strlen(file) + 1;
      if (len > saved_len)
	{
	  if (saved_file == 0)
	    saved_file = xmalloc(len);
	  else
	    saved_file = xrealloc(saved_file, len);
	  memcpy(saved_file, file, len);
	  saved_len = len;
	}
      else
	strcpy(saved_file, file);
      saved_line = 0;
    }
}

void
funcstab(func)
     char *func;
{
  if (now_seg != SEG_TEXT)
    return;

  filestab();
  stabf(func);
}

void
linestab()
{
  line_numberT line;

  if (now_seg != SEG_TEXT)
    return;

  filestab();

  line = logical_input_line ? logical_input_line : physical_input_line;

  if (saved_line == 0 || line != saved_line)
    {
      stabd(line);
      saved_line = line;
    }
}

/*
 *			a s _ h o w m u c h ( )
 *
 * Output to given stream how much of line we have scanned so far.
 * Assumes we have scanned up to and including input_line_pointer.
 * No free '\n' at end of line.
 */
void
as_howmuch (stream)
     FILE * stream;		/* Opened for write please. */
{
  register	char *	p;	/* Scan input line. */
  /* register	char	c; JF unused */

  for (p = input_line_pointer - 1;   * p != '\n';   --p)
    {
    }
  ++ p;				/* p -> 1st char of line. */
  for (;  p <= input_line_pointer;  p++)
    {
      /* Assume ASCII. EBCDIC & other micro-computer char sets ignored. */
      /* c = *p & 0xFF; JF unused */
      as_1_char (*p, stream);
    }
}

static void
as_1_char (c,stream)
     unsigned char c;
     FILE *	stream;
{
  if ( c > 127 )
    {
      (void)putc( '%', stream);
      c -= 128;
    }
  if ( c < 32 )
    {
      (void)putc( '^', stream);
      c += '@';
    }
  (void)putc( c, stream);
}

/* end: input_scrub.c */
