/****************************************************************************
 * Copyright (c) 1998,1999,2000,2001 Free Software Foundation, Inc.         *
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
 *	tputs.c
 *		delay_output()
 *		_nc_outch()
 *		tputs()
 *
 */

#include <curses.priv.h>
#include <ctype.h>
#include <term.h>		/* padding_baud_rate, xon_xoff */
#include <termcap.h>		/* ospeed */
#include <tic.h>

MODULE_ID("$Id: lib_tputs.c,v 1.56 2001/04/21 18:53:53 tom Exp $")

NCURSES_EXPORT_VAR(char)
PC = 0;				/* used by termcap library */
NCURSES_EXPORT_VAR(NCURSES_OSPEED) ospeed = 0;	/* used by termcap library */

NCURSES_EXPORT_VAR(int)
_nc_nulls_sent = 0;		/* used by 'tack' program */

     static int (*my_outch) (int c) = _nc_outch;

NCURSES_EXPORT(int)
delay_output(int ms)
{
    T((T_CALLED("delay_output(%d)"), ms));

    if (no_pad_char) {
	_nc_flush();
	napms(ms);
    } else {
	register int nullcount;

	nullcount = (ms * _nc_baudrate(ospeed)) / 10000;
	for (_nc_nulls_sent += nullcount; nullcount > 0; nullcount--)
	    my_outch(PC);
	if (my_outch == _nc_outch)
	    _nc_flush();
    }

    returnCode(OK);
}

NCURSES_EXPORT(void)
_nc_flush(void)
{
    (void) fflush(NC_OUTPUT);
}

NCURSES_EXPORT(int)
_nc_outch(int ch)
{
#ifdef TRACE
    _nc_outchars++;
#endif /* TRACE */

    if (SP != 0
	&& SP->_cleanup) {
	char tmp = ch;
	/*
	 * POSIX says write() is safe in a signal handler, but the
	 * buffered I/O is not.
	 */
	write(fileno(NC_OUTPUT), &tmp, 1);
    } else {
	putc(ch, NC_OUTPUT);
    }
    return OK;
}

#if USE_WIDEC_SUPPORT
/*
 * Reference: The Unicode Standard 2.0
 *
 * No surrogates supported (we're storing only one 16-bit Unicode value per
 * cell).
 */
NCURSES_EXPORT(int)
_nc_utf8_outch(int ch)
{
    static const unsigned byteMask = 0xBF;
    static const unsigned otherMark = 0x80;
    static const unsigned firstMark[] =
    {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

    int result[7], *ptr;
    int count = 0;

    if ((unsigned int) ch < 0x80)
	count = 1;
    else if ((unsigned int) ch < 0x800)
	count = 2;
    else if ((unsigned int) ch < 0x10000)
	count = 3;
    else if ((unsigned int) ch < 0x200000)
	count = 4;
    else if ((unsigned int) ch < 0x4000000)
	count = 5;
    else if ((unsigned int) ch <= 0x7FFFFFFF)
	count = 6;
    else {
	count = 3;
	ch = 0xFFFD;
    }
    ptr = result + count;
    switch (count) {
    case 6:
	*--ptr = (ch | otherMark) & byteMask;
	ch >>= 6;
	/* FALLTHRU */
    case 5:
	*--ptr = (ch | otherMark) & byteMask;
	ch >>= 6;
	/* FALLTHRU */
    case 4:
	*--ptr = (ch | otherMark) & byteMask;
	ch >>= 6;
	/* FALLTHRU */
    case 3:
	*--ptr = (ch | otherMark) & byteMask;
	ch >>= 6;
	/* FALLTHRU */
    case 2:
	*--ptr = (ch | otherMark) & byteMask;
	ch >>= 6;
	/* FALLTHRU */
    case 1:
	*--ptr = (ch | firstMark[count]);
	break;
    }
    while (count--)
	_nc_outch(*ptr++);
    return OK;
}
#endif

NCURSES_EXPORT(int)
putp(const char *string)
{
    return tputs(string, 1, _nc_outch);
}

NCURSES_EXPORT(int)
tputs
(const char *string, int affcnt, int (*outc) (int))
{
    bool always_delay;
    bool normal_delay;
    int number;
#if BSD_TPUTS
    int trailpad;
#endif /* BSD_TPUTS */

#ifdef TRACE
    char addrbuf[32];

    if (_nc_tracing & TRACE_TPUTS) {
	if (outc == _nc_outch)
	    (void) strcpy(addrbuf, "_nc_outch");
	else
	    (void) sprintf(addrbuf, "%p", outc);
	if (_nc_tputs_trace) {
	    _tracef("tputs(%s = %s, %d, %s) called", _nc_tputs_trace,
		    _nc_visbuf(string), affcnt, addrbuf);
	} else {
	    _tracef("tputs(%s, %d, %s) called", _nc_visbuf(string), affcnt, addrbuf);
	}
	_nc_tputs_trace = (char *) NULL;
    }
#endif /* TRACE */

    if (!VALID_STRING(string))
	return ERR;

    if (cur_term == 0) {
	always_delay = FALSE;
	normal_delay = TRUE;
    } else {
	always_delay = (string == bell) || (string == flash_screen);
	normal_delay =
	    !xon_xoff
	    && padding_baud_rate
#if NCURSES_NO_PADDING
	    && (SP == 0 || !(SP->_no_padding))
#endif
	    && (_nc_baudrate(ospeed) >= padding_baud_rate);
    }

#if BSD_TPUTS
    /*
     * This ugly kluge deals with the fact that some ancient BSD programs
     * (like nethack) actually do the likes of tputs("50") to get delays.
     */
    trailpad = 0;
    if (isdigit(*string)) {
	while (isdigit(*string)) {
	    trailpad = trailpad * 10 + (*string - '0');
	    string++;
	}
	trailpad *= 10;
	if (*string == '.') {
	    string++;
	    if (isdigit(*string)) {
		trailpad += (*string - '0');
		string++;
	    }
	    while (isdigit(*string))
		string++;
	}

	if (*string == '*') {
	    trailpad *= affcnt;
	    string++;
	}
    }
#endif /* BSD_TPUTS */

    my_outch = outc;		/* redirect delay_output() */
    while (*string) {
	if (*string != '$')
	    (*outc) (*string);
	else {
	    string++;
	    if (*string != '<') {
		(*outc) ('$');
		if (*string)
		    (*outc) (*string);
	    } else {
		bool mandatory;

		string++;
		if ((!isdigit(CharOf(*string)) && *string != '.')
		    || !strchr(string, '>')) {
		    (*outc) ('$');
		    (*outc) ('<');
		    continue;
		}

		number = 0;
		while (isdigit(CharOf(*string))) {
		    number = number * 10 + (*string - '0');
		    string++;
		}
		number *= 10;
		if (*string == '.') {
		    string++;
		    if (isdigit(CharOf(*string))) {
			number += (*string - '0');
			string++;
		    }
		    while (isdigit(CharOf(*string)))
			string++;
		}

		mandatory = FALSE;
		while (*string == '*' || *string == '/') {
		    if (*string == '*') {
			number *= affcnt;
			string++;
		    } else {	/* if (*string == '/') */
			mandatory = TRUE;
			string++;
		    }
		}

		if (number > 0
		    && (always_delay
			|| normal_delay
			|| mandatory))
		    delay_output(number / 10);

	    }			/* endelse (*string == '<') */
	}			/* endelse (*string == '$') */

	if (*string == '\0')
	    break;

	string++;
    }

#if BSD_TPUTS
    /*
     * Emit any BSD-style prefix padding that we've accumulated now.
     */
    if (trailpad > 0
	&& (always_delay || normal_delay))
	delay_output(trailpad / 10);
#endif /* BSD_TPUTS */

    my_outch = _nc_outch;
    return OK;
}
