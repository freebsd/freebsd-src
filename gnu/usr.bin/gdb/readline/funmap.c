/* funmap.c -- attach names to functions. */

/* Copyright (C) 1988,1989 Free Software Foundation, Inc.

   This file is part of GNU Readline, a library for reading lines
   of text with interactive input and history editing.

   Readline is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 1, or (at your option) any
   later version.

   Readline is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Readline; see the file COPYING.  If not, write to the Free
   Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#define STATIC_MALLOC
#ifndef STATIC_MALLOC
extern char *xmalloc (), *xrealloc ();
#else
static char *xmalloc (), *xrealloc ();
#endif

#ifndef FILE
#include <stdio.h>
#endif /* FILE */

#include "readline.h"

FUNMAP **funmap = (FUNMAP **)NULL;
static int funmap_size = 0;

static int just_testing_ar_tmp = 0;
static int just_testing_ar_tmp_2 = 5;
int foo_testing_ar;

static int funmap_entry = 0;

static FUNMAP default_funmap[] = {
  { "beginning-of-line", rl_beg_of_line },
  { "backward-char", rl_backward },
  { "delete-char", rl_delete },
  { "end-of-line", rl_end_of_line },
  { "forward-char", rl_forward },
  { "accept-line", rl_newline },
  { "kill-line", rl_kill_line },
  { "clear-screen", rl_clear_screen },
  { "next-history", rl_get_next_history },
  { "previous-history", rl_get_previous_history },
  { "quoted-insert", rl_quoted_insert },
  { "reverse-search-history", rl_reverse_search_history },
  { "forward-search-history", rl_forward_search_history },
  { "transpose-chars", rl_transpose_chars },
  { "unix-line-discard", rl_unix_line_discard },
  { "unix-word-rubout", rl_unix_word_rubout },
  { "yank", rl_yank },
  { "yank-pop", rl_yank_pop },
  { "yank-nth-arg", rl_yank_nth_arg },
  { "backward-delete-char", rl_rubout },
  { "backward-word", rl_backward_word },
  { "kill-word", rl_kill_word },
  { "forward-word", rl_forward_word },
  { "tab-insert", rl_tab_insert },
  { "backward-kill-word", rl_backward_kill_word },
  { "backward-kill-line", rl_backward_kill_line },
  { "transpose-words", rl_transpose_words },
  { "digit-argument", rl_digit_argument },
  { "complete", rl_complete },
  { "possible-completions", rl_possible_completions },
  { "do-lowercase-version", rl_do_lowercase_version },
  { "digit-argument", rl_digit_argument },
  { "universal-argument", rl_universal_argument },
  { "abort", rl_abort },
  { "undo", rl_undo_command },
  { "upcase-word", rl_upcase_word },
  { "downcase-word", rl_downcase_word },
  { "capitalize-word", rl_capitalize_word },
  { "revert-line", rl_revert_line },
  { "beginning-of-history", rl_beginning_of_history },
  { "end-of-history", rl_end_of_history },
  { "self-insert", rl_insert },
  { "start-kbd-macro", rl_start_kbd_macro },
  { "end-kbd-macro", rl_end_kbd_macro },
  { "re-read-init-file", rl_re_read_init_file },
#ifdef VI_MODE
  { "vi-movement-mode", rl_vi_movement_mode },
  { "vi-insertion-mode", rl_vi_insertion_mode },
  { "vi-arg-digit", rl_vi_arg_digit },
  { "vi-prev-word", rl_vi_prev_word },
  { "vi-next-word", rl_vi_next_word },
  { "vi-char-search", rl_vi_char_search },
  { "vi-editing-mode", rl_vi_editing_mode },
  { "vi-eof-maybe", rl_vi_eof_maybe },
  { "vi-append-mode", rl_vi_append_mode },
  { "vi-put", rl_vi_put },
  { "vi-append-eol", rl_vi_append_eol },
  { "vi-insert-beg", rl_vi_insert_beg },
  { "vi-delete", rl_vi_delete },
  { "vi-comment", rl_vi_comment },
  { "vi-first-print", rl_vi_first_print },
  { "vi-fword", rl_vi_fword },
  { "vi-fWord", rl_vi_fWord },
  { "vi-bword", rl_vi_bword },
  { "vi-bWord", rl_vi_bWord },
  { "vi-eword", rl_vi_eword },
  { "vi-eWord", rl_vi_eWord },
  { "vi-end-word", rl_vi_end_word },
  { "vi-change-case", rl_vi_change_case },
  { "vi-match", rl_vi_match },
  { "vi-bracktype", rl_vi_bracktype },
  { "vi-change-char", rl_vi_change_char },
  { "vi-yank-arg", rl_vi_yank_arg },
  { "vi-search", rl_vi_search },
  { "vi-search-again", rl_vi_search_again },
  { "vi-dosearch", rl_vi_dosearch },
  { "vi-subst", rl_vi_subst },
  { "vi-overstrike", rl_vi_overstrike },
  { "vi-overstrike-delete", rl_vi_overstrike_delete },
  { "vi-replace, ", rl_vi_replace },
  { "vi-column", rl_vi_column },
  { "vi-delete-to", rl_vi_delete_to },
  { "vi-change-to", rl_vi_change_to },
  { "vi-yank-to", rl_vi_yank_to },
  { "vi-complete", rl_vi_complete },
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
}

/* Things that mean `Control'. */
char *possible_control_prefixes[] = {
  "Control-", "C-", "CTRL-", (char *)NULL
};

char *possible_meta_prefixes[] = {
  "Meta", "M-", (char *)NULL
};

#ifdef STATIC_MALLOC

/* **************************************************************** */
/*								    */
/*			xmalloc and xrealloc ()		     	    */
/*								    */
/* **************************************************************** */

static char *
xmalloc (bytes)
     int bytes;
{
  static memory_error_and_abort ();
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
  static memory_error_and_abort ();
  char *temp = (char *)realloc (pointer, bytes);

  if (!temp)
    memory_error_and_abort ();
  return (temp);
}

static
memory_error_and_abort ()
{
  fprintf (stderr, "history: Out of virtual memory!\n");
  abort ();
}
#endif /* STATIC_MALLOC */
