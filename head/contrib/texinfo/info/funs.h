/* funs.h -- Generated declarations for Info commands. */

#include "info.h"

/* Functions declared in "./session.c". */
#define A_info_next_line 0
extern void info_next_line (WINDOW *window, int count, unsigned char key);
#define A_info_prev_line 1
extern void info_prev_line (WINDOW *window, int count, unsigned char key);
#define A_info_end_of_line 2
extern void info_end_of_line (WINDOW *window, int count, unsigned char key);
#define A_info_beginning_of_line 3
extern void info_beginning_of_line (WINDOW *window, int count, unsigned char key);
#define A_info_forward_char 4
extern void info_forward_char (WINDOW *window, int count, unsigned char key);
#define A_info_backward_char 5
extern void info_backward_char (WINDOW *window, int count, unsigned char key);
#define A_info_forward_word 6
extern void info_forward_word (WINDOW *window, int count, unsigned char key);
#define A_info_backward_word 7
extern void info_backward_word (WINDOW *window, int count, unsigned char key);
#define A_info_global_next_node 8
extern void info_global_next_node (WINDOW *window, int count, unsigned char key);
#define A_info_global_prev_node 9
extern void info_global_prev_node (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_forward 10
extern void info_scroll_forward (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_forward_set_window 11
extern void info_scroll_forward_set_window (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_forward_page_only 12
extern void info_scroll_forward_page_only (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_forward_page_only_set_window 13
extern void info_scroll_forward_page_only_set_window (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_backward 14
extern void info_scroll_backward (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_backward_set_window 15
extern void info_scroll_backward_set_window (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_backward_page_only 16
extern void info_scroll_backward_page_only (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_backward_page_only_set_window 17
extern void info_scroll_backward_page_only_set_window (WINDOW *window, int count, unsigned char key);
#define A_info_beginning_of_node 18
extern void info_beginning_of_node (WINDOW *window, int count, unsigned char key);
#define A_info_end_of_node 19
extern void info_end_of_node (WINDOW *window, int count, unsigned char key);
#define A_info_down_line 20
extern void info_down_line (WINDOW *window, int count, unsigned char key);
#define A_info_up_line 21
extern void info_up_line (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_half_screen_down 22
extern void info_scroll_half_screen_down (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_half_screen_up 23
extern void info_scroll_half_screen_up (WINDOW *window, int count, unsigned char key);
#define A_info_next_window 24
extern void info_next_window (WINDOW *window, int count, unsigned char key);
#define A_info_prev_window 25
extern void info_prev_window (WINDOW *window, int count, unsigned char key);
#define A_info_split_window 26
extern void info_split_window (WINDOW *window, int count, unsigned char key);
#define A_info_delete_window 27
extern void info_delete_window (WINDOW *window, int count, unsigned char key);
#define A_info_keep_one_window 28
extern void info_keep_one_window (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_other_window 29
extern void info_scroll_other_window (WINDOW *window, int count, unsigned char key);
#define A_info_scroll_other_window_backward 30
extern void info_scroll_other_window_backward (WINDOW *window, int count, unsigned char key);
#define A_info_grow_window 31
extern void info_grow_window (WINDOW *window, int count, unsigned char key);
#define A_info_tile_windows 32
extern void info_tile_windows (WINDOW *window, int count, unsigned char key);
#define A_info_toggle_wrap 33
extern void info_toggle_wrap (WINDOW *window, int count, unsigned char key);
#define A_info_next_node 34
extern void info_next_node (WINDOW *window, int count, unsigned char key);
#define A_info_prev_node 35
extern void info_prev_node (WINDOW *window, int count, unsigned char key);
#define A_info_up_node 36
extern void info_up_node (WINDOW *window, int count, unsigned char key);
#define A_info_last_node 37
extern void info_last_node (WINDOW *window, int count, unsigned char key);
#define A_info_first_node 38
extern void info_first_node (WINDOW *window, int count, unsigned char key);
#define A_info_last_menu_item 39
extern void info_last_menu_item (WINDOW *window, int count, unsigned char key);
#define A_info_menu_digit 40
extern void info_menu_digit (WINDOW *window, int count, unsigned char key);
#define A_info_menu_item 41
extern void info_menu_item (WINDOW *window, int count, unsigned char key);
#define A_info_xref_item 42
extern void info_xref_item (WINDOW *window, int count, unsigned char key);
#define A_info_find_menu 43
extern void info_find_menu (WINDOW *window, int count, unsigned char key);
#define A_info_visit_menu 44
extern void info_visit_menu (WINDOW *window, int count, unsigned char key);
#define A_info_goto_node 45
extern void info_goto_node (WINDOW *window, int count, unsigned char key);
#define A_info_menu_sequence 46
extern void info_menu_sequence (WINDOW *window, int count, unsigned char key);
#define A_info_goto_invocation_node 47
extern void info_goto_invocation_node (WINDOW *window, int count, unsigned char key);
#define A_info_man 48
extern void info_man (WINDOW *window, int count, unsigned char key);
#define A_info_top_node 49
extern void info_top_node (WINDOW *window, int count, unsigned char key);
#define A_info_dir_node 50
extern void info_dir_node (WINDOW *window, int count, unsigned char key);
#define A_info_history_node 51
extern void info_history_node (WINDOW *window, int count, unsigned char key);
#define A_info_kill_node 52
extern void info_kill_node (WINDOW *window, int count, unsigned char key);
#define A_info_view_file 53
extern void info_view_file (WINDOW *window, int count, unsigned char key);
#define A_info_print_node 54
extern void info_print_node (WINDOW *window, int count, unsigned char key);
#define A_info_search_case_sensitively 55
extern void info_search_case_sensitively (WINDOW *window, int count, unsigned char key);
#define A_info_search 56
extern void info_search (WINDOW *window, int count, unsigned char key);
#define A_info_search_backward 57
extern void info_search_backward (WINDOW *window, int count, unsigned char key);
#define A_info_search_next 58
extern void info_search_next (WINDOW *window, int count, unsigned char key);
#define A_info_search_previous 59
extern void info_search_previous (WINDOW *window, int count, unsigned char key);
#define A_isearch_forward 60
extern void isearch_forward (WINDOW *window, int count, unsigned char key);
#define A_isearch_backward 61
extern void isearch_backward (WINDOW *window, int count, unsigned char key);
#define A_info_move_to_prev_xref 62
extern void info_move_to_prev_xref (WINDOW *window, int count, unsigned char key);
#define A_info_move_to_next_xref 63
extern void info_move_to_next_xref (WINDOW *window, int count, unsigned char key);
#define A_info_select_reference_this_line 64
extern void info_select_reference_this_line (WINDOW *window, int count, unsigned char key);
#define A_info_abort_key 65
extern void info_abort_key (WINDOW *window, int count, unsigned char key);
#define A_info_move_to_window_line 66
extern void info_move_to_window_line (WINDOW *window, int count, unsigned char key);
#define A_info_redraw_display 67
extern void info_redraw_display (WINDOW *window, int count, unsigned char key);
#define A_info_quit 68
extern void info_quit (WINDOW *window, int count, unsigned char key);
#define A_info_do_lowercase_version 69
extern void info_do_lowercase_version (WINDOW *window, int count, unsigned char key);
#define A_info_add_digit_to_numeric_arg 70
extern void info_add_digit_to_numeric_arg (WINDOW *window, int count, unsigned char key);
#define A_info_universal_argument 71
extern void info_universal_argument (WINDOW *window, int count, unsigned char key);
#define A_info_numeric_arg_digit_loop 72
extern void info_numeric_arg_digit_loop (WINDOW *window, int count, unsigned char key);

/* Functions declared in "./echo-area.c". */
#define A_ea_forward 73
extern void ea_forward (WINDOW *window, int count, unsigned char key);
#define A_ea_backward 74
extern void ea_backward (WINDOW *window, int count, unsigned char key);
#define A_ea_beg_of_line 75
extern void ea_beg_of_line (WINDOW *window, int count, unsigned char key);
#define A_ea_end_of_line 76
extern void ea_end_of_line (WINDOW *window, int count, unsigned char key);
#define A_ea_forward_word 77
extern void ea_forward_word (WINDOW *window, int count, unsigned char key);
#define A_ea_backward_word 78
extern void ea_backward_word (WINDOW *window, int count, unsigned char key);
#define A_ea_delete 79
extern void ea_delete (WINDOW *window, int count, unsigned char key);
#define A_ea_rubout 80
extern void ea_rubout (WINDOW *window, int count, unsigned char key);
#define A_ea_abort 81
extern void ea_abort (WINDOW *window, int count, unsigned char key);
#define A_ea_newline 82
extern void ea_newline (WINDOW *window, int count, unsigned char key);
#define A_ea_quoted_insert 83
extern void ea_quoted_insert (WINDOW *window, int count, unsigned char key);
#define A_ea_insert 84
extern void ea_insert (WINDOW *window, int count, unsigned char key);
#define A_ea_tab_insert 85
extern void ea_tab_insert (WINDOW *window, int count, unsigned char key);
#define A_ea_transpose_chars 86
extern void ea_transpose_chars (WINDOW *window, int count, unsigned char key);
#define A_ea_yank 87
extern void ea_yank (WINDOW *window, int count, unsigned char key);
#define A_ea_yank_pop 88
extern void ea_yank_pop (WINDOW *window, int count, unsigned char key);
#define A_ea_kill_line 89
extern void ea_kill_line (WINDOW *window, int count, unsigned char key);
#define A_ea_backward_kill_line 90
extern void ea_backward_kill_line (WINDOW *window, int count, unsigned char key);
#define A_ea_kill_word 91
extern void ea_kill_word (WINDOW *window, int count, unsigned char key);
#define A_ea_backward_kill_word 92
extern void ea_backward_kill_word (WINDOW *window, int count, unsigned char key);
#define A_ea_possible_completions 93
extern void ea_possible_completions (WINDOW *window, int count, unsigned char key);
#define A_ea_complete 94
extern void ea_complete (WINDOW *window, int count, unsigned char key);
#define A_ea_scroll_completions_window 95
extern void ea_scroll_completions_window (WINDOW *window, int count, unsigned char key);

/* Functions declared in "./infodoc.c". */
#define A_info_get_help_window 96
extern void info_get_help_window (WINDOW *window, int count, unsigned char key);
#define A_info_get_info_help_node 97
extern void info_get_info_help_node (WINDOW *window, int count, unsigned char key);
#define A_describe_key 98
extern void describe_key (WINDOW *window, int count, unsigned char key);
#define A_info_where_is 99
extern void info_where_is (WINDOW *window, int count, unsigned char key);

/* Functions declared in "./m-x.c". */
#define A_describe_command 100
extern void describe_command (WINDOW *window, int count, unsigned char key);
#define A_info_execute_command 101
extern void info_execute_command (WINDOW *window, int count, unsigned char key);
#define A_set_screen_height 102
extern void set_screen_height (WINDOW *window, int count, unsigned char key);

/* Functions declared in "./indices.c". */
#define A_info_index_search 103
extern void info_index_search (WINDOW *window, int count, unsigned char key);
#define A_info_next_index_match 104
extern void info_next_index_match (WINDOW *window, int count, unsigned char key);
#define A_info_index_apropos 105
extern void info_index_apropos (WINDOW *window, int count, unsigned char key);

/* Functions declared in "./nodemenu.c". */
#define A_list_visited_nodes 106
extern void list_visited_nodes (WINDOW *window, int count, unsigned char key);
#define A_select_visited_node 107
extern void select_visited_node (WINDOW *window, int count, unsigned char key);

/* Functions declared in "./footnotes.c". */
#define A_info_show_footnotes 108
extern void info_show_footnotes (WINDOW *window, int count, unsigned char key);

/* Functions declared in "./variables.c". */
#define A_describe_variable 109
extern void describe_variable (WINDOW *window, int count, unsigned char key);
#define A_set_variable 110
extern void set_variable (WINDOW *window, int count, unsigned char key);

#define A_NCOMMANDS 111
