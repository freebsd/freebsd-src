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

#ifdef PORTNCURSES
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

/* "Info": infobox */

#define MIN_HEIGHT	3

extern struct bsddialog_theme t;

static int
infobox_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w,
    char *text)
{
	int maxword, maxline, nlines;

	if (get_text_properties(conf, text, &maxword, &maxline, &nlines) != 0)
		return BSDDIALOG_ERROR;

	if (cols == BSDDIALOG_AUTOSIZE) {
		/* text size */
		*w =  maxline + VBORDERS + t.text.hmargin * 2;
		/* conf.auto_minwidth */
		*w = MAX(*w, (int)conf->auto_minwidth);
		/* avoid terminal overflow */
		*w = MIN(*w, widget_max_width(conf));
	}

	if (rows == BSDDIALOG_AUTOSIZE) {
		*h = MIN_HEIGHT - 1;
		if (maxword > 0)
			*h += MIN(nlines, (int)(*w / GET_ASPECT_RATIO(conf)));
		*h = MAX(*h, MIN_HEIGHT);
		/* conf.auto_minheight */
		*h = MAX(*h, (int)conf->auto_minheight);
		/* avoid terminal overflow */
		*h = MIN(*h, widget_max_height(conf));
	}

	return 0;
}

static int infobox_checksize(int rows, int cols)
{

	if (cols < HBORDERS + 1 + (int) t.text.hmargin * 2)
		RETURN_ERROR("Few cols, infobox needs at least width 3 + text "\
		    "margins");

	if (rows < 3)
		RETURN_ERROR("Infobox needs at least height 3");

	return 0;
}

int
bsddialog_infobox(struct bsddialog_conf *conf, char* text, int rows, int cols)
{
	WINDOW *shadow, *widget, *textpad;
	int y, x, h, w, htextpad;

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;
	if (infobox_autosize(conf, rows, cols, &h, &w, text) != 0)
		return BSDDIALOG_ERROR;
	if (infobox_checksize(h, w) != 0)
		return BSDDIALOG_ERROR;
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	if (new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w, RAISED,
	    &textpad, &htextpad, text, false) != 0)
		return BSDDIALOG_ERROR;

	pnoutrefresh(textpad, 0, 0, y+1, x+1+t.text.hmargin, y+h-2, x+w-t.text.hmargin);

	doupdate();

	end_widget_withtextpad(conf, widget, h, w, textpad, shadow);

	return (BSDDIALOG_OK);
}

