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

#include <sys/param.h>

#include <ctype.h>
#include <form.h>
#include <stdlib.h>
#include <string.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

#define REDRAWFORM 19860214 /* magic number */
#define ISFIELDHIDDEN(item)   (item.flags & BSDDIALOG_FIELDHIDDEN)
#define ISFIELDREADONLY(item) (item.flags & BSDDIALOG_FIELDREADONLY)

extern struct bsddialog_theme t;

/* field_userptr for private buffer and view options */
struct myfield {
	int  buflen;
	char *buf;
	int  pos;
	int  maxpos;
	bool secure;
	int  securech;
	const char *bottomdesc;
};
#define GETMYFIELD(field) ((struct myfield*)field_userptr(field))
#define GETMYFIELD2(form) ((struct myfield*)field_userptr(current_field(form)))

static void insertch(struct myfield *mf, int ch)
{
	int i;

	if (mf->buflen > mf->maxpos)
		return;

	for (i = mf->buflen; i >= mf->pos; i--) {
		mf->buf[i+1] = mf->buf[i];
	}

	mf->buf[mf->pos] = ch;
	mf->pos += 1;
	if (mf->pos > mf->maxpos)
		mf->pos = mf->maxpos;
	mf->buflen += 1;
	mf->buf[mf->buflen] = '\0';
}

static void shiftleft(struct myfield *mf)
{
	int i, last;

	for (i = mf->pos; i < mf->buflen -1; i++) {
		mf->buf[i] = mf->buf[i+1];
	}

	last = mf->buflen > 0 ? mf->buflen -1 : 0;
	mf->buf[last] = '\0';
		mf->buflen = last;
}

static void print_bottomdesc(struct myfield *mf)
{
	move(SCREENLINES - 1, 2);
	clrtoeol();
	if (mf->bottomdesc != NULL) {
		addstr(mf->bottomdesc);
		refresh();
	}
}

int
return_values(struct bsddialog_conf *conf, int output, int nitems,
    struct bsddialog_formitem *items, FORM *form, FIELD **cfield)
{
	int i;
	struct myfield *mf;

	if (output != BSDDIALOG_OK && conf->form.value_without_ok == false)
		return (output);

	form_driver(form, REQ_NEXT_FIELD);
	form_driver(form, REQ_PREV_FIELD);
	for (i = 0; i < nitems; i++) {
		mf = GETMYFIELD(cfield[i]);
		items[i].value = strdup(mf->buf);
		if (items[i].value == NULL)
			RETURN_ERROR("Cannot allocate memory for form value");
	}

	return (output);
}

static int
form_handler(struct bsddialog_conf *conf, WINDOW *widget, struct buttons bs,
    WINDOW *formwin, FORM *form, FIELD **cfield, int nitems,
    struct bsddialog_formitem *items)
{
	bool loop, buttupdate, informwin;
	int i, input, output;
	struct myfield *mf;

	mf = GETMYFIELD2(form);
	print_bottomdesc(mf);
	pos_form_cursor(form);
	form_driver(form, REQ_END_LINE);
	mf->pos = MIN(mf->buflen, mf->maxpos);
	curs_set(2);
	informwin = true;

	bs.curr = -1;
	buttupdate = true;

	loop = true;
	while (loop) {
		if (buttupdate) {
			draw_buttons(widget, bs, !informwin);
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
			output = return_values(conf, bs.value[bs.curr], nitems,
			    items, form, cfield);
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				output = return_values(conf, BSDDIALOG_ESC,
				    nitems, items, form, cfield);
				loop = false;
			}
			break;
		case '\t': /* TAB */
			if (informwin) {
				bs.curr = 0;
				informwin = false;
				curs_set(0);
			} else {
				bs.curr++;
				informwin = bs.curr >= (int)bs.nbuttons ?
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
				if (mf->pos >= mf->buflen)
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
			print_bottomdesc(mf);
			mf->pos = MIN(mf->buflen, mf->maxpos);
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
			print_bottomdesc(mf);
			mf->pos = MIN(mf->buflen, mf->maxpos);
			set_field_fore(current_field(form), t.form.f_fieldcolor);
			set_field_back(current_field(form), t.form.f_fieldcolor);
			break;
		case KEY_BACKSPACE:
		case 127: /* Backspace */
			mf = GETMYFIELD2(form);
			if (mf->pos <= 0)
				break;
			form_driver(form, REQ_DEL_PREV);
			form_driver(form, REQ_BEG_LINE);
			mf->pos = mf->pos - 1;
			for (i = 0; i < mf->pos; i++)
				form_driver(form, REQ_NEXT_CHAR);
			shiftleft(mf);
			break;
		case KEY_DC:
			form_driver(form, REQ_DEL_CHAR);
			mf = GETMYFIELD2(form);
			if (mf->pos < mf->buflen)
				shiftleft(mf);
			break;
		case KEY_F(1):
			if (conf->f1_file == NULL && conf->f1_message == NULL)
				break;
			if (f1help(conf) != 0)
				return (BSDDIALOG_ERROR);
			/* No Break */
		case KEY_RESIZE:
			output = REDRAWFORM;
			loop = false;
			break;
		default:
			if (informwin) {
				mf = GETMYFIELD2(form);
				if (mf->secure)
					form_driver(form, mf->securech);
				else
					form_driver(form, input);
				insertch(mf, input);
			}
			else {
				if (shortcut_buttons(input, &bs)) {
					output = return_values(conf,
					    bs.value[bs.curr], nitems, items,
					    form, cfield);
					loop = false;
				}
			}
			break;
		}
	}

	curs_set(0);

	return (output);
}

static int
form_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w,
    const char *text, int linelen, unsigned int *formheight, int nitems,
    struct buttons bs)
{
	int htext, wtext, menusize;

	if (cols == BSDDIALOG_AUTOSIZE || rows == BSDDIALOG_AUTOSIZE) {
		if (text_size(conf, rows, cols, text, &bs, *formheight + 2,
		    linelen + 2, &htext, &wtext) != 0)
			return (BSDDIALOG_ERROR);
	}

	if (cols == BSDDIALOG_AUTOSIZE)
		*w = widget_min_width(conf, wtext, linelen + 2, &bs);

	if (rows == BSDDIALOG_AUTOSIZE) {
		if (*formheight == 0) {
			menusize = widget_max_height(conf) - HBORDERS -
			     2 /*buttons*/ - htext;
			menusize = MIN(menusize, nitems + 2);
			*formheight = menusize - 2 < 0 ? 0 : menusize - 2;
		}
		else /* h autosize with fixed formheight */
			menusize = *formheight + 2;

		*h = widget_min_height(conf, htext, menusize, true);
	} else {
		if (*formheight == 0)
			*formheight = MIN(rows-6-htext, nitems);
	}

	return (0);
}

static int
form_checksize(int rows, int cols, const char *text, int formheight, int nitems,
    unsigned int linelen, struct buttons bs)
{
	int mincols, textrow, formrows;

	mincols = VBORDERS;
	/* buttons */
	mincols += bs.nbuttons * bs.sizebutton;
	mincols += bs.nbuttons > 0 ? (bs.nbuttons-1) * t.button.space : 0;
	mincols = MAX(mincols, (int)linelen + 4);

	if (cols < mincols)
		RETURN_ERROR("Few cols, width < size buttons or "
		    "forms (label + field)");

	textrow = text != NULL && strlen(text) > 0 ? 1 : 0;

	if (nitems > 0 && formheight == 0)
		RETURN_ERROR("fields > 0 but formheight == 0, probably "
		    "terminal too small");

	formrows = nitems > 0 ? 3 : 0;
	if (rows < 2  + 2 + formrows + textrow)
		RETURN_ERROR("Few lines for this menus");

	return (0);
}

int
bsddialog_form(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int formheight, unsigned int nitems,
    struct bsddialog_formitem *items)
{
	WINDOW *widget, *formwin, *textpad, *shadow;
	int i, output, color, y, x, h, w;
	FIELD **cfield;
	FORM *form;
	struct buttons bs;
	struct myfield *myfields;
	unsigned long maxline;

	/* disable form scrolling */
	if (formheight < nitems)
		formheight = nitems;

	for (i = 0; i < (int)nitems; i++) {
		if (items[i].maxvaluelen == 0)
			RETURN_ERROR("maxvaluelen cannot be zero");
		if (items[i].fieldlen == 0)
			RETURN_ERROR("fieldlen cannot be zero");
		if (items[i].fieldlen > items[i].maxvaluelen)
			RETURN_ERROR("fieldlen cannot be > maxvaluelen");
	}

	maxline = 0;
	myfields = malloc(nitems * sizeof(struct myfield));
	cfield = calloc(nitems + 1, sizeof(FIELD*));
	for (i = 0; i < (int)nitems; i++) {
		cfield[i] = new_field(1, items[i].fieldlen, items[i].yfield-1,
		    items[i].xfield-1, 0, 0);
		field_opts_off(cfield[i], O_STATIC);
		set_max_field(cfield[i], items[i].maxvaluelen);
		set_field_buffer(cfield[i], 0, items[i].init);

		myfields[i].buf = malloc(items[i].maxvaluelen + 1);
		memset(myfields[i].buf, 0, items[i].maxvaluelen + 1);
		strncpy(myfields[i].buf, items[i].init, items[i].maxvaluelen);

		myfields[i].buflen = strlen(myfields[i].buf);

		myfields[i].maxpos = items[i].maxvaluelen -1;
		myfields[i].pos = MIN(myfields[i].buflen, myfields[i].maxpos);

		myfields[i].bottomdesc = items[i].bottomdesc;
		set_field_userptr(cfield[i], &myfields[i]);

		field_opts_off(cfield[i], O_AUTOSKIP);
		field_opts_off(cfield[i], O_BLANK);

		if (ISFIELDHIDDEN(items[i])) {
			myfields[i].secure = true;
			myfields[i].securech = ' ';
			if (conf->form.securech != '\0')
				myfields[i].securech = conf->form.securech;
		}
		else
			myfields[i].secure = false;

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
		maxline = MAX(maxline, items[i].xfield + items[i].fieldlen - 1);
	}
	cfield[i] = NULL;

	 /* disable focus with 1 item (inputbox or passwordbox) */
	if (formheight == 1 && nitems == 1 && strlen(items[0].label) == 0 &&
	    items[0].xfield == 1 ) {
		set_field_fore(cfield[0], t.dialog.color);
		set_field_back(cfield[0], t.dialog.color);
	}

	get_buttons(conf, &bs, BUTTON_OK_LABEL, BUTTON_CANCEL_LABEL);

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);
	if (form_autosize(conf, rows, cols, &h, &w, text, maxline, &formheight,
	    nitems, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (form_checksize(h, w, text, formheight, nitems, maxline, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	if (new_dialog(conf, &shadow, &widget, y, x, h, w, &textpad, text, &bs,
	    true) != 0)
		return (BSDDIALOG_ERROR);

	prefresh(textpad, 0, 0, y + 1, x + 1 + TEXTHMARGIN,
	    y + h - formheight, x + 1 + w - TEXTHMARGIN);

	formwin = new_boxed_window(conf, y + h - 3 - formheight -2, x +1,
	    formheight+2, w-2, LOWERED);

	form = new_form(cfield);
	set_form_win(form, formwin);
	/* should be formheight */
	set_form_sub(form, derwin(formwin, nitems, w-4, 1, 1));
	post_form(form);

	for (i = 0; i < (int)nitems; i++)
		mvwaddstr(formwin, items[i].ylabel, items[i].xlabel,
		    items[i].label);

	wrefresh(formwin);

	do {
		output = form_handler(conf, widget, bs, formwin, form, cfield,
		    nitems, items);

		if (update_dialog(conf, shadow, widget, y, x, h, w, textpad,
		    text, &bs, true) != 0)
			return (BSDDIALOG_ERROR);

		doupdate();
		wrefresh(widget);

		prefresh(textpad, 0, 0, y + 1, x + 1 + TEXTHMARGIN,
		    y + h - formheight, x + 1 + w - TEXTHMARGIN);

		draw_borders(conf, formwin, formheight+2, w-2, LOWERED);
		wrefresh(formwin);

		refresh();
	} while (output == REDRAWFORM);

	unpost_form(form);
	free_form(form);
	for (i = 0; i < (int)nitems; i++) {
		free_field(cfield[i]);
		free(myfields[i].buf);
	}
	free(cfield);
	free(myfields);

	delwin(formwin);
	end_dialog(conf, shadow, widget, textpad);

	return (output);
}