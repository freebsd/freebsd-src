/* funmap.c -- attach names to functions. */

/* Copyright (C) 1988, 1989, 1991 Free Software Foundation, Inc.

   This file is part of GNU Readline, a library for reading lines
   of text with interactive input and history editing.

   Readline is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   Readline is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* #define STATIC_MALLOC */
#if !defined (STATIC_MALLOC)
extern char *xmalloc (), *xrealloc ();
#else
static char *xmalloc (), *xrealloc ();
#endif /* STATIC_MALLOC */

#include "sysdep.h"
#include <stdio.h>

#include "readline.h"

FUNMAP **funmap = (FUNMAP **)NULL;
static int funmap_size = 0;
static int funmap_entry = 0;

/* After initializing the function map, this is the index of the first
   program specific function. */
int funmap_program_specific_entry_start;

static FUNMAP default_funmap[] = {

  { "abort", rl_abort },
  { "accept-line", rl_newline },
  { "arrow-key-prefix", rl_arrow_keys },
  { "backward-char", rl_backward },
  { "backward-delete-char", rl_rubout },
  { "backward-kill-line", rl_backward_kill_line },
  { "backward-kill-word", rl_backward_kill_word },
  { "backward-word", rl_backward_word },
  { "beginning-of-history", rl_beginning_of_history },
  { "beginning-of-line", rl_beg_of_line },
  { "call-last-kbd-macro", rl_call_last_kbd_macro },
  { "capitalize-word", rl_capitalize_word },
  { "clear-screen", rl_clear_screen },
  { "complete", rl_complete },
  { "delete-char", rl_delete },
  { "digit-argument", rl_digit_argument },
  { "do-lowercase-version", rl_do_lowercase_version },
  { "downcase-word", rl_downcase_word },
  { "dump-functions", rl_dump_functions },
  { "end-kbd-macro", rl_end_kbd_macro },
  { "end-of-history", rl_end_of_history },
  { "end-of-line", rl_end_of_line },
  { "forward-char", rl_forward },
  { "forward-search-history", rl_forward_search_history },
  { "forward-word", rl_forward_word },
  { "kill-line", rl_kill_line },
  { "kill-word", rl_kill_word },
  { "next-history", rl_get_next_history },
  { "possible-completions", rl_possible_completions },
  { "previous-history", rl_get_previous_history },
  { "quoted-insert", rl_quoted_insert },
  { "re-read-init-file", rl_re_read_init_file },
  { "redraw-current-line", rl_refresh_line},
  { "reverse-search-history", rl_reverse_search_history },
  { "revert-line", rl_revert_line },
  { "self-insert", rl_insert },
  { "start-kbd-macro", rl_start_kbd_macro },
  { "tab-insert", rl_tab_insert },
  { "transpose-chars", rl_transpose_chars },
  { "transpose-words", rl_transpose_words },
  { "undo", rl_undo_command },
  { "universal-argument", rl_universal_argument },
  { "unix-line-discard", rl_unix_line_discard },
  { "unix-word-rubout", rl_unix_word_rubout },
  { "upcase-word", rl_upcase_word },
  { "yank", rl_yank },
  { "yank-nth-arg", rl_yank_nth_arg },
  { "yank-pop", rl_yank_pop },

#if defined (VI_MODE)

  { "vi-append-eol", rl_vi_append_eol },
  { "vi-append-mode", rl_vi_append_mode },
  { "vi-arg-digit", rl_vi_arg_digit },
  { "vi-bWord", rl_vi_bWord },
  { "vi-bracktype", rl_vi_bracktype },
  { "vi-bword", rl_vi_bword },
  { "vi-change-case", rl_vi_change_case },
  { "vi-change-char", rl_vi_change_char },
  { "vi-change-to", rl_vi_change_to },
  { "vi-char-search", rl_vi_char_search },
  { "vi-column", rl_vi_column },
  { "vi-comment", rl_vi_comment },
  { "vi-complete", rl_vi_complete },
  { "vi-delete", rl_vi_delete },
  { "vi-delete-to", rl_vi_delete_to },
  { "vi-dosearch", rl_vi_dosearch },
  { "vi-eWord", rl_vi_eWord },
  { "vi-editing-mode", rl_vi_editing_mode },
  { "vi-end-word", rl_vi_end_word },
  { "vi-eof-maybe", rl_vi_eof_maybe },
  { "vi-eword", rl_vi_eword },
  { "vi-fWord", rl_vi_fWord },
  { "vi-first-print", rl_vi_first_print },
  { "vi-fword", rl_vi_fword },
  { "vi-insert-beg", rl_vi_insert_beg },
  { "vi-insertion-mode", rl_vi_insertion_mode },
  { "vi-match", rl_vi_match },
  { "vi-movement-mode", rl_vi_movement_mode },
  { "vi-next-word", rl_vi_next_word },
  { "vi-overstrike", rl_vi_overstrike },
  { "vi-overstrike-delete", rl_vi_overstrike_delete },
  { "vi-prev-word", rl_vi_prev_word },
  { "vi-put", rl_vi_put },
  { "vi-replace, ", rl_vi_replace },
  { "vi-search", rl_vi_search },
  { "vi-search-again", rl_vi_search_again },
  { "vi-subst", rl_vi_subst },
  { "vi-yank-arg", rl_vi_yank_arg },
  { "vi-yank-to", rl_vi_yank_to },

#endif /* VI_MODE */

 {(char *)NULL, (Function *)NULL }
};

rl_add_funmap_entry (name, function)
     char *name;
     Function *function;
{
  if (funmap_entry + 2 >= funmap_size)
    if (!funmap)
      funmap = (FUNMAP **)xmalloc ((funmap_size = 80) * sizeof (FUNMAP *));
    else
      funmap =
	(FUNMAP **)xrealloc (funmap, (funmap_size += 80) * sizeof (FUNMAP *));
  
  funmap[funmap_entry] = (FUNMAP *)xmalloc (sizeof (FUNMAP));
  funmap[funmap_entry]->name = name;
  funmap[funmap_entry]->function = function;

  funmap[++funmap_entry] = (FUNMAP *)NULL;
}

static int funmap_initialized = 0;

/* Make the funmap contain all of the default entries. */
rl_initialize_funmap ()
{
  register int i;

  if (funmap_initialized)
    return;

  for (i = 0; default_funmap[i].name; i++)
    rl_add_funmap_entry (default_funmap[i].name, default_funmap[i].function);

  funmap_initialized = 1;
  funmap_program_specific_entry_start = i;
}

/* Stupid comparison routine for qsort () ing strings. */
static int
qsort_string_compare (s1, s2)
     register char **s1, **s2;
{
  return (strcmp (*s1, *s2));
}

/* Produce a NULL terminated array of known function names.  The array
   is sorted.  The array itself is allocated, but not the strings inside.
   You should free () the array when you done, but not the pointrs. */
char **
rl_funmap_names ()
{
  char **result = (char **)NULL;
  int result_size, result_index;

  result_size = result_index = 0;

  /* Make sure that the function map has been initialized. */
  rl_initialize_funmap ();

  for (result_index = 0; funmap[result_index]; result_index++)
    {
      if (result_index + 2 > result_size)
	{
	  if (!result)
	    result = (char **)xmalloc ((result_size = 20) * sizeof (char *));
	  else
	    result = (char **)
	      xrealloc (result, (result_size += 20) * sizeof (char *));
	}

      result[result_index] = funmap[result_index]->name;
      result[result_index + 1] = (char *)NULL;
    }

  qsort (result, result_index, sizeof (char *), qsort_string_compare);
  return (result);
}

/* Things that mean `Control'. */
char *possible_control_prefixes[] = {
  "Control-", "C-", "CTRL-", (char *)NULL
};

char *possible_meta_prefixes[] = {
  "Meta", "M-", (char *)NULL
};

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
    temp = (char *)malloc (bytes);
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
