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
#include <time.h>
#include <unistd.h>

#include <bsddialog.h>
#include <bsddialog_theme.h>

#define BSDDIALOG_VERSION "0.1"

enum OPTS {
	/* Common options */
	ASCII_LINES = '?' + 1,
	BACKTITLE,
	BEGIN_X,
	BEGIN_Y,
	CANCEL_LABEL,
	CLEAR,
	COLORS,
	CR_WRAP,
	DATE_FORMAT,
	DEFAULT_BUTTON,
	DEFAULT_ITEM,
	DEFAULT_NO,
	DISABLE_ESC,
	ESC_CANCELVALUE,
	EXIT_LABEL,
	EXTRA_BUTTON,
	EXTRA_LABEL,
	GENERIC_BUTTON1,
	GENERIC_BUTTON2,
	HELP,
	HELP_BUTTON,
	HELP_LABEL,
	HELP_STATUS,
	HELP_TAGS,
	HFILE,
	HLINE,
	HMSG,
	IGNORE,
	INSECURE,
	ITEM_DEPTH,
	ITEM_HELP,
	ITEM_PREFIX,
	MAX_INPUT,
	NO_CANCEL,
	NO_COLLAPSE,
	NO_ITEMS,
	NO_LINES,
	NO_NL_EXPAND,
	NO_OK,
	NO_SHADOW,
	NO_TAGS,
	OK_LABEL,
	OUTPUT_FD,
	OUTPUT_SEPARATOR,
	PRINT_MAXSIZE,
	PRINT_SIZE,
	PRINT_VERSION,
	QUOTED,
	SEPARATE_OUTPUT,
	SHADOW,
	SINGLE_QUOTED,
	SLEEP,
	STDERR,
	STDOUT,
	TAB_LEN,
	THEME,
	TIME_FORMAT,
	TITLE,
	TRIM,
	VERSION,
	/* Dialogs */
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

/* Menus flags and options */
static bool item_prefix_opt, item_bottomdesc_opt, item_output_sepnl_opt;
static bool item_singlequote_opt, list_items_on_opt, item_tag_help_opt;
static bool item_always_quote_opt, item_depth_opt;
static char *item_output_sep_opt, *item_default_opt;
/* Time and calendar options */
static char *date_fmt_opt, *time_fmt_opt;
/* Forms */
static int unsigned max_input_form_opt;
/* General flags and options */
static int output_fd_opt;

static void
custom_text(bool cr_wrap, bool no_collapse, bool no_nl_expand, bool trim,
    char *text, char *buf);
    
static void sigint_handler(int sig);

/* Dialogs */
#define BUILDER_ARGS struct bsddialog_conf conf, char* text, int rows,         \
	int cols, int argc, char **argv, char *errbuf
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

static void usage(void)
{
	printf("usage: bsddialog --help\n");
	printf("       bsddialog --version\n");
	printf("       bsddialog [--<common-opts>] --<dialog> <text> <rows> "
	    "<cols> [--<dialog-opts>]\n");
	printf("\n");

	printf("Common Options:\n");
	printf("--ascii-lines, --backtitle <backtitle>, --begin-x <x>, "
	    "--begin-y <y>, --cancel-label <label>, --clear, --colors, "
	    "--cr-wrap, --date-format <format>, --defaultno, "
	    "--default-button <label>, --default-no, --default-item <name>, "
	    "--disable-esc, --esc-cancelvalue, --exit-label <label>, "
	    "--extra-button, --extra-label <label>, "
	    "--generic-button1 <label>, --generic-button2 <label>, --help, "
	    "--help-button, --help-label <label>, --help-status, --help-tags, "
	    "--hfile <filename>, --hline <string>, --hmsg <string>, --ignore, "
	    "--insecure, --item-depth, --item-help, --items-prefix, "
	    "--max-input <size>, --no-cancel, --nocancel, --no-collapse, "
	    "--no-items, --no-label <label>, --no-lines, --no-nl-expand, "
	    "--no-ok, --nook, --no-shadow, --no-tags, --ok-label <label>, "
	    "--output-fd <fd>, --output-separator <sep>, --print-maxsize, "
	    "--print-size, --print-version, --quoted, --separate-output, "
	    "--separator <sep>, --shadow, --single-quoted, --sleep <secs>, "
	    "--stderr, --stdout, --tab-len <spaces>, "
	    "--theme <blackwhite|bsddialog|default|dialog>, "
	    "--time-format <format>, --title <title>, --trim, --version, "
	    "--yes-label <label>.\n");
	printf("\n");

	printf("Dialogs:\n");
	printf("--checklist <text> <rows> <cols> <menurows> [<name> <desc> "
	    "<on|off>] ...\n");
	printf("--datebox <text> <rows> <cols> [<yy> <mm> <dd>]\n");
	printf("--form <text> <rows> <cols> <formrows> [<label> <ylabel> "
	    "<xlabel> <init> <yfield> <xfield> <fieldlen> <maxvaluelen>] "
	    "...\n");
	printf("--gauge <text> <rows> <cols> [<perc>]\n");
	printf("--infobox <text> <rows> <cols>\n");
	printf("--inputbox <text> <rows> <cols> [init]\n");
	printf("--menu <text> <rows> <cols> <menurows> [<name> <desc>] ...\n");
	printf("--mixedform <text> <rows> <cols> <formrows> [<label> <ylabel> "
	    "<xlabel> <init> <yfield> <xfield> <fieldlen> <maxvaluelen> "
	    "<0|1|2>] ...\n");
	printf("--mixedgauge <text> <rows> <cols> <mainperc> [<minilabel> "
	    "<miniperc>] ...\n");
	printf("--msgbox <text> <rows> <cols>\n");
	printf("--passwordbox <text> <rows> <cols> [init]\n");
	printf("--passwordform <text> <rows> <cols> <formrows> [<label> "
	    "<ylabel> <xlabel> <init> <yfield> <xfield> <fieldlen> "
	    "<maxvaluelen>] ...\n");
	printf("--pause <text> <rows> <cols> <secs>\n");
	printf("--radiolist <text> <rows> <cols> <menurows> [<name> <desc> "
	    "<on|off>] ...\n");
	printf("--rangebox <text> <rows> <cols> <min> <max> [<init>]\n");
	printf("--textbox <file> <rows> <cols>\n");
	printf("--timebox <text> <rows> <cols> [<hh> <mm> <ss>]\n");
	printf("--treeview <text> <rows> <cols> <menurows> [<depth> <name> "
	    "<desc> <on|off>] ...\n");
	printf("--yesno <text> <rows> <cols>\n");
	printf("\n");

	printf("See 'man 1 bsddialog' for more information.\n");
}

int main(int argc, char *argv[argc])
{
	bool cr_wrap_opt, no_collapse_opt, no_nl_expand_opt, trim_opt;
	bool esc_cancelvalue_opt, ignore_opt, print_maxsize_opt;;
	int input, rows, cols, output, getH, getW;
	int (*dialogbuilder)(BUILDER_ARGS) = NULL;
	enum bsddialog_default_theme theme_opt;
	char *text, *backtitle_opt, errorbuilder[1024];
	struct winsize ws;
	struct bsddialog_conf conf;

	setlocale(LC_ALL, "");

	bsddialog_initconf(&conf);
	conf.key.enable_esc = true;
	conf.menu.on_without_ok = true;
	conf.form.value_without_ok = true;

	backtitle_opt = NULL;
	theme_opt = BSDDIALOG_THEME_DEFAULT;
	output_fd_opt = STDERR_FILENO;
	print_maxsize_opt = false;
	ignore_opt = false;
	errorbuilder[0] = '\0';
	cr_wrap_opt = no_collapse_opt = no_nl_expand_opt = trim_opt = false;
	esc_cancelvalue_opt = false;

	item_output_sepnl_opt = item_singlequote_opt = false;
	item_prefix_opt = item_bottomdesc_opt = item_depth_opt = false;
	list_items_on_opt = item_tag_help_opt = false;
	item_always_quote_opt = false;
	item_output_sep_opt = NULL;
	item_default_opt = NULL;

	date_fmt_opt = time_fmt_opt = NULL;

	max_input_form_opt = 0;

	/* options descriptor */
	struct option longopts[] = {
		/* common options */
		{"ascii-lines",      no_argument,       NULL, ASCII_LINES},
		{"backtitle",        required_argument, NULL, BACKTITLE},
		{"begin-x",          required_argument, NULL, BEGIN_X},
		{"begin-y",          required_argument, NULL, BEGIN_Y},
		{"cancel-label",     required_argument, NULL, CANCEL_LABEL},
		{"clear",            no_argument,       NULL, CLEAR},
		{"colors",           no_argument,       NULL, COLORS},
		{"cr-wrap",          no_argument,       NULL, CR_WRAP},
		{"date-format",      required_argument, NULL, DATE_FORMAT},
		{"defaultno",        no_argument,       NULL, DEFAULT_NO},
		{"default-button",   required_argument, NULL, DEFAULT_BUTTON},
		{"default-item",     required_argument, NULL, DEFAULT_ITEM},
		{"default-no",       no_argument,       NULL, DEFAULT_NO},
		{"disable-esc",      no_argument,       NULL, DISABLE_ESC},
		{"esc-cancelvalue",  no_argument,       NULL, ESC_CANCELVALUE},
		{"exit-label",       required_argument, NULL, EXIT_LABEL},
		{"extra-button",     no_argument,       NULL, EXTRA_BUTTON},
		{"extra-label",      required_argument, NULL, EXTRA_LABEL},
		{"generic-button1",  required_argument, NULL, GENERIC_BUTTON1},
		{"generic-button2",  required_argument, NULL, GENERIC_BUTTON2},
		{"help",             no_argument,       NULL, HELP},
		{"help-button",      no_argument,       NULL, HELP_BUTTON},
		{"help-label",       required_argument, NULL, HELP_LABEL},
		{"help-status",      no_argument,       NULL, HELP_STATUS},
		{"help-tags",        no_argument,       NULL, HELP_TAGS},
		{"hfile",            required_argument, NULL, HFILE},
		{"hline",            required_argument, NULL, HLINE},
		{"hmsg",             required_argument, NULL, HMSG},
		{"ignore",           no_argument,       NULL, IGNORE},
		{"insecure",         no_argument,       NULL, INSECURE},
		{"item-depth",       no_argument,       NULL, ITEM_DEPTH},
		{"item-help",        no_argument,       NULL, ITEM_HELP},
		{"item-prefix",      no_argument,       NULL, ITEM_PREFIX},
		{"max-input",        required_argument, NULL, MAX_INPUT},
		{"no-cancel",        no_argument,       NULL, NO_CANCEL},
		{"nocancel",         no_argument,       NULL, NO_CANCEL},
		{"no-collapse",      no_argument,       NULL, NO_COLLAPSE},
		{"no-items",         no_argument,       NULL, NO_ITEMS},
		{"no-label",         required_argument, NULL, CANCEL_LABEL},
		{"no-lines",         no_argument,       NULL, NO_LINES},
		{"no-nl-expand",     no_argument,       NULL, NO_NL_EXPAND},
		{"no-ok",            no_argument,       NULL, NO_OK},
		{"nook ",            no_argument,       NULL, NO_OK},
		{"no-shadow",        no_argument,       NULL, NO_SHADOW},
		{"no-tags",          no_argument,       NULL, NO_TAGS},
		{"ok-label",         required_argument, NULL, OK_LABEL},
		{"output-fd",        required_argument, NULL, OUTPUT_FD},
		{"output-separator", required_argument, NULL, OUTPUT_SEPARATOR},
		{"print-maxsize",    no_argument,       NULL, PRINT_MAXSIZE},
		{"print-size",       no_argument,       NULL, PRINT_SIZE},
		{"print-version",    no_argument,       NULL, PRINT_VERSION},
		{"quoted",           no_argument,       NULL, QUOTED},
		{"separate-output",  no_argument,       NULL, SEPARATE_OUTPUT},
		{"separator",        required_argument, NULL, OUTPUT_SEPARATOR},
		{"shadow",           no_argument,       NULL, SHADOW},
		{"single-quoted",    no_argument,       NULL, SINGLE_QUOTED},
		{"sleep",            required_argument, NULL, SLEEP},
		{"stderr",           no_argument,       NULL, STDERR},
		{"stdout",           no_argument,       NULL, STDOUT},
		{"tab-len",          required_argument, NULL, TAB_LEN},
		{"theme",            required_argument, NULL, THEME},
		{"time-format",      required_argument, NULL, TIME_FORMAT},
		{"title",            required_argument, NULL, TITLE},
		{"trim",             no_argument,       NULL, TRIM},
		{"version",          no_argument,       NULL, VERSION},
		{"yes-label",        required_argument, NULL, OK_LABEL},
		/* Dialogs */
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

	while ((input = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
		switch (input) {
		/* Common options */
		case ASCII_LINES:
			conf.ascii_lines = true;
			break;
		case BACKTITLE:
			backtitle_opt = optarg;
			break;
		case BEGIN_X:
			conf.x = (int)strtol(optarg, NULL, 10);
			if (conf.x < BSDDIALOG_CENTER) {
				printf("Error: --begin-x %d < %d", 
				    conf.x, BSDDIALOG_CENTER);
				return (255);
			}
			break;
		case BEGIN_Y:
			conf.y = (int)strtol(optarg, NULL, 10);
			if (conf.y < BSDDIALOG_CENTER) {
				printf("Error: --begin-y %d < %d",
				    conf.y, BSDDIALOG_CENTER);
				return (255);
			}
			break;
		case CANCEL_LABEL:
			conf.button.cancel_label = optarg;
			break;
		case CLEAR:
			conf.clear = true;
			break;
		case COLORS:
			conf.text.highlight = true;
			break;
		case CR_WRAP:
			cr_wrap_opt = true;
			break;
		case DATE_FORMAT:
			date_fmt_opt = optarg;
			break;
		case DEFAULT_BUTTON:
			conf.button.default_label = optarg;
			break;
		case DEFAULT_ITEM:
			item_default_opt = optarg;
			break;
		case DEFAULT_NO:
			conf.button.default_cancel = true;
			break;
		case DISABLE_ESC:
			conf.key.enable_esc = false;
			break;
		case ESC_CANCELVALUE:
			esc_cancelvalue_opt = true;
			break;
		case EXIT_LABEL:
			conf.button.ok_label = optarg;
			break;
		case EXTRA_BUTTON:
			conf.button.with_extra = true;
			break;
		case EXTRA_LABEL:
			conf.button.extra_label = optarg;
			break;
		case GENERIC_BUTTON1:
			conf.button.generic1_label = optarg;
			break;
		case GENERIC_BUTTON2:
			conf.button.generic2_label = optarg;
			break;
		case HELP:
			usage();
			return (BSDDIALOG_OK);
		case HELP_BUTTON:
			conf.button.with_help = true;
			break;
		case HELP_LABEL:
			conf.button.help_label = optarg;
			break;
		case HELP_STATUS:
			list_items_on_opt = true;
			break;
		case HELP_TAGS:
			item_tag_help_opt = true;
			break;
		case HFILE:
			conf.f1_file = optarg;
			break;
		case HLINE:
			conf.bottomtitle = optarg;
			break;
		case HMSG:
			conf.f1_message = optarg;
			break;
		case IGNORE:
			ignore_opt = true;
			break;
		case INSECURE:
			conf.form.securech = '*';
			break;
		case ITEM_DEPTH:
			item_depth_opt = true;
			break;
		case ITEM_HELP:
			item_bottomdesc_opt = true;
			break;
		case ITEM_PREFIX:
			item_prefix_opt = true;
			break;
		case MAX_INPUT:
			max_input_form_opt = (u_int)strtoul(optarg, NULL, 10);
			break;
		case NO_ITEMS:
			conf.menu.no_desc = true;
			break;
		case NO_CANCEL:
			conf.button.without_cancel = true;
			break;
		case NO_COLLAPSE:
			no_collapse_opt = true;
			break;
		case NO_LINES:
			conf.no_lines = true;
			break;
		case NO_NL_EXPAND:
			no_nl_expand_opt = true;
			break;
		case NO_OK:
			conf.button.without_ok = true;
			break;
		case NO_TAGS:
			conf.menu.no_name = true;
			break;
		case NO_SHADOW:
			conf.shadow = false;
			break;
		case OK_LABEL:
			conf.button.ok_label = optarg;
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
			print_maxsize_opt = true;
			break;
		case PRINT_SIZE:
			conf.get_height = &getH;;
			conf.get_width = &getW;
			break;
		case PRINT_VERSION:
			printf("bsddialog version %s\n", BSDDIALOG_VERSION);
			break;
		case SEPARATE_OUTPUT:
			item_output_sepnl_opt = true;
			break;
		case SHADOW:
			conf.shadow = true;
			break;
		case SINGLE_QUOTED:
			item_singlequote_opt = true;
			break;
		case SLEEP:
			conf.sleep = (u_int)strtoul(optarg, NULL, 10);
			break;
		case STDERR:
			output_fd_opt = STDERR_FILENO;
			break;
		case STDOUT:
			output_fd_opt = STDOUT_FILENO;
			break;
		case TAB_LEN:
			conf.text.tablen = (u_int)strtoul(optarg, NULL, 10);
			break;
		case THEME:
			if (strcasecmp(optarg, "bsddialog") == 0)
				theme_opt = BSDDIALOG_THEME_BSDDIALOG;
			else if (strcasecmp(optarg, "blackwhite") == 0)
				theme_opt = BSDDIALOG_THEME_BLACKWHITE;
			else if (strcasecmp(optarg, "default") == 0)
				theme_opt = BSDDIALOG_THEME_DEFAULT;
			else if (strcasecmp(optarg, "dialog") == 0)
				theme_opt = BSDDIALOG_THEME_DIALOG;
			else {
				printf("Error: unknown theme\n");
				return (255);
			}
			break;
		case TIME_FORMAT:
			time_fmt_opt = optarg;
			break;
		case TITLE:
			conf.title = optarg;
			break;
		case TRIM:
			trim_opt = true;
			break;
		case VERSION:
			printf("bsddialog %s (libbsddialog %s)\n",
			    BSDDIALOG_VERSION, LIBBSDDIALOG_VERSION);
			return (BSDDIALOG_OK);
		/* Dialogs */
		case CHECKLIST:
			dialogbuilder = checklist_builder;
			break;
		case DATEBOX:
			dialogbuilder = datebox_builder;
			break;
		case FORM:
			dialogbuilder = form_builder;
			break;
		case GAUGE:
			dialogbuilder = gauge_builder;
			break;
		case INFOBOX:
			dialogbuilder = infobox_builder;
			break;
		case INPUTBOX:
			dialogbuilder = inputbox_builder;
			break;
		case MENU:
			dialogbuilder = menu_builder;
			break;
		case MIXEDFORM:
			dialogbuilder = mixedform_builder;
			break;
		case MIXEDGAUGE:
			dialogbuilder = mixedgauge_builder;
			break;
		case MSGBOX:
			dialogbuilder = msgbox_builder;
			break;
		case PAUSE:
			dialogbuilder = pause_builder;
			break;
		case PASSWORDBOX:
			dialogbuilder = passwordbox_builder;
			break;
		case PASSWORDFORM:
			dialogbuilder = passwordform_builder;
			break;
		case RADIOLIST:
			dialogbuilder = radiolist_builder;
			break;
		case RANGEBOX:
			dialogbuilder = rangebox_builder;
			break;
		case TEXTBOX:
			dialogbuilder = textbox_builder;
			break;
		case TIMEBOX:
			dialogbuilder = timebox_builder;
			break;
		case TREEVIEW:
			dialogbuilder = treeview_builder;
			break;
		case YESNO:
			dialogbuilder = yesno_builder;
			break;
		/* Error */
		default:
			if (ignore_opt == true)
				break;
			usage();
			return (255);
		}
	}
	argc -= optind;
	argv += optind;

	if (print_maxsize_opt) {
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
		dprintf(output_fd_opt, "Screen size: (%d - %d)\n",
		    ws.ws_row, ws.ws_col);
		if (argc == 0)
			return (BSDDIALOG_OK);
	}

	if (argc < 3) {
		usage();
		return (255);
	}
	if (dialogbuilder == textbox_builder)
		text = argv[0];
	else {
		if ((text = malloc(strlen(argv[0]) + 1)) == NULL) {
			printf("Error: cannot allocate memory for text\n");
			return (255);
		}
		custom_text(cr_wrap_opt, no_collapse_opt, no_nl_expand_opt,
		    trim_opt, argv[0], text);
	}
	rows = (int)strtol(argv[1], NULL, 10);
	cols = (int)strtol(argv[2], NULL, 10);
	argc -= 3;
	argv += 3;

	/* bsddialog terminal mode */
	if (bsddialog_init() != 0) {
		printf("Error: %s\n", bsddialog_geterror());
		return (BSDDIALOG_ERROR);
	}

	signal(SIGINT, sigint_handler);

	if (theme_opt != BSDDIALOG_THEME_DEFAULT)
		bsddialog_set_default_theme(theme_opt);

	if (backtitle_opt != NULL)
		bsddialog_backtitle(&conf, backtitle_opt);

	output = BSDDIALOG_OK;
	if (dialogbuilder != NULL)
		output = dialogbuilder(conf, text, rows, cols, argc, argv,
		    errorbuilder);

	if (dialogbuilder != textbox_builder)
		free(text);

	bsddialog_end();
	/* end bsddialog terminal mode */

	if (output == BSDDIALOG_ERROR) {
		if (errorbuilder[0] != '\0')
			printf("Error: %s\n", errorbuilder);
		else
			printf("Error: %s\n", bsddialog_geterror());
		return (255);
	}

	if (conf.get_height != NULL && conf.get_width != NULL)
		dprintf(output_fd_opt, "Dialog size: (%d - %d)\n",
		    *conf.get_height, *conf.get_width);

	if (output == BSDDIALOG_ESC && esc_cancelvalue_opt)
		output = BSDDIALOG_CANCEL;

	return (output);
}

void sigint_handler(int sig)
{
	bsddialog_end();

	exit(255);
}

void
custom_text(bool cr_wrap, bool no_collapse, bool no_nl_expand, bool trim,
    char *text, char *buf)
{
	int i, j;

	i = j = 0;
	while (text[i] != '\0') {
		switch (text[i]) {
		case '\\':
			buf[j] = '\\';
			switch (text[i+1]) {
			case '\\':
				i++;
				break;
			case 'n':
				if (no_nl_expand) {
					j++;
					buf[j] = 'n';
				} else
					buf[j] = '\n';
				i++;
				break;
			case 't':
				if (no_collapse) {
					j++;
					buf[j] = 't';
				} else
					buf[j] = '\t';
				i++;
				break;
			}
			break;
		case '\n':
			buf[j] = cr_wrap ? ' ' : '\n';
			break;
		case '\t':
			buf[j] = no_collapse ? '\t' : ' ';
			break;
		default:
			buf[j] = text[i];
		}
		i++;
		j += (buf[j] == ' ' && trim && j > 0 && buf[j-1] == ' ') ?
		    0 : 1;
	}
	buf[j] = '\0';
}

/* Dialogs */
int gauge_builder(BUILDER_ARGS)
{
	int output;
	unsigned int perc;

	if (argc > 0) {
		perc = argc > 0 ? (u_int)strtoul(argv[0], NULL, 10) : 0;
		perc = perc > 100 ? 100 : perc;
	}
	else
		perc = 0;

	output = bsddialog_gauge(&conf, text, rows, cols, perc, STDIN_FILENO,
	    "XXX");

	return (output);
}

int infobox_builder(BUILDER_ARGS)
{
	int output;

	output = bsddialog_infobox(&conf, text, rows, cols);

	return (output);
}

int mixedgauge_builder(BUILDER_ARGS)
{
	int output, *minipercs;
	unsigned int i, mainperc, nminibars;
	const char **minilabels;

	if (argc < 1 || (((argc-1) % 2) != 0) ) {
		strcpy(errbuf, "bad --mixedgauge arguments\n");
		return (BSDDIALOG_ERROR);
	}

	mainperc = (u_int)strtoul(argv[0], NULL, 10);
	mainperc = mainperc > 100 ? 100 : mainperc;
	argc--;
	argv++;

	nminibars  = argc / 2;
	if ((minilabels = calloc(nminibars, sizeof(char*))) == NULL) {
		strcpy(errbuf, "Cannot allocate memory for minilabels\n");
		return BSDDIALOG_ERROR;
	}
	if ((minipercs = calloc(nminibars, sizeof(int))) == NULL) {
		strcpy(errbuf, "Cannot allocate memory for minipercs\n");
		return BSDDIALOG_ERROR;
	}

	for (i = 0; i < nminibars; i++) {
		minilabels[i] = argv[i * 2];
		minipercs[i] = (int)strtol(argv[i * 2 + 1], NULL, 10);
	}

	output = bsddialog_mixedgauge(&conf, text, rows, cols, mainperc,
	    nminibars, minilabels, minipercs);

	return (output);
}

int msgbox_builder(BUILDER_ARGS)
{
	int output;

	output = bsddialog_msgbox(&conf, text, rows, cols);

	return (output);
}

int pause_builder(BUILDER_ARGS)
{
	int output;
	unsigned int secs;

	if (argc < 1) {
		strcpy(errbuf, "missing <seconds> for --pause\n");
		return (BSDDIALOG_ERROR);
	}

	secs = (u_int)strtoul(argv[0], NULL, 10);
	output = bsddialog_pause(&conf, text, rows, cols, secs);

	return (output);
}

int rangebox_builder(BUILDER_ARGS)
{
	int output, min, max, value;

	if (argc < 2) {
		strcpy(errbuf, "usage --rangebox <text> <rows> <cols> <min> "
		    "<max> [<init>]\n");
		return (BSDDIALOG_ERROR);
	}

	min = (int)strtol(argv[0], NULL, 10);
	max = (int)strtol(argv[1], NULL, 10);

	if (argc > 2) {
		value = (int)strtol(argv[2], NULL, 10);
		value = value < min ? min : value;
		value = value > max ? max : value;
	}
	else
		value = min;

	output = bsddialog_rangebox(&conf, text, rows, cols, min, max, &value);

	dprintf(output_fd_opt, "%d", value);

	return (output);
}

int textbox_builder(BUILDER_ARGS)
{
	int output;

	output = bsddialog_textbox(&conf, text, rows, cols);

	return (output);
}

int yesno_builder(BUILDER_ARGS)
{
	int output;

	output = bsddialog_yesno(&conf, text, rows, cols);

	return (output);
}

/* DATE and TIME */
int datebox_builder(BUILDER_ARGS)
{
	int output;
	unsigned int yy, mm, dd;
	time_t cal;
	struct tm *localtm;
	char stringdate[1024];

	time(&cal);
	localtm = localtime(&cal);
	yy = localtm->tm_year + 1900;
	mm = localtm->tm_mon + 1;
	dd = localtm->tm_mday;

	if (argc == 3) {
		yy = (u_int)strtoul(argv[0], NULL, 10);
		mm = (u_int)strtoul(argv[1], NULL, 10);
		dd = (u_int)strtoul(argv[2], NULL, 10);
	}

	output = bsddialog_datebox(&conf, text, rows, cols, &yy, &mm, &dd);
	if (output != BSDDIALOG_OK)
		return (output);

	if (date_fmt_opt == NULL) {
		dprintf(output_fd_opt, "%u/%u/%u", yy, mm, dd);
	} else {
		time(&cal);
		localtm = localtime(&cal);
		localtm->tm_year = yy - 1900;
		localtm->tm_mon  = mm;
		localtm->tm_mday = dd;
		strftime(stringdate, 1024, date_fmt_opt, localtm);
		dprintf(output_fd_opt, "%s", stringdate);
	}

	return (output);
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

	if (argc == 3) {
		hh = (u_int)strtoul(argv[0], NULL, 10);
		mm = (u_int)strtoul(argv[1], NULL, 10);
		ss = (u_int)strtoul(argv[2], NULL, 10);
	}

	output = bsddialog_timebox(&conf, text, rows, cols, &hh, &mm, &ss);
	if (output != BSDDIALOG_OK)
		return (output);

	if (time_fmt_opt == NULL) {
		dprintf(output_fd_opt, "%u:%u:%u", hh, mm, ss);
	} else {
		time(&clock);
		localtm = localtime(&clock);
		localtm->tm_hour = hh;
		localtm->tm_min  = mm;
		localtm->tm_sec = ss;
		strftime(stringtime, 1024, time_fmt_opt, localtm);
		dprintf(output_fd_opt, "%s", stringtime);
	}

	return (output);
}

/* MENU */
static int
get_menu_items(char *errbuf, int argc, char **argv, bool setprefix,
    bool setdepth, bool setname, bool setdesc, bool setstatus, bool sethelp,
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
	if ((argc % sizeitem) != 0) {
		strcpy(errbuf, "bad number of arguments for this menu\n");
		return (BSDDIALOG_ERROR);
	}
	*nitems = argc / sizeitem;

	*items = calloc(*nitems, sizeof(struct bsddialog_menuitem));
	if (items == NULL) {
		strcpy(errbuf, "cannot allocate memory menu items\n");
		return (BSDDIALOG_ERROR);
	}

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

	return (BSDDIALOG_OK);
}

static void
print_menu_items(struct bsddialog_conf *conf, int output, int nitems,
    struct bsddialog_menuitem *items, int focusitem)
{
	bool sep, toquote;
	int i;
	char *sepstr, quotech;
	const char *helpvalue;

	sep = false;
	quotech = item_singlequote_opt ? '\'' : '"';
	sepstr = item_output_sep_opt != NULL ? item_output_sep_opt : " ";

	if (output == BSDDIALOG_HELP && focusitem >= 0) {
		dprintf(output_fd_opt, "HELP ");

		helpvalue = items[focusitem].name;
		if (item_bottomdesc_opt && item_tag_help_opt == false)
			helpvalue = items[focusitem].bottomdesc;

		toquote = item_always_quote_opt ||
		    strchr(helpvalue, ' ') != NULL;

		if (toquote)
			dprintf(output_fd_opt, "%c", quotech);
		dprintf(output_fd_opt, "%s", helpvalue);
		if (toquote)
			dprintf(output_fd_opt, "%c", quotech);

		sep = true;
	}

	if (output != BSDDIALOG_OK &&
	    !(output == BSDDIALOG_HELP && list_items_on_opt))
		return;

	for (i = 0; i < nitems; i++) {
		if (items[i].on == false)
			continue;

		if (sep == true) {
			dprintf(output_fd_opt, "%s", sepstr);
			if (item_output_sepnl_opt)
				dprintf(output_fd_opt, "\n");
		}
		sep = true;

		toquote = item_always_quote_opt ||
		    strchr(items[i].name, ' ') != NULL;

		if (toquote)
			dprintf(output_fd_opt, "%c", quotech);
		dprintf(output_fd_opt, "%s", items[i].name);
		if (toquote)
			dprintf(output_fd_opt, "%c", quotech);
	}
}

int checklist_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = (u_int)strtoul(argv[0], NULL, 10);

	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_opt,
	    item_depth_opt, true, true, true, item_bottomdesc_opt, &nitems,
	    &items, &focusitem);
	if (output != 0)
		return (output);

	output = bsddialog_checklist(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(&conf, output, nitems, items, focusitem);

	free(items);

	return (output);
}

int menu_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = (u_int)strtoul(argv[0], NULL, 10);

	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_opt,
	    item_depth_opt, true, true, false, item_bottomdesc_opt, &nitems,
	    &items, &focusitem);
	if (output != 0)
		return (output);

	output = bsddialog_menu(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(&conf, output, nitems, items, focusitem);

	free(items);

	return (output);
}

int radiolist_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = (u_int)strtoul(argv[0], NULL, 10);

	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_opt,
	    item_depth_opt, true, true, true, item_bottomdesc_opt, &nitems,
	    &items, &focusitem);
	if (output != 0)
		return (output);

	output = bsddialog_radiolist(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(&conf, output, nitems, items, focusitem);

	free(items);

	return (output);
}

int treeview_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = (u_int)strtoul(argv[0], NULL, 10);

	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_opt, true,
	    true, true, true, item_bottomdesc_opt, &nitems, &items, &focusitem);
	if (output != 0)
		return (output);

	conf.menu.no_name = true;
	conf.menu.align_left = true;

	output = bsddialog_radiolist(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(&conf, output, nitems, items, focusitem);

	free(items);

	return (output);
}

/* FORM */
static int
alloc_formitems(int nitems, struct bsddialog_formitem **items, char *errbuf)
{
	*items = calloc(nitems, sizeof(struct bsddialog_formitem));
	if (items == NULL) {
		strcpy(errbuf, "cannot allocate memory for form items\n");
		return (BSDDIALOG_ERROR);
	}

	return (BSDDIALOG_OK);
}

static void
print_form_items(struct bsddialog_conf *conf, int output, int nitems,
    struct bsddialog_formitem *items)
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

	sizeitem = item_bottomdesc_opt ? 9 : 8;
	if (argc < 1 || (((argc-1) % sizeitem) != 0)) {
		strcpy(errbuf, "bad number of arguments for this form\n");
		return (BSDDIALOG_ERROR);
	}

	formheight = (u_int)strtoul(argv[0], NULL, 10);
	flags = 0;

	argc--;
	argv++;

	nitems = argc / sizeitem;
	if (alloc_formitems(nitems, &items, errbuf) != BSDDIALOG_OK)
		return (BSDDIALOG_ERROR);
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

		items[i].bottomdesc = item_bottomdesc_opt ? argv[j++] : "";
	}

	output = bsddialog_form(&conf, text, rows, cols, formheight, nitems,
	    items);
	print_form_items(&conf, output, nitems, items);
	free(items);

	return (output);
}

int inputbox_builder(BUILDER_ARGS)
{
	int output;
	struct bsddialog_formitem item;

	item.label	 = "";
	item.ylabel	 = 0;
	item.xlabel	 = 0;
	item.init	 = argc > 0 ? argv[0] : "";
	item.yfield	 = 1;
	item.xfield	 = 1;
	item.fieldlen    = cols > 4 ? cols-4 : 25;
	item.maxvaluelen = max_input_form_opt > 0 ? max_input_form_opt : 2048;
	item.flags	 = 0;
	item.bottomdesc  = "";

	output = bsddialog_form(&conf, text, rows, cols, 1, 1, &item);
	print_form_items(&conf, output, 1, &item);

	return (output);
}

int mixedform_builder(BUILDER_ARGS)
{
	int output;
	unsigned int i, j, formheight, nitems, sizeitem;
	struct bsddialog_formitem *items;

	sizeitem = item_bottomdesc_opt ? 10 : 9;
	if (argc < 1 || (((argc-1) % sizeitem) != 0)) {
		strcpy(errbuf, "bad number of arguments for this form\n");
		return (BSDDIALOG_ERROR);
	}

	formheight = (u_int)strtoul(argv[0], NULL, 10);

	argc--;
	argv++;

	nitems = argc / sizeitem;
	if (alloc_formitems(nitems, &items, errbuf) != BSDDIALOG_OK)
		return (BSDDIALOG_ERROR);
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

	output = bsddialog_form(&conf, text, rows, cols, formheight, nitems,
	    items);
	print_form_items(&conf, output, nitems, items);
	free(items);

	return (output);
}

int passwordbox_builder(BUILDER_ARGS)
{
	int output;
	struct bsddialog_formitem item;

	item.label	 = "";
	item.ylabel	 = 0;
	item.xlabel	 = 0;
	item.init	 = argc > 0 ? argv[0] : "";
	item.yfield	 = 1;
	item.xfield	 = 1;
	item.fieldlen	 = cols > 4 ? cols-4 : 25;
	item.maxvaluelen = max_input_form_opt > 0 ? max_input_form_opt : 2048;
	item.flags       = BSDDIALOG_FIELDHIDDEN;
	item.bottomdesc  = "";

	output = bsddialog_form(&conf, text, rows, cols, 1, 1, &item);
	print_form_items(&conf, output, 1, &item);

	return (output);
}

int passwordform_builder(BUILDER_ARGS)
{
	int output, fieldlen, valuelen;
	unsigned int i, j, flags, formheight, nitems, sizeitem;
	struct bsddialog_formitem *items;

	sizeitem = item_bottomdesc_opt ? 9 : 8;
	if (argc < 1 || (((argc-1) % sizeitem) != 0) ) {
		strcpy(errbuf, "bad number of arguments for this form\n");
		return (BSDDIALOG_ERROR);
	}

	formheight = (u_int)strtoul(argv[0], NULL, 10);
	flags = BSDDIALOG_FIELDHIDDEN;

	argc--;
	argv++;

	nitems = argc / sizeitem;
	if (alloc_formitems(nitems, &items, errbuf) != BSDDIALOG_OK)
		return (BSDDIALOG_ERROR);
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

	output = bsddialog_form(&conf, text, rows, cols, formheight, nitems,
	    items);
	print_form_items(&conf, output, nitems, items);
	free(items);

	return (output);
}
