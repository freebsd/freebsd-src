/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2022 Alfonso Sabato Siciliano
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/ioctl.h>

#include <getopt.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <term.h>
#include <time.h>
#include <unistd.h>

#include <bsddialog.h>
#include <bsddialog_theme.h>

#include "util_theme.h"

enum OPTS {
	/* Options */
	ALTERNATE_SCREEN = '?' + 1,
	AND_DIALOG,
	ASCII_LINES,
	BACKTITLE,
	BEGIN_X,
	BEGIN_Y,
	BIKESHED,
	CANCEL_LABEL,
	CLEAR_DIALOG,
	CLEAR_SCREEN,
	COLORS,
	COLUMNS_PER_ROW,
	CR_WRAP,
	DATE_FORMAT,
	DEFAULT_BUTTON,
	DEFAULT_ITEM,
	DEFAULT_NO,
	DISABLE_ESC,
	ESC_RETURNCANCEL,
	EXIT_LABEL,
	EXTRA_BUTTON,
	EXTRA_LABEL,
	GENERIC_BUTTON1,
	GENERIC_BUTTON2,
	HELP_BUTTON,
	HELP_LABEL,
	HELP_PRINT_NAME,
	HELP_STATUS,
	HFILE,
	HLINE,
	HMSG,
	IGNORE,
	INSECURE,
	ITEM_BOTTOM_DESC,
	ITEM_DEPTH,
	ITEM_PREFIX,
	LOAD_THEME,
	MAX_INPUT,
	NO_CANCEL,
	NO_DESCRIPTIONS,
	NO_LINES,
	NO_NAMES,
	NO_OK,
	NO_SHADOW,
	NORMAL_SCREEN,
	OK_LABEL,
	OUTPUT_FD,
	OUTPUT_SEPARATOR,
	PRINT_MAXSIZE,
	PRINT_SIZE,
	PRINT_VERSION,
	QUOTED,
	SAVE_THEME,
	SEPARATE_OUTPUT,
	SHADOW,
	SINGLE_QUOTED,
	SLEEP,
	STDERR,
	STDOUT,
	SWITCH_BUTTONS,
	TAB_ESCAPE,
	TAB_LEN,
	TEXT_UNCHANGED,
	THEME,
	TIME_FORMAT,
	TITLE,
	/* Dialogs */
	CALENDAR,
	CHECKLIST,
	DATEBOX,
	FORM,
	GAUGE,
	INFOBOX,
	INPUTBOX,
	MENU,
	MIXEDFORM,
	MIXEDGAUGE,
	MSGBOX,
	PASSWORDBOX,
	PASSWORDFORM,
	PAUSE,
	RADIOLIST,
	RANGEBOX,
	TEXTBOX,
	TIMEBOX,
	TREEVIEW,
	YESNO
};

/* options descriptor */
static struct option longopts[] = {
	/* Options */
	{"alternate-screen",  no_argument,       NULL, ALTERNATE_SCREEN},
	{"and-dialog",        no_argument,       NULL, AND_DIALOG},
	{"and-widget",        no_argument,       NULL, AND_DIALOG},
	{"ascii-lines",       no_argument,       NULL, ASCII_LINES},
	{"backtitle",         required_argument, NULL, BACKTITLE},
	{"begin-x",           required_argument, NULL, BEGIN_X},
	{"begin-y",           required_argument, NULL, BEGIN_Y},
	{"bikeshed",          no_argument,       NULL, BIKESHED},
	{"cancel-label",      required_argument, NULL, CANCEL_LABEL},
	{"clear",             no_argument,       NULL, CLEAR_SCREEN},
	{"clear-dialog",      no_argument,       NULL, CLEAR_DIALOG},
	{"clear-screen",      no_argument,       NULL, CLEAR_SCREEN},
	{"colors",            no_argument,       NULL, COLORS},
	{"columns-per-row",   required_argument, NULL, COLUMNS_PER_ROW},
	{"cr-wrap",           no_argument,       NULL, CR_WRAP},
	{"date-format",       required_argument, NULL, DATE_FORMAT},
	{"defaultno",         no_argument,       NULL, DEFAULT_NO},
	{"default-button",    required_argument, NULL, DEFAULT_BUTTON},
	{"default-item",      required_argument, NULL, DEFAULT_ITEM},
	{"default-no",        no_argument,       NULL, DEFAULT_NO},
	{"disable-esc",       no_argument,       NULL, DISABLE_ESC},
	{"esc-return-cancel", no_argument,       NULL, ESC_RETURNCANCEL},
	{"exit-label",        required_argument, NULL, EXIT_LABEL},
	{"extra-button",      no_argument,       NULL, EXTRA_BUTTON},
	{"extra-label",       required_argument, NULL, EXTRA_LABEL},
	{"generic-button1",   required_argument, NULL, GENERIC_BUTTON1},
	{"generic-button2",   required_argument, NULL, GENERIC_BUTTON2},
	{"help-button",       no_argument,       NULL, HELP_BUTTON},
	{"help-label",        required_argument, NULL, HELP_LABEL},
	{"help-print-name",   no_argument,       NULL, HELP_PRINT_NAME},
	{"help-status",       no_argument,       NULL, HELP_STATUS},
	{"help-tags",         no_argument,       NULL, HELP_PRINT_NAME},
	{"hfile",             required_argument, NULL, HFILE},
	{"hline",             required_argument, NULL, HLINE},
	{"hmsg",              required_argument, NULL, HMSG},
	{"ignore",            no_argument,       NULL, IGNORE},
	{"insecure",          no_argument,       NULL, INSECURE},
	{"item-bottom-desc",  no_argument,       NULL, ITEM_BOTTOM_DESC},
	{"item-depth",        no_argument,       NULL, ITEM_DEPTH},
	{"item-help",         no_argument,       NULL, ITEM_BOTTOM_DESC},
	{"item-prefix",       no_argument,       NULL, ITEM_PREFIX},
	{"keep-tite",         no_argument,       NULL, ALTERNATE_SCREEN},
	{"load-theme",        required_argument, NULL, LOAD_THEME},
	{"max-input",         required_argument, NULL, MAX_INPUT},
	{"no-cancel",         no_argument,       NULL, NO_CANCEL},
	{"nocancel",          no_argument,       NULL, NO_CANCEL},
	{"no-descriptions",   no_argument,       NULL, NO_DESCRIPTIONS},
	{"no-items",          no_argument,       NULL, NO_DESCRIPTIONS},
	{"no-label",          required_argument, NULL, CANCEL_LABEL},
	{"no-lines",          no_argument,       NULL, NO_LINES},
	{"no-names",          no_argument,       NULL, NO_NAMES},
	{"no-ok",             no_argument,       NULL, NO_OK},
	{"nook ",             no_argument,       NULL, NO_OK},
	{"no-shadow",         no_argument,       NULL, NO_SHADOW},
	{"no-tags",           no_argument,       NULL, NO_NAMES},
	{"normal-screen",     no_argument,       NULL, NORMAL_SCREEN},
	{"ok-label",          required_argument, NULL, OK_LABEL},
	{"output-fd",         required_argument, NULL, OUTPUT_FD},
	{"output-separator",  required_argument, NULL, OUTPUT_SEPARATOR},
	{"print-maxsize",     no_argument,       NULL, PRINT_MAXSIZE},
	{"print-size",        no_argument,       NULL, PRINT_SIZE},
	{"print-version",     no_argument,       NULL, PRINT_VERSION},
	{"quoted",            no_argument,       NULL, QUOTED},
	{"save-theme",        required_argument, NULL, SAVE_THEME},
	{"separate-output",   no_argument,       NULL, SEPARATE_OUTPUT},
	{"separator",         required_argument, NULL, OUTPUT_SEPARATOR},
	{"shadow",            no_argument,       NULL, SHADOW},
	{"single-quoted",     no_argument,       NULL, SINGLE_QUOTED},
	{"sleep",             required_argument, NULL, SLEEP},
	{"stderr",            no_argument,       NULL, STDERR},
	{"stdout",            no_argument,       NULL, STDOUT},
	{"switch-buttons",    no_argument,       NULL, SWITCH_BUTTONS},
	{"tab-escape",        no_argument,       NULL, TAB_ESCAPE},
	{"tab-len",           required_argument, NULL, TAB_LEN},
	{"text-unchanged",    no_argument,       NULL, TEXT_UNCHANGED},
	{"theme",             required_argument, NULL, THEME},
	{"time-format",       required_argument, NULL, TIME_FORMAT},
	{"title",             required_argument, NULL, TITLE},
	{"yes-label",         required_argument, NULL, OK_LABEL},
	/* Dialogs */
	{"calendar",     no_argument, NULL, CALENDAR},
	{"checklist",    no_argument, NULL, CHECKLIST},
	{"datebox",      no_argument, NULL, DATEBOX},
	{"form",         no_argument, NULL, FORM},
	{"gauge",        no_argument, NULL, GAUGE},
	{"infobox",      no_argument, NULL, INFOBOX},
	{"inputbox",     no_argument, NULL, INPUTBOX},
	{"menu",         no_argument, NULL, MENU},
	{"mixedform",    no_argument, NULL, MIXEDFORM},
	{"mixedgauge",   no_argument, NULL, MIXEDGAUGE},
	{"msgbox",       no_argument, NULL, MSGBOX},
	{"passwordbox",  no_argument, NULL, PASSWORDBOX},
	{"passwordform", no_argument, NULL, PASSWORDFORM},
	{"pause",        no_argument, NULL, PAUSE},
	{"radiolist",    no_argument, NULL, RADIOLIST},
	{"rangebox",     no_argument, NULL, RANGEBOX},
	{"textbox",      no_argument, NULL, TEXTBOX},
	{"timebox",      no_argument, NULL, TIMEBOX},
	{"treeview",     no_argument, NULL, TREEVIEW},
	{"yesno",        no_argument, NULL, YESNO},
	/* END */
	{ NULL, 0, NULL, 0}
};

/* Menus options */
static bool item_prefix_opt;
static bool item_bottomdesc_opt;
static bool item_output_sepnl_opt;
static bool item_singlequote_opt;
static bool list_items_on_opt;
static bool item_help_print_name_opt;
static bool item_always_quote_opt;
static bool item_depth_opt;
static char *item_output_sep_opt;
static char *item_default_opt;
/* Date and Time options */
static char *date_fmt_opt;
static char *time_fmt_opt;
/* Forms options */
static int unsigned max_input_form_opt;
/* General options */
static bool esc_return_cancel_opt;
static bool ignore_opt;
static int output_fd_opt;
static int getH_opt;
static int getW_opt;
/* Text option */
static bool cr_wrap_opt;
static bool tab_escape_opt;
static bool text_unchanged_opt;
/* Theme and Screen options*/
static bool bikeshed_opt;
static enum bsddialog_default_theme theme_opt;
static char *backtitle_opt;
static bool clear_screen_opt;
static char *loadthemefile;
static char *savethemefile;
static const char *screen_mode_opt;

/* Functions */
#define UNUSED_PAR(x) UNUSED_ ## x __attribute__((__unused__))
static void custom_text(char *text, char *buf);
static void usage(void);
/* Dialogs */
#define BUILDER_ARGS struct bsddialog_conf *conf, char* text, int rows,        \
	int cols, int argc, char **argv
static int calendar_builder(BUILDER_ARGS);
static int checklist_builder(BUILDER_ARGS);
static int datebox_builder(BUILDER_ARGS);
static int form_builder(BUILDER_ARGS);
static int gauge_builder(BUILDER_ARGS);
static int infobox_builder(BUILDER_ARGS);
static int inputbox_builder(BUILDER_ARGS);
static int menu_builder(BUILDER_ARGS);
static int mixedform_builder(BUILDER_ARGS);
static int mixedgauge_builder(BUILDER_ARGS);
static int msgbox_builder(BUILDER_ARGS);
static int passwordbox_builder(BUILDER_ARGS);
static int passwordform_builder(BUILDER_ARGS);
static int pause_builder(BUILDER_ARGS);
static int radiolist_builder(BUILDER_ARGS);
static int rangebox_builder(BUILDER_ARGS);
static int textbox_builder(BUILDER_ARGS);
static int timebox_builder(BUILDER_ARGS);
static int treeview_builder(BUILDER_ARGS);
static int yesno_builder(BUILDER_ARGS);

/* init, exit and internals */
static bool in_bsddialog_mode;
static bool mandatory_dialog;
static int (*dialogbuilder)(BUILDER_ARGS);

static void exit_error(const char *errstr, bool with_usage)
{
	if (in_bsddialog_mode)
		bsddialog_end();

	printf("Error: %s.\n\n", errstr);
	if (with_usage) {
		printf("See \'bsddialog --help\' or \'man 1 bsddialog\' ");
		printf("for more information.\n");
	}

	exit (255);
}

static void sigint_handler(int UNUSED_PAR(sig))
{
	bsddialog_end();

	exit(255);
}

static void start_bsddialog_mode(void)
{
	if (in_bsddialog_mode)
		return;

	if (bsddialog_init() != BSDDIALOG_OK)
		exit_error(bsddialog_geterror(), false);

	in_bsddialog_mode = true;
	signal(SIGINT, sigint_handler);
}

static void error_args(const char *dialog, int argc, char **argv)
{
	int i;

	if (in_bsddialog_mode)
		bsddialog_end();

	printf("Error: %s unexpected argument%s:", dialog,
	    argc > 1 ? "s" : "");
	for (i = 0; i < argc; i++)
		printf(" \"%s\"", argv[i]);
	printf(".\n\n");
	printf("See \'bsddialog --help\' or \'man 1 bsddialog\' ");
	printf("for more information.\n");

	exit (255);
}

static void usage(void)
{
	printf("usage: bsddialog --help\n");
	printf("       bsddialog --version\n");
	printf("       bsddialog [--<opt>] --<dialog> <text> <rows> <cols> "
	    "[<arg>]\n");
	printf("       bsddialog --<dialog1> ... [--and-dialog --<dialog2> "
	    "...] ...\n");
	printf("\n");

	printf("Options:\n");
	printf(" --alternate-screen, --ascii-lines, --backtitle <backtitle>,"
	    " --begin-x <x>,\n --begin-y <y>, --bikeshed, --calendar,"
	    " --cancel-label <label>, --clear-dialog,\n --clear-screen,"
	    " --colors, --columns-per-row <columns>, --cr-wrap,\n"
	    " --date-format <format>, --default-button <label>,"
	    " --default-item <name>,\n --default-no, --disable-esc,"
	    " --esc-return-cancel, --exit-label <label>,\n --extra-button,"
	    " --extra-label <label>, --generic-button1 <label>,\n"
	    " --generic-button2 <label>, --help-button, --help-label <label>,\n"
	    " --help-print-name, --help-status, --hfile <file>,"
	    " --hline <string>,\n --hmsg <string>, --ignore, --insecure,"
	    " --item-bottom-desc, --item-depth,\n --item-prefix,"
	    " --load-theme <file>, --max-input <size>, --no-cancel,\n"
	    " --no-descriptions, --no-label <label>, --no-lines, --no-names,"
	    " --no-ok,\n --no-shadow, --normal-screen, --ok-label <label>,"
	    " --output-fd <fd>,\n --output-separator <sep>, --print-maxsize,"
	    " --print-size, --print-version,\n --quoted, --save-theme <file>,"
	    " --separate-output, --separator <sep>, --shadow,\n"
	    " --single-quoted, --sleep <secs>, --stderr, --stdout,"
	    " --tab-escape,\n --tab-len <spaces>, --text-unchanged,"
	    " --switch-buttons,\n --theme <blackwhite|bsddialog|flat|dialog>,"
	    " --time-format <format>,\n --title <title>,"
	    " --yes-label <label>.\n");
	printf("\n");

	printf("Dialogs:\n");
	printf(" --calendar <text> <rows> <cols> [<dd> <mm> <yy>]\n");
	printf(" --checklist <text> <rows> <cols> <menurows> [<name> <desc> "
	    "<on|off>] ...\n");
	printf(" --datebox <text> <rows> <cols> [<dd> <mm> <yy>]\n");
	printf(" --form <text> <rows> <cols> <formrows> [<label> <ylabel> "
	    "<xlabel> <init> <yfield> <xfield> <fieldlen> <maxletters>] "
	    "...\n");
	printf(" --gauge <text> <rows> <cols> [<perc>]\n");
	printf(" --infobox <text> <rows> <cols>\n");
	printf(" --inputbox <text> <rows> <cols> [init]\n");
	printf(" --menu <text> <rows> <cols> <menurows> [<name> <desc>] ...\n");
	printf(" --mixedform <text> <rows> <cols> <formrows> [<label> <ylabel> "
	    "<xlabel> <init> <yfield> <xfield> <fieldlen> <maxletters> "
	    "<0|1|2>] ...\n");
	printf(" --mixedgauge <text> <rows> <cols> <mainperc> [<minilabel> "
	    "<miniperc>] ...\n");
	printf(" --msgbox <text> <rows> <cols>\n");
	printf(" --passwordbox <text> <rows> <cols> [init]\n");
	printf(" --passwordform <text> <rows> <cols> <formrows> [<label> "
	    "<ylabel> <xlabel> <init> <yfield> <xfield> <fieldlen> "
	    "<maxletters>] ...\n");
	printf(" --pause <text> <rows> <cols> <secs>\n");
	printf(" --radiolist <text> <rows> <cols> <menurows> [<name> <desc> "
	    "<on|off>] ...\n");
	printf(" --rangebox <text> <rows> <cols> <min> <max> [<init>]\n");
	printf(" --textbox <file> <rows> <cols>\n");
	printf(" --timebox <text> <rows> <cols> [<hh> <mm> <ss>]\n");
	printf(" --treeview <text> <rows> <cols> <menurows> [<depth> <name> "
	    "<desc> <on|off>] ...\n");
	printf(" --yesno <text> <rows> <cols>\n");
	printf("\n");

	printf("See 'man 1 bsddialog' for more information.\n");
}

static int parseargs(int argc, char **argv, struct bsddialog_conf *conf)
{
	int arg, parsed, i;
	struct winsize ws;

	bsddialog_initconf(conf);
	conf->key.enable_esc = true;
	conf->menu.on_without_ok = true;
	conf->form.value_without_ok = true;
	conf->button.always_active = true;

	dialogbuilder = NULL;

	backtitle_opt = NULL;
	theme_opt = -1;
	output_fd_opt = STDERR_FILENO;
	ignore_opt = false;
	cr_wrap_opt = false;
	tab_escape_opt = false;
	text_unchanged_opt = false;
	esc_return_cancel_opt = false;
	bikeshed_opt = false;
	savethemefile = NULL;
	loadthemefile = NULL;
	clear_screen_opt = false;
	screen_mode_opt = NULL;

	item_output_sepnl_opt = false;
	item_singlequote_opt = false;
	item_prefix_opt = false;
	item_bottomdesc_opt = false;
	item_depth_opt = false;
	list_items_on_opt = false;
	item_help_print_name_opt = false;
	item_always_quote_opt = false;
	item_output_sep_opt = NULL;
	item_default_opt = NULL;

	date_fmt_opt = NULL;
	time_fmt_opt = NULL;

	max_input_form_opt = 2048;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--and-dialog") == 0 ||
		    strcmp(argv[i], "--and-widget") == 0) {
			argc = i + 1;
			break;
		}
	}
	parsed = argc;
	while ((arg = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
		switch (arg) {
		/* Options */
		case ALTERNATE_SCREEN:
			screen_mode_opt = "smcup";
			break;
		case AND_DIALOG:
			if (dialogbuilder == NULL)
				exit_error("--and-dialog without previous "
				    "--<dialog>", true);
			break;
		case ASCII_LINES:
			conf->ascii_lines = true;
			break;
		case BACKTITLE:
			backtitle_opt = optarg;
			if (conf->y == BSDDIALOG_CENTER)
				conf->auto_topmargin = 2;
			break;
		case BEGIN_X:
			conf->x = (int)strtol(optarg, NULL, 10);
			if (conf->x < BSDDIALOG_CENTER)
				exit_error("--begin-x < -1", false);
			break;
		case BEGIN_Y:
			conf->y = (int)strtol(optarg, NULL, 10);
			if (conf->y < BSDDIALOG_CENTER)
				exit_error("--begin-y < -1", false);
			conf->auto_topmargin = 0;
			break;
		case BIKESHED:
			bikeshed_opt = true;
			break;
		case CANCEL_LABEL:
			conf->button.cancel_label = optarg;
			break;
		case CLEAR_DIALOG:
			conf->clear = true;
			break;
		case CLEAR_SCREEN:
			mandatory_dialog = false;
			clear_screen_opt = true;
			break;
		case COLORS:
			conf->text.highlight = true;
			break;
		case COLUMNS_PER_ROW:
			conf->text.cols_per_row =
			    (u_int)strtoul(optarg, NULL, 10);
			break;
		case CR_WRAP:
			cr_wrap_opt = true;
			break;
		case DATE_FORMAT:
			date_fmt_opt = optarg;
			break;
		case DEFAULT_BUTTON:
			conf->button.default_label = optarg;
			break;
		case DEFAULT_ITEM:
			item_default_opt = optarg;
			break;
		case DEFAULT_NO:
			conf->button.default_cancel = true;
			break;
		case DISABLE_ESC:
			conf->key.enable_esc = false;
			break;
		case ESC_RETURNCANCEL:
			esc_return_cancel_opt = true;
			break;
		case EXIT_LABEL:
			conf->button.ok_label = optarg;
			break;
		case EXTRA_BUTTON:
			conf->button.with_extra = true;
			break;
		case EXTRA_LABEL:
			conf->button.extra_label = optarg;
			break;
		case GENERIC_BUTTON1:
			conf->button.generic1_label = optarg;
			break;
		case GENERIC_BUTTON2:
			conf->button.generic2_label = optarg;
			break;
		case HELP_BUTTON:
			conf->button.with_help = true;
			break;
		case HELP_LABEL:
			conf->button.help_label = optarg;
			break;
		case HELP_PRINT_NAME:
			item_help_print_name_opt = true;
			break;
		case HELP_STATUS:
			list_items_on_opt = true;
			break;
		case HFILE:
			conf->key.f1_file = optarg;
			break;
		case HLINE:
			if (optarg[0] != '\0')
				conf->bottomtitle = optarg;
			break;
		case HMSG:
			conf->key.f1_message = optarg;
			break;
		case IGNORE:
			ignore_opt = true;
			break;
		case INSECURE:
			conf->form.securech = '*';
			break;
		case ITEM_BOTTOM_DESC:
			item_bottomdesc_opt = true;
			break;
		case ITEM_DEPTH:
			item_depth_opt = true;
			break;
		case ITEM_PREFIX:
			item_prefix_opt = true;
			break;
		case LOAD_THEME:
			loadthemefile = optarg;
			break;
		case MAX_INPUT:
			max_input_form_opt = (u_int)strtoul(optarg, NULL, 10);
			break;
		case NO_CANCEL:
			conf->button.without_cancel = true;
			break;
		case NO_DESCRIPTIONS:
			conf->menu.no_desc = true;
			break;
		case NO_LINES:
			conf->no_lines = true;
			break;
		case NO_NAMES:
			conf->menu.no_name = true;
			break;
		case NO_OK:
			conf->button.without_ok = true;
			break;
		case NO_SHADOW:
			conf->shadow = false;
			break;
		case NORMAL_SCREEN:
			screen_mode_opt = "rmcup";
			break;
		case OK_LABEL:
			conf->button.ok_label = optarg;
			break;
		case OUTPUT_FD:
			output_fd_opt = (int)strtol(optarg, NULL, 10);
			break;
		case OUTPUT_SEPARATOR:
			item_output_sep_opt = optarg;
			break;
		case QUOTED:
			item_always_quote_opt = true;
			break;
		case PRINT_MAXSIZE:
			mandatory_dialog = false;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
			dprintf(output_fd_opt, "MaxSize: %d, %d\n",
			    ws.ws_row, ws.ws_col);
			break;
		case PRINT_SIZE:
			conf->get_height = &getH_opt;
			conf->get_width = &getW_opt;
			break;
		case PRINT_VERSION:
			mandatory_dialog = false;
			dprintf(output_fd_opt, "Version: %s\n",
			    LIBBSDDIALOG_VERSION);
			break;
		case SAVE_THEME:
			mandatory_dialog = false;
			savethemefile = optarg;
			break;
		case SEPARATE_OUTPUT:
			item_output_sepnl_opt = true;
			break;
		case SHADOW:
			conf->shadow = true;
			break;
		case SINGLE_QUOTED:
			item_singlequote_opt = true;
			break;
		case SLEEP:
			conf->sleep = (u_int)strtoul(optarg, NULL, 10);
			break;
		case STDERR:
			output_fd_opt = STDERR_FILENO;
			break;
		case STDOUT:
			output_fd_opt = STDOUT_FILENO;
			break;
		case SWITCH_BUTTONS:
			conf->button.always_active = false;
			break;
		case TAB_ESCAPE:
			tab_escape_opt = true;
			break;
		case TAB_LEN:
			conf->text.tablen = (u_int)strtoul(optarg, NULL, 10);
			break;
		case TEXT_UNCHANGED:
			text_unchanged_opt = true;
			break;
		case THEME:
			if (strcasecmp(optarg, "bsddialog") == 0)
				theme_opt = BSDDIALOG_THEME_BSDDIALOG;
			else if (strcasecmp(optarg, "blackwhite") == 0)
				theme_opt = BSDDIALOG_THEME_BLACKWHITE;
			else if (strcasecmp(optarg, "flat") == 0)
				theme_opt = BSDDIALOG_THEME_FLAT;
			else if (strcasecmp(optarg, "dialog") == 0)
				theme_opt = BSDDIALOG_THEME_DIALOG;
			else
				exit_error("--theme: <unknown> theme", false);
			break;
		case TIME_FORMAT:
			time_fmt_opt = optarg;
			break;
		case TITLE:
			conf->title = optarg;
			break;
		/* Dialogs */
		case CALENDAR:
			if (dialogbuilder != NULL)
				exit_error("unexpected --calendar", true);
			dialogbuilder = calendar_builder;
			break;
		case CHECKLIST:
			if (dialogbuilder != NULL)
				exit_error("unexpected --checklist", true);
			dialogbuilder = checklist_builder;
			conf->auto_downmargin = 1;
			break;
		case DATEBOX:
			if (dialogbuilder != NULL)
				exit_error("unexpected --datebox", true);
			dialogbuilder = datebox_builder;
			break;
		case FORM:
			if (dialogbuilder != NULL)
				exit_error("unexpected --form", true);
			dialogbuilder = form_builder;
			conf->auto_downmargin = 1;
			break;
		case GAUGE:
			if (dialogbuilder != NULL)
				exit_error("unexpected --gauge", true);
			dialogbuilder = gauge_builder;
			break;
		case INFOBOX:
			if (dialogbuilder != NULL)
				exit_error("unexpected --infobox", true);
			dialogbuilder = infobox_builder;
			break;
		case INPUTBOX:
			if (dialogbuilder != NULL)
				exit_error("unexpected --inputbox", true);
			dialogbuilder = inputbox_builder;
			conf->auto_downmargin = 1;
			break;
		case MENU:
			if (dialogbuilder != NULL)
				exit_error("unexpected --menu", true);
			dialogbuilder = menu_builder;
			conf->auto_downmargin = 1;
			break;
		case MIXEDFORM:
			if (dialogbuilder != NULL)
				exit_error("unexpected --mixedform", true);
			dialogbuilder = mixedform_builder;
			conf->auto_downmargin = 1;
			break;
		case MIXEDGAUGE:
			if (dialogbuilder != NULL)
				exit_error("unexpected --mixedgauge", true);
			dialogbuilder = mixedgauge_builder;
			break;
		case MSGBOX:
			if (dialogbuilder != NULL)
				exit_error("unexpected --msgbox", true);
			dialogbuilder = msgbox_builder;
			break;
		case PAUSE:
			if (dialogbuilder != NULL)
				exit_error("unexpected --pause", true);
			dialogbuilder = pause_builder;
			break;
		case PASSWORDBOX:
			if (dialogbuilder != NULL)
				exit_error("unexpected --passwordbox", true);
			dialogbuilder = passwordbox_builder;
			conf->auto_downmargin = 1;
			break;
		case PASSWORDFORM:
			if (dialogbuilder != NULL)
				exit_error("unexpected --passwordform", true);
			dialogbuilder = passwordform_builder;
			conf->auto_downmargin = 1;
			break;
		case RADIOLIST:
			if (dialogbuilder != NULL)
				exit_error("unexpected --radiolist", true);
			dialogbuilder = radiolist_builder;
			conf->auto_downmargin = 1;
			break;
		case RANGEBOX:
			if (dialogbuilder != NULL)
				exit_error("unexpected --rangebox", true);
			dialogbuilder = rangebox_builder;
			break;
		case TEXTBOX:
			if (dialogbuilder != NULL)
				exit_error("unexpected --textbox", true);
			dialogbuilder = textbox_builder;
			break;
		case TIMEBOX:
			if (dialogbuilder != NULL)
				exit_error("unexpected --timebox", true);
			dialogbuilder = timebox_builder;
			break;
		case TREEVIEW:
			if (dialogbuilder != NULL)
				exit_error("unexpected --treeview", true);
			dialogbuilder = treeview_builder;
			conf->auto_downmargin = 1;
			break;
		case YESNO:
			if (dialogbuilder != NULL)
				exit_error("unexpected --yesno", true);
			dialogbuilder = yesno_builder;
			break;
		default: /* Error */
			if (ignore_opt == true)
				break;
			exit_error("--ignore to continue", true);
		}
	}

	return (parsed);
}

int main(int argc, char *argv[argc])
{
	int i, rows, cols, retval, parsed, nargc, firstoptind;
	char *text, **nargv, *pn;
	struct bsddialog_conf conf;

	setlocale(LC_ALL, "");

	in_bsddialog_mode = false;
	mandatory_dialog = true;
	firstoptind = optind;
	pn = argv[0];
	retval = BSDDIALOG_OK;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--version") == 0) {
			printf("Version: %s\n", LIBBSDDIALOG_VERSION);
			return (BSDDIALOG_OK);
		}
		if (strcmp(argv[i], "--help") == 0) {
			usage();
			return (BSDDIALOG_OK);
		}
	}

	while (true) {
		parsed = parseargs(argc, argv, &conf);
		nargc = argc - parsed;
		nargv = argv + parsed;
		argc = parsed - optind;
		argv += optind;

		if (mandatory_dialog && dialogbuilder == NULL)
			exit_error("expected a --<dialog>", true);

		if (dialogbuilder == NULL && argc > 0)
			error_args("(no --<dialog>)", argc, argv);

		/* --print-maxsize or --print-version */
		if (mandatory_dialog == false && savethemefile == NULL &&
		    clear_screen_opt == false)
			return (BSDDIALOG_OK);

		/* --<dialog>, --save-theme or clear-screen */
		if (dialogbuilder != NULL) {
			if (argc < 3)
				exit_error("expected <text> <rows> <cols>",
				    true);
			if ((text = strdup(argv[0])) == NULL)
				exit_error("cannot allocate text", false);
			if (dialogbuilder != textbox_builder)
				custom_text(argv[0], text);
			rows = (int)strtol(argv[1], NULL, 10);
			cols = (int)strtol(argv[2], NULL, 10);
			argc -= 3;
			argv += 3;
		}

		/* bsddialog terminal mode (first iteration) */
		start_bsddialog_mode();

		if (screen_mode_opt != NULL) {
			screen_mode_opt = tigetstr(screen_mode_opt);
			if (screen_mode_opt != NULL &&
			    screen_mode_opt != (char*)-1) {
				tputs(screen_mode_opt, 1, putchar);
				fflush(stdout);
				 /* only to refresh, useless in the library */
				bsddialog_clearterminal();
			}
		}

		/* theme */
		if (theme_opt >= 0)
			bsddialog_set_default_theme(theme_opt);
		if (loadthemefile != NULL)
			loadtheme(loadthemefile);
		if (bikeshed_opt)
			bikeshed(&conf);
		if (savethemefile != NULL)
			savetheme(savethemefile, LIBBSDDIALOG_VERSION);

		/* backtitle and dialog */
		if (dialogbuilder == NULL)
			break;
		if (backtitle_opt != NULL)
			if(bsddialog_backtitle(&conf, backtitle_opt))
				exit_error(bsddialog_geterror(), false);
		retval = dialogbuilder(&conf, text, rows, cols, argc, argv);
		free(text);
		if (retval == BSDDIALOG_ERROR)
			exit_error(bsddialog_geterror(), false);
		if (retval == BSDDIALOG_ESC && esc_return_cancel_opt)
			retval = BSDDIALOG_CANCEL;
		if (conf.get_height != NULL && conf.get_width != NULL)
			dprintf(output_fd_opt, "DialogSize: %d, %d\n",
			    *conf.get_height, *conf.get_width);
		if (clear_screen_opt)
			bsddialog_clearterminal();
		clear_screen_opt = false;
		/* --and-dialog ends loop with Cancel or ESC */
		if (retval == BSDDIALOG_CANCEL || retval == BSDDIALOG_ESC)
			break;
		argc = nargc;
		argv = nargv;
		if (argc <= 0)
			break;
		/* prepare next parseargs() call */
		argc++;
		argv--;
		argv[0] = pn;
		optind = firstoptind;
	}

	if (in_bsddialog_mode) {
		/* --clear-screen can be a single option */
		if (clear_screen_opt)
			bsddialog_clearterminal();
		bsddialog_end();
	}
	/* end bsddialog terminal mode */

	return (retval);
}

void custom_text(char *text, char *buf)
{
	bool trim, crwrap;
	int i, j;

	if (strstr(text, "\\n") == NULL) {
		/* "hasnl" mode */
		trim = true;
		crwrap = true;
	} else {
		trim = false;
		crwrap = cr_wrap_opt;
	}
	if (text_unchanged_opt) {
		trim = false;
		crwrap = true;
	}

	i = j = 0;
	while (text[i] != '\0') {
		switch (text[i]) {
		case '\\':
			buf[j] = '\\';
			switch (text[i+1]) {
			case 'n': /* implicitly in "hasnl" mode */
				buf[j] = '\n';
				i++;
				if (text[i+1] == '\n')
					i++;
				break;
			case 't':
				if (tab_escape_opt) {
					buf[j] = '\t';
				} else {
					j++;
					buf[j] = 't';
				}
				i++;
				break;
			}
			break;
		case '\n':
			buf[j] = crwrap ? '\n' : ' ';
			break;
		case '\t':
			buf[j] = text_unchanged_opt ? '\t' : ' ';
			break;
		default:
			buf[j] = text[i];
		}
		i++;
		if (!trim || buf[j] != ' ' || j == 0 || buf[j-1] != ' ')
			j++;
	}
	buf[j] = '\0';
}

/* Dialogs */
int gauge_builder(BUILDER_ARGS)
{
	int output;
	unsigned int perc;

	perc = 0;
	if (argc == 1) {
		perc = (u_int)strtoul(argv[0], NULL, 10);
		perc = perc > 100 ? 100 : perc;
	} else if (argc > 1) {
		error_args("--gauge", argc - 1, argv + 1);
	}

	output = bsddialog_gauge(conf, text, rows, cols, perc, STDIN_FILENO,
	    "XXX");

	return (output);
}

int infobox_builder(BUILDER_ARGS)
{
	if (argc > 0)
		error_args("--infobox", argc, argv);

	return (bsddialog_infobox(conf, text, rows, cols));
}

int mixedgauge_builder(BUILDER_ARGS)
{
	int output, *minipercs;
	unsigned int i, mainperc, nminibars;
	const char **minilabels;

	if (argc < 1 || (((argc-1) % 2) != 0) )
		exit_error("bad --mixedgauge arguments", true);

	mainperc = (u_int)strtoul(argv[0], NULL, 10);
	mainperc = mainperc > 100 ? 100 : mainperc;
	argc--;
	argv++;

	nminibars  = argc / 2;
	if ((minilabels = calloc(nminibars, sizeof(char*))) == NULL)
		exit_error("Cannot allocate memory for minilabels", false);
	if ((minipercs = calloc(nminibars, sizeof(int))) == NULL)
		exit_error("Cannot allocate memory for minipercs", false);

	for (i = 0; i < nminibars; i++) {
		minilabels[i] = argv[i * 2];
		minipercs[i] = (int)strtol(argv[i * 2 + 1], NULL, 10);
	}

	output = bsddialog_mixedgauge(conf, text, rows, cols, mainperc,
	    nminibars, minilabels, minipercs);

	return (output);
}

int msgbox_builder(BUILDER_ARGS)
{
	if (argc > 0)
		error_args("--msgbox", argc, argv);

	return (bsddialog_msgbox(conf, text, rows, cols));
}

int pause_builder(BUILDER_ARGS)
{
	int output;
	unsigned int secs;

	if (argc == 0)
		exit_error("--pause missing <seconds>", true);
	if (argc > 1)
		error_args("--pause", argc - 1, argv + 1);

	secs = (u_int)strtoul(argv[0], NULL, 10);
	output = bsddialog_pause(conf, text, rows, cols, secs);

	return (output);
}

int rangebox_builder(BUILDER_ARGS)
{
	int output, min, max, value;

	if (argc < 2)
		exit_error("--rangebox missing <min> <max> [<init>]", true);
	if (argc > 3)
		error_args("--rangebox", argc - 3, argv + 3);

	min = (int)strtol(argv[0], NULL, 10);
	max = (int)strtol(argv[1], NULL, 10);

	if (argc == 3) {
		value = (int)strtol(argv[2], NULL, 10);
		value = value < min ? min : value;
		value = value > max ? max : value;
	} else
		value = min;

	output = bsddialog_rangebox(conf, text, rows, cols, min, max, &value);
	dprintf(output_fd_opt, "%d", value);

	return (output);
}

int textbox_builder(BUILDER_ARGS)
{
	if (argc > 0)
		error_args("--textbox", argc, argv);

	return (bsddialog_textbox(conf, text, rows, cols));
}

int yesno_builder(BUILDER_ARGS)
{
	if (argc > 0)
		error_args("--yesno", argc, argv);

	return (bsddialog_yesno(conf, text, rows, cols));
}

/* CALENDAR, DATE and TIME */
static int date(BUILDER_ARGS, bool is_datebox)
{
	int ret;
	unsigned int yy, mm, dd;
	time_t cal;
	struct tm *localtm;
	char stringdate[1024];
	const char *name;

	name = is_datebox ? "--datebox" : "--calendar";
	time(&cal);
	localtm = localtime(&cal);
	yy = localtm->tm_year + 1900;
	mm = localtm->tm_mon + 1;
	dd = localtm->tm_mday;

	if (argc > 3) {
		error_args(name, argc - 3, argv + 3);
	} else if (argc == 3) {
		dd = (u_int)strtoul(argv[0], NULL, 10);
		mm = (u_int)strtoul(argv[1], NULL, 10);
		yy = (u_int)strtoul(argv[2], NULL, 10);
		if (yy < 1900)
			yy = 1900;
		/* max yy check is in lib */
	}

	if (is_datebox)
		ret = bsddialog_datebox(conf, text, rows, cols, &yy, &mm, &dd);
	else
		ret = bsddialog_calendar(conf, text, rows, cols, &yy, &mm, &dd);
	if (ret != BSDDIALOG_OK)
		return (ret);

	if (date_fmt_opt != NULL) {
		time(&cal);
		localtm = localtime(&cal);
		localtm->tm_year = yy - 1900;
		localtm->tm_mon = mm - 1;
		localtm->tm_mday = dd;
		strftime(stringdate, 1024, date_fmt_opt, localtm);
		dprintf(output_fd_opt, "%s", stringdate);
	} else if (bikeshed_opt && (dd % 2 == 0)) {
		dprintf(output_fd_opt, "%u/%u/%u", dd, mm, yy);
	} else {
		dprintf(output_fd_opt, "%02u/%02u/%u", dd, mm, yy);
	}

	return (ret);
}

int calendar_builder(BUILDER_ARGS)
{
	if (rows == 2) {
		/*
		 * (bsdconfig/share/dialog.subr:1352) f_dialog_calendar_size()
		 * computes height 2 for `dialog --calendar' in
		 * (bsdconfig/usermgmt/share/user_input.subr:517)
		 * f_dialog_input_expire_password() and
		 * (bsdconfig/usermgmt/share/user_input.subr:660)
		 * f_dialog_input_expire_account().
		 * Use height auto-sizing that is min height like dialog,
		 * documented in bsddialog(1).
		 */
		rows = 0;
	}

	return (date(conf, text, rows, cols, argc, argv, false));
}

int datebox_builder(BUILDER_ARGS)
{
	return (date(conf, text, rows, cols, argc, argv, true));
}

int timebox_builder(BUILDER_ARGS)
{
	int output;
	unsigned int hh, mm, ss;
	time_t clock;
	struct tm *localtm;
	char stringtime[1024];

	time(&clock);
	localtm = localtime(&clock);
	hh = localtm->tm_hour;
	mm = localtm->tm_min;
	ss = localtm->tm_sec;

	if (argc > 3) {
		error_args("--timebox", argc - 3, argv + 3);
	} else if (argc == 3) {
		hh = (u_int)strtoul(argv[0], NULL, 10);
		mm = (u_int)strtoul(argv[1], NULL, 10);
		ss = (u_int)strtoul(argv[2], NULL, 10);
	}

	output = bsddialog_timebox(conf, text, rows, cols, &hh, &mm, &ss);
	if (output != BSDDIALOG_OK)
		return (output);

	if (time_fmt_opt != NULL) {
		time(&clock);
		localtm = localtime(&clock);
		localtm->tm_hour = hh;
		localtm->tm_min = mm;
		localtm->tm_sec = ss;
		strftime(stringtime, 1024, time_fmt_opt, localtm);
		dprintf(output_fd_opt, "%s", stringtime);
	} else if (bikeshed_opt && (ss % 2 == 0)) {
		dprintf(output_fd_opt, "%u:%u:%u", hh, mm, ss);
	} else {
		dprintf(output_fd_opt, "%02u:%02u:%02u", hh, mm, ss);
	}

	return (output);
}

/* MENU */
static void
get_menu_items(int argc, char **argv, bool setprefix, bool setdepth,
    bool setname, bool setdesc, bool setstatus, bool sethelp,
    unsigned int *nitems, struct bsddialog_menuitem **items, int *focusitem)
{
	unsigned int i, j, sizeitem;

	*focusitem = -1;

	sizeitem = 0;
	sizeitem += setprefix ? 1 : 0;
	sizeitem += setdepth  ? 1 : 0;
	sizeitem += setname   ? 1 : 0;
	sizeitem += setdesc   ? 1 : 0;
	sizeitem += setstatus ? 1 : 0;
	sizeitem += sethelp   ? 1 : 0;
	if ((argc % sizeitem) != 0)
		exit_error("\"menu\" bad arguments items number", true);

	*nitems = argc / sizeitem;

	*items = calloc(*nitems, sizeof(struct bsddialog_menuitem));
	if (items == NULL)
		exit_error("cannot allocate memory \"menu\" items", false);

	j = 0;
	for (i = 0; i < *nitems; i++) {
		(*items)[i].prefix = setprefix ? argv[j++] : "";
		(*items)[i].depth = setdepth ?
		    (u_int)strtoul(argv[j++], NULL, 0) : 0;
		(*items)[i].name = setname ? argv[j++] : "";
		(*items)[i].desc = setdesc ? argv[j++] : "";
		if (setstatus)
			(*items)[i].on = strcmp(argv[j++], "on") == 0 ?
			    true : false;
		else
			(*items)[i].on = false;
		(*items)[i].bottomdesc = sethelp ? argv[j++] : "";

		if (item_default_opt != NULL && *focusitem == -1)
			if (strcmp((*items)[i].name, item_default_opt) == 0)
				*focusitem = i;
	}
}

static void
print_menu_items(int output, int nitems, struct bsddialog_menuitem *items,
    int focusitem, bool ismenu)
{
	bool sep, sepfirst, seplast, toquote;
	int i;
	char quotech;
	const char *focusname, *sepstr;

	sep = false;
	quotech = item_singlequote_opt ? '\'' : '"';

	if (output == BSDDIALOG_ERROR || output == BSDDIALOG_CANCEL ||
	    output == BSDDIALOG_ESC)
		return;

	if (output == BSDDIALOG_HELP) {
		dprintf(output_fd_opt, "HELP ");

		if (focusitem >= 0) {
			focusname = items[focusitem].name;
			if (item_bottomdesc_opt &&
			    item_help_print_name_opt == false)
				focusname = items[focusitem].bottomdesc;

			toquote = false;
			if (strchr(focusname, ' ') != NULL) {
				toquote = item_always_quote_opt;
				if (ismenu == false &&
				    item_output_sepnl_opt == false)
					toquote = true;
			}
			if (toquote) {
				dprintf(output_fd_opt, "%c%s%c",
				    quotech, focusname, quotech);
			} else
				dprintf(output_fd_opt, "%s", focusname);
		}

		if (ismenu || list_items_on_opt == false)
			return;
		sep = true;
	}

	sepfirst = false;
	if ((sepstr = item_output_sep_opt) == NULL)
		sepstr = item_output_sepnl_opt ? "\n" : " ";
	else
		sepfirst = true;

	seplast = false;
	if (item_output_sepnl_opt) {
		sepfirst = false;
		seplast = true;
	}

	for (i = 0; i < nitems; i++) {
		if (items[i].on == false)
			continue;

		if (sep || sepfirst)
			dprintf(output_fd_opt, "%s", sepstr);
		sep = false;

		toquote = false;
		if (strchr(items[i].name, ' ') != NULL) {
			toquote = item_always_quote_opt;
			if (ismenu == false && item_output_sepnl_opt == false)
				toquote = true;
		}
		if (toquote)
			dprintf(output_fd_opt, "%c%s%c",
			    quotech, items[i].name, quotech);
		else
			dprintf(output_fd_opt, "%s", items[i].name);

		if (seplast)
			dprintf(output_fd_opt, "%s", sepstr);
	}
}

int checklist_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1)
		exit_error("--checklist missing <menurows>", true);
	menurows = (u_int)strtoul(argv[0], NULL, 10);

	get_menu_items(argc-1, argv+1, item_prefix_opt, item_depth_opt, true,
	    true, true, item_bottomdesc_opt, &nitems, &items, &focusitem);

	output = bsddialog_checklist(conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(output, nitems, items, focusitem, false);

	free(items);

	return (output);
}

int menu_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1)
		exit_error("--menu missing <menurows>", true);
	menurows = (u_int)strtoul(argv[0], NULL, 10);

	get_menu_items(argc-1, argv+1, item_prefix_opt, item_depth_opt, true,
	    true, false, item_bottomdesc_opt, &nitems, &items, &focusitem);

	output = bsddialog_menu(conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(output, nitems, items, focusitem, true);

	free(items);

	return (output);
}

int radiolist_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1)
		exit_error("--radiolist missing <menurows>", true);
	menurows = (u_int)strtoul(argv[0], NULL, 10);

	get_menu_items(argc-1, argv+1, item_prefix_opt, item_depth_opt, true,
	    true, true, item_bottomdesc_opt, &nitems, &items, &focusitem);

	output = bsddialog_radiolist(conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(output, nitems, items, focusitem, false);

	free(items);

	return (output);
}

int treeview_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1)
		exit_error("--treeview missing <menurows>", true);
	menurows = (u_int)strtoul(argv[0], NULL, 10);

	get_menu_items(argc-1, argv+1, item_prefix_opt, true, true, true, true,
	    item_bottomdesc_opt, &nitems, &items, &focusitem);

	conf->menu.no_name = true;
	conf->menu.align_left = true;

	output = bsddialog_radiolist(conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(output, nitems, items, focusitem, false);

	free(items);

	return (output);
}

/* FORM */
static void
print_form_items(int output, int nitems, struct bsddialog_formitem *items)
{
	int i;

	if (output == BSDDIALOG_ERROR)
		return;

	for (i = 0; i < nitems; i++) {
		dprintf(output_fd_opt, "%s\n", items[i].value);
		free(items[i].value);
	}
}

int form_builder(BUILDER_ARGS)
{
	int output, fieldlen, valuelen;
	unsigned int i, j, flags, formheight, nitems, sizeitem;
	struct bsddialog_formitem *items;

	if (argc < 1)
		exit_error("--form missing <formheight>", true);
	formheight = (u_int)strtoul(argv[0], NULL, 10);

	argc--;
	argv++;
	sizeitem = item_bottomdesc_opt ? 9 : 8;
	if (argc % sizeitem != 0)
		exit_error("--form bad number of arguments items", true);

	nitems = argc / sizeitem;
	if ((items = calloc(nitems, sizeof(struct bsddialog_formitem))) == NULL)
		exit_error("cannot allocate memory for form items", false);
	j = 0;
	for (i = 0; i < nitems; i++) {
		items[i].label	= argv[j++];
		items[i].ylabel = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xlabel = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].init	= argv[j++];
		items[i].yfield	= (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xfield	= (u_int)strtoul(argv[j++], NULL, 10);

		fieldlen = (int)strtol(argv[j++], NULL, 10);
		items[i].fieldlen = abs(fieldlen);

		valuelen = (int)strtol(argv[j++], NULL, 10);
		items[i].maxvaluelen = valuelen == 0 ? abs(fieldlen) : valuelen;

		flags = (fieldlen < 0 ? BSDDIALOG_FIELDREADONLY : 0);
		items[i].flags = flags;

		items[i].bottomdesc = item_bottomdesc_opt ? argv[j++] : "";
	}

	output = bsddialog_form(conf, text, rows, cols, formheight, nitems,
	    items);
	print_form_items(output, nitems, items);
	free(items);

	return (output);
}

int inputbox_builder(BUILDER_ARGS)
{
	int output;
	struct bsddialog_formitem item;

	if (argc > 1)
		error_args("--inputbox", argc - 1, argv + 1);

	item.label	 = "";
	item.ylabel	 = 0;
	item.xlabel	 = 0;
	item.init	 = argc > 0 ? argv[0] : "";
	item.yfield	 = 0;
	item.xfield	 = 0;
	item.fieldlen    = 1;
	item.maxvaluelen = max_input_form_opt;
	item.flags	 = BSDDIALOG_FIELDNOCOLOR;
	item.flags      |= BSDDIALOG_FIELDCURSOREND;
	item.flags      |= BSDDIALOG_FIELDEXTEND;
	item.bottomdesc  = "";

	output = bsddialog_form(conf, text, rows, cols, 1, 1, &item);
	print_form_items(output, 1, &item);

	return (output);
}

int mixedform_builder(BUILDER_ARGS)
{
	int output;
	unsigned int i, j, formheight, nitems, sizeitem;
	struct bsddialog_formitem *items;

	if (argc < 1)
		exit_error("--mixedform missing <formheight>", true);
	formheight = (u_int)strtoul(argv[0], NULL, 10);

	argc--;
	argv++;
	sizeitem = item_bottomdesc_opt ? 10 : 9;
	if (argc % sizeitem != 0)
		exit_error("--mixedform bad number of arguments items", true);

	nitems = argc / sizeitem;
	if ((items = calloc(nitems, sizeof(struct bsddialog_formitem))) == NULL)
		exit_error("cannot allocate memory for form items", false);
	j = 0;
	for (i = 0; i < nitems; i++) {
		items[i].label	     = argv[j++];
		items[i].ylabel      = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xlabel      = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].init	     = argv[j++];
		items[i].yfield	     = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xfield	     = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].fieldlen    = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].maxvaluelen = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].flags       = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].bottomdesc  = item_bottomdesc_opt ? argv[j++] : "";
	}

	output = bsddialog_form(conf, text, rows, cols, formheight, nitems,
	    items);
	print_form_items(output, nitems, items);
	free(items);

	return (output);
}

int passwordbox_builder(BUILDER_ARGS)
{
	int output;
	struct bsddialog_formitem item;

	if (argc > 1)
		error_args("--passwordbox", argc - 1, argv + 1);

	item.label	 = "";
	item.ylabel	 = 0;
	item.xlabel	 = 0;
	item.init	 = argc > 0 ? argv[0] : "";
	item.yfield	 = 0;
	item.xfield	 = 0;
	item.fieldlen	 = 1;
	item.maxvaluelen = max_input_form_opt;
	item.flags       = BSDDIALOG_FIELDHIDDEN;
	item.flags      |= BSDDIALOG_FIELDNOCOLOR;
	item.flags      |= BSDDIALOG_FIELDCURSOREND;
	item.flags      |= BSDDIALOG_FIELDEXTEND;
	item.bottomdesc  = "";

	output = bsddialog_form(conf, text, rows, cols, 1, 1, &item);
	print_form_items(output, 1, &item);

	return (output);
}

int passwordform_builder(BUILDER_ARGS)
{
	int output, fieldlen, valuelen;
	unsigned int i, j, flags, formheight, nitems, sizeitem;
	struct bsddialog_formitem *items;

	if (argc < 1)
		exit_error("--passwordform missing <formheight>", true);
	formheight = (u_int)strtoul(argv[0], NULL, 10);

	argc--;
	argv++;
	sizeitem = item_bottomdesc_opt ? 9 : 8;
	if (argc % sizeitem != 0)
		exit_error("--passwordform bad arguments items number", true);

	flags = BSDDIALOG_FIELDHIDDEN;
	nitems = argc / sizeitem;
	if ((items = calloc(nitems, sizeof(struct bsddialog_formitem))) == NULL)
		exit_error("cannot allocate memory for form items", false);
	j = 0;
	for (i = 0; i < nitems; i++) {
		items[i].label	= argv[j++];
		items[i].ylabel = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xlabel = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].init	= argv[j++];
		items[i].yfield	= (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xfield	= (u_int)strtoul(argv[j++], NULL, 10);

		fieldlen = (int)strtol(argv[j++], NULL, 10);
		items[i].fieldlen = abs(fieldlen);

		valuelen = (int)strtol(argv[j++], NULL, 10);
		items[i].maxvaluelen = valuelen == 0 ? abs(fieldlen) : valuelen;

		flags |= (fieldlen < 0 ? BSDDIALOG_FIELDREADONLY : 0);
		items[i].flags = flags;

		items[i].bottomdesc  = item_bottomdesc_opt ? argv[j++] : "";
	}

	output = bsddialog_form(conf, text, rows, cols, formheight, nitems,
	    items);
	print_form_items(output, nitems, items);
	free(items);

	return (output);
}
