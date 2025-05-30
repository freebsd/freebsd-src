/*
 * Copyright (C) 1984-2025  Mark Nudelman
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
public long jump_sline_fraction = -1;
public int shift_count;         /* Number of positions to shift horizontally */
public long shift_count_fraction = -1;
public int chopline;            /* Truncate displayed lines at screen width */
public int wordwrap;            /* Wrap lines at space */
public int no_init;             /* Disable sending ti/te termcap strings */
public int no_keypad;           /* Disable sending ks/ke termcap strings */
public int twiddle;             /* Show tildes after EOF */
public int show_attn;           /* Hilite first unread line */
public int status_col;          /* Display a status column */
public int use_lessopen;        /* Use the LESSOPEN filter */
public int quit_on_intr;        /* Quit on interrupt */
public int follow_mode;         /* F cmd Follows file desc or file name? */
public int oldbot;              /* Old bottom of screen behavior {{REMOVE}} */
public int opt_use_backslash;   /* Use backslash escaping in option parsing */
public LWCHAR rscroll_char;     /* Char which marks chopped lines with -S */
public int rscroll_attr;        /* Attribute of rscroll_char */
public int no_hist_dups;        /* Remove dups from history list */
public int mousecap;            /* Allow mouse for scrolling */
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
public long match_shift_fraction = NUM_FRAC_DENOM/2; /* 1/2 of screen width */
public char intr_char = CONTROL('X'); /* Char to interrupt reads */
#if HILITE_SEARCH
public int hilite_search;       /* Highlight matched search patterns? */
#endif

public int less_is_more = 0;    /* Make compatible with POSIX more */

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
static struct optname mousecap_optname = { "mouse",              NULL };
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
 * a new value, and odesc[1], if not NULL, is the set of characters
 * that are valid in the string.
 */
static struct loption option[] =
{
	{ 'a', &a_optname,
		O_TRIPLE, OPT_ONPLUS, &how_search, NULL,
		{
			"Search includes displayed screen",
			"Search skips displayed screen",
			"Search includes all of displayed screen"
		}
	},

	{ 'b', &b_optname,
		O_NUMBER|O_INIT_HANDLER, 64, &bufspace, opt_b, 
		{
			"Max buffer space per file (K): ",
			"Max buffer space per file: %dK",
			NULL
		}
	},
	{ 'B', &B__optname,
		O_BOOL, OPT_ON, &autobuf, NULL,
		{
			"Don't automatically allocate buffers",
			"Automatically allocate buffers when needed",
			NULL
		}
	},
	{ 'c', &c_optname,
		O_TRIPLE, OPT_OFF, &top_scroll, NULL,
		{
			"Repaint by scrolling from bottom of screen",
			"Repaint by painting from top of screen",
			"Repaint by painting from top of screen"
		}
	},
	{ 'd', &d_optname,
		O_BOOL|O_NO_TOGGLE, OPT_OFF, &know_dumb, NULL,
		{
			"Assume intelligent terminal",
			"Assume dumb terminal",
			NULL
		}
	},
	{ 'D', &D__optname,
		O_STRING|O_REPAINT|O_NO_QUERY, 0, NULL, opt_D,
		{ "color desc: ", "s", NULL }
	},
	{ 'e', &e_optname,
		O_TRIPLE, OPT_OFF, &quit_at_eof, NULL,
		{
			"Don't quit at end-of-file",
			"Quit at end-of-file",
			"Quit immediately at end-of-file"
		}
	},
	{ 'f', &f_optname,
		O_BOOL, OPT_OFF, &force_open, NULL,
		{
			"Open only regular files",
			"Open even non-regular files",
			NULL
		}
	},
	{ 'F', &F__optname,
		O_BOOL, OPT_OFF, &quit_if_one_screen, NULL,
		{
			"Don't quit if end-of-file on first screen",
			"Quit if end-of-file on first screen",
			NULL
		}
	},
#if HILITE_SEARCH
	{ 'g', &g_optname,
		O_TRIPLE|O_HL_REPAINT, OPT_ONPLUS, &hilite_search, NULL,
		{
			"Don't highlight search matches",
			"Highlight matches for previous search only",
			"Highlight all matches for previous search pattern",
		}
	},
#endif
	{ 'h', &h_optname,
		O_NUMBER, -1, &back_scroll, NULL,
		{
			"Backwards scroll limit: ",
			"Backwards scroll limit is %d lines",
			NULL
		}
	},
	{ 'i', &i_optname,
		O_TRIPLE|O_HL_REPAINT, OPT_OFF, &caseless, opt_i,
		{
			"Case is significant in searches",
			"Ignore case in searches",
			"Ignore case in searches and in patterns"
		}
	},
	{ 'j', &j_optname,
		O_STRING, 0, NULL, opt_j,
		{
			"Target line: ",
			"-.d",
			NULL
		}
	},
	{ 'J', &J__optname,
		O_BOOL|O_REPAINT, OPT_OFF, &status_col, NULL,
		{
			"Don't display a status column",
			"Display a status column",
			NULL
		}
	},
#if USERFILE
	{ 'k', &k_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_k,
		{ NULL, NULL, NULL }
	},
#if HAVE_LESSKEYSRC 
	{ OLETTER_NONE, &kc_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_kc,
		{ NULL, NULL, NULL }
	},
	{ OLETTER_NONE, &ks_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_ks,
		{ NULL, NULL, NULL }
	},
#endif /* HAVE_LESSKEYSRC */
#endif
	{ 'K', &K__optname,
		O_BOOL, OPT_OFF, &quit_on_intr, NULL,
		{
			"Interrupt (ctrl-C) returns to prompt",
			"Interrupt (ctrl-C) exits less",
			NULL
		}
	},
	{ 'L', &L__optname,
		O_BOOL, OPT_ON, &use_lessopen, NULL,
		{
			"Don't use the LESSOPEN filter",
			"Use the LESSOPEN filter",
			NULL
		}
	},
	{ 'm', &m_optname,
		O_TRIPLE, OPT_OFF, &pr_type, NULL,
		{
			"Short prompt",
			"Medium prompt",
			"Long prompt"
		}
	},
	{ 'n', &n_optname,
		O_TRIPLE|O_REPAINT, OPT_ON, &linenums, NULL,
		{
			"Don't use line numbers",
			"Use line numbers",
			"Constantly display line numbers"
		}
	},
#if LOGFILE
	{ 'o', &o_optname,
		O_STRING, 0, NULL, opt_o,
		{ "log file: ", NULL, NULL }
	},
	{ 'O', &O__optname,
		O_STRING, 0, NULL, opt__O,
		{ "Log file: ", NULL, NULL }
	},
#endif
	{ 'p', &p_optname,
		O_STRING|O_NO_TOGGLE|O_NO_QUERY, 0, NULL, opt_p,
		{ NULL, NULL, NULL }
	},
	{ 'P', &P__optname,
		O_STRING, 0, NULL, opt__P,
		{ "prompt: ", NULL, NULL }
	},
	{ 'q', &q_optname,
		O_TRIPLE, OPT_OFF, &quiet, NULL,
		{
			"Ring the bell for errors AND at eof/bof",
			"Ring the bell for errors but not at eof/bof",
			"Never ring the bell"
		}
	},
	{ 'r', &r_optname,
		O_TRIPLE|O_REPAINT, OPT_OFF, &ctldisp, NULL,
		{
			"Display control characters as ^X",
			"Display control characters directly (not recommended)",
			"Display ANSI sequences directly, other control characters as ^X"
		}
	},
	{ 's', &s_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &squeeze, NULL,
		{
			"Display all blank lines",
			"Squeeze multiple blank lines",
			NULL
		}
	},
	{ 'S', &S__optname,
		O_BOOL|O_REPAINT, OPT_OFF, &chopline, opt__S,
		{
			"Fold long lines",
			"Chop long lines",
			NULL
		}
	},
#if TAGS
	{ 't', &t_optname,
		O_STRING|O_NO_QUERY, 0, NULL, opt_t,
		{ "tag: ", NULL, NULL }
	},
	{ 'T', &T__optname,
		O_STRING, 0, NULL, opt__T,
		{ "tags file: ", NULL, NULL }
	},
#endif
	{ 'u', &u_optname,
		O_TRIPLE|O_REPAINT|O_HL_REPAINT, OPT_OFF, &bs_mode, NULL,
		{
			"Display underlined text in underline mode",
			"Backspaces cause overstrike",
			"Print backspace as ^H"
		}
	},
	{ 'V', &V__optname,
		O_NOVAR, 0, NULL, opt__V,
		{ NULL, NULL, NULL }
	},
	{ 'w', &w_optname,
		O_TRIPLE|O_REPAINT, OPT_OFF, &show_attn, NULL,
		{
			"Don't highlight first unread line",
			"Highlight first unread line after forward-screen",
			"Highlight first unread line after any forward movement",
		}
	},
	{ 'x', &x_optname,
		O_STRING|O_REPAINT, 0, NULL, opt_x,
		{
			"Tab stops: ",
			"d,",
			NULL
		}
	},
	{ 'X', &X__optname,
		O_BOOL|O_NO_TOGGLE, OPT_OFF, &no_init, NULL,
		{
			"Send init/deinit strings to terminal",
			"Don't use init/deinit strings",
			NULL
		}
	},
	{ 'y', &y_optname,
		O_NUMBER, -1, &forw_scroll, NULL,
		{
			"Forward scroll limit: ",
			"Forward scroll limit is %d lines",
			NULL
		}
	},
	{ 'z', &z_optname,
		O_NUMBER, -1, &swindow, NULL,
		{
			"Scroll window size: ",
			"Scroll window size is %d lines",
			NULL
		}
	},
	{ '"', &quote_optname,
		O_STRING, 0, NULL, opt_quote,
		{ "quotes: ", "s", NULL }
	},
	{ '~', &tilde_optname,
		O_BOOL|O_REPAINT, OPT_ON, &twiddle, NULL,
		{
			"Don't show tildes after end of file",
			"Show tildes after end of file",
			NULL
		}
	},
	{ '?', &query_optname,
		O_NOVAR, 0, NULL, opt_query,
		{ NULL, NULL, NULL }
	},
	{ '#', &pound_optname,
		O_STRING, 0, NULL, opt_shift,
		{
			"Horizontal shift: ",
			".d",
			NULL
		}
	},
	{ OLETTER_NONE, &keypad_optname,
		O_BOOL|O_NO_TOGGLE, OPT_OFF, &no_keypad, NULL,
		{
			"Use keypad mode",
			"Don't use keypad mode",
			NULL
		}
	},
	{ OLETTER_NONE, &oldbot_optname,
		O_BOOL, OPT_OFF, &oldbot, NULL,
		{
			"Use new bottom of screen behavior",
			"Use old bottom of screen behavior",
			NULL
		}
	},
	{ OLETTER_NONE, &follow_optname,
		O_BOOL, FOLLOW_DESC, &follow_mode, NULL,
		{
			"F command follows file descriptor",
			"F command follows file name",
			NULL
		}
	},
	{ OLETTER_NONE, &use_backslash_optname,
		O_BOOL, OPT_OFF, &opt_use_backslash, NULL,
		{
			"Use backslash escaping in command line parameters",
			"Don't use backslash escaping in command line parameters",
			NULL
		}
	},
	{ OLETTER_NONE, &rscroll_optname,
		O_STRING|O_REPAINT|O_INIT_HANDLER, 0, NULL, opt_rscroll,
		{ "rscroll character: ", "s", NULL }
	},
	{ OLETTER_NONE, &nohistdups_optname,
		O_BOOL, OPT_OFF, &no_hist_dups, NULL,
		{
			"Allow duplicates in history list",
			"Remove duplicates from history list",
			NULL
		}
	},
	{ OLETTER_NONE, &mousecap_optname,
		O_TRIPLE, OPT_OFF, &mousecap, opt_mousecap,
		{
			"Ignore mouse input",
			"Use the mouse for scrolling",
			"Use the mouse for scrolling (reverse)"
		}
	},
	{ OLETTER_NONE, &wheel_lines_optname,
		O_NUMBER|O_INIT_HANDLER, 0, &wheel_lines, opt_wheel_lines,
		{
			"Lines to scroll on mouse wheel: ",
			"Scroll %d line(s) on mouse wheel",
			NULL
		}
	},
	{ OLETTER_NONE, &perma_marks_optname,
		O_BOOL, OPT_OFF, &perma_marks, NULL,
		{
			"Don't save marks in history file",
			"Save marks in history file",
			NULL
		}
	},
	{ OLETTER_NONE, &linenum_width_optname,
		O_NUMBER|O_REPAINT, MIN_LINENUM_WIDTH, &linenum_width, opt_linenum_width,
		{
			"Line number width: ",
			"Line number width is %d chars",
			NULL
		}
	},
	{ OLETTER_NONE, &status_col_width_optname,
		O_NUMBER|O_REPAINT, 2, &status_col_width, opt_status_col_width,
		{
			"Status column width: ",
			"Status column width is %d chars",
			NULL
		}
	},
	{ OLETTER_NONE, &incr_search_optname,
		O_BOOL, OPT_OFF, &incr_search, NULL,
		{
			"Incremental search is off",
			"Incremental search is on",
			NULL
		}
	},
	{ OLETTER_NONE, &use_color_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &use_color, NULL,
		{
			"Don't use color",
			"Use color",
			NULL
		}
	},
	{ OLETTER_NONE, &want_filesize_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &want_filesize, opt_filesize,
		{
			"Don't get size of each file",
			"Get size of each file",
			NULL
		}
	},
	{ OLETTER_NONE, &status_line_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &status_line, NULL,
		{
			"Don't color each line with its status column color",
			"Color each line with its status column color",
			NULL
		}
	},
	{ OLETTER_NONE, &header_optname,
		O_STRING|O_REPAINT, 0, NULL, opt_header,
		{ "Header lines: ", "d,", NULL }
	},
	{ OLETTER_NONE, &no_paste_optname,
		O_BOOL, OPT_OFF, &no_paste, opt_no_paste,
		{ 
			"Accept pasted input",
			"Ignore pasted input",
			NULL
		}
	},
	{ OLETTER_NONE, &form_feed_optname,
		O_BOOL, OPT_OFF, &stop_on_form_feed, NULL,
		{
			"Don't stop on form feed",
			"Stop on form feed",
			NULL
		}
	},
	{ OLETTER_NONE, &no_edit_warn_optname,
		O_BOOL, OPT_OFF, &no_edit_warn, NULL,
		{
			"Warn when editing a file opened via LESSOPEN",
			"Don't warn when editing a file opened via LESSOPEN",
			NULL
		}
	},
	{ OLETTER_NONE, &nonum_headers_optname,
		O_BOOL|O_REPAINT, 0, &nonum_headers, NULL,
		{
			"Number header lines",
			"Don't number header lines",
			NULL
		}
	},
	{ OLETTER_NONE, &nosearch_headers_optname,
		O_BOOL|O_HL_REPAINT, 0, NULL, opt_nosearch_headers,
		{
			NULL, NULL, NULL
		}
	},
	{ OLETTER_NONE, &nosearch_header_lines_optname,
		O_BOOL|O_HL_REPAINT, 0, NULL, opt_nosearch_header_lines,
		{
			NULL, NULL, NULL
		}
	},
	{ OLETTER_NONE, &nosearch_header_cols_optname,
		O_BOOL|O_HL_REPAINT, 0, NULL, opt_nosearch_header_cols,
		{
			NULL, NULL, NULL
		}
	},
	{ OLETTER_NONE, &redraw_on_quit_optname,
		O_BOOL, OPT_OFF, &redraw_on_quit, NULL,
		{
			"Don't redraw screen when quitting",
			"Redraw last screen when quitting",
			NULL
		}
	},
	{ OLETTER_NONE, &search_type_optname,
		O_STRING, 0, NULL, opt_search_type,
		{ "Search options: ", "s", NULL }
	},
	{ OLETTER_NONE, &exit_F_on_close_optname,
		O_BOOL, OPT_OFF, &exit_F_on_close, NULL,
		{
			"Don't exit F command when input closes",
			"Exit F command when input closes",
			NULL
		}
	},
	{ OLETTER_NONE, &no_vbell_optname,
		O_BOOL, OPT_OFF, &no_vbell, NULL,
		{
			"Display visual bell",
			"Don't display visual bell",
			NULL
		}
	},
	{ OLETTER_NONE, &modelines_optname,
		O_NUMBER, 0, &modelines, NULL,
		{
			"Lines to read looking for modelines: ",
			"Read %d lines looking for modelines",
			NULL
		}
	},
	{ OLETTER_NONE, &intr_optname,
		O_STRING, 0, NULL, opt_intr,
		{ "interrupt character: ", "s", NULL }
	},
	{ OLETTER_NONE, &wordwrap_optname,
		O_BOOL|O_REPAINT, OPT_OFF, &wordwrap, NULL,
		{
			"Wrap lines at any character",
			"Wrap lines at spaces",
			NULL
		}
	},
	{ OLETTER_NONE, &show_preproc_error_optname,
		O_BOOL, OPT_OFF, &show_preproc_error, NULL,
		{
			"Don't show error message if preprocessor fails",
			"Show error message if preprocessor fails",
			NULL
		}
	},
	{ OLETTER_NONE, &proc_backspace_optname,
		O_TRIPLE|O_REPAINT|O_HL_REPAINT, OPT_OFF, &proc_backspace, NULL,
		{
			"Backspace handling is specified by the -U option",
			"Display underline text in underline mode",
			"Print backspaces as ^H"
		}
	},
	{ OLETTER_NONE, &proc_tab_optname,
		O_TRIPLE|O_REPAINT|O_HL_REPAINT, OPT_OFF, &proc_tab, NULL,
		{
			"Tab handling is specified by the -U option",
			"Expand tabs to spaces",
			"Print tabs as ^I"
		}
	},
	{ OLETTER_NONE, &proc_return_optname,
		O_TRIPLE|O_REPAINT|O_HL_REPAINT, OPT_OFF, &proc_return, NULL,
		{
			"Carriage return handling is specified by the -U option",
			"Delete carriage return before newline",
			"Print carriage return as ^M"
		}
	},
	{ OLETTER_NONE, &match_shift_optname,
		O_STRING|O_INIT_HANDLER, 0, NULL, opt_match_shift,
		{
			"Search match shift: ",
			".d",
			NULL
		}
	},
#if LESSTEST
	{ OLETTER_NONE, &ttyin_name_optname,
		O_STRING|O_NO_TOGGLE, 0, NULL, opt_ttyin_name,
		{
			NULL,
			NULL,
			NULL
		}
	},
#endif /*LESSTEST*/
	{ '\0', NULL, O_NOVAR, 0, NULL, NULL, { NULL, NULL, NULL } }
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
		less_is_more = 1;

	for (o = option;  o->oletter != '\0';  o++)
	{
		/*
		 * Set each variable to its default.
		 */
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
