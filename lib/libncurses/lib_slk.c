
/*
 *	lib_slk.c
 *	Soft key routines.
 *
 *	Copyright (C) Gerhard Fuernkranz 1993
 *	Permisson is granted to redistribute this
 *	code under the terms of the GNU Copyleft.
 */

#include "terminfo.h"
#include "curses.priv.h"
#include <string.h>
#include <stdlib.h>

int _slk_format;		/* format specified in slk_init() */

#define MAXCOLUMNS    135
#define MAXLINES      66
#define UNINITIALISED ((struct try * ) -1)
/*
 * Retrieve label text.
 */

char *
slk_label(int n)
{
SLK *slk = SP->_slk;

	T(("slk_label(%d)", n));

	if (slk == NULL || n < 1 || n > 8)
		return NULL;
	return slk->ent[n-1].text;
}

/*
 * Write the soft lables to the slk window.
 */

static void
slk_intern_refresh(SLK *slk)
{
int i;
	T(("slk_intern_refresh(%x)", slk));

	for (i = 0; i < 8; i++) {
		if (slk->dirty || slk->ent[i].dirty) {
			if (slk->ent[i].visible) {
				wmove(slk->win,0,slk->ent[i].x);
				wattrset(slk->win,A_REVERSE);
				waddstr(slk->win,slk->ent[i].form_text);
				wattrset(slk->win,A_NORMAL);
			}
			slk->ent[i].dirty = FALSE;
		}
	}
	slk->dirty = FALSE;
}

/*
 * Refresh the soft label window.
 */

int
slk_noutrefresh(void)
{
SLK *slk = SP->_slk;

	T(("slk_noutrefresh()"));

	if (slk == NULL)
		return ERR;
	if (slk->hidden)
		return OK;
	slk_intern_refresh(slk);
	return wnoutrefresh(slk->win);
}

/*
 * Refresh the soft label window.
 */

int
slk_refresh(void)
{
SLK *slk = SP->_slk;

	T(("slk_refresh()"));

	if (slk == NULL)
		return ERR;
	if (slk->hidden)
		return OK;
	slk_intern_refresh(slk);
	return wrefresh(slk->win);
}

/*
 * Restore the soft labels on the screen.
 */

int
slk_restore(void)
{
SLK *slk = SP->_slk;

	T(("slk_restore()"));

	if (slk == NULL)
		return ERR;
	slk->hidden = FALSE;
	slk->dirty = TRUE;
	return slk_refresh();
}

/*
 * Set soft label text.
 */

int
slk_set(int i, char *str, int format)
{
SLK *slk = SP->_slk;
int len;
	T(("slk_set(%d, \"%s\", %d)", i, str, format));

	if (slk == NULL || i < 1 || i > 8 || format < 0 || format > 2)
		return ERR;
	if (str == NULL)
		str = "";
	i--;
	strncpy(slk->ent[i].text,str,8);
	memset(slk->ent[i].form_text,' ',8);
	slk->ent[i].text[8] = 0;
	slk->ent[i].form_text[8] = 0;
	len = strlen(slk->ent[i].text);
	switch(format) {
	case 0: /* left */
		memcpy(slk->ent[i].form_text,slk->ent[i].text,len);
		break;
	case 1: /* center */
		memcpy(slk->ent[i].form_text+(8-len)/2,slk->ent[i].text,len);
		break;
	case 2: /* right */
		memcpy(slk->ent[i].form_text+8-len,slk->ent[i].text,len);
		break;
	}
	slk->ent[i].dirty = TRUE;
	return OK;
}

/*
 * Pretend, that soft keys have been changed.
 */

int
slk_touch(void)
{
SLK *slk = SP->_slk;
	T(("slk_touch()"));

	if (slk == NULL)
		return ERR;
	slk->dirty = TRUE;
	return OK;
}

/*
 * Remove soft labels from the screen.
 */

int
slk_clear(void)
{
SLK *slk = SP->_slk;

	T(("slk_clear()"));

	if (slk == NULL)
		return ERR;
	slk->hidden = TRUE;
	/* For simulated SLK's it's looks much more natural to
	   inherit those attributes from the standard screen */
	slk->win->_bkgd  = stdscr->_bkgd;
	slk->win->_attrs = stdscr->_attrs;
	werase(slk->win);
	return wrefresh(slk->win);
}

/*
 * Initialize soft labels.
 * Called from newterm()
 */

static int
slk_initialize(WINDOW *stwin, int cols)
{
SLK *slk;
int i, maxlab, x;

	T(("slk_initialize()"));

	if ((SP->_slk = slk = (SLK*) calloc(1,sizeof(SLK))) == NULL)
		return OK;
	maxlab = (cols+1)/9;
	for (i = 0; i < 8; i++) {
		memset(slk->ent[i].form_text,' ',8);
		slk->ent[i].visible = i < maxlab;
	}
	if (_slk_format == 1) {		/* 4-4 */
		int gap = cols - 64 - 6;
	if (gap < 1)
		gap = 1;
	for (i = x = 0; i < 8; i++) {
		slk->ent[i].x = x;
		x += 8;
		x += (i == 3) ? gap : 1;
	}
	}
	else {				/* 0 -> 3-2-3 */
		int gap = (cols - 64 - 5) / 2;
	if (gap < 1)
		gap = 1;
	for (i = x = 0; i < 8; i++) {
		slk->ent[i].x = x;
		x += 8;
		x += (i == 2 || i == 4) ? gap : 1;
	}
	}
	slk->dirty = TRUE;
	if ((slk->win = stwin) == NULL)
	{
		free(slk);
		return ERR;
	}

	return OK;
}

/*
 * Initialize soft labels.
 * Called by the user.
 */

int
slk_init(int format)
{
	if (format < 0 || format > 1)
		return ERR;
	_slk_format = format;
	ripoffline(-1, slk_initialize);
	return OK;
}

