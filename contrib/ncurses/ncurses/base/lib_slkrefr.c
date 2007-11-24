/****************************************************************************
 * Copyright (c) 1998-2005,2006 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 *     and: Juergen Pfeifer                         1996-on                 *
 *     and: Thomas E. Dickey                                                *
 ****************************************************************************/

/*
 *	lib_slkrefr.c
 *	Write SLK window to the (virtual) screen.
 */
#include <curses.priv.h>
#include <term.h>		/* num_labels, label_*, plab_norm */

MODULE_ID("$Id: lib_slkrefr.c,v 1.15 2006/11/25 22:32:15 tom Exp $")

/*
 * Write the soft labels to the soft-key window.
 */
static void
slk_intern_refresh(SLK * slk)
{
    int i;
    int fmt = SP->slk_format;

    for (i = 0; i < slk->labcnt; i++) {
	if (slk->dirty || slk->ent[i].dirty) {
	    if (slk->ent[i].visible) {
		if (num_labels > 0 && SLK_STDFMT(fmt)) {
		    if (i < num_labels) {
			TPUTS_TRACE("plab_norm");
			putp(TPARM_2(plab_norm, i + 1, slk->ent[i].form_text));
		    }
		} else {
		    wmove(slk->win, SLK_LINES(fmt) - 1, slk->ent[i].ent_x);
		    if (SP && SP->_slk) {
			wattrset(slk->win, AttrOf(SP->_slk->attr));
		    }
		    waddstr(slk->win, slk->ent[i].form_text);
		    /* if we simulate SLK's, it's looking much more
		       natural to use the current ATTRIBUTE also
		       for the label window */
		    wattrset(slk->win, WINDOW_ATTRS(stdscr));
		}
	    }
	    slk->ent[i].dirty = FALSE;
	}
    }
    slk->dirty = FALSE;

    if (num_labels > 0) {
	if (slk->hidden) {
	    TPUTS_TRACE("label_off");
	    putp(label_off);
	} else {
	    TPUTS_TRACE("label_on");
	    putp(label_on);
	}
    }
}

/*
 * Refresh the soft labels.
 */
NCURSES_EXPORT(int)
slk_noutrefresh(void)
{
    T((T_CALLED("slk_noutrefresh()")));

    if (SP == NULL || SP->_slk == NULL)
	returnCode(ERR);
    if (SP->_slk->hidden)
	returnCode(OK);
    slk_intern_refresh(SP->_slk);

    returnCode(wnoutrefresh(SP->_slk->win));
}

/*
 * Refresh the soft labels.
 */
NCURSES_EXPORT(int)
slk_refresh(void)
{
    T((T_CALLED("slk_refresh()")));

    if (SP == NULL || SP->_slk == NULL)
	returnCode(ERR);
    if (SP->_slk->hidden)
	returnCode(OK);
    slk_intern_refresh(SP->_slk);

    returnCode(wrefresh(SP->_slk->win));
}
