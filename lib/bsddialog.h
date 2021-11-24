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

#ifndef _BSDDIALOG_H_
#define _BSDDIALOG_H_

#include <stdbool.h>

/* Exit status */
#define BSDDIALOG_ERROR		-1
#define BSDDIALOG_YESOK		 0
#define BSDDIALOG_NOCANCEL	 1
#define BSDDIALOG_HELP		 2
#define BSDDIALOG_EXTRA		 3
#define BSDDIALOG_ITEM_HELP	 4
#define BSDDIALOG_ESC		 5

/* size and position */
#define BSDDIALOG_FULLSCREEN	-1
#define BSDDIALOG_AUTOSIZE	 0
#define BSDDIALOG_CENTER	-1

struct bsddialog_conf {
	/* conf.* */
	bool ascii_lines;
	int aspect_ratio;
	int x;
	int y;
	bool clear;
	int *get_height;
	int *get_width;
	char *hfile;
	char *hline;
	/*int input_fd;*/
	/*bool keep_tite;*/
	/*bool keep_window;*/
	/*bool last_key;*/
	/*int max_input;*/
	/*bool no_kill;*/
	bool no_lines;
	/*bool no_mouse; useful?*/
	/*bool scrollbar; useful?*/
	/*char *separate_witget;*/
	bool shadow;
	/*bool size_err;*/
	int sleep;
	/*int timeout;*/
	char *title;
	/* conf.text.* */
	struct {
		bool colors;
		bool cr_wrap;
		bool no_collapse;
		bool no_nl_expand;
		/*bool tab_correct; textbox?*/
		/*int tab_len; textbox?*/
		bool trim;
	} text;
	/* conf.form.* */
	/*struct {
		bool insecure;
	} form;*/
	/* conf.menu.* */
	struct {
		bool align_left;
		char *colums_separator;
		char *default_item; /*delete, add int *focus to API?*/
		bool no_items;
		bool no_tags;
		/*bool visit_items;*/
	} menu;
	/* conf.button.* */
	struct {
		char *cancel_label;
		bool defaultno;
		char *default_label;
		char *exit_label;
		bool extra_button;
		char *extra_label;
		bool help_button;
		char *help_label;
		bool no_cancel;
		char *no_label;
		bool no_ok;
		char *ok_label;
		char *yes_label;
	} button;
};

struct bsddialog_menuitem {
	char *prefix;
	bool  on;
	int   depth;
	char *name;
	char *desc;
	char *bottomdesc;
};

enum bsddialog_grouptype {
	BSDDIALOG_CHECKLIST,
	BSDDIALOG_RADIOLIST,
	BSDDIALOG_SEPARATOR,
};

struct bsddialog_menugroup {
	enum bsddialog_grouptype type;
	unsigned int nitems;
	struct bsddialog_menuitem *items;
};

int bsddialog_init(void);
int bsddialog_end(void);
int bsddialog_backtitle(struct bsddialog_conf conf, char *backtitle);
const char *bsddialog_geterror(void);
int bsddialog_terminalheight(void);
int bsddialog_terminalwidth(void);
void bsddialog_initconf(struct bsddialog_conf *conf);

/* widgets */
int
bsddialog_buildlist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem);

int
bsddialog_calendar(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int *yy, unsigned int *mm, unsigned int *dd);

int
bsddialog_checklist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem);

int
bsddialog_dselect(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_editbox(struct bsddialog_conf conf, char* text, int rows, int cols);

int bsddialog_form(struct bsddialog_conf conf, char* text, int rows, int cols,
    int formheight, int argc, char **argv);

int
bsddialog_fselect(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_gauge(struct bsddialog_conf conf, char* text, int rows, int cols,
    int perc);

int
bsddialog_infobox(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_inputbox(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_inputmenu(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_menu(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem);

int
bsddialog_mixedform(struct bsddialog_conf conf, char* text, int rows, int cols,
    int formheight, int argc, char **argv);

int
bsddialog_mixedgauge(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int perc, int argc, char **argv);

int
bsddialog_mixedlist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int ngroups, struct bsddialog_menugroup *groups,
    int *focuslist, int *focusitem);

int
bsddialog_msgbox(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_passwordbox(struct bsddialog_conf conf, char* text, int rows,
    int cols);

int
bsddialog_passwordform(struct bsddialog_conf conf, char* text, int rows,
    int cols, int formheight, int argc, char **argv);

int
bsddialog_pause(struct bsddialog_conf conf, char* text, int rows, int cols,
    int sec);

int
bsddialog_prgbox(struct bsddialog_conf conf, char* text, int rows, int cols,
    char *command);

int
bsddialog_programbox(struct bsddialog_conf conf, char* text, int rows,
    int cols);

int
bsddialog_progressbox(struct bsddialog_conf conf, char* text, int rows,
    int cols);

int
bsddialog_radiolist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem);

int
bsddialog_rangebox(struct bsddialog_conf conf, char* text, int rows, int cols,
    int min, int max, int *value);

int
bsddialog_tailbox(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_tailboxbg(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_textbox(struct bsddialog_conf conf, char* text, int rows, int cols);

int
bsddialog_timebox(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int *hh, unsigned int *mm, unsigned int *ss);

int
bsddialog_treeview(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem);

int bsddialog_yesno(struct bsddialog_conf conf, char* text, int rows, int cols);

#endif
