/* Readline.h -- the names of functions callable from within readline. */

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

#if !defined (_READLINE_H_)
#define _READLINE_H_

#include <readline/keymaps.h>

/* The functions for manipulating the text of the line within readline.
Most of these functions are bound to keys by default. */
extern int
  rl_tilde_expand (),
  rl_beg_of_line (), rl_backward (), rl_delete (), rl_end_of_line (),
  rl_forward (), ding (), rl_backward (), rl_newline (), rl_kill_line (),
  rl_clear_screen (), rl_get_next_history (), rl_get_previous_history (),
  rl_quoted_insert (), rl_reverse_search_history (), rl_transpose_chars (),
  rl_unix_line_discard (), rl_quoted_insert (), rl_unix_word_rubout (),
  rl_yank (), rl_rubout (), rl_backward_word (), rl_kill_word (),
  rl_forward_word (), rl_tab_insert (), rl_yank_pop (), rl_yank_nth_arg (),
  rl_backward_kill_word (), rl_backward_kill_line (), rl_transpose_words (),
  rl_complete (), rl_possible_completions (), rl_insert_completions (),
  rl_do_lowercase_version (),
  rl_digit_argument (), rl_universal_argument (), rl_abort (),
  rl_undo_command (), rl_revert_line (), rl_beginning_of_history (),
  rl_end_of_history (), rl_forward_search_history (), rl_insert (),
  rl_upcase_word (), rl_downcase_word (), rl_capitalize_word (),
  rl_restart_output (), rl_re_read_init_file (), rl_dump_functions (),
  rl_delete_horizontal_space ();

extern int rl_insert_close ();

/* These are *both* defined even when VI_MODE is not. */
extern int rl_vi_editing_mode (), rl_emacs_editing_mode ();

/* Non incremental history searching. */
extern int
  rl_noninc_forward_search (), rl_noninc_reverse_search (),
  rl_noninc_forward_search_again (), rl_noninc_reverse_search_again ();

/* Things for vi mode. */
extern int
  rl_vi_redo (), rl_vi_tilde_expand (),
  rl_vi_movement_mode (), rl_vi_insertion_mode (), rl_vi_arg_digit (),
  rl_vi_prev_word (), rl_vi_next_word (), rl_vi_char_search (),
  rl_vi_eof_maybe (), rl_vi_append_mode (), rl_vi_put (),
  rl_vi_append_eol (), rl_vi_insert_beg (), rl_vi_delete (), rl_vi_comment (),
  rl_vi_first_print (), rl_vi_fword (), rl_vi_fWord (), rl_vi_bword (),
  rl_vi_bWord (), rl_vi_eword (), rl_vi_eWord (), rl_vi_end_word (),
  rl_vi_change_case (), rl_vi_match (), rl_vi_bracktype (),
  rl_vi_change_char (), rl_vi_yank_arg (), rl_vi_search (),
  rl_vi_search_again (),  rl_vi_subst (), rl_vi_overstrike (),
  rl_vi_overstrike_delete (), rl_vi_replace(), rl_vi_column (),
  rl_vi_delete_to (), rl_vi_change_to (), rl_vi_yank_to (),
  rl_vi_complete (), rl_vi_fetch_history ();

/* Keyboard macro commands. */
extern int
rl_start_kbd_macro (), rl_end_kbd_macro (), rl_call_last_kbd_macro ();

extern int rl_arrow_keys(), rl_refresh_line ();

/* Maintaining the state of undo.  We remember individual deletes and inserts
   on a chain of things to do. */

/* The actions that undo knows how to undo.  Notice that UNDO_DELETE means
   to insert some text, and UNDO_INSERT means to delete some text.   I.e.,
   the code tells undo what to undo, not how to undo it. */
enum undo_code { UNDO_DELETE, UNDO_INSERT, UNDO_BEGIN, UNDO_END };

/* What an element of THE_UNDO_LIST looks like. */
typedef struct undo_list {
  struct undo_list *next;
  int start, end;		/* Where the change took place. */
  char *text;			/* The text to insert, if undoing a delete. */
  enum undo_code what;		/* Delete, Insert, Begin, End. */
} UNDO_LIST;

/* The current undo list for RL_LINE_BUFFER. */
extern UNDO_LIST *rl_undo_list;

/* The data structure for mapping textual names to code addresses. */
typedef struct {
  char *name;
  Function *function;
} FUNMAP;

extern FUNMAP **funmap;

/* **************************************************************** */
/*								    */
/*			Well Published Variables		    */
/*								    */
/* **************************************************************** */

/* The name of the calling program.  You should initialize this to
   whatever was in argv[0].  It is used when parsing conditionals. */
extern char *rl_readline_name;

/* The line buffer that is in use. */
extern char *rl_line_buffer;

/* The location of point, and end. */
extern int rl_point, rl_end;

/* The name of the terminal to use. */
extern char *rl_terminal_name;

/* The input and output streams. */
extern FILE *rl_instream, *rl_outstream;

/* The basic list of characters that signal a break between words for the
   completer routine.  The initial contents of this variable is what
   breaks words in the shell, i.e. "n\"\\'`@$>". */
extern char *rl_basic_word_break_characters;

/* The list of characters that signal a break between words for
   rl_complete_internal.  The default list is the contents of
   rl_basic_word_break_characters.  */
extern char *rl_completer_word_break_characters;

/* List of characters which are used to quote a substring of the command
   line, upon which completion is to be performed for the entire substring.
   Within quoted substrings, rl_completer_word_break_characters are treated
   as normal characters, unless they also appear in this list. */
extern char *rl_completer_quote_characters;

/* List of characters that are word break characters, but should be left
   in TEXT when it is passed to the completion function.  The shell uses
   this to help determine what kind of completing to do. */
extern char *rl_special_prefixes;

/* Pointer to the generator function for completion_matches ().
   NULL means to use filename_entry_function (), the default filename
   completer. */
extern Function *rl_completion_entry_function;

/* If rl_ignore_some_completions_function is non-NULL it is the address
   of a function to call after all of the possible matches have been
   generated, but before the actual completion is done to the input line.
   The function is called with one argument; a NULL terminated array
   of (char *).  If your function removes any of the elements, they
   must be free()'ed. */
extern Function *rl_ignore_some_completions_function;

/* Pointer to alternative function to create matches.
   Function is called with TEXT, START, and END.
   START and END are indices in RL_LINE_BUFFER saying what the boundaries
   of TEXT are.
   If this function exists and returns NULL then call the value of
   rl_completion_entry_function to try to match, otherwise use the
   array of strings returned. */
extern CPPFunction *rl_attempted_completion_function;

/* If non-zero, then this is the address of a function to call just
   before readline_internal () prints the first prompt. */
extern Function *rl_startup_hook;

/* If non-zero, then this is the address of a function to call when
   completing on a directory name.  The function is called with
   the address of a string (the current directory name) as an arg. */
extern Function *rl_symbolic_link_hook;

/* The address of a function to call periodically while Readline is
   awaiting character input, or NULL, for no event handling. */
extern Function *rl_event_hook;

/* Non-zero means that modified history lines are preceded
   with an asterisk. */
extern int rl_show_star;


/* **************************************************************** */
/*                                                                  */
/*             Tilde Variables That Can be Externally Set           */
/*                                                                  */
/* **************************************************************** */

/* If non-null, this contains the address of a function to call if the
   standard meaning for expanding a tilde fails.  The function is called
   with the text (sans tilde, as in "foo"), and returns a malloc()'ed string
   which is the expansion, or a NULL pointer if there is no expansion. */
extern Function *tilde_expansion_failure_hook;

/* When non-null, this is a NULL terminated array of strings which
   are duplicates for a tilde prefix.  Bash uses this to expand
   `=~' and `:~'. */
extern char **tilde_additional_prefixes;

/* When non-null, this is a NULL terminated array of strings which match
   the end of a username, instead of just "/".  Bash sets this to
   `/' and `:'. */
extern char **tilde_additional_suffixes;

/* **************************************************************** */
/*								    */
/*			Well Published Functions		    */
/*								    */
/* **************************************************************** */

/* Read a line of input.  Prompt with PROMPT.  A NULL PROMPT means none. */
extern char *readline ();

/* Return an array of strings which are the result of repeatadly calling
   FUNC with TEXT. */
extern char **completion_matches ();

/* rl_add_defun (char *name, Function *function, int key)
   Add NAME to the list of named functions.  Make FUNCTION
   be the function that gets called.
   If KEY is not -1, then bind it. */
extern int rl_add_defun ();

#endif /* _READLINE_H_ */
