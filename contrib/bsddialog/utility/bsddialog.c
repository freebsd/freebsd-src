/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 Alfonso Sabato Siciliano
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

#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <term.h>

#include <bsddialog.h>
#include <bsddialog_theme.h>

#include "util.h"

#define EXITCODE(retval) (exitcodes[retval + 1].value)
#define UNUSED_PAR(x) UNUSED_ ## x __attribute__((__unused__))

static void custom_text(struct options *opt, char *text, char *buf);

/* Exit codes */
struct exitcode {
	const char *name;
	int value;
};

static struct exitcode exitcodes[14] = {
	{ "BSDDIALOG_ERROR",    255 },
	{ "BSDDIALOG_OK",         0 },
	{ "BSDDIALOG_CANCEL",     1 },
	{ "BSDDIALOG_HELP",       2 },
	{ "BSDDIALOG_EXTRA",      3 },
	{ "BSDDIALOG_TIMEOUT",    4 },
	{ "BSDDIALOG_ESC",        5 },
	{ "BSDDIALOG_LEFT1",      6 },
	{ "BSDDIALOG_LEFT2",      7 },
	{ "BSDDIALOG_LEFT3",      8 },
	{ "BSDDIALOG_RIGHT1",     9 },
	{ "BSDDIALOG_RIGHT2",    10 },
	{ "BSDDIALOG_RIGHT3",    11 },
	{ "BSDDIALOG_ITEM_HELP",  2 } /* like HELP by default */
};

void set_exit_code(int lib_retval, int exitcode)
{
	exitcodes[lib_retval + 1].value = exitcode;
}

/* Error */
void exit_error(bool usage, const char *fmt, ...)
{
	va_list arg_ptr;

	if (bsddialog_inmode())
		bsddialog_end();
	printf("Error: ");
	va_start(arg_ptr, fmt);
	vprintf(fmt, arg_ptr);
	va_end(arg_ptr);
	printf(".\n\n");
	if (usage) {
		printf("See \'bsddialog --help\' or \'man 1 bsddialog\' ");
		printf("for more information.\n");
	}

	exit (EXITCODE(BSDDIALOG_ERROR));
}

void error_args(const char *dialog, int argc, char **argv)
{
	int i;

	if (bsddialog_inmode())
		bsddialog_end();
	printf("Error: %s unexpected argument%s:", dialog, argc > 1 ? "s" : "");
	for (i = 0; i < argc; i++)
		printf(" \"%s\"", argv[i]);
	printf(".\n\n");
	printf("See \'bsddialog --help\' or \'man 1 bsddialog\' ");
	printf("for more information.\n");

	exit (EXITCODE(BSDDIALOG_ERROR));
}

/* init */
static void sigint_handler(int UNUSED_PAR(sig))
{
	bsddialog_end();

	exit(EXITCODE(BSDDIALOG_ERROR));
}

static void start_bsddialog_mode(void)
{
	if (bsddialog_inmode())
		return;
	if (bsddialog_init() != BSDDIALOG_OK)
		exit_error(false, bsddialog_geterror());

	signal(SIGINT, sigint_handler);
}

static void getenv_exitcodes(void)
{
	int i;
	int value;
	char *envvalue;

	for (i = 0; i < 10; i++) {
		envvalue = getenv(exitcodes[i].name);
		if (envvalue == NULL || envvalue[0] == '\0')
			continue;
		value = (int)strtol(envvalue, NULL, 10);
		exitcodes[i].value = value;
		/* ITEM_HELP follows HELP without explicit setting */
		if(i == BSDDIALOG_HELP + 1)
			exitcodes[BSDDIALOG_ITEM_HELP + 1].value = value;
	}
}

/*
 * bsddialog utility: TUI widgets and dialogs.
 */
int main(int argc, char *argv[argc])
{
	bool startup;
	int i, rows, cols, retval, parsed, nargc, firstoptind;
	char *text, **nargv, *pn;
	struct bsddialog_conf conf;
	struct options opt;

	setlocale(LC_ALL, "");
	getenv_exitcodes();
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

	startup = true;
	while (true) {
		parsed = parseargs(argc, argv, &conf, &opt);
		nargc = argc - parsed;
		nargv = argv + parsed;
		argc = parsed - optind;
		argv += optind;

		if (opt.mandatory_dialog && opt.dialogbuilder == NULL)
			exit_error(true, "expected a --<dialog>");

		if (opt.dialogbuilder == NULL && argc > 0)
			error_args("(no --<dialog>)", argc, argv);

		/* --print-maxsize or --print-version */
		if (opt.mandatory_dialog == false && opt.clearscreen == false &&
		    opt.savethemefile == NULL && opt.dialogbuilder == NULL) {
			retval = BSDDIALOG_OK;
			break;
		}

		/* --<dialog>, --save-theme or clear-screen */
		text = NULL; /* useless inits, fix compiler warnings */
		rows = BSDDIALOG_AUTOSIZE;
		cols = BSDDIALOG_AUTOSIZE;
		if (opt.dialogbuilder != NULL) {
			if (argc < 3)
				exit_error(true,
				    "expected <text> <rows> <cols>");
			if ((text = strdup(argv[0])) == NULL)
				exit_error(false, "cannot allocate <text>");
			if (opt.dialogbuilder != textbox_builder)
				custom_text(&opt, argv[0], text);
			rows = (int)strtol(argv[1], NULL, 10);
			cols = (int)strtol(argv[2], NULL, 10);
			argc -= 3;
			argv += 3;
		}

		/* bsddialog terminal mode (first iteration) */
		start_bsddialog_mode();

		if (opt.screen_mode != NULL) {
			opt.screen_mode = tigetstr(opt.screen_mode);
			if (opt.screen_mode != NULL &&
			    opt.screen_mode != (char*)-1) {
				tputs(opt.screen_mode, 1, putchar);
				fflush(stdout);
				bsddialog_refresh();
			}
		}

		/* theme */
		if (startup)
			startuptheme();
		startup = false;
		if ((int)opt.theme >= 0)
			setdeftheme(opt.theme);
		if (opt.loadthemefile != NULL)
			loadtheme(opt.loadthemefile, false);
		if (opt.bikeshed)
			bikeshed(&conf);
		if (opt.savethemefile != NULL)
			savetheme(opt.savethemefile);

		/* backtitle and dialog */
		if (opt.dialogbuilder == NULL)
			break;
		if (opt.backtitle != NULL)
			if(bsddialog_backtitle(&conf, opt.backtitle))
				exit_error(false, bsddialog_geterror());
		retval = opt.dialogbuilder(&conf, text, rows, cols, argc, argv,
		    &opt);
		free(text);
		if (retval == BSDDIALOG_ERROR)
			exit_error(false, bsddialog_geterror());
		if (conf.get_height != NULL && conf.get_width != NULL)
			dprintf(opt.output_fd, "DialogSize: %d, %d\n",
			    *conf.get_height, *conf.get_width);
		if (opt.clearscreen)
			bsddialog_clear(0);
		opt.clearscreen = false;
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

	if (bsddialog_inmode()) {
		/* --clear-screen can be a single option */
		if (opt.clearscreen)
			bsddialog_clear(0);
		bsddialog_end();
	}
	/* end bsddialog terminal mode */

	return (EXITCODE(retval));
}

void custom_text(struct options *opt, char *text, char *buf)
{
	bool trim, crwrap;
	int i, j;

	if (strstr(text, "\\n") == NULL) {
		/* "hasnl" mode */
		trim = true;
		crwrap = true;
	} else {
		trim = false;
		crwrap = opt->cr_wrap;
	}
	if (opt->text_unchanged) {
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
				if (opt->tab_escape) {
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
			buf[j] = opt->text_unchanged ? '\t' : ' ';
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
