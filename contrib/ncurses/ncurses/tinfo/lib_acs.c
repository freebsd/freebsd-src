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
#include <term.h>	/* ena_acs, acs_chars */

MODULE_ID("$Id: lib_acs.c,v 1.15 1999/02/18 11:31:43 tom Exp $")

chtype acs_map[ACS_LEN];

void _nc_init_acs(void)
{
	T(("initializing ACS map"));

	/*
	 * Initializations for a UNIX-like multi-terminal environment.  Use
	 * ASCII chars and count on the terminfo description to do better.
	 */
	ACS_ULCORNER = '+';	/* should be upper left corner */
	ACS_LLCORNER = '+';	/* should be lower left corner */
	ACS_URCORNER = '+';	/* should be upper right corner */
	ACS_LRCORNER = '+';	/* should be lower right corner */
	ACS_RTEE     = '+';	/* should be tee pointing left */
	ACS_LTEE     = '+';	/* should be tee pointing right */
	ACS_BTEE     = '+';	/* should be tee pointing up */
	ACS_TTEE     = '+';	/* should be tee pointing down */
	ACS_HLINE    = '-';	/* should be horizontal line */
	ACS_VLINE    = '|';	/* should be vertical line */
	ACS_PLUS     = '+';	/* should be large plus or crossover */
	ACS_S1       = '~';	/* should be scan line 1 */
	ACS_S9       = '_';	/* should be scan line 9 */
	ACS_DIAMOND  = '+';	/* should be diamond */
	ACS_CKBOARD  = ':';	/* should be checker board (stipple) */
	ACS_DEGREE   = '\'';	/* should be degree symbol */
	ACS_PLMINUS  = '#';	/* should be plus/minus */
	ACS_BULLET   = 'o';	/* should be bullet */
	ACS_LARROW   = '<';	/* should be arrow pointing left */
	ACS_RARROW   = '>';	/* should be arrow pointing right */
	ACS_DARROW   = 'v';	/* should be arrow pointing down */
	ACS_UARROW   = '^';	/* should be arrow pointing up */
	ACS_BOARD    = '#';	/* should be board of squares */
	ACS_LANTERN  = '#';	/* should be lantern symbol */
	ACS_BLOCK    = '#';	/* should be solid square block */
	/* these defaults were invented for ncurses */
	ACS_S3       = '-';	/* should be scan line 3 */
	ACS_S7       = '-';	/* should be scan line 7 */
	ACS_LEQUAL   = '<';	/* should be less-than-or-equal-to */
	ACS_GEQUAL   = '>';	/* should be greater-than-or-equal-to */
	ACS_PI       = '*';	/* should be greek pi */
        ACS_NEQUAL   = '!';	/* should be not-equal */
        ACS_STERLING = 'f';	/* should be pound-sterling symbol */

	if (ena_acs != NULL)
	{
		TPUTS_TRACE("ena_acs");
		putp(ena_acs);
	}

#define ALTCHAR(c)	((chtype)(((unsigned char)(c)) | A_ALTCHARSET))

	if (acs_chars != NULL) {
	    size_t i = 0;
	    size_t length = strlen(acs_chars);

		while (i < length)
			switch (acs_chars[i]) {
			case 'l':case 'm':case 'k':case 'j':
			case 'u':case 't':case 'v':case 'w':
			case 'q':case 'x':case 'n':case 'o':
			case 's':case '`':case 'a':case 'f':
			case 'g':case '~':case ',':case '+':
			case '.':case '-':case 'h':case 'i':
			case '0':case 'p':case 'r':case 'y':
			case 'z':case '{':case '|':case '}':
				acs_map[(unsigned int)acs_chars[i]] =
					ALTCHAR(acs_chars[i+1]);
				i++;
				/* FALLTHRU */
			default:
				i++;
				break;
			}
	}
#ifdef TRACE
	/* Show the equivalent mapping, noting if it does not match the
	 * given attribute, whether by re-ordering or duplication.
	 */
	if (_nc_tracing & TRACE_CALLS) {
		size_t n, m;
		char show[SIZEOF(acs_map) + 1];
		for (n = 1, m = 0; n < SIZEOF(acs_map); n++) {
			if (acs_map[n] != 0) {
				show[m++] = (char)n;
				show[m++] = TextOf(acs_map[n]);
			}
		}
		show[m] = 0;
		_tracef("%s acs_chars %s",
			(acs_chars == NULL)
			? "NULL"
			: (strcmp(acs_chars, show)
				? "DIFF"
				: "SAME"),
			_nc_visbuf(show));
	}
#endif /* TRACE */
}
