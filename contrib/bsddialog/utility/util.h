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

#ifndef _BSDDIALOG_UTIL_H_
#define _BSDDIALOG_UTIL_H_

/*
 * Exit codes and errors, bsddialog.c
 */
#define BSDDIALOG_ITEM_HELP 12

void set_exit_code(int lib_retval, int exitcode);
void exit_error(bool usage, const char *fmt, ...);
void error_args(const char *dialog, int argc, char **argv);

/*
 * Command Line, util_cli.c
 */
struct options {
	/* Menus options */
	bool item_always_quote;
	char *item_default;
	bool item_depth;
	char *item_output_sep;
	bool item_output_sepnl;
	bool item_prefix;
	bool item_singlequote;
	/* Menus and Forms options */
	bool help_print_item_name;
	bool help_print_items;
	bool item_bottomdesc;
	/* Forms options */
	int unsigned max_input_form;
	/* Date and Time options */
	char *date_fmt;
	char *time_fmt;
	/* General options */
	int getH;
	int getW;
	bool ignore;
	int output_fd;
	/* Text option */
	bool cr_wrap;
	bool tab_escape;
	bool text_unchanged;
	/* Theme and Screen options*/
	char *backtitle;
	bool bikeshed;
	enum bsddialog_default_theme theme;
	bool clearscreen;
	char *loadthemefile;
	char *savethemefile;
	const char *screen_mode;
	/* Dialog */
	bool mandatory_dialog;
	const char *name;
	int (*dialogbuilder)(struct bsddialog_conf *conf, char* text, int rows,
	    int cols, int argc, char **argv, struct options *opt);
};

void usage(void);
int
parseargs(int argc, char **argv, struct bsddialog_conf *conf,
    struct options *opt);

/*
 * Dialogs builders, util_builders.c
 */
#define BUILDER_ARGS struct bsddialog_conf *conf, char* text, int rows,        \
	int cols, int argc, char **argv, struct options *opt
int calendar_builder(BUILDER_ARGS);
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

/*
 * Theme, util_theme.c
 */
void savetheme(const char *file);
void loadtheme(const char *file, bool compatibility);
void setdeftheme(enum bsddialog_default_theme theme);
void bikeshed(struct bsddialog_conf *conf);
void startuptheme(void);

#endif
