/* session.h -- Functions found in session.c.
   $Id: session.h,v 1.3 2004/04/11 17:56:46 karl Exp $

   Copyright (C) 1993, 1998, 1999, 2001, 2002, 2004 Free Software
   Foundation, Inc.

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

#ifndef SESSION_H
#define SESSION_H

#include "info.h"
#include "dribble.h"

/* All commands that can be invoked from within info_session () receive
   arguments in the same way.  This simple define declares the header
   of a function named NAME, with associated documentation DOC.  The
   documentation string is groveled out of the source files by the
   utility program `makedoc', which is also responsible for making
   the documentation/function-pointer maps. */
#define DECLARE_INFO_COMMAND(name, doc) \
void name (WINDOW *window, int count, unsigned char key)

/* Variables found in session.h. */
extern VFunction *info_last_executed_command;

/* Variable controlling the garbage collection of files briefly visited
   during searches.  Such files are normally gc'ed, unless they were
   compressed to begin with.  If this variable is non-zero, it says
   to gc even those file buffer contents which had to be uncompressed. */
extern int gc_compressed_files;

/* When non-zero, tiling takes place automatically when info_split_window
   is called. */
extern int auto_tiling_p;

/* Variable controlling the behaviour of default scrolling when you are
   already at the bottom of a node. */
extern int info_scroll_behaviour;
extern char *info_scroll_choices[];

/* Values for info_scroll_behaviour. */
#define IS_Continuous 0 /* Try to get first menu item, or failing that, the
                           "Next:" pointer, or failing that, the "Up:" and
                           "Next:" of the up. */
#define IS_NextOnly   1 /* Try to get "Next:" menu item. */
#define IS_PageOnly   2 /* Simply give up at the bottom of a node. */

/* Utility functions found in session.c */
extern void info_dispatch_on_key (unsigned char key, Keymap map);
extern unsigned char info_get_input_char (void);
extern unsigned char info_get_another_input_char (void);
extern unsigned char info_input_pending_p (void);
extern void remember_window_and_node (WINDOW *window, NODE *node);
extern void set_remembered_pagetop_and_point (WINDOW *window);
extern void set_window_pagetop (WINDOW *window, int desired_top);
extern void info_set_node_of_window (int remember, WINDOW *window,
    NODE *node);
extern void initialize_keyseq (void);
extern void add_char_to_keyseq (char character);
extern void info_gather_typeahead (void);
extern FILE_BUFFER *file_buffer_of_window (WINDOW *window);
extern long info_search_in_node (char *string, NODE *node,
    long int start, WINDOW *window, int dir, int case_sensitive);
extern long info_target_search_node (NODE *node, char *string,
    long int start);
extern void info_select_reference (WINDOW *window, REFERENCE *entry);
extern int info_any_buffered_input_p (void);
extern void print_node (NODE *node);
extern void dump_node_to_file (NODE *node, char *filename,
    int dump_subnodes);
extern void dump_nodes_to_file (char *filename, char **nodenames,
    char *output_filename, int dump_subnodes);
extern char *program_name_from_file_name (char *file_name);

/* Do the physical deletion of WINDOW, and forget this window and
   associated nodes. */
extern void info_delete_window_internal (WINDOW *window);

/* Tell Info that input is coming from the file FILENAME. */
extern void info_set_input_from_file (char *filename);

#define return_if_control_g(val) \
  do { \
    info_gather_typeahead (); \
    if (info_input_pending_p () == Control ('g')) \
      return (val); \
  } while (0)

/* The names of the functions that run an info session. */

/* Starting an info session. */
extern void begin_multiple_window_info_session (char *filename,
    char **nodenames);
extern void begin_info_session (NODE *initial_node);
extern void begin_info_session_with_error (NODE *initial_node,
    char *format, void *arg1, void *arg2);
extern void info_session (void);
extern void initialize_info_session (NODE *node, int clear_screen);
extern void info_read_and_dispatch (void);
extern void info_intuit_options_node (WINDOW *window,
    NODE *initial_node, char *program);

/* Moving the point within a node. */
extern void info_next_line (WINDOW *window, int count, unsigned char key);
extern void info_prev_line (WINDOW *window, int count, unsigned char key);
extern void info_end_of_line (WINDOW *window, int count, unsigned char key);
extern void info_beginning_of_line (WINDOW *window, int count, unsigned char key);
extern void info_forward_char (WINDOW *window, int count, unsigned char key);
extern void info_backward_char (WINDOW *window, int count, unsigned char key);
extern void info_forward_word (WINDOW *window, int count, unsigned char key);
extern void info_backward_word (WINDOW *window, int count, unsigned char key);
extern void info_beginning_of_node (WINDOW *window, int count, unsigned char key);
extern void info_end_of_node (WINDOW *window, int count, unsigned char key);
extern void info_move_to_prev_xref (WINDOW *window, int count, unsigned char key);
extern void info_move_to_next_xref (WINDOW *window, int count, unsigned char key);

/* Scrolling text within a window. */
extern void info_scroll_forward (WINDOW *window, int count, unsigned char key);
extern void info_scroll_backward (WINDOW *window, int count, unsigned char key);
extern void info_redraw_display (WINDOW *window, int count, unsigned char key);
extern void info_toggle_wrap (WINDOW *window, int count, unsigned char key);
extern void info_move_to_window_line (WINDOW *window, int count,
    unsigned char key);
extern void info_up_line (WINDOW *window, int count, unsigned char key);
extern void info_down_line (WINDOW *window, int count, unsigned char key);
extern void info_scroll_half_screen_down (WINDOW *window, int count,
    unsigned char key);
extern void info_scroll_half_screen_up (WINDOW *window, int count,
    unsigned char key);
extern void info_scroll_forward_set_window (WINDOW *window, int count,
    unsigned char key);
extern void info_scroll_forward_page_only (WINDOW *window, int count,
    unsigned char key);
extern void info_scroll_forward_page_only_set_window (WINDOW *window, int count,
    unsigned char key);
extern void info_scroll_backward_set_window (WINDOW *window, int count,
    unsigned char key);
extern void info_scroll_backward_page_only (WINDOW *window, int count,
    unsigned char key);
extern void info_scroll_backward_page_only_set_window (WINDOW *window, int count,
    unsigned char key);
extern void info_scroll_other_window_backward (WINDOW *window, int count,
    unsigned char key);

/* Manipulating multiple windows. */
extern void info_split_window (WINDOW *window, int count, unsigned char key);
extern void info_delete_window (WINDOW *window, int count, unsigned char key);
extern void info_keep_one_window (WINDOW *window, int count, unsigned char key);
extern void info_grow_window (WINDOW *window, int count, unsigned char key);
extern void info_scroll_other_window (WINDOW *window, int count,
    unsigned char key);
extern void info_tile_windows (WINDOW *window, int count, unsigned char key);
extern void info_next_window (WINDOW *window, int count, unsigned char key);
extern void info_prev_window (WINDOW *window, int count, unsigned char key);

/* Selecting nodes. */
extern void info_next_node (WINDOW *window, int count, unsigned char key);
extern void info_prev_node (WINDOW *window, int count, unsigned char key);
extern void info_up_node (WINDOW *window, int count, unsigned char key);
extern void info_last_node (WINDOW *window, int count, unsigned char key);
extern void info_first_node (WINDOW *window, int count, unsigned char key);
extern void info_history_node (WINDOW *window, int count, unsigned char key);
extern void info_goto_node (WINDOW *window, int count, unsigned char key);
extern void info_goto_invocation_node (WINDOW *window, int count,
    unsigned char key);
extern void info_top_node (WINDOW *window, int count, unsigned char key);
extern void info_dir_node (WINDOW *window, int count, unsigned char key);
extern void info_global_next_node (WINDOW *window, int count, unsigned char key);
extern void info_global_prev_node (WINDOW *window, int count, unsigned char key);
extern void info_kill_node (WINDOW *window, int count, unsigned char key);
extern void info_view_file (WINDOW *window, int count, unsigned char key);
extern void info_menu_sequence (WINDOW *window, int count, unsigned char key);
extern NODE *info_follow_menus (NODE *initial_node, char **menus,
    const char **errstr, char **errarg1, char **errarg2);
extern void info_man (WINDOW *window, int count, unsigned char key);
extern void list_visited_nodes (WINDOW *window, int count, unsigned char key);
extern void select_visited_node (WINDOW *window, int count, unsigned char key);

/* Selecting cross references. */
extern void info_menu_digit (WINDOW *window, int count, unsigned char key);
extern void info_menu_item (WINDOW *window, int count, unsigned char key);
extern void info_xref_item (WINDOW *window, int count, unsigned char key);
extern void info_find_menu (WINDOW *window, int count, unsigned char key);
extern void info_select_reference_this_line (WINDOW *window, int count,
    unsigned char key);
extern void info_last_menu_item (WINDOW *window, int count, unsigned char key);
extern void info_visit_menu (WINDOW *window, int count, unsigned char key);

/* Hacking numeric arguments. */
extern int info_explicit_arg, info_numeric_arg, info_numeric_arg_sign;

extern void info_add_digit_to_numeric_arg (WINDOW *window, int count,
    unsigned char key);
extern void info_universal_argument (WINDOW *window, int count,
    unsigned char key);
extern void info_initialize_numeric_arg (void);
extern void info_numeric_arg_digit_loop (WINDOW *window, int count,
    unsigned char key);

/* Searching commands. */
extern void info_search (WINDOW *window, int count, unsigned char key);
extern void isearch_forward (WINDOW *window, int count, unsigned char key);
extern void isearch_backward (WINDOW *window, int count, unsigned char key);
extern void info_search_case_sensitively (WINDOW *window, int count,
    unsigned char key);
extern void info_search_backward (WINDOW *window, int count, unsigned char key);
extern void info_search_next (WINDOW *window, int count, unsigned char key);
extern void info_search_previous (WINDOW *window, int count, unsigned char key);

/* Dumping and printing nodes. */
extern void info_print_node (WINDOW *window, int count, unsigned char key);

/* Footnotes.  */
extern void info_show_footnotes (WINDOW *window, int count, unsigned char key);

/* Miscellaneous commands. */
extern void info_abort_key (WINDOW *window, int count, unsigned char key);
extern void info_quit (WINDOW *window, int count, unsigned char key);
extern void info_do_lowercase_version (WINDOW *window, int count,
    unsigned char key);

#endif /* not SESSION_H */
