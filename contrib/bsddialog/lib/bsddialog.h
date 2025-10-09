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

#ifndef _LIBBSDDIALOG_H_
#define _LIBBSDDIALOG_H_

#include <stdbool.h>

#define LIBBSDDIALOG_VERSION     "1.0.5"

/* Return values */
#define BSDDIALOG_ERROR          -1
#define BSDDIALOG_OK              0
#define BSDDIALOG_YES             BSDDIALOG_OK
#define BSDDIALOG_CANCEL          1
#define BSDDIALOG_NO              BSDDIALOG_CANCEL
#define BSDDIALOG_HELP            2
#define BSDDIALOG_EXTRA           3
#define BSDDIALOG_TIMEOUT         4
#define BSDDIALOG_ESC             5
#define BSDDIALOG_LEFT1           6
#define BSDDIALOG_LEFT2           7
#define BSDDIALOG_LEFT3           8
#define BSDDIALOG_RIGHT1          9
#define BSDDIALOG_RIGHT2          10
#define BSDDIALOG_RIGHT3          11

/* Size and position */
#define BSDDIALOG_FULLSCREEN     -1
#define BSDDIALOG_AUTOSIZE        0
#define BSDDIALOG_CENTER         -1

/* Mixedgauge */
#define BSDDIALOG_MG_SUCCEEDED   -1
#define BSDDIALOG_MG_FAILED      -2
#define BSDDIALOG_MG_PASSED      -3
#define BSDDIALOG_MG_COMPLETED   -4
#define BSDDIALOG_MG_CHECKED     -5
#define BSDDIALOG_MG_DONE        -6
#define BSDDIALOG_MG_SKIPPED     -7
#define BSDDIALOG_MG_INPROGRESS  -8
#define BSDDIALOG_MG_BLANK       -9
#define BSDDIALOG_MG_NA          -10
#define BSDDIALOG_MG_PENDING     -11

/* Form */
#define BSDDIALOG_FIELDHIDDEN      1U
#define BSDDIALOG_FIELDREADONLY    2U
#define BSDDIALOG_FIELDNOCOLOR     4U
#define BSDDIALOG_FIELDCURSOREND   8U
#define BSDDIALOG_FIELDEXTEND      16U
#define BSDDIALOG_FIELDSINGLEBYTE  32U

struct bsddialog_conf {
	bool ascii_lines;
	unsigned int auto_minheight;
	unsigned int auto_minwidth;
	unsigned int auto_topmargin;
	unsigned int auto_downmargin;
	const char *bottomtitle;
	bool clear;
	int *get_height;
	int *get_width;
	bool no_lines;
	bool shadow;
	unsigned int sleep;
	const char *title;
	int y;
	int x;
	struct {
		bool enable_esc;
		const char *f1_file;
		const char *f1_message;
	} key;
	struct {
		unsigned int cols_per_row;
		bool escape;
		unsigned int tablen;
	} text;
	struct {
		bool align_left;
		bool no_desc;
		bool no_name;
		bool shortcut_buttons;
	} menu;
	struct {
		char securech;
		char *securembch;
		bool value_wchar;
	} form;
	struct {
		const char *format;
	} date;
	struct {
		bool always_active;
		const char *left1_label;
		const char *left2_label;
		const char *left3_label;
		bool without_ok;
		const char *ok_label;
		bool with_extra;
		const char *extra_label;
		bool without_cancel;
		const char *cancel_label;
		bool default_cancel;
		bool with_help;
		const char *help_label;
		const char *right1_label;
		const char *right2_label;
		const char *right3_label;
		const char *default_label;
	} button;
};

struct bsddialog_menuitem {
	const char *prefix;
	bool on;
	unsigned int depth;
	const char *name;
	const char *desc;
	const char *bottomdesc;
};

enum bsddialog_menutype {
	BSDDIALOG_CHECKLIST,
	BSDDIALOG_RADIOLIST,
	BSDDIALOG_SEPARATOR,
};

struct bsddialog_menugroup {
	enum bsddialog_menutype type;
	unsigned int nitems;
	struct bsddialog_menuitem *items;
	unsigned int min_on; /* unused for now */
};

struct bsddialog_formitem {
	const char *label;
	unsigned int ylabel;
	unsigned int xlabel;

	const char *init;
	unsigned int yfield;
	unsigned int xfield;
	unsigned int fieldlen;
	unsigned int maxvaluelen;
	char *value;
	unsigned int flags;

	const char *bottomdesc;
};

int bsddialog_init(void);
int bsddialog_init_notheme(void);
bool bsddialog_inmode(void);
int bsddialog_end(void);
int bsddialog_backtitle(struct bsddialog_conf *conf, const char *backtitle);
int bsddialog_backtitle_rf(struct bsddialog_conf *conf, const char *backtitle);
int bsddialog_initconf(struct bsddialog_conf *conf);
void bsddialog_clear(unsigned int y);
void bsddialog_refresh(void);
const char *bsddialog_geterror(void);

/* Dialogs */
int
bsddialog_calendar(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int *year, unsigned int *month, unsigned int *day);

int
bsddialog_checklist(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int menurows, unsigned int nitems,
    struct bsddialog_menuitem *items, int *focusitem);

int
bsddialog_datebox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int *year, unsigned int *month, unsigned int *day);

int
bsddialog_form(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int formheight, unsigned int nitems,
    struct bsddialog_formitem *items, int *focusitem);

int
bsddialog_gauge(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int perc, int fd, const char *sep, const char *end);

int
bsddialog_infobox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols);

int
bsddialog_menu(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int menurows, unsigned int nitems,
    struct bsddialog_menuitem *items, int *focusitem);

int
bsddialog_mixedgauge(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int mainperc, unsigned int nminibars,
    const char **minilabels, int *minipercs);

int
bsddialog_mixedlist(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int menurows, unsigned int ngroups,
    struct bsddialog_menugroup *groups, int *focuslist, int *focusitem);

int
bsddialog_msgbox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols);

int
bsddialog_pause(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int *seconds);

int
bsddialog_radiolist(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int menurows, unsigned int nitems,
    struct bsddialog_menuitem *items, int *focusitem);

int
bsddialog_rangebox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, int min, int max, int *value);

int
bsddialog_textbox(struct bsddialog_conf *conf, const char *file, int rows,
    int cols);

int
bsddialog_timebox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int *hh, unsigned int *mm, unsigned int *ss);

int
bsddialog_yesno(struct bsddialog_conf *conf, const char *text, int rows,
    int cols);

#endif
