/* History.c -- standalone history library */

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

/* Not all systems declare ERRNO in errno.h... and some systems #define it! */
#if !defined (errno)
extern int errno;
#endif /* !errno */

#include "memalloc.h"
#include <readline/history.h>

#if defined (STATIC_MALLOC)
static char *xmalloc (), *xrealloc ();
#else
extern char *xmalloc (), *xrealloc ();
#endif /* STATIC_MALLOC */

#define STREQ(a, b)	(((a)[0] == (b)[0]) && (strcmp ((a), (b)) == 0))
#define STREQN(a, b, n)	(((a)[0] == (b)[0]) && (strncmp ((a), (b), (n)) == 0))

#ifndef savestring
#  ifndef strcpy
extern char *strcpy ();
#  endif
#define savestring(x) strcpy (xmalloc (1 + strlen (x)), (x))
#endif

#ifndef whitespace
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))
#endif

#ifndef digit
#define digit(c)  ((c) >= '0' && (c) <= '9')
#endif

#ifndef digit_value
#define digit_value(c) ((c) - '0')
#endif

#ifndef member
#  ifndef strchr
extern char *strchr ();
#  endif
#define member(c, s) ((c) ? ((char *)strchr ((s), (c)) != (char *)NULL) : 0)
#endif

/* Possible history errors passed to hist_error. */
#define EVENT_NOT_FOUND 0
#define BAD_WORD_SPEC	1
#define SUBST_FAILED	2
#define BAD_MODIFIER	3

static char error_pointer;

static char *subst_lhs;
static char *subst_rhs;
static int subst_lhs_len = 0;
static int subst_rhs_len = 0;

static char *get_history_word_specifier ();

#if defined (SHELL)
extern char *single_quote ();
#endif

/* **************************************************************** */
/*								    */
/*			History Functions			    */
/*								    */
/* **************************************************************** */

/* An array of HIST_ENTRY.  This is where we store the history. */
static HIST_ENTRY **the_history = (HIST_ENTRY **)NULL;

/* Non-zero means that we have enforced a limit on the amount of
   history that we save. */
int history_stifled = 0;

/* If HISTORY_STIFLED is non-zero, then this is the maximum number of
   entries to remember. */
int max_input_history;

/* The current location of the interactive history pointer.  Just makes
   life easier for outside callers. */
static int history_offset = 0;

/* The number of strings currently stored in the input_history list. */
int history_length = 0;

/* The current number of slots allocated to the input_history. */
static int history_size = 0;

/* The number of slots to increase the_history by. */
#define DEFAULT_HISTORY_GROW_SIZE 50

/* The character that represents the start of a history expansion
   request.  This is usually `!'. */
char history_expansion_char = '!';

/* The character that invokes word substitution if found at the start of
   a line.  This is usually `^'. */
char history_subst_char = '^';

/* During tokenization, if this character is seen as the first character
   of a word, then it, and all subsequent characters upto a newline are
   ignored.  For a Bourne shell, this should be '#'.  Bash special cases
   the interactive comment character to not be a comment delimiter. */
char history_comment_char = '\0';

/* The list of characters which inhibit the expansion of text if found
   immediately following history_expansion_char. */
char *history_no_expand_chars = " \t\n\r=";

/* The logical `base' of the history array.  It defaults to 1. */
int history_base = 1;

/* Return the current HISTORY_STATE of the history. */
HISTORY_STATE *
history_get_history_state ()
{
  HISTORY_STATE *state;

  state = (HISTORY_STATE *)xmalloc (sizeof (HISTORY_STATE));
  state->entries = the_history;
  state->offset = history_offset;
  state->length = history_length;
  state->size = history_size;

  return (state);
}

/* Set the state of the current history array to STATE. */
void
history_set_history_state (state)
     HISTORY_STATE *state;
{
  the_history = state->entries;
  history_offset = state->offset;
  history_length = state->length;
  history_size = state->size;
}

/* Begin a session in which the history functions might be used.  This
   initializes interactive variables. */
void
using_history ()
{
  history_offset = history_length;
}

/* Return the number of bytes that the primary history entries are using.
   This just adds up the lengths of the_history->lines. */
int
history_total_bytes ()
{
  register int i, result;

  result = 0;

  for (i = 0; the_history && the_history[i]; i++)
    result += strlen (the_history[i]->line);

  return (result);
}

/* Place STRING at the end of the history list.  The data field
   is  set to NULL. */
void
add_history (string)
     char *string;
{
  HIST_ENTRY *temp;

  if (history_stifled && (history_length == max_input_history))
    {
      register int i;

      /* If the history is stifled, and history_length is zero,
	 and it equals max_input_history, we don't save items. */
      if (history_length == 0)
	return;

      /* If there is something in the slot, then remove it. */
      if (the_history[0])
	{
	  free (the_history[0]->line);
	  free (the_history[0]);
	}

      /* Copy the rest of the entries, moving down one slot. */
      for (i = 0; i < history_length; i++)
	the_history[i] = the_history[i + 1];

      history_base++;

    }
  else
    {
      if (!history_size)
	{
	  history_size = DEFAULT_HISTORY_GROW_SIZE;
	  the_history = (HIST_ENTRY **)xmalloc (history_size * sizeof (HIST_ENTRY *));
	  history_length = 1;

	}
      else
	{
	  if (history_length == (history_size - 1))
	    {
	      history_size += DEFAULT_HISTORY_GROW_SIZE;
	      the_history = (HIST_ENTRY **)
		xrealloc (the_history, history_size * sizeof (HIST_ENTRY *));
	    }
	  history_length++;
	}
    }

  temp = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
  temp->line = savestring (string);
  temp->data = (char *)NULL;

  the_history[history_length] = (HIST_ENTRY *)NULL;
  the_history[history_length - 1] = temp;
}

/* Make the history entry at WHICH have LINE and DATA.  This returns
   the old entry so you can dispose of the data.  In the case of an
   invalid WHICH, a NULL pointer is returned. */
HIST_ENTRY *
replace_history_entry (which, line, data)
     int which;
     char *line;
     char *data;
{
  HIST_ENTRY *temp = (HIST_ENTRY *)xmalloc (sizeof (HIST_ENTRY));
  HIST_ENTRY *old_value;

  if (which >= history_length)
    return ((HIST_ENTRY *)NULL);

  old_value = the_history[which];

  temp->line = savestring (line);
  temp->data = data;
  the_history[which] = temp;

  return (old_value);
}

/* Returns the magic number which says what history element we are
   looking at now.  In this implementation, it returns history_offset. */
int
where_history ()
{
  return (history_offset);
}

/* Search the history for STRING, starting at history_offset.
   If DIRECTION < 0, then the search is through previous entries, else
   through subsequent.  If ANCHORED is non-zero, the string must
   appear at the beginning of a history line, otherwise, the string
   may appear anywhere in the line.  If the string is found, then
   current_history () is the history entry, and the value of this
   function is the offset in the line of that history entry that the
   string was found in.  Otherwise, nothing is changed, and a -1 is
   returned. */

#define ANCHORED_SEARCH 1
#define NON_ANCHORED_SEARCH 0

static int
history_search_internal (string, direction, anchored)
     char *string;
     int direction, anchored;
{
  register int i, reverse;
  register char *line;
  register int line_index;
  int string_len;

  i = history_offset;
  reverse = (direction < 0);

  /* Take care of trivial cases first. */

  if (!history_length || ((i == history_length) && !reverse))
    return (-1);

  if (reverse && (i == history_length))
    i--;

#define NEXT_LINE() do { if (reverse) i--; else i++; } while (0)

  string_len = strlen (string);
  while (1)
    {
      /* Search each line in the history list for STRING. */

      /* At limit for direction? */
      if ((reverse && i < 0) || (!reverse && i == history_length))
	return (-1);

      line = the_history[i]->line;
      line_index = strlen (line);

      /* If STRING is longer than line, no match. */
      if (string_len > line_index)
	{
	  NEXT_LINE ();
	  continue;
	}

      /* Handle anchored searches first. */
      if (anchored == ANCHORED_SEARCH)
	{
	  if (STREQN (string, line, string_len))
	    {
	      history_offset = i;
	      return (0);
	    }

	  NEXT_LINE ();
	  continue;
	}

      /* Do substring search. */
      if (reverse)
	{
	  line_index -= string_len;

	  while (line_index >= 0)
	    {
	      if (STREQN (string, line + line_index, string_len))
		{
		  history_offset = i;
		  return (line_index);
		}
	      line_index--;
	    }
	}
      else
	{
	  register int limit = line_index - string_len + 1;
	  line_index = 0;

	  while (line_index < limit)
	    {
	      if (STREQN (string, line + line_index, string_len))
		{
		  history_offset = i;
		  return (line_index);
		}
	      line_index++;
	    }
	}
      NEXT_LINE ();
    }
}

/* Do a non-anchored search for STRING through the history in DIRECTION. */
int
history_search (string, direction)
     char *string;
     int direction;
{
  return (history_search_internal (string, direction, NON_ANCHORED_SEARCH));
}

/* Do an anchored search for string through the history in DIRECTION. */
int
history_search_prefix (string, direction)
     char *string;
     int direction;
{
  return (history_search_internal (string, direction, ANCHORED_SEARCH));
}

/* Remove history element WHICH from the history.  The removed
   element is returned to you so you can free the line, data,
   and containing structure. */
HIST_ENTRY *
remove_history (which)
     int which;
{
  HIST_ENTRY *return_value;

  if (which >= history_length || !history_length)
    return_value = (HIST_ENTRY *)NULL;
  else
    {
      register int i;
      return_value = the_history[which];

      for (i = which; i < history_length; i++)
	the_history[i] = the_history[i + 1];

      history_length--;
    }

  return (return_value);
}

/* Stifle the history list, remembering only MAX number of lines. */
void
stifle_history (max)
     int max;
{
  if (max < 0)
    max = 0;

  if (history_length > max)
    {
      register int i, j;

      /* This loses because we cannot free the data. */
      for (i = 0; i < (history_length - max); i++)
	{
	  free (the_history[i]->line);
	  free (the_history[i]);
	}

      history_base = i;
      for (j = 0, i = history_length - max; j < max; i++, j++)
	the_history[j] = the_history[i];
      the_history[j] = (HIST_ENTRY *)NULL;
      history_length = j;
    }

  history_stifled = 1;
  max_input_history = max;
}

/* Stop stifling the history.  This returns the previous amount the history
 was stifled by.  The value is positive if the history was stifled, negative
 if it wasn't. */
int
unstifle_history ()
{
  int result = max_input_history;

  if (history_stifled)
    {
      result = -result;
      history_stifled = 0;
    }

  return (result);
}

/* Return the string that should be used in the place of this
   filename.  This only matters when you don't specify the
   filename to read_history (), or write_history (). */
static char *
history_filename (filename)
     char *filename;
{
  char *return_val = filename ? savestring (filename) : (char *)NULL;

  if (!return_val)
    {
      char *home;
      int home_len;

      home = getenv ("HOME");

      if (!home)
	home = ".";

      home_len = strlen (home);
      /* strlen(".history") == 8 */
      return_val = xmalloc (2 + home_len + 8);

      strcpy (return_val, home);
      return_val[home_len] = '/';
      strcpy (return_val + home_len + 1, ".history");
    }

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

      if (input)
	free (input);

      if (buffer)
	free (buffer);

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

  if (input)
    free (input);

  if (buffer)
    free (buffer);

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
  if (i && ((file = open (filename, O_WRONLY, 0666)) != -1))
    {
      write (file, buffer + i, finfo.st_size - i);
      close (file);
    }

 truncate_exit:
  if (buffer)
    free (buffer);

  free (filename);
  return 0;
}

#define HISTORY_APPEND 0
#define HISTORY_OVERWRITE 1

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
      if (output)
	free (output);

      return (errno);
    }

  if (nelements > history_length)
    nelements = history_length;

  /* Build a buffer of all the lines to write, and write them in one syscall.
     Suggested by Peter Ho (peter@robosts.oxford.ac.uk). */
  {
    register int j = 0;
    int buffer_size = 0;
    char *buffer;

    /* Calculate the total number of bytes to write. */
    for (i = history_length - nelements; i < history_length; i++)
      buffer_size += 1 + strlen (the_history[i]->line);

    /* Allocate the buffer, and fill it. */
    buffer = xmalloc (buffer_size);

    for (i = history_length - nelements; i < history_length; i++)
      {
	strcpy (buffer + j, the_history[i]->line);
	j += strlen (the_history[i]->line);
	buffer[j++] = '\n';
      }

    write (file, buffer, buffer_size);
    free (buffer);
  }

  close (file);

  if (output)
    free (output);

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

/* Return the history entry at the current position, as determined by
   history_offset.  If there is no entry there, return a NULL pointer. */
HIST_ENTRY *
current_history ()
{
  if ((history_offset == history_length) || !the_history)
    return ((HIST_ENTRY *)NULL);
  else
    return (the_history[history_offset]);
}

/* Back up history_offset to the previous history entry, and return
   a pointer to that entry.  If there is no previous entry then return
   a NULL pointer. */
HIST_ENTRY *
previous_history ()
{
  if (!history_offset)
    return ((HIST_ENTRY *)NULL);
  else
    return (the_history[--history_offset]);
}

/* Move history_offset forward to the next history entry, and return
   a pointer to that entry.  If there is no next entry then return a
   NULL pointer. */
HIST_ENTRY *
next_history ()
{
  if (history_offset == history_length)
    return ((HIST_ENTRY *)NULL);
  else
    return (the_history[++history_offset]);
}

/* Return the current history array.  The caller has to be carefull, since this
   is the actual array of data, and could be bashed or made corrupt easily.
   The array is terminated with a NULL pointer. */
HIST_ENTRY **
history_list ()
{
  return (the_history);
}

/* Return the history entry which is logically at OFFSET in the history array.
   OFFSET is relative to history_base. */
HIST_ENTRY *
history_get (offset)
     int offset;
{
  int local_index = offset - history_base;

  if (local_index >= history_length ||
      local_index < 0 ||
      !the_history)
    return ((HIST_ENTRY *)NULL);
  return (the_history[local_index]);
}

/* Search for STRING in the history list.  DIR is < 0 for searching
   backwards.  POS is an absolute index into the history list at
   which point to begin searching. */
int
history_search_pos (string, dir, pos)
     char *string;
     int dir, pos;
{
  int ret, old = where_history ();
  history_set_pos (pos);
  if (history_search (string, dir) == -1)
    {
      history_set_pos (old);
      return (-1);
    }
  ret = where_history ();
  history_set_pos (old);
  return ret;
}

/* Make the current history item be the one at POS, an absolute index.
   Returns zero if POS is out of range, else non-zero. */
int
history_set_pos (pos)
     int pos;
{
  if (pos > history_length || pos < 0 || !the_history)
    return (0);
  history_offset = pos;
  return (1);
}
 

/* **************************************************************** */
/*								    */
/*			History Expansion			    */
/*								    */
/* **************************************************************** */

/* Hairy history expansion on text, not tokens.  This is of general
   use, and thus belongs in this library. */

/* The last string searched for in a !?string? search. */
static char *search_string = (char *)NULL;

/* Return the event specified at TEXT + OFFSET modifying OFFSET to
   point to after the event specifier.  Just a pointer to the history
   line is returned; NULL is returned in the event of a bad specifier.
   You pass STRING with *INDEX equal to the history_expansion_char that
   begins this specification.
   DELIMITING_QUOTE is a character that is allowed to end the string
   specification for what to search for in addition to the normal
   characters `:', ` ', `\t', `\n', and sometimes `?'.
   So you might call this function like:
   line = get_history_event ("!echo:p", &index, 0);  */
char *
get_history_event (string, caller_index, delimiting_quote)
     char *string;
     int *caller_index;
     int delimiting_quote;
{
  register int i = *caller_index;
  register char c;
  HIST_ENTRY *entry;
  int which, sign = 1;
  int local_index, search_mode, substring_okay = 0;
  char *temp;

  /* The event can be specified in a number of ways.

     !!   the previous command
     !n   command line N
     !-n  current command-line minus N
     !str the most recent command starting with STR
     !?str[?]
	  the most recent command containing STR

     All values N are determined via HISTORY_BASE. */

  if (string[i] != history_expansion_char)
    return ((char *)NULL);

  /* Move on to the specification. */
  i++;

#define RETURN_ENTRY(e, w) \
	return ((e = history_get (w)) ? e->line : (char *)NULL)

  /* Handle !! case. */
  if (string[i] == history_expansion_char)
    {
      i++;
      which = history_base + (history_length - 1);
      *caller_index = i;
      RETURN_ENTRY (entry, which);
    }

  /* Hack case of numeric line specification. */
  if (string[i] == '-')
    {
      sign = -1;
      i++;
    }

  if (digit (string[i]))
    {
      /* Get the extent of the digits and compute the value. */
      for (which = 0; digit (string[i]); i++)
	which = (which * 10) + digit_value (string[i]);

      *caller_index = i;

      if (sign < 0)
	which = (history_length + history_base) - which;

      RETURN_ENTRY (entry, which);
    }

  /* This must be something to search for.  If the spec begins with
     a '?', then the string may be anywhere on the line.  Otherwise,
     the string must be found at the start of a line. */
  if (string[i] == '?')
    {
      substring_okay++;
      i++;
    }

  /* Only a closing `?' or a newline delimit a substring search string. */
  for (local_index = i; c = string[i]; i++)
    if ((!substring_okay && (whitespace (c) || c == ':' ||
#if defined (SHELL)
	  member (c, ";&()|<>") ||
#endif /* SHELL */
	  string[i] == delimiting_quote)) ||
	string[i] == '\n' ||
	(substring_okay && string[i] == '?'))
      break;

  temp = xmalloc (1 + (i - local_index));
  strncpy (temp, &string[local_index], (i - local_index));
  temp[i - local_index] = '\0';

  if (substring_okay && string[i] == '?')
    i++;

  *caller_index = i;

#define FAIL_SEARCH() \
  do { history_offset = history_length; free (temp) ; return (char *)NULL; } while (0)

  search_mode = substring_okay ? NON_ANCHORED_SEARCH : ANCHORED_SEARCH;
  while (1)
    {
      local_index = history_search_internal (temp, -1, search_mode);

      if (local_index < 0)
	FAIL_SEARCH ();

      if (local_index == 0 || substring_okay)
	{
	  entry = current_history ();
	  history_offset = history_length;
	
	  /* If this was a substring search, then remember the
	     string that we matched for word substitution. */
	  if (substring_okay)
	    {
	      if (search_string)
		free (search_string);
	      search_string = temp;
	    }
	  else
	    free (temp);
	  return (entry->line);
	}

      if (history_offset)
	history_offset--;
      else
	FAIL_SEARCH ();
    }
#undef FAIL_SEARCH
#undef RETURN_ENTRY
}
#if defined (SHELL)
/* Function for extracting single-quoted strings.  Used for inhibiting
   history expansion within single quotes. */

/* Extract the contents of STRING as if it is enclosed in single quotes.
   SINDEX, when passed in, is the offset of the character immediately
   following the opening single quote; on exit, SINDEX is left pointing
   to the closing single quote. */
static void
rl_string_extract_single_quoted (string, sindex)
     char *string;
     int *sindex;
{
  register int i = *sindex;

  while (string[i] && string[i] != '\'')
    i++;

  *sindex = i;
}

static char *
quote_breaks (s)
     char *s;
{
  register char *p, *r;
  char *ret;
  int len = 3;

  for (p = s; p && *p; p++, len++)
    {
      if (*p == '\'')
	len += 3;
      else if (whitespace (*p) || *p == '\n')
	len += 2;
    }

  r = ret = xmalloc (len);
  *r++ = '\'';
  for (p = s; p && *p; )
    {
      if (*p == '\'')
	{
	  *r++ = '\'';
	  *r++ = '\\';
	  *r++ = '\'';
	  *r++ = '\'';
	  p++;
	}
      else if (whitespace (*p) || *p == '\n')
	{
	  *r++ = '\'';
	  *r++ = *p++;
	  *r++ = '\'';
	}
      else
	*r++ = *p++;
    }
  *r++ = '\'';
  *r = '\0';
  return ret;
}
#endif /* SHELL */

static char *
hist_error (ret, s, start, current, errtype)
      char *ret, *s;
      int start, current, errtype;
{
  char *temp, *emsg;
  int ll;

  ll = 1 + (current - start);
  temp = xmalloc (1 + ll);
  strncpy (temp, s + start, ll);
  temp[ll] = 0;

  switch (errtype)
    {
    case EVENT_NOT_FOUND:
      emsg = "event not found";
      break;
    case BAD_WORD_SPEC:
      emsg = "bad word specifier";
      break;
    case SUBST_FAILED:
      emsg = "substitution failed";
      break;
    case BAD_MODIFIER:
      emsg = "unrecognized history modifier";
      break;
    default:
      emsg = "unknown expansion error";
      break;
    }

  sprintf (ret, "%s: %s", temp, emsg);
  free (temp);
  return ret;
}

/* Get a history substitution string from STR starting at *IPTR
   and return it.  The length is returned in LENPTR.

   A backslash can quote the delimiter.  If the string is the
   empty string, the previous pattern is used.  If there is
   no previous pattern for the lhs, the last history search
   string is used.

   If IS_RHS is 1, we ignore empty strings and set the pattern
   to "" anyway.  subst_lhs is not changed if the lhs is empty;
   subst_rhs is allowed to be set to the empty string. */

static char *
get_subst_pattern (str, iptr, delimiter, is_rhs, lenptr)
     char *str;
     int *iptr, delimiter, is_rhs, *lenptr;
{
  register int si, i, j, k;
  char *s = (char *) NULL;

  i = *iptr;

  for (si = i; str[si] && str[si] != delimiter; si++)
    if (str[si] == '\\' && str[si + 1] == delimiter)
      si++;

  if (si > i || is_rhs)
    {
      s = xmalloc (si - i + 1);
      for (j = 0, k = i; k < si; j++, k++)
	{
	  /* Remove a backslash quoting the search string delimiter. */
	  if (str[k] == '\\' && str[k + 1] == delimiter)
	    k++;
	  s[j] = str[k];
	}
      s[j] = '\0';
      if (lenptr)
        *lenptr = j;
    }

  i = si;
  if (str[i])
    i++;
  *iptr = i;

  return s;
}

static void
postproc_subst_rhs ()
{
  char *new;
  int i, j, new_size;

  new = xmalloc (new_size = subst_rhs_len + subst_lhs_len);
  for (i = j = 0; i < subst_rhs_len; i++)
    {
      if (subst_rhs[i] == '&')
	{
	  if (j + subst_lhs_len >= new_size)
	    new = xrealloc (new, (new_size = new_size * 2 + subst_lhs_len));
	  strcpy (new + j, subst_lhs);
	  j += subst_lhs_len;
	}
      else
	{
	  /* a single backslash protects the `&' from lhs interpolation */
	  if (subst_rhs[i] == '\\' && subst_rhs[i + 1] == '&')
	    i++;
	  if (j >= new_size)
	    new = xrealloc (new, new_size *= 2);
	  new[j++] = subst_rhs[i];
	}
    }
  new[j] = '\0';
  free (subst_rhs);
  subst_rhs = new;
  subst_rhs_len = j;
}

/* Expand the bulk of a history specifier starting at STRING[START].
   Returns 0 if everything is OK, -1 if an error occurred, and 1
   if the `p' modifier was supplied and the caller should just print
   the returned string.  Returns the new index into string in
   *END_INDEX_PTR, and the expanded specifier in *RET_STRING. */
static int
history_expand_internal (string, start, end_index_ptr, ret_string, current_line)
     char *string;
     int start, *end_index_ptr;
     char **ret_string;
     char *current_line;	/* for !# */
{
  int i, n, starting_index;
  int substitute_globally, want_quotes, print_only;
  char *event, *temp, *result, *tstr, *t, c, *word_spec;
  int result_len;

  result = xmalloc (result_len = 128);

  i = start;

  /* If it is followed by something that starts a word specifier,
     then !! is implied as the event specifier. */

  if (member (string[i + 1], ":$*%^"))
    {
      char fake_s[3];
      int fake_i = 0;
      i++;
      fake_s[0] = fake_s[1] = history_expansion_char;
      fake_s[2] = '\0';
      event = get_history_event (fake_s, &fake_i, 0);
    }
  else if (string[i + 1] == '#')
    {
      i += 2;
      event = current_line;
    }
  else
    {
      int quoted_search_delimiter = 0;

      /* If the character before this `!' is a double or single
	 quote, then this expansion takes place inside of the
	 quoted string.  If we have to search for some text ("!foo"),
	 allow the delimiter to end the search string. */
      if (i && (string[i - 1] == '\'' || string[i - 1] == '"'))
	quoted_search_delimiter = string[i - 1];
      event = get_history_event (string, &i, quoted_search_delimiter);
    }
	  
  if (!event)
    {
      hist_error (result, string, start, i, EVENT_NOT_FOUND);
      *ret_string = result;
      return (-1);
    }

  /* If a word specifier is found, then do what that requires. */
  starting_index = i;
  word_spec = get_history_word_specifier (string, event, &i);

  /* There is no such thing as a `malformed word specifier'.  However,
     it is possible for a specifier that has no match.  In that case,
     we complain. */
  if (word_spec == (char *)&error_pointer)
    {
      hist_error (result, string, starting_index, i, BAD_WORD_SPEC);
      *ret_string = result;
      return (-1);
    }

  /* If no word specifier, than the thing of interest was the event. */
  if (!word_spec)
    temp = savestring (event);
  else
    {
      temp = savestring (word_spec);
      free (word_spec);
    }

  /* Perhaps there are other modifiers involved.  Do what they say. */
  want_quotes = substitute_globally = print_only = 0;
  starting_index = i;

  while (string[i] == ':')
    {
      c = string[i + 1];

      if (c == 'g')
	{
	  substitute_globally = 1;
	  i++;
	  c = string[i + 1];
	}

      switch (c)
	{
	default:
	  hist_error (result, string, i+1, i+2, BAD_MODIFIER);
	  *ret_string = result;
	  free (temp);
	  return -1;

#if defined (SHELL)
	case 'q':
	  want_quotes = 'q';
	  break;

	case 'x':
	  want_quotes = 'x';
	  break;
#endif /* SHELL */

	  /* :p means make this the last executed line.  So we
	     return an error state after adding this line to the
	     history. */
	case 'p':
	  print_only++;
	  break;

	  /* :t discards all but the last part of the pathname. */
	case 't':
	  tstr = strrchr (temp, '/');
	  if (tstr)
	    {
	      tstr++;
	      t = savestring (tstr);
	      free (temp);
	      temp = t;
	    }
	  break;

	  /* :h discards the last part of a pathname. */
	case 'h':
	  tstr = strrchr (temp, '/');
	  if (tstr)
	    *tstr = '\0';
	  break;

	  /* :r discards the suffix. */
	case 'r':
	  tstr = strrchr (temp, '.');
	  if (tstr)
	    *tstr = '\0';
	  break;

	  /* :e discards everything but the suffix. */
	case 'e':
	  tstr = strrchr (temp, '.');
	  if (tstr)
	    {
	      t = savestring (tstr);
	      free (temp);
	      temp = t;
	    }
	  break;

	/* :s/this/that substitutes `that' for the first
	   occurrence of `this'.  :gs/this/that substitutes `that'
	   for each occurrence of `this'.  :& repeats the last
	   substitution.  :g& repeats the last substitution
	   globally. */

	case '&':
	case 's':
	  {
	    char *new_event, *t;
	    int delimiter, failed, si, l_temp;

	    if (c == 's')
	      {
		if (i + 2 < (int)strlen (string))
		  delimiter = string[i + 2];
		else
		  break;	/* no search delimiter */

		i += 3;

		t = get_subst_pattern (string, &i, delimiter, 0, &subst_lhs_len);
		/* An empty substitution lhs with no previous substitution
		   uses the last search string as the lhs. */
		if (t)
		  {
		    if (subst_lhs)
		      free (subst_lhs);
		    subst_lhs = t;
		  }
		else if (!subst_lhs)
		  {
		    if (search_string && *search_string)
		      {
			subst_lhs = savestring (search_string);
			subst_lhs_len = strlen (subst_lhs);
		      }
		    else
		      {
		        subst_lhs = (char *) NULL;
		        subst_lhs_len = 0;
		      }
		  }

		/* If there is no lhs, the substitution can't succeed. */
		if (subst_lhs_len == 0)
		  {
		    hist_error (result, string, starting_index, i, SUBST_FAILED);
		    *ret_string = result;
		    free (temp);
		    return -1;
		  }

		if (subst_rhs)
		  free (subst_rhs);
		subst_rhs = get_subst_pattern (string, &i, delimiter, 1, &subst_rhs_len);

		/* If `&' appears in the rhs, it's supposed to be replaced
		   with the lhs. */
		if (member ('&', subst_rhs))
		  postproc_subst_rhs ();
	      }
	    else
	      i += 2;

	    l_temp = strlen (temp);
	    /* Ignore impossible cases. */
	    if (subst_lhs_len > l_temp)
	      {
		hist_error (result, string, starting_index, i, SUBST_FAILED);
		*ret_string = result;
		free (temp);
		return (-1);
	      }

	    /* Find the first occurrence of THIS in TEMP. */
	    si = 0;
	    for (failed = 1; (si + subst_lhs_len) <= l_temp; si++)
	      if (STREQN (temp+si, subst_lhs, subst_lhs_len))
		{
		  int len = subst_rhs_len - subst_lhs_len + l_temp;
		  new_event = xmalloc (1 + len);
		  strncpy (new_event, temp, si);
		  strncpy (new_event + si, subst_rhs, subst_rhs_len);
		  strncpy (new_event + si + subst_rhs_len,
			   temp + si + subst_lhs_len,
			   l_temp - (si + subst_lhs_len));
		  new_event[len] = '\0';
		  free (temp);
		  temp = new_event;

		  failed = 0;

		  if (substitute_globally)
		    {
		      si += subst_rhs_len;
		      l_temp = strlen (temp);
		      substitute_globally++;
		      continue;
		    }
		  else
		    break;
		}

	    if (substitute_globally > 1)
	      {
		substitute_globally = 0;
		continue;	/* don't want to increment i */
	      }

	    if (failed == 0)
	      continue;		/* don't want to increment i */

	    hist_error (result, string, starting_index, i, SUBST_FAILED);
	    *ret_string = result;
	    free (temp);
	    return (-1);
	  }
	}
      i += 2;
    }
  /* Done with modfiers. */
  /* Believe it or not, we have to back the pointer up by one. */
  --i;

#if defined (SHELL)
  if (want_quotes)
    {
      char *x;

      if (want_quotes == 'q')
	x = single_quote (temp);
      else if (want_quotes == 'x')
	x = quote_breaks (temp);
      else
	x = savestring (temp);

      free (temp);
      temp = x;
    }
#endif /* SHELL */

  n = strlen (temp);
  if (n > result_len)
    result = xrealloc (result, n + 1);
  strcpy (result, temp);
  free (temp);

  *end_index_ptr = i;
  *ret_string = result;
  return (print_only);
}

/* Expand the string STRING, placing the result into OUTPUT, a pointer
   to a string.  Returns:

  -1) If there was an error in expansion.
   0) If no expansions took place (or, if the only change in
      the text was the de-slashifying of the history expansion
      character)
   1) If expansions did take place
   2) If the `p' modifier was given and the caller should print the result

  If an error ocurred in expansion, then OUTPUT contains a descriptive
  error message. */

#define ADD_STRING(s) \
	do \
	  { \
	    int sl = strlen (s); \
	    j += sl; \
	    while (j >= result_len) \
	      result = xrealloc (result, result_len += 128); \
	    strcpy (result + j - sl, s); \
	  } \
	while (0)

#define ADD_CHAR(c) \
	do \
	  { \
	    if (j >= result_len) \
	      result = xrealloc (result, result_len += 64); \
	    result[j++] = c; \
	    result[j] = '\0'; \
	  } \
	while (0)

int
history_expand (hstring, output)
     char *hstring;
     char **output;
{
  register int j;
  int i, r, l, passc, cc, modified, eindex, only_printing;
  char *string;

  /* The output string, and its length. */
  int result_len;
  char *result;

  /* Used when adding the string. */
  char *temp;

  /* Prepare the buffer for printing error messages. */
  result = xmalloc (result_len = 256);
  result[0] = '\0';

  only_printing = modified = 0;
  l = strlen (hstring);

  /* Grovel the string.  Only backslash can quote the history escape
     character.  We also handle arg specifiers. */

  /* Before we grovel forever, see if the history_expansion_char appears
     anywhere within the text. */

  /* The quick substitution character is a history expansion all right.  That
     is to say, "^this^that^" is equivalent to "!!:s^this^that^", and in fact,
     that is the substitution that we do. */
  if (hstring[0] == history_subst_char)
    {
      string = xmalloc (l + 5);

      string[0] = string[1] = history_expansion_char;
      string[2] = ':';
      string[3] = 's';
      strcpy (string + 4, hstring);
      l += 4;
    }
  else
    {
      string = hstring;
      /* If not quick substitution, still maybe have to do expansion. */

      /* `!' followed by one of the characters in history_no_expand_chars
	 is NOT an expansion. */
      for (i = 0; string[i]; i++)
	{
	  cc = string[i + 1];
          if (string[i] == history_expansion_char)
	    {
	      if (!cc || member (cc, history_no_expand_chars))
		continue;
#if defined (SHELL)
	      /* The shell uses ! as a pattern negation character
	         in globbing [...] expressions, so let those pass
	         without expansion. */
	      else if (i > 0 && (string[i - 1] == '[') &&
		       member (']', string + i + 1))
		continue;
#endif /* SHELL */
	      else
		break;
	    }
#if defined (SHELL)
	  else if (string[i] == '\'')
	    {
	      /* If this is bash, single quotes inhibit history expansion. */
	      i++;
	      rl_string_extract_single_quoted (string, &i);
	    }
	  else if (string[i] == '\\')
	    {
	      /* If this is bash, allow backslashes to quote single
		 quotes and
		 the history expansion character. */
	      if (cc == '\'' || cc == history_expansion_char)
		i++;
	    }
#endif /* SHELL */
	}
	  
      if (string[i] != history_expansion_char)
	{
	  free (result);
	  *output = savestring (string);
	  return (0);
	}
    }

  /* Extract and perform the substitution. */
  for (passc = i = j = 0; i < l; i++)
    {
      int tchar = string[i];

      if (passc)
	{
	  passc = 0;
	  ADD_CHAR (tchar);
	  continue;
	}

      if (tchar == history_expansion_char)
	tchar = -3;

      switch (tchar)
	{
	default:
	  ADD_CHAR (string[i]);
	  break;

	case '\\':
	  passc++;
	  ADD_CHAR (tchar);
	  break;

#if defined (SHELL)
	case '\'':
	  {
	    /* If this is bash, single quotes inhibit history expansion. */
	    int quote, slen;

	    quote = i++;
	    rl_string_extract_single_quoted (string, &i);

	    slen = i - quote + 2;
	    temp = xmalloc (slen);
	    strncpy (temp, string + quote, slen);
	    temp[slen - 1] = '\0';
	    ADD_STRING (temp);
	    free (temp);
	    break;
	  }
#endif /* SHELL */

	case -3:		/* history_expansion_char */
	  cc = string[i + 1];

	  /* If the history_expansion_char is followed by one of the
	     characters in history_no_expand_chars, then it is not a
	     candidate for expansion of any kind. */
	  if (member (cc, history_no_expand_chars))
	    {
	      ADD_CHAR (string[i]);
	      break;
	    }

#ifdef NO_BANG_HASH_MODIFIERS
	  /* There is something that is listed as a `word specifier' in csh
	     documentation which means `the expanded text to this point'.
	     That is not a word specifier, it is an event specifier.  If we
	     don't want to allow modifiers with `!#', just stick the current
	     output line in again. */
	  if (cc == '#')
	    {
	      if (result)
		{
		  temp = xmalloc (1 + strlen (result));
		  strcpy (temp, result);
		  ADD_STRING (temp);
		  free (temp);
		}
	      i++;
	      break;
	    }
#endif

	  r = history_expand_internal (string, i, &eindex, &temp, result);
	  if (r < 0)
	    {
	      *output = temp;
	      free (result);
	      if (string != hstring)
		free (string);
	      return -1;
	    }
	  else
	    {
	      if (temp)
		{
		  modified++;
		  if (*temp)
		    ADD_STRING (temp);
		  free (temp);
		}
	      only_printing = r == 1;
	      i = eindex;
	    }
	  break;
	}
    }

  *output = result;
  if (string != hstring)
    free (string);

  if (only_printing)
    {
      add_history (result);
      return (2);
    }

  return (modified != 0);
}

/* Return a consed string which is the word specified in SPEC, and found
   in FROM.  NULL is returned if there is no spec.  The address of
   ERROR_POINTER is returned if the word specified cannot be found.
   CALLER_INDEX is the offset in SPEC to start looking; it is updated
   to point to just after the last character parsed. */
static char *
get_history_word_specifier (spec, from, caller_index)
     char *spec, *from;
     int *caller_index;
{
  register int i = *caller_index;
  int first, last;
  int expecting_word_spec = 0;
  char *result;

  /* The range of words to return doesn't exist yet. */
  first = last = 0;
  result = (char *)NULL;

  /* If we found a colon, then this *must* be a word specification.  If
     it isn't, then it is an error. */
  if (spec[i] == ':')
    {
      i++;
      expecting_word_spec++;
    }

  /* Handle special cases first. */

  /* `%' is the word last searched for. */
  if (spec[i] == '%')
    {
      *caller_index = i + 1;
      return (search_string ? savestring (search_string) : savestring (""));
    }

  /* `*' matches all of the arguments, but not the command. */
  if (spec[i] == '*')
    {
      *caller_index = i + 1;
      result = history_arg_extract (1, '$', from);
      return (result ? result : savestring (""));
    }

  /* `$' is last arg. */
  if (spec[i] == '$')
    {
      *caller_index = i + 1;
      return (history_arg_extract ('$', '$', from));
    }

  /* Try to get FIRST and LAST figured out. */

  if (spec[i] == '-' || spec[i] == '^')
    first = 1;
  else if (digit (spec[i]) && expecting_word_spec)
    {
      for (first = 0; digit (spec[i]); i++)
	first = (first * 10) + digit_value (spec[i]);
    }
  else
    return ((char *)NULL);	/* no valid `first' for word specifier */

  if (spec[i] == '^' || spec[i] == '*')
    {
      last = (spec[i] == '^') ? 1 : '$';	/* x* abbreviates x-$ */
      i++;
    }
  else if (spec[i] != '-')
    last = first;
  else
    {
      i++;

      if (digit (spec[i]))
	{
	  for (last = 0; digit (spec[i]); i++)
	    last = (last * 10) + digit_value (spec[i]);
	}
      else if (spec[i] == '$')
	{
	  i++;
	  last = '$';
	}
      else if (!spec[i] || spec[i] == ':')  /* could be modifier separator */
	last = -1;		/* x- abbreviates x-$ omitting word `$' */
    }

  *caller_index = i;

  if (last >= first || last == '$' || last < 0)
    result = history_arg_extract (first, last, from);

  return (result ? result : (char *)&error_pointer);
}

/* Extract the args specified, starting at FIRST, and ending at LAST.
   The args are taken from STRING.  If either FIRST or LAST is < 0,
   then make that arg count from the right (subtract from the number of
   tokens, so that FIRST = -1 means the next to last token on the line).
   If LAST is `$' the last arg from STRING is used. */
char *
history_arg_extract (first, last, string)
     int first, last;
     char *string;
{
  register int i, len;
  char *result = (char *)NULL;
  int size = 0, offset = 0;
  char **list;

  /* XXX - think about making history_tokenize return a struct array,
     each struct in array being a string and a length to avoid the
     calls to strlen below. */
  if ((list = history_tokenize (string)) == NULL)
    return ((char *)NULL);

  for (len = 0; list[len]; len++)
    ;

  if (last < 0)
    last = len + last - 1;

  if (first < 0)
    first = len + first - 1;

  if (last == '$')
    last = len - 1;

  if (first == '$')
    first = len - 1;

  last++;

  if (first > len || last > len || first < 0 || last < 0)
    result = ((char *)NULL);
  else
    {
      for (size = 0, i = first; i < last; i++)
	size += strlen (list[i]) + 1;
      result = xmalloc (size + 1);

      for (i = first; i < last; i++)
	{
	  strcpy (result + offset, list[i]);
	  offset += strlen (list[i]);
	  if (i + 1 < last)
	    {
      	      result[offset++] = ' ';
	      result[offset] = 0;
	    }
	}
    }

  for (i = 0; i < len; i++)
    free (list[i]);
  free (list);

  return (result);
}

#define slashify_in_quotes "\\`\"$"

/* Return an array of tokens, much as the shell might.  The tokens are
   parsed out of STRING. */
char **
history_tokenize (string)
     char *string;
{
  char **result = (char **)NULL;
  register int i, start, result_index, size;
  int len;

  i = result_index = size = 0;

  /* Get a token, and stuff it into RESULT.  The tokens are split
     exactly where the shell would split them. */
  while (string[i])
    {
      int delimiter = 0;

      /* Skip leading whitespace. */
      for (; string[i] && whitespace (string[i]); i++)
	;
      if (!string[i] || string[i] == history_comment_char)
	return (result);

      start = i;
      
      if (member (string[i], "()\n"))
	{
	  i++;
	  goto got_token;
	}

      if (member (string[i], "<>;&|$"))
	{
	  int peek = string[i + 1];

	  if (peek == string[i] && peek != '$')
	    {
	      if (peek == '<' && string[i + 2] == '-')
		i++;
	      i += 2;
	      goto got_token;
	    }
	  else
	    {
	      if ((peek == '&' && (string[i] == '>' || string[i] == '<')) ||
		  ((peek == '>') && (string[i] == '&')) ||
		  ((peek == '(') && (string[i] == '$')))
		{
		  i += 2;
		  goto got_token;
		}
	    }
	  if (string[i] != '$')
	    {
	      i++;
	      goto got_token;
	    }
	}

      /* Get word from string + i; */

      if (member (string[i], "\"'`"))
	delimiter = string[i++];

      for (; string[i]; i++)
	{
	  if (string[i] == '\\' && string[i + 1] == '\n')
	    {
	      i++;
	      continue;
	    }

	  if (string[i] == '\\' && delimiter != '\'' &&
	      (delimiter != '"' || member (string[i], slashify_in_quotes)))
	    {
	      i++;
	      continue;
	    }

	  if (delimiter && string[i] == delimiter)
	    {
	      delimiter = 0;
	      continue;
	    }

	  if (!delimiter && (member (string[i], " \t\n;&()|<>")))
	    break;

	  if (!delimiter && member (string[i], "\"'`"))
	    delimiter = string[i];
	}
    got_token:

      len = i - start;
      if (result_index + 2 >= size)
	result = (char **)xrealloc (result, ((size += 10) * sizeof (char *)));
      result[result_index] = xmalloc (1 + len);
      strncpy (result[result_index], string + start, len);
      result[result_index][len] = '\0';
      result[++result_index] = (char *)NULL;
    }

  return (result);
}

#if defined (STATIC_MALLOC)

/* **************************************************************** */
/*								    */
/*			xmalloc and xrealloc ()		     	    */
/*								    */
/* **************************************************************** */

static void memory_error_and_abort ();

static char *
xmalloc (bytes)
     int bytes;
{
  char *temp = (char *)malloc (bytes);

  if (!temp)
    memory_error_and_abort ();
  return (temp);
}

static char *
xrealloc (pointer, bytes)
     char *pointer;
     int bytes;
{
  char *temp;

  if (!pointer)
    temp = (char *)xmalloc (bytes);
  else
    temp = (char *)realloc (pointer, bytes);

  if (!temp)
    memory_error_and_abort ();

  return (temp);
}

static void
memory_error_and_abort ()
{
  fprintf (stderr, "history: Out of virtual memory!\n");
  abort ();
}
#endif /* STATIC_MALLOC */

/* **************************************************************** */
/*								    */
/*				Test Code			    */
/*								    */
/* **************************************************************** */
#ifdef TEST
main ()
{
  char line[1024], *t;
  int done = 0;

  line[0] = 0;

  while (!done)
    {
      fprintf (stdout, "history%% ");
      t = gets (line);

      if (!t)
	strcpy (line, "quit");

      if (line[0])
	{
	  char *expansion;
	  int result;

	  using_history ();

	  result = history_expand (line, &expansion);
	  strcpy (line, expansion);
	  free (expansion);
	  if (result)
	    fprintf (stderr, "%s\n", line);

	  if (result < 0)
	    continue;

	  add_history (line);
	}

      if (strcmp (line, "quit") == 0) done = 1;
      if (strcmp (line, "save") == 0) write_history (0);
      if (strcmp (line, "read") == 0) read_history (0);
      if (strcmp (line, "list") == 0)
	{
	  register HIST_ENTRY **the_list = history_list ();
	  register int i;

	  if (the_list)
	    for (i = 0; the_list[i]; i++)
	      fprintf (stdout, "%d: %s\n", i + history_base, the_list[i]->line);
	}
      if (strncmp (line, "delete", strlen ("delete")) == 0)
	{
	  int which;
	  if ((sscanf (line + strlen ("delete"), "%d", &which)) == 1)
	    {
	      HIST_ENTRY *entry = remove_history (which);
	      if (!entry)
		fprintf (stderr, "No such entry %d\n", which);
	      else
		{
		  free (entry->line);
		  free (entry);
		}
	    }
	  else
	    {
	      fprintf (stderr, "non-numeric arg given to `delete'\n");
	    }
	}
    }
}

#endif /* TEST */

/*
* Local variables:
* compile-command: "gcc -g -DTEST -o history history.c"
* end:
*/
