/****************************************************************************
 * Copyright (c) 1999 Free Software Foundation, Inc.                        *
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
 *  Author: Thomas E. Dickey <dickey@clark.net> 1999                        *
 ****************************************************************************/
/*
 *	trace_tries.c - Tracing/Debugging buffers (keycode tries-trees)
 */

#include <curses.priv.h>

MODULE_ID("$Id: trace_tries.c,v 1.6 1999/03/06 22:51:07 tom Exp $")

#ifdef TRACE
static unsigned char *buffer;
static unsigned len;

static void recur_tries(struct tries *tree, unsigned level)
{
	if (level > len)
		buffer = (unsigned char *)realloc(buffer, len = (level + 1) * 4);

	while (tree != 0) {
		if ((buffer[level] = tree->ch) == 0)
			buffer[level] = 128;
		buffer[level+1] = 0;
		if (tree->value != 0) {
			_tracef("%5d: %s (%s)", tree->value, _nc_visbuf((char *)buffer), keyname(tree->value));
		}
		if (tree->child)
			recur_tries(tree->child, level+1);
		tree = tree->sibling;
	}
}

void _nc_trace_tries(struct tries *tree)
{
	buffer = typeMalloc(unsigned char, len = 80);
	_tracef("BEGIN tries %p", tree);
	recur_tries(tree, 0);
	_tracef(". . . tries %p", tree);
	free(buffer);
}
#else
void _nc_trace_tries(struct tries *tree GCC_UNUSED)
{
}
#endif
