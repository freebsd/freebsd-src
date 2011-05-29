/*
 *  $Id: trace.c,v 1.12 2011/01/13 01:36:34 tom Exp $
 *
 *  trace.c -- implements screen-dump and keystroke-logging
 *
 *  Copyright 2007-2010,2011	Thomas E. Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 2.1
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to
 *	Free Software Foundation, Inc.
 *	51 Franklin St., Fifth Floor
 *	Boston, MA 02110, USA.
 */

#include <dialog.h>

#ifdef HAVE_DLG_TRACE

#include <dlg_keys.h>
#include <time.h>

#define myFP dialog_state.trace_output

static void
dlg_trace_time(const char *tag)
{
    time_t now = time((time_t *) 0);
    fprintf(myFP, "%s %s", tag, ctime(&now));
}

void
dlg_trace_msg(const char *fmt,...)
{
    if (myFP != 0) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(myFP, fmt, ap);
	va_end(ap);
	fflush(myFP);
    }
}

void
dlg_trace_win(WINDOW *win)
{
    if (myFP != 0) {
	int y, x;
	int j, k;
	int rc = getmaxy(win);
	int cc = getmaxx(win);
	chtype ch, c2;

	fprintf(myFP, "window %dx%d at %d,%d\n",
		rc, cc, getbegy(win), getbegx(win));

	getyx(win, y, x);
	for (j = 0; j < rc; ++j) {
	    fprintf(myFP, "%3d:", j);
	    for (k = 0; k < cc; ++k) {
		ch = mvwinch(win, j, k) & (A_CHARTEXT | A_ALTCHARSET);
		c2 = dlg_asciibox(ch);
		if (c2 != 0) {
		    ch = c2;
		} else if (unctrl(ch) == 0 || strlen(unctrl(ch)) > 1) {
		    ch = '.';
		}
		fputc((int) (ch & 0xff), myFP);
	    }
	    fputc('\n', myFP);
	}
	wmove(win, y, x);
	fflush(myFP);
    }
}

void
dlg_trace_chr(int ch, int fkey)
{
    if (myFP != 0) {
	const char *fkey_name = "?";
	if (fkey) {
	    if (fkey > KEY_MAX || (fkey_name = keyname(fkey)) == 0) {
#define CASE(name) case name: fkey_name = #name; break
		switch ((DLG_KEYS_ENUM) fkey) {
		    CASE(DLGK_MIN);
		    CASE(DLGK_OK);
		    CASE(DLGK_CANCEL);
		    CASE(DLGK_EXTRA);
		    CASE(DLGK_HELP);
		    CASE(DLGK_ESC);
		    CASE(DLGK_PAGE_FIRST);
		    CASE(DLGK_PAGE_LAST);
		    CASE(DLGK_PAGE_NEXT);
		    CASE(DLGK_PAGE_PREV);
		    CASE(DLGK_ITEM_FIRST);
		    CASE(DLGK_ITEM_LAST);
		    CASE(DLGK_ITEM_NEXT);
		    CASE(DLGK_ITEM_PREV);
		    CASE(DLGK_FIELD_FIRST);
		    CASE(DLGK_FIELD_LAST);
		    CASE(DLGK_FIELD_NEXT);
		    CASE(DLGK_FIELD_PREV);
		    CASE(DLGK_GRID_UP);
		    CASE(DLGK_GRID_DOWN);
		    CASE(DLGK_GRID_LEFT);
		    CASE(DLGK_GRID_RIGHT);
		    CASE(DLGK_DELETE_LEFT);
		    CASE(DLGK_DELETE_RIGHT);
		    CASE(DLGK_DELETE_ALL);
		    CASE(DLGK_ENTER);
		    CASE(DLGK_BEGIN);
		    CASE(DLGK_FINAL);
		    CASE(DLGK_SELECT);
		    CASE(DLGK_TRACE);
		}
	    }
	} else if (ch == ERR) {
	    fkey_name = "ERR";
	} else {
	    fkey_name = unctrl((chtype) ch);
	    if (fkey_name == 0)
		fkey_name = "UNKNOWN";
	}
	fprintf(myFP, "chr %s (ch=%#x, fkey=%d)\n",
		fkey_name,
		ch, fkey);
	fflush(myFP);
    }
}

void
dlg_trace(const char *fname)
{
    if (fname != 0) {
	if (myFP == 0) {
	    myFP = fopen(fname, "a");
	    if (myFP != 0) {
		dlg_trace_time("** opened at");
	    }
	}
    } else if (myFP != 0) {
	dlg_trace_time("** closed at");
	fclose(myFP);
	myFP = 0;
    }
}
#else
#undef dlg_trace
extern void dlg_trace(const char *);
void
dlg_trace(const char *fname)
{
    (void) fname;
}
#endif
