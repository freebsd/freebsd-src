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

#include <sys/param.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef PORTNCURSES
#include <ncurses/form.h>
#else
#include <form.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

#define REDRAWFORM 14021986 /* magic number */
#define ISFIELDHIDDEN(item)   (item.flags & BSDDIALOG_FIELDHIDDEN)
#define ISFIELDREADONLY(item) (item.flags & BSDDIALOG_FIELDREADONLY)

/* "Form": inputbox - passwordbox - form - passwordform - mixedform */

extern struct bsddialog_theme t;

/* util struct for private buffer and view options */
struct myfield {
	int len;
	char *buf;
	int pos;
	int size;
	bool secure;
	int securech;
};
#define GETMYFIELD(field) ((struct myfield*)field_userptr(field))
#define GETMYFIELD2(form) ((struct myfield*)field_userptr(current_field(form)))

static void insertch(struct myfield *mf, int ch)
{
	int i;

	if (mf->len == mf->size)
		return;

	for (i=mf->len-1; i>=mf->pos; i--) {
		mf->buf[i+1] = mf->buf[i];
	}

	mf->buf[mf->pos] = ch;
	mf->pos += 1;
	mf->len += 1;
	mf->buf[mf->len] = '\0';
}

static void shiftleft(struct myfield *mf)
{
	int i, last;

	for (i=mf->pos; i<mf->len; i++) {
		mf->buf[i] = mf->buf[i+1];
	}

	last = mf->len > 0 ? mf->len -1 : 0;
	mf->buf[last] = '\0';
		mf->len = last;
}

static int
form_handler(struct bsddialog_conf *conf, WINDOW *widget, int y, int cols,
    struct buttons bs, WINDOW *formwin, FORM *form, FIELD **cfield, int nitems,
    struct bsddialog_formitem *items)
{
	bool loop, buttupdate, informwin = true;
	int i, input, output;
	struct myfield *mf;

	curs_set(2);
	pos_form_cursor(form);
	loop = buttupdate = true;
	bs.curr = -1;
	form_driver(form, REQ_END_LINE);
	form_driver(form, REQ_END_LINE);
	mf = GETMYFIELD2(form);
	mf->pos = mf->len;
	while(loop) {
		if (buttupdate) {
			draw_buttons(widget, y, cols, bs, !informwin);
			wrefresh(widget);
			buttupdate = false;
		}
		wrefresh(formwin);
		input = getch();
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			if (informwin)
				break;
			output = bs.value[bs.curr];
			if (output == BSDDIALOG_YESOK) {
				form_driver(form, REQ_NEXT_FIELD);
				form_driver(form, REQ_PREV_FIELD);
				for (i=0; i<nitems; i++) {
					mf = GETMYFIELD(cfield[i]);
					items[i].value = strdup(mf->buf);
				}
			}
			loop = false;
			break;
		case 27: /* Esc */
			output = BSDDIALOG_ESC;
			loop = false;
			break;
		case '\t': /* TAB */
			if (informwin) {
				bs.curr = 0;
				informwin = false;
				curs_set(0);
			} else {
				bs.curr++;
				informwin = bs.curr >= (int) bs.nbuttons ?
				    true : false;
				if (informwin) {
					curs_set(2);
					pos_form_cursor(form);
				}
			}
			buttupdate = true;
			break;
		case KEY_LEFT:
			if (informwin) {
				form_driver(form, REQ_PREV_CHAR);
				mf = GETMYFIELD2(form);
				if (mf->pos > 0)
					mf->pos -= 1;
			} else {
				if (bs.curr > 0) {
					bs.curr--;
					buttupdate = true;
				}
			}
			break;
		case KEY_RIGHT:
			if (informwin) {
				mf = GETMYFIELD2(form);
				if (mf->pos >= mf->len)
					break;
				mf->pos += 1;
				form_driver(form, REQ_NEXT_CHAR);
			} else {
				if (bs.curr < (int) bs.nbuttons - 1) {
					bs.curr++;
					buttupdate = true;
				}
			}
			break;
		case KEY_UP:
			if (nitems < 2)
				break;
			set_field_fore(current_field(form), t.form.fieldcolor);
			set_field_back(current_field(form), t.form.fieldcolor);
			form_driver(form, REQ_PREV_FIELD);
			form_driver(form, REQ_END_LINE);
			mf = GETMYFIELD2(form);
			mf->pos = mf->len;
			set_field_fore(current_field(form), t.form.f_fieldcolor);
			set_field_back(current_field(form), t.form.f_fieldcolor);
			break;
		case KEY_DOWN:
			if (nitems < 2)
				break;
			set_field_fore(current_field(form), t.form.fieldcolor);
			set_field_back(current_field(form), t.form.fieldcolor);
			form_driver(form, REQ_NEXT_FIELD);
			form_driver(form, REQ_END_LINE);
			mf = GETMYFIELD2(form);
			mf->pos = mf->len;
			set_field_fore(current_field(form), t.form.f_fieldcolor);
			set_field_back(current_field(form), t.form.f_fieldcolor);
			break;
		case KEY_BACKSPACE:
		case 127: /* Backspace */
			mf = GETMYFIELD2(form);
			if (mf->pos <= 0)
				break;
			form_driver(form, REQ_DEL_PREV);
			mf = GETMYFIELD2(form);
			mf->pos -= 1;
			shiftleft(mf);
			break;
		case KEY_DC:
			form_driver(form, REQ_DEL_CHAR);
			mf = GETMYFIELD2(form);
			if (mf->len-1 >= mf->pos)
				shiftleft(mf);
			break;
		case KEY_F(1):
			if (conf->hfile == NULL)
				break;
			if (f1help(conf) != 0)
				return BSDDIALOG_ERROR;
			/* No Break */
		case KEY_RESIZE:
			output = REDRAWFORM;
			loop = false;
			break;
		default:
			/*
			 * user input, add unicode chars to "public" buffer
			 */
			if (informwin) {
				mf = GETMYFIELD2(form);
				if (mf->secure)
					form_driver(form, mf->securech);
				else
					form_driver(form, input);
				insertch(mf, input);
			}
			else {
				for (i = 0; i < (int) bs.nbuttons; i++) {
					if (tolower(input) ==
					    tolower((bs.label[i])[0])) {
						output = bs.value[i];
						loop = false;
					}
				}
			}
			break;
		}
	}

	curs_set(0);

	return output;
}

static void
form_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w,
    char *text, int linelen, unsigned int *formheight, int nitems,
    struct buttons bs)
{
	int textrow, menusize;

	textrow = text != NULL && strlen(text) > 0 ? 1 : 0;

	if (cols == BSDDIALOG_AUTOSIZE) {
		*w = VBORDERS;
		/* buttons size */
		*w += bs.nbuttons * bs.sizebutton;
		*w += bs.nbuttons > 0 ? (bs.nbuttons-1) * t.button.space : 0;
		/* line size */
		*w = MAX(*w, linelen + 3);
		/*
		* avoid terminal overflow,
		* -1 fix false negative with big menu over the terminal and
		* autosize, for example "portconfig /usr/ports/www/apache24/".
		*/
		*w = MIN(*w, widget_max_width(conf)-1);
	}

	if (rows == BSDDIALOG_AUTOSIZE) {
		*h = HBORDERS + 2 /* buttons */ + textrow;

		if (*formheight == 0) {
			*h += nitems + 2;
			*h = MIN(*h, widget_max_height(conf));
			menusize = MIN(nitems + 2, *h - (HBORDERS + 2 + textrow));
			menusize -=2;
			*formheight = menusize < 0 ? 0 : menusize;
		}
		else /* h autosize with a fixed formheight */
			*h = *h + *formheight + 2;

		/* avoid terminal overflow */
		*h = MIN(*h, widget_max_height(conf));
	}
	else {
		if (*formheight == 0)
			*formheight = MIN(rows-6-textrow, nitems);
	}
}

static int
form_checksize(int rows, int cols, char *text, int formheight, int nitems,
    struct buttons bs)
{
	int mincols, textrow, formrows;

	mincols = VBORDERS;
	/* buttons */
	mincols += bs.nbuttons * bs.sizebutton;
	mincols += bs.nbuttons > 0 ? (bs.nbuttons-1) * t.button.space : 0;
	/* line, comment to permet some cols hidden */
	/* mincols = MAX(mincols, linelen); */

	if (cols < mincols)
		RETURN_ERROR("Few cols, width < size buttons or "\
		    "labels + forms");

	textrow = text != NULL && strlen(text) > 0 ? 1 : 0;

	if (nitems > 0 && formheight == 0)
		RETURN_ERROR("fields > 0 but formheight == 0, probably "\
		    "terminal too small");

	formrows = nitems > 0 ? 3 : 0;
	if (rows < 2  + 2 + formrows + textrow)
		RETURN_ERROR("Few lines for this menus");

	return 0;
}

int
bsddialog_form(struct bsddialog_conf *conf, char* text, int rows, int cols,
    unsigned int formheight, unsigned int nitems,
    struct bsddialog_formitem *items)
{
	WINDOW *widget, *formwin, *textpad, *shadow;
	int i, output, color, y, x, h, w, htextpad;
	FIELD **cfield;
	FORM *form;
	struct buttons bs;
	struct myfield *myfields;
	unsigned long maxline;

	/* disable form scrolling like dialog */
	if (formheight < nitems)
		formheight = nitems;

	maxline = 0;
	myfields = malloc(nitems * sizeof(struct myfield));
	cfield = calloc(nitems + 1, sizeof(FIELD*));
	for (i=0; i < (int)nitems; i++) {
		cfield[i] = new_field(1, items[i].fieldlen, items[i].yfield-1,
		    items[i].xfield-1, 0, 0);
		field_opts_off(cfield[i], O_STATIC);
		set_max_field(cfield[i], items[i].maxvaluelen);
		set_field_buffer(cfield[i], 0, items[i].init);

		myfields[i].pos  = strlen(items[i].init);
		myfields[i].len  = strlen(items[i].init);
		myfields[i].size = items[i].maxvaluelen;
		myfields[i].buf  = malloc(myfields[i].size);
		memset(myfields[i].buf, 0, myfields[i].size);
		strcpy(myfields[i].buf, items[i].init);
		set_field_userptr(cfield[i], &myfields[i]);

		field_opts_off(cfield[i], O_AUTOSKIP);
		field_opts_off(cfield[i], O_BLANK);
		/* field_opts_off(field[i], O_BS_OVERLOAD); */

		if (ISFIELDHIDDEN(items[i])) {
			/* field_opts_off(field[i], O_PUBLIC); old hidden */
			myfields[i].secure = true;
			myfields[i].securech = ' ';
			if (conf->form.securech != '\0')
				myfields[i].securech = conf->form.securech;
		}
		else myfields[i].secure = false;

		if (ISFIELDREADONLY(items[i])) {
			field_opts_off(cfield[i], O_EDIT);
			field_opts_off(cfield[i], O_ACTIVE);
			color = t.form.readonlycolor;
		} else {
			color = i == 0 ? t.form.f_fieldcolor : t.form.fieldcolor;
		}
		set_field_fore(cfield[i], color);
		set_field_back(cfield[i], color);

		maxline = MAX(maxline, items[i].xlabel + strlen(items[i].label));
		maxline = MAX(maxline, items[i].xfield + items[i].fieldlen);
	}
	cfield[i] = NULL;

	 /* disable focus with 1 item (inputbox or passwordbox) */
	if (formheight == 1 && nitems == 1 && strlen(items[0].label) == 0 &&
	    items[0].xfield == 1 ) {
		set_field_fore(cfield[0], t.widget.color);
		set_field_back(cfield[0], t.widget.color);
	}
	
	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    BUTTONLABEL(cancel_label), BUTTONLABEL(help_label));

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;
	form_autosize(conf, rows, cols, &h, &w, text, maxline, &formheight,
	    nitems, bs);
	if (form_checksize(h, w, text, formheight, nitems, bs) != 0)
		return BSDDIALOG_ERROR;
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	if (new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w, RAISED,
	    &textpad, &htextpad, text, true) != 0)
		return BSDDIALOG_ERROR;

	prefresh(textpad, 0, 0, y + 1, x + 1 + t.text.hmargin,
	    y + h - formheight, x + 1 + w - t.text.hmargin);
	
	formwin = new_boxed_window(conf, y + h - 3 - formheight -2, x +1,
	    formheight+2, w-2, LOWERED);

	form = new_form(cfield);
	set_form_win(form, formwin);
	/* should be formheight */
	set_form_sub(form, derwin(formwin, nitems, w-4, 1, 1));
	post_form(form);

	for (i=0; i < (int)nitems; i++)
		mvwaddstr(formwin, items[i].ylabel, items[i].xlabel, items[i].label);

	wrefresh(formwin);

	do {
		output = form_handler(conf, widget, h-2, w, bs, formwin, form,
		    cfield, nitems, items);

		if(update_widget_withtextpad(conf, shadow, widget, h, w,
		    RAISED, textpad, &htextpad, text, true) != 0)
		return BSDDIALOG_ERROR;
			
		draw_buttons(widget, h-2, w, bs, true);
		wrefresh(widget);

		prefresh(textpad, 0, 0, y + 1, x + 1 + t.text.hmargin,
		    y + h - formheight, x + 1 + w - t.text.hmargin);

		draw_borders(conf, formwin, formheight+2, w-2, LOWERED);
		/* wrefresh(formwin); */
	} while (output == REDRAWFORM);

	unpost_form(form);
	free_form(form);
	for (i=0; i < (int)nitems; i++) {
		free_field(cfield[i]);
		free(myfields[i].buf);
	}
	free(cfield);
	free(myfields);

	delwin(formwin);
	end_widget_withtextpad(conf, widget, h, w, textpad, shadow);

	return output;
}
