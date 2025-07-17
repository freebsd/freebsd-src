/*
 * $Id: dialog.c,v 1.281 2021/01/17 16:52:18 tom Exp $
 *
 *  cdialog - Display simple dialog boxes from shell scripts
 *
 *  Copyright 2000-2020,2021	Thomas E. Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 2.1
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to
 *	Free Software Foundation, Inc.
 *	51 Franklin St., Fifth Floor
 *	Boston, MA 02110, USA.
 *
 *  An earlier version of this program lists as authors
 *	Savio Lam (lam836@cs.cuhk.hk)
 */

#include <dialog.h>

#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SETLOCALE
#include <locale.h>
#endif

#include <dlg_internals.h>

#define PASSARGS             t,       av,        offset_add
#define CALLARGS const char *t, char *av[], int *offset_add
typedef int (callerFn) (CALLARGS);

typedef enum {
    o_unknown = 0
    ,o_allow_close
    ,o_and_widget
    ,o_ascii_lines
    ,o_aspect_ratio
    ,o_auto_placement
    ,o_backtitle
    ,o_beep_signal
    ,o_beep_after_signal
    ,o_begin_set
    ,o_cancel_label
    ,o_checklist
    ,o_dlg_clear_screen
    ,o_colors
    ,o_column_separator
    ,o_cr_wrap
    ,o_create_rc
    ,o_cursor_off_label
    ,o_date_format
    ,o_default_button
    ,o_default_item
    ,o_defaultno
    ,o_erase_on_exit
    ,o_exit_label
    ,o_extra_button
    ,o_extra_label
    ,o_fixed_font
    ,o_form
    ,o_gauge
    ,o_help
    ,o_help_button
    ,o_help_file
    ,o_help_label
    ,o_help_line
    ,o_help_status
    ,o_help_tags
    ,o_icon
    ,o_ignore
    ,o_infobox
    ,o_input_fd
    ,o_inputbox
    ,o_inputmenu
    ,o_insecure
    ,o_item_help
    ,o_keep_colors
    ,o_keep_tite
    ,o_keep_window
    ,o_last_key
    ,o_max_input
    ,o_menu
    ,o_mixedform
    ,o_mixedgauge
    ,o_msgbox
    ,o_no_close
    ,o_nocollapse
    ,o_no_cr_wrap
    ,o_cant_kill
    ,o_no_hot_list
    ,o_no_label
    ,o_no_lines
    ,o_no_mouse
    ,o_no_nl_expand
    ,o_use_shadow
    ,o_nocancel
    ,o_nook
    ,o_ok_label
    ,o_output_fd
    ,o_output_separator
    ,o_passwordbox
    ,o_passwordform
    ,o_pause
    ,o_prgbox
    ,o_print_maxsize
    ,o_print_siz
    ,o_text_only
    ,o_print_text_size
    ,o_print_version
    ,o_programbox
    ,o_progressbox
    ,o_quoted
    ,o_radiolist
    ,o_screen_center
    ,o_use_scrollbar
    ,o_separate_output
    ,o_separate_str
    ,o_single_quoted
    ,o_size_err
    ,o_sleep_secs
    ,o_smooth
    ,o_output_stderr
    ,o_output_stdout
    ,o_tab_correct
    ,o_tab_len
    ,o_tailbox
    ,o_tailboxbg
    ,o_textbox
    ,o_time_format
    ,o_timeout_secs
    ,o_title
    ,o_trim_whitespace
    ,o_under_mouse
    ,o_version
    ,o_visit_items
    ,o_wmclass
    ,o_yes_label
    ,o_yesno
#ifdef HAVE_WHIPTAIL
    ,o_fullbutton
    ,o_topleft
#endif
#ifdef HAVE_XDIALOG
    ,o_calendar
    ,o_dselect
    ,o_editbox
    ,o_fselect
    ,o_timebox
    ,o_week_start
#endif
#ifdef HAVE_XDIALOG2
    ,o_buildlist
    ,o_rangebox
    ,o_reorder
    ,o_treeview
#endif
#if defined(HAVE_XDIALOG2) || defined(HAVE_WHIPTAIL)
    ,o_no_items
    ,o_no_tags
#endif
#ifdef HAVE_DLG_TRACE
    ,o_trace
#endif
    ,o_iso_week
} eOptions;

typedef enum {
    tUnknown
    ,tFalse
    ,tTrue
    ,tNumber
    ,tString
} tOptions;

/*
 * The bits in 'pass' are used to decide which options are applicable at
 * different stages in the program:
 *	1 flags before widgets
 *	2 widgets
 *	4 non-widget options
 */
typedef struct {
    const char *name;
    eOptions code;
    int vars;			/* 0=none, 1=state, 2=vars */
    tOptions type;		/* type for bool(true/false), number, string */
    unsigned offset;
    int pass;			/* 1,2,4 or combination */
    const char *help;		/* NULL to suppress, non-empty to display params */
} Options;

typedef struct {
    eOptions code;
    int argmin, argmax;
    callerFn *jumper;
} Mode;

/* use these macros for simple options in DIALOG_STATE */
#define ssF(name) o_##name, 1, tFalse,  offsetof(DIALOG_STATE,name)
#define ssT(name) o_##name, 1, tTrue,   offsetof(DIALOG_STATE,name)
#define ssN(name) o_##name, 1, tNumber, offsetof(DIALOG_STATE,name)
#define ssS(name) o_##name, 1, tString, offsetof(DIALOG_STATE,name)

/* use these macros for simple options in DIALOG_VARS */
#define svF(name) o_##name, 2, tFalse,  offsetof(DIALOG_VARS,name)
#define svT(name) o_##name, 2, tTrue,   offsetof(DIALOG_VARS,name)
#define svN(name) o_##name, 2, tNumber, offsetof(DIALOG_VARS,name)
#define svS(name) o_##name, 2, tString, offsetof(DIALOG_VARS,name)

/* use these macros for ignored options */
#define xxF(name) o_##name, 0, tFalse,  0
#define xxT(name) o_##name, 0, tTrue,   0
#define xxN(name) o_##name, 0, tNumber, 0
#define xxS(name) o_##name, 0, tString, 0

/* use this macro for widget options */
#define opW(name) o_##name, 0, 0, 0

/* use this macro for other options */
#define opO(name) o_##name, 0, 0, 0

static int known_opts = 0;
static const char **dialog_opts;
static char **dialog_argv;

static char **special_argv = 0;
static int special_argc = 0;

static bool ignore_unknown = FALSE;

static const char *program = "dialog";

#ifdef NO_LEAKS
typedef struct _all_blobs {
    struct _all_blobs *next;
    void *blob;
} AllBlobs;

static AllBlobs *all_blobs;
#endif

/*
 * The options[] table is organized this way to make it simple to maintain
 * a sorted list of options for the help-message.
 *
 * Because Boolean options correspond to "true", --shadow is listed here while
 * --no-shadow is not.  The findOption and optionBool functions handle the
 * cases where "no" is added or removed from the option name to derive an
 * opposite setting.
 */
/* *INDENT-OFF* */
static const Options options[] = {
    { "allow-close",	xxT(allow_close),	1, NULL },
    { "and-widget",	opO(and_widget),	4, NULL },
    { "ascii-lines",	svT(ascii_lines), 	1, "" },
    { "aspect",		ssN(aspect_ratio),	1, "<ratio>" },
    { "auto-placement", xxT(auto_placement),	1, NULL },
    { "backtitle",	svS(backtitle),		1, "<backtitle>" },
    { "beep",		svT(beep_signal),	1, "" },
    { "beep-after",	svT(beep_after_signal),	1, "" },
    { "begin",		svT(begin_set),		1, "<y> <x>" },
    { "cancel-label",	svS(cancel_label),	1, "<str>" },
    { "checklist",	opW(checklist),		2, "<text> <height> <width> <list height> <tag1> <item1> <status1>..." },
    { "clear",		svT(dlg_clear_screen),	1, "" },
    { "colors",		svT(colors),		1, "" },
    { "column-separator",svS(column_separator),	1, "<str>" },
    { "cr-wrap",	svT(cr_wrap),		1, "" },
    { "create-rc",	opO(create_rc),		1, NULL },
    { "cursor-off-label",svT(cursor_off_label),	1, "" },
    { "date-format",	svS(date_format),	1, "<str>" },
    { "default-button",	xxS(default_button),	1, "<str>" },
    { "default-item",	svS(default_item),	1, "<str>" },
    { "defaultno",	svT(defaultno),		1, "" },
    { "erase-on-exit",	svT(erase_on_exit),	1, "" },
    { "exit-label",	svS(exit_label),	1, "<str>" },
    { "extra-button",	svT(extra_button),	1, "" },
    { "extra-label",	svS(extra_label),	1, "<str>" },
    { "fixed-font",	xxT(fixed_font),	1, NULL },
    { "form",		opW(form),		2, "<text> <height> <width> <form height> <label1> <l_y1> <l_x1> <item1> <i_y1> <i_x1> <flen1> <ilen1>..." },
    { "gauge",		opW(gauge),		2, "<text> <height> <width> [<percent>]" },
    { "guage",		opW(gauge),		2, NULL },
    { "help",		opO(help),		4, "" },
    { "help-button",	svT(help_button),	1, "" },
    { "help-label",	svS(help_label),	1, "<str>" },
    { "help-status",	svT(help_status),	1, "" },
    { "help-tags",	svT(help_tags),		1, "" },
    { "hfile",		svS(help_file),		1, "<str>" },
    { "hline",		svS(help_line),		1, "<str>" },
    { "icon",		xxS(icon),		1, NULL },
    { "ignore",		opO(ignore),		1, "" },
    { "infobox",	opW(infobox),		2, "<text> <height> <width>" },
    { "input-fd",	opO(input_fd),		1, "<fd>" },
    { "inputbox",	opW(inputbox),		2, "<text> <height> <width> [<init>]" },
    { "inputmenu",	opW(inputmenu),		2, "<text> <height> <width> <menu height> <tag1> <item1>..." },
    { "insecure",	svT(insecure),		1, "" },
    { "item-help",	svT(item_help),		1, "" },
    { "keep-colors",	xxT(keep_colors),	1, NULL },
    { "keep-tite",	svT(keep_tite),		1, "" },
    { "keep-window",	svT(keep_window),	1, "" },
    { "last-key",	svT(last_key),		1, "" },
    { "max-input",	svN(max_input),		1, "<n>" },
    { "menu",		opW(menu),		2, "<text> <height> <width> <menu height> <tag1> <item1>..." },
    { "mixedform",	opW(mixedform),		2, "<text> <height> <width> <form height> <label1> <l_y1> <l_x1> <item1> <i_y1> <i_x1> <flen1> <ilen1> <itype>..." },
    { "mixedgauge",	opW(mixedgauge),	2, "<text> <height> <width> <percent> <tag1> <item1>..." },
    { "msgbox",		opW(msgbox),		2, "<text> <height> <width>" },
    { "no-cancel",	svT(nocancel),		1, "" },
    { "no-close",	xxT(no_close),		1, NULL },
    { "no-collapse",	svT(nocollapse),	1, "" },
    { "no-hot-list",	svT(no_hot_list),	1, "" },
    { "no-kill",	svT(cant_kill),		1, "" },
    { "no-label",	svS(no_label),		1, "<str>" },
    { "no-lines",	svT(no_lines), 		1, "" },
    { "no-mouse",	ssT(no_mouse),		1, "" },
    { "no-nl-expand",	svT(no_nl_expand),	1, "" },
    { "no-ok",		svT(nook),		1, "" },
    { "no-shadow",	ssF(use_shadow),	1, "" },
    { "ok-label",	svS(ok_label),		1, "<str>" },
    { "output-fd",	opO(output_fd),		1, "<fd>" },
    { "output-separator",svS(output_separator),	1, "<str>" },
    { "passwordbox",	opW(passwordbox),	2, "<text> <height> <width> [<init>]" },
    { "passwordform",	opW(passwordform),	2, "<text> <height> <width> <form height> <label1> <l_y1> <l_x1> <item1> <i_y1> <i_x1> <flen1> <ilen1>..." },
    { "pause",		opW(pause),		2, "<text> <height> <width> <seconds>" },
    { "prgbox",		opW(prgbox),		2, "<text> <command> <height> <width>" },
    { "print-maxsize",	opO(print_maxsize),	1, "" },
    { "print-size",	svT(print_siz),		1, "" },
    { "print-text-only",ssT(text_only),		5, "<text> <height> <width>" },
    { "print-text-size",opO(print_text_size),	5, "<text> <height> <width>" },
    { "print-version",	opO(print_version),	5, "" },
    { "programbox",	opW(programbox),	2, "<text> <height> <width>" },
    { "progressbox",	opW(progressbox),	2, "<text> <height> <width>" },
    { "quoted",		svT(quoted),		1, "" },
    { "radiolist",	opW(radiolist),		2, "<text> <height> <width> <list height> <tag1> <item1> <status1>..." },
    { "screen-center",	xxT(screen_center),	1, NULL },
    { "scrollbar",	ssT(use_scrollbar),	1, "" },
    { "separate-output",svT(separate_output),	1, "" },
    { "separate-widget",ssS(separate_str),	1, "<str>" },
    { "separator",	svS(output_separator),	1, NULL },
    { "single-quoted",	svT(single_quoted),	1, "" },
    { "size-err",	svT(size_err),		1, "" },
    { "sleep",		svN(sleep_secs),	1, "<secs>" },
    { "smooth",		xxT(smooth),		1, NULL },
    { "stderr",		opO(output_stderr),	1, "" },
    { "stdout",		opO(output_stdout),	1, "" },
    { "tab-correct",	svT(tab_correct),	1, "" },
    { "tab-len",	ssN(tab_len),		1, "<n>" },
    { "tailbox",	opW(tailbox),		2, "<file> <height> <width>" },
    { "tailboxbg",	opW(tailboxbg),		2, "<file> <height> <width>" },
    { "textbox",	opW(textbox),		2, "<file> <height> <width>" },
    { "time-format",	svS(time_format),	1, "<str>" },
    { "timeout",	svN(timeout_secs),	1, "<secs>" },
    { "title",		svS(title),		1, "<title>" },
    { "trim",		svT(trim_whitespace),	1, "" },
    { "under-mouse", 	xxT(under_mouse),	1, NULL },
    { "version",	opO(version),		5, "" },
    { "visit-items", 	ssT(visit_items),	1, "" },
    { "wmclass",	xxS(wmclass),		1, NULL },
    { "yes-label",	svS(yes_label),		1, "<str>" },
    { "yesno",		opW(yesno),		2, "<text> <height> <width>" },
#ifdef HAVE_WHIPTAIL
    { "cancel-button",	svS(cancel_label),	1, NULL },
    { "fb",		xxT(fullbutton),	1, NULL },
    { "fullbutton",	xxT(fullbutton),	1, NULL },
    { "no-button",	svS(no_label),		1, NULL },
    { "ok-button",	svS(ok_label),		1, NULL },
    { "scrolltext",	ssT(use_scrollbar),	1, NULL },
    { "topleft",	svT(begin_set),		1, NULL },
    { "yes-button",	svS(yes_label),		1, NULL },
#endif
#ifdef HAVE_XDIALOG
    { "calendar",	opW(calendar),		2, "<text> <height> <width> <day> <month> <year>" },
    { "dselect",	opW(dselect),		2, "<directory> <height> <width>" },
    { "editbox",	opW(editbox),		2, "<file> <height> <width>" },
    { "fselect",	opW(fselect),		2, "<filepath> <height> <width>" },
    { "timebox",	opW(timebox),		2, "<text> <height> <width> <hour> <minute> <second>" },
    { "week-start",	svS(week_start),	1, "<str>" },
    { "iso-week",	svT(iso_week),		1, NULL },
#endif
#ifdef HAVE_XDIALOG2
    { "buildlist",	opW(buildlist),		2, "<text> <height> <width> <list-height> <tag1> <item1> <status1>..." },
    { "no-items", 	svT(no_items),		1, "" },
    { "no-tags", 	svT(no_tags),		1, "" },
    { "rangebox",	opW(rangebox),		2, "<text> <height> <width> <min-value> <max-value> <default-value>" },
    { "reorder", 	svT(reorder),		1, "" },
    { "treeview",	opW(treeview),		2, "<text> <height> <width> <list-height> <tag1> <item1> <status1> <depth1>..." },
#endif
#if defined(HAVE_XDIALOG2) || defined(HAVE_WHIPTAIL)
    { "noitem", 	svT(no_items),		1, NULL },
    { "notags", 	svT(no_tags),		1, NULL },
#endif
#ifdef HAVE_DLG_TRACE
    { "trace",		opO(trace),		1, "<file>" },
#endif
};
/* *INDENT-ON* */

#ifdef NO_LEAKS
static void
ignore_leak(void *value)
{
    AllBlobs *next = dlg_calloc(AllBlobs, (size_t) 1);
    if (next != 0) {
	next->blob = value;
	next->next = all_blobs;
	all_blobs = next;
    }
}

static void
handle_leaks(void)
{
    while (all_blobs != 0) {
	char *blob = all_blobs->blob;
	AllBlobs *next = all_blobs->next;
	free(blob);
	free(all_blobs);
	all_blobs = next;
    }
    free(dialog_opts);
    if (special_argv != 0) {
	free(special_argv[0]);
	free(special_argv);
	special_argv = 0;
	special_argc = 0;
    }
}
#else
#define handle_leaks()		/* nothing */
#define ignore_leak(n)		/* nothing */
#endif

#define OptionChars "\
0123456789\
-\
abcdefghijklmnopqrstuvwxyz\
"

/*
 * Check if the given string from main's argv is an option.
 */
static bool
isOption(const char *arg)
{
    bool result = FALSE;

    if (arg != 0) {
	if (dialog_opts != 0) {
	    int n;
	    for (n = 0; dialog_opts[n] != 0; ++n) {
		if (dialog_opts[n] == arg) {
		    result = TRUE;
		    break;
		}
	    }
	} else if (!strncmp(arg, "--", (size_t) 2) && isalpha(UCH(arg[2]))) {
	    if (strlen(arg) == (strspn) (arg, OptionChars)) {
		result = TRUE;
	    } else {
		handle_leaks();
		dlg_exiterr("Invalid option \"%s\"", arg);
	    }
	}
    }
    return result;
}

/*
 * Make an array showing which argv[] entries are options.  Use "--" as a
 * special token to escape the next argument, allowing it to begin with "--".
 * When we find a "--" argument, also remove it from argv[] and adjust argc.
 * That appears to be an undocumented feature of the popt library.
 *
 * Also, if we see a "--file", expand it into the parameter list by reading the
 * text from the given file and stripping quotes, treating whitespace outside
 * quotes as a parameter delimiter.
 *
 * Finally, if we see a "--args", dump the current list of arguments to the
 * standard error.  This is used for debugging complex --file combinations.
 */
static void
unescape_argv(int *argcp, char ***argvp)
{
    int j, k;
    int limit_includes = 20 + *argcp;
    int count_includes = 0;
    bool doalloc = FALSE;
    char *filename;
    const char **my_argv = 0;
    int my_argc;

    DLG_TRACE(("# unescape_argv\n"));
    for (k = 0; k < 2; ++k) {

	my_argc = 0;
	if (special_argv != 0) {
	    for (j = 0; special_argv[j] != 0; ++j) {
		if (!strcmp(special_argv[j], "--")) {
		    break;
		} else if (isOption(special_argv[j])) {
		    if (k != 0)
			my_argv[my_argc] = special_argv[j];
		    my_argc++;
		}
	    }
	}

	if (k == 0) {
	    my_argc += (*argcp + 1);
	    my_argv = dlg_calloc(const char *, (size_t) my_argc);
	    assert_ptr(my_argv, "unescape_argv");
	}
    }

    for (j = 1; j < *argcp; j++) {
	bool escaped = FALSE;
	if (!strcmp((*argvp)[j], "--")) {
	    escaped = TRUE;
	    dlg_eat_argv(argcp, argvp, j, 1);
	} else if (!strcmp((*argvp)[j], "--args")) {
	    fprintf(stderr, "Showing arguments at arg%d\n", j);
	    for (k = 0; k < *argcp; ++k) {
		fprintf(stderr, " arg%d:%s\n", k, (*argvp)[k]);
	    }
	    dlg_eat_argv(argcp, argvp, j, 1);
	    --j;
	} else if (!strcmp((*argvp)[j], "--file")) {
	    if (++count_includes > limit_includes) {
		handle_leaks();
		dlg_exiterr("Too many --file options");
	    }

	    if ((filename = (*argvp)[j + 1]) != 0) {
		FILE *fp;
		char **list;

		if (*filename == '&') {
		    fp = fdopen(atoi(filename + sizeof(char)), "r");
		} else {
		    fp = fopen(filename, "r");
		}

		if (fp) {
		    char *blob;
		    int added;
		    size_t bytes_read;
		    size_t length;
		    int n;

		    DLG_TRACE(("# opened --file %s ..\n", filename));
		    blob = NULL;
		    length = 0;
		    do {
			blob = dlg_realloc(char, length + BUFSIZ + 1, blob);
			assert_ptr(blob, "unescape_argv");
			bytes_read = fread(blob + length,
					   sizeof(char),
					     (size_t) BUFSIZ,
					   fp);
			length += bytes_read;
			if (ferror(fp)) {
			    handle_leaks();
			    dlg_exiterr("error on filehandle in unescape_argv");
			}
		    } while (bytes_read == BUFSIZ);
		    fclose(fp);

		    blob[length] = '\0';
		    ignore_leak(blob);

		    list = dlg_string_to_argv(blob);
		    added = dlg_count_argv(list);
		    if (added > 2) {
			/* *argcp arguments before the expansion of --file
			   - 2 for the removal of '--file <filepath>'
			   + added for the arguments contained in <filepath>
			   + 1 for the terminating NULL pointer */
			size_t need = (size_t) (*argcp + added - 1);
			if (doalloc) {
			    *argvp = dlg_realloc(char *, need, *argvp);
			    assert_ptr(*argvp, "unescape_argv");
			} else {
			    char **newp = dlg_malloc(char *, need);
			    ignore_leak(newp);
			    assert_ptr(newp, "unescape_argv");
			    for (n = 0; n < *argcp; ++n) {
				newp[n] = (*argvp)[n];
			    }
			    /* The new array is not NULL-terminated yet. */
			    *argvp = newp;
			    doalloc = TRUE;
			}
			my_argv = dlg_realloc(const char *, need, my_argv);
			assert_ptr(my_argv, "unescape_argv");

			/* Shift the arguments after '--file <filepath>'
			   right by (added - 2) positions */
			for (n = *argcp - 1; n >= j + 2; --n) {
			    (*argvp)[n + added - 2] = (*argvp)[n];
			}
		    } else if (added < 2) {
			/* 0 or 1 argument read from the included file
			   -> shift the arguments after '--file <filepath>'
			   left by (2 - added) positions */
			for (n = j + added; n + 2 - added < *argcp; ++n) {
			    (*argvp)[n] = (*argvp)[n + 2 - added];
			}
		    }
		    /* Copy the inserted arguments to *argvp */
		    for (n = 0; n < added; ++n) {
			(*argvp)[n + j] = list[n];
		    }
		    *argcp += added - 2;
		    (*argvp)[*argcp] = 0;	/* Write the NULL terminator */
		    free(list);	/* No-op if 'list' is NULL */
		    /* Force rescan starting from the first inserted argument */
		    --j;
		    DLG_TRACE(("# finished --file\n"));
		    continue;
		} else {
		    handle_leaks();
		    dlg_exiterr("Cannot open --file %s", filename);
		}
	    } else {
		handle_leaks();
		dlg_exiterr("No value given for --file");
	    }
	}
	if (!escaped
	    && (*argvp)[j] != 0
	    && !strncmp((*argvp)[j], "--", (size_t) 2)
	    && isalpha(UCH((*argvp)[j][2]))) {
	    my_argv[my_argc++] = (*argvp)[j];
	    DLG_TRACE(("#\toption argv[%d]=%s\n", j, (*argvp)[j]));
	}
    }

    my_argv[my_argc] = 0;

    known_opts = my_argc;
    dialog_opts = my_argv;

    DLG_TRACE(("#\t%d options vs %d arguments\n", known_opts, *argcp));
    dialog_argv = (*argvp);
}

static const Options *
findOption(const char *name, int pass, bool recur)
{
    const Options *result = NULL;

    if (!strncmp(name, "--", 2) && isalpha(UCH(name[2]))) {
	unsigned n;

	name += 2;		/* skip the "--" */
	for (n = 0; n < TableSize(options); n++) {
	    if ((pass & options[n].pass) != 0
		&& !strcmp(name, options[n].name)) {
		result = &options[n];
		break;
	    }
	}
	if (result == NULL && !recur) {
	    char *temp = malloc(8 + strlen(name));
	    if (temp != NULL) {
		if (!strncmp(name, "no", 2)) {
		    int skip = !strncmp(name, "no-", 3) ? 3 : 2;
		    sprintf(temp, "--no-%s", name + skip);
		    result = findOption(temp, pass, TRUE);
		    if (result == NULL) {
			sprintf(temp, "--%s", name + skip);
			result = findOption(temp, pass, TRUE);
		    }
		}
		if (result == NULL && strncmp(name, "no", 2)) {
		    sprintf(temp, "--no-%s", name);
		    result = findOption(temp, pass, TRUE);
		}
		free(temp);
	    }
	}
    }
    return result;
}

static eOptions
lookupOption(const char *name, int pass)
{
    eOptions result = o_unknown;
    const Options *data = findOption(name, pass, FALSE);
    if (data != NULL) {
	result = data->code;
    }
    return result;
}

static void
Usage(const char *msg)
{
    handle_leaks();
    dlg_exiterr("%s.\nUse --help to list options.\n\n", msg);
}

/*
 * Count arguments, stopping at the end of the argument list, or on any of our
 * "--" tokens.
 */
static int
arg_rest(char *argv[])
{
    int i = 1;			/* argv[0] points to a "--" token */

    while (argv[i] != 0
	   && (!isOption(argv[i]) || lookupOption(argv[i], 7) == o_unknown))
	i++;
    return i;
}

/*
 * In MultiWidget this function is needed to count how many tags
 * a widget (menu, checklist, radiolist) has
 */
static int
howmany_tags(char *argv[], int group)
{
    int result = 0;
    char temp[80];

    while (argv[0] != 0) {
	int have;

	if (isOption(argv[0]))
	    break;
	if ((have = arg_rest(argv)) < group) {
	    const char *format = _("Expected %d arguments, found only %d");
	    sprintf(temp, format, group, have);
	    Usage(temp);
	} else if ((have % group) != 0) {
	    const char *format = _("Expected %d arguments, found extra %d");
	    sprintf(temp, format, group, (have % group));
	    Usage(temp);
	}

	argv += have;
	result += (have / group);
    }

    return result;
}

static int
numeric_arg(char **av, int n)
{
    int result = 0;

    if (n < dlg_count_argv(av)) {
	char *last = 0;
	result = (int) strtol(av[n], &last, 10);

	if (last == 0 || *last != 0) {
	    char msg[80];

	    sprintf(msg, "Expected a number for token %d of %.20s", n, av[0]);
	    Usage(msg);
	}
    }
    return result;
}

static char *
optional_str(char **av, int n, char *dft)
{
    char *ret = dft;
    if (arg_rest(av) > n)
	ret = av[n];
    return ret;
}

#if defined(HAVE_DLG_GAUGE) || defined(HAVE_XDIALOG)
static int
optional_num(char **av, int n, int dft)
{
    int ret = dft;
    if (arg_rest(av) > n)
	ret = numeric_arg(av, n);
    return ret;
}
#endif

/*
 * On AIX 4.x, we have to flush the output right away since there is a bug in
 * the curses package which discards stdout even when we've used newterm to
 * redirect output to /dev/tty.
 */
static int
show_result(int ret)
{
    bool either = FALSE;

    switch (ret) {
    case DLG_EXIT_OK:
    case DLG_EXIT_EXTRA:
    case DLG_EXIT_HELP:
    case DLG_EXIT_ITEM_HELP:
	if ((dialog_state.output_count > 1) && !dialog_vars.separate_output) {
	    fputs((dialog_state.separate_str
		   ? dialog_state.separate_str
		   : DEFAULT_SEPARATE_STR),
		  dialog_state.output);
	    either = TRUE;
	}
	if (dialog_vars.input_result != 0
	    && dialog_vars.input_result[0] != '\0') {
	    fputs(dialog_vars.input_result, dialog_state.output);
	    DLG_TRACE(("# input_result:\n%s\n", dialog_vars.input_result));
	    either = TRUE;
	}
	if (either) {
	    fflush(dialog_state.output);
	}
	break;
    }
    return ret;
}

/*
 * These are the widget callers.
 */

static int
call_yesno(CALLARGS)
{
    *offset_add = 4;
    return dialog_yesno(t,
			av[1],
			numeric_arg(av, 2),
			numeric_arg(av, 3));
}

static int
call_msgbox(CALLARGS)
{
    *offset_add = 4;
    return dialog_msgbox(t,
			 av[1],
			 numeric_arg(av, 2),
			 numeric_arg(av, 3), 1);
}

static int
call_infobox(CALLARGS)
{
    *offset_add = 4;
    return dialog_msgbox(t,
			 av[1],
			 numeric_arg(av, 2),
			 numeric_arg(av, 3), 0);
}

static int
call_textbox(CALLARGS)
{
    *offset_add = 4;
    return dialog_textbox(t,
			  av[1],
			  numeric_arg(av, 2),
			  numeric_arg(av, 3));
}

static int
call_menu(CALLARGS)
{
    int tags = howmany_tags(av + 5, MENUBOX_TAGS);
    *offset_add = 5 + tags * MENUBOX_TAGS;

    return dialog_menu(t,
		       av[1],
		       numeric_arg(av, 2),
		       numeric_arg(av, 3),
		       numeric_arg(av, 4),
		       tags, av + 5);
}

static int
call_inputmenu(CALLARGS)
{
    int tags = howmany_tags(av + 5, MENUBOX_TAGS);
    bool free_extra_label = FALSE;
    int result;

    dialog_vars.input_menu = TRUE;

    if (dialog_vars.max_input == 0)
	dialog_vars.max_input = MAX_LEN / 2;

    if (dialog_vars.extra_label == 0) {
	free_extra_label = TRUE;
	dialog_vars.extra_label = dlg_strclone(_("Rename"));
    }

    dialog_vars.extra_button = TRUE;

    *offset_add = 5 + tags * MENUBOX_TAGS;
    result = dialog_menu(t,
			 av[1],
			 numeric_arg(av, 2),
			 numeric_arg(av, 3),
			 numeric_arg(av, 4),
			 tags, av + 5);
    if (free_extra_label) {
	free(dialog_vars.extra_label);
	dialog_vars.extra_label = 0;
    }
    return result;
}

static int
call_checklist(CALLARGS)
{
    int tags = howmany_tags(av + 5, CHECKBOX_TAGS);
    int code;

    *offset_add = 5 + tags * CHECKBOX_TAGS;
    code = dialog_checklist(t,
			    av[1],
			    numeric_arg(av, 2),
			    numeric_arg(av, 3),
			    numeric_arg(av, 4),
			    tags, av + 5, FLAG_CHECK);
    return code;
}

static int
call_radiolist(CALLARGS)
{
    int tags = howmany_tags(av + 5, CHECKBOX_TAGS);
    *offset_add = 5 + tags * CHECKBOX_TAGS;
    return dialog_checklist(t,
			    av[1],
			    numeric_arg(av, 2),
			    numeric_arg(av, 3),
			    numeric_arg(av, 4),
			    tags, av + 5, FLAG_RADIO);
}

static int
call_inputbox(CALLARGS)
{
    *offset_add = arg_rest(av);
    return dialog_inputbox(t,
			   av[1],
			   numeric_arg(av, 2),
			   numeric_arg(av, 3),
			   optional_str(av, 4, 0), 0);
}

static int
call_passwordbox(CALLARGS)
{
    *offset_add = arg_rest(av);
    return dialog_inputbox(t,
			   av[1],
			   numeric_arg(av, 2),
			   numeric_arg(av, 3),
			   optional_str(av, 4, 0), 1);
}

#ifdef HAVE_XDIALOG
static int
call_calendar(CALLARGS)
{
    *offset_add = arg_rest(av);
    return dialog_calendar(t,
			   av[1],
			   numeric_arg(av, 2),
			   numeric_arg(av, 3),
			   optional_num(av, 4, -1),
			   optional_num(av, 5, -1),
			   optional_num(av, 6, -1));
}

static int
call_dselect(CALLARGS)
{
    *offset_add = arg_rest(av);
    return dialog_dselect(t,
			  av[1],
			  numeric_arg(av, 2),
			  numeric_arg(av, 3));
}

static int
call_editbox(CALLARGS)
{
    *offset_add = 4;
    return dialog_editbox(t,
			  av[1],
			  numeric_arg(av, 2),
			  numeric_arg(av, 3));
}

static int
call_fselect(CALLARGS)
{
    *offset_add = arg_rest(av);
    return dialog_fselect(t,
			  av[1],
			  numeric_arg(av, 2),
			  numeric_arg(av, 3));
}

static int
call_timebox(CALLARGS)
{
    *offset_add = arg_rest(av);
    return dialog_timebox(t,
			  av[1],
			  numeric_arg(av, 2),
			  numeric_arg(av, 3),
			  optional_num(av, 4, -1),
			  optional_num(av, 5, -1),
			  optional_num(av, 6, -1));
}
#endif /* HAVE_XDIALOG */

/* dialog 1.2 widgets */
#ifdef HAVE_XDIALOG2

#define DisableNoTags() \
	bool save_no_tags = dialog_vars.no_tags; \
	bool save_no_items = dialog_vars.no_items; \
	dialog_vars.no_tags = TRUE; \
	dialog_vars.no_items = FALSE

#define RestoreNoTags() \
	dialog_vars.no_tags = save_no_tags; \
	dialog_vars.no_items = save_no_items

static int
call_buildlist(CALLARGS)
{
    int tags = howmany_tags(av + 5, CHECKBOX_TAGS);
    int result;

    DisableNoTags();

    *offset_add = 5 + tags * CHECKBOX_TAGS;
    result = dialog_buildlist(t,
			      av[1],
			      numeric_arg(av, 2),
			      numeric_arg(av, 3),
			      numeric_arg(av, 4),
			      tags, av + 5,
			      dialog_vars.reorder);
    RestoreNoTags();
    return result;
}

static int
call_rangebox(CALLARGS)
{
    int min_value;

    *offset_add = arg_rest(av);
    min_value = numeric_arg(av, 4);
    return dialog_rangebox(t,
			   av[1],
			   numeric_arg(av, 2),
			   numeric_arg(av, 3),
			   min_value,
			   numeric_arg(av, 5),
			   (*offset_add > 6) ? numeric_arg(av, 6) : min_value);
}

static int
call_treeview(CALLARGS)
{
    int tags = howmany_tags(av + 5, TREEVIEW_TAGS);
    int result;

    DisableNoTags();

    *offset_add = arg_rest(av);
    result = dialog_treeview(t,
			     av[1],
			     numeric_arg(av, 2),
			     numeric_arg(av, 3),
			     numeric_arg(av, 4),
			     tags, av + 5, FLAG_RADIO);
    RestoreNoTags();
    return result;
}
#endif /* HAVE_XDIALOG */

#ifdef HAVE_DLG_FORMBOX
static int
call_form(CALLARGS)
{
    int group = FORMBOX_TAGS;
    int tags = howmany_tags(av + 5, group);
    *offset_add = 5 + tags * group;

    return dialog_form(t,
		       av[1],
		       numeric_arg(av, 2),
		       numeric_arg(av, 3),
		       numeric_arg(av, 4),
		       tags, av + 5);
}

static int
call_password_form(CALLARGS)
{
    unsigned save = dialog_vars.formitem_type;
    int result;

    dialog_vars.formitem_type = 1;
    result = call_form(PASSARGS);
    dialog_vars.formitem_type = save;

    return result;
}
#endif /* HAVE_DLG_FORMBOX */

#ifdef HAVE_DLG_MIXEDFORM
static int
call_mixed_form(CALLARGS)
{
    int group = MIXEDFORM_TAGS;
    int tags = howmany_tags(av + 5, group);
    *offset_add = 5 + tags * group;

    return dialog_mixedform(t,
			    av[1],
			    numeric_arg(av, 2),
			    numeric_arg(av, 3),
			    numeric_arg(av, 4),
			    tags, av + 5);
}
#endif /* HAVE_DLG_MIXEDFORM */

#ifdef HAVE_DLG_GAUGE
static int
call_gauge(CALLARGS)
{
    *offset_add = arg_rest(av);
    return dialog_gauge(t,
			av[1],
			numeric_arg(av, 2),
			numeric_arg(av, 3),
			optional_num(av, 4, 0));
}

static int
call_pause(CALLARGS)
{
    *offset_add = arg_rest(av);
    return dialog_pause(t,
			av[1],
			numeric_arg(av, 2),
			numeric_arg(av, 3),
			numeric_arg(av, 4));
}
#endif

#ifdef HAVE_MIXEDGAUGE
static int
call_mixed_gauge(CALLARGS)
{
#define MIXEDGAUGE_BASE 5
    int tags = howmany_tags(av + MIXEDGAUGE_BASE, MIXEDGAUGE_TAGS);
    *offset_add = MIXEDGAUGE_BASE + tags * MIXEDGAUGE_TAGS;
    return dialog_mixedgauge(t,
			     av[1],
			     numeric_arg(av, 2),
			     numeric_arg(av, 3),
			     numeric_arg(av, 4),
			     tags, av + MIXEDGAUGE_BASE);
}
#endif

#ifdef HAVE_DLG_GAUGE
static int
call_prgbox(CALLARGS)
{
    *offset_add = arg_rest(av);
    /* the original version does not accept a prompt string, but for
     * consistency we allow it.
     */
    return ((*offset_add == 5)
	    ? dialog_prgbox(t,
			    av[1],
			    av[2],
			    numeric_arg(av, 3),
			    numeric_arg(av, 4), TRUE)
	    : dialog_prgbox(t,
			    "",
			    av[1],
			    numeric_arg(av, 2),
			    numeric_arg(av, 3), TRUE));
}
#endif

#ifdef HAVE_DLG_GAUGE
static int
call_programbox(CALLARGS)
{
    int result;

    *offset_add = arg_rest(av);
    /* this function is a compromise between --prgbox and --progressbox.
     */
    result = ((*offset_add == 4)
	      ? dlg_progressbox(t,
				av[1],
				numeric_arg(av, 2),
				numeric_arg(av, 3),
				TRUE,
				dialog_state.pipe_input)
	      : dlg_progressbox(t,
				"",
				numeric_arg(av, 1),
				numeric_arg(av, 2),
				TRUE,
				dialog_state.pipe_input));
    dialog_state.pipe_input = 0;
    return result;
}
#endif

#ifdef HAVE_DLG_GAUGE
static int
call_progressbox(CALLARGS)
{
    *offset_add = arg_rest(av);
    /* the original version does not accept a prompt string, but for
     * consistency we allow it.
     */
    return ((*offset_add == 4)
	    ? dialog_progressbox(t,
				 av[1],
				 numeric_arg(av, 2),
				 numeric_arg(av, 3))
	    : dialog_progressbox(t,
				 "",
				 numeric_arg(av, 1),
				 numeric_arg(av, 2)));
}
#endif

#ifdef HAVE_DLG_TAILBOX
static int
call_tailbox(CALLARGS)
{
    *offset_add = 4;
    return dialog_tailbox(t,
			  av[1],
			  numeric_arg(av, 2),
			  numeric_arg(av, 3),
			  FALSE);
}

static int
call_tailboxbg(CALLARGS)
{
    *offset_add = 4;
    return dialog_tailbox(t,
			  av[1],
			  numeric_arg(av, 2),
			  numeric_arg(av, 3),
			  TRUE);
}
#endif
/* *INDENT-OFF* */
static const Mode modes[] =
{
    {o_yesno,           4, 4, call_yesno},
    {o_msgbox,          4, 4, call_msgbox},
    {o_infobox,         4, 4, call_infobox},
    {o_textbox,         4, 4, call_textbox},
    {o_menu,            6, 0, call_menu},
    {o_inputmenu,       6, 0, call_inputmenu},
    {o_checklist,       7, 0, call_checklist},
    {o_radiolist,       7, 0, call_radiolist},
    {o_inputbox,        4, 5, call_inputbox},
    {o_passwordbox,     4, 5, call_passwordbox},
#ifdef HAVE_DLG_GAUGE
    {o_gauge,           4, 5, call_gauge},
    {o_pause,           5, 5, call_pause},
    {o_prgbox,          4, 5, call_prgbox},
    {o_programbox,      3, 4, call_programbox},
    {o_progressbox,     3, 4, call_progressbox},
#endif
#ifdef HAVE_DLG_FORMBOX
    {o_passwordform,   13, 0, call_password_form},
    {o_form,           13, 0, call_form},
#endif
#ifdef HAVE_MIXEDGAUGE
    {o_mixedgauge,      MIXEDGAUGE_BASE, 0, call_mixed_gauge},
#endif
#ifdef HAVE_DLG_MIXEDFORM
    {o_mixedform,      13, 0, call_mixed_form},
#endif
#ifdef HAVE_DLG_TAILBOX
    {o_tailbox,         4, 4, call_tailbox},
    {o_tailboxbg,       4, 4, call_tailboxbg},
#endif
#ifdef HAVE_XDIALOG
    {o_calendar,        4, 7, call_calendar},
    {o_dselect,         4, 5, call_dselect},
    {o_editbox,         4, 4, call_editbox},
    {o_fselect,         4, 5, call_fselect},
    {o_timebox,         4, 7, call_timebox},
#endif
#ifdef HAVE_XDIALOG2
    {o_buildlist,       4, 0, call_buildlist},
    {o_rangebox,        5, 7, call_rangebox},
    {o_treeview,        4, 0, call_treeview},
#endif
};
/* *INDENT-ON* */

static char *
optionString(char **argv, int *num)
{
    int next = *num + 1;
    char *result = argv[next];
    if (result == 0) {
	char temp[80];
	sprintf(temp, "Expected a string-parameter for %.20s", argv[*num]);
	Usage(temp);
    }
    *num = next;
    return result;
}

static int
optionValue(char **argv, int *num)
{
    int next = *num + 1;
    char *src = argv[next];
    char *tmp = 0;
    int result = 0;

    if (src != 0) {
	result = (int) strtol(src, &tmp, 0);
	if (tmp == 0 || *tmp != 0)
	    src = 0;
    }

    if (src == 0) {
	char temp[80];
	sprintf(temp, "Expected a numeric-parameter for %.20s", argv[*num]);
	Usage(temp);
    }
    *num = next;
    return result;
}

/*
 * In findOption, we made provision for adding/removing a "no" prefix from
 * any boolean option.  This function determines the actual true/false result.
 */
static bool
optionBool(const char *actual, const Options * data)
{
    bool normal = (data->type == tTrue) ? TRUE : FALSE;
    bool result = !strcmp(actual + 2, data->name) ? normal : !normal;
    if (result != normal) {
	int want_no = (strncmp(actual + 2, "no", 2) == 0);
	int have_no = (strncmp(data->name, "no", 2) == 0);
	if (have_no == want_no)
	    result = normal;
    }
    return result;
}

/* Return exit-code for a named button */
static int
button_code(const char *name)
{
    /* *INDENT-OFF* */
    static struct {
	const char *name;
	int code;
    } table[] = {
	{ "ok",	    DLG_EXIT_OK },
	{ "yes",    DLG_EXIT_OK },
	{ "cancel", DLG_EXIT_CANCEL },
	{ "no",	    DLG_EXIT_CANCEL },
	{ "help",   DLG_EXIT_HELP },
	{ "extra",  DLG_EXIT_EXTRA },
    };
    /* *INDENT-ON* */

    int code = DLG_EXIT_ERROR;
    size_t i;

    for (i = 0; i < TableSize(table); i++) {
	if (!dlg_strcmp(name, table[i].name)) {
	    code = table[i].code;
	    break;
	}
    }

    if (code == DLG_EXIT_ERROR) {
	char temp[80];
	sprintf(temp, "Button name \"%.20s\" unknown", name);
	Usage(temp);
    }

    return code;
}

/*
 * If this is the last option, we do not want any error messages - just our
 * output.  Calling end_dialog() cancels the refresh() at the end of the
 * program as well.
 */
static void
IgnoreNonScreen(char **argv, int offset)
{
    if (argv[offset + 1] == 0) {
	ignore_unknown = TRUE;
	end_dialog();
    }
}

static void
PrintTextOnly(char **argv, int *offset, eOptions code)
{
    /* TODO - handle two optional numeric params */
    char *text;
    int height = 0;
    int width = 0;
    int height2 = 0;
    int width2 = 0;
    int next = arg_rest(argv + *offset);

    if (LINES <= 0 && COLS <= 0) {
	dlg_ttysize(fileno(dialog_state.input),
		    &dialog_state.screen_height,
		    &dialog_state.screen_width);
    }

    text = strdup(optionString(argv, offset));
    IgnoreNonScreen(argv, *offset);

    if (next >= 1) {
	next = MIN(next, 3);
	height = numeric_arg(argv, *offset + 1);
	if (next >= 2)
	    width = numeric_arg(argv, *offset + 2);
	*offset += next - 1;
    }

    dlg_trim_string(text);
    dlg_auto_size(NULL, text, &height2, &width2, height, width);

    switch (code) {
    case o_text_only:
	dialog_state.text_only = TRUE;
	dlg_print_autowrap(stdscr, text, height2, width2);
	dialog_state.text_only = FALSE;
	break;
    case o_print_text_size:
	fprintf(dialog_state.output, "%d %d\n",
		dialog_state.text_height,
		dialog_state.text_width);
	break;
    default:
	break;
    }
}

/*
 * Print parts of a message
 */
static void
PrintList(const char *const *list)
{
    const char *leaf = strrchr(program, '/');
    unsigned n = 0;

    if (leaf != 0)
	leaf++;
    else
	leaf = program;

    while (*list != 0) {
	fprintf(dialog_state.output, *list, n ? leaf : dialog_version());
	(void) fputc('\n', dialog_state.output);
	n = 1;
	list++;
    }
}

static const Mode *
lookupMode(eOptions code)
{
    const Mode *modePtr = 0;
    unsigned n;

    for (n = 0; n < TableSize(modes); n++) {
	if (modes[n].code == code) {
	    modePtr = &modes[n];
	    break;
	}
    }
    return modePtr;
}

static int
compare_opts(const void *a, const void *b)
{
    Options *const *p = (Options * const *) a;
    Options *const *q = (Options * const *) b;
    return strcmp((*p)->name, (*q)->name);
}

/*
 * Print program's version.
 */
static void
PrintVersion(FILE *fp)
{
    fprintf(fp, "Version: %s\n", dialog_version());
}

/*
 * Print program help-message
 */
static void
Help(void)
{
    static const char *const tbl_1[] =
    {
	"cdialog (ComeOn Dialog!) version %s",
	"Copyright 2000-2020,2021 Thomas E. Dickey",
	"This is free software; see the source for copying conditions.  There is NO",
	"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.",
	"",
	"* Display dialog boxes from shell scripts *",
	"",
	"Usage: %s <options> { --and-widget <options> }",
	"where options are \"common\" options, followed by \"box\" options",
	"",
#ifdef HAVE_RC_FILE
	"Special options:",
	"  [--create-rc \"file\"]",
#endif
	0
    }, *const tbl_3[] =
    {
	"",
	"Auto-size with height and width = 0. Maximize with height and width = -1.",
	"Global-auto-size if also menu_height/list_height = 0.",
	0
    };
    size_t limit = TableSize(options);
    size_t j, k;
    const Options **opts;

    end_dialog();
    dialog_state.output = stdout;

    opts = dlg_calloc(const Options *, limit);
    assert_ptr(opts, "Help");
    for (j = 0; j < limit; ++j) {
	opts[j] = &(options[j]);
    }
    qsort(opts, limit, sizeof(Options *), compare_opts);

    PrintList(tbl_1);
    fprintf(dialog_state.output, "Common options:\n ");
    for (j = k = 0; j < limit; j++) {
	if ((opts[j]->pass & 1)
	    && opts[j]->help != 0) {
	    size_t len = 6 + strlen(opts[j]->name) + strlen(opts[j]->help);
	    k += len;
	    if (k > 75) {
		fprintf(dialog_state.output, "\n ");
		k = len;
	    }
	    fprintf(dialog_state.output, " [--%s%s%s]", opts[j]->name,
		    *(opts[j]->help) ? " " : "", opts[j]->help);
	}
    }
    fprintf(dialog_state.output, "\nBox options:\n");
    for (j = 0; j < limit; j++) {
	if ((opts[j]->pass & 2) != 0
	    && opts[j]->help != 0
	    && lookupMode(opts[j]->code)) {
	    fprintf(dialog_state.output, "  --%-12s %s\n", opts[j]->name,
		    opts[j]->help);
	}
    }
    PrintList(tbl_3);

    free(opts);
    handle_leaks();
    dlg_exit(DLG_EXIT_OK);
}

#ifdef HAVE_DLG_TRACE
/*
 * Only the first call to dlg_trace will open a trace file.  But each time
 * --trace is parsed, we show the whole parameter list as it is at that moment,
 * counting discarded parameters.  The only way to capture the whole parameter
 * list is if --trace is the first option.
 */
static void
process_trace_option(char **argv, int *offset)
{
    int j;

    if (dialog_state.trace_output == 0) {
	dlg_trace(optionString(argv, offset));
    } else {
	DLG_TRACE(("# ignore extra --trace option\n"));
	*offset += 1;
    }

    DLG_TRACE(("# Parameters:\n"));
    for (j = 0; argv[j] != 0; ++j) {
	DLG_TRACE(("#\targv[%d] = %s\n", j, argv[j]));
    }
}
#endif

/*
 * "Common" options apply to all widgets more/less.  Most of the common options
 * set values in dialog_vars, a few set dialog_state and a couple write to the
 * output stream.
 */
static int
process_common_options(int argc, char **argv, int offset, int output)
{
    const Options *data = NULL;
    bool done = FALSE;

    DLG_TRACE(("# process_common_options, offset %d:%d\n", offset, argc));

    while (!done && offset < argc) {
	static char empty[] = "";
	eOptions code = o_unknown;
	char *sval = empty;
	bool bval;
	int nval;
	char *target;

	DLG_TRACE(("#\targv[%d] = %s\n", offset, argv[offset]));
	if ((data = findOption(argv[offset], 1, FALSE)) == NULL) {
	    done = TRUE;
	    continue;
	}

	switch (data->vars) {
	default:
	    target = NULL;
	    break;
	case 1:
	    target = (char *) &dialog_state;
	    break;
	case 2:
	    target = (char *) &dialog_vars;
	    break;
	}

#define TraceTarget \
	    ((target == (char *) &dialog_state) \
	     ? "dialog_state" \
	     : "dialog_vars")
#define TraceBVal (bval ? "TRUE" : "FALSE")
#define TraceDone(fmt,value) \
	DLG_TRACE(("#\t.. set %s.%s = %"fmt"\n", TraceTarget, data->name, value))
#define TraceLate(fmt,value) \
	DLG_TRACE(("#\t.. defer setting %s = %"fmt"\n", data->name, value))

	code = data->code;
	switch (data->type) {
	default:
	    break;
	case tFalse:
	case tTrue:
	    bval = optionBool(argv[offset], data);
	    if (target != NULL) {
		*(bool *) (target + data->offset) = bval;
		TraceDone("s", TraceBVal);
	    } else {
		TraceLate("s", TraceBVal);
	    }
	    break;
	case tNumber:
	    nval = optionValue(argv, &offset);
	    if (target != NULL) {
		*(int *) (void *) (target + data->offset) = nval;
		TraceDone("d", nval);
	    } else {
		TraceLate("d", nval);
	    }
	    break;
	case tString:
	    sval = optionString(argv, &offset);
	    if (target != NULL) {
		*(char **) (void *) (target + data->offset) = sval;
		TraceDone("s", sval);
	    } else {
		TraceLate("s", sval);
	    }
	    break;
	}

	switch (code) {
	case o_defaultno:
	    dialog_vars.default_button = DLG_EXIT_CANCEL;
	    break;
	case o_default_button:
	    dialog_vars.default_button = button_code(sval);
	    dialog_vars.defaultno = dialog_vars.default_button == DLG_EXIT_CANCEL;
	    break;
	case o_text_only:
	case o_print_text_size:
	    PrintTextOnly(argv, &offset, code);
	    break;
	case o_print_maxsize:
	    if (output) {
		IgnoreNonScreen(argv, offset);
		fflush(dialog_state.output);
		fprintf(dialog_state.output, "MaxSize: %d, %d\n", SLINES, SCOLS);
	    }
	    break;
	case o_print_version:
	    if (output) {
		PrintVersion(dialog_state.output);
	    }
	    break;
	case o_visit_items:
	    dialog_state.visit_cols = 1;
	    break;
	case o_begin_set:
#ifdef HAVE_WHIPTAIL
	    if (!strcmp(argv[offset], "--topleft")) {
		dialog_vars.begin_y = 0;
		dialog_vars.begin_x = 0;
	    } else
#endif
	    {
		dialog_vars.begin_y = optionValue(argv, &offset);
		dialog_vars.begin_x = optionValue(argv, &offset);
	    }
	    break;
	case o_ascii_lines:
	    dialog_vars.no_lines = FALSE;
	    break;
	case o_no_lines:
	    dialog_vars.ascii_lines = FALSE;
	    break;
	case o_no_mouse:
	    mouse_close();
	    break;
	case o_unknown:
	    done = !ignore_unknown;
	default:
	    break;
#ifdef HAVE_DLG_TRACE
	case o_trace:
	    process_trace_option(argv, &offset);
	    break;
#endif
	case o_iso_week:
	    if (dialog_vars.week_start == 0) {	/* Monday is implied */
		static char default_1st[] = "1";
		dialog_vars.week_start = default_1st;
	    }
	    break;
	}
	if (!done)
	    offset++;
    }

    if (dialog_state.aspect_ratio == 0)
	dialog_state.aspect_ratio = DEFAULT_ASPECT_RATIO;

    return offset;
}

/*
 * Initialize options at the start of a series of common options culminating
 * in a widget.
 */
static void
init_result(char *buffer)
{
    static bool first = TRUE;

    DLG_TRACE(("# init_result\n"));

    /* clear everything we do not save for the next widget */
    memset(&dialog_vars, 0, sizeof(dialog_vars));

    dialog_vars.input_result = buffer;
    dialog_vars.input_result[0] = '\0';

    dialog_vars.default_button = -1;

    /*
     * The first time this is called, check for common options given by an
     * environment variable.
     */
    if (first) {
	char *env = dlg_getenv_str("DIALOGOPTS");
	if (env != 0)
	    env = dlg_strclone(env);
	if (env != 0) {
	    special_argv = dlg_string_to_argv(env);
	    special_argc = dlg_count_argv(special_argv);
	}
	first = FALSE;
    }

    if (special_argv != 0) {
	process_common_options(special_argc, special_argv, 0, FALSE);
    }
}

int
main(int argc, char *argv[])
{
    char temp[256];
    bool esc_pressed = FALSE;
    bool keep_tite = FALSE;
    int initial = 1;
    int offset = 1;
    int offset_add = 0;
    int retval = DLG_EXIT_OK;
    int j;
    eOptions code;
    char my_buffer[MAX_LEN + 1];

    memset(&dialog_state, 0, sizeof(dialog_state));
    memset(&dialog_vars, 0, sizeof(dialog_vars));

#if defined(ENABLE_NLS)
    /* initialize locale support */
    setlocale(LC_ALL, "");
    bindtextdomain(NLS_TEXTDOMAIN, LOCALEDIR);
    textdomain(NLS_TEXTDOMAIN);
#elif defined(HAVE_SETLOCALE)
    (void) setlocale(LC_ALL, "");
#endif

    init_result(my_buffer);	/* honor $DIALOGOPTS */
    unescape_argv(&argc, &argv);
    program = argv[0];
    dialog_state.output = stderr;
    dialog_state.input = stdin;

    /*
     * Look for the last --stdout, --stderr or --output-fd option, and use
     * that.  We can only write to one of them.  If --stdout is used, that
     * can interfere with initializing the curses library, so we want to
     * know explicitly if it is used.
     *
     * Also, look for any --version or --help message, processing those
     * immediately.
     */
    while (offset < argc) {
	int base = offset;
	switch (lookupOption(argv[offset], 7)) {
	case o_output_stdout:
	    dialog_state.output = stdout;
	    break;
	case o_output_stderr:
	    dialog_state.output = stderr;
	    break;
	case o_input_fd:
	    if ((j = optionValue(argv, &offset)) < 0
		|| (dialog_state.input = fdopen(j, "r")) == 0) {
		handle_leaks();
		dlg_exiterr("Cannot open input-fd\n");
	    }
	    break;
	case o_output_fd:
	    if ((j = optionValue(argv, &offset)) < 0
		|| (dialog_state.output = fdopen(j, "w")) == 0) {
		handle_leaks();
		dlg_exiterr("Cannot open output-fd\n");
	    }
	    break;
	case o_keep_tite:
	    keep_tite = TRUE;
	    break;
	case o_version:
	    dialog_state.output = stdout;
	    PrintVersion(dialog_state.output);
	    dlg_exit(DLG_EXIT_OK);
	    break;
	case o_help:
	    Help();
	    break;
#ifdef HAVE_DLG_TRACE
	case o_trace:
	    /*
	     * Process/remove the --trace option if it is the first option.
	     * Otherwise, process it in more/less expected order as a
	     * "common" option.
	     */
	    if (base == 1) {
		process_trace_option(argv, &offset);
		break;
	    } else {
		++offset;
		continue;
	    }
#endif
	default:
	    ++offset;
	    continue;
	}
	DLG_TRACE(("# discarding %d parameters starting with argv[%d] (%s)\n",
		   1 + offset - base, base,
		   argv[base]));
	for (j = base; j < argc; ++j) {
	    dialog_argv[j] = dialog_argv[j + 1 + (offset - base)];
	}
	argc -= (1 + offset - base);
	offset = base;
    }
    offset = 1;
    init_result(my_buffer);
    dialog_vars.keep_tite |= keep_tite;		/* init_result() cleared global */

    /*
     * Dialog's output may be redirected (see above).  Handle the special
     * case of options that only report information without interaction.
     */
    if (argc == 2) {
	switch (code = lookupOption(argv[1], 7)) {
	case o_print_maxsize:
	    (void) initscr();
	    endwin();
	    fflush(dialog_state.output);
	    fprintf(dialog_state.output, "MaxSize: %d, %d\n", SLINES, SCOLS);
	    break;
	case o_print_version:
	    PrintVersion(dialog_state.output);
	    break;
	case o_dlg_clear_screen:
	    initscr();
	    refresh();
	    dlg_keep_tite((dialog_state.output == stdout) ? stderr : stdout);
	    endwin();
	    break;
	case o_ignore:
	    break;
	default:
	    Help();
	    break;
	}
	dlg_exit(DLG_EXIT_OK);
    } else if (argc < 2) {
	Help();
    }
#ifdef HAVE_RC_FILE
    else if (lookupOption(argv[1], 7) == o_create_rc) {
	if (argc != 3) {
	    sprintf(temp, "Expected a filename for %.50s", argv[1]);
	    Usage(temp);
	}
	if (dlg_parse_rc() == -1) {	/* Read the configuration file */
	    handle_leaks();
	    dlg_exiterr("dialog: dlg_parse_rc");
	}
	dlg_create_rc(argv[2]);
	dlg_exit(DLG_EXIT_OK);
    }
#endif
    else {
	/*
	 * Handle combinations of common options including --print-text-only
	 * which can be done before involving curses, in case we can exit
	 * without initializing curses (and writing to the terminal).
	 */
	initial = process_common_options(argc, argv, offset, TRUE);
	if (initial >= argc)
	    dlg_exit(DLG_EXIT_OK);
    }

    init_dialog(dialog_state.input, dialog_state.output);

    while (offset < argc && !esc_pressed) {
	int have;
	const Mode *modePtr;

	init_result(my_buffer);
	offset = process_common_options(argc, argv, offset, offset > initial);

	if (argv[offset] == NULL) {
	    if (ignore_unknown)
		break;
	    Usage("Expected a box option");
	}

	if (dialog_vars.separate_output) {
	    switch (lookupOption(argv[offset], 2)) {
#ifdef HAVE_XDIALOG2
	    case o_buildlist:
	    case o_treeview:
#endif
	    case o_checklist:
		break;
	    default:
		sprintf(temp,
			"Unexpected widget with --separate-output %.20s",
			argv[offset]);
		Usage(temp);
	    }
	}

	dlg_put_backtitle();

	/* use a table to look for the requested mode, to avoid code duplication */

	modePtr = 0;
	if ((code = lookupOption(argv[offset], 2)) != o_unknown)
	    modePtr = lookupMode(code);
	if (modePtr == 0) {
	    sprintf(temp, "%s option %.20s",
		    lookupOption(argv[offset], 7) != o_unknown
		    ? "Unexpected"
		    : "Unknown",
		    argv[offset]);
	    Usage(temp);
	}

	have = arg_rest(&argv[offset]);
	if (have < modePtr->argmin) {
	    sprintf(temp, "Expected at least %d tokens for %.20s, have %d",
		    modePtr->argmin - 1, argv[offset],
		    have - 1);
	    Usage(temp);
	}
	if (modePtr->argmax && have > modePtr->argmax) {
	    sprintf(temp,
		    "Expected no more than %d tokens for %.20s, have %d",
		    modePtr->argmax - 1, argv[offset],
		    have - 1);
	    Usage(temp);
	}

	/*
	 * Trim whitespace from non-title option values, e.g., the ones that
	 * will be used as captions or prompts.   Do that only for the widget
	 * we are about to process, since the "--trim" option is reset before
	 * accumulating options for each widget.
	 */
	for (j = offset + 1; j <= offset + have; j++) {
	    switch (lookupOption(argv[j - 1], 7)) {
	    case o_unknown:
	    case o_title:
	    case o_backtitle:
	    case o_help_line:
	    case o_help_file:
		break;
	    default:
		if (argv[j] != 0) {
		    char *argv_j = strdup(argv[j]);
		    if (argv_j != 0) {
			dlg_trim_string(argv_j);
			argv[j] = argv_j;
		    } else {
			argv[j] = strdup("?");
		    }
		    ignore_leak(argv[j]);
		}
		break;
	    }
	}

	DLG_TRACE(("# execute %s\n", argv[offset]));
	retval = show_result((*(modePtr->jumper)) (dialog_vars.title,
						   argv + offset,
						   &offset_add));
	DLG_TRACE(("# widget returns %d\n", retval));
	offset += offset_add;

	if (dialog_vars.input_result != my_buffer) {
	    free(dialog_vars.input_result);
	    dialog_vars.input_result = 0;
	}

	if (retval == DLG_EXIT_ESC) {
	    esc_pressed = TRUE;
	} else {

	    if (dialog_vars.beep_after_signal)
		(void) beep();

	    if (dialog_vars.sleep_secs)
		(void) napms(dialog_vars.sleep_secs * 1000);

	    if (offset < argc) {
		switch (lookupOption(argv[offset], 7)) {
		case o_and_widget:
		    offset++;
		    break;
		case o_unknown:
		    sprintf(temp, "Expected --and-widget, not %.20s",
			    argv[offset]);
		    Usage(temp);
		    break;
		default:
		    /* if we got a cancel, etc., stop chaining */
		    if (retval != DLG_EXIT_OK)
			esc_pressed = TRUE;
		    else
			dialog_vars.dlg_clear_screen = TRUE;
		    break;
		}
	    }
	    if (dialog_vars.dlg_clear_screen)
		dlg_clear();
	}
    }

    dlg_killall_bg(&retval);
    if (dialog_state.screen_initialized) {
	(void) refresh();
	end_dialog();
    }
    handle_leaks();
    dlg_exit(retval);
}
