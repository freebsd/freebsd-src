/* History.c -- standalone history library */

/* Copyright (C) 1989 Free Software Foundation, Inc.

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

/* Remove these declarations when we have a complete libgnu.a. */
#define STATIC_MALLOC
#ifndef STATIC_MALLOC
extern char *xmalloc (), *xrealloc ();
#else
static char *xmalloc (), *xrealloc ();
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef __GNUC__
#define alloca __builtin_alloca
#else
#if defined (sparc) && defined (sun)
#include <alloca.h>
#else
extern char *alloca ();
#endif
#endif

#include "history.h"

#ifndef savestring
#define savestring(x) (char *)strcpy (xmalloc (1 + strlen (x)), (x))
#endif

#ifndef whitespace
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))
#endif

#ifndef digit
#define digit(c)  ((c) >= '0' && (c) <= '9')
#endif

#ifndef member
#define member(c, s) ((c) ? index ((s), (c)) : 0)
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
      if (!history_length)
	return;

      /* If there is something in the slot, then remove it. */
      if (the_history[0])
	{
	  free (the_history[0]->line);
	  free (the_history[0]);
	}

      for (i = 0; i < history_length; i++)
	the_history[i] = the_history[i + 1];

      history_base++;

    }
  else
    {
      if (!history_size)
	{
	  the_history = (HIST_ENTRY **)
	    xmalloc ((history_size = DEFAULT_HISTORY_GROW_SIZE)
		     * sizeof (HIST_ENTRY *));
	  history_length = 1;

	}
      else
	{
	  if (history_length == (history_size - 1))
	    {
	      the_history = (HIST_ENTRY **)
		xrealloc (the_history,
			  ((history_size += DEFAULT_HISTORY_GROW_SIZE)
			   * sizeof (HIST_ENTRY *)));
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
   If DIRECTION < 0, then the search is through previous entries,
   else through subsequent.  If the string is found, then
   current_history () is the history entry, and the value of this function
   is the offset in the line of that history entry that the string was
   found in.  Otherwise, nothing is changed, and a -1 is returned. */
int
history_search (string, direction)
     char *string;
     int direction;
{
  register int i = history_offset;
  register int reverse = (direction < 0);
  register char *line;
  register int index;
  int string_len = strlen (string);

  /* Take care of trivial cases first. */

  if (!history_length || ((i == history_length) && !reverse))
    return (-1);

  if (reverse && (i == history_length))
    i--;

  while (1)
    {
      /* Search each line in the history list for STRING. */

      /* At limit for direction? */
      if ((reverse && i < 0) ||
	  (!reverse && i == history_length))
	return (-1);

      line = the_history[i]->line;
      index = strlen (line);

      /* If STRING is longer than line, no match. */
      if (string_len > index)
	goto next_line;

      /* Do the actual search. */
      if (reverse)
	{
	  index -= string_len;

	  while (index >= 0)
	    {
	      if (strncmp (string, line + index, string_len) == 0)
		{
		  history_offset = i;
		  return (index);
		}
	      index--;
	    }
	}
      else
	{
	  register int limit = (string_len - index) + 1;
	  index = 0;

	  while (index < limit)
	    {
	      if (strncmp (string, line + index, string_len) == 0)
		{
		  history_offset = i;
		  return (index);
		}
	      index++;
	    }
	}
    next_line:
      if (reverse)
	i--;
      else
	i++;
    }
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
      result = - result;
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
      char *home = (char *)getenv ("HOME");
      if (!home) home = ".";
      return_val = (char *)xmalloc (2 + strlen (home) + strlen (".history"));
      sprintf (return_val, "%s/.history", home);
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
  int file, current_line, done;
  struct stat finfo;
  extern int errno;

  input = history_filename (filename);
  file = open (input, O_RDONLY, 0666);

  if ((file < 0) ||
      (stat (input, &finfo) == -1))
    goto error_and_exit;

  buffer = (char *)xmalloc (finfo.st_size + 1);

  if (read (file, buffer, finfo.st_size) != finfo.st_size)
  error_and_exit:
    {
      if (file >= 0)
	close (file);

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
  return (0);
}

/* Truncate the history file FNAME, leaving only LINES trailing lines.
   If FNAME is NULL, then use ~/.history. */
history_truncate_file (fname, lines)
     char *fname;
     register int lines;
{
  register int i;
  int file;
  char *buffer = (char *)NULL, *filename;
  struct stat finfo;

  filename = history_filename (fname);
  if (stat (filename, &finfo) == -1)
    goto truncate_exit;

  file = open (filename, O_RDONLY, 066);

  if (file == -1)
    goto truncate_exit;

  buffer = (char *)xmalloc (finfo.st_size + 1);
  read (file, buffer, finfo.st_size);
  close (file);

  /* Count backwards from the end of buffer until we have passed
     LINES lines. */
  for (i = finfo.st_size; lines && i; i--)
    {
      if (buffer[i] == '\n')
	lines--;
    }

  /* If there are fewer lines in the file than we want to truncate to,
     then we are all done. */
  if (!i)
    goto truncate_exit;

  /* Otherwise, write from the start of this line until the end of the
     buffer. */
  for (--i; i; i--)
    if (buffer[i] == '\n')
      {
	i++;
	break;
      }

  file = open (filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
  if (file == -1)
    goto truncate_exit;

  write (file, buffer + i, finfo.st_size - i);
  close (file);

 truncate_exit:
  if (buffer)
    free (buffer);

  free (filename);
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
  extern int errno;
  register int i;
  char *output = history_filename (filename);
  int file, mode;
  char cr = '\n';

  if (overwrite)
    mode = O_WRONLY | O_CREAT | O_TRUNC;
  else
    mode = O_WRONLY | O_APPEND;

  if ((file = open (output, mode, 0666)) == -1)
    return (errno);

  if (nelements > history_length)
    nelements = history_length;

  for (i = history_length - nelements; i < history_length; i++)
    {
      if (write (file, the_history[i]->line, strlen (the_history[i]->line)) < 0)
	break;
      if (write (file, &cr, 1) < 0)
	break;
    }

  close (file);
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
  int index = offset - history_base;

  if (index >= history_length ||
      index < 0 ||
      !the_history)
    return ((HIST_ENTRY *)NULL);
  return (the_history[index]);
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
  int which, sign = 1;
  HIST_ENTRY *entry;

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

  /* Handle !! case. */
  if (string[i] == history_expansion_char)
    {
      i++;
      which = history_base + (history_length - 1);
      *caller_index = i;
      goto get_which;
    }

  /* Hack case of numeric line specification. */
 read_which:
  if (string[i] == '-')
    {
      sign = -1;
      i++;
    }

  if (digit (string[i]))
    {
      int start = i;

      /* Get the extent of the digits. */
      for (; digit (string[i]); i++);

      /* Get the digit value. */
      sscanf (string + start, "%d", &which);

      *caller_index = i;

      if (sign < 0)
	which = (history_length + history_base) - which;

    get_which:
      if (entry = history_get (which))
	return (entry->line);

      return ((char *)NULL);
    }

  /* This must be something to search for.  If the spec begins with
     a '?', then the string may be anywhere on the line.  Otherwise,
     the string must be found at the start of a line. */
  {
    int index;
    char *temp;
    int substring_okay = 0;

    if (string[i] == '?')
      {
	substring_okay++;
	i++;
      }

    for (index = i; string[i]; i++)
      if (whitespace (string[i]) ||
	  string[i] == '\n' ||
	  string[i] == ':' ||
	  (substring_okay && string[i] == '?') ||
	  string[i] == delimiting_quote)
	break;

    temp = (char *)alloca (1 + (i - index));
    strncpy (temp, &string[index], (i - index));
    temp[i - index] = '\0';

    if (string[i] == '?')
      i++;

    *caller_index = i;

  search_again:

    index = history_search (temp, -1);

    if (index < 0)
    search_lost:
      {
	history_offset = history_length;
	return ((char *)NULL);
      }

    if (index == 0 || substring_okay || 
	(strncmp (temp, the_history[history_offset]->line,
		  strlen (temp)) == 0))
      {
      search_won:
	entry = current_history ();
	history_offset = history_length;
	
	/* If this was a substring search, then remember the string that
	   we matched for word substitution. */
	if (substring_okay)
	  {
	    if (search_string)
	      free (search_string);
	    search_string = savestring (temp);
	  }

	return (entry->line);
      }

    if (history_offset)
      history_offset--;
    else
      goto search_lost;
    
    goto search_again;
  }
}

/* Expand the string STRING, placing the result into OUTPUT, a pointer
   to a string.  Returns:

   0) If no expansions took place (or, if the only change in
      the text was the de-slashifying of the history expansion
      character)
   1) If expansions did take place
  -1) If there was an error in expansion.

  If an error ocurred in expansion, then OUTPUT contains a descriptive
  error message. */
int
history_expand (string, output)
     char *string;
     char **output;
{
  register int j, l = strlen (string);
  int i, word_spec_error = 0;
  int cc, modified = 0;
  char *word_spec, *event;
  int starting_index, only_printing = 0, substitute_globally = 0;

  char *get_history_word_specifier (), *rindex ();

  /* The output string, and its length. */
  int len = 0;
  char *result = (char *)NULL;

  /* Used in add_string; */
  char *temp, tt[2], tbl[3];

  /* Prepare the buffer for printing error messages. */
  result = (char *)xmalloc (len = 255);

  result[0] = tt[1] = tbl[2] = '\0';
  tbl[0] = '\\';
  tbl[1] = history_expansion_char;

  /* Grovel the string.  Only backslash can quote the history escape
     character.  We also handle arg specifiers. */

  /* Before we grovel forever, see if the history_expansion_char appears
     anywhere within the text. */

  /* The quick substitution character is a history expansion all right.  That
     is to say, "^this^that^" is equivalent to "!!:s^this^that^", and in fact,
     that is the substitution that we do. */
  if (string[0] == history_subst_char)
    {
      char *format_string = (char *)alloca (10 + strlen (string));

      sprintf (format_string, "%c%c:s%s",
	       history_expansion_char, history_expansion_char,
	       string);
      string = format_string;
      l += 4;
      goto grovel;
    }

  /* If not quick substitution, still maybe have to do expansion. */

  /* `!' followed by one of the characters in history_no_expand_chars
     is NOT an expansion. */
  for (i = 0; string[i]; i++)
    if (string[i] == history_expansion_char)
      if (!string[i + 1] || member (string[i + 1], history_no_expand_chars))
	continue;
      else
	goto grovel;

  free (result);
  *output = savestring (string);
  return (0);

 grovel:

  for (i = j = 0; i < l; i++)
    {
      int tchar = string[i];
      if (tchar == history_expansion_char)
	tchar = -3;

      switch (tchar)
	{
	case '\\':
	  if (string[i + 1] == history_expansion_char)
	    {
	      i++;
	      temp = tbl;
	      goto do_add;
	    }
	  else
	    goto add_char;

	  /* case history_expansion_char: */
	case -3:
	  starting_index = i + 1;
	  cc = string[i + 1];

	  /* If the history_expansion_char is followed by one of the
	     characters in history_no_expand_chars, then it is not a
	     candidate for expansion of any kind. */
	  if (member (cc, history_no_expand_chars))
	    goto add_char;

	  /* There is something that is listed as a `word specifier' in csh
	     documentation which means `the expanded text to this point'.
	     That is not a word specifier, it is an event specifier. */

	  if (cc == '#')
	    goto hack_pound_sign;

	  /* If it is followed by something that starts a word specifier,
	     then !! is implied as the event specifier. */

	  if (member (cc, ":$*%^"))
	    {
	      char fake_s[3];
	      int fake_i = 0;
	      i++;
	      fake_s[0] = fake_s[1] = history_expansion_char;
	      fake_s[2] = '\0';
	      event = get_history_event (fake_s, &fake_i, 0);
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
	  event_not_found:
	    {
	    int l = 1 + (i - starting_index);

	    temp = (char *)alloca (1 + l);
	    strncpy (temp, string + starting_index, l);
	    temp[l - 1] = 0;
	    sprintf (result, "%s: %s.", temp,
		     word_spec_error ? "Bad word specifier" : "Event not found");
	  error_exit:
	    *output = result;
	    return (-1);
	  }

	  /* If a word specifier is found, then do what that requires. */
	  starting_index = i;

	  word_spec = get_history_word_specifier (string, event, &i);

	  /* There is no such thing as a `malformed word specifier'.  However,
	     it is possible for a specifier that has no match.  In that case,
	     we complain. */
	  if (word_spec == (char *)-1)
	  bad_word_spec:
	  {
	    word_spec_error++;
	    goto event_not_found;
	  }

	  /* If no word specifier, than the thing of interest was the event. */
	  if (!word_spec)
	    temp = event;
	  else
	    {
	      temp = (char *)alloca (1 + strlen (word_spec));
	      strcpy (temp, word_spec);
	      free (word_spec);
	    }

	  /* Perhaps there are other modifiers involved.  Do what they say. */

	hack_specials:

	  if (string[i] == ':')
	    {
	      char *tstr;

	      switch (string[i + 1])
		{
		  /* :p means make this the last executed line.  So we
		     return an error state after adding this line to the
		     history. */
		case 'p':
		  only_printing++;
		  goto next_special;

		  /* :t discards all but the last part of the pathname. */
		case 't':
		  tstr = rindex (temp, '/');
		  if (tstr)
		    temp = ++tstr;
		  goto next_special;

		  /* :h discards the last part of a pathname. */
		case 'h':
		  tstr = rindex (temp, '/');
		  if (tstr)
		    *tstr = '\0';
		  goto next_special;

		  /* :r discards the suffix. */
		case 'r':
		  tstr = rindex (temp, '.');
		  if (tstr)
		    *tstr = '\0';
		  goto next_special;

		  /* :e discards everything but the suffix. */
		case 'e':
		  tstr = rindex (temp, '.');
		  if (tstr)
		    temp = tstr;
		  goto next_special;

		  /* :s/this/that substitutes `this' for `that'. */
		  /* :gs/this/that substitutes `this' for `that' globally. */
		case 'g':
		  if (string[i + 2] == 's')
		    {
		      i++;
		      substitute_globally = 1;
		      goto substitute;
		    }
		  else
		    
		case 's':
		  substitute:
		  {
		    char *this, *that, *new_event;
		    int delimiter = 0;
		    int si, l_this, l_that, l_temp = strlen (temp);

		    if (i + 2 < strlen (string))
		      delimiter = string[i + 2];

		    if (!delimiter)
		      break;

		    i += 3;

		    /* Get THIS. */
		    for (si = i; string[si] && string[si] != delimiter; si++);
		    l_this = (si - i);
		    this = (char *)alloca (1 + l_this);
		    strncpy (this, string + i, l_this);
		    this[l_this] = '\0';

		    i = si;
		    if (string[si])
		      i++;

		    /* Get THAT. */
		    for (si = i; string[si] && string[si] != delimiter; si++);
		    l_that = (si - i);
		    that = (char *)alloca (1 + l_that);
		    strncpy (that, string + i, l_that);
		    that[l_that] = '\0';

		    i = si;
		    if (string[si]) i++;

		    /* Ignore impossible cases. */
		    if (l_this > l_temp)
		      goto cant_substitute;

		    /* Find the first occurrence of THIS in TEMP. */
		    si = 0;
		    for (; (si + l_this) <= l_temp; si++)
		      if (strncmp (temp + si, this, l_this) == 0)
			{
			  new_event =
			    (char *)alloca (1 + (l_that - l_this) + l_temp);
			  strncpy (new_event, temp, si);
			  strncpy (new_event + si, that, l_that);
			  strncpy (new_event + si + l_that,
				   temp + si + l_this,
				   l_temp - (si + l_this));
			  new_event[(l_that - l_this) + l_temp] = '\0';
			  temp = new_event;

			  if (substitute_globally)
			    {
			      si += l_that;
			      l_temp = strlen (temp);
			      substitute_globally++;
			      continue;
			    }

			  goto hack_specials;
			}

		  cant_substitute:

		    if (substitute_globally > 1)
		      {
			substitute_globally = 0;
			goto hack_specials;
		      }

		    goto event_not_found;
		  }

		  /* :# is the line so far.  Note that we have to
		     alloca () it since RESULT could be realloc ()'ed
		     below in add_string. */
		case '#':
		hack_pound_sign:
		  if (result)
		    {
		      temp = (char *)alloca (1 + strlen (result));
		      strcpy (temp, result);
		    }
		  else
		    temp = "";

		next_special:
		  i += 2;
		  goto hack_specials;
		}

	    }
	  /* Believe it or not, we have to back the pointer up by one. */
	  --i;
	  goto add_string;

	  /* A regular character.  Just add it to the output string. */
	default:
	add_char:
	  tt[0] = string[i];
	  temp = tt;
	  goto do_add;

	add_string:
	  modified++;

	do_add:
	  j += strlen (temp);
	  while (j > len)
	    result = (char *)xrealloc (result, (len += 255));

	  strcpy (result + (j - strlen (temp)), temp);
	}
    }

  *output = result;

  if (only_printing)
    {
      add_history (result);
      return (-1);
    }

  return (modified != 0);
}

/* Return a consed string which is the word specified in SPEC, and found
   in FROM.  NULL is returned if there is no spec.  -1 is returned if
   the word specified cannot be found.  CALLER_INDEX is the offset in
   SPEC to start looking; it is updated to point to just after the last
   character parsed. */
char *
get_history_word_specifier (spec, from, caller_index)
     char *spec, *from;
     int *caller_index;
{
  register int i = *caller_index;
  int first, last;
  int expecting_word_spec = 0;
  char *history_arg_extract ();

  /* The range of words to return doesn't exist yet. */
  first = last = 0;

  /* If we found a colon, then this *must* be a word specification.  If
     it isn't, then it is an error. */
  if (spec[i] == ':')
    i++, expecting_word_spec++;

  /* Handle special cases first. */

  /* `%' is the word last searched for. */
  if (spec[i] == '%')
    {
      *caller_index = i + 1;
      if (search_string)
	return (savestring (search_string));
      else
	return (savestring (""));
    }

  /* `*' matches all of the arguments, but not the command. */
  if (spec[i] == '*')
    {
      char *star_result;

      *caller_index = i + 1;
      star_result = history_arg_extract (1, '$', from);

      if (!star_result)
	star_result = savestring ("");

      return (star_result);
    }

  /* `$' is last arg. */
  if (spec[i] == '$')
    {
      *caller_index = i + 1;
      return (history_arg_extract ('$', '$', from));
    }

  /* Try to get FIRST and LAST figured out. */
  if (spec[i] == '-' || spec[i] == '^')
    {
      first = 1;
      goto get_last;
    }

 get_first:
  if (digit (spec[i]) && expecting_word_spec)
    {
      sscanf (spec + i, "%d", &first);
      for (; digit (spec[i]); i++);
    }
  else
    return ((char *)NULL);

 get_last:
  if (spec[i] == '^')
    {
      i++;
      last = 1;
      goto get_args;
    }

  if (spec[i] != '-')
    {
      last = first;
      goto get_args;
    }

  i++;

  if (digit (spec[i]))
    {
      sscanf (spec + i, "%d", &last);
      for (; digit (spec[i]); i++);
    }
  else
    if (spec[i] == '$')
      {
	i++;
	last = '$';
      }

 get_args:
  {
    char *result = (char *)NULL;

    *caller_index = i;

    if (last >= first)
      result = history_arg_extract (first, last, from);

    if (result)
      return (result);
    else
      return ((char *)-1);
  }
}

/* Extract the args specified, starting at FIRST, and ending at LAST.
   The args are taken from STRING.  If either FIRST or LAST is < 0,
   then make that arg count from the right (subtract from the number of
   tokens, so that FIRST = -1 means the next to last token on the line). */
char *
history_arg_extract (first, last, string)
     int first, last;
     char *string;
{
  register int i, len;
  char *result = (char *)NULL;
  int size = 0, offset = 0;

  char **history_tokenize (), **list;

  if (!(list = history_tokenize (string)))
    return ((char *)NULL);

  for (len = 0; list[len]; len++);

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
      for (i = first; i < last; i++)
	{
	  int l = strlen (list[i]);

	  if (!result)
	    result = (char *)xmalloc ((size = (2 + l)));
	  else
	    result = (char *)xrealloc (result, (size += (2 + l)));
	  strcpy (result + offset, list[i]);
	  offset += l;
	  if (i + 1 < last)
	    {
	      strcpy (result + offset, " ");
	      offset++;
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
 get_token:

  /* Skip leading whitespace. */
  for (; string[i] && whitespace(string[i]); i++);

  start = i;

  if (!string[i] || string[i] == history_comment_char)
    return (result);

  if (member (string[i], "()\n")) {
    i++;
    goto got_token;
  }

  if (member (string[i], "<>;&|")) {
    int peek = string[i + 1];

    if (peek == string[i]) {
      if (peek ==  '<') {
	if (string[1 + 2] == '-')
	  i++;
	i += 2;
	goto got_token;
      }

      if (member (peek, ">:&|")) {
	i += 2;
	goto got_token;
      }
    } else {
      if ((peek == '&' &&
	  (string[i] == '>' || string[i] == '<')) ||
	  ((peek == '>') &&
	  (string[i] == '&'))) {
	i += 2;
	goto got_token;
      }
    }
    i++;
    goto got_token;
  }

  /* Get word from string + i; */
  {
    int delimiter = 0;

    if (member (string[i], "\"'`"))
      delimiter = string[i++];

    for (;string[i]; i++) {

      if (string[i] == '\\') {

	if (string[i + 1] == '\n') {
	  i++;
	  continue;
	} else {
	  if (delimiter != '\'')
	    if ((delimiter != '"') ||
		(member (string[i], slashify_in_quotes))) {
	      i++;
	      continue;
	    }
	}
      }

      if (delimiter && string[i] == delimiter) {
	delimiter = 0;
	continue;
      }

      if (!delimiter && (member (string[i], " \t\n;&()|<>")))
	goto got_token;

      if (!delimiter && member (string[i], "\"'`")) {
	delimiter = string[i];
	continue;
      }
    }
    got_token:

    len = i - start;
    if (result_index + 2 >= size) {
      if (!size)
	result = (char **)xmalloc ((size = 10) * (sizeof (char *)));
      else
	result =
	  (char **)xrealloc (result, ((size += 10) * (sizeof (char *))));
    }
    result[result_index] = (char *)xmalloc (1 + len);
    strncpy (result[result_index], string + start, len);
    result[result_index][len] = '\0';
    result_index++;
    result[result_index] = (char *)NULL;
  }
  if (string[i])
    goto get_token;

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

#endif				/* TEST */

/*
* Local variables:
* compile-command: "gcc -g -DTEST -o history history.c"
* end:
*/
