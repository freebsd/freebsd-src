/* $FreeBSD$ */
/* infomap.c -- keymaps for Info.
   $Id: infomap.c,v 1.28 2002/02/08 23:02:53 karl Exp $

   Copyright (C) 1993, 97, 98, 99, 2001, 02 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#include "info.h"
#include "infomap.h"
#include "funs.h"
#include "terminal.h"

#if defined(INFOKEY)
#include "infokey.h"
#include "variables.h"
#endif /* INFOKEY */

/* Return a new keymap which has all the uppercase letters mapped to run
   the function info_do_lowercase_version (). */
Keymap
keymap_make_keymap ()
{
  int i;
  Keymap keymap;

  keymap = (Keymap)xmalloc (256 * sizeof (KEYMAP_ENTRY));

  for (i = 0; i < 256; i++)
    {
      keymap[i].type = ISFUNC;
      keymap[i].function = (InfoCommand *)NULL;
    }

  for (i = 'A'; i < ('Z' + 1); i++)
    {
      keymap[i].type = ISFUNC;
#if defined(INFOKEY)
      keymap[Meta(i)].type = ISFUNC;
      keymap[Meta(i)].function =
#endif /* INFOKEY */
      keymap[i].function = InfoCmd(info_do_lowercase_version);
    }

  return (keymap);
}

/* Return a new keymap which is a copy of MAP. */
Keymap
keymap_copy_keymap (map)
     Keymap map;
{
  int i;
  Keymap keymap;

  keymap = keymap_make_keymap ();

  for (i = 0; i < 256; i++)
    {
      keymap[i].type = map[i].type;
      keymap[i].function = map[i].function;
    }
  return (keymap);
}

/* Free the keymap and its descendants. */
void
keymap_discard_keymap (map)
     Keymap (map);
{
  int i;

  if (!map)
    return;

  for (i = 0; i < 256; i++)
    {
      switch (map[i].type)
        {
        case ISFUNC:
          break;

        case ISKMAP:
          keymap_discard_keymap ((Keymap)map[i].function);
          break;

        }
    }
}

/* Conditionally bind key sequence. */
int
keymap_bind_keyseq (map, keyseq, keyentry)
     Keymap map;
     const unsigned char *keyseq;
     KEYMAP_ENTRY *keyentry;
{
  Keymap m = map;
  const unsigned char *s = keyseq;
  int c;

  if (s == NULL || *s == '\0') return 0;

  while ((c = *s++) != '\0')
    {
      switch (m[c].type)
        {
        case ISFUNC:
          if (!(m[c].function == NULL || (
#if !defined(INFOKEY)
                m != map &&
#endif /* !INFOKEY */
                m[c].function == InfoCmd(info_do_lowercase_version))
	      ))
            return 0;

          if (*s != '\0')
            {
              m[c].type = ISKMAP;
              m[c].function = (InfoCommand *)keymap_make_keymap ();
            }
          break;

        case ISKMAP:
          if (*s == '\0')
            return 0;
          break;
        }
      if (*s != '\0')
        {
          m = (Keymap)m[c].function;
        }
      else
        {
#if defined(INFOKEY)
	  FUNCTION_KEYSEQ *k;

	  for (k = keyentry->function->keys; k && k->map != map; k = k->next)
	    ;
	  if (!k)
	    {
	      FUNCTION_KEYSEQ *ks = (FUNCTION_KEYSEQ *)xmalloc (sizeof(FUNCTION_KEYSEQ));
	      ks->next = keyentry->function->keys;
	      ks->map = map;
	      ks->keyseq = xstrdup (keyseq);
	      keyentry->function->keys = ks;
	    }
#endif /* INFOKEY */
          m[c] = *keyentry;
        }
    }

  return 1;
}

/* Initialize the standard info keymaps. */

Keymap info_keymap = NULL;
Keymap echo_area_keymap = NULL;

#if !defined(INFOKEY)

static void
initialize_emacs_like_keymaps ()
{
  int i;
  Keymap map;

  if (!info_keymap)
    {
      info_keymap = keymap_make_keymap ();
      echo_area_keymap = keymap_make_keymap ();
    }

  info_keymap[ESC].type = ISKMAP;
  info_keymap[ESC].function = (InfoCommand *)keymap_make_keymap ();
  info_keymap[Control ('x')].type = ISKMAP;
  info_keymap[Control ('x')].function = (InfoCommand *)keymap_make_keymap ();

  /* Bind the echo area insert routines.  Let's make all characters
     insertable by default, regardless of which character set we might
     be using.  */
  for (i = 0; i < 256; i++)
    echo_area_keymap[i].function = ea_insert;

  echo_area_keymap[ESC].type = ISKMAP;
  echo_area_keymap[ESC].function = (InfoCommand *) keymap_make_keymap ();
  echo_area_keymap[Control ('x')].type = ISKMAP;
  echo_area_keymap[Control ('x')].function
    = (InfoCommand *) keymap_make_keymap ();

  /* Bind numeric arg functions for both echo area and info window maps. */
  for (i = '0'; i < '9' + 1; i++)
    {
      ((Keymap) info_keymap[ESC].function)[i].function
        = ((Keymap) echo_area_keymap[ESC].function)[i].function
        = info_add_digit_to_numeric_arg;
    }
  ((Keymap) info_keymap[ESC].function)['-'].function =
    ((Keymap) echo_area_keymap[ESC].function)['-'].function =
      info_add_digit_to_numeric_arg;

  info_keymap['-'].function = info_add_digit_to_numeric_arg;

  /* Bind the echo area routines. */
  map = echo_area_keymap;

  map[Control ('a')].function = ea_beg_of_line;
  map[Control ('b')].function = ea_backward;
  map[Control ('d')].function = ea_delete;
  map[Control ('e')].function = ea_end_of_line;
  map[Control ('f')].function = ea_forward;
  map[Control ('g')].function = ea_abort;
  map[Control ('h')].function = ea_rubout;
  map[Control ('k')].function = ea_kill_line;
  map[Control ('l')].function = info_redraw_display;
  map[Control ('q')].function = ea_quoted_insert;
  map[Control ('t')].function = ea_transpose_chars;
  map[Control ('u')].function = info_universal_argument;
  map[Control ('y')].function = ea_yank;

  map[LFD].function = ea_newline;
  map[RET].function = ea_newline;
  map[SPC].function = ea_complete;
  map[TAB].function = ea_complete;
  map['?'].function = ea_possible_completions;
#ifdef __MSDOS__
  /* PC users will lynch me if I don't give them their usual DEL effect...  */
  map[DEL].function = ea_delete;
#else
  map[DEL].function = ea_rubout;
#endif

  /* Bind the echo area ESC keymap. */
  map = (Keymap)echo_area_keymap[ESC].function;

  map[Control ('g')].function = ea_abort;
  map[Control ('v')].function = ea_scroll_completions_window;
  map['b'].function = ea_backward_word;
  map['d'].function = ea_kill_word;
  map['f'].function = ea_forward_word;
#if defined (NAMED_FUNCTIONS)
  /* map['x'].function = info_execute_command; */
#endif /* NAMED_FUNCTIONS */
  map['y'].function = ea_yank_pop;
  map['?'].function = ea_possible_completions;
  map[TAB].function = ea_tab_insert;
  map[DEL].function = ea_backward_kill_word;

  /* Bind the echo area Control-x keymap. */
  map = (Keymap)echo_area_keymap[Control ('x')].function;

  map['o'].function = info_next_window;
  map[DEL].function = ea_backward_kill_line;

  /* Arrow key bindings for echo area keymaps.  It seems that some
     terminals do not match their termcap entries, so it's best to just
     define everything with both of the usual prefixes.  */
  map = echo_area_keymap;
  keymap_bind_keyseq (map, term_ku, &map[Control ('p')]); /* up */
  keymap_bind_keyseq (map, "\033OA", &map[Control ('p')]);
  keymap_bind_keyseq (map, "\033[A", &map[Control ('p')]);
  keymap_bind_keyseq (map, term_kd, &map[Control ('n')]); /* down */
  keymap_bind_keyseq (map, "\033OB", &map[Control ('n')]);
  keymap_bind_keyseq (map, "\033[B", &map[Control ('n')]);
  keymap_bind_keyseq (map, term_kr, &map[Control ('f')]); /* right */
  keymap_bind_keyseq (map, "\033OC", &map[Control ('f')]);
  keymap_bind_keyseq (map, "\033[C", &map[Control ('f')]);
  keymap_bind_keyseq (map, term_kl, &map[Control ('b')]); /* left */
  keymap_bind_keyseq (map, "\033OD", &map[Control ('b')]);
  keymap_bind_keyseq (map, "\033[D", &map[Control ('b')]);
  keymap_bind_keyseq (map, term_kD, &map[DEL]);	/* delete */
  keymap_bind_keyseq (map, term_kh, &map[Control ('a')]); /* home */
  keymap_bind_keyseq (map, term_ke, &map[Control ('e')]); /* end */

  map = (Keymap)echo_area_keymap[ESC].function;
  keymap_bind_keyseq (map, term_kl, &map['b']); /* left */
  keymap_bind_keyseq (map, "\033OA", &map['b']);
  keymap_bind_keyseq (map, "\033[A", &map['b']);
  keymap_bind_keyseq (map, term_kr, &map['f']); /* right */
  keymap_bind_keyseq (map, "\033OB", &map['f']);
  keymap_bind_keyseq (map, "\033[B", &map['f']);
  keymap_bind_keyseq (map, term_kD, &map[DEL]);	/* delete */

  map = (Keymap)echo_area_keymap[Control ('x')].function;
  keymap_bind_keyseq (map, term_kD, &map[DEL]);	/* delete */

  /* Bind commands for Info window keymaps. */
  map = info_keymap;
  map[TAB].function = info_move_to_next_xref;
  map[LFD].function = info_select_reference_this_line;
  map[RET].function = info_select_reference_this_line;
  map[SPC].function = info_scroll_forward;
  map[Control ('a')].function = info_beginning_of_line;
  map[Control ('b')].function = info_backward_char;
  map[Control ('e')].function = info_end_of_line;
  map[Control ('f')].function = info_forward_char;
  map[Control ('g')].function = info_abort_key;
  map[Control ('h')].function = info_get_help_window;
  map[Control ('l')].function = info_redraw_display;
  map[Control ('n')].function = info_next_line;
  map[Control ('p')].function = info_prev_line;
  map[Control ('r')].function = isearch_backward;
  map[Control ('s')].function = isearch_forward;
  map[Control ('u')].function = info_universal_argument;
  map[Control ('v')].function = info_scroll_forward_page_only;
  map[','].function = info_next_index_match;
  map['/'].function = info_search;

  for (i = '1'; i < '9' + 1; i++)
    map[i].function = info_menu_digit;
  map['0'].function = info_last_menu_item;

  map['<'].function = info_first_node;
  map['>'].function = info_last_node;
  map['?'].function = info_get_help_window;
  map['['].function = info_global_prev_node;
  map[']'].function = info_global_next_node;

  map['b'].function = info_beginning_of_node;
  map['d'].function = info_dir_node;
  map['e'].function = info_end_of_node;
  map['f'].function = info_xref_item;
  map['g'].function = info_goto_node;
  map['G'].function = info_menu_sequence;
  map['h'].function = info_get_info_help_node;
  map['i'].function = info_index_search;
  map['I'].function = info_goto_invocation_node;
  map['l'].function = info_history_node;
  map['m'].function = info_menu_item;
  map['n'].function = info_next_node;
  map['O'].function = info_goto_invocation_node;
  map['p'].function = info_prev_node;
  map['q'].function = info_quit;
  map['r'].function = info_xref_item;
  map['s'].function = info_search;
  map['S'].function = info_search_case_sensitively;
  map['t'].function = info_top_node;
  map['u'].function = info_up_node;
  map[DEL].function = info_scroll_backward;

  /* Bind members in the ESC map for Info windows. */
  map = (Keymap)info_keymap[ESC].function;
  map[Control ('f')].function = info_show_footnotes;
  map[Control ('g')].function = info_abort_key;
  map[TAB].function = info_move_to_prev_xref;
  map[Control ('v')].function = info_scroll_other_window;
  map['<'].function = info_beginning_of_node;
  map['>'].function = info_end_of_node;
  map['b'].function = info_backward_word;
  map['f'].function = info_forward_word;
  map['r'].function = info_move_to_window_line;
  map['v'].function = info_scroll_backward_page_only;
#if defined (NAMED_FUNCTIONS)
  map['x'].function = info_execute_command;
#endif /* NAMED_FUNCTIONS */
  map[DEL].function = info_scroll_other_window_backward;

  /* Bind members in the Control-X map for Info windows. */
  map = (Keymap)info_keymap[Control ('x')].function;

  map[Control ('b')].function = list_visited_nodes;
  map[Control ('c')].function = info_quit;
  map[Control ('f')].function = info_view_file;
  map[Control ('g')].function = info_abort_key;
  map[Control ('v')].function = info_view_file;
  map['0'].function = info_delete_window;
  map['1'].function = info_keep_one_window;
  map['2'].function = info_split_window;
  map['^'].function = info_grow_window;
  map['b'].function = select_visited_node;
  map['k'].function = info_kill_node;
  map['n'].function = info_search_next;
  map['N'].function = info_search_previous;
  map['o'].function = info_next_window;
  map['t'].function = info_tile_windows;
  map['w'].function = info_toggle_wrap;

  /* Arrow key bindings for Info windows keymap. */
  map = info_keymap;
  keymap_bind_keyseq (map, term_kN, &map[Control ('v')]); /* pagedown */
  keymap_bind_keyseq (map, term_ku, &map[Control ('p')]); /* up */
  keymap_bind_keyseq (map, "\033OA", &map[Control ('p')]);
  keymap_bind_keyseq (map, "\033[A", &map[Control ('p')]);
  keymap_bind_keyseq (map, term_kd, &map[Control ('n')]); /* down */
  keymap_bind_keyseq (map, "\033OB", &map[Control ('n')]);
  keymap_bind_keyseq (map, "\033[B", &map[Control ('n')]);
  keymap_bind_keyseq (map, term_kr, &map[Control ('f')]); /* right */
  keymap_bind_keyseq (map, "\033OC", &map[Control ('f')]);
  keymap_bind_keyseq (map, "\033[C", &map[Control ('f')]);
  keymap_bind_keyseq (map, term_kl, &map[Control ('b')]); /* left */
  keymap_bind_keyseq (map, "\033OD", &map[Control ('b')]);
  keymap_bind_keyseq (map, "\033[D", &map[Control ('b')]);
  keymap_bind_keyseq (map, term_kh, &map['b']);	/* home */
  keymap_bind_keyseq (map, term_ke, &map['e']);	/* end */
  keymap_bind_keyseq (map, term_kD, &map[DEL]);	/* delete */

  map = (Keymap)info_keymap[ESC].function;
  keymap_bind_keyseq (map, term_kl, &map['b']); /* left */
  keymap_bind_keyseq (map, "\033OA", &map['b']);
  keymap_bind_keyseq (map, "\033[A", &map['b']);
  keymap_bind_keyseq (map, term_kr, &map['f']); /* right */
  keymap_bind_keyseq (map, "\033OB", &map['f']);
  keymap_bind_keyseq (map, "\033[B", &map['f']);
  keymap_bind_keyseq (map, term_kN, &map[Control ('v')]); /* pagedown */
  keymap_bind_keyseq (map, term_kP, &map[DEL]); /* pageup */
  keymap_bind_keyseq (map, term_kD, &map[DEL]);	/* delete */

  /* The alternative to this definition of a `main map' key in the
     `ESC map' section, is something like:
    keymap_bind_keyseq (map, term_kP, &((KeyMap)map[ESC].function).map['v']);
  */
  keymap_bind_keyseq (info_keymap/*sic*/, term_kP, &map['v']); /* pageup */
}

static void
initialize_vi_like_keymaps ()
{
  int i;
  Keymap map;

  if (!info_keymap)
    {
      info_keymap = keymap_make_keymap ();
      echo_area_keymap = keymap_make_keymap ();
    }

  info_keymap[ESC].type = ISKMAP;
  info_keymap[ESC].function = (InfoCommand *)keymap_make_keymap ();
  info_keymap[Control ('x')].type = ISKMAP;
  info_keymap[Control ('x')].function = (InfoCommand *)keymap_make_keymap ();

  /* Bind the echo area insert routines. */
  for (i = 0; i < 256; i++)
    echo_area_keymap[i].function = ea_insert;

  echo_area_keymap[ESC].type = ISKMAP;
  echo_area_keymap[ESC].function = (InfoCommand *)keymap_make_keymap ();
  echo_area_keymap[Control ('x')].type = ISKMAP;
  echo_area_keymap[Control ('x')].function =
    (InfoCommand *)keymap_make_keymap ();

  /* Bind numeric arg functions for both echo area and info window maps. */
  for (i = '0'; i < '9' + 1; i++)
    {
      info_keymap[i].function =
        ((Keymap) echo_area_keymap[ESC].function)[i].function =
	info_add_digit_to_numeric_arg;
    }
  info_keymap['-'].function =
    ((Keymap) echo_area_keymap[ESC].function)['-'].function =
      info_add_digit_to_numeric_arg;

  /* Bind the echo area routines. */
  map = echo_area_keymap;

  map[Control ('a')].function = ea_beg_of_line;
  map[Control ('b')].function = ea_backward;
  map[Control ('d')].function = ea_delete;
  map[Control ('e')].function = ea_end_of_line;
  map[Control ('f')].function = ea_forward;
  map[Control ('g')].function = ea_abort;
  map[Control ('h')].function = ea_rubout;
  map[Control ('k')].function = ea_kill_line;
  map[Control ('l')].function = info_redraw_display;
  map[Control ('q')].function = ea_quoted_insert;
  map[Control ('t')].function = ea_transpose_chars;
  map[Control ('u')].function = ea_abort;
  map[Control ('v')].function = ea_quoted_insert;
  map[Control ('y')].function = ea_yank;

  map[LFD].function = ea_newline;
  map[RET].function = ea_newline;
  map[SPC].function = ea_complete;
  map[TAB].function = ea_complete;
  map['?'].function = ea_possible_completions;
#ifdef __MSDOS__
  /* PC users will lynch me if I don't give them their usual DEL effect...  */
  map[DEL].function = ea_delete;
#else
  map[DEL].function = ea_rubout;
#endif

  /* Bind the echo area ESC keymap. */
  map = (Keymap)echo_area_keymap[ESC].function;

  map[Control ('g')].function = ea_abort;
  map[Control ('h')].function = ea_backward_kill_word;
  map[Control ('v')].function = ea_scroll_completions_window;
  map['0'].function = ea_beg_of_line;
  map['$'].function = ea_end_of_line;
  map['b'].function = ea_backward_word;
  map['d'].function = ea_kill_word;
  map['f'].function = ea_forward_word;
  map['h'].function = ea_forward;
  map['l'].function = ea_backward;
  map['w'].function = ea_forward_word;
  map['x'].function = ea_delete;
  map['X'].function = ea_kill_word;
  map['y'].function = ea_yank_pop;
  map['?'].function = ea_possible_completions;
  map[TAB].function = ea_tab_insert;
  map[DEL].function = ea_kill_word;

  /* Bind the echo area Control-x keymap. */
  map = (Keymap)echo_area_keymap[Control ('x')].function;

  map['o'].function = info_next_window;
  map[DEL].function = ea_backward_kill_line;

  /* Arrow key bindings for echo area keymaps.  It seems that some
     terminals do not match their termcap entries, so it's best to just
     define everything with both of the usual prefixes.  */
  map = echo_area_keymap;
  keymap_bind_keyseq (map, term_ku, &map[Control ('p')]); /* up */
  keymap_bind_keyseq (map, "\033OA", &map[Control ('p')]);
  keymap_bind_keyseq (map, "\033[A", &map[Control ('p')]);
  keymap_bind_keyseq (map, term_kd, &map[Control ('n')]); /* down */
  keymap_bind_keyseq (map, "\033OB", &map[Control ('n')]);
  keymap_bind_keyseq (map, "\033[B", &map[Control ('n')]);
  keymap_bind_keyseq (map, term_kr, &map[Control ('f')]); /* right */
  keymap_bind_keyseq (map, "\033OC", &map[Control ('f')]);
  keymap_bind_keyseq (map, "\033[C", &map[Control ('f')]);
  keymap_bind_keyseq (map, term_kl, &map[Control ('b')]); /* left */
  keymap_bind_keyseq (map, "\033OD", &map[Control ('b')]);
  keymap_bind_keyseq (map, "\033[D", &map[Control ('b')]);
  keymap_bind_keyseq (map, term_kh, &map[Control ('a')]); /* home */
  keymap_bind_keyseq (map, term_ke, &map[Control ('e')]); /* end */
  keymap_bind_keyseq (map, term_kD, &map[DEL]);	/* delete */

  map = (Keymap)echo_area_keymap[ESC].function;
  keymap_bind_keyseq (map, term_kl, &map['b']); /* left */
  keymap_bind_keyseq (map, "\033OA", &map['b']);
  keymap_bind_keyseq (map, "\033[A", &map['b']);
  keymap_bind_keyseq (map, term_kr, &map['f']); /* right */
  keymap_bind_keyseq (map, "\033OB", &map['f']);
  keymap_bind_keyseq (map, "\033[B", &map['f']);
  keymap_bind_keyseq (map, term_kD, &map[DEL]);	/* delete */

  map = (Keymap)echo_area_keymap[Control ('x')].function;
  keymap_bind_keyseq (map, term_kD, &map[DEL]);

  /* Bind commands for Info window keymaps. */
  map = info_keymap;
  map[TAB].function = info_move_to_next_xref;
  map[LFD].function = info_down_line;
  map[RET].function = info_down_line;
  map[SPC].function = info_scroll_forward;
  map[Control ('a')].function = info_beginning_of_line;
  map[Control ('b')].function = info_scroll_backward_page_only;
  map[Control ('d')].function = info_scroll_half_screen_down;
  map[Control ('e')].function = info_down_line;
  map[Control ('f')].function = info_scroll_forward_page_only;
  map[Control ('g')].function = info_abort_key;
  map[Control ('k')].function = info_up_line;
  map[Control ('l')].function = info_redraw_display;
  map[Control ('n')].function = info_down_line;
  map[Control ('p')].function = info_up_line;
  map[Control ('r')].function = info_redraw_display;
  map[Control ('s')].function = isearch_forward;
  map[Control ('u')].function = info_scroll_half_screen_up;
  map[Control ('v')].function = info_scroll_forward_page_only;
  map[Control ('y')].function = info_up_line;
  map[','].function = info_next_index_match;
  map['/'].function = info_search;

  for (i = '1'; i < '9' + 1; i++)
    ((Keymap) info_keymap[ESC].function)[i].function = info_menu_digit;
  ((Keymap) info_keymap[ESC].function)['0'].function = info_last_menu_item;

  map['<'].function = info_first_node;
  map['>'].function = info_last_node;
  map['?'].function = info_search_backward;
  map['['].function = info_global_prev_node;
  map[']'].function = info_global_next_node;
  map['\''].function = info_history_node;

  map['b'].function = info_scroll_backward;
  map['d'].function = info_scroll_half_screen_down;
  map['e'].function = info_down_line;
  map['E'].function = info_view_file;
  map['f'].function = info_scroll_forward_page_only;
  map['F'].function = info_scroll_forward_page_only;
  map['g'].function = info_first_node;
  map['G'].function = info_last_node;
  map['h'].function = info_get_help_window;
  map['H'].function = info_get_help_window;
  map['i'].function = info_index_search;
  map['I'].function = info_goto_invocation_node;
  map['j'].function = info_down_line;
  map['k'].function = info_up_line;
  map['l'].function = info_history_node;
  map['m'].function = info_menu_item;
  map['n'].function = info_search_next;
  map['N'].function = info_search_previous;
  map['O'].function = info_goto_invocation_node;
  map['p'].function = info_prev_node;
  map['q'].function = info_quit;
  map['Q'].function = info_quit;
  map['r'].function = info_redraw_display;
  map['R'].function = info_redraw_display;
  map['s'].function = info_search;
  map['S'].function = info_search_case_sensitively;
  map['t'].function = info_top_node;
  map['u'].function = info_scroll_half_screen_up;
  map['w'].function = info_scroll_backward_page_only_set_window;
  map['y'].function = info_up_line;
  map['z'].function = info_scroll_forward_page_only_set_window;
  map['Z'].function = NULL;	/* unbind, so it works to bind "ZZ" below */
  map[DEL].function = info_scroll_backward;
  keymap_bind_keyseq (map, term_kD, &map[DEL]);
  keymap_bind_keyseq (map, ":q", &map['q']);
  keymap_bind_keyseq (map, ":Q", &map['q']);
  keymap_bind_keyseq (map, "ZZ", &map['q']);

  /* Bind members in the ESC map for Info windows. */
  map = (Keymap)info_keymap[ESC].function;
  map[Control ('f')].function = info_show_footnotes;
  map[Control ('g')].function = info_abort_key;
  map[TAB].function = info_move_to_prev_xref;
  map[SPC].function = info_scroll_forward_page_only;
  map[Control ('v')].function = info_scroll_other_window;
  map['<'].function = info_beginning_of_node;
  map['>'].function = info_end_of_node;
  map['/'].function = info_search;
  map['?'].function = info_search_backward;
  map['b'].function = info_beginning_of_node;
  map['d'].function = info_dir_node;
  map['e'].function = info_end_of_node;
  map['f'].function = info_xref_item;
  map['g'].function = info_select_reference_this_line;
  map['h'].function = info_get_info_help_node;
  map['m'].function = info_menu_item;
  map['n'].function = info_search;
  map['N'].function = info_search_backward;
  map['r'].function = isearch_backward;
  map['s'].function = isearch_forward;
  map['t'].function = info_top_node;
  map['v'].function = info_scroll_backward_page_only;
#if defined (NAMED_FUNCTIONS)
  map['x'].function = info_execute_command;
#endif /* NAMED_FUNCTIONS */
  map[DEL].function = info_scroll_other_window_backward;

  /* Bind members in the Control-X map for Info windows. */
  map = (Keymap)info_keymap[Control ('x')].function;

  map[Control ('b')].function = list_visited_nodes;
  map[Control ('c')].function = info_quit;
  map[Control ('f')].function = info_view_file;
  map[Control ('g')].function = info_abort_key;
  map[Control ('v')].function = info_view_file;
  map[LFD].function = info_select_reference_this_line;
  map[RET].function = info_select_reference_this_line;
  map['0'].function = info_delete_window;
  map['1'].function = info_keep_one_window;
  map['2'].function = info_split_window;
  map['^'].function = info_grow_window;
  map['b'].function = select_visited_node;
  map['g'].function = info_goto_node;
  map['i'].function = info_index_search;
  map['I'].function = info_goto_invocation_node;
  map['k'].function = info_kill_node;
  map['n'].function = info_next_node;
  map['o'].function = info_next_window;
  map['O'].function = info_goto_invocation_node;
  map['p'].function = info_prev_node;
  map['r'].function = info_xref_item;
  map['t'].function = info_tile_windows;
  map['u'].function = info_up_node;
  map['w'].function = info_toggle_wrap;
  map[','].function = info_next_index_match;
  keymap_bind_keyseq (info_keymap, ":e", &map[Control ('v')]);

  /* Arrow key bindings for Info windows keymap. */
  map = info_keymap;
  keymap_bind_keyseq (map, term_kN, &map[Control ('v')]); /* pagedown */
  keymap_bind_keyseq (map, term_ku, &map[Control ('p')]); /* up */
  keymap_bind_keyseq (map, "\033OA", &map[Control ('p')]);
  keymap_bind_keyseq (map, "\033[A", &map[Control ('p')]);
  keymap_bind_keyseq (map, term_kd, &map[Control ('n')]); /* down */
  keymap_bind_keyseq (map, "\033OB", &map[Control ('n')]);
  keymap_bind_keyseq (map, "\033[B", &map[Control ('n')]);
  keymap_bind_keyseq (map, term_kr, &map[Control ('f')]); /* right */
  keymap_bind_keyseq (map, "\033OC", &map[Control ('f')]);
  keymap_bind_keyseq (map, "\033[C", &map[Control ('f')]);
  keymap_bind_keyseq (map, term_kl, &map[Control ('b')]); /* left */
  keymap_bind_keyseq (map, "\033OD", &map[Control ('b')]);
  keymap_bind_keyseq (map, "\033[D", &map[Control ('b')]);
  keymap_bind_keyseq (map, term_kh, &map['b']);	/* home */
  keymap_bind_keyseq (map, term_ke, &map['e']);	/* end */

  map = (Keymap)info_keymap[ESC].function;
  keymap_bind_keyseq (map, term_kl, &map['b']); /* left */
  keymap_bind_keyseq (map, "\033OA", &map['b']);
  keymap_bind_keyseq (map, "\033[A", &map['b']);
  keymap_bind_keyseq (map, term_kr, &map['f']); /* right */
  keymap_bind_keyseq (map, "\033OB", &map['f']);
  keymap_bind_keyseq (map, "\033[B", &map['f']);
  keymap_bind_keyseq (map, term_kN, &map[Control ('v')]); /* pagedown */
  keymap_bind_keyseq (map, term_kP, &map[DEL]); /* pageup */
  keymap_bind_keyseq (map, term_kD, &map[DEL]);	/* delete */

  /* The alternative to this definition of a `main map' key in the
     `ESC map' section, is something like:
    keymap_bind_keyseq (map, term_kP, &((KeyMap)map[ESC].function).map['v']);
  */
  keymap_bind_keyseq (info_keymap/*sic*/, term_kP, &map['v']); /* pageup */
}

void
initialize_info_keymaps ()
{
  if (vi_keys_p)
    initialize_vi_like_keymaps ();
  else
    initialize_emacs_like_keymaps ();
}

#else /* defined(INFOKEY) */

/* Make sure that we don't have too many command codes defined. */

#if A_NCOMMANDS > A_MAX_COMMAND + 1
#error "too many commands defined"
#endif

/* Initialize the keymaps from the .info keymap file. */

#define NUL	'\0'

static unsigned char default_emacs_like_info_keys[] =
{
	0,	/* suppress-default-keybindings flag */
	TAB, NUL,			A_info_move_to_next_xref,
	LFD, NUL,			A_info_select_reference_this_line,
	RET, NUL,			A_info_select_reference_this_line,
	SPC, NUL,			A_info_scroll_forward,
	CONTROL('a'), NUL,		A_info_beginning_of_line,
	CONTROL('b'), NUL,		A_info_backward_char,
	CONTROL('e'), NUL,		A_info_end_of_line,
	CONTROL('f'), NUL,		A_info_forward_char,
	CONTROL('g'), NUL,		A_info_abort_key,
	CONTROL('h'), NUL,		A_info_get_help_window,
	CONTROL('l'), NUL,		A_info_redraw_display,
	CONTROL('n'), NUL,		A_info_next_line,
	CONTROL('p'), NUL,		A_info_prev_line,
	CONTROL('r'), NUL,		A_isearch_backward,
	CONTROL('s'), NUL,		A_isearch_forward,
	CONTROL('u'), NUL,		A_info_universal_argument,
	CONTROL('v'), NUL,		A_info_scroll_forward_page_only,
	',', NUL,			A_info_next_index_match,
	'/', NUL,			A_info_search,
	'0', NUL,			A_info_last_menu_item,
	'1', NUL,			A_info_menu_digit,
	'2', NUL,			A_info_menu_digit,
	'3', NUL,			A_info_menu_digit,
	'4', NUL,			A_info_menu_digit,
	'5', NUL,			A_info_menu_digit,
	'6', NUL,			A_info_menu_digit,
	'7', NUL,			A_info_menu_digit,
	'8', NUL,			A_info_menu_digit,
	'9', NUL,			A_info_menu_digit,
	'<', NUL,			A_info_first_node,
	'>', NUL,			A_info_last_node,
	'?', NUL,			A_info_get_help_window,
	'[', NUL,			A_info_global_prev_node,
	']', NUL,			A_info_global_next_node,
	'b', NUL,			A_info_beginning_of_node,
	'd', NUL,			A_info_dir_node,
	'e', NUL,			A_info_end_of_node,
	'f', NUL,			A_info_xref_item,
	'g', NUL,			A_info_goto_node,
	'G', NUL,			A_info_menu_sequence,
	'h', NUL,			A_info_get_info_help_node,
	'i', NUL,			A_info_index_search,
	'l', NUL,			A_info_history_node,
	'm', NUL,			A_info_menu_item,
	'n', NUL,			A_info_next_node,
	'O', NUL,			A_info_goto_invocation_node,
	'p', NUL,			A_info_prev_node,
	'q', NUL,			A_info_quit,
	'r', NUL,			A_info_xref_item,
	's', NUL,			A_info_search,
	'S', NUL,			A_info_search_case_sensitively,
	't', NUL,			A_info_top_node,
	'u', NUL,			A_info_up_node,
	DEL, NUL,			A_info_scroll_backward,
	ESC, '0', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '1', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '2', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '3', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '4', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '5', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '6', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '7', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '8', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '9', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '-', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, CONTROL('f'), NUL,		A_info_show_footnotes,
	ESC, CONTROL('g'), NUL,		A_info_abort_key,
	ESC, TAB, NUL,			A_info_move_to_prev_xref,
	ESC, CONTROL('v'), NUL,		A_info_scroll_other_window,
	ESC, '<', NUL,			A_info_beginning_of_node,
	ESC, '>', NUL,			A_info_end_of_node,
	ESC, 'b', NUL,			A_info_backward_word,
	ESC, 'f', NUL,			A_info_forward_word,
	ESC, 'r', NUL,			A_info_move_to_window_line,
	ESC, 'v', NUL,			A_info_scroll_backward_page_only,
	Meta('0'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('1'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('2'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('3'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('4'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('5'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('6'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('7'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('8'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('9'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('-'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta(CONTROL('f')), NUL,	A_info_show_footnotes,
	Meta(CONTROL('g')), NUL,	A_info_abort_key,
	Meta(TAB), NUL,			A_info_move_to_prev_xref,
	Meta(CONTROL('v')), NUL,	A_info_scroll_other_window,
	Meta('<'), NUL,			A_info_beginning_of_node,
	Meta('>'), NUL,			A_info_end_of_node,
	Meta('b'), NUL,			A_info_backward_word,
	Meta('f'), NUL,			A_info_forward_word,
	Meta('r'), NUL,			A_info_move_to_window_line,
	Meta('v'), NUL,			A_info_scroll_backward_page_only,
#if defined (NAMED_FUNCTIONS)
	ESC, 'x', NUL,			A_info_execute_command,
	Meta('x'), NUL,			A_info_execute_command,
#endif /* NAMED_FUNCTIONS */

	CONTROL('x'), CONTROL('b'), NUL,	A_list_visited_nodes,
	CONTROL('x'), CONTROL('c'), NUL,	A_info_quit,
	CONTROL('x'), CONTROL('f'), NUL,	A_info_view_file,
	CONTROL('x'), CONTROL('g'), NUL,	A_info_abort_key,
	CONTROL('x'), CONTROL('v'), NUL,	A_info_view_file,
	CONTROL('x'), '0', NUL,		A_info_delete_window,
	CONTROL('x'), '1', NUL,		A_info_keep_one_window,
	CONTROL('x'), '2', NUL,		A_info_split_window,
	CONTROL('x'), '^', NUL,		A_info_grow_window,
	CONTROL('x'), 'b', NUL,		A_select_visited_node,
	CONTROL('x'), 'k', NUL,		A_info_kill_node,
	CONTROL('x'), 'n', NUL,		A_info_search_next,
	CONTROL('x'), 'N', NUL,		A_info_search_previous,
	CONTROL('x'), 'o', NUL,		A_info_next_window,
	CONTROL('x'), 't', NUL,		A_info_tile_windows,
	CONTROL('x'), 'w', NUL,		A_info_toggle_wrap,

/*	Arrow key bindings for info keymaps.  It seems that some
	terminals do not match their termcap entries, so it's best to just
	define everything with both of the usual prefixes.  */

	SK_ESCAPE, SK_PAGE_UP, NUL,		A_info_scroll_backward_page_only,
	SK_ESCAPE, SK_PAGE_DOWN, NUL,		A_info_scroll_forward_page_only,
	SK_ESCAPE, SK_UP_ARROW, NUL,		A_info_prev_line,
	'\033', 'O', 'A', NUL,			A_info_prev_line,
	'\033', '[', 'A', NUL,			A_info_prev_line,
	SK_ESCAPE, SK_DOWN_ARROW, NUL,		A_info_next_line,
	'\033', 'O', 'B', NUL,			A_info_next_line,
	'\033', '[', 'B', NUL,			A_info_next_line,
	SK_ESCAPE, SK_RIGHT_ARROW, NUL,		A_info_forward_char,
	'\033', 'O', 'C', NUL,			A_info_forward_char,
	'\033', '[', 'C', NUL,			A_info_forward_char,
	SK_ESCAPE, SK_LEFT_ARROW, NUL,		A_info_backward_char,
	'\033', 'O', 'D', NUL,			A_info_backward_char,
	'\033', '[', 'D', NUL,			A_info_backward_char,
	SK_ESCAPE, SK_HOME, NUL,		A_info_beginning_of_node,
	SK_ESCAPE, SK_END, NUL,			A_info_end_of_node,
	SK_ESCAPE, SK_DELETE, NUL,		A_info_scroll_backward,

	ESC, SK_ESCAPE, SK_PAGE_UP, NUL,	A_info_scroll_other_window_backward,
	ESC, SK_ESCAPE, SK_PAGE_DOWN, NUL,	A_info_scroll_other_window,
	ESC, SK_ESCAPE, SK_UP_ARROW, NUL,	A_info_prev_line,
	ESC, '\033', 'O', 'A', NUL,		A_info_prev_line,
	ESC, '\033', '[', 'A', NUL,		A_info_prev_line,
	ESC, SK_ESCAPE, SK_DOWN_ARROW, NUL,	A_info_next_line,
	ESC, '\033', 'O', 'B', NUL,		A_info_next_line,
	ESC, '\033', '[', 'B', NUL,		A_info_next_line,
	ESC, SK_ESCAPE, SK_RIGHT_ARROW, NUL,	A_info_forward_word,
	ESC, '\033', 'O', 'C', NUL,		A_info_forward_word,
	ESC, '\033', '[', 'C', NUL,		A_info_forward_word,
	ESC, SK_ESCAPE, SK_LEFT_ARROW, NUL,	A_info_backward_word,
	ESC, '\033', 'O', 'D', NUL,		A_info_backward_word,
	ESC, '\033', '[', 'D', NUL,		A_info_backward_word,
};

static unsigned char default_emacs_like_ea_keys[] =
{
	0,	/* suppress-default-keybindings flag */
	ESC, '0', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '1', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '2', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '3', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '4', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '5', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '6', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '7', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '8', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '9', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '-', NUL,			A_info_add_digit_to_numeric_arg,
	Meta('0'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('1'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('2'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('3'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('4'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('5'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('6'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('7'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('8'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('9'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('-'), NUL,			A_info_add_digit_to_numeric_arg,
	ESC, CONTROL('g'), NUL,		A_ea_abort,
	ESC, CONTROL('v'), NUL,		A_ea_scroll_completions_window,
	ESC, 'b', NUL,			A_ea_backward_word,
	ESC, 'd', NUL,			A_ea_kill_word,
	ESC, 'f', NUL,			A_ea_forward_word,
	ESC, 'y', NUL,			A_ea_yank_pop,
	ESC, '?', NUL,			A_ea_possible_completions,
	ESC, TAB, NUL,			A_ea_tab_insert,
	ESC, DEL, NUL,			A_ea_backward_kill_word,
	Meta(CONTROL('g')), NUL,	A_ea_abort,
	Meta(CONTROL('v')), NUL,	A_ea_scroll_completions_window,
	Meta('b'), NUL,			A_ea_backward_word,
	Meta('d'), NUL,			A_ea_kill_word,
	Meta('f'), NUL,			A_ea_forward_word,
	Meta('y'), NUL,			A_ea_yank_pop,
	Meta('?'), NUL,			A_ea_possible_completions,
	Meta(TAB), NUL,			A_ea_tab_insert,
	Meta(DEL), NUL,			A_ea_backward_kill_word,
	CONTROL('a'), NUL,		A_ea_beg_of_line,
	CONTROL('b'), NUL,		A_ea_backward,
	CONTROL('d'), NUL,		A_ea_delete,
	CONTROL('e'), NUL,		A_ea_end_of_line,
	CONTROL('f'), NUL,		A_ea_forward,
	CONTROL('g'), NUL,		A_ea_abort,
	CONTROL('h'), NUL,		A_ea_rubout,
/*	CONTROL('k') */
	SK_ESCAPE, SK_LITERAL, NUL,	A_ea_kill_line,
	CONTROL('l'), NUL,		A_info_redraw_display,
	CONTROL('q'), NUL,		A_ea_quoted_insert,
	CONTROL('t'), NUL,		A_ea_transpose_chars,
	CONTROL('u'), NUL,		A_info_universal_argument,
	CONTROL('y'), NUL,		A_ea_yank,
	LFD, NUL,			A_ea_newline,
	RET, NUL,			A_ea_newline,
	SPC, NUL,			A_ea_complete,
	TAB, NUL,			A_ea_complete,
	'?', NUL,			A_ea_possible_completions,
#ifdef __MSDOS__
        /* PC users will lynch me if I don't give them their usual DEL
	   effect...  */
	DEL, NUL,			A_ea_delete,
#else
	DEL, NUL,			A_ea_rubout,
#endif
#if defined (NAMED_FUNCTIONS)
  /* 	ESC, 'x', NUL,			A_info_execute_command, */
  /* 	Meta('x'), NUL,			A_info_execute_command, */
#endif /* NAMED_FUNCTIONS */
	CONTROL('x'), 'o', NUL,		A_info_next_window,
  	CONTROL('x'), DEL, NUL,		A_ea_backward_kill_line,

/*	Arrow key bindings for echo area keymaps.  It seems that some
	terminals do not match their termcap entries, so it's best to just
	define everything with both of the usual prefixes.  */

	SK_ESCAPE, SK_RIGHT_ARROW, NUL,		A_ea_forward,
	'\033', 'O', 'C', NUL,			A_ea_forward,
	'\033', '[', 'C', NUL,			A_ea_forward,
	SK_ESCAPE, SK_LEFT_ARROW, NUL,		A_ea_backward,
	'\033', 'O', 'D', NUL,			A_ea_backward,
	'\033', '[', 'D', NUL,			A_ea_backward,
	ESC, SK_ESCAPE, SK_RIGHT_ARROW, NUL,	A_ea_forward_word,
	ESC, '\033', 'O', 'C', NUL,		A_ea_forward_word,
	ESC, '\033', '[', 'C', NUL,		A_ea_forward_word,
	ESC, SK_ESCAPE, SK_LEFT_ARROW, NUL,	A_ea_backward_word,
	ESC, '\033', 'O', 'D', NUL,		A_ea_backward_word,
	ESC, '\033', '[', 'D', NUL,		A_ea_backward_word,
#ifdef __MSDOS__
	SK_ESCAPE, SK_DELETE, NUL,		A_ea_delete,
#else
	SK_ESCAPE, SK_DELETE, NUL,		A_ea_rubout,
#endif
	SK_ESCAPE, SK_HOME, NUL,		A_ea_beg_of_line,
	SK_ESCAPE, SK_END, NUL,			A_ea_end_of_line,
	ESC, SK_ESCAPE, SK_DELETE, NUL,		A_ea_backward_kill_word,
	CONTROL('x'), SK_ESCAPE, SK_DELETE, NUL,A_ea_backward_kill_line,
};

static unsigned char default_vi_like_info_keys[] =
{
	0,	/* suppress-default-keybindings flag */
	'0', NUL,			A_info_add_digit_to_numeric_arg,
	'1', NUL,			A_info_add_digit_to_numeric_arg,
	'2', NUL,			A_info_add_digit_to_numeric_arg,
	'3', NUL,			A_info_add_digit_to_numeric_arg,
	'4', NUL,			A_info_add_digit_to_numeric_arg,
	'5', NUL,			A_info_add_digit_to_numeric_arg,
	'6', NUL,			A_info_add_digit_to_numeric_arg,
	'7', NUL,			A_info_add_digit_to_numeric_arg,
	'8', NUL,			A_info_add_digit_to_numeric_arg,
	'9', NUL,			A_info_add_digit_to_numeric_arg,
	'-', NUL,			A_info_add_digit_to_numeric_arg,
	TAB, NUL,			A_info_move_to_next_xref,
	LFD, NUL,			A_info_down_line,
	RET, NUL,			A_info_down_line,
	SPC, NUL,			A_info_scroll_forward,
	CONTROL('a'), NUL,		A_info_beginning_of_line,
	CONTROL('b'), NUL,		A_info_scroll_backward_page_only,
	CONTROL('d'), NUL,		A_info_scroll_half_screen_down,
	CONTROL('e'), NUL,		A_info_down_line,
	CONTROL('f'), NUL,		A_info_scroll_forward_page_only,
	CONTROL('g'), NUL,		A_info_abort_key,
	CONTROL('k'), NUL,		A_info_up_line,
	CONTROL('l'), NUL,		A_info_redraw_display,
	CONTROL('n'), NUL,		A_info_down_line,
	CONTROL('p'), NUL,		A_info_up_line,
	CONTROL('r'), NUL,		A_info_redraw_display,
	CONTROL('s'), NUL,		A_isearch_forward,
	CONTROL('u'), NUL,		A_info_scroll_half_screen_up,
	CONTROL('v'), NUL,		A_info_scroll_forward_page_only,
	CONTROL('y'), NUL,		A_info_up_line,
	',', NUL,			A_info_next_index_match,
	'/', NUL,			A_info_search,
	ESC, '0', NUL,			A_info_last_menu_item,
	ESC, '1', NUL,			A_info_menu_digit,
	ESC, '2', NUL,			A_info_menu_digit,
	ESC, '3', NUL,			A_info_menu_digit,
	ESC, '4', NUL,			A_info_menu_digit,
	ESC, '5', NUL,			A_info_menu_digit,
	ESC, '6', NUL,			A_info_menu_digit,
	ESC, '7', NUL,			A_info_menu_digit,
	ESC, '8', NUL,			A_info_menu_digit,
	ESC, '9', NUL,			A_info_menu_digit,
	Meta('0'), NUL,			A_info_last_menu_item,
	Meta('1'), NUL,			A_info_menu_digit,
	Meta('2'), NUL,			A_info_menu_digit,
	Meta('3'), NUL,			A_info_menu_digit,
	Meta('4'), NUL,			A_info_menu_digit,
	Meta('5'), NUL,			A_info_menu_digit,
	Meta('6'), NUL,			A_info_menu_digit,
	Meta('7'), NUL,			A_info_menu_digit,
	Meta('8'), NUL,			A_info_menu_digit,
	Meta('9'), NUL,			A_info_menu_digit,
	'<', NUL,			A_info_first_node,
	'>', NUL,			A_info_last_node,
	'?', NUL,			A_info_search_backward,
	'[', NUL,			A_info_global_prev_node,
	']', NUL,			A_info_global_next_node,
	'\'', NUL,			A_info_history_node,
	'b', NUL,			A_info_scroll_backward,
	'd', NUL,			A_info_scroll_half_screen_down,
	'e', NUL,			A_info_down_line,
	'E', NUL,			A_info_view_file,
	':', 'e', NUL,			A_info_view_file,
	'f', NUL,			A_info_scroll_forward_page_only,
	'F', NUL,			A_info_scroll_forward_page_only,
	'g', NUL,			A_info_first_node,
	'G', NUL,			A_info_last_node,
	'h', NUL,			A_info_get_help_window,
	'H', NUL,			A_info_get_help_window,
	'i', NUL,			A_info_index_search,
	'I', NUL,			A_info_goto_invocation_node,
	'j', NUL,			A_info_down_line,
	'k', NUL,			A_info_up_line,
	'l', NUL,			A_info_history_node,
	'm', NUL,			A_info_menu_item,
	'n', NUL,			A_info_search_next,
	'N', NUL,			A_info_search_previous,
	'O', NUL,			A_info_goto_invocation_node,
	'p', NUL,			A_info_prev_node,
	'q', NUL,			A_info_quit,
	'Q', NUL,			A_info_quit,
	':', 'q', NUL,			A_info_quit,
	':', 'Q', NUL,			A_info_quit,
	'Z', 'Z', NUL,			A_info_quit,
	'r', NUL,			A_info_redraw_display,
	'R', NUL,			A_info_redraw_display,
	's', NUL,			A_info_search,
	'S', NUL,			A_info_search_case_sensitively,
	't', NUL,			A_info_top_node,
	'u', NUL,			A_info_scroll_half_screen_up,
	'w', NUL,			A_info_scroll_backward_page_only_set_window,
	'y', NUL,			A_info_up_line,
	'z', NUL,			A_info_scroll_forward_page_only_set_window,
	DEL, NUL,			A_info_scroll_backward,
	ESC, CONTROL('f'), NUL,		A_info_show_footnotes,
	ESC, CONTROL('g'), NUL,		A_info_abort_key,
	ESC, TAB, NUL,			A_info_move_to_prev_xref,
	ESC, SPC, NUL,			A_info_scroll_forward_page_only,
	ESC, CONTROL('v'), NUL,		A_info_scroll_other_window,
	ESC, '<', NUL,			A_info_beginning_of_node,
	ESC, '>', NUL,			A_info_end_of_node,
	ESC, '/', NUL,			A_info_search,
	ESC, '?', NUL,			A_info_search_backward,
	ESC, 'b', NUL,			A_info_beginning_of_node,
	ESC, 'd', NUL,			A_info_dir_node,
	ESC, 'e', NUL,			A_info_end_of_node,
	ESC, 'f', NUL,			A_info_xref_item,
	ESC, 'g', NUL,			A_info_select_reference_this_line,
	ESC, 'h', NUL,			A_info_get_info_help_node,
	ESC, 'm', NUL,			A_info_menu_item,
	ESC, 'n', NUL,			A_info_search,
	ESC, 'N', NUL,			A_info_search_backward,
	ESC, 'r', NUL,			A_isearch_backward,
	ESC, 's', NUL,			A_isearch_forward,
	ESC, 't', NUL,			A_info_top_node,
	ESC, 'v', NUL,			A_info_scroll_backward_page_only,
#if defined (NAMED_FUNCTIONS)
	ESC, 'x', NUL,			A_info_execute_command,
	Meta('x'), NUL,			A_info_execute_command,
#endif /* NAMED_FUNCTIONS */
	ESC, DEL, NUL,			A_info_scroll_other_window_backward,
	CONTROL('x'), CONTROL('b'), NUL,	A_list_visited_nodes,
	CONTROL('x'), CONTROL('c'), NUL,	A_info_quit,
	CONTROL('x'), CONTROL('f'), NUL,	A_info_view_file,
	CONTROL('x'), CONTROL('g'), NUL,	A_info_abort_key,
	CONTROL('x'), CONTROL('v'), NUL,	A_info_view_file,
	CONTROL('x'), LFD, NUL,		A_info_select_reference_this_line,
	CONTROL('x'), RET, NUL,		A_info_select_reference_this_line,
	CONTROL('x'), '0', NUL,		A_info_delete_window,
	CONTROL('x'), '1', NUL,		A_info_keep_one_window,
	CONTROL('x'), '2', NUL,		A_info_split_window,
	CONTROL('x'), '^', NUL,		A_info_grow_window,
	CONTROL('x'), 'b', NUL,		A_select_visited_node,
	CONTROL('x'), 'g', NUL,		A_info_goto_node,
	CONTROL('x'), 'i', NUL,		A_info_index_search,
	CONTROL('x'), 'I', NUL,		A_info_goto_invocation_node,
	CONTROL('x'), 'k', NUL,		A_info_kill_node,
	CONTROL('x'), 'n', NUL,		A_info_next_node,
	CONTROL('x'), 'o', NUL,		A_info_next_window,
	CONTROL('x'), 'O', NUL,		A_info_goto_invocation_node,
	CONTROL('x'), 'p', NUL,		A_info_prev_node,
	CONTROL('x'), 'r', NUL,		A_info_xref_item,
	CONTROL('x'), 't', NUL,		A_info_tile_windows,
	CONTROL('x'), 'u', NUL,		A_info_up_node,
	CONTROL('x'), 'w', NUL,		A_info_toggle_wrap,
	CONTROL('x'), ',', NUL,		A_info_next_index_match,

/*	Arrow key bindings for info keymaps.  It seems that some
	terminals do not match their termcap entries, so it's best to just
	define everything with both of the usual prefixes.  */

	SK_ESCAPE, SK_PAGE_UP, NUL,		A_info_scroll_backward_page_only,
	SK_ESCAPE, SK_PAGE_DOWN, NUL,		A_info_scroll_forward_page_only,
	SK_ESCAPE, SK_UP_ARROW, NUL,		A_info_up_line,
	'\033', 'O', 'A', NUL,			A_info_up_line,
	'\033', '[', 'A', NUL,			A_info_up_line,
	SK_ESCAPE, SK_DOWN_ARROW, NUL,		A_info_down_line,
	'\033', 'O', 'B', NUL,			A_info_down_line,
	'\033', '[', 'B', NUL,			A_info_down_line,
	SK_ESCAPE, SK_RIGHT_ARROW, NUL,		A_info_scroll_forward_page_only,
	'\033', 'O', 'C', NUL,			A_info_scroll_forward_page_only,
	'\033', '[', 'C', NUL,			A_info_scroll_forward_page_only,
	SK_ESCAPE, SK_LEFT_ARROW, NUL,		A_info_scroll_backward_page_only,
	'\033', 'O', 'D', NUL,			A_info_scroll_backward_page_only,
	'\033', '[', 'D', NUL,			A_info_scroll_backward_page_only,
	SK_ESCAPE, SK_HOME, NUL,		A_info_beginning_of_node,
	SK_ESCAPE, SK_END, NUL,			A_info_end_of_node,
	ESC, SK_ESCAPE, SK_PAGE_DOWN, NUL,	A_info_scroll_other_window,
	ESC, SK_ESCAPE, SK_PAGE_UP, NUL,	A_info_scroll_other_window_backward,
	ESC, SK_ESCAPE, SK_DELETE, NUL,		A_info_scroll_other_window_backward,
	ESC, SK_ESCAPE, SK_UP_ARROW, NUL,	A_info_prev_node,
	ESC, '\033', 'O', 'A', NUL,		A_info_prev_node,
	ESC, '\033', '[', 'A', NUL,		A_info_prev_node,
	ESC, SK_ESCAPE, SK_DOWN_ARROW, NUL,	A_info_next_node,
	ESC, '\033', 'O', 'B', NUL,		A_info_next_node,
	ESC, '\033', '[', 'B', NUL,		A_info_next_node,
	ESC, SK_ESCAPE, SK_RIGHT_ARROW, NUL,	A_info_xref_item,
	ESC, '\033', 'O', 'C', NUL,		A_info_xref_item,
	ESC, '\033', '[', 'C', NUL,		A_info_xref_item,
	ESC, SK_ESCAPE, SK_LEFT_ARROW, NUL,	A_info_beginning_of_node,
	ESC, '\033', 'O', 'D', NUL,		A_info_beginning_of_node,
	ESC, '\033', '[', 'D', NUL,		A_info_beginning_of_node,
	CONTROL('x'), SK_ESCAPE, SK_DELETE, NUL,A_ea_backward_kill_line,
};

static unsigned char default_vi_like_ea_keys[] =
{
	0,	/* suppress-default-keybindings flag */
	ESC, '1', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '2', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '3', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '4', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '5', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '6', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '7', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '8', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '9', NUL,			A_info_add_digit_to_numeric_arg,
	ESC, '-', NUL,			A_info_add_digit_to_numeric_arg,
	Meta('1'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('2'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('3'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('4'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('5'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('6'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('7'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('8'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('9'), NUL,			A_info_add_digit_to_numeric_arg,
	Meta('-'), NUL,			A_info_add_digit_to_numeric_arg,
	ESC, CONTROL('g'), NUL,		A_ea_abort,
	ESC, CONTROL('h'), NUL,		A_ea_backward_kill_word,
	ESC, CONTROL('v'), NUL,		A_ea_scroll_completions_window,
	ESC, '0', NUL,			A_ea_beg_of_line,
	ESC, '$', NUL,			A_ea_end_of_line,
	ESC, 'b', NUL,			A_ea_backward_word,
	ESC, 'd', NUL,			A_ea_kill_word,
	ESC, 'f', NUL,			A_ea_forward_word,
	ESC, 'h', NUL,			A_ea_forward,
	ESC, 'l', NUL,			A_ea_backward,
	ESC, 'w', NUL,			A_ea_forward_word,
	ESC, 'x', NUL,			A_ea_delete,
	ESC, 'X', NUL,			A_ea_kill_word,
	ESC, 'y', NUL,			A_ea_yank_pop,
	ESC, '?', NUL,			A_ea_possible_completions,
	ESC, TAB, NUL,			A_ea_tab_insert,
	ESC, DEL, NUL,			A_ea_kill_word,
	Meta(CONTROL('g')), NUL,	A_ea_abort,
	Meta(CONTROL('h')), NUL,	A_ea_backward_kill_word,
	Meta(CONTROL('v')), NUL,	A_ea_scroll_completions_window,
	Meta('0'), NUL,			A_ea_beg_of_line,
	Meta('$'), NUL,			A_ea_end_of_line,
	Meta('b'), NUL,			A_ea_backward_word,
	Meta('d'), NUL,			A_ea_kill_word,
	Meta('f'), NUL,			A_ea_forward_word,
	Meta('h'), NUL,			A_ea_forward,
	Meta('l'), NUL,			A_ea_backward,
	Meta('w'), NUL,			A_ea_forward_word,
	Meta('x'), NUL,			A_ea_delete,
	Meta('X'), NUL,			A_ea_kill_word,
	Meta('y'), NUL,			A_ea_yank_pop,
	Meta('?'), NUL,			A_ea_possible_completions,
	Meta(TAB), NUL,			A_ea_tab_insert,
	Meta(DEL), NUL,			A_ea_kill_word,
	CONTROL('a'), NUL,		A_ea_beg_of_line,
	CONTROL('b'), NUL,		A_ea_backward,
	CONTROL('d'), NUL,		A_ea_delete,
	CONTROL('e'), NUL,		A_ea_end_of_line,
	CONTROL('f'), NUL,		A_ea_forward,
	CONTROL('g'), NUL,		A_ea_abort,
	CONTROL('h'), NUL,		A_ea_rubout,
/*	CONTROL('k') */
	SK_ESCAPE, SK_LITERAL, NUL,	A_ea_kill_line,
	CONTROL('l'), NUL,		A_info_redraw_display,
	CONTROL('q'), NUL,		A_ea_quoted_insert,
	CONTROL('t'), NUL,		A_ea_transpose_chars,
	CONTROL('u'), NUL,		A_ea_abort,
	CONTROL('v'), NUL,		A_ea_quoted_insert,
	CONTROL('y'), NUL,		A_ea_yank,
	LFD, NUL,			A_ea_newline,
	RET, NUL,			A_ea_newline,
	SPC, NUL,			A_ea_complete,
	TAB, NUL,			A_ea_complete,
	'?', NUL,			A_ea_possible_completions,
#ifdef __MSDOS__
        /* PC users will lynch me if I don't give them their usual DEL
	   effect...  */
	DEL, NUL,			A_ea_delete,
#else
	DEL, NUL,			A_ea_rubout,
#endif
	CONTROL('x'), 'o', NUL,		A_info_next_window,
  	CONTROL('x'), DEL, NUL,		A_ea_backward_kill_line,

  /* Arrow key bindings for echo area keymaps.  It seems that some
     terminals do not match their termcap entries, so it's best to just
     define everything with both of the usual prefixes.  */

	SK_ESCAPE, SK_RIGHT_ARROW, NUL,		A_ea_forward,
	'\033', 'O', 'C', NUL,			A_ea_forward,
	'\033', '[', 'C', NUL,			A_ea_forward,
	SK_ESCAPE, SK_LEFT_ARROW, NUL,		A_ea_backward,
	'\033', 'O', 'D', NUL,			A_ea_backward,
	'\033', '[', 'D', NUL,			A_ea_backward,
	SK_ESCAPE, SK_HOME, NUL,		A_ea_beg_of_line,
	SK_ESCAPE, SK_END, NUL,			A_ea_end_of_line,
#ifdef __MSDOS__
	SK_ESCAPE, SK_DELETE, NUL,		A_ea_delete,
#else
	SK_DELETE, SK_DELETE, NUL,		A_ea_rubout,
#endif
	ESC, SK_ESCAPE, SK_RIGHT_ARROW, NUL,	A_ea_forward_word,
	ESC, '\033', 'O', 'C', NUL,		A_ea_forward_word,
	ESC, '\033', '[', 'C', NUL,		A_ea_forward_word,
	ESC, SK_ESCAPE, SK_LEFT_ARROW, NUL,	A_ea_backward_word,
	ESC, '\033', 'O', 'D', NUL,		A_ea_backward_word,
	ESC, '\033', '[', 'D', NUL,		A_ea_backward_word,
	ESC, SK_ESCAPE, SK_DELETE, NUL,		A_ea_kill_word,
	CONTROL('x'), SK_ESCAPE, SK_DELETE, NUL,A_ea_backward_kill_line,
};

static unsigned char *user_info_keys;
static unsigned int user_info_keys_len;
static unsigned char *user_ea_keys;
static unsigned int user_ea_keys_len;
static unsigned char *user_vars;
static unsigned int user_vars_len;

/*
 * Return the size of a file, or 0 if the size can't be determined.
 */
static unsigned long
filesize(f)
	int f;
{
	long pos = lseek(f, 0L, SEEK_CUR);
	long sz = -1L;
	if (pos != -1L)
	{
		sz = lseek(f, 0L, SEEK_END);
		lseek(f, pos, SEEK_SET);
	}
	return sz == -1L ? 0L : sz;
}

/* Get an integer from a infokey file.
   Integers are stored as two bytes, low order first, in radix INFOKEY_RADIX.
 */
static int
getint(sp)
	unsigned char **sp;
{
	int n;

	if ( !((*sp)[0] < INFOKEY_RADIX && (*sp)[1] < INFOKEY_RADIX) )
		return -1;
	n = (*sp)[0] + (*sp)[1] * INFOKEY_RADIX;
	*sp += 2;
	return n;
}


/* Fetch the contents of the standard infokey file "$HOME/.info".  Return
   true if ok, false if not.  */
static int
fetch_user_maps()
{
	char *filename = NULL;
	char *homedir;
	int f;
	unsigned char *buf;
	unsigned long len;
	long nread;
	unsigned char *p;
	int n;

	/* Find and open file. */
	if ((filename = getenv("INFOKEY")) != NULL)
		filename = xstrdup(filename);
	else if ((homedir = getenv("HOME")) != NULL)
	{
		filename = xmalloc(strlen(homedir) + 2 + strlen(INFOKEY_FILE));
		strcpy(filename, homedir);
		strcat(filename, "/");
		strcat(filename, INFOKEY_FILE);
	}
#ifdef __MSDOS__
	/* Poor baby, she doesn't have a HOME...  */
	else
		filename = xstrdup(INFOKEY_FILE); /* try current directory */
#endif
	if (filename == NULL || (f = open(filename, O_RDONLY)) == (-1))
	{
		if (filename)
		{
			info_error(filesys_error_string(filename, errno));
			free(filename);
		}
		return 0;
	}
	SET_BINARY (f);

	/* Ensure that the file is a reasonable size. */
	len = filesize(f);
	if (len < INFOKEY_NMAGIC + 2 || len > 100 * 1024)
	{
		/* Bad file (a valid file must have at least 9 chars, and
		   more than 100 KB is a problem). */
		if (len < INFOKEY_NMAGIC + 2)
			info_error(_("Ignoring invalid infokey file `%s' - too small"),
				   filename);
		else
			info_error(_("Ignoring invalid infokey file `%s' - too big"),
				   filename);
		close(f);
		free(filename);
		return 0;
	}

	/* Read the file into a buffer. */
	buf = (unsigned char *)xmalloc((int)len);
	nread = read(f, buf, (unsigned int) len);
	close(f);
	if (nread != len)
	{
		info_error(_("Error reading infokey file `%s' - short read"), filename);
		free(buf);
		free(filename);
		return 0;
	}

	/* Check the header, trailer, and version of the file to increase
	   our confidence that the contents are valid.  */
	if (	buf[0] != INFOKEY_MAGIC_S0
		|| buf[1] != INFOKEY_MAGIC_S1
		|| buf[2] != INFOKEY_MAGIC_S2
		|| buf[3] != INFOKEY_MAGIC_S3
		|| buf[len - 4] != INFOKEY_MAGIC_E0
		|| buf[len - 3] != INFOKEY_MAGIC_E1
		|| buf[len - 2] != INFOKEY_MAGIC_E2
		|| buf[len - 1] != INFOKEY_MAGIC_E3
	)
	{
		info_error(_("Invalid infokey file `%s' (bad magic numbers) -- run infokey to update it"), filename);
		free(filename);
		return 0;
	}
	if (len < INFOKEY_NMAGIC + strlen(VERSION) + 1 || strcmp(VERSION, buf + 4) != 0)
	{
		info_error(_("Your infokey file `%s' is out of date -- run infokey to update it"), filename);
		free(filename);
		return 0;
	}

	/* Extract the pieces.  */
	for (p = buf + 4 + strlen(VERSION) + 1; p - buf < len - 4; p += n)
	{
		int s = *p++;

		n = getint(&p);
		if (n < 0 || n > len - 4 - (p - buf))
		{
			info_error(_("Invalid infokey file `%s' (bad section length) -- run infokey to update it"), filename);
			free(filename);
			return 0;
		}

		switch (s)
		{
		case INFOKEY_SECTION_INFO:
			user_info_keys = p;
			user_info_keys_len = n;
			break;
		case INFOKEY_SECTION_EA:
			user_ea_keys = p;
			user_ea_keys_len = n;
			break;
		case INFOKEY_SECTION_VAR:
			user_vars = p;
			user_vars_len = n;
			break;
		default:
			info_error(_("Invalid infokey file `%s' (bad section code) -- run infokey to update it"), filename);
			free(filename);
			return 0;
		}
	}

	free(filename);
	return 1;
}

/* Decode special key sequences from the infokey file.  Return zero
   if the key sequence includes special keys which the terminal
   doesn't define.
 */
static int
decode_keys(src, slen, dst, dlen)
	unsigned char *src;
	unsigned int slen;
	unsigned char *dst;
	unsigned int dlen;
{
	unsigned char *s = src;
	unsigned char *d = dst;

#define To_dst(c) do { if (d - dst < dlen) *d++ = (c); } while (0)

	while (s - src < slen)
	{
		unsigned char c = ISMETA(*s) ? UNMETA(*s) : *s;

		if (c == SK_ESCAPE)
		{
			unsigned char *t;
			static char lit[] = { SK_ESCAPE, NUL };

			switch (s + 1 - src < slen ? s[1] : '\0')
			{
			case SK_RIGHT_ARROW:	t = term_kr; break;
			case SK_LEFT_ARROW:	t = term_kl; break;
			case SK_UP_ARROW:	t = term_ku; break;
			case SK_DOWN_ARROW:	t = term_kd; break;
			case SK_PAGE_UP:	t = term_kP; break;
			case SK_PAGE_DOWN:	t = term_kN; break;
			case SK_HOME:		t = term_kh; break;
			case SK_END:		t = term_ke; break;
			case SK_DELETE:		t = term_kx; break;
			case SK_INSERT:		t = term_ki; break;
			case SK_LITERAL:
			default:		t = lit; break;
			}
			if (t == NULL)
				return 0;
			while (*t)
				To_dst(ISMETA(*s) ? Meta(*t++) : *t++);
			s += 2;
		}
		else
		{
			if (ISMETA(*s))
				To_dst(Meta(*s++));
			else
				To_dst(*s++);
		}
	}

	To_dst('\0');

	return 1;

#undef To_dst

}

/* Convert an infokey file section to keymap bindings.  Return false if
   the default bindings are to be suppressed.  */
static int
section_to_keymaps(map, table, len)
	Keymap map;
	unsigned char *table;
	unsigned int len;
{
	int stop;
	unsigned char *p;
	unsigned char *seq;
	unsigned int seqlen;
	KEYMAP_ENTRY ke;
	enum { getseq, gotseq, getaction } state = getseq;

	stop = len > 0 ? table[0] : 0;

	for (p = table + 1; p - table < len; p++)
	{
		switch (state)
		{
		case getseq:
			if (*p)
			{
				seq = p;
				state = gotseq;
			}
			break;

		case gotseq:
			if (!*p)
			{
				seqlen = p - seq;
				state = getaction;
			}
			break;

		case getaction:
			{
				unsigned int action = *p;
				unsigned char keyseq[256];
				KEYMAP_ENTRY ke;

				state = getseq;
				/* If decode_keys returns zero, it
				   means that seq includes keys which
				   the terminal doesn't support, like
				   PageDown.  In that case, don't bind
				   the key sequence.  */
				if (decode_keys(seq, seqlen, keyseq,
						sizeof keyseq))
				{
					keyseq[sizeof keyseq - 1] = '\0';
					ke.type = ISFUNC;
					ke.function =
					  action < A_NCOMMANDS
					  ? &function_doc_array[action]
					  : NULL;
					keymap_bind_keyseq(map, keyseq, &ke);
				}
			}
			break;
		}
	}
	if (state != getseq)
		info_error(_("Bad data in infokey file -- some key bindings ignored"));
	return !stop;
}

/* Convert an infokey file section to variable settings.
 */
static void
section_to_vars(table, len)
	unsigned char *table;
	unsigned int len;
{
	enum { getvar, gotvar, getval, gotval } state = getvar;
	unsigned char *var = NULL;
	unsigned char *val = NULL;
	unsigned char *p;

	for (p = table; p - table < len; p++)
	  {
	    switch (state)
	      {
	      case getvar:
		if (*p)
		  {
		    var = p;
		    state = gotvar;
		  }
		break;

	      case gotvar:
		if (!*p)
		  state = getval;
		break;

	      case getval:
		if (*p)
		  {
		    val = p;
		    state = gotval;
		  }
		break;

	      case gotval:
		if (!*p)
		  {
		    set_variable_to_value(var, val);
		    state = getvar;
		  }
		break;
	      }
	  }
      if (state != getvar)
	info_error(_("Bad data in infokey file -- some var settings ignored"));
}

void
initialize_info_keymaps ()
{
  int i;
  int suppress_info_default_bindings = 0;
  int suppress_ea_default_bindings = 0;
  Keymap map;

  if (!info_keymap)
    {
      info_keymap = keymap_make_keymap ();
      echo_area_keymap = keymap_make_keymap ();
    }

  /* Bind the echo area insert routines. */
  for (i = 0; i < 256; i++)
    if (isprint (i))
      echo_area_keymap[i].function = InfoCmd(ea_insert);

  /* Get user-defined keys and variables.  */
  if (fetch_user_maps())
    {
      if (user_info_keys_len && user_info_keys[0])
	suppress_info_default_bindings = 1;
      if (user_ea_keys_len && user_ea_keys[0])
	suppress_ea_default_bindings = 1;
    }

  /* Apply the default bindings, unless the user says to suppress
     them.  */
  if (vi_keys_p)
    {
      if (!suppress_info_default_bindings)
	section_to_keymaps(info_keymap, default_vi_like_info_keys,
			   sizeof(default_vi_like_info_keys));
      if (!suppress_ea_default_bindings)
	  section_to_keymaps(echo_area_keymap, default_vi_like_ea_keys,
			     sizeof(default_vi_like_ea_keys));
    }
  else
    {
      if (!suppress_info_default_bindings)
	section_to_keymaps(info_keymap, default_emacs_like_info_keys,
			   sizeof(default_emacs_like_info_keys));
      if (!suppress_ea_default_bindings)
	  section_to_keymaps(echo_area_keymap, default_emacs_like_ea_keys,
			     sizeof(default_emacs_like_ea_keys));
    }

  /* If the user specified custom bindings, apply them on top of the
     default ones.  */
  if (user_info_keys_len)
    section_to_keymaps(info_keymap, user_info_keys, user_info_keys_len);

  if (user_ea_keys_len)
    section_to_keymaps(echo_area_keymap, user_ea_keys, user_ea_keys_len);

  if (user_vars_len)
    section_to_vars(user_vars, user_vars_len);
}

#endif /* defined(INFOKEY) */
