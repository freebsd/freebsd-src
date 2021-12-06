/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alfonso Sabato Siciliano
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <bsddialog.h>
#include <bsddialog_theme.h>

#define BSDDIALOG_VERSION "0.0.1"

enum OPTS {
	/* Common options */
	ASCII_LINES = '?' + 1,
	ASPECT_RATIO,
	BACKTITLE,
	BEGIN_X,
	BEGIN_Y,
	CANCEL_LABEL,
	CLEAR,
	COLORS,
	CR_WRAP,
	DATE_FORMAT,
	DEFAULTNO,
	DEFAULT_BUTTON,
	DEFAULT_ITEM,
	EXIT_LABEL,
	EXTRA_BUTTON,
	EXTRA_LABEL,
	HELP,
	HELP_BUTTON,
	HELP_LABEL,
	HELP_STATUS,
	HELP_TAGS,
	HFILE,	
	HLINE,
	IGNORE,
	INSECURE,
	ITEM_HELP,
	ITEM_PREFIX,
	MAX_INPUT,
	NO_CANCEL,
	NOCANCEL,
	NO_COLLAPSE,
	NO_ITEMS,
	NO_LABEL,
	NO_LINES,
	NO_NL_EXPAND,
	NO_OK,
	NOOK,
	NO_TAGS,
	NO_SHADOW,
	OK_LABEL,
	OUTPUT_FD,
	OUTPUT_SEPARATOR,
	PRINT_MAXSIZE,
	PRINT_SIZE,
	PRINT_VERSION,
	QUOTED,
	SEPARATE_OUTPUT,
	SEPARATOR,
	SHADOW,
	SINGLE_QUOTED,
	SLEEP,
	STDERR,
	STDOUT,
	THEME,
	TIME_FORMAT,
	TITLE,
	TRIM,
	VERSION,
	YES_LABEL,
	/* Widgets */
	BUILDLIST,
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
	YESNO,
};

/* libbsddialog does not support NULL string for now */
static char *nostring = "";
/* Menus flags and options */
static bool item_prefix_flag, item_bottomdesc_flag, item_output_sepnl_flag;
static bool item_singlequote_flag, list_items_on_flag, item_tag_help_flag;
static bool item_always_quote_flag;
static char *item_output_sep_flag;
/* Time and calendar options */
static char *date_fmt_flag, *time_fmt_flag;
/* Forms */
static int max_input_form_flag;
/* General flags and options */
static int output_fd_flag;

void usage(void);
/* widgets */
#define BUILDER_ARGS struct bsddialog_conf conf, char* text, int rows,         \
	int cols, int argc, char **argv, char *errbuf
int buildlist_builder(BUILDER_ARGS);
int checklist_builder(BUILDER_ARGS);
int datebox_builder(BUILDER_ARGS);
int form_builder(BUILDER_ARGS);
int gauge_builder(BUILDER_ARGS);
int infobox_builder(BUILDER_ARGS);
int inputbox_builder(BUILDER_ARGS);
int menu_builder(BUILDER_ARGS);
int mixedform_builder(BUILDER_ARGS);
int mixedgauge_builder(BUILDER_ARGS);
int msgbox_builder(BUILDER_ARGS);
int passwordbox_builder(BUILDER_ARGS);
int passwordform_builder(BUILDER_ARGS);
int pause_builder(BUILDER_ARGS);
int radiolist_builder(BUILDER_ARGS);
int rangebox_builder(BUILDER_ARGS);
int textbox_builder(BUILDER_ARGS);
int timebox_builder(BUILDER_ARGS);
int treeview_builder(BUILDER_ARGS);
int yesno_builder(BUILDER_ARGS);

void usage(void)
{

	printf("usage: bsddialog --help\n"
	       "       bsddialog --version\n"
	       "       bsddialog [--<common-opts>] --<widget> <text> "
	       "<height> <width> [--<widget-opts>]\n");
	printf("\n");
	printf("Common Options:\n");
	printf("--ascii-lines, --aspect <ratio>, --backtitle <backtitle>, "
		"--begin-x <x>, --begin-y <y>, --cancel-label <string>, "
		"-clear, --colors, --date-format <format>, "
		"--default-button <label>, --defaultno, --default-item <name>,"
		"--exit-label <label>, --extra-button, --extra-label <label>,"
		"--hfile <filename>, --help-button, --help-label <label>, "
		"--help-status, --help-tags, --hline string, --ignore, "
		"--insecure, --item-help, --max-input <size>, --no-cancel, "
		"--nocancel, --no-label <label>, --no-items, --no-lines, "
		"--no-ok, --nook, --no-shadow, --no-tags, --ok-label <label>, "
		"--output-fd <fd>, --output-separator <sep>, --print-version, "
		"--print-size, --quoted, --print-maxsize, --shadow, "
		"--single-quoted, --separator <sep>, --separate-output, "
		"--sleep <secs>, --stderr, --stdout, "
		"--theme <blackwhite|bsddialog|dialog>, "
		"--time-format <format>, --title <title>, "
		"--yes-label <string>.\n");
	printf("\n");
	printf("Widgets:\n");
	printf("--buildlist <text> <rows> <cols> <menurows> [<name> <desc> "
		    "<on|off> ...]\n"
		"--checklist <text> <rows> <cols> <menurows> [<name> <desc> "
		    "<on|off> ...]\n"
		"--datebox <text> <rows> <cols> [<yy> <mm> <dd>]\n"
		"--form <text> <rows> <cols> <formrows> [<label> <ylabel> "
		    "<xlabel> <init> <yfield> <xfield> <fieldlen> <maxvalue> "
		    "...]\n"
		"--gauge <text> <rows> <cols> [<perc\\n> [<text> ...] XXX "
		    "...] EOF\n"
		"--infobox <text> <rows> <cols>\n"
		"--inputbox <text> <rows> <cols> [init]\n"
		"--menu <text> <rows> <cols> <menurows> [<name> <desc> ...]\n"
		"--mixedform <text> <rows> <cols> <formrows> [<label> <ylabel> "
		    "<xlabe> <init> <yfield> <xfield> <fieldlen> <maxvalue> "
		    "<0|1|2> ...]\n"
		"--mixedgauge <text> <rows> <cols> <mainperc> [<label> "
		    "<01234567|-perc> ...]\n"
		"--msgbox <text> <rows> <cols>\n"
		"--passwordbox <text> <rows> <cols> [init]\n"
		"--passwordform <text> <rows> <cols> <formrows> [<label> "
		    "<ylabel> <xlabe> <init> <yfield> <xfield> <fieldlen> "
		    "<maxvalue> ...]\n"
		"--pause <text> <rows> <cols> <secs>\n"
		"--radiolist <text> <rows> <cols> <menurows> [<name> <desc> "
		    "<on|off> ...]\n"
		"--rangebox <text> <rows> <cols> <min> <max> <default>\n"
		"--textbox <file> <rows> <cols>\n"
		"--timebox <text> <rows> <cols> [<hh> <mm> <ss>]\n"
		"--treeview <text> <rows> <cols> <menurows> [<depth> <name> "
		    "<desc> <on|off> ...]\n"
		"--yesno <text> <rows> <cols>\n");
		
	
}

int main(int argc, char *argv[argc])
{
	char *text, *backtitle_flag, errorbuilder[1024];
	int input, rows, cols, output, getH, getW;
	int (*widgetbuilder)(BUILDER_ARGS) = NULL;
	bool ignore_flag, print_maxsize_flag;
	struct winsize ws;
	struct bsddialog_conf conf;
	enum bsddialog_default_theme theme_flag;

	bsddialog_initconf(&conf);

	backtitle_flag = NULL;
	theme_flag = -1;
	output_fd_flag = STDERR_FILENO;
	print_maxsize_flag = false;
	ignore_flag = false;
	errorbuilder[0] = '\0';

	item_output_sepnl_flag = item_singlequote_flag = false;
	item_prefix_flag = item_bottomdesc_flag = false;
	list_items_on_flag = item_tag_help_flag = false;
	item_always_quote_flag = false;
	item_output_sep_flag = NULL;

	date_fmt_flag = time_fmt_flag = NULL;
	
	max_input_form_flag = 0;

	/* options descriptor */
	struct option longopts[] = {
	    /* common options */
	    {"ascii-lines",     no_argument,       NULL, ASCII_LINES },
	    {"aspect",          required_argument, NULL, ASPECT_RATIO },
	    {"backtitle",       required_argument, NULL, BACKTITLE },
	    {"begin-x",         required_argument, NULL, BEGIN_X },
	    {"begin-y",         required_argument, NULL, BEGIN_Y },
	    {"cancel-label",    required_argument, NULL, CANCEL_LABEL },
	    {"clear",           no_argument,       NULL, CLEAR },
	    {"colors",          no_argument,       NULL, COLORS },
	    {"cr-wrap",         no_argument,       NULL, CR_WRAP },
	    {"date-format",     required_argument, NULL, DATE_FORMAT },
	    {"defaultno",       no_argument,       NULL, DEFAULTNO },
	    {"default-button",  required_argument, NULL, DEFAULT_BUTTON },
	    {"default-item",    required_argument, NULL, DEFAULT_ITEM },
	    {"exit-label",      required_argument, NULL, EXIT_LABEL },
	    {"extra-button",    no_argument,       NULL, EXTRA_BUTTON },
	    {"extra-label",     required_argument, NULL, EXTRA_LABEL },
	    {"help",            no_argument,       NULL, HELP },
	    {"help-button",     no_argument,       NULL, HELP_BUTTON },
	    {"help-label",      required_argument, NULL, HELP_LABEL },
	    {"help-status",     no_argument,       NULL, HELP_STATUS },
	    {"help-tags",       no_argument,       NULL, HELP_TAGS },
	    {"hfile",           required_argument, NULL, HFILE },
	    {"hline",           required_argument, NULL, HLINE },
	    {"ignore",          no_argument,       NULL, IGNORE },
	    {"insecure",        no_argument,       NULL, INSECURE },
	    {"item-help",       no_argument,       NULL, ITEM_HELP },
	    {"max-input",       required_argument, NULL, MAX_INPUT },
	    {"no-cancel",       no_argument,       NULL, NO_CANCEL },
	    {"nocancel",        no_argument,       NULL, NOCANCEL },
	    {"no-collapse",     no_argument,       NULL, NO_COLLAPSE },
	    {"no-items",        no_argument,       NULL, NO_ITEMS },
	    {"no-label",        required_argument, NULL, NO_LABEL },
	    {"no-lines",        no_argument,       NULL, NO_LINES },
	    {"no-nl-expand",    no_argument,       NULL, NO_NL_EXPAND },
	    {"no-ok",           no_argument,       NULL, NO_OK },
	    {"nook ",           no_argument,       NULL, NOOK },
	    {"no-tags",         no_argument,       NULL, NO_TAGS },
	    {"no-shadow",       no_argument,       NULL, NO_SHADOW },
	    {"ok-label",        required_argument, NULL, OK_LABEL },
	    {"output-fd",       required_argument, NULL, OUTPUT_FD },
	    {"separator",       required_argument, NULL, SEPARATOR },
	    {"output-separator",required_argument, NULL, OUTPUT_SEPARATOR },
	    {"print-maxsize",   no_argument,       NULL, PRINT_MAXSIZE },
	    {"print-size",      no_argument,       NULL, PRINT_SIZE },
	    {"print-version",   no_argument,       NULL, PRINT_VERSION },
	    {"quoted",          no_argument,       NULL, QUOTED },
	    {"separate-output", no_argument,       NULL, SEPARATE_OUTPUT },
	    {"shadow",          no_argument,       NULL, SHADOW },
	    {"single-quoted",   no_argument,       NULL, SINGLE_QUOTED },
	    {"sleep",           required_argument, NULL, SLEEP },
	    {"stderr",          no_argument,       NULL, STDERR },
	    {"stdout",          no_argument,       NULL, STDOUT },
	    {"theme_flag",      required_argument, NULL, THEME },
	    {"time-format",     required_argument, NULL, TIME_FORMAT },
	    {"title",           required_argument, NULL, TITLE },
	    {"trim",            no_argument,       NULL, TRIM },
	    {"version",         no_argument,       NULL, VERSION },
	    {"yes-label",       required_argument, NULL, YES_LABEL },
	    /* Widgets */
	    {"buildlist",    no_argument, NULL, BUILDLIST },
	    {"checklist",    no_argument, NULL, CHECKLIST },
	    {"datebox",      no_argument, NULL, DATEBOX },
	    {"form",         no_argument, NULL, FORM },
	    {"gauge",        no_argument, NULL, GAUGE },
	    {"infobox",      no_argument, NULL, INFOBOX },
	    {"inputbox",     no_argument, NULL, INPUTBOX },
	    {"menu",         no_argument, NULL, MENU },
	    {"mixedform",    no_argument, NULL, MIXEDFORM },
	    {"mixedgauge",   no_argument, NULL, MIXEDGAUGE },
	    {"msgbox",       no_argument, NULL, MSGBOX },
	    {"passwordbox",  no_argument, NULL, PASSWORDBOX },
	    {"passwordform", no_argument, NULL, PASSWORDFORM },
	    {"pause",        no_argument, NULL, PAUSE },
	    {"radiolist",    no_argument, NULL, RADIOLIST },
	    {"rangebox",     no_argument, NULL, RANGEBOX },
	    {"textbox",      no_argument, NULL, TEXTBOX },
	    {"timebox",      no_argument, NULL, TIMEBOX },
	    {"treeview",     no_argument, NULL, TREEVIEW },
	    {"yesno",        no_argument, NULL, YESNO },
	    /* END */
	    { NULL, 0, NULL, 0 }
	};

	while ((input = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
		switch (input) {
		/* Common options */
		case ASCII_LINES:
			conf.ascii_lines = true;
			break;
		case ASPECT_RATIO:
			conf.aspect_ratio = atoi(optarg);
			if (conf.aspect_ratio < 1) {
				printf("Error: aspect cannot be < 1");
				return (BSDDIALOG_ERROR);
			}
			break;
		case BACKTITLE:
			backtitle_flag = optarg;
			break;
		case BEGIN_X:
			conf.x = atoi(optarg);
			if (conf.x < BSDDIALOG_CENTER) {
				printf("Error: --begin-x %d, cannot be < %d",
				    conf.x, BSDDIALOG_CENTER);
				return (BSDDIALOG_ERROR);
			}
			break;
		case BEGIN_Y:
			conf.y = atoi(optarg);
			if (conf.y < BSDDIALOG_CENTER) {
				printf("Error: --begin-y %d, cannot be < %d",
				    conf.y, BSDDIALOG_CENTER);
				return (BSDDIALOG_ERROR);
			}
			break;
		case CANCEL_LABEL:
			conf.button.cancel_label = optarg;
			break;
		case CLEAR:
			conf.clear = true;
			break;
		case COLORS:
			conf.text.colors = true;
			break;
		case CR_WRAP:
			conf.text.cr_wrap = true;
			break;
		case DATE_FORMAT:
			date_fmt_flag = optarg;
			break;
		case DEFAULT_BUTTON:
			conf.button.default_label = optarg;
			break;
		case DEFAULT_ITEM:
			conf.menu.default_item = optarg;
			break;
		case DEFAULTNO:
			conf.button.defaultno = true;
			break;
		case EXIT_LABEL:
			conf.button.exit_label = optarg;
			break;
		case EXTRA_BUTTON:
			conf.button.extra_button = true;
			break;
		case EXTRA_LABEL:
			conf.button.extra_label = optarg;
			break;
		case HELP:
			usage();
			return (BSDDIALOG_YESOK);
		case HELP_BUTTON:
			conf.button.help_button = true;
			break;
		case HELP_LABEL:
			conf.button.help_label = optarg;
			break;
		case HELP_STATUS:
			list_items_on_flag = true;
			break;
		case HELP_TAGS:
			item_tag_help_flag = true;
			break;
		case HFILE:
			conf.hfile = optarg;
			break;
		case HLINE:
			conf.hline = optarg;
			break;
		case IGNORE:
			ignore_flag = true;
			break;
		case INSECURE:
			conf.form.securech = '*';
			break;
		case ITEM_HELP:
			item_bottomdesc_flag = true;
			break;
		case MAX_INPUT:
			max_input_form_flag = atoi(optarg);
			break;
		case NO_ITEMS:
			conf.menu.no_desc = true;
			break;
		case ITEM_PREFIX:
			item_prefix_flag = true;
			break;
		case NOCANCEL:
		case NO_CANCEL:
			conf.button.no_cancel = true;
			break;
		case NO_COLLAPSE:
			conf.text.no_collapse = true;
			break;
		case NO_LABEL:
			conf.button.no_label = optarg;
			break;
		case NO_LINES:
			conf.no_lines = true;
			break;
		case NO_NL_EXPAND:
			conf.text.no_nl_expand = true;
			break;
		case NOOK:
		case NO_OK:
			conf.button.no_ok = true;
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
			output_fd_flag = atoi(optarg);
			break;
		case SEPARATOR:
		case OUTPUT_SEPARATOR:
			item_output_sep_flag = optarg;
			break;
		case QUOTED:
			item_always_quote_flag = true;
			break;
		case PRINT_MAXSIZE:
			print_maxsize_flag = true;
			break;
		case PRINT_SIZE:
			conf.get_height = &getH;;
			conf.get_width = &getW;
			break;
		case PRINT_VERSION:
			printf("bsddialog version %s\n", BSDDIALOG_VERSION);
			break;
		case SEPARATE_OUTPUT:
			item_output_sepnl_flag = true;
			break;
		case SHADOW:
			conf.shadow = true;
			break;
		case SINGLE_QUOTED:
			item_singlequote_flag = true;
			break;
		case SLEEP:
			conf.sleep = atoi(optarg);
			break;
		case STDERR:
			output_fd_flag = STDERR_FILENO;
			break;
		case STDOUT:
			output_fd_flag = STDOUT_FILENO;
			break;
		case THEME:
			if (strcmp(optarg, "bsddialog") == 0)
				theme_flag = BSDDIALOG_THEME_BSDDIALOG;
			else if (strcmp(optarg, "blackwhite") == 0)
				theme_flag = BSDDIALOG_THEME_BLACKWHITE;
			else if (strcmp(optarg, "dialog") == 0)
				theme_flag = BSDDIALOG_THEME_DIALOG;
			else {
				printf("Unknown theme, possible values: ");
				printf("blackwhite, bsddialog, dialog");
				return (BSDDIALOG_ERROR);
			}
			break;
		case TIME_FORMAT:
			time_fmt_flag = optarg;
			break;
		case TITLE:
			conf.title = optarg;
			break;
		case TRIM:
			conf.text.trim = true;
			break;
		case VERSION:
			printf("bsddialog %s (libbsddialog %s).\n",
			    BSDDIALOG_VERSION, LIBBSDDIALOG_VERSION);
			return (BSDDIALOG_YESOK);
		case YES_LABEL:
			conf.button.yes_label = optarg;
			break;
		/* Widgets */
		case BUILDLIST:
			widgetbuilder = buildlist_builder;
			break;
		case CHECKLIST:
			widgetbuilder = checklist_builder;
			break;
		case DATEBOX:
			widgetbuilder = datebox_builder;
			break;
		case FORM:
			widgetbuilder = form_builder;
			break;
		case GAUGE:
			widgetbuilder = gauge_builder;
			break;
		case INFOBOX:
			widgetbuilder = infobox_builder;
			break;
		case INPUTBOX:
			widgetbuilder = inputbox_builder;
			break;
		case MENU:
			widgetbuilder = menu_builder;
			break;
		case MIXEDFORM:
			widgetbuilder = mixedform_builder;
			break;
		case MIXEDGAUGE:
			widgetbuilder = mixedgauge_builder;
			break;
		case MSGBOX:
			widgetbuilder = msgbox_builder;
			break;
		case PAUSE:
			widgetbuilder = pause_builder;
			break;
		case PASSWORDBOX:
			widgetbuilder = passwordbox_builder;
			break;
		case PASSWORDFORM:
			widgetbuilder = passwordform_builder;
			break;
		case RADIOLIST:
			widgetbuilder = radiolist_builder;
			break;
		case RANGEBOX:
			widgetbuilder = rangebox_builder;
			break;
		case TEXTBOX:
			widgetbuilder = textbox_builder;
			break;
		case TIMEBOX:
			widgetbuilder = timebox_builder;
			break;
		case TREEVIEW:
			widgetbuilder = treeview_builder;
			break;
		case YESNO:
			widgetbuilder = yesno_builder;
			break;
		/* Error */
		default:
			if (ignore_flag == true) 
				break;
			usage();
			return (BSDDIALOG_ERROR);
		}
	}
	argc -= optind;
	argv += optind;

	if (print_maxsize_flag) {
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
		dprintf(output_fd_flag, "MaxSize: %d, %d\n", ws.ws_row, ws.ws_col);
		if (argc == 0)
			return (BSDDIALOG_YESOK);
	}

	if (argc < 3) {
		usage();
		return (BSDDIALOG_ERROR);
	}
	text = argv[0];
	rows = atoi(argv[1]);
	cols = atoi(argv[2]);
	argc -= 3;
	argv += 3;

	/* bsddialog terminal mode */
	if(bsddialog_init() != 0) {
		printf("Error: %s\n", bsddialog_geterror());
		return (BSDDIALOG_ERROR);
	}

	if (theme_flag >= 0)
		bsddialog_set_default_theme(theme_flag);

	if (backtitle_flag != NULL)
		bsddialog_backtitle(&conf, backtitle_flag);

	output = BSDDIALOG_YESOK;
	if (widgetbuilder != NULL)
		output = widgetbuilder(conf, text, rows, cols, argc, argv,
		    errorbuilder);

	bsddialog_end();
	/* end bsddialog terminal mode */

	/* debug & devel */
	/*printf("[Debug] Exit status: %d ", output);
	switch (output) {
	case BSDDIALOG_ERROR: 	  printf("ERROR");    break;
	case BSDDIALOG_YESOK: 	  printf("YESOK");    break;
	case BSDDIALOG_NOCANCEL:  printf("NOCANCEL"); break;
	case BSDDIALOG_HELP: 	  printf("HELP");     break;
	case BSDDIALOG_EXTRA:     printf("EXTRA");    break;
	case BSDDIALOG_ITEM_HELP: printf("ITEM_HELP");break;
	case BSDDIALOG_ESC: 	  printf("ESC");      break;
	default: 		  printf("Unknow status! Bug!"); break;
	}
	printf("\n");*/

	if (output == BSDDIALOG_ERROR) {
		if (errorbuilder[0] != '\0')
			printf("Error: %s\n", errorbuilder);
		else
			printf("Error: %s\n", bsddialog_geterror());
	}

	if (conf.get_height != NULL && conf.get_width != NULL)
		dprintf(output_fd_flag, "Widget size: (%d - %d)\n",
		    *conf.get_height, *conf.get_width);

	return (output);
}

/* Widgets */

int gauge_builder(BUILDER_ARGS)
{
	int output, perc;

	if (argc > 0) {
		perc = argc > 0 ? atoi (argv[0]) : 0;
		perc = perc < 0 ? 0 : perc;
		perc = perc > 100 ? 100 : perc;
	}
	else
		perc = 0;

	output = bsddialog_gauge(&conf, text, rows, cols, perc);

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
	int output, perc;

	if (argc < 1 || (((argc-1) % 2) != 0) ) {
		strcpy(errbuf, "bad --mixedgauge arguments\n");
		return (BSDDIALOG_ERROR);
	}

	perc = atoi(argv[0]);
	perc = perc < 0 ? 0 : perc;
	perc = perc > 100 ? 100 : perc;

	output = bsddialog_mixedgauge(&conf, text, rows, cols, perc,
	    argc-1, argv + 1);

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
	int output, sec;

	if (argc < 1) {
		strcpy(errbuf, "not <seconds> argument for --pause\n");
		return (BSDDIALOG_ERROR);
	}

	sec = atoi(argv[0]);
	output = bsddialog_pause(&conf, text, rows, cols, sec);

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

	min = atoi(argv[0]);
	max = atoi(argv[1]);
	
	if (argc > 2) {
		value = atoi(argv[2]);
		value = value < min ? min : value;
		value = value > max ? max : value;
	}
	else
		value = min;

	output = bsddialog_rangebox(&conf, text, rows, cols, min, max, &value);

	dprintf(output_fd_flag, "%d", value);

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

	/* --calendar text h w [year month day] */
	if (argc == 3) {
		yy = atoi(argv[0]);
		mm = atoi(argv[1]);
		dd = atoi(argv[2]);
	}

	output = bsddialog_datebox(&conf, text, rows, cols, &yy, &mm, &dd);
	if (output != BSDDIALOG_YESOK)
		return (output);

	if (date_fmt_flag == NULL) {
		dprintf(output_fd_flag, "%u/%u/%u", yy, mm, dd);
	}
	else {
		time(&cal);
		localtm = localtime(&cal);
		localtm->tm_year = yy - 1900;
		localtm->tm_mon  = mm;
		localtm->tm_mday = dd;
		strftime(stringdate, 1024, date_fmt_flag, localtm);
		dprintf(output_fd_flag, "%s", stringdate);
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

	/* --timebox text h w [hour minute second] */
	if (argc == 3) {
		hh = atoi(argv[0]);
		mm = atoi(argv[1]);
		ss = atoi(argv[2]);
	}

	output = bsddialog_timebox(&conf, text, rows, cols, &hh, &mm, &ss);
	if (output != BSDDIALOG_YESOK)
		return (output);

	if (time_fmt_flag == NULL) {
		dprintf(output_fd_flag, "%u:%u:%u", hh, mm, ss);
	}
	else {
		time(&clock);
		localtm = localtime(&clock);
		localtm->tm_hour = hh;
		localtm->tm_min  = mm;
		localtm->tm_sec = ss;
		strftime(stringtime, 1024, time_fmt_flag, localtm);
		dprintf(output_fd_flag, "%s", stringtime);
	}

	return (output);
}

/* MENU */
static int
get_menu_items(char *errbuf, int argc, char **argv, bool setprefix,
    bool setdepth, bool setname, bool setdesc, bool setstatus, bool sethelp,
    int *nitems, struct bsddialog_menuitem *items)
{
	int i, j, sizeitem;

	sizeitem = 0;
	if (setprefix) sizeitem++;
	if (setdepth)  sizeitem++;
	if (setname)   sizeitem++;
	if (setdesc)   sizeitem++;
	if (setstatus) sizeitem++;
	if (sethelp)   sizeitem++;
	if ((argc % sizeitem) != 0) {
		strcpy(errbuf, "bad number of arguments for this menu\n");
		return (BSDDIALOG_ERROR);
	}

	*nitems = argc / sizeitem;
	j = 0;
	for (i=0; i<*nitems; i++) {
		items[i].prefix = setprefix ? argv[j++] : nostring;
		items[i].depth = setdepth ? atoi(argv[j++]) : 0;
		items[i].name = setname ? argv[j++] : nostring;
		items[i].desc = setdesc ? argv[j++] : nostring;
		if (setstatus)
			items[i].on = strcmp(argv[j++], "on") == 0 ? true : false;
		else
			items[i].on = false;
		items[i].bottomdesc = sethelp ? argv[j++] : nostring;
	}

	return (BSDDIALOG_YESOK);
}

static void
print_menu_items(struct bsddialog_conf *conf, int output, int nitems,
    struct bsddialog_menuitem *items, int focusitem)
{
	int i;
	bool sep, toquote;
	char *sepstr, quotech, *helpvalue;

	sep = false;
	quotech = item_singlequote_flag ? '\'' : '"';
	sepstr = item_output_sep_flag != NULL ? item_output_sep_flag : " ";

	if (output == BSDDIALOG_HELP && focusitem >= 0) {
		dprintf(output_fd_flag, "HELP ");
		
		helpvalue = items[focusitem].name;
		if (item_bottomdesc_flag && item_tag_help_flag == false)
			helpvalue = items[focusitem].bottomdesc;

		toquote = item_always_quote_flag || strchr(helpvalue, ' ') != NULL;

		if (toquote)
			dprintf(output_fd_flag, "%c", quotech);
		dprintf(output_fd_flag, "%s", helpvalue);
		if (toquote)
			dprintf(output_fd_flag, "%c", quotech);
		
		if (list_items_on_flag == false)
			return;
			
		sep = true;
	}

	if (output != BSDDIALOG_YESOK && list_items_on_flag == false)
		return;

	for (i = 0; i < nitems; i++) {
		if (items[i].on == false)
			continue;

		if (sep == true) {
			dprintf(output_fd_flag, "%s", sepstr);
			if (item_output_sepnl_flag)
				dprintf(output_fd_flag, "\n");
		}
		sep = true;

		toquote = item_always_quote_flag || strchr(items[i].name, ' ') != NULL;

		if (toquote)
			dprintf(output_fd_flag, "%c", quotech);
		dprintf(output_fd_flag, "%s", items[i].name);
		if (toquote)
			dprintf(output_fd_flag, "%c", quotech);
	}
}

int buildlist_builder(BUILDER_ARGS)
{
	int output, menurows, nitems, focusitem;
	struct bsddialog_menuitem items[1024];

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = atoi(argv[0]);
   
	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_flag, false,
	    true, true, true, item_bottomdesc_flag, &nitems, items);
	if (output != 0)
		return (output);

	output = bsddialog_buildlist(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);
	if (output == BSDDIALOG_ERROR)
		return (BSDDIALOG_ERROR);

	print_menu_items(&conf, output, nitems, items, focusitem);

	return (output);
}

int checklist_builder(BUILDER_ARGS)
{
	int output, menurows, nitems, focusitem;
	struct bsddialog_menuitem items[1024];

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = atoi(argv[0]);

	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_flag, false,
	    true, true, true, item_bottomdesc_flag, &nitems, items);
	if (output != 0)
		return (output);

	output = bsddialog_checklist(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(&conf, output, nitems, items, focusitem);

	return (output);
}

int menu_builder(BUILDER_ARGS)
{
	int output, menurows, nitems, focusitem;
	struct bsddialog_menuitem items[1024];

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = atoi(argv[0]);

	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_flag, false,
	    true, true, false, item_bottomdesc_flag, &nitems, items);
	if (output != 0)
		return (output);

	output = bsddialog_menu(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(&conf, output, nitems, items, focusitem);

	return (output);
}

int radiolist_builder(BUILDER_ARGS)
{
	int output, menurows, nitems, focusitem;
	struct bsddialog_menuitem items[1024];

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = atoi(argv[0]);

	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_flag, false,
	    true, true, true, item_bottomdesc_flag, &nitems, items);
	if (output != 0)
		return (output);

	output = bsddialog_radiolist(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(&conf, output, nitems, items, focusitem);

	return (output);
}

int treeview_builder(BUILDER_ARGS)
{
	int output, menurows, nitems, focusitem;
	struct bsddialog_menuitem items[1024];

	if (argc < 1) {
		strcpy(errbuf, "<menurows> not provided");
		return (BSDDIALOG_ERROR);
	}

	menurows = atoi(argv[0]);

	output = get_menu_items(errbuf, argc-1, argv+1, item_prefix_flag, true,
	    true, true, true, item_bottomdesc_flag, &nitems, items);
	if (output != 0)
		return (output);

	conf.menu.no_name = true;
	conf.menu.align_left = true;

	output = bsddialog_radiolist(&conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);
	
	print_menu_items(&conf, output, nitems, items, focusitem);

	return (output);
}

/* FORM */
static void
print_form_items(struct bsddialog_conf *conf, int output, int nitems,
    struct bsddialog_formitem *items)
{
	int i;

	if (output == BSDDIALOG_ERROR)
		return;

	for (i=0; i < nitems; i++) {
		dprintf(output_fd_flag, "%s\n", items[i].value);
		free(items[i].value);
	}
}

int form_builder(BUILDER_ARGS)
{
	int i, output, formheight, nitems, fieldlen, valuelen;
	struct bsddialog_formitem items[1024];
	unsigned int flags = 0;

	if (argc < 1 || (((argc-1) % 8) != 0) ) {
		strcpy(errbuf, "bad number of arguments for this form\n");
		return (BSDDIALOG_ERROR);
	}

	formheight = atoi(argv[0]);

	argc--;
	argv++;

	nitems = argc / 8;
	for (i=0; i<nitems; i++) {
		items[i].label	= argv[8*i];
		items[i].ylabel = atoi(argv[8*i+1]);
		items[i].xlabel = atoi(argv[8*i+2]);
		items[i].init	= argv[8*i+3];
		items[i].yfield	= atoi(argv[8*i+4]);
		items[i].xfield	= atoi(argv[8*i+5]);

		fieldlen = atoi(argv[8*i+6]);
		items[i].fieldlen = abs(fieldlen);

		valuelen = atoi(argv[8*i+7]);
		items[i].maxvaluelen = valuelen == 0 ? abs(fieldlen) : valuelen;

		flags |= (fieldlen < 0 ? BSDDIALOG_FIELDREADONLY : 0);
		items[i].flags = flags;
	}

	output = bsddialog_form(&conf, text, rows, cols, formheight, nitems,
	    items);
	print_form_items(&conf, output, nitems, items);

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
	item.maxvaluelen = max_input_form_flag > 0 ? max_input_form_flag : 2048;
	item.flags	 = 0;

	output = bsddialog_form(&conf, text, rows, cols, 1, 1, &item);
	print_form_items(&conf, output, 1, &item);

	return (output);
}

int mixedform_builder(BUILDER_ARGS)
{
	int i, output, formheight, nitems;
	struct bsddialog_formitem items[1024];

	if (argc < 1 || (((argc-1) % 9) != 0) ) {
		strcpy(errbuf, "bad number of arguments for this form\n");
		return (BSDDIALOG_ERROR);
	}

	formheight = atoi(argv[0]);
	
	argc--;
	argv++;

	nitems = argc / 9;
	for (i=0; i<nitems; i++) {
		items[i].label	     = argv[9*i];
		items[i].ylabel      = atoi(argv[9*i+1]);
		items[i].xlabel      = atoi(argv[9*i+2]);
		items[i].init	     = argv[9*i+3];
		items[i].yfield	     = atoi(argv[9*i+4]);
		items[i].xfield	     = atoi(argv[9*i+5]);
		items[i].fieldlen    = atoi(argv[9*i+6]);
		items[i].maxvaluelen = atoi(argv[9*i+7]);
		items[i].flags       = atoi(argv[9*i+8]);
	}

	output = bsddialog_form(&conf, text, rows, cols, formheight, nitems,
	    items);
	print_form_items(&conf, output, nitems, items);

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
	item.maxvaluelen = max_input_form_flag > 0 ? max_input_form_flag : 2048;
	item.flags       = BSDDIALOG_FIELDHIDDEN;

	output = bsddialog_form(&conf, text, rows, cols, 1, 1, &item);
	print_form_items(&conf, output, 1, &item);

	return (output);
}

int passwordform_builder(BUILDER_ARGS)
{
	int i, output, formheight, nitems, fieldlen, valuelen;
	struct bsddialog_formitem items[1024];
	unsigned int flags = BSDDIALOG_FIELDHIDDEN;

	if (argc < 1 || (((argc-1) % 8) != 0) ) {
		strcpy(errbuf, "bad number of arguments for this form\n");
		return (BSDDIALOG_ERROR);
	}

	formheight = atoi(argv[0]);
	
	argc--;
	argv++;

	nitems = argc / 8;
	for (i=0; i<nitems; i++) {
		items[i].label	= argv[8*i];
		items[i].ylabel = atoi(argv[8*i+1]);
		items[i].xlabel = atoi(argv[8*i+2]);
		items[i].init	= argv[8*i+3];
		items[i].yfield	= atoi(argv[8*i+4]);
		items[i].xfield	= atoi(argv[8*i+5]);

		fieldlen = atoi(argv[8*i+6]);
		items[i].fieldlen = abs(fieldlen);

		valuelen = atoi(argv[8*i+7]);
		items[i].maxvaluelen = valuelen == 0 ? abs(fieldlen) : valuelen;

		flags |= (fieldlen < 0 ? BSDDIALOG_FIELDREADONLY : 0);
		items[i].flags = flags;
	}

	output = bsddialog_form(&conf, text, rows, cols, formheight,
	    nitems, items);
	print_form_items(&conf, output, nitems, items);

	return (output);
}
