/* vi_mode.c -- A vi emulation mode for Bash.
   Derived from code written by Jeff Sparkes (jsparkes@bnr.ca).  */

/* Copyright (C) 1987, 1989, 1992 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 1, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */
#define READLINE_LIBRARY

/* **************************************************************** */
/*								    */
/*			VI Emulation Mode			    */
/*								    */
/* **************************************************************** */
#include "rlconf.h"

#if defined (VI_MODE)

#include <sys/types.h>

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif /* HAVE_STDLIB_H */

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <stdio.h>

/* Some standard library routines. */
#include "rldefs.h"
#include "readline.h"
#include "history.h"

#ifndef digit_p
#define digit_p(c)  ((c) >= '0' && (c) <= '9')
#endif

#ifndef digit_value
#define digit_value(c) ((c) - '0')
#endif

#ifndef member
#define member(c, s) ((c) ? (char *)strchr ((s), (c)) != (char *)NULL : 0)
#endif

#ifndef isident
#define isident(c) ((pure_alphabetic (c) || digit_p (c) || c == '_'))
#endif

#ifndef exchange
#define exchange(x, y) do {int temp = x; x = y; y = temp;} while (0)
#endif

#ifndef VI_COMMENT_BEGIN_DEFAULT
#define VI_COMMENT_BEGIN_DEFAULT "#"
#endif

#if defined (STATIC_MALLOC)
static char *xmalloc (), *xrealloc ();
#else
extern char *xmalloc (), *xrealloc ();
#endif /* STATIC_MALLOC */

/* Variables imported from readline.c */
extern int rl_point, rl_end, rl_mark, rl_done;
extern FILE *rl_instream;
extern int rl_line_buffer_len, rl_explicit_arg, rl_numeric_arg;
extern Keymap _rl_keymap;
extern char *rl_prompt;
extern char *rl_line_buffer;
extern int rl_arg_sign;

extern void _rl_dispatch ();

extern void rl_extend_line_buffer ();
extern int rl_vi_check ();

/* Non-zero means enter insertion mode. */
static int _rl_vi_doing_insert = 0;

/* String inserted into the line by rl_vi_comment (). */
char *rl_vi_comment_begin = (char *)NULL;

/* *** UNCLEAN *** */
/* Command keys which do movement for xxx_to commands. */
static char *vi_motion = " hl^$0ftFt;,%wbeWBE|";

/* Keymap used for vi replace characters.  Created dynamically since
   rarely used. */
static Keymap vi_replace_map = (Keymap)NULL;

/* The number of characters inserted in the last replace operation. */
static int vi_replace_count = 0;

/* If non-zero, we have text inserted after a c[motion] command that put
   us implicitly into insert mode.  Some people want this text to be
   attached to the command so that it is `redoable' with `.'. */
static int vi_continued_command = 0;

static int _rl_vi_last_command = 'i';	/* default `.' puts you in insert mode */
static int _rl_vi_last_repeat = 1;
static int _rl_vi_last_arg_sign = 1;
static int _rl_vi_last_motion = 0;
static int _rl_vi_last_search_char = 0;
static int _rl_vi_last_replacement = 0;

static int vi_redoing = 0;

/* Text modification commands.  These are the `redoable' commands. */
static char *vi_textmod = "_*\\AaIiCcDdPpYyRrSsXx~";

static int rl_digit_loop1 ();

void
_rl_vi_reset_last ()
{
  _rl_vi_last_command = 'i';
  _rl_vi_last_repeat = 1;
  _rl_vi_last_arg_sign = 1;
  _rl_vi_last_motion = 0;
}

void
_rl_vi_set_last (key, repeat, sign)
     int key, repeat, sign;
{
  _rl_vi_last_command = key;
  _rl_vi_last_repeat = repeat;
  _rl_vi_last_arg_sign = sign;
}

/* Is the command C a VI mode text modification command? */
int
rl_vi_textmod_command (c)
     int c;
{
  return (member (c, vi_textmod));
}

/* Bound to `.'.  Called from command mode, so we know that we have to
   redo a text modification command.  The default for _rl_vi_last_command
   puts you back into insert mode. */
rl_vi_redo (count, c)
     int count, c;
{
  if (!rl_explicit_arg)
    {
      rl_numeric_arg = _rl_vi_last_repeat;
      rl_arg_sign = _rl_vi_last_arg_sign;
    }

  vi_redoing = 1;
  _rl_dispatch (_rl_vi_last_command, _rl_keymap);
  vi_redoing = 0;

  return (0);
}
    
/* Yank the nth arg from the previous line into this line at point. */
rl_vi_yank_arg (count, key)
     int count, key;
{
  /* Readline thinks that the first word on a line is the 0th, while vi
     thinks the first word on a line is the 1st.  Compensate. */
  if (rl_explicit_arg)
    rl_yank_nth_arg (count - 1, 0);
  else
    rl_yank_nth_arg ('$', 0);

  return (0);
}

/* With an argument, move back that many history lines, else move to the
   beginning of history. */
rl_vi_fetch_history (count, c)
     int count, c;
{
  int current = where_history ();

  /* Giving an argument of n means we want the nth command in the history
     file.  The command number is interpreted the same way that the bash
     `history' command does it -- that is, giving an argument count of 450
     to this command would get the command listed as number 450 in the
     output of `history'. */
  if (rl_explicit_arg)
    {
      int wanted = history_base + current - count;
      if (wanted <= 0)
        rl_beginning_of_history (0, 0);
      else
        rl_get_previous_history (wanted);
    }
  else
    rl_beginning_of_history (count, 0);
  return (0);
}

/* Search again for the last thing searched for. */
rl_vi_search_again (count, key)
     int count, key;
{
  switch (key)
    {
    case 'n':
      rl_noninc_reverse_search_again (count, key);
      break;

    case 'N':
      rl_noninc_forward_search_again (count, key);
      break;
    }
  return (0);
}

/* Do a vi style search. */
rl_vi_search (count, key)
     int count, key;
{
  switch (key)
    {
    case '?':
      rl_noninc_forward_search (count, key);
      break;

    case '/':
      rl_noninc_reverse_search (count, key);
      break;

    default:
      ding ();
      break;
    }
  return (0);
}

/* Completion, from vi's point of view. */
rl_vi_complete (ignore, key)
     int ignore, key;
{
  if ((rl_point < rl_end) && (!whitespace (rl_line_buffer[rl_point])))
    {
      if (!whitespace (rl_line_buffer[rl_point + 1]))
	rl_vi_end_word (1, 'E');
      rl_point++;
    }

  if (key == '*')
    rl_complete_internal ('*');	/* Expansion and replacement. */
  else if (key == '=')
    rl_complete_internal ('?');	/* List possible completions. */
  else if (key == '\\')
    rl_complete_internal (TAB);	/* Standard Readline completion. */
  else
    rl_complete (0, key);

  if (key == '*' || key == '\\')
    {
      _rl_vi_set_last (key, 1, rl_arg_sign);
      rl_vi_insertion_mode (1, key);
    }
  return (0);
}

/* Tilde expansion for vi mode. */
rl_vi_tilde_expand (ignore, key)
     int ignore, key;
{
  rl_tilde_expand (0, key);
  _rl_vi_set_last (key, 1, rl_arg_sign);	/* XXX */
  rl_vi_insertion_mode (1, key);
  return (0);
}

/* Previous word in vi mode. */
rl_vi_prev_word (count, key)
     int count, key;
{
  if (count < 0)
    return (rl_vi_next_word (-count, key));

  if (rl_point == 0)
    {
      ding ();
      return (0);
    }

  if (uppercase_p (key))
    rl_vi_bWord (count);
  else
    rl_vi_bword (count);

  return (0);
}

/* Next word in vi mode. */
rl_vi_next_word (count, key)
     int count, key;
{
  if (count < 0)
    return (rl_vi_prev_word (-count, key));

  if (rl_point >= (rl_end - 1))
    {
      ding ();
      return (0);
    }

  if (uppercase_p (key))
    rl_vi_fWord (count);
  else
    rl_vi_fword (count);
  return (0);
}

/* Move to the end of the ?next? word. */
rl_vi_end_word (count, key)
     int count, key;
{
  if (count < 0)
    {
      ding ();
      return -1;
    }

  if (uppercase_p (key))
    rl_vi_eWord (count);
  else
    rl_vi_eword (count);
  return (0);
}

/* Move forward a word the way that 'W' does. */
rl_vi_fWord (count)
     int count;
{
  while (count-- && rl_point < (rl_end - 1))
    {
      /* Skip until whitespace. */
      while (!whitespace (rl_line_buffer[rl_point]) && rl_point < rl_end)
	rl_point++;

      /* Now skip whitespace. */
      while (whitespace (rl_line_buffer[rl_point]) && rl_point < rl_end)
	rl_point++;
    }
  return (0);
}

rl_vi_bWord (count)
     int count;
{
  while (count-- && rl_point > 0)
    {
      /* If we are at the start of a word, move back to whitespace so
	 we will go back to the start of the previous word. */
      if (!whitespace (rl_line_buffer[rl_point]) &&
	  whitespace (rl_line_buffer[rl_point - 1]))
	rl_point--;

      while (rl_point > 0 && whitespace (rl_line_buffer[rl_point]))
	rl_point--;

      if (rl_point > 0)
	{
	  while (--rl_point >= 0 && !whitespace (rl_line_buffer[rl_point]));
	  rl_point++;
	}
    }
  return (0);
}

rl_vi_eWord (count)
     int count;
{
  while (count-- && rl_point < (rl_end - 1))
    {
      if (!whitespace (rl_line_buffer[rl_point]))
	rl_point++;

      /* Move to the next non-whitespace character (to the start of the
	 next word). */
      while (++rl_point < rl_end && whitespace (rl_line_buffer[rl_point]));

      if (rl_point && rl_point < rl_end)
	{
	  /* Skip whitespace. */
	  while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
	    rl_point++;

	  /* Skip until whitespace. */
	  while (rl_point < rl_end && !whitespace (rl_line_buffer[rl_point]))
	    rl_point++;

	  /* Move back to the last character of the word. */
	  rl_point--;
	}
    }
  return (0);
}

rl_vi_fword (count)
     int count;
{
  while (count-- && rl_point < (rl_end - 1))
    {
      /* Move to white space (really non-identifer). */
      if (isident (rl_line_buffer[rl_point]))
	{
	  while (isident (rl_line_buffer[rl_point]) && rl_point < rl_end)
	    rl_point++;
	}
      else /* if (!whitespace (rl_line_buffer[rl_point])) */
	{
	  while (!isident (rl_line_buffer[rl_point]) &&
		 !whitespace (rl_line_buffer[rl_point]) && rl_point < rl_end)
	    rl_point++;
	}

      /* Move past whitespace. */
      while (whitespace (rl_line_buffer[rl_point]) && rl_point < rl_end)
	rl_point++;
    }
  return (0);
}

rl_vi_bword (count)
     int count;
{
  while (count-- && rl_point > 0)
    {
      int last_is_ident;

      /* If we are at the start of a word, move back to whitespace
	 so we will go back to the start of the previous word. */
      if (!whitespace (rl_line_buffer[rl_point]) &&
	  whitespace (rl_line_buffer[rl_point - 1]))
	rl_point--;

      /* If this character and the previous character are `opposite', move
	 back so we don't get messed up by the rl_point++ down there in
	 the while loop.  Without this code, words like `l;' screw up the
	 function. */
      last_is_ident = isident (rl_line_buffer[rl_point - 1]);
      if ((isident (rl_line_buffer[rl_point]) && !last_is_ident) ||
	  (!isident (rl_line_buffer[rl_point]) && last_is_ident))
	rl_point--;

      while (rl_point > 0 && whitespace (rl_line_buffer[rl_point]))
	rl_point--;

      if (rl_point > 0)
	{
	  if (isident (rl_line_buffer[rl_point]))
	    while (--rl_point >= 0 && isident (rl_line_buffer[rl_point]));
	  else
	    while (--rl_point >= 0 && !isident (rl_line_buffer[rl_point]) &&
		   !whitespace (rl_line_buffer[rl_point]));
	  rl_point++;
	}
    }
  return (0);
}

rl_vi_eword (count)
     int count;
{
  while (count-- && rl_point < rl_end - 1)
    {
      if (!whitespace (rl_line_buffer[rl_point]))
	rl_point++;

      while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
	rl_point++;

      if (rl_point < rl_end)
	{
	  if (isident (rl_line_buffer[rl_point]))
	    while (++rl_point < rl_end && isident (rl_line_buffer[rl_point]));
	  else
	    while (++rl_point < rl_end && !isident (rl_line_buffer[rl_point])
		   && !whitespace (rl_line_buffer[rl_point]));
	}
      rl_point--;
    }
  return (0);
}

rl_vi_insert_beg (count, key)
     int count, key;
{
  rl_beg_of_line (1, key);
  rl_vi_insertion_mode (1, key);
  return (0);
}

rl_vi_append_mode (count, key)
     int count, key;
{
  if (rl_point < rl_end)
    rl_point++;
  rl_vi_insertion_mode (1, key);
  return (0);
}

rl_vi_append_eol (count, key)
     int count, key;
{
  rl_end_of_line (1, key);
  rl_vi_append_mode (1, key);
  return (0);
}

/* What to do in the case of C-d. */
rl_vi_eof_maybe (count, c)
     int count, c;
{
  return (rl_newline (1, '\n'));
}

/* Insertion mode stuff. */

/* Switching from one mode to the other really just involves
   switching keymaps. */
rl_vi_insertion_mode (count, key)
     int count, key;
{
  _rl_keymap = vi_insertion_keymap;
  return (0);
}

void
_rl_vi_done_inserting ()
{
  if (_rl_vi_doing_insert)
    {
      rl_end_undo_group ();
      /* Now, the text between rl_undo_list->next->start and
	 rl_undo_list->next->end is what was inserted while in insert
	 mode. */
      _rl_vi_doing_insert = 0;
      vi_continued_command = 1;
    }
  else
    vi_continued_command = 0;
}

rl_vi_movement_mode (count, key)
     int count, key;
{
  if (rl_point > 0)
    rl_backward (1);

#if 0
  _rl_vi_reset_last ();
#endif

  _rl_keymap = vi_movement_keymap;
  _rl_vi_done_inserting ();
  return (0);
}

rl_vi_arg_digit (count, c)
     int count, c;
{
  if (c == '0' && rl_numeric_arg == 1 && !rl_explicit_arg)
    return (rl_beg_of_line ());
  else
    return (rl_digit_argument (count, c));
}

rl_vi_change_case (count, ignore)
     int count, ignore;
{
  char c = 0;

  /* Don't try this on an empty line. */
  if (rl_point >= rl_end)
    return (0);

  while (count-- && rl_point < rl_end)
    {
      if (uppercase_p (rl_line_buffer[rl_point]))
	c = to_lower (rl_line_buffer[rl_point]);
      else if (lowercase_p (rl_line_buffer[rl_point]))
	c = to_upper (rl_line_buffer[rl_point]);
      else
	{
	  /* Just skip over characters neither upper nor lower case. */
	  rl_forward (1);
	  continue;
	}

      /* Vi is kind of strange here. */
      if (c)
	{
	  rl_begin_undo_group ();
	  rl_delete (1, c);
	  rl_insert (1, c);
	  rl_end_undo_group ();
	  rl_vi_check ();
        }
      else
	rl_forward (1);
    }
  return (0);
}

rl_vi_put (count, key)
     int count, key;
{
  if (!uppercase_p (key) && (rl_point + 1 <= rl_end))
    rl_point++;

  rl_yank ();
  rl_backward (1);
  return (0);
}

rl_vi_check ()
{
  if (rl_point && rl_point == rl_end)
    rl_point--;
  return (0);
}

rl_vi_column (count, key)
     int count, key;
{
  if (count > rl_end)
    rl_end_of_line ();
  else
    rl_point = count - 1;
  return (0);
}

int
rl_vi_domove (key, nextkey)
     int key, *nextkey;
{
  int c, save;
  int old_end;

  rl_mark = rl_point;
  c = rl_read_key ();
  *nextkey = c;

  if (!member (c, vi_motion))
    {
      if (digit_p (c))
	{
	  save = rl_numeric_arg;
	  rl_numeric_arg = digit_value (c);
	  rl_digit_loop1 ();
	  rl_numeric_arg *= save;
	  c = rl_read_key ();	/* real command */
	  *nextkey = c;
	}
      else if (key == c && (key == 'd' || key == 'y' || key == 'c'))
	{
	  rl_mark = rl_end;
	  rl_beg_of_line ();
	  _rl_vi_last_motion = c;
	  return (0);
	}
      else
	return (-1);
    }

  _rl_vi_last_motion = c;

  /* Append a blank character temporarily so that the motion routines
     work right at the end of the line. */
  old_end = rl_end;
  rl_line_buffer[rl_end++] = ' ';
  rl_line_buffer[rl_end] = '\0';

  _rl_dispatch (c, _rl_keymap);

  /* Remove the blank that we added. */
  rl_end = old_end;
  rl_line_buffer[rl_end] = '\0';
  if (rl_point > rl_end)
    rl_point = rl_end;

  /* No change in position means the command failed. */
  if (rl_mark == rl_point)
    return (-1);

  /* rl_vi_f[wW]ord () leaves the cursor on the first character of the next
     word.  If we are not at the end of the line, and we are on a
     non-whitespace character, move back one (presumably to whitespace). */
  if ((to_upper (c) == 'W') && rl_point < rl_end && rl_point > rl_mark &&
      !whitespace (rl_line_buffer[rl_point]))
    rl_point--;

  /* If cw or cW, back up to the end of a word, so the behaviour of ce
     or cE is the actual result.  Brute-force, no subtlety. */
  if (key == 'c' && rl_point >= rl_mark && (to_upper (c) == 'W'))
    {
      /* Don't move farther back than where we started. */
      while (rl_point > rl_mark && whitespace (rl_line_buffer[rl_point]))
	rl_point--;

      /* Posix.2 says that if cw or cW moves the cursor towards the end of
	 the line, the character under the cursor should be deleted. */
      if (rl_point == rl_mark)
        rl_point++;
      else
	{
	  /* Move past the end of the word so that the kill doesn't
	     remove the last letter of the previous word.  Only do this
	     if we are not at the end of the line. */
	  if (rl_point >= 0 && rl_point < (rl_end - 1) && !whitespace (rl_line_buffer[rl_point]))
	    rl_point++;
	}
    }

  if (rl_mark < rl_point)
    exchange (rl_point, rl_mark);

  return (0);
}

/* A simplified loop for vi. Don't dispatch key at end.
   Don't recognize minus sign? */
static int
rl_digit_loop1 ()
{
  int key, c;

  while (1)
    {
      rl_message ("(arg: %d) ", rl_arg_sign * rl_numeric_arg, 0);
      key = c = rl_read_key ();

      if (_rl_keymap[c].type == ISFUNC &&
	  _rl_keymap[c].function == rl_universal_argument)
	{
	  rl_numeric_arg *= 4;
	  continue;
	}

      c = UNMETA (c);
      if (digit_p (c))
	{
	  if (rl_explicit_arg)
	    rl_numeric_arg = (rl_numeric_arg * 10) + digit_value (c);
	  else
	    rl_numeric_arg = digit_value (c);
	  rl_explicit_arg = 1;
	}
      else
	{
	  rl_clear_message ();
	  rl_stuff_char (key);
	  break;
	}
    }
  return (0);
}

rl_vi_delete_to (count, key)
     int count, key;
{
  int c;

  if (uppercase_p (key))
    rl_stuff_char ('$');
  else if (vi_redoing)
    rl_stuff_char (_rl_vi_last_motion);

  if (rl_vi_domove (key, &c))
    {
      ding ();
      return -1;
    }

  /* These are the motion commands that do not require adjusting the
     mark. */
  if ((strchr (" l|h^0bB", c) == 0) && (rl_mark < rl_end))
    rl_mark++;

  rl_kill_text (rl_point, rl_mark);
  return (0);
}

rl_vi_change_to (count, key)
     int count, key;
{
  int c, start_pos;

  if (uppercase_p (key))
    rl_stuff_char ('$');
  else if (vi_redoing)
    rl_stuff_char (_rl_vi_last_motion);

  start_pos = rl_point;

  if (rl_vi_domove (key, &c))
    {
      ding ();
      return -1;
    }

  /* These are the motion commands that do not require adjusting the
     mark.  c[wW] are handled by special-case code in rl_vi_domove(),
     and already leave the mark at the correct location. */
  if ((strchr (" l|hwW^0bB", c) == 0) && (rl_mark < rl_end))
    rl_mark++;

  /* The cursor never moves with c[wW]. */
  if ((to_upper (c) == 'W') && rl_point < start_pos)
    rl_point = start_pos;

  rl_kill_text (rl_point, rl_mark);

  rl_begin_undo_group ();
  _rl_vi_doing_insert = 1;
  _rl_vi_set_last (key, count, rl_arg_sign);
  rl_vi_insertion_mode (1, key);

  return (0);
}

rl_vi_yank_to (count, key)
     int count, key;
{
  int c, save = rl_point;

  if (uppercase_p (key))
    rl_stuff_char ('$');

  if (rl_vi_domove (key, &c))
    {
      ding ();
      return -1;
    }

  /* These are the motion commands that do not require adjusting the
     mark. */
  if ((strchr (" l|h^0%bB", c) == 0) && (rl_mark < rl_end))
    rl_mark++;

  rl_begin_undo_group ();
  rl_kill_text (rl_point, rl_mark);
  rl_end_undo_group ();
  rl_do_undo ();
  rl_point = save;

  return (0);
}

rl_vi_delete (count, key)
     int count, key;
{
  int end;

  if (rl_end == 0)
    {
      ding ();
      return -1;
    }

  end = rl_point + count;

  if (end >= rl_end)
    end = rl_end;

  rl_kill_text (rl_point, end);
  
  if (rl_point > 0 && rl_point == rl_end)
    rl_backward (1);
  return (0);
}

/* Turn the current line into a comment in shell history.
   A K*rn shell style function. */
rl_vi_comment (count, key)
     int count, key;
{
  rl_beg_of_line ();

  if (rl_vi_comment_begin != (char *)NULL)
    rl_insert_text (rl_vi_comment_begin);
  else
    rl_insert_text (VI_COMMENT_BEGIN_DEFAULT);	/* Default. */

  rl_redisplay ();
  rl_newline (1, '\n');
  return (0);
}

rl_vi_first_print (count, key)
     int count, key;
{
  return (rl_back_to_indent ());
}

rl_back_to_indent (ignore1, ignore2)
     int ignore1, ignore2;
{
  rl_beg_of_line ();
  while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
    rl_point++;
  return (0);
}

/* NOTE: it is necessary that opposite directions are inverses */
#define	FTO	 1		/* forward to */
#define BTO	-1		/* backward to */
#define FFIND	 2		/* forward find */
#define BFIND	-2		/* backward find */

rl_vi_char_search (count, key)
     int count, key;
{
  static char target;
  static int orig_dir, dir;
  int pos;

  if (key == ';' || key == ',')
    dir = (key == ';' ? orig_dir : -orig_dir);
  else
    {
      if (vi_redoing)
	target = _rl_vi_last_search_char;
      else
	_rl_vi_last_search_char = target = rl_getc (rl_instream);

      switch (key)
        {
        case 't':
          orig_dir = dir = FTO;
          break;

        case 'T':
          orig_dir = dir = BTO;
          break;

        case 'f':
          orig_dir = dir = FFIND;
          break;

        case 'F':
          orig_dir = dir = BFIND;
          break;
        }
    }

  pos = rl_point;

  while (count--)
    {
      if (dir < 0)
	{
	  if (pos == 0)
	    {
	      ding ();
	      return -1;
	    }

	  pos--;
	  do
	    {
	      if (rl_line_buffer[pos] == target)
		{
		  if (dir == BTO)
		    rl_point = pos + 1;
		  else
		    rl_point = pos;
		  break;
		}
	    }
	  while (pos--);

	  if (pos < 0)
	    {
	      ding ();
	      return -1;
	    }
	}
      else
	{			/* dir > 0 */
	  if (pos >= rl_end)
	    {
	      ding ();
	      return -1;
	    }

	  pos++;
	  do
	    {
	      if (rl_line_buffer[pos] == target)
		{
		  if (dir == FTO)
		    rl_point = pos - 1;
		  else
		    rl_point = pos;
		  break;
		}
	    }
	  while (++pos < rl_end);

	  if (pos >= (rl_end - 1))
	    {
	      ding ();
	      return -1;
	    }
	}
    }
  return (0);
}

/* Match brackets */
rl_vi_match (ignore, key)
     int ignore, key;
{
  int count = 1, brack, pos;

  pos = rl_point;
  if ((brack = rl_vi_bracktype (rl_line_buffer[rl_point])) == 0)
    {
      while ((brack = rl_vi_bracktype (rl_line_buffer[rl_point])) == 0 &&
	     rl_point < rl_end - 1)
	rl_forward (1);

      if (brack <= 0)
	{
	  rl_point = pos;
	  ding ();
	  return -1;
	}
    }

  pos = rl_point;

  if (brack < 0)
    {
      while (count)
	{
	  if (--pos >= 0)
	    {
	      int b = rl_vi_bracktype (rl_line_buffer[pos]);
	      if (b == -brack)
		count--;
	      else if (b == brack)
		count++;
	    }
	  else
	    {
	      ding ();
	      return -1;
	    }
	}
    }
  else
    {			/* brack > 0 */
      while (count)
	{
	  if (++pos < rl_end)
	    {
	      int b = rl_vi_bracktype (rl_line_buffer[pos]);
	      if (b == -brack)
		count--;
	      else if (b == brack)
		count++;
	    }
	  else
	    {
	      ding ();
	      return -1;
	    }
	}
    }
  rl_point = pos;
  return (0);
}

int
rl_vi_bracktype (c)
     int c;
{
  switch (c)
    {
    case '(': return  1;
    case ')': return -1;
    case '[': return  2;
    case ']': return -2;
    case '{': return  3;
    case '}': return -3;
    default:  return  0;
    }
}

rl_vi_change_char (count, key)
     int count, key;
{
  int c;

  if (vi_redoing)
    c = _rl_vi_last_replacement;
  else
    _rl_vi_last_replacement = c = rl_getc (rl_instream);

  if (c == '\033' || c == CTRL ('C'))
    return -1;

  while (count-- && rl_point < rl_end)
    {
      rl_begin_undo_group ();

      rl_delete (1, c);
      rl_insert (1, c);
      if (count == 0)
	rl_backward (1);

      rl_end_undo_group ();
    }
  return (0);
}

rl_vi_subst (count, key)
     int count, key;
{
  rl_begin_undo_group ();

  if (uppercase_p (key))
    {
      rl_beg_of_line ();
      rl_kill_line (1);
    }
  else
    rl_delete_text (rl_point, rl_point+count);

  rl_end_undo_group ();

  _rl_vi_set_last (key, count, rl_arg_sign);

  rl_begin_undo_group ();
  _rl_vi_doing_insert = 1;
  rl_vi_insertion_mode (1, key);

  return (0);
}

rl_vi_overstrike (count, key)
     int count, key;
{
  int i;

  if (_rl_vi_doing_insert == 0)
    {
      _rl_vi_doing_insert = 1;
      rl_begin_undo_group ();
    }

  for (i = 0; i < count; i++)
    {
      vi_replace_count++;
      rl_begin_undo_group ();

      if (rl_point < rl_end)
	{
	  rl_delete (1, key);
	  rl_insert (1, key);
	}
      else
	rl_insert (1, key);

      rl_end_undo_group ();
    }
  return (0);
}

rl_vi_overstrike_delete (count)
     int count;
{
  int i, s;

  for (i = 0; i < count; i++)
    {
      if (vi_replace_count == 0)
	{
	  ding ();
	  break;
	}
      s = rl_point;

      if (rl_do_undo ())
	vi_replace_count--;

      if (rl_point == s)
	rl_backward (1);
    }

  if (vi_replace_count == 0 && _rl_vi_doing_insert)
    {
      rl_end_undo_group ();
      rl_do_undo ();
      _rl_vi_doing_insert = 0;
    }
  return (0);
}

rl_vi_replace (count, key)
     int count, key;
{
  int i;

  vi_replace_count = 0;

  if (!vi_replace_map)
    {
      vi_replace_map = rl_make_bare_keymap ();

      for (i = ' '; i < KEYMAP_SIZE; i++)
	vi_replace_map[i].function = rl_vi_overstrike;

      vi_replace_map[RUBOUT].function = rl_vi_overstrike_delete;
      vi_replace_map[ESC].function = rl_vi_movement_mode;
      vi_replace_map[RETURN].function = rl_newline;
      vi_replace_map[NEWLINE].function = rl_newline;

      /* If the normal vi insertion keymap has ^H bound to erase, do the
         same here.  Probably should remove the assignment to RUBOUT up
         there, but I don't think it will make a difference in real life. */
      if (vi_insertion_keymap[CTRL ('H')].type == ISFUNC &&
	  vi_insertion_keymap[CTRL ('H')].function == rl_rubout)
	vi_replace_map[CTRL ('H')].function = rl_vi_overstrike_delete;

    }
  _rl_keymap = vi_replace_map;
  return (0);
}

#if 0
/* Try to complete the word we are standing on or the word that ends with
   the previous character.  A space matches everything.  Word delimiters are
   space and ;. */
rl_vi_possible_completions()
{
  int save_pos = rl_point;

  if (rl_line_buffer[rl_point] != ' ' && rl_line_buffer[rl_point] != ';')
    {
      while (rl_point < rl_end && rl_line_buffer[rl_point] != ' ' &&
	     rl_line_buffer[rl_point] != ';')
	rl_point++;
    }
  else if (rl_line_buffer[rl_point - 1] == ';')
    {
      ding ();
      return (0);
    }

  rl_possible_completions ();
  rl_point = save_pos;

  return (0);
}
#endif

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
  fprintf (stderr, "readline: Out of virtual memory!\n");
  abort ();
}
#endif /* STATIC_MALLOC */

#endif /* VI_MODE */
