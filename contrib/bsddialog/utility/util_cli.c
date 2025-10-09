/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2025 Alfonso Sabato Siciliano
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bsddialog.h>
#include <bsddialog_theme.h>

#include "util.h"

enum OPTS {
	/* Options */
	ALTERNATE_SCREEN = '?' + 1,
	AND_DIALOG,
	ASCII_LINES,
	BACKTITLE,
	BEGIN_X,
	BEGIN_Y,
	BIKESHED,
	CANCEL_EXIT_CODE,
	CANCEL_LABEL,
	CLEAR_DIALOG,
	CLEAR_SCREEN,
	COLUMNS_PER_ROW,
	CR_WRAP,
	DATEBOX_FORMAT,
	DATE_FORMAT,
	DEFAULT_BUTTON,
	DEFAULT_ITEM,
	DEFAULT_NO,
	DISABLE_ESC,
	ERROR_EXIT_CODE,
	ESC_EXIT_CODE,
	EXIT_LABEL,
	EXTRA_BUTTON,
	EXTRA_EXIT_CODE,
	EXTRA_LABEL,
	HELP_BUTTON,
	HELP_EXIT_CODE,
	HELP_LABEL,
	HELP_PRINT_ITEMS,
	HELP_PRINT_NAME,
	HFILE,
	HLINE,
	HMSG,
	IGNORE,
	INSECURE,
	ITEM_BOTTOM_DESC,
	ITEM_DEPTH,
	ITEM_PREFIX,
	LEFT1_BUTTON,
	LEFT1_EXIT_CODE,
	LEFT2_BUTTON,
	LEFT2_EXIT_CODE,
	LEFT3_BUTTON,
	LEFT3_EXIT_CODE,
	LOAD_THEME,
	MAX_INPUT_FORM,
	NO_CANCEL,
	NO_DESCRIPTIONS,
	NO_LINES,
	NO_NAMES,
	NO_OK,
	NO_SHADOW,
	NORMAL_SCREEN,
	OK_EXIT_CODE,
	OK_LABEL,
	OUTPUT_FD,
	OUTPUT_SEPARATOR,
	PRINT_MAXSIZE,
	PRINT_SIZE,
	PRINT_VERSION,
	QUOTED,
	RIGHT1_BUTTON,
	RIGHT1_EXIT_CODE,
	RIGHT2_BUTTON,
	RIGHT2_EXIT_CODE,
	RIGHT3_BUTTON,
	RIGHT3_EXIT_CODE,
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
	TEXT_ESCAPE,
	TEXT_UNCHANGED,
	THEME,
	TIMEOUT_EXIT_CODE,
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
	{"cancel-exit-code",  required_argument, NULL, CANCEL_EXIT_CODE},
	{"cancel-label",      required_argument, NULL, CANCEL_LABEL},
	{"clear",             no_argument,       NULL, CLEAR_SCREEN},
	{"clear-dialog",      no_argument,       NULL, CLEAR_DIALOG},
	{"clear-screen",      no_argument,       NULL, CLEAR_SCREEN},
	{"colors",            no_argument,       NULL, TEXT_ESCAPE},
	{"columns-per-row",   required_argument, NULL, COLUMNS_PER_ROW},
	{"cr-wrap",           no_argument,       NULL, CR_WRAP},
	{"datebox-format",    required_argument, NULL, DATEBOX_FORMAT},
	{"date-format",       required_argument, NULL, DATE_FORMAT},
	{"defaultno",         no_argument,       NULL, DEFAULT_NO},
	{"default-button",    required_argument, NULL, DEFAULT_BUTTON},
	{"default-item",      required_argument, NULL, DEFAULT_ITEM},
	{"default-no",        no_argument,       NULL, DEFAULT_NO},
	{"disable-esc",       no_argument,       NULL, DISABLE_ESC},
	{"error-exit-code",   required_argument, NULL, ERROR_EXIT_CODE},
	{"esc-exit-code",     required_argument, NULL, ESC_EXIT_CODE},
	{"exit-label",        required_argument, NULL, EXIT_LABEL},
	{"extra-button",      no_argument,       NULL, EXTRA_BUTTON},
	{"extra-exit-code",   required_argument, NULL, EXTRA_EXIT_CODE},
	{"extra-label",       required_argument, NULL, EXTRA_LABEL},
	{"help-button",       no_argument,       NULL, HELP_BUTTON},
	{"help-exit-code",    required_argument, NULL, HELP_EXIT_CODE},
	{"help-label",        required_argument, NULL, HELP_LABEL},
	{"help-print-items",  no_argument,       NULL, HELP_PRINT_ITEMS},
	{"help-print-name",   no_argument,       NULL, HELP_PRINT_NAME},
	{"help-status",       no_argument,       NULL, HELP_PRINT_ITEMS},
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
	{"left1-button",      required_argument, NULL, LEFT1_BUTTON},
	{"left1-exit-code",   required_argument, NULL, LEFT1_EXIT_CODE},
	{"left2-button",      required_argument, NULL, LEFT2_BUTTON},
	{"left2-exit-code",   required_argument, NULL, LEFT2_EXIT_CODE},
	{"left3-button",      required_argument, NULL, LEFT3_BUTTON},
	{"left3-exit-code",   required_argument, NULL, LEFT3_EXIT_CODE},
	{"load-theme",        required_argument, NULL, LOAD_THEME},
	{"max-input",         required_argument, NULL, MAX_INPUT_FORM},
	{"no-cancel",         no_argument,       NULL, NO_CANCEL},
	{"nocancel",          no_argument,       NULL, NO_CANCEL},
	{"no-descriptions",   no_argument,       NULL, NO_DESCRIPTIONS},
	{"no-items",          no_argument,       NULL, NO_DESCRIPTIONS},
	{"no-label",          required_argument, NULL, CANCEL_LABEL},
	{"no-lines",          no_argument,       NULL, NO_LINES},
	{"no-names",          no_argument,       NULL, NO_NAMES},
	{"no-ok",             no_argument,       NULL, NO_OK},
	{"nook",              no_argument,       NULL, NO_OK},
	{"no-shadow",         no_argument,       NULL, NO_SHADOW},
	{"no-tags",           no_argument,       NULL, NO_NAMES},
	{"normal-screen",     no_argument,       NULL, NORMAL_SCREEN},
	{"ok-exit-code",      required_argument, NULL, OK_EXIT_CODE},
	{"ok-label",          required_argument, NULL, OK_LABEL},
	{"output-fd",         required_argument, NULL, OUTPUT_FD},
	{"output-separator",  required_argument, NULL, OUTPUT_SEPARATOR},
	{"print-maxsize",     no_argument,       NULL, PRINT_MAXSIZE},
	{"print-size",        no_argument,       NULL, PRINT_SIZE},
	{"print-version",     no_argument,       NULL, PRINT_VERSION},
	{"quoted",            no_argument,       NULL, QUOTED},
	{"right1-button",     required_argument, NULL, RIGHT1_BUTTON},
	{"right1-exit-code",  required_argument, NULL, RIGHT1_EXIT_CODE},
	{"right2-button",     required_argument, NULL, RIGHT2_BUTTON},
	{"right2-exit-code",  required_argument, NULL, RIGHT2_EXIT_CODE},
	{"right3-button",     required_argument, NULL, RIGHT3_BUTTON},
	{"right3-exit-code",  required_argument, NULL, RIGHT3_EXIT_CODE},
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
	{"text-escape",       no_argument,       NULL, TEXT_ESCAPE},
	{"text-unchanged",    no_argument,       NULL, TEXT_UNCHANGED},
	{"theme",             required_argument, NULL, THEME},
	{"timeout-exit-code", required_argument, NULL, TIMEOUT_EXIT_CODE},
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

void usage(void)
{
	printf("usage: bsddialog --help | --version\n");
	printf("       bsddialog [--<opt>] --<dialog> <text> <rows> <cols> "
	    "[<arg>] [--<opt>]\n");
	printf("       bsddialog ... --<dialog1> ... [--and-dialog --<dialog2> "
	    "...] ...\n");
	printf("\n");

	printf("Options:\n");
	printf(" --alternate-screen, --ascii-lines, --backtitle <backtitle>,"
	    " --begin-x <x>,\n --begin-y <y>, --bikeshed,"
	    " --cancel-exit-code <retval>, --cancel-label <label>,\n"
	    " --clear-dialog, --clear-screen, --columns-per-row <columns>,"
	    " --cr-wrap,\n --datebox-format d/m/y|m/d/y|y/m/d,"
	    " --date-format <format>,\n --default-button <label>,"
	    " --default-item <name>, --default-no, --disable-esc,\n"
	    " --error-exit-code <retval>, --esc-exit-code <retval>,"
	    " --exit-label <label>,\n --extra-button,"
	    " --extra-exit-code <retval>, --extra-label <label>,\n"
	    " --left1-button <label>, --left1-exit-code <retval>,"
	    " --left2-button <label>,\n --left2-exit-code <retval>,"
	    " --left3-button <label>, --left3-exit-code <retval>,\n"
	    " --help-button, --help-exit-code <retval>, --help-label <label>,\n"
	    " --help-print-items, --help-print-name, --hfile <file>,"
	    " --hline <string>,\n --hmsg <string>, --ignore, --insecure,"
	    " --item-bottom-desc, --item-depth,\n --item-prefix,"
	    " --load-theme <file>, --max-input <size>, --no-cancel,\n"
	    " --no-descriptions, --no-label <label>, --no-lines, --no-names,"
	    " --no-ok,\n --no-shadow, --normal-screen, --ok-exit-code <retval>,"
	    " --ok-label <label>,\n --output-fd <fd>, --output-separator <sep>,"
	    " --print-maxsize, --print-size,\n --print-version, --quoted,"
	    " --right1-button <label>,\n --right1-exit-code <retval>,"
	    " --right2-button <label>,\n --right2-exit-code <retval>,"
	    " --right3-button <label>,\n --right3-exit-code <retval>,"
	    " --save-theme <file>, --separate-output,\n --separator <sep>,"
	    " --shadow, --single-quoted, --sleep <secs>, --stderr,\n --stdout,"
	    " --switch-buttons, --tab-escape, --tab-len <spaces>,"
	    " --text-escape,\n --text-unchanged, --theme 3d|blackwhite|flat,"
	    " --timeout-exit-code <retval>,\n --time-format <format>,"
	    " --title <title>, --yes-label <label>.");
	printf("\n\n");

	printf("Dialogs:\n");
	printf(" --calendar <text> <rows> <cols> [<dd> <mm> <yy>]\n");
	printf(" --checklist <text> <rows> <cols> <menurows> [<name> <desc> "
	    "on|off] ...\n");
	printf(" --datebox <text> <rows> <cols> [<dd> <mm> <yy>]\n");
	printf(" --form <text> <rows> <cols> <formrows> [<label> <ylabel> "
	    "<xlabel> <init> <yfield> <xfield> <fieldlen> <maxletters>] "
	    "...\n");
	printf(" --gauge <text> <rows> <cols> [<perc>]\n");
	printf(" --infobox <text> <rows> <cols>\n");
	printf(" --inputbox <text> <rows> <cols> [<init>]\n");
	printf(" --menu <text> <rows> <cols> <menurows> [<name> <desc>] ...\n");
	printf(" --mixedform <text> <rows> <cols> <formrows> [<label> <ylabel> "
	    "<xlabel> <init> <yfield> <xfield> <fieldlen> <maxletters> "
	    "0|1|2] ...\n");
	printf(" --mixedgauge <text> <rows> <cols> <mainperc> [<minilabel> "
	    "<miniperc>] ...\n");
	printf(" --msgbox <text> <rows> <cols>\n");
	printf(" --passwordbox <text> <rows> <cols> [<init>]\n");
	printf(" --passwordform <text> <rows> <cols> <formrows> [<label> "
	    "<ylabel> <xlabel> <init> <yfield> <xfield> <fieldlen> "
	    "<maxletters>] ...\n");
	printf(" --pause <text> <rows> <cols> <secs>\n");
	printf(" --radiolist <text> <rows> <cols> <menurows> [<name> <desc> "
	    "on|off] ...\n");
	printf(" --rangebox <text> <rows> <cols> <min> <max> [<init>]\n");
	printf(" --textbox <file> <rows> <cols>\n");
	printf(" --timebox <text> <rows> <cols> [<hh> <mm> <ss>]\n");
	printf(" --treeview <text> <rows> <cols> <menurows> [<depth> <name> "
	    "<desc> on|off] ...\n");
	printf(" --yesno <text> <rows> <cols>\n");
	printf("\n");

	printf("See 'man 1 bsddialog' for more information.\n");
}

int
parseargs(int argc, char **argv, struct bsddialog_conf *conf,
    struct options *opt)
{
	int arg, parsed, i;
	struct winsize ws;

	bsddialog_initconf(conf);
	conf->key.enable_esc = true;
	conf->button.always_active = true;

	memset(opt, 0, sizeof(struct options));
	opt->theme = -1;
	opt->output_fd = STDERR_FILENO;
	opt->max_input_form = 2048;
	opt->mandatory_dialog = true;

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
			opt->screen_mode = "smcup";
			break;
		case AND_DIALOG:
			if (opt->dialogbuilder == NULL)
				exit_error(true,"--and-dialog without "
				    "previous --<dialog>");
			break;
		case ASCII_LINES:
			conf->ascii_lines = true;
			break;
		case BACKTITLE:
			opt->backtitle = optarg;
			if (conf->y == BSDDIALOG_CENTER)
				conf->auto_topmargin = 2;
			break;
		case BEGIN_X:
			conf->x = (int)strtol(optarg, NULL, 10);
			if (conf->x < BSDDIALOG_CENTER)
				exit_error(false, "--begin-x %d is < %d",
				    conf->x, BSDDIALOG_CENTER);
			break;
		case BEGIN_Y:
			conf->y = (int)strtol(optarg, NULL, 10);
			if (conf->y < BSDDIALOG_CENTER)
				exit_error(false, "--begin-y %d is < %d",
				    conf->y, BSDDIALOG_CENTER);
			conf->auto_topmargin = 0;
			break;
		case BIKESHED:
			opt->bikeshed = true;
			break;
		case CANCEL_EXIT_CODE:
			set_exit_code(BSDDIALOG_CANCEL,
			    (int)strtol(optarg, NULL, 10));
			break;
		case CANCEL_LABEL:
			conf->button.cancel_label = optarg;
			break;
		case CLEAR_DIALOG:
			conf->clear = true;
			break;
		case CLEAR_SCREEN:
			opt->mandatory_dialog = false;
			opt->clearscreen = true;
			break;
		case COLUMNS_PER_ROW:
			conf->text.cols_per_row =
			    (u_int)strtoul(optarg, NULL, 10);
			break;
		case CR_WRAP:
			opt->cr_wrap = true;
			break;
		case DATEBOX_FORMAT:
			if (strcasecmp(optarg, "d/m/y") == 0)
				conf->date.format = "d/m/y";
			else if (strcasecmp(optarg, "m/d/y") == 0)
				conf->date.format = "m/d/y";
			else if (strcasecmp(optarg, "y/m/d") == 0)
				conf->date.format = "y/m/d";
			else
				exit_error(true,
				    "date format \"%s\" is invalid", optarg);
			break;
		case DATE_FORMAT:
			opt->date_fmt = optarg;
			break;
		case DEFAULT_BUTTON:
			conf->button.default_label = optarg;
			break;
		case DEFAULT_ITEM:
			opt->item_default = optarg;
			break;
		case DEFAULT_NO:
			conf->button.default_cancel = true;
			break;
		case DISABLE_ESC:
			conf->key.enable_esc = false;
			break;
		case ERROR_EXIT_CODE:
			set_exit_code(BSDDIALOG_ERROR,
			    (int)strtol(optarg, NULL, 10));
			break;
		case ESC_EXIT_CODE:
			set_exit_code(BSDDIALOG_ESC,
			    (int)strtol(optarg, NULL, 10));
			break;
		case EXIT_LABEL:
			conf->button.ok_label = optarg;
			break;
		case EXTRA_BUTTON:
			conf->button.with_extra = true;
			break;
		case EXTRA_EXIT_CODE:
			set_exit_code(BSDDIALOG_EXTRA,
			    (int)strtol(optarg, NULL, 10));
			break;
		case EXTRA_LABEL:
			conf->button.extra_label = optarg;
			break;
		case HELP_BUTTON:
			conf->button.with_help = true;
			break;
		case HELP_EXIT_CODE:
			i = (int)strtol(optarg, NULL, 10);
			set_exit_code(BSDDIALOG_HELP, i);
			/* _TEM_HELP follows _HELP */
			set_exit_code(BSDDIALOG_ITEM_HELP, i);
			break;
		case HELP_LABEL:
			conf->button.help_label = optarg;
			break;
		case HELP_PRINT_ITEMS:
			opt->help_print_items = true;
			break;
		case HELP_PRINT_NAME:
			opt->help_print_item_name = true;
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
			opt->ignore = true;
			break;
		case INSECURE:
			conf->form.securech = '*';
			break;
		case ITEM_BOTTOM_DESC:
			opt->item_bottomdesc = true;
			break;
		case ITEM_DEPTH:
			opt->item_depth = true;
			break;
		case ITEM_PREFIX:
			opt->item_prefix = true;
			break;
		case LEFT1_BUTTON:
			conf->button.left1_label = optarg;
			break;
		case LEFT1_EXIT_CODE:
			set_exit_code(BSDDIALOG_LEFT1,
			    (int)strtol(optarg, NULL, 10));
			break;
		case LEFT2_BUTTON:
			conf->button.left2_label = optarg;
			break;
		case LEFT2_EXIT_CODE:
			set_exit_code(BSDDIALOG_LEFT2,
			    (int)strtol(optarg, NULL, 10));
			break;
		case LEFT3_BUTTON:
			conf->button.left3_label = optarg;
			break;
		case LEFT3_EXIT_CODE:
			set_exit_code(BSDDIALOG_LEFT3,
			    (int)strtol(optarg, NULL, 10));
			break;
		case LOAD_THEME:
			opt->loadthemefile = optarg;
			break;
		case MAX_INPUT_FORM:
			opt->max_input_form = (u_int)strtoul(optarg, NULL, 10);
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
			opt->screen_mode = "rmcup";
			break;
		case OK_EXIT_CODE:
			set_exit_code(BSDDIALOG_OK,
			    (int)strtol(optarg, NULL, 10));
			break;
		case OK_LABEL:
			conf->button.ok_label = optarg;
			break;
		case OUTPUT_FD:
			opt->output_fd = (int)strtol(optarg, NULL, 10);
			break;
		case OUTPUT_SEPARATOR:
			opt->item_output_sep = optarg;
			break;
		case QUOTED:
			opt->item_always_quote = true;
			break;
		case PRINT_MAXSIZE:
			opt->mandatory_dialog = false;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
			dprintf(opt->output_fd, "MaxSize: %d, %d\n",
			    ws.ws_row, ws.ws_col);
			break;
		case PRINT_SIZE:
			conf->get_height = &opt->getH;
			conf->get_width = &opt->getW;
			break;
		case PRINT_VERSION:
			opt->mandatory_dialog = false;
			dprintf(opt->output_fd, "Version: %s\n",
			    LIBBSDDIALOG_VERSION);
			break;
		case RIGHT1_BUTTON:
			conf->button.right1_label = optarg;
			break;
		case RIGHT1_EXIT_CODE:
			set_exit_code(BSDDIALOG_RIGHT1,
			    (int)strtol(optarg, NULL, 10));
			break;
		case RIGHT2_BUTTON:
			conf->button.right2_label = optarg;
			break;
		case RIGHT2_EXIT_CODE:
			set_exit_code(BSDDIALOG_RIGHT2,
			    (int)strtol(optarg, NULL, 10));
			break;
		case RIGHT3_BUTTON:
			conf->button.right3_label = optarg;
			break;
		case RIGHT3_EXIT_CODE:
			set_exit_code(BSDDIALOG_RIGHT3,
			    (int)strtol(optarg, NULL, 10));
			break;
		case SAVE_THEME:
			opt->mandatory_dialog = false;
			opt->savethemefile = optarg;
			break;
		case SEPARATE_OUTPUT:
			opt->item_output_sepnl = true;
			break;
		case SHADOW:
			conf->shadow = true;
			break;
		case SINGLE_QUOTED:
			opt->item_singlequote = true;
			break;
		case SLEEP:
			conf->sleep = (u_int)strtoul(optarg, NULL, 10);
			break;
		case STDERR:
			opt->output_fd = STDERR_FILENO;
			break;
		case STDOUT:
			opt->output_fd = STDOUT_FILENO;
			break;
		case SWITCH_BUTTONS:
			conf->button.always_active = false;
			break;
		case TAB_ESCAPE:
			opt->tab_escape = true;
			break;
		case TAB_LEN:
			conf->text.tablen = (u_int)strtoul(optarg, NULL, 10);
			break;
		case TEXT_ESCAPE:
			conf->text.escape = true;
			break;
		case TEXT_UNCHANGED:
			opt->text_unchanged = true;
			break;
		case THEME:
			if (strcasecmp(optarg, "blackwhite") == 0)
				opt->theme = BSDDIALOG_THEME_BLACKWHITE;
			else if (strcasecmp(optarg, "flat") == 0)
				opt->theme = BSDDIALOG_THEME_FLAT;
			else if (strcasecmp(optarg, "3d") == 0)
				opt->theme = BSDDIALOG_THEME_3D;
			else
				exit_error(true,
				    "--theme: \"%s\" is unknown", optarg);
			break;
		case TIMEOUT_EXIT_CODE:
			set_exit_code(BSDDIALOG_TIMEOUT,
			    (int)strtol(optarg, NULL, 10));
			break;
		case TIME_FORMAT:
			opt->time_fmt = optarg;
			break;
		case TITLE:
			conf->title = optarg;
			break;
		/* Dialogs */
		case CALENDAR:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --calendar without "
				    "--and-dialog", opt->name);
			opt->name = "--calendar";
			opt->dialogbuilder = calendar_builder;
			break;
		case CHECKLIST:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --checklist without "
				    "--and-dialog", opt->name);
			opt->name = "--checklist";
			opt->dialogbuilder = checklist_builder;
			conf->auto_downmargin = 1;
			break;
		case DATEBOX:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --datebox without "
				    "--and-dialog", opt->name);
			opt->name = "--datebox";
			opt->dialogbuilder = datebox_builder;
			break;
		case FORM:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --form without "
				    "--and-dialog", opt->name);
			opt->name = "--form";
			opt->dialogbuilder = form_builder;
			conf->auto_downmargin = 1;
			break;
		case GAUGE:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --gauge without "
				    "--and-dialog", opt->name);
			opt->name = "--gauge";
			opt->dialogbuilder = gauge_builder;
			break;
		case INFOBOX:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --infobox without "
				    "--and-dialog", opt->name);
			opt->name = "--infobox";
			opt->dialogbuilder = infobox_builder;
			break;
		case INPUTBOX:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --inputbox without "
				    "--and-dialog", opt->name);
			opt->name = "--inputbox";
			opt->dialogbuilder = inputbox_builder;
			conf->auto_downmargin = 1;
			break;
		case MENU:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --menu without "
				    "--and-dialog", opt->name);
			opt->name = "--menu";
			opt->dialogbuilder = menu_builder;
			conf->auto_downmargin = 1;
			break;
		case MIXEDFORM:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --mixedform without "
				    "--and-dialog", opt->name);
			opt->name = "--mixedform";
			opt->dialogbuilder = mixedform_builder;
			conf->auto_downmargin = 1;
			break;
		case MIXEDGAUGE:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --mixedgauge without "
				    "--and-dialog", opt->name);
			opt->name = "--mixedgauge";
			opt->dialogbuilder = mixedgauge_builder;
			break;
		case MSGBOX:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --msgbox without "
				    "--and-dialog", opt->name);
			opt->name = "--";
			opt->dialogbuilder = msgbox_builder;
			break;
		case PAUSE:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --pause without "
				    "--and-dialog", opt->name);
			opt->name = "--pause";
			opt->dialogbuilder = pause_builder;
			break;
		case PASSWORDBOX:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --passwordbox without "
				    "--and-dialog", opt->name);
			opt->name = "--passwordbox";
			opt->dialogbuilder = passwordbox_builder;
			conf->auto_downmargin = 1;
			break;
		case PASSWORDFORM:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --passwordform "
				    "without --and-dialog", opt->name);
			opt->name = "--passwordform";
			opt->dialogbuilder = passwordform_builder;
			conf->auto_downmargin = 1;
			break;
		case RADIOLIST:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --radiolist without "
				    "--and-dialog", opt->name);
			opt->name = "--radiolist";
			opt->dialogbuilder = radiolist_builder;
			conf->auto_downmargin = 1;
			break;
		case RANGEBOX:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --rangebox without "
				    "--and-dialog", opt->name);
			opt->name = "--rangebox";
			opt->dialogbuilder = rangebox_builder;
			break;
		case TEXTBOX:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --textbox without "
				    "--and-dialog", opt->name);
			opt->name = "--textbox";
			opt->dialogbuilder = textbox_builder;
			break;
		case TIMEBOX:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --timebox without "
				    "--and-dialog", opt->name);
			opt->name = "--timebox";
			opt->dialogbuilder = timebox_builder;
			break;
		case TREEVIEW:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --treeview without "
				    "--and-dialog", opt->name);
			opt->name = "--treeview";
			opt->dialogbuilder = treeview_builder;
			conf->auto_downmargin = 1;
			break;
		case YESNO:
			if (opt->dialogbuilder != NULL)
				exit_error(true, "%s and --yesno without "
				    "--and-dialog", opt->name);
			opt->name = "--yesno";
			opt->dialogbuilder = yesno_builder;
			break;
		default: /* Error */
			if (opt->ignore == true)
				break;
			exit_error(true, "--ignore to continue");
		}
	}

	return (parsed);
}
