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
 *	lib_trace.c - Tracing/Debugging routines
 */

#include <curses.priv.h>
#include <tic.h>

MODULE_ID("$Id: lib_trace.c,v 1.30 1998/10/03 23:41:42 tom Exp $")

#include <ctype.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

unsigned _nc_tracing = 0;	/* always define this */

#ifdef TRACE
const char *_nc_tputs_trace = "";
long _nc_outchars;

static FILE *	tracefp;	/* default to writing to stderr */
#endif

void trace(const unsigned int tracelevel GCC_UNUSED)
{
#ifdef TRACE
static bool	been_here = FALSE;
static char	my_name[] = "trace";

   	_nc_tracing = tracelevel;
	if (! been_here && tracelevel) {
		been_here = TRUE;

		if (_nc_access(my_name, W_OK) < 0
		 || (tracefp = fopen(my_name, "w")) == 0) {
			perror("curses: Can't open 'trace' file: ");
			exit(EXIT_FAILURE);
		}
		/* Try to set line-buffered mode, or (failing that) unbuffered,
		 * so that the trace-output gets flushed automatically at the
		 * end of each line.  This is useful in case the program dies. 
		 */
#if HAVE_SETVBUF	/* ANSI */
		(void) setvbuf(tracefp, (char *)0, _IOLBF, 0);
#elif HAVE_SETBUF	/* POSIX */
		(void) setbuffer(tracefp, (char *)0);
#endif
		_tracef("TRACING NCURSES version %s (%d)",
			NCURSES_VERSION, NCURSES_VERSION_PATCH);
	}
#endif
}

const char *_nc_visbuf2(int bufnum, const char *buf)
/* visibilize a given string */
{
char *vbuf;
char *tp;
int c;

	if (buf == 0)
	    return("(null)");
	if (buf == CANCELLED_STRING)
	    return("(cancelled)");

	tp = vbuf = _nc_trace_buf(bufnum, (strlen(buf) * 4) + 5);
	*tp++ = '"';
    	while ((c = *buf++) != '\0') {
		if (c == '"') {
			*tp++ = '\\'; *tp++ = '"';
		} else if (is7bits(c) && (isgraph(c) || c == ' ')) {
			*tp++ = c;
		} else if (c == '\n') {
			*tp++ = '\\'; *tp++ = 'n';
		} else if (c == '\r') {
			*tp++ = '\\'; *tp++ = 'r';
		} else if (c == '\b') {
			*tp++ = '\\'; *tp++ = 'b';
		} else if (c == '\033') {
			*tp++ = '\\'; *tp++ = 'e';
		} else if (is7bits(c) && iscntrl(c)) {
			*tp++ = '\\'; *tp++ = '^'; *tp++ = '@' + c;
		} else {
			sprintf(tp, "\\%03o", c & 0xff);
			tp += strlen(tp);
		}
	}
	*tp++ = '"';
	*tp++ = '\0';
	return(vbuf);
}

const char *_nc_visbuf(const char *buf)
{
	return _nc_visbuf2(0, buf);
}

#ifdef TRACE
void
_tracef(const char *fmt, ...)
{
static const char Called[] = T_CALLED("");
static const char Return[] = T_RETURN("");
static int level;
va_list ap;
bool	before = FALSE;
bool	after = FALSE;
int	doit = _nc_tracing;
int	save_err = errno;

	if (strlen(fmt) >= sizeof(Called) - 1) {
		if (!strncmp(fmt, Called, sizeof(Called)-1)) {
			before = TRUE;
			level++;
		} else if (!strncmp(fmt, Return, sizeof(Return)-1)) {
			after = TRUE;
		}
		if (before || after) {
			if ((level <= 1)
			 || (doit & TRACE_ICALLS) != 0)
				doit &= (TRACE_CALLS|TRACE_CCALLS);
			else
				doit = 0;
		}
	}

	if (doit != 0) {
		if (tracefp == 0)
			tracefp = stderr;
		if (before || after) {
			int n;
			for (n = 1; n < level; n++)
				fputs("+ ", tracefp);
		}
		va_start(ap, fmt);
		vfprintf(tracefp, fmt, ap);
		fputc('\n', tracefp);
		va_end(ap);
		fflush(tracefp);
	}

	if (after && level)
		level--;
	errno = save_err;
}

/* Trace 'int' return-values */
int _nc_retrace_int(int code)
{
	T((T_RETURN("%d"), code));
	return code;
}

/* Trace 'char*' return-values */
char * _nc_retrace_ptr(char * code)
{
	T((T_RETURN("%s"), _nc_visbuf(code)));
	return code;
}

/* Trace 'WINDOW *' return-values */
WINDOW *_nc_retrace_win(WINDOW *code)
{
	T((T_RETURN("%p"), code));
	return code;
}
#endif /* TRACE */
