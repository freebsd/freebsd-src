/****************************************************************************
 * Copyright (c) 1998-2001,2002 Free Software Foundation, Inc.              *
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
 *                                                                          *
 * some of the code in here was contributed by:                             *
 * Magnus Bengtsson, d6mbeng@dtek.chalmers.se (Nov'93)                      *
 ****************************************************************************/

#define __INTERNAL_CAPS_VISIBLE
#include <curses.priv.h>

#include <termcap.h>
#include <tic.h>
#include <ctype.h>

#include <term_entry.h>

MODULE_ID("$Id: lib_termcap.c,v 1.43 2002/05/25 12:24:13 tom Exp $")

#define CSI       233
#define ESC       033		/* ^[ */
#define L_BRACK   '['
#define SHIFT_OUT 017		/* ^N */

NCURSES_EXPORT_VAR(char *) UP = 0;
NCURSES_EXPORT_VAR(char *) BC = 0;

static char *fix_me = 0;

static char *
set_attribute_9(int flag)
{
    const char *result;

    if ((result = tparm(set_attributes, 0, 0, 0, 0, 0, 0, 0, 0, flag)) == 0)
	result = "";
    return strdup(result);
}

static int
is_csi(char *s)
{
    if (UChar(s[0]) == CSI)
	return 1;
    else if (s[0] == ESC && s[1] == L_BRACK)
	return 2;
    return 0;
}

static char *
skip_zero(char *s)
{
    if (s[0] == '0') {
	if (s[1] == ';')
	    s += 2;
	else if (isalpha(UChar(s[1])))
	    s += 1;
    }
    return s;
}

static bool
similar_sgr(char *a, char *b)
{
    int csi_a = is_csi(a);
    int csi_b = is_csi(b);

    if (csi_a != 0 && csi_b != 0 && csi_a == csi_b) {
	a += csi_a;
	b += csi_b;
	if (*a != *b) {
	    a = skip_zero(a);
	    b = skip_zero(b);
	}
    }
    return strcmp(a, b) == 0;
}

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

NCURSES_EXPORT(int)
tgetent(char *bufp GCC_UNUSED, const char *name)
{
    int errcode;

    T((T_CALLED("tgetent()")));

    setupterm((NCURSES_CONST char *) name, STDOUT_FILENO, &errcode);

    PC = 0;
    UP = 0;
    BC = 0;
    fix_me = 0;

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

	/*
	 * While 'sgr0' is the "same" as termcap 'me', there is a compatibility
	 * issue.  The sgr/sgr0 capabilities include setting/clearing alternate
	 * character set mode.  A termcap application cannot use sgr, so sgr0
	 * strings that reset alternate character set mode will be
	 * misinterpreted.  Here, we remove those from the more common
	 * ISO/ANSI/VT100 entries, which have sgr0 agreeing with sgr.
	 */
	if (exit_attribute_mode != 0
	    && set_attributes != 0) {
	    char *on = set_attribute_9(1);
	    char *off = set_attribute_9(0);
	    char *tmp;
	    size_t i, j, k;

	    if (similar_sgr(off, exit_attribute_mode)
		&& !similar_sgr(off, on)) {
		TR(TRACE_DATABASE, ("adjusting sgr0 : %s", _nc_visbuf(off)));
		FreeIfNeeded(fix_me);
		fix_me = off;
		for (i = 0; off[i] != '\0'; ++i) {
		    if (on[i] != off[i]) {
			j = strlen(off);
			k = strlen(on);
			while (j != 0
			       && k != 0
			       && off[j - 1] == on[k - 1]) {
			    --j, --k;
			}
			while (off[j] != '\0') {
			    off[i++] = off[j++];
			}
			off[i] = '\0';
			break;
		    }
		}
		/* SGR 10 would reset to normal font */
		if ((i = is_csi(off)) != 0
		    && off[strlen(off) - 1] == 'm') {
		    tmp = skip_zero(off + i);
		    if (tmp[0] == '1'
			&& skip_zero(tmp + 1) != tmp + 1) {
			i = tmp - off;
			if (off[i - 1] == ';')
			    i--;
			j = skip_zero(tmp + 1) - off;
			while (off[j] != '\0') {
			    off[i++] = off[j++];
			}
			off[i] = '\0';
		    }
		}
		TR(TRACE_DATABASE, ("...adjusted me : %s", _nc_visbuf(fix_me)));
		if (!strcmp(fix_me, exit_attribute_mode)) {
		    TR(TRACE_DATABASE, ("...same result, discard"));
		    free(fix_me);
		    fix_me = 0;
		}
	    }
	    free(on);
	}

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

NCURSES_EXPORT(int)
tgetflag(NCURSES_CONST char *id)
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
    returnCode(0);		/* Solaris does this */
}

/***************************************************************************
 *
 * tgetnum(str)
 *
 * Look up numeric termcap capability str and return its value, or -1 if
 * not given.
 *
 ***************************************************************************/

NCURSES_EXPORT(int)
tgetnum(NCURSES_CONST char *id)
{
    int i;

    T((T_CALLED("tgetnum(%s)"), id));
    if (cur_term != 0) {
	TERMTYPE *tp = &(cur_term->type);
	for_each_number(i, tp) {
	    const char *capname = ExtNumname(tp, i, numcodes);
	    if (!strncmp(id, capname, 2)) {
		if (!VALID_NUMERIC(tp->Numbers[i]))
		    returnCode(ABSENT_NUMERIC);
		returnCode(tp->Numbers[i]);
	    }
	}
    }
    returnCode(ABSENT_NUMERIC);
}

/***************************************************************************
 *
 * tgetstr(str, area)
 *
 * Look up string termcap capability str and return a pointer to its value,
 * or NULL if not given.
 *
 ***************************************************************************/

NCURSES_EXPORT(char *)
tgetstr(NCURSES_CONST char *id, char **area)
{
    int i;
    char *result = NULL;

    T((T_CALLED("tgetstr(%s,%p)"), id, area));
    if (cur_term != 0) {
	TERMTYPE *tp = &(cur_term->type);
	for_each_string(i, tp) {
	    const char *capname = ExtStrname(tp, i, strcodes);
	    if (!strncmp(id, capname, 2)) {
		result = tp->Strings[i];
		TR(TRACE_DATABASE, ("found match : %s", _nc_visbuf(result)));
		/* setupterm forces canceled strings to null */
		if (VALID_STRING(result)) {
		    if (result == exit_attribute_mode
			&& fix_me != 0) {
			result = fix_me;
			TR(TRACE_DATABASE, ("altered to : %s", _nc_visbuf(result)));
		    }
		    if (area != 0
			&& *area != 0) {
			(void) strcpy(*area, result);
			*area += strlen(*area) + 1;
		    }
		}
		break;
	    }
	}
    }
    returnPtr(result);
}
