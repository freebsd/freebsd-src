/* vi_mode.c -- A vi emulation mode for Bash.
   Derived from code written by Jeff Sparkes (jsparkes@bnr.ca).  */

/* Copyright (C) 1988, 1991 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library (the Library), a set of
   routines for providing Emacs style line input to programs that ask
   for it.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   The Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */


/* **************************************************************** */
/*								    */
/*			VI Emulation Mode			    */
/*								    */
/* **************************************************************** */
#if defined (VI_MODE)

/* Some standard library routines. */
#include "sysdep.h"
#include <stdio.h>
#include "readline.h"
#include "history.h"

#ifndef digit
#define digit(c)  ((c) >= '0' && (c) <= '9')
#endif

#ifndef isletter
#define isletter(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#endif

#ifndef digit_value
#define digit_value(c) ((c) - '0')
#endif

#ifndef member
#define member(c, s) ((c) ? index ((s), (c)) : 0)
#endif

#ifndef isident
#define isident(c) ((isletter(c) || digit(c) || c == '_'))
#endif

#ifndef exchange
#define exchange(x, y) {int temp = x; x = y; y = temp;}
#endif

/* Variables imported from readline.c */
extern int rl_point, rl_end, rl_mark, rl_done;
extern FILE *rl_instream;
extern int rl_line_buffer_len, rl_explicit_arg, rl_numeric_arg;
extern Keymap keymap;
extern char *rl_prompt;
extern char *rl_line_buffer;
extern int rl_arg_sign;

extern char *xmalloc (), *xrealloc ();
extern void rl_extend_line_buffer ();

/* Last string searched for from `/' or `?'. */
static char *vi_last_search = (char *)NULL;
static int vi_histpos;

/* Non-zero means enter insertion mode. */
int vi_doing_insert = 0;

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

/* Yank the nth arg from the previous line into this line at point. */
rl_vi_yank_arg (count)
     int count;
{
  /* Readline thinks that the first word on a line is the 0th, while vi
     thinks the first word on a line is the 1st.  Compensate. */
  if (rl_explicit_arg)
    rl_yank_nth_arg (count - 1, 0);
  else
    rl_yank_nth_arg ('$', 0);
}

/* With an argument, move back that many history lines, else move to the
   beginning of history. */
rl_vi_fetch_history (count, c)
     int count, c;
{
  extern int rl_explicit_arg;
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
}

/* Search again for the last thing searched for. */
rl_vi_search_again (ignore, key)
     int ignore, key;
{
  switch (key)
    {
    case 'n':
      rl_vi_dosearch (vi_last_search, -1);
      break;

    case 'N':
      rl_vi_dosearch (vi_last_search, 1);
      break;
    }
}

/* Do a vi style search. */
rl_vi_search (count, key)
     int count, key;
{
  int dir, c, save_pos;
  char *p;

  switch (key)
    {
    case '?':
      dir = 1;
      break;

    case '/':
      dir = -1;
      break;

    default:
      ding ();
      return;
    }

  vi_histpos = where_history ();
  maybe_save_line ();
  save_pos = rl_point;

  /* Reuse the line input buffer to read the search string. */
  rl_line_buffer[0] = 0;
  rl_end = rl_point = 0;
  p = (char *)alloca (2 + (rl_prompt ? strlen (rl_prompt) : 0));

  sprintf (p, "%s%c", rl_prompt ? rl_prompt : "", key);

  rl_message (p, 0, 0);

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
	      rl_point = save_pos;
	      return;
	    }

	case CTRL('W'):
	case CTRL('U'):
	  rl_dispatch (c, keymap);
	  break;

	case ESC:
	case RETURN:
	case NEWLINE:
	  goto dosearch;
	  break;

	case CTRL('C'):
	  maybe_unsave_line ();
	  rl_clear_message ();
	  rl_point = 0;
	  ding ();
	  return;

	default:
	  rl_insert (1, c);
	  break;
	}
      rl_redisplay ();
    }
 dosearch:
  if (vi_last_search)
    free (vi_last_search);

  vi_last_search = savestring (rl_line_buffer);
  rl_vi_dosearch (rl_line_buffer, dir);
}

/* Search for STRING in the history list.  DIR is < 0 for searching
   backwards.  POS is an absolute index into the history list at
   which point to begin searching.  If the first character of STRING
   is `^', the string must match a prefix of a history line, otherwise
   a full substring match is performed. */
static int
vi_history_search_pos (string, dir, pos)
     char *string;
     int dir, pos;
{
  int ret, old = where_history ();

  history_set_pos (pos);

  if (*string == '^')
    ret = history_search_prefix (string + 1, dir);
  else
    ret = history_search (string, dir);
    
  if (ret == -1)
    {
      history_set_pos (old);
      return (-1);
    }

  ret = where_history ();
  history_set_pos (old);
  return ret;
}

rl_vi_dosearch (string, dir)
     char *string;
     int dir;
{
  int old, save = vi_histpos;
  HIST_ENTRY *h;

  if (string == 0 || *string == 0 || vi_histpos < 0)
    {
      ding ();
      return;
    }

  if ((save = vi_history_search_pos (string, dir, vi_histpos + dir)) == -1)
    {
      maybe_unsave_line ();
      rl_clear_message ();
      rl_point = 0;
      ding ();
      return;
    }

  vi_histpos = save;

  old = where_history ();
  history_set_pos (vi_histpos);
  h = current_history ();
  history_set_pos (old);

  {
    int line_len = strlen (h->line);

    if (line_len >= rl_line_buffer_len)
      rl_extend_line_buffer (line_len);
    strcpy (rl_line_buffer, h->line);
  }

  rl_undo_list = (UNDO_LIST *)h->data;
  rl_end = strlen (rl_line_buffer);
  rl_point = 0;
  rl_clear_message ();
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
}

/* Previous word in vi mode. */
rl_vi_prev_word (count, key)
     int count, key;
{
  if (count < 0)
    {
      rl_vi_next_word (-count, key);
      return;
    }

  if (rl_point == 0)
    {
      ding ();
      return;
    }

  if (uppercase_p (key))
    rl_vi_bWord (count);
  else
    rl_vi_bword (count);
}

/* Next word in vi mode. */
rl_vi_next_word (count, key)
     int count;
{
  if (count < 0)
    {
      rl_vi_prev_word (-count, key);
      return;
    }

  if (rl_point >= (rl_end - 1))
    {
      ding ();
      return;
    }

  if (uppercase_p (key))
    rl_vi_fWord (count);
  else
    rl_vi_fword (count);
}

/* Move to the end of the ?next? word. */
rl_vi_end_word (count, key)
     int count, key;
{
  if (count < 0)
    {
      ding ();
      return;
    }

  if (uppercase_p (key))
    rl_vi_eWord (count);
  else
    rl_vi_eword (count);
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
}

rl_vi_insert_beg ()
{
  rl_beg_of_line ();
  rl_vi_insertion_mode ();
  return 0;
}

rl_vi_append_mode ()
{
  if (rl_point < rl_end)
    rl_point += 1;
  rl_vi_insertion_mode ();
  return 0;
}

rl_vi_append_eol ()
{
  rl_end_of_line ();
  rl_vi_append_mode ();
  return 0;
}

/* What to do in the case of C-d. */
rl_vi_eof_maybe (count, c)
     int count, c;
{
  rl_newline (1, '\n');
}

/* Insertion mode stuff. */

/* Switching from one mode to the other really just involves
   switching keymaps. */
rl_vi_insertion_mode ()
{
  keymap = vi_insertion_keymap;
}

rl_vi_movement_mode ()
{
  if (rl_point > 0)
    rl_backward (1);

  keymap = vi_movement_keymap;
  vi_done_inserting ();
}

vi_done_inserting ()
{
  if (vi_doing_insert)
    {
      rl_end_undo_group ();
      vi_doing_insert = 0;
    }
}

rl_vi_arg_digit (count, c)
     int count, c;
{
  if (c == '0' && rl_numeric_arg == 1 && !rl_explicit_arg)
    rl_beg_of_line ();
  else
    rl_digit_argument (count, c);
}

rl_vi_change_case (count, ignore)
     int count, ignore;
{
  char c = 0;

  /* Don't try this on an empty line. */
  if (rl_point >= rl_end)
    return;

  while (count-- && rl_point < rl_end)
    {
      if (uppercase_p (rl_line_buffer[rl_point]))
	c = to_lower (rl_line_buffer[rl_point]);
      else if (lowercase_p (rl_line_buffer[rl_point]))
	c = to_upper (rl_line_buffer[rl_point]);

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
}

rl_vi_put (count, key)
     int count, key;
{
  if (!uppercase_p (key) && (rl_point + 1 <= rl_end))
    rl_point++;

  rl_yank ();
  rl_backward (1);
}

rl_vi_check ()
{
  if (rl_point && rl_point == rl_end)
    rl_point--;
}

rl_vi_column (count)
{
  if (count > rl_end)
    rl_end_of_line ();
  else
    rl_point = count - 1;
}

int
rl_vi_domove (key, nextkey)
     int key, *nextkey;
{
  int c, save;
  int old_end, added_blank;

  added_blank = 0;

  rl_mark = rl_point;
  c = rl_read_key ();
  *nextkey = c;

  if (!member (c, vi_motion))
    {
      if (digit (c))
	{
	  save = rl_numeric_arg;
	  rl_numeric_arg = digit_value (c);
	  rl_digit_loop1 ();
	  rl_numeric_arg *= save;
	  c = rl_read_key ();	/* real command */
	  *nextkey = c;
	}
      else if ((key == 'd' && c == 'd') ||
	       (key == 'y' && c == 'y') ||
	       (key == 'c' && c == 'c'))
	{
	  rl_mark = rl_end;
	  rl_beg_of_line ();
	  return (0);
	}
      else
	return (-1);
    }

  /* Append a blank character temporarily so that the motion routines
     work right at the end of the line. */
  old_end = rl_end;
  rl_line_buffer[rl_end++] = ' ';	/* This looks pretty bogus to me??? */
  rl_line_buffer[rl_end] = '\0';
  added_blank++;

  rl_dispatch (c, keymap);

  /* Remove the blank that we added. */
  rl_end = old_end;
  rl_line_buffer[rl_end] = '\0';
  if (rl_point > rl_end)
    rl_point = rl_end - 1;

  /* No change in position means the command failed. */
  if (rl_mark == rl_point)
    return (-1);

  /* rl_vi_f[wW]ord () leaves the cursor on the first character of the next
     word.  If we are not at the end of the line, and we are on a
     non-whitespace character, move back one (presumably to whitespace). */
  if ((c == 'w' || c == 'W') && (rl_point < rl_end) &&
      !whitespace (rl_line_buffer[rl_point]))
    rl_point--;

  /* If cw or cW, back up to the end of a word, so the behaviour of ce
     or cE is the actual result.  Brute-force, no subtlety.  Do the same
     thing for dw or dW. */
  if (key == 'c' && (to_upper (c) == 'W'))
    {
      while (rl_point && whitespace (rl_line_buffer[rl_point]))
	rl_point--;
    }

  if (rl_mark < rl_point)
    exchange (rl_point, rl_mark);

  return (0);
}

/* A simplified loop for vi. Don't dispatch key at end.
   Don't recognize minus sign? */
rl_digit_loop1 ()
{
  int key, c;

  while (1)
    {
      rl_message ("(arg: %d) ", rl_arg_sign * rl_numeric_arg, 0);
      key = c = rl_read_key ();

      if (keymap[c].type == ISFUNC &&
	  keymap[c].function == rl_universal_argument)
	{
	  rl_numeric_arg *= 4;
	  continue;
	}

      c = UNMETA (c);
      if (numeric (c))
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
}

rl_vi_delete_to (count, key)
     int count, key;
{
  int c;

  if (uppercase_p (key))
    rl_stuff_char ('$');

  if (rl_vi_domove (key, &c))
    {
      ding ();
      return;
    }

  if ((c != 'l') && (c != '|') && (c != 'h') && rl_mark < rl_end)
    rl_mark++;

  rl_kill_text (rl_point, rl_mark);
}

rl_vi_change_to (count, key)
     int count, key;
{
  int c;

  if (uppercase_p (key))
    rl_stuff_char ('$');

  if (rl_vi_domove (key, &c))
    {
      ding ();
      return;
    }

  if ((c != 'l') && (c != '|') && (c != 'h') && rl_mark < rl_end)
    rl_mark++;

  rl_begin_undo_group ();
  vi_doing_insert = 1;
  rl_kill_text (rl_point, rl_mark);
  rl_vi_insertion_mode ();
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
      return;
    }

  if ((c != 'l') && (c != '|') && (c != 'h') && rl_mark < rl_end)
    rl_mark++;

  rl_begin_undo_group ();
  rl_kill_text (rl_point, rl_mark);
  rl_end_undo_group ();
  rl_do_undo ();
  rl_point = save;
}

rl_vi_delete (count)
{
  int end;

  if (rl_end == 0)
    {
      ding ();
      return;
    }

  end = rl_point + count;

  if (end >= rl_end)
    end = rl_end;

  rl_kill_text (rl_point, end);
  
  if (rl_point > 0 && rl_point == rl_end)
    rl_backward (1);
}

/* Turn the current line into a comment in shell history.
   A K*rn shell style function. */
rl_vi_comment ()
{
  rl_beg_of_line ();

  if (rl_vi_comment_begin != (char *)NULL)
    rl_insert_text (rl_vi_comment_begin);
  else
    rl_insert_text (": ");	/* Default. */

  rl_redisplay ();
  rl_newline (1, '\010');
}

rl_vi_first_print ()
{
  rl_back_to_indent (0, 0);
}

rl_back_to_indent (ignore1, ignore2)
     int ignore1, ignore2;
{
  rl_beg_of_line ();
  while (rl_point < rl_end && whitespace (rl_line_buffer[rl_point]))
    rl_point++;
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
      target = rl_getc (rl_instream);

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
	      return;
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
	      return;
	    }
	}
      else
	{			/* dir > 0 */
	  if (pos >= rl_end)
	    {
	      ding ();
	      return;
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
	      return;
	    }
	}
    }
}

/* Match brackets */
rl_vi_match ()
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
	  return;
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
	      return;
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
	      return;
	    }
	}
    }
  rl_point = pos;
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

  c = rl_getc (rl_instream);

  if (c == '\033' || c == CTRL ('C'))
    return;

  while (count-- && rl_point < rl_end)
    {
      rl_begin_undo_group ();

      rl_delete (1, c);
      rl_insert (1, c);
      if (count == 0)
	rl_backward (1);

      rl_end_undo_group ();
    }
}

rl_vi_subst (count, key)
     int count, key;
{
  rl_begin_undo_group ();
  vi_doing_insert = 1;

  if (uppercase_p (key))
    {
      rl_beg_of_line ();
      rl_kill_line (1);
    }
  else
    rl_delete (count, key);

  rl_end_undo_group ();
  rl_vi_insertion_mode ();
}

rl_vi_overstrike (count, key)
     int count, key;
{
  int i;

  if (vi_doing_insert == 0)
    {
      vi_doing_insert = 1;
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

  if (vi_replace_count == 0 && vi_doing_insert)
    {
      rl_end_undo_group ();
      rl_do_undo ();
      vi_doing_insert = 0;
    }
}

rl_vi_replace (count, key)
     int count, key;
{
  int i;

  vi_replace_count = 0;

  if (!vi_replace_map)
    {
      vi_replace_map = rl_make_bare_keymap ();

      for (i = ' '; i < 127; i++)
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
  keymap = vi_replace_map;
}

/*
 * Try to complete the word we are standing on or the word that ends with
 * the previous character. A space matches everything.
 * Word delimiters are space and ;.
 */
rl_vi_possible_completions()
{
  int save_pos = rl_point;

  if (!index (" ;", rl_line_buffer[rl_point]))
    {
      while (!index(" ;", rl_line_buffer[++rl_point]))
	;
    }
  else if (rl_line_buffer[rl_point-1] == ';')
    {
      ding ();
      return (0);
    }

  rl_possible_completions ();
  rl_point = save_pos;

  return (0);
}

#endif /* VI_MODE */
