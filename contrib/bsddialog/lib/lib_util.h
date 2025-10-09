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

#ifndef _LIBBSDDIALOG_UTIL_H_
#define _LIBBSDDIALOG_UTIL_H_

#define BORDER          1
#define BORDERS         (BORDER + BORDER)
#define TEXTHMARGIN     1
#define TEXTHMARGINS    (TEXTHMARGIN + TEXTHMARGIN)
#define HBUTTONS        2
#define OK_LABEL        "OK"
#define CANCEL_LABEL    "Cancel"

/* theme util */
extern struct bsddialog_theme t;
extern bool hastermcolors;

#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))
/* debug */
#define BSDDIALOG_DEBUG(y,x,fmt, ...) do {                                     \
	mvprintw(y, x, fmt, __VA_ARGS__);                                      \
	refresh();                                                             \
} while (0)
/* error and diagnostic */
#define RETURN_ERROR(str) do {                                                 \
	set_error_string(str);                                                 \
	return (BSDDIALOG_ERROR);                                              \
} while (0)
#define RETURN_FMTERROR(fmt, ...) do {                                         \
	set_fmt_error_string(fmt, __VA_ARGS__);                                \
	return (BSDDIALOG_ERROR);                                              \
} while (0)
/* check ptr */
#define CHECK_PTR(p) do {                                                      \
	if (p == NULL)                                                         \
		RETURN_ERROR("*" #p " is NULL");                               \
} while (0)
#define CHECK_ARRAY(nitem, a) do {                                             \
	if (nitem > 0 && a == NULL)                                            \
		RETURN_FMTERROR(#nitem " is %d but *" #a " is NULL", nitem);   \
} while (0)
/* widget utils */
#define KEY_CTRL(c) (c & 037)
#define TEXTPAD(d, downnotext) rtextpad(d, 0, 0, 0, downnotext)
#define SCREENLINES (getmaxy(stdscr))
#define SCREENCOLS  (getmaxx(stdscr))
#define CHECK_STR(s) (s == NULL ? "" : s)
#define UARROW(c) (c->ascii_lines ? '^' : ACS_UARROW)
#define DARROW(c) (c->ascii_lines ? 'v' : ACS_DARROW)
#define LARROW(c) (c->ascii_lines ? '<' : ACS_LARROW)
#define RARROW(c) (c->ascii_lines ? '>' : ACS_RARROW)
#define DRAW_BUTTONS(d) do {                                                   \
	draw_buttons(&d);                                                      \
	wnoutrefresh(d.widget);                                                \
} while (0)

/* internal types */
enum elevation { RAISED, LOWERED };

struct buttons {
	unsigned int nbuttons;
#define MAXBUTTONS 10 /* 3left + ok + extra + cancel + help + 3 right */
	const char *label[MAXBUTTONS];
	bool shortcut;
	wchar_t first[MAXBUTTONS];
	int value[MAXBUTTONS];
	int curr;
#define BUTTONVALUE(bs) bs.value[bs.curr]
	unsigned int sizebutton; /* including left and right delimiters */
};

struct dialog {
	bool built;         /* true after the first draw_dialog() */
	struct bsddialog_conf *conf;  /* Checked API conf */
	WINDOW *widget;     /* Size and position refer to widget */
	int y, x;           /* Current position, API conf.[y|x]: -1, >=0 */
	int rows, cols;     /* API rows and cols: -1, 0, >0 */
	int h, w;           /* Current height and width */
	const char *text;   /* Checked API text, at least "" */
	WINDOW *textpad;    /* Fake for textbox */
	struct buttons bs;  /* bs.nbuttons = 0 for no buttons */
	WINDOW *shadow;
};

/* error and diagnostic */
const char *get_error_string(void);
void set_error_string(const char *string);
void set_fmt_error_string(const char *fmt, ...);

/* multicolumn character string */
unsigned int strcols(const char *mbstring);
int str_props(const char *mbstring, unsigned int *cols, bool *has_multi_col);
void mvwaddwch(WINDOW *w, int y, int x, wchar_t wch);
wchar_t* alloc_mbstows(const char *mbstring);

/* buttons */
void
set_buttons(struct dialog *d, bool shortcut, const char *oklabel,
    const char *canclabel);
void draw_buttons(struct dialog *d);
bool shortcut_buttons(wint_t key, struct buttons *bs);

/* widget utils */
int hide_dialog(struct dialog *d);
int f1help_dialog(struct bsddialog_conf *conf);

void
draw_borders(struct bsddialog_conf *conf, WINDOW *win, enum elevation elev);

void
update_box(struct bsddialog_conf *conf, WINDOW *win, int y, int x, int h, int w,
    enum elevation elev);

void
rtextpad(struct dialog *d, int ytext, int xtext, int upnotext, int downnotext);

/* (auto) sizing and (auto) position */
int
set_widget_size(struct bsddialog_conf *conf, int rows, int cols, int *h,
    int *w);

int
set_widget_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h,
    int *w, const char *text, int *rowstext, struct buttons *bs, int hnotext,
    int minw);

int widget_checksize(int h, int w, struct buttons *bs, int hnotext, int minw);

int
set_widget_position(struct bsddialog_conf *conf, int *y, int *x, int h, int w);

int dialog_size_position(struct dialog *d, int hnotext, int minw, int *htext);

/* dialog */
void end_dialog(struct dialog *d);
int draw_dialog(struct dialog *d);

int
prepare_dialog(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, struct dialog *d);

#endif
