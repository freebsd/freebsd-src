/*
 * Copyright (C) 1984-2026  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * The option table.
 */

#include "less.h"
#include "option.h"

/*
 * Variables controlled by command line options.
 */
public int quiet;               /* Should we suppress the audible bell? */
public int no_vbell;            /* Should we suppress the visual bell? */
public int how_search;          /* Where should forward searches start? */
public int top_scroll;          /* Repaint screen from top?
                                   (alternative is scroll from bottom) */
public int pr_type;             /* Type of prompt (short, medium, long) */
public int bs_mode;             /* How to process backspaces */
public int know_dumb;           /* Don't complain about dumb terminals */
public int quit_at_eof;         /* Quit after hitting end of file twice */
public int quit_if_one_screen;  /* Quit if EOF on first screen */
public int squeeze;             /* Squeeze multiple blank lines into one */
public int tabstop;             /* Tab settings */
public int back_scroll;         /* Repaint screen on backwards movement */
public int forw_scroll;         /* Repaint screen on forward movement */
public int caseless;            /* Do "caseless" searches */
public int linenums;            /* Use line numbers */
public int autobuf;             /* Automatically allocate buffers as needed */
public int bufspace;            /* Max buffer space per file (K) */
public int ctldisp;             /* Send control chars to screen untranslated */
public int force_open;          /* Open the file even if not regular file */
public int swindow;             /* Size of scrolling window */
public int jump_sline;          /* Screen line of "jump target" */
public int jump_sline_arg;
public long jump_sline_fraction = -1;
public int shift_count;         /* Number of positions to shift horizontally */
public long shift_count_fraction = -1;
public int chopline;            /* Truncate displayed lines at screen width */
public int wordwrap;            /* Wrap lines at space */
public int no_init;             /* Disable sending ti/te termcap strings */
public int no_keypad;           /* Disable sending ks/ke termcap strings */
public int twiddle;             /* Show tildes after EOF */
public int show_attn;           /* Hilite first unread line */
public int hilite_target;       /* Hilite target line */
public int status_col;          /* Display a status column */
public int use_lessopen;        /* Use the LESSOPEN filter */
public int quit_on_intr;        /* Quit on interrupt */
public int follow_mode;         /* F cmd Follows file desc or file name? */
public int oldbot;              /* Old bottom of screen behavior {{REMOVE}} */
public int opt_use_backslash;   /* Use backslash escaping in option parsing */
public LWCHAR rscroll_char;     /* Char which marks chopped lines with -S */
public int rscroll_attr;        /* Attribute of rscroll_char */
public int no_hist_dups;        /* Remove dups from history list */
public int emouse;              /* Enabled mouse features */
public int mouse_reverse;       /* Reverse mouse wheel scrolling direction */
public int xmouse;              /* Old --mouse option, replaced by --emouse */
public int wheel_lines;         /* Number of lines to scroll on mouse wheel scroll */
public int perma_marks;         /* Save marks in history file */
public int linenum_width;       /* Width of line numbers */
public int status_col_width;    /* Width of status column */
public int incr_search;         /* Incremental search */
public int use_color;           /* Use UI color */
public int want_filesize;       /* Scan to EOF if necessary to get file size */
public int status_line;         /* Highlight entire marked lines */
public int header_lines;        /* Freeze header lines at top of screen */
public int header_cols;         /* Freeze header columns at left of screen */
public int nonum_headers;       /* Don't give headers line numbers */
public int nosearch_header_lines = 0; /* Don't search in header lines */
public int nosearch_header_cols = 0; /* Don't search in header columns */
public int redraw_on_quit;      /* Redraw last screen after term deinit */
public int def_search_type;     /* */
public int exit_F_on_close;     /* Exit F command when input closes */
public int modelines;           /* Lines to read looking for modelines */
public int show_preproc_error;  /* Display msg when preproc exits with error */
public int proc_backspace;      /* Special handling of backspace */
public int proc_tab;            /* Special handling of tab */
public int proc_return;         /* Special handling of carriage return */
public int match_shift;         /* Extra horizontal shift on search match */
public int no_paste;            /* Don't accept pasted input */
public int no_edit_warn;        /* Don't warn when editing a LESSOPENed file */
public int stop_on_form_feed;   /* Stop scrolling on a line starting with form feed */
public int past_eof;            /* Continue scrolling past EOF */
public long match_shift_fraction = NUM_FRAC_DENOM/2; /* 1/2 of screen width */
public char intr_char = CONTROL('X'); /* Char to interrupt reads */
public char *first_cmd_at_prompt = NULL; /* Command to exec before first prompt */
public char *autosave;          /* Actions which do autosave of history file */
public char *end_prompt;        /* Printed after clearing the prompt */
#if HILITE_SEARCH
public int hilite_search;       /* Highlight matched search patterns? */
#endif

public lbool less_is_more = FALSE; /* Make compatible with POSIX more */

/*
 * Long option names.
 */
static struct optname a_optname      = { "search-skip-screen",   NULL };
static struct optname b_optname      = { "buffers",              NULL };
static struct optname B__optname     = { "auto-buffers",         NULL };
static struct optname c_optname      = { "clear-screen",         NULL };
static struct optname d_optname      = { "dumb",                 NULL };
static struct optname D__optname     = { "color",                NULL };
static struct optname e_optname      = { "quit-at-eof",          NULL };
static struct optname f_optname      = { "force",                NULL };
static struct optname F__optname     = { "quit-if-one-screen",   NULL };
#if HILITE_SEARCH
static struct optname g_optname      = { "hilite-search",        NULL };
#endif
static struct optname h_optname      = { "max-back-scroll",      NULL };
static struct optname i_optname      = { "ignore-case",          NULL };
static struct optname j_optname      = { "jump-target",          NULL };
static struct optname J__optname     = { "status-column",        NULL };
#if USERFILE
static struct optname k_optname      = { "lesskey-file",         NULL };
#if HAVE_LESSKEYSRC 
static struct optname ks_optname     = { "lesskey-src",          NULL };
static struct optname kc_optname     = { "lesskey-content",      NULL };
#endif /* HAVE_LESSKEYSRC */
#endif
static struct optname K__optname     = { "quit-on-intr",         NULL };
static struct optname L__optname     = { "no-lessopen",          NULL };
static struct optname m_optname      = { "long-prompt",          NULL };
static struct optname n_optname      = { "line-numbers",         NULL };
#if LOGFILE
static struct optname o_optname      = { "log-file",             NULL };
static struct optname O__optname     = { "LOG-FILE",             NULL };
#endif
static struct optname p_optname      = { "pattern",              NULL };
static struct optname P__optname     = { "prompt",               NULL };
static struct optname q2_optname     = { "silent",               NULL };
static struct optname q_optname      = { "quiet",                &q2_optname };
static struct optname r_optname      = { "raw-control-chars",    NULL };
static struct optname s_optname      = { "squeeze-blank-lines",  NULL };
static struct optname S__optname     = { "chop-long-lines",      NULL };
#if TAGS
static struct optname t_optname      = { "tag",                  NULL };
static struct optname T__optname     = { "tag-file",             NULL };
#endif
static struct optname u_optname      = { "underline-special",    NULL };
static struct optname V__optname     = { "version",              NULL };
static struct optname w_optname      = { "hilite-unread",        NULL };
static struct optname x_optname      = { "tabs",                 NULL };
static struct optname X__optname     = { "no-init",              NULL };
static struct optname y_optname      = { "max-forw-scroll",      NULL };
static struct optname z_optname      = { "window",               NULL };
static struct optname quote_optname  = { "quotes",               NULL };
static struct optname tilde_optname  = { "tilde",                NULL };
static struct optname query_optname  = { "help",                 NULL };
static struct optname pound_optname  = { "shift",                NULL };
static struct optname keypad_optname = { "no-keypad",            NULL };
static struct optname oldbot_optname = { "old-bot",              NULL };
static struct optname follow_optname = { "follow-name",          NULL };
static struct optname use_backslash_optname = { "use-backslash", NULL };
static struct optname rscroll_optname = { "rscroll", NULL };
static struct optname nohistdups_optname = { "no-histdups",      NULL };
static struct optname mouse_optname = { "mouse",                 NULL };
static struct optname emouse_optname = { "emouse",               NULL };
static struct optname mouse_reverse_optname = { "rmouse",        NULL };
static struct optname wheel_lines_optname = { "wheel-lines",     NULL };
static struct optname perma_marks_optname = { "save-marks",      NULL };
static struct optname linenum_width_optname = { "line-num-width", NULL };
static struct optname status_col_width_optname = { "status-col-width", NULL };
static struct optname incr_search_optname = { "incsearch",       NULL };
static struct optname use_color_optname = { "use-color",         NULL };
static struct optname want_filesize_optname = { "file-size",     NULL };
static struct optname status_line_optname = { "status-line",     NULL };
static struct optname header_optname = { "header",               NULL };
static struct optname no_paste_optname = { "no-paste",           NULL };
static struct optname form_feed_optname = { "form-feed",         NULL };
static struct optname past_eof_optname = { "past-eof",           NULL };
static struct optname no_edit_warn_optname2 = { "no-warn-edit",   NULL };
static struct optname no_edit_warn_optname = { "no-edit-warn",   &no_edit_warn_optname2 };
static struct optname nonum_headers_optname = { "no-number-headers", NULL };
static struct optname nosearch_headers_optname = { "no-search-headers", NULL };
static struct optname nosearch_header_lines_optname = { "no-search-header-lines", NULL };
static struct optname nosearch_header_cols_optname = { "no-search-header-columns", NULL };
static struct optname redraw_on_quit_optname = { "redraw-on-quit", NULL };
static struct optname search_type_optname = { "search-options", NULL };
static struct optname exit_F_on_close_optname = { "exit-follow-on-close", NULL };
static struct optname modelines_optname = { "modelines", NULL };
static struct optname no_vbell_optname = { "no-vbell", NULL };
static struct optname intr_optname = { "intr", NULL };
static struct optname wordwrap_optname = { "wordwrap", NULL };
static struct optname show_preproc_error_optname = { "show-preproc-errors", NULL };
static struct optname proc_backspace_optname = { "proc-backspace", NULL };
static struct optname proc_tab_optname = { "proc-tab", NULL };
static struct optname proc_return_optname = { "proc-return", NULL };
static struct optname match_shift_optname = { "match-shift", NULL };
static struct optname first_cmd_at_prompt_optname = { "cmd", NULL };
static struct optname autosave_optname = { "autosave", NULL };
static struct optname hilite_target_optname = { "hilite-target", NULL };
static struct optname end_prompt_optname = { "end-prompt", NULL };
#if LESSTEST
static struct optname ttyin_name_optname = { "tty",              NULL };
#endif /*LESSTEST*/


/*
 * Table of all options and their semantics.
 *
 * For BOOL and TRIPLE options, odesc[0], odesc[1], odesc[2] are
 * the description of the option when set to 0, 1 or 2, respectively.
 * For NUMBER options, odesc[0] is the prompt to use when entering
 * a new value, and odesc[1] is the description, which should contain 
 * one %d which is replaced by the value of the number.
 * For STRING options, odesc[0] is the prompt to use when entering
 * a new value, and odesc[1], if not NULL, is a description of the
 * characters that are valid in the string (see optstring()).
 */
static struct loption option[] =
{
	{ 'a', &a_optname,
		O_TRIPLE, OPT_ONPLUS, &how_search, NULL,
		{
			LM_Search_includes_displayed_screen,
			LM_Search_skips_displayed_screen,
			LM_Search_includes_all_of_displayed_screen
		},
		{ NULL, NULL, NULL }
	},

	{ 'b', &b_optname,
		O_NUMBER|O_INIT_HANDLER, 64, &bufspace, opt_b, 
		{
			LM_Max_buffer_space_per_file,
			LM_Max_buffer_space_per_file_X,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'B', &B__optname,
		O_BOOL, OPT_ON, &autobuf, NULL,
		{
			LM_Dont_automatically_allocate_buffers,
			LM_Automatically_allocate_buffers_when_needed,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'c', &c_optname,
		O_TRIPLE, OPT_OFF, &top_scroll, NULL,
		{
			LM_Repaint_by_scrolling_from_bottom_of_screen,
			LM_Repaint_by_painting_from_top_of_screen,
			LM_Repaint_by_painting_from_top_of_screen,
		},
		{ NULL, NULL, NULL }
	},
	{ 'd', &d_optname,
		O_BOOL|O_NO_TOGGLE, OPT_OFF, &know_dumb, NULL,
		{
			LM_Assume_intelligent_terminal,
			LM_Assume_dumb_terminal,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'D', &D__optname,
		O_STRING|O_REPAINT|O_NO_QUERY, 0, NULL, opt_D,
		{ LM_color_desc, LM_s, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ 'e', &e_optname,
		O_TRIPLE, OPT_OFF, &quit_at_eof, NULL,
		{
			LM_Dont_quit_at_end_of_file,
			LM_Quit_at_end_of_file,
			LM_Quit_immediately_at_end_of_file
		},
		{ NULL, NULL, NULL }
	},
	{ 'f', &f_optname,
		O_BOOL, OPT_OFF, &force_open, NULL,
		{
			LM_Open_only_regular_files,
			LM_Open_even_non_regular_files,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'F', &F__optname,
		O_BOOL, OPT_OFF, &quit_if_one_screen, NULL,
		{
			LM_Dont_quit_if_end_of_file_on_first_screen,
			LM_Quit_if_end_of_file_on_first_screen,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
#if HILITE_SEARCH
	{ 'g', &g_optname,
		O_TRIPLE|O_HL_REPAINT, OPT_ONPLUS, &hilite_search, NULL,
		{
			LM_Dont_highlight_search_matches,
			LM_Highlight_matches_for_previous_search_only,
			LM_Highlight_all_matches_for_previous_search_pattern,
		},
		{ NULL, NULL, NULL }
	},
#endif
	{ 'h', &h_optname,
		O_NUMBER, -1, &back_scroll, NULL,
		{
			LM_Backwards_scroll_limit,
			LM_Backwards_scroll_limit_is_X_lines,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'i', &i_optname,
		O_TRIPLE|O_HL_REPAINT, OPT_OFF, &caseless, opt_i,
		{
			LM_Case_is_significant_in_searches,
			LM_Ignore_case_in_searches,
			LM_Ignore_case_in_searches_and_in_patterns,
		},
		{ NULL, NULL, NULL }
	},
	{ 'j', &j_optname,
		O_STRING, 0, NULL, opt_j,
		{
			LM_Target_line,
			LM_neg_dot_d,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'J', &J__optname,
		O_BOOL|O_REPAINT, OPT_OFF, &status_col, NULL,
		{
			LM_Dont_display_a_status_column,
			LM_Display_a_status_column,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
#if USERFILE
	{ 'k', &k_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_k,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
#if HAVE_LESSKEYSRC 
	{ OLETTER_NONE, &kc_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_kc,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &ks_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_ks,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
#endif /* HAVE_LESSKEYSRC */
#endif
	{ 'K', &K__optname,
		O_BOOL, OPT_OFF, &quit_on_intr, NULL,
		{
			LM_Dont_quit_if_end_of_file_on_first_screen,
			LM_Interrupt_exits_less,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'L', &L__optname,
		O_BOOL, OPT_ON, &use_lessopen, NULL,
		{
			LM_Dont_use_the_LESSOPEN_filter,
			LM_Use_the_LESSOPEN_filter,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'm', &m_optname,
		O_TRIPLE, OPT_OFF, &pr_type, NULL,
		{
			LM_Short_prompt,
			LM_Medium_prompt,
			LM_Long_prompt,
		},
		{ NULL, NULL, NULL }
	},
	{ 'n', &n_optname,
		O_TRIPLE|O_REPAINT, OPT_ON, &linenums, NULL,
		{
			LM_Dont_use_line_numbers,
			LM_Use_line_numbers,
			LM_Constantly_display_line_numbers,
		},
		{ NULL, NULL, NULL }
	},
#if LOGFILE
	{ 'o', &o_optname,
		O_STRING, 0, NULL, opt_o,
		{ LM_log_file, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ 'O', &O__optname,
		O_STRING, 0, NULL, opt__O,
		{ LM_Log_file, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
#endif
	{ 'p', &p_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_p,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ 'P', &P__optname,
		O_STRING, 0, NULL, opt__P,
		{ LM_prompt, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ 'q', &q_optname,
		O_TRIPLE, OPT_OFF, &quiet, NULL,
		{
			LM_Ring_the_bell_for_errors_AND_at_eof,
			LM_Ring_the_bell_for_errors_but_not_at_eof,
			LM_Never_ring_the_bell,
		},
		{ NULL, NULL, NULL }
	},
	{ 'r', &r_optname,
		O_TRIPLE|O_REPAINT, OPT_OFF, &ctldisp, NULL,
		{
			LM_Display_control_characters_as_caret,
			LM_Display_control_characters_directly,
			LM_Display_ANSI_sequences_directly,
		},
		{ NULL, NULL, NULL }
	},
	{ 's', &s_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &squeeze, NULL,
		{
			LM_Display_all_blank_lines,
			LM_Squeeze_multiple_blank_lines,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'S', &S__optname,
		O_BOOL|O_REPAINT, OPT_OFF, &chopline, opt__S,
		{
			LM_Fold_long_lines,
			LM_Chop_long_lines,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
#if TAGS
	{ 't', &t_optname,
		O_STRING|O_NO_QUERY, 0, NULL, opt_t,
		{ LM_tag, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ 'T', &T__optname,
		O_STRING, 0, NULL, opt__T,
		{ LM_tags_file, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
#endif
	{ 'u', &u_optname,
		O_TRIPLE|O_REPAINT|O_HL_REPAINT, OPT_OFF, &bs_mode, NULL,
		{
			LM_Display_underlined_text_in_underline_mode,
			LM_Backspaces_cause_overstrike,
			LM_Print_backspace_as_caret,
		},
		{ NULL, NULL, NULL }
	},
	{ 'V', &V__optname,
		O_NOVAR, 0, NULL, opt__V,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &hilite_target_optname,
		O_BOOL, OPT_OFF, &hilite_target, opt_hilite_target,
		{
			LM_Dont_highlight_target_line,
			LM_Highlight_target_line,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'w', &w_optname,
		O_TRIPLE|O_REPAINT, OPT_OFF, &show_attn, NULL,
		{
			LM_Dont_highlight_first_unread_line,
			LM_Highlight_first_unread_line_after_forward_screen,
			LM_Highlight_first_unread_line_after_any_forward_movement,
		},
		{ NULL, NULL, NULL }
	},
	{ 'x', &x_optname,
		O_STRING|O_REPAINT, 0, NULL, opt_x,
		{ LM_Tab_stops_, LM_d_comma, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ 'X', &X__optname,
		O_BOOL|O_NO_TOGGLE, OPT_OFF, &no_init, NULL,
		{
			LM_Send_init_deinit_strings_to_terminal,
			LM_Dont_use_init_deinit_strings,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'y', &y_optname,
		O_NUMBER, -1, &forw_scroll, NULL,
		{
			LM_Forward_scroll_limit,
			LM_Forward_scroll_limit_is_X_lines,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ 'z', &z_optname,
		O_NUMBER|O_NEGOK, -1, &swindow, NULL,
		{
			LM_Scroll_window_size,
			LM_Scroll_window_size_is_X_lines,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ '"', &quote_optname,
		O_STRING, 0, NULL, opt_quote,
		{ LM_quotes, LM_s, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ '~', &tilde_optname,
		O_BOOL|O_REPAINT, OPT_ON, &twiddle, NULL,
		{
			LM_Dont_show_tildes_after_end_of_file,
			LM_Show_tildes_after_end_of_file,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ '?', &query_optname,
		O_NOVAR, 0, NULL, opt_query,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ '#', &pound_optname,
		O_STRING, 0, NULL, opt_shift,
		{ LM_Horizontal_shift, LM_dot_d, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &keypad_optname,
		O_BOOL|O_NO_TOGGLE, OPT_OFF, &no_keypad, NULL,
		{
			LM_Use_keypad_mode,
			LM_Dont_use_keypad_mode,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &oldbot_optname,
		O_BOOL, OPT_OFF, &oldbot, NULL,
		{
			LM_Use_new_bottom_of_screen_behavior,
			LM_Use_old_bottom_of_screen_behavior,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &follow_optname,
		O_BOOL, FOLLOW_DESC, &follow_mode, NULL,
		{
			LM_F_command_follows_file_descriptor,
			LM_F_command_follows_file_name,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &use_backslash_optname,
		O_BOOL, OPT_OFF, &opt_use_backslash, NULL,
		{
			LM_Dont_use_backslash_escaping_in_command_line_parameters,
			LM_Use_backslash_escaping_in_command_line_parameters,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &rscroll_optname,
		O_STRING|O_REPAINT|O_INIT_HANDLER, 0, NULL, opt_rscroll,
		{ LM_rscroll_character, LM_s, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &nohistdups_optname,
		O_BOOL, OPT_OFF, &no_hist_dups, NULL,
		{
			LM_Allow_duplicates_in_history_list,
			LM_Remove_duplicates_from_history_list,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &mouse_optname,
		O_TRIPLE, OPT_OFF, &xmouse, opt_mouse,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &emouse_optname,
		O_STRING, 0, NULL, opt_emouse,
		{ LM_Mouse_features, LM_s, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &mouse_reverse_optname,
		O_BOOL, OPT_OFF, &mouse_reverse, NULL,
		{
			LM_Normal_mouse_scroll_direction,
			LM_Reverse_mouse_scroll_direction,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &wheel_lines_optname,
		O_NUMBER|O_INIT_HANDLER, 0, &wheel_lines, opt_wheel_lines,
		{
			LM_Lines_to_scroll_on_mouse_wheel,
			LM_Scroll_X_lines_on_mouse_wheel,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &perma_marks_optname,
		O_BOOL, OPT_OFF, &perma_marks, NULL,
		{
			LM_Dont_save_marks_in_history_file,
			LM_Save_marks_in_history_file,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &linenum_width_optname,
		O_NUMBER|O_REPAINT, MIN_LINENUM_WIDTH, &linenum_width, opt_linenum_width,
		{
			LM_Line_number_width,
			LM_Line_number_width_is_X_chars,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &status_col_width_optname,
		O_NUMBER|O_REPAINT, 2, &status_col_width, opt_status_col_width,
		{
			LM_Status_column_width,
			LM_Status_column_width_is_X_chars,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &incr_search_optname,
		O_BOOL, OPT_OFF, &incr_search, NULL,
		{
			LM_Incremental_search_is_off,
			LM_Incremental_search_is_on,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &use_color_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &use_color, NULL,
		{
			LM_Dont_use_color,
			LM_Use_color,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &want_filesize_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &want_filesize, opt_filesize,
		{
			LM_Dont_get_size_of_each_file,
			LM_Get_size_of_each_file,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &status_line_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &status_line, NULL,
		{
			LM_Line_highlight_applies_to_text_only,
			LM_Line_highlight_applies_to_entire_width_of_screen,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &header_optname,
		O_STRING|O_REPAINT, 0, NULL, opt_header,
		{ LM_Header_lines, LM_d_comma, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &no_paste_optname,
		O_BOOL, OPT_OFF, &no_paste, opt_no_paste,
		{ 
			LM_Accept_pasted_input,
			LM_Ignore_pasted_input,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &form_feed_optname,
		O_BOOL, OPT_OFF, &stop_on_form_feed, NULL,
		{
			LM_Dont_stop_on_form_feed,
			LM_Stop_on_form_feed,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &past_eof_optname,
		O_BOOL, OPT_OFF, &past_eof, NULL,
		{
			LM_Stop_scrolling_at_end_of_file,
			LM_Dont_stop_scrolling_at_end_of_file,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &no_edit_warn_optname,
		O_BOOL, OPT_OFF, &no_edit_warn, NULL,
		{
			LM_Warn_when_editing_a_file_opened_via_LESSOPEN,
			LM_Dont_warn_when_editing_a_file_opened_via_LESSOPEN,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &nonum_headers_optname,
		O_BOOL|O_REPAINT, 0, &nonum_headers, NULL,
		{
			LM_Number_header_lines,
			LM_Dont_number_header_lines,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &nosearch_headers_optname,
		O_BOOL|O_HL_REPAINT, 0, NULL, opt_nosearch_headers,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &nosearch_header_lines_optname,
		O_BOOL|O_HL_REPAINT, 0, NULL, opt_nosearch_header_lines,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &nosearch_header_cols_optname,
		O_BOOL|O_HL_REPAINT, 0, NULL, opt_nosearch_header_cols,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &redraw_on_quit_optname,
		O_BOOL, OPT_OFF, &redraw_on_quit, NULL,
		{
			LM_Dont_redraw_screen_when_quitting,
			LM_Redraw_last_screen_when_quitting,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &search_type_optname,
		O_STRING, 0, NULL, opt_search_type,
		{ LM_Search_options, LM_s, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &exit_F_on_close_optname,
		O_BOOL, OPT_OFF, &exit_F_on_close, NULL,
		{
			LM_Dont_exit_F_command_when_input_closes,
			LM_Exit_F_command_when_input_closes,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &no_vbell_optname,
		O_BOOL, OPT_OFF, &no_vbell, NULL,
		{
			LM_Display_visual_bell,
			LM_Dont_display_visual_bell,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &modelines_optname,
		O_NUMBER, 0, &modelines, NULL,
		{
			LM_Lines_to_read_looking_for_modelines,
			LM_Read_X_lines_looking_for_modelines,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &intr_optname,
		O_STRING, 0, NULL, opt_intr,
		{ LM_interrupt_character, LM_s, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &wordwrap_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &wordwrap, NULL,
		{
			LM_Wrap_lines_at_any_character,
			LM_Wrap_lines_at_spaces,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &show_preproc_error_optname,
		O_BOOL, OPT_OFF, &show_preproc_error, NULL,
		{
			LM_Dont_show_error_message_if_preprocessor_fails,
			LM_Show_error_message_if_preprocessor_fails,
			LM_NULL
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &proc_backspace_optname,
		O_TRIPLE|O_REPAINT|O_HL_REPAINT, OPT_OFF, &proc_backspace, NULL,
		{
			LM_Backspace_handling_is_specified_by_the_U_option,
			LM_Display_underline_text_in_underline_mode,
			LM_Print_backspaces_as_caret,
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &proc_tab_optname,
		O_TRIPLE|O_REPAINT|O_HL_REPAINT, OPT_OFF, &proc_tab, NULL,
		{
			LM_Tab_handling_is_specified_by_the_U_option,
			LM_Expand_tabs_to_spaces,
			LM_Print_tabs_as_caret,
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &proc_return_optname,
		O_TRIPLE|O_REPAINT|O_HL_REPAINT, OPT_OFF, &proc_return, NULL,
		{
			LM_Carriage_return_handling_is_specified_by_the_U_option,
			LM_Delete_carriage_return_before_newline,
			LM_Print_carriage_return_as_caret,
		},
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &first_cmd_at_prompt_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_first_cmd_at_prompt,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &match_shift_optname,
		O_STRING|O_INIT_HANDLER, 0, NULL, opt_match_shift,
		{ LM_Search_match_shift, LM_dot_d, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &autosave_optname,
		O_STRING|O_INIT_HANDLER, 0, NULL, opt_autosave,
		{ LM_Autosave_actions, LM_s, LM_NULL },
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &end_prompt_optname,
		O_STRING, 0, NULL, opt_end_prompt,
		{ LM_Print_after_prompt, LM_s, LM_NULL },
		{ NULL, NULL, NULL }
	},
#if LESSTEST
	{ OLETTER_NONE, &ttyin_name_optname,
		O_STRING|O_NO_TOGGLE, 0, NULL, opt_ttyin_name,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	},
#endif /*LESSTEST*/
	{ '\0', NULL, O_NOVAR, 0, NULL, NULL,
		{ LM_NULL, LM_NULL, LM_NULL },
		{ NULL, NULL, NULL }
	}
};


/*
 * Initialize each option to its default value.
 */
public void init_option(void)
{
	struct loption *o;
	constant char *p;

	p = lgetenv("LESS_IS_MORE");
	if (!isnullenv(p) && !(p[0] == '0' && p[1] == '\0'))
		less_is_more = TRUE;

	for (o = option;  o->oletter != '\0';  o++)
	{
		int i;
		for (i = 0;  i < 3;  i++)
			o->odesc[i] = (o->odescid[i] == LM_NULL) ? NULL : lmsg(o->odescid[i]);
		if (o->ovar != NULL)
			*(o->ovar) = o->odefault;
		if (o->otype & O_INIT_HANDLER)
			(*(o->ofunc))(INIT, (char *) NULL);
	}
}

/*
 * Find an option in the option table, given its option letter.
 */
public struct loption * findopt(int c)
{
	struct loption *o;

	for (o = option;  o->oletter != '\0';  o++)
	{
		if (o->oletter == c)
			return (o);
		if ((o->otype & O_TRIPLE) && ASCII_TO_UPPER(o->oletter) == c)
			return (o);
	}
	return (NULL);
}

/*
 *
 */
static lbool is_optchar(char c)
{
	if (ASCII_IS_UPPER(c))
		return TRUE;
	if (ASCII_IS_LOWER(c))
		return TRUE;
	if (c == '-')
		return TRUE;
	return FALSE;
}

/*
 * Find an option in the option table, given its option name.
 * p_optname is the (possibly partial) name to look for, and
 * is updated to point after the matched name.
 * p_oname if non-NULL is set to point to the full option name.
 */
public struct loption * findopt_name(constant char **p_optname, constant char **p_oname, lbool *p_ambig)
{
	constant char *optname = *p_optname;
	struct loption *o;
	struct optname *oname;
	size_t len;
	int uppercase;
	struct loption *maxo = NULL;
	struct optname *maxoname = NULL;
	size_t maxlen = 0;
	lbool ambig = FALSE;
	lbool exact = FALSE;

	/*
	 * Check all options.
	 */
	for (o = option;  o->oletter != '\0';  o++)
	{
		/*
		 * Check all names for this option.
		 */
		for (oname = o->onames;  oname != NULL;  oname = oname->onext)
		{
			/* 
			 * Try normal match first (uppercase == 0),
			 * then, then if it's a TRIPLE option,
			 * try uppercase match (uppercase == 1).
			 */
			for (uppercase = 0;  uppercase <= 1;  uppercase++)
			{
				len = sprefix(optname, oname->oname, uppercase);
				if (len == 0 || is_optchar(optname[len]))
				{
					/*
					 * We didn't use all of the option name.
					 */
					continue;
				}
				if (!exact && len == maxlen)
					/*
					 * Already had a partial match,
					 * and now there's another one that
					 * matches the same length.
					 */
					ambig = TRUE;
				else if (len > maxlen)
				{
					/*
					 * Found a better match than
					 * the one we had.
					 */
					maxo = o;
					maxoname = oname;
					maxlen = len;
					ambig = FALSE;
					exact = (len == strlen(oname->oname));
				}
				if (!(o->otype & O_TRIPLE))
					break;
			}
		}
	}
	if (p_ambig != NULL)
		*p_ambig = ambig;
	if (ambig)
	{
		/*
		 * Name matched more than one option.
		 */
		return (NULL);
	}
	*p_optname = optname + maxlen;
	if (p_oname != NULL)
		*p_oname = maxoname == NULL ? NULL : maxoname->oname;
	return (maxo);
}

/*
 * Find all toggleable options whose names begin with a specified string.
 * Return them in a space-separated string.
 */
public char * findopts_name(constant char *pfx)
{
	constant struct loption *o;
	constant struct optname *oname;
	struct xbuffer xbuf;
	int uppercase;

	xbuf_init(&xbuf);
	for (o = option;  o->oletter != '\0';  o++)
	{
		if (o->otype & O_NO_TOGGLE)
			continue;
		for (oname = o->onames;  oname != NULL;  oname = oname->onext)
		{
			for (uppercase = 0;  uppercase <= 1;  uppercase++)
			{
				size_t len = sprefix(pfx, oname->oname, uppercase);
				if (len >= strlen(pfx))
				{
					constant char *np;
					for (np = oname->oname;  *np != '\0';  np++)
						xbuf_add_char(&xbuf, uppercase && ASCII_IS_LOWER(*np) ? ASCII_TO_UPPER(*np) : *np);
					xbuf_add_char(&xbuf, ' ');
				}
				if (!(o->otype & O_TRIPLE))
					break;
			}
		}
	}
	xbuf_pop(&xbuf); /* remove final space */
	xbuf_add_char(&xbuf, '\0');
	return (char *) xbuf.data;
}
