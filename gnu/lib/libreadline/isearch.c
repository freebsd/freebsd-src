/* **************************************************************** */
/*								    */
/*			I-Search and Searching			    */
/*								    */
/* **************************************************************** */

/* Copyright (C) 1987,1989 Free Software Foundation, Inc.

   This file contains the Readline Library (the Library), a set of
   routines for providing Emacs style line input to programs that ask
   for it.

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

#include <stdio.h>

#if defined (__GNUC__)
#  define alloca __builtin_alloca
#else
#  if defined (sparc) || defined (HAVE_ALLOCA_H)
#    include <alloca.h>
#  endif
#endif

#include "readline.h"
#include "history.h"

extern Keymap _rl_keymap;
extern HIST_ENTRY *saved_line_for_history;
extern int rl_line_buffer_len;
extern int rl_point, rl_end;
extern char *rl_prompt, *rl_line_buffer;

/* Remove these declarations when we have a complete libgnu.a. */
extern char *xmalloc (), *xrealloc ();

static void rl_search_history ();

/* Search backwards through the history looking for a string which is typed
   interactively.  Start with the current line. */
rl_reverse_search_history (sign, key)
     int sign;
     int key;
{
  rl_search_history (-sign, key);
}

/* Search forwards through the history looking for a string which is typed
   interactively.  Start with the current line. */
rl_forward_search_history (sign, key)
     int sign;
     int key;
{
  rl_search_history (sign, key);
}

/* Display the current state of the search in the echo-area.
   SEARCH_STRING contains the string that is being searched for,
   DIRECTION is zero for forward, or 1 for reverse,
   WHERE is the history list number of the current line.  If it is
   -1, then this line is the starting one. */
static void
rl_display_search (search_string, reverse_p, where)
     char *search_string;
     int reverse_p, where;
{
  char *message = (char *)NULL;

  message =
    (char *)xmalloc (1 + (search_string ? strlen (search_string) : 0) + 30);

  *message = '\0';

#if defined (NOTDEF)
  if (where != -1)
    sprintf (message, "[%d]", where + history_base);
#endif /* NOTDEF */

  strcat (message, "(");

  if (reverse_p)
    strcat (message, "reverse-");

  strcat (message, "i-search)`");

  if (search_string)
    strcat (message, search_string);

  strcat (message, "': ");
  rl_message ("%s", message, 0);
  free (message);
  rl_redisplay ();
}

/* Search through the history looking for an interactively typed string.
   This is analogous to i-search.  We start the search in the current line.
   DIRECTION is which direction to search; >= 0 means forward, < 0 means
   backwards. */
static void
rl_search_history (direction, invoking_key)
     int direction;
     int invoking_key;
{
  /* The string that the user types in to search for. */
  char *search_string;

  /* The current length of SEARCH_STRING. */
  int search_string_index;

  /* The amount of space that SEARCH_STRING has allocated to it. */
  int search_string_size;

  /* The list of lines to search through. */
  char **lines;

  /* The length of LINES. */
  int hlen;

  /* Where we get LINES from. */
  HIST_ENTRY **hlist = history_list ();

  register int i = 0;
  int orig_point = rl_point;
  int orig_line = where_history ();
  int last_found_line = orig_line;
  int c, done = 0;

  /* The line currently being searched. */
  char *sline;

  /* Offset in that line. */
  int index;

  /* Non-zero if we are doing a reverse search. */
  int reverse = (direction < 0);

  /* Create an arrary of pointers to the lines that we want to search. */
  maybe_replace_line ();
  if (hlist)
    for (i = 0; hlist[i]; i++);

  /* Allocate space for this many lines, +1 for the current input line,
     and remember those lines. */
  lines = (char **)alloca ((1 + (hlen = i)) * sizeof (char *));
  for (i = 0; i < hlen; i++)
    lines[i] = hlist[i]->line;

  if (saved_line_for_history)
    lines[i] = saved_line_for_history->line;
  else
    /* So I have to type it in this way instead. */
    {
      char *alloced_line;

      /* Keep that MIPS alloca () happy. */
      alloced_line = (char *)alloca (1 + strlen (rl_line_buffer));
      lines[i] = alloced_line;
      strcpy (lines[i], &rl_line_buffer[0]);
    }

  hlen++;

  /* The line where we start the search. */
  i = orig_line;

  /* Initialize search parameters. */
  search_string = (char *)xmalloc (search_string_size = 128);
  *search_string = '\0';
  search_string_index = 0;

  /* Normalize DIRECTION into 1 or -1. */
  if (direction >= 0)
    direction = 1;
  else
    direction = -1;

  rl_display_search (search_string, reverse, -1);

  sline = rl_line_buffer;
  index = rl_point;

  while (!done)
    {
      c = rl_read_key ();

      /* Hack C to Do What I Mean. */
      {
	Function *f = (Function *)NULL;

	if (_rl_keymap[c].type == ISFUNC)
	  {
	    f = _rl_keymap[c].function;

	    if (f == rl_reverse_search_history)
	      c = reverse ? -1 : -2;
	    else if (f == rl_forward_search_history)
	      c =  !reverse ? -1 : -2;
	  }
      }

      switch (c)
	{
	case ESC:
	  done = 1;
	  continue;

	  /* case invoking_key: */
	case -1:
	  goto search_again;

	  /* switch directions */
	case -2:
	  direction = -direction;
	  reverse = (direction < 0);

	  goto do_search;

	case CTRL ('G'):
	  strcpy (rl_line_buffer, lines[orig_line]);
	  rl_point = orig_point;
	  rl_end = strlen (rl_line_buffer);
	  rl_clear_message ();
	  return;

	default:
	  if (c < 32 || c > 126)
	    {
	      rl_execute_next (c);
	      done = 1;
	      continue;
	    }
	  else
	    {
	      if (search_string_index + 2 >= search_string_size)
		search_string = (char *)xrealloc
		  (search_string, (search_string_size += 128));
	      search_string[search_string_index++] = c;
	      search_string[search_string_index] = '\0';
	      goto do_search;

	    search_again:

	      if (!search_string_index)
		continue;
	      else
		{
		  if (reverse)
		    --index;
		  else
		    if (index != strlen (sline))
		      ++index;
		    else
		      ding ();
		}
	    do_search:

	      while (1)
		{
		  if (reverse)
		    {
		      while (index >= 0)
			if (strncmp
			    (search_string, sline + index, search_string_index)
			    == 0)
			  goto string_found;
			else
			  index--;
		    }
		  else
		    {
		      register int limit =
			(strlen (sline) - search_string_index) + 1;

		      while (index < limit)
			{
			  if (strncmp (search_string,
				       sline + index,
				       search_string_index) == 0)
			    goto string_found;
			  index++;
			}
		    }

		next_line:
		  i += direction;

		  /* At limit for direction? */
		  if ((reverse && i < 0) ||
		      (!reverse && i == hlen))
		    goto search_failed;

		  sline = lines[i];
		  if (reverse)
		    index = strlen (sline);
		  else
		    index = 0;

		  /* If the search string is longer than the current
		     line, no match. */
		  if (search_string_index > (int)strlen (sline))
		    goto next_line;

		  /* Start actually searching. */
		  if (reverse)
		    index -= search_string_index;
		}

	    search_failed:
	      /* We cannot find the search string.  Ding the bell. */
	      ding ();
	      i = last_found_line;
	      break;

	    string_found:
	      /* We have found the search string.  Just display it.  But don't
		 actually move there in the history list until the user accepts
		 the location. */
	      {
		int line_len;

		line_len = strlen (lines[i]);

		if (line_len >= rl_line_buffer_len)
		  rl_extend_line_buffer (line_len);

		strcpy (rl_line_buffer, lines[i]);
		rl_point = index;
		rl_end = line_len;
		last_found_line = i;
		rl_display_search
		  (search_string, reverse, (i == orig_line) ? -1 : i);
	      }
	    }
	}
    }

  /* The searching is over.  The user may have found the string that she
     was looking for, or else she may have exited a failing search.  If
     INDEX is -1, then that shows that the string searched for was not
     found.  We use this to determine where to place rl_point. */
  {
    int now = last_found_line;

    /* First put back the original state. */
    strcpy (rl_line_buffer, lines[orig_line]);

    /* Free the search string. */
    free (search_string);

    if (now < orig_line)
      rl_get_previous_history (orig_line - now);
    else
      rl_get_next_history (now - orig_line);

    /* If the index of the "matched" string is less than zero, then the
       final search string was never matched, so put point somewhere
       reasonable. */
    if (index < 0)
      index = strlen (rl_line_buffer);

    rl_point = index;
    rl_clear_message ();
  }
}
