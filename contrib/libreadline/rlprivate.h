/* rlprivate.h -- functions and variables global to the readline library,
		  but not intended for use by applications. */

/* Copyright (C) 1999 Free Software Foundation, Inc.

   This file is part of the GNU Readline Library, a library for
   reading lines of text with interactive input and history editing.

   The GNU Readline Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The GNU Readline Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   59 Temple Place, Suite 330, Boston, MA 02111 USA. */

#if !defined (_RL_PRIVATE_H_)
#define _RL_PRIVATE_H_

#include "rlconf.h"	/* for VISIBLE_STATS */
#include "rlstdc.h"
#include "posixjmp.h" /* defines procenv_t */

/*************************************************************************
 *									 *
 * Global functions undocumented in texinfo manual and not in readline.h *
 *									 *
 *************************************************************************/

/* terminal.c */
extern char *rl_get_termcap __P((const char *));

/*************************************************************************
 *									 *
 * Global variables undocumented in texinfo manual and not in readline.h *
 *									 *
 *************************************************************************/

/* complete.c */
extern int rl_complete_with_tilde_expansion;
#if defined (VISIBLE_STATS)
extern int rl_visible_stats;
#endif /* VISIBLE_STATS */

/* readline.c */
extern int rl_line_buffer_len;
extern int rl_arg_sign;
extern int rl_visible_prompt_length;
extern int readline_echoing_p;
extern int rl_key_sequence_length;

/* display.c */
extern int rl_display_fixed;

/* parens.c */
extern int rl_blink_matching_paren;

/*************************************************************************
 *									 *
 * Global functions and variables unsed and undocumented		 *
 *									 *
 *************************************************************************/

/* bind.c */
extern char *rl_untranslate_keyseq __P((int));

/* kill.c */
extern int rl_set_retained_kills __P((int));

/* readline.c */
extern int rl_discard_argument __P((void));

/* rltty.c */
extern int rl_stop_output __P((int, int));

/* terminal.c */
extern void _rl_set_screen_size __P((int, int));

/* undo.c */
extern int _rl_fix_last_undo_of_type __P((int, int, int));

/* util.c */
extern char *_rl_savestring __P((const char *));

/*************************************************************************
 *									 *
 * Functions and variables private to the readline library		 *
 *									 *
 *************************************************************************/

/* NOTE: Functions and variables prefixed with `_rl_' are
   pseudo-global: they are global so they can be shared
   between files in the readline library, but are not intended
   to be visible to readline callers. */

/*************************************************************************
 * Undocumented private functions					 *
 *************************************************************************/

#if defined(READLINE_CALLBACKS)

/* readline.c */
extern void readline_internal_setup __P((void));
extern char *readline_internal_teardown __P((int));
extern int readline_internal_char __P((void));

#endif /* READLINE_CALLBACKS */

/* bind.c */
extern void _rl_bind_if_unbound __P((const char *, rl_command_func_t *));

/* display.c */
extern char *_rl_strip_prompt __P((char *));
extern void _rl_move_cursor_relative __P((int, const char *));
extern void _rl_move_vert __P((int));
extern void _rl_save_prompt __P((void));
extern void _rl_restore_prompt __P((void));
extern char *_rl_make_prompt_for_search __P((int));
extern void _rl_erase_at_end_of_line __P((int));
extern void _rl_clear_to_eol __P((int));
extern void _rl_clear_screen __P((void));
extern void _rl_update_final __P((void));
extern void _rl_redisplay_after_sigwinch __P((void));
extern void _rl_clean_up_for_exit __P((void));
extern void _rl_erase_entire_line __P((void));
extern int _rl_current_display_line __P((void));

/* input.c */
extern int _rl_any_typein __P((void));
extern int _rl_input_available __P((void));
extern void _rl_insert_typein __P((int));

/* macro.c */
extern void _rl_with_macro_input __P((char *));
extern int _rl_next_macro_key __P((void));
extern void _rl_push_executing_macro __P((void));
extern void _rl_pop_executing_macro __P((void));
extern void _rl_add_macro_char __P((int));
extern void _rl_kill_kbd_macro __P((void));

/* nls.c */
extern int _rl_init_eightbit __P((void));

/* parens.c */
extern void _rl_enable_paren_matching __P((int));

/* readline.c */
extern void _rl_init_line_state __P((void));
extern void _rl_set_the_line __P((void));
extern int _rl_dispatch __P((int, Keymap));
extern int _rl_init_argument __P((void));
extern void _rl_fix_point __P((int));
extern void _rl_replace_text __P((const char *, int, int));
extern int _rl_char_search_internal __P((int, int, int));
extern int _rl_set_mark_at_pos __P((int));
extern int _rl_free_saved_history_line __P((void));

/* rltty.c */
extern int _rl_disable_tty_signals __P((void));
extern int _rl_restore_tty_signals __P((void));

/* terminal.c */
extern void _rl_get_screen_size __P((int, int));
extern int _rl_init_terminal_io __P((const char *));
#ifdef _MINIX
extern void _rl_output_character_function __P((int));
#else
extern int _rl_output_character_function __P((int));
#endif
extern void _rl_output_some_chars __P((const char *, int));
extern int _rl_backspace __P((int));
extern void _rl_enable_meta_key __P((void));
extern void _rl_control_keypad __P((int));

/* util.c */
extern int rl_alphabetic __P((int));
extern int _rl_abort_internal __P((void));
extern char *_rl_strindex __P((const char *, const char *));
extern char *_rl_strpbrk __P((const char *, const char *));
extern int _rl_qsort_string_compare __P((char **, char **));
extern int (_rl_uppercase_p) __P((int));
extern int (_rl_lowercase_p) __P((int));
extern int (_rl_pure_alphabetic) __P((int));
extern int (_rl_digit_p) __P((int));
extern int (_rl_to_lower) __P((int));
extern int (_rl_to_upper) __P((int));
extern int (_rl_digit_value) __P((int));

/* vi_mode.c */
extern void _rl_vi_initialize_line __P((void));
extern void _rl_vi_reset_last __P((void));
extern void _rl_vi_set_last __P((int, int, int));
extern int _rl_vi_textmod_command __P((int));
extern void _rl_vi_done_inserting __P((void));

/*************************************************************************
 * Undocumented private variables					 *
 *************************************************************************/

/* bind.c */
extern const char *_rl_possible_control_prefixes[];
extern const char *_rl_possible_meta_prefixes[];

/* complete.c */
extern int _rl_complete_show_all;
extern int _rl_complete_mark_directories;
extern int _rl_print_completions_horizontally;
extern int _rl_completion_case_fold;

/* display.c */
extern int _rl_vis_botlin;
extern int _rl_last_c_pos;
extern int _rl_suppress_redisplay;
extern char *rl_display_prompt;

/* isearch.c */
extern unsigned char *_rl_isearch_terminators;

/* macro.c */
extern int _rl_defining_kbd_macro;
extern char *_rl_executing_macro;

/* readline.c */
extern int _rl_horizontal_scroll_mode;
extern int _rl_mark_modified_lines;
extern int _rl_bell_preference;
extern int _rl_meta_flag;
extern int _rl_convert_meta_chars_to_ascii;
extern int _rl_output_meta_chars;
extern char *_rl_comment_begin;
extern unsigned char _rl_parsing_conditionalized_out;
extern Keymap _rl_keymap;
extern FILE *_rl_in_stream;
extern FILE *_rl_out_stream;
extern int _rl_last_command_was_kill;
extern int _rl_eof_char;
extern procenv_t readline_top_level;

/* terminal.c */
extern int _rl_enable_keypad;
extern int _rl_enable_meta;
extern char *_rl_term_clreol;
extern char *_rl_term_clrpag;
extern char *_rl_term_im;
extern char *_rl_term_ic;
extern char *_rl_term_ei;
extern char *_rl_term_DC;
extern char *_rl_term_up;
extern char *_rl_term_dc;
extern char *_rl_term_cr;
extern char *_rl_term_IC;
extern int _rl_screenheight;
extern int _rl_screenwidth;
extern int _rl_screenchars;
extern int _rl_terminal_can_insert;
extern int _rl_term_autowrap;

/* undo.c */
extern int _rl_doing_an_undo;
extern int _rl_undo_group_level;

#endif /* _RL_PRIVATE_H_ */
