/* search.c - code for non-incremental searching in emacs and vi modes. */

/* Copyright (C) 1992 Free Software Foundation, Inc.

   This file is part of the Readline Library (the Library), a set of
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

extern char *xmalloc (), *xrealloc ();

/* Variables imported from readline.c */
extern int rl_point, rl_end, rl_line_buffer_len;
extern Keymap _rl_keymap;
extern char *rl_prompt;
extern char *rl_line_buffer;
extern HIST_ENTRY *saved_line_for_history;

static char *noninc_search_string = (char *) NULL;
static int noninc_history_pos = 0;

/* Search the history list for STRING starting at absolute history position
   POS.  If STRING begins with `^', the search must match STRING at the
   beginning of a history line, otherwise a full substring match is performed
   for STRING.  DIR < 0 means to search backwards through the history list,
   DIR >= 0 means to search forward. */
static int
noninc_search_from_pos (string, pos, dir)
     char *string;
     int pos, dir;
{
  int ret, old;

  old = where_history ();
  history_set_pos (pos);

  if (*string == '^')
    ret = history_search_prefix (string + 1, dir);
  else
    ret = history_search (string, dir);

  if (ret != -1)
    ret = where_history ();

  history_set_pos (old);
  return (ret);
}

/* Search for a line in the history containing STRING.  If DIR is < 0, the
   search is backwards through previous entries, else through subsequent
   entries. */
static void
noninc_dosearch (string, dir)
     char *string;
     int dir;
{
  int oldpos, pos;
  HIST_ENTRY *entry;

  if (string == 0 || *string == 0 || noninc_history_pos < 0)
    {
      ding ();
      return;
    }

  pos = noninc_search_from_pos (string, noninc_history_pos + dir, dir);
  if (pos == -1)
    {
      /* Search failed, current history position unchanged. */
      maybe_unsave_line ();
      rl_clear_message ();
      rl_point = 0;
      ding ();
      return;
    }

  noninc_history_pos = pos;

  oldpos = where_history ();
  history_set_pos (noninc_history_pos);
  entry = current_history ();
  history_set_pos (oldpos);

  {
    int line_len;

    line_len = strlen (entry->line);
    if (line_len >= rl_line_buffer_len)
      rl_extend_line_buffer (line_len);
    strcpy (rl_line_buffer, entry->line);
  }

  rl_undo_list = (UNDO_LIST *)entry->data;
  rl_end = strlen (rl_line_buffer);
  rl_point = 0;
  rl_clear_message ();

  if (saved_line_for_history)
    free_history_entry (saved_line_for_history);
  saved_line_for_history = (HIST_ENTRY *)NULL;
}

/* Search non-interactively through the history list.  DIR < 0 means to
   search backwards through the history of previous commands; otherwise
   the search is for commands subsequent to the current position in the
   history list.  PCHAR is the character to use for prompting when reading
   the search string; if not specified (0), it defaults to `:'. */
static void
noninc_search (dir, pchar)
     int dir;
     int pchar;
{
  int saved_point, c, pmtlen;
  char *p;

  maybe_save_line ();
  saved_point = rl_point;

  /* Use the line buffer to read the search string. */
  rl_line_buffer[0] = 0;
  rl_end = rl_point = 0;

  pmtlen = (rl_prompt && *rl_prompt) ? strlen (rl_prompt) : 0;
  p = (char *)alloca (2 + pmtlen);
  if (pmtlen)
    strcpy (p, rl_prompt);
  p[pmtlen] = pchar ? pchar : ':';
  p[pmtlen + 1]  = '\0';

  rl_message (p, 0, 0);

  /* Read the search string. */
  while (c = rl_read_key ())
    {
      switch (c)
	{
	case CTRL('H'):
	case RUBOUT:
	  if (rl_point == 0)
	    {
	      maybe_unsave_line ();
	      rl_clear_message ();
	      rl_point = saved_point;
	      return;
	    }
	  /* FALLTHROUGH */

	case CTRL('W'):
	case CTRL('U'):
	  rl_dispatch (c, _rl_keymap);
	  break;

	case RETURN:
	case NEWLINE:
	  goto dosearch;
	  /* NOTREACHED */
	  break;

	case CTRL('C'):
	case CTRL('G'):
	  maybe_unsave_line ();
	  rl_clear_message ();
	  rl_point = saved_point;
	  ding ();
	  return;

	default:
	  rl_insert (1, c);
	  break;
	}
      rl_redisplay ();
    }

 dosearch:
  /* If rl_point == 0, we want to re-use the previous search string and
     start from the saved history position.  If there's no previous search
     string, punt. */
  if (rl_point == 0)
    {
      if (!noninc_search_string)
	{
	  ding ();
	  return;
	}
    }
  else
    {
      /* We want to start the search from the current history position. */
      noninc_history_pos = where_history ();
      if (noninc_search_string)
	free (noninc_search_string);
      noninc_search_string = savestring (rl_line_buffer);
    }

  noninc_dosearch (noninc_search_string, dir);
}

/* Search forward through the history list for a string.  If the vi-mode
   code calls this, KEY will be `?'. */
rl_noninc_forward_search (count, key)
     int count, key;
{
  if (key == '?')
    noninc_search (1, '?');
  else
    noninc_search (1, 0);
  return 0;
}

/* Reverse search the history list for a string.  If the vi-mode code
   calls this, KEY will be `/'. */
rl_noninc_reverse_search (count, key)
     int count, key;
{
  if (key == '/')
    noninc_search (-1, '/');
  else
    noninc_search (-1, 0);
  return 0;
}

/* Search forward through the history list for the last string searched
   for.  If there is no saved search string, abort. */
rl_noninc_forward_search_again (count, key)
     int count, key;
{
  if (!noninc_search_string)
    {
      ding ();
      return (-1);
    }
  noninc_dosearch (noninc_search_string, 1);
  return 0;
}

/* Reverse search in the history list for the last string searched
   for.  If there is no saved search string, abort. */
rl_noninc_reverse_search_again (count, key)
     int count, key;
{
  if (!noninc_search_string)
    {
      ding ();
      return (-1);
    }
  noninc_dosearch (noninc_search_string, -1);
  return 0;
}
