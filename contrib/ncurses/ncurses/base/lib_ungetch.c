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
**	lib_ungetch.c
**
**	The routine ungetch().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_ungetch.c,v 1.2 1998/02/11 12:13:56 tom Exp $")

#include <fifo_defs.h>

#ifdef TRACE
void _nc_fifo_dump(void)
{
int i;
	T(("head = %d, tail = %d, peek = %d", head, tail, peek));
	for (i = 0; i < 10; i++)
		T(("char %d = %s", i, _trace_key(SP->_fifo[i])));
}
#endif /* TRACE */

int ungetch(int ch)
{
	if (tail == -1)
		return ERR;
	if (head == -1) {
		head = 0;
		t_inc()
		peek = tail; /* no raw keys */
	} else
		h_dec();

	SP->_fifo[head] = ch;
	T(("ungetch %#x ok", ch));
#ifdef TRACE
	if (_nc_tracing & TRACE_IEVENT) _nc_fifo_dump();
#endif
	return OK;
}
