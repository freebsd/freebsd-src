/* histfile.c - functions to manipulate the history file. */

/* Copyright (C) 1989, 1992 Free Software Foundation, Inc.

   This file contains the GNU History Library (the Library), a set of
   routines for managing the text of previously typed lines.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   The Library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */

/* The goal is to make the implementation transparent, so that you
   don't have to know what data types are used, just what functions
   you can call.  I think I have done that. */
#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (HAVE_STRING_H)
#  include <string.h>
#else
#  include <strings.h>
#endif /* !HAVE_STRING_H */

#include <errno.h>
#if !defined (errno)
extern int errno;
#endif /* !errno */

#include "history.h"
#include "histlib.h"

extern char *xmalloc (), *xrealloc ();

/* Return the string that should be used in the place of this
   filename.  This only matters when you don't specify the
   filename to read_history (), or write_history (). */
static char *
history_filename (filename)
     char *filename;
{
  char *return_val, *home;
  int home_len;

  return_val = filename ? savestring (filename) : (char *)NULL;

  if (return_val)
    return (return_val);
  
  home = getenv ("HOME");

  if (home == 0)
    {
      home = ".";
      home_len = 1;
    }
  else
    home_len = strlen (home);

  return_val = xmalloc (2 + home_len + 8); /* strlen(".history") == 8 */
  strcpy (return_val, home);
  return_val[home_len] = '/';
  strcpy (return_val + home_len + 1, ".history");

  return (return_val);
}

/* Add the contents of FILENAME to the history list, a line at a time.
   If FILENAME is NULL, then read from ~/.history.  Returns 0 if
   successful, or errno if not. */
int
read_history (filename)
     char *filename;
{
  return (read_history_range (filename, 0, -1));
}

/* Read a range of lines from FILENAME, adding them to the history list.
   Start reading at the FROM'th line and end at the TO'th.  If FROM
   is zero, start at the beginning.  If TO is less than FROM, read
   until the end of the file.  If FILENAME is NULL, then read from
   ~/.history.  Returns 0 if successful, or errno if not. */
int
read_history_range (filename, from, to)
     char *filename;
     int from, to;
{
  register int line_start, line_end;
  char *input, *buffer = (char *)NULL;
  int file, current_line;
  struct stat finfo;

  input = history_filename (filename);
  file = open (input, O_RDONLY, 0666);

  if ((file < 0) || (fstat (file, &finfo) == -1))
    goto error_and_exit;

  buffer = xmalloc ((int)finfo.st_size + 1);

  if (read (file, buffer, finfo.st_size) != finfo.st_size)
    {
  error_and_exit:
      if (file >= 0)
	close (file);

      FREE (input);
      FREE (buffer);

      return (errno);
    }

  close (file);

  /* Set TO to larger than end of file if negative. */
  if (to < 0)
    to = finfo.st_size;

  /* Start at beginning of file, work to end. */
  line_start = line_end = current_line = 0;

  /* Skip lines until we are at FROM. */
  while (line_start < finfo.st_size && current_line < from)
    {
      for (line_end = line_start; line_end < finfo.st_size; line_end++)
	if (buffer[line_end] == '\n')
	  {
	    current_line++;
	    line_start = line_end + 1;
	    if (current_line == from)
	      break;
	  }
    }

  /* If there are lines left to gobble, then gobble them now. */
  for (line_end = line_start; line_end < finfo.st_size; line_end++)
    if (buffer[line_end] == '\n')
      {
	buffer[line_end] = '\0';

	if (buffer[line_start])
	  add_history (buffer + line_start);

	current_line++;

	if (current_line >= to)
	  break;

	line_start = line_end + 1;
      }

  FREE (input);
  FREE (buffer);

  return (0);
}

/* Truncate the history file FNAME, leaving only LINES trailing lines.
   If FNAME is NULL, then use ~/.history. */
int
history_truncate_file (fname, lines)
     char *fname;
     register int lines;
{
  register int i;
  int file, chars_read;
  char *buffer = (char *)NULL, *filename;
  struct stat finfo;

  filename = history_filename (fname);
  file = open (filename, O_RDONLY, 0666);

  if (file == -1 || fstat (file, &finfo) == -1)
    goto truncate_exit;

  buffer = xmalloc ((int)finfo.st_size + 1);
  chars_read = read (file, buffer, finfo.st_size);
  close (file);

  if (chars_read <= 0)
    goto truncate_exit;

  /* Count backwards from the end of buffer until we have passed
     LINES lines. */
  for (i = chars_read - 1; lines && i; i--)
    {
      if (buffer[i] == '\n')
	lines--;
    }

  /* If this is the first line, then the file contains exactly the
     number of lines we want to truncate to, so we don't need to do
     anything.  It's the first line if we don't find a newline between
     the current value of i and 0.  Otherwise, write from the start of
     this line until the end of the buffer. */
  for ( ; i; i--)
    if (buffer[i] == '\n')
      {
	i++;
	break;
      }

  /* Write only if there are more lines in the file than we want to
     truncate to. */
  if (i && ((file = open (filename, O_WRONLY|O_TRUNC, 0666)) != -1))
    {
      write (file, buffer + i, finfo.st_size - i);
      close (file);
    }

 truncate_exit:

  FREE (buffer);

  free (filename);
  return 0;
}

/* Workhorse function for writing history.  Writes NELEMENT entries
   from the history list to FILENAME.  OVERWRITE is non-zero if you
   wish to replace FILENAME with the entries. */
static int
history_do_write (filename, nelements, overwrite)
     char *filename;
     int nelements, overwrite;
{
  register int i;
  char *output = history_filename (filename);
  int file, mode;

  mode = overwrite ? O_WRONLY | O_CREAT | O_TRUNC : O_WRONLY | O_APPEND;

  if ((file = open (output, mode, 0666)) == -1)
    {
      FREE (output);
      return (errno);
    }

  if (nelements > history_length)
    nelements = history_length;

  /* Build a buffer of all the lines to write, and write them in one syscall.
     Suggested by Peter Ho (peter@robosts.oxford.ac.uk). */
  {
    HIST_ENTRY **the_history;	/* local */
    register int j;
    int buffer_size;
    char *buffer;

    the_history = history_list ();
    /* Calculate the total number of bytes to write. */
    for (buffer_size = 0, i = history_length - nelements; i < history_length; i++)
      buffer_size += 1 + strlen (the_history[i]->line);

    /* Allocate the buffer, and fill it. */
    buffer = xmalloc (buffer_size);

    for (j = 0, i = history_length - nelements; i < history_length; i++)
      {
	strcpy (buffer + j, the_history[i]->line);
	j += strlen (the_history[i]->line);
	buffer[j++] = '\n';
      }

    write (file, buffer, buffer_size);
    free (buffer);
  }

  close (file);

  FREE (output);

  return (0);
}

/* Append NELEMENT entries to FILENAME.  The entries appended are from
   the end of the list minus NELEMENTs up to the end of the list. */
int
append_history (nelements, filename)
     int nelements;
     char *filename;
{
  return (history_do_write (filename, nelements, HISTORY_APPEND));
}

/* Overwrite FILENAME with the current history.  If FILENAME is NULL,
   then write the history list to ~/.history.  Values returned
   are as in read_history ().*/
int
write_history (filename)
     char *filename;
{
  return (history_do_write (filename, history_length, HISTORY_OVERWRITE));
}
