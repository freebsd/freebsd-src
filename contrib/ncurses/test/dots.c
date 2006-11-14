/****************************************************************************
 * Copyright (c) 1999-2001,2002 Free Software Foundation, Inc.              *
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

/*
 * Author: Thomas E. Dickey <dickey@clark.net> 1999
 *
 * $Id: dots.c,v 1.8 2002/04/06 21:33:42 tom Exp $
 *
 * A simple demo of the terminfo interface.
 */
#include <time.h>

#include <test.priv.h>

#define valid(s) ((s != 0) && s != (char *)-1)

static bool interrupted = FALSE;

static int
outc(int c)
{
    if (interrupted) {
	char tmp = c;
	write(STDOUT_FILENO, &tmp, 1);
    } else {
	putc(c, stdout);
    }
    return 0;
}

static bool
outs(char *s)
{
    if (valid(s)) {
	tputs(s, 1, outc);
	return TRUE;
    }
    return FALSE;
}

static void
cleanup(void)
{
    outs(exit_attribute_mode);
    if (!outs(orig_colors))
	outs(orig_pair);
    outs(clear_screen);
    outs(cursor_normal);
}

static void
onsig(int n GCC_UNUSED)
{
    interrupted = TRUE;
    cleanup();
    ExitProgram(EXIT_FAILURE);
}

static float
ranf(void)
{
    long r = (rand() & 077777);
    return ((float) r / 32768.);
}

int
main(
	int argc GCC_UNUSED,
	char *argv[]GCC_UNUSED)
{
    int x, y, z, j, p;
    float r;
    float c;

    for (j = SIGHUP; j <= SIGTERM; j++)
	if (signal(j, SIG_IGN) != SIG_IGN)
	    signal(j, onsig);

    srand(time(0));
    setupterm((char *) 0, 1, (int *) 0);
    outs(clear_screen);
    outs(cursor_invisible);
    if (max_colors > 1) {
	if (!valid(set_a_foreground)
	    || !valid(set_a_background)
	    || (!valid(orig_colors) && !valid(orig_pair)))
	    max_colors = -1;
    }

    r = (float) (lines - 4);
    c = (float) (columns - 4);

    for (;;) {
	x = (int) (c * ranf()) + 2;
	y = (int) (r * ranf()) + 2;
	p = (ranf() > 0.9) ? '*' : ' ';

	tputs(tparm3(cursor_address, y, x), 1, outc);
	if (max_colors > 0) {
	    z = (int) (ranf() * max_colors);
	    if (ranf() > 0.01) {
		tputs(tparm2(set_a_foreground, z), 1, outc);
	    } else {
		tputs(tparm2(set_a_background, z), 1, outc);
	    }
	} else if (valid(exit_attribute_mode)
		   && valid(enter_reverse_mode)) {
	    if (ranf() <= 0.01)
		outs((ranf() > 0.6) ? enter_reverse_mode :
		     exit_attribute_mode);
	}
	outc(p);
	fflush(stdout);
    }
}
