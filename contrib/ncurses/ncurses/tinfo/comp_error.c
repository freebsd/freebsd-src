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


/*
 *	comp_error.c -- Error message routines
 *
 */

#include <curses.priv.h>

#include <tic.h>

MODULE_ID("$Id: comp_error.c,v 1.16 1998/08/01 23:39:51 tom Exp $")

bool	_nc_suppress_warnings;
int	_nc_curr_line;		/* current line # in input */
int	_nc_curr_col;		/* current column # in input */

static const char *sourcename;
static char termtype[MAX_NAME_SIZE+1];

void _nc_set_source(const char *const name)
{
	sourcename = name;
}

void _nc_set_type(const char *const name)
{
	if (name)
		strncpy( termtype, name, MAX_NAME_SIZE );
	else
		termtype[0] = '\0';
}

void _nc_get_type(char *name)
{
	strcpy( name, termtype );
}

static inline void where_is_problem(void)
{
	fprintf (stderr, "\"%s\"", sourcename);
	if (_nc_curr_line >= 0)
		fprintf (stderr, ", line %d", _nc_curr_line);
	if (_nc_curr_col >= 0)
		fprintf (stderr, ", col %d", _nc_curr_col);
	if (termtype[0])
		fprintf (stderr, ", terminal '%s'", termtype);
	fputc(':', stderr);
	fputc(' ', stderr);
}

void _nc_warning(const char *const fmt, ...)
{
va_list argp;

	if (_nc_suppress_warnings)
	    return;

	where_is_problem();
	va_start(argp,fmt);
	vfprintf (stderr, fmt, argp);
	fprintf (stderr, "\n");
	va_end(argp);
}


void _nc_err_abort(const char *const fmt, ...)
{
va_list argp;

	where_is_problem();
	va_start(argp,fmt);
	vfprintf (stderr, fmt, argp);
	fprintf (stderr, "\n");
	va_end(argp);
	exit(EXIT_FAILURE);
}


void _nc_syserr_abort(const char *const fmt, ...)
{
va_list argp;

	where_is_problem();
	va_start(argp,fmt);
	vfprintf (stderr, fmt, argp);
	fprintf (stderr, "\n");
	va_end(argp);

	/* If we're debugging, try to show where the problem occurred - this
	 * will dump core.
	 */
#if defined(TRACE) || !defined(NDEBUG)
	abort();
#else
	/* Dumping core in production code is not a good idea.
	 */
	exit(EXIT_FAILURE);
#endif
}
