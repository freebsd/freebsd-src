/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
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
 ****************************************************************************/

#include <curses.priv.h>

#include <termcap.h>
#include <tic.h>

#define __INTERNAL_CAPS_VISIBLE
#include <term_entry.h>

MODULE_ID("$Id: lib_termcap.c,v 1.29 1999/09/05 01:06:43 tom Exp $")

/*
   some of the code in here was contributed by:
   Magnus Bengtsson, d6mbeng@dtek.chalmers.se
*/

char *UP;
char *BC;

/***************************************************************************
 *
 * tgetent(bufp, term)
 *
 * In termcap, this function reads in the entry for terminal `term' into the
 * buffer pointed to by bufp. It must be called before any of the functions
 * below are called.
 * In this terminfo emulation, tgetent() simply calls setupterm() (which
 * does a bit more than tgetent() in termcap does), and returns its return
 * value (1 if successful, 0 if no terminal with the given name could be
 * found, or -1 if no terminal descriptions have been installed on the
 * system).  The bufp argument is ignored.
 *
 ***************************************************************************/

int tgetent(char *bufp GCC_UNUSED, const char *name)
{
int errcode;

	T((T_CALLED("tgetent()")));

	setupterm((NCURSES_CONST char *)name, STDOUT_FILENO, &errcode);

	if (errcode == 1) {

		if (cursor_left)
		    if ((backspaces_with_bs = !strcmp(cursor_left, "\b")) == 0)
			backspace_if_not_bs = cursor_left;

		/* we're required to export these */
		if (pad_char != NULL)
			PC = pad_char[0];
		if (cursor_up != NULL)
			UP = cursor_up;
		if (backspace_if_not_bs != NULL)
			BC = backspace_if_not_bs;

		(void) baudrate();	/* sets ospeed as a side-effect */

/* LINT_PREPRO
#if 0*/
#include <capdefaults.c>
/* LINT_PREPRO
#endif*/

	}
	returnCode(errcode);
}

/***************************************************************************
 *
 * tgetflag(str)
 *
 * Look up boolean termcap capability str and return its value (TRUE=1 if
 * present, FALSE=0 if not).
 *
 ***************************************************************************/

int tgetflag(NCURSES_CONST char *id)
{
int i;

	T((T_CALLED("tgetflag(%s)"), id));
	if (cur_term != 0) {
	    TERMTYPE *tp = &(cur_term->type);
	    for_each_boolean(i, tp) {
		const char *capname = ExtBoolname(tp, i, boolcodes);
		if (!strncmp(id, capname, 2)) {
		    /* setupterm forces invalid booleans to false */
		    returnCode(tp->Booleans[i]);
		}
	    }
	}
	returnCode(0);	/* Solaris does this */
}

/***************************************************************************
 *
 * tgetnum(str)
 *
 * Look up numeric termcap capability str and return its value, or -1 if
 * not given.
 *
 ***************************************************************************/

int tgetnum(NCURSES_CONST char *id)
{
int i;

	T((T_CALLED("tgetnum(%s)"), id));
	if (cur_term != 0) {
	    TERMTYPE *tp = &(cur_term->type);
	    for_each_number(i, tp) {
		const char *capname = ExtNumname(tp, i, numcodes);
		if (!strncmp(id, capname, 2)) {
		    if (!VALID_NUMERIC(tp->Numbers[i]))
			return -1;
		    returnCode(tp->Numbers[i]);
		}
	    }
	}
	returnCode(ERR);
}

/***************************************************************************
 *
 * tgetstr(str, area)
 *
 * Look up string termcap capability str and return a pointer to its value,
 * or NULL if not given.
 *
 ***************************************************************************/

char *tgetstr(NCURSES_CONST char *id, char **area)
{
int i;

	T((T_CALLED("tgetstr(%s,%p)"), id, area));
	if (cur_term != 0) {
	    TERMTYPE *tp = &(cur_term->type);
	    for_each_string(i, tp) {
		const char *capname = ExtStrname(tp, i, strcodes);
		T(("trying %s", capname));
		if (!strncmp(id, capname, 2)) {
		    T(("found match : %s", _nc_visbuf(tp->Strings[i])));
		    /* setupterm forces cancelled strings to null */
		    if (area != 0
		     && *area != 0
		     && VALID_STRING(tp->Strings[i])) {
			(void) strcpy(*area, tp->Strings[i]);
			*area += strlen(*area) + 1;
		    }
		    returnPtr(tp->Strings[i]);
		}
	    }
	}
	returnPtr(NULL);
}

/*
 *	char *
 *	tgoto(string, x, y)
 *
 *	Retained solely for upward compatibility.  Note the intentional
 *	reversing of the last two arguments.
 *
 */

char *tgoto(const char *string, int x, int y)
{
	T((T_CALLED("tgoto(%s,%d,%d)"), string, x, y));
	returnPtr(tparm((NCURSES_CONST char *)string, y, x));
}
