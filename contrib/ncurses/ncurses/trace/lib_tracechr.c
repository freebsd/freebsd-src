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
 *	lib_tracechr.c - Tracing/Debugging routines
 */

#ifndef TRACE
#define TRACE			/* turn on internal defs for this module */
#endif

#include <curses.priv.h>

#include <ctype.h>

#ifdef TRACE
char *_tracechar(const unsigned char ch)
{
    static char crep[20];
    /* 
     * We can show the actual character if it's either an ordinary printable
     * or one of the high-half characters.
     */
    if (isprint(ch) || (ch & 0x80))
    {
	crep[0] = '\'';
	crep[1] = ch;	/* necessary; printf tries too hard on metachars */
	(void) sprintf(crep + 2, "' = 0x%02x", (unsigned)ch);
    }
    else
	(void) sprintf(crep, "0x%02x", (unsigned)ch);
    return(crep);
}
#else
extern	void _nc_lib_tracechr(void);
	void _nc_lib_tracechr(void) { }
#endif
