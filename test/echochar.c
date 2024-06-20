/****************************************************************************
 * Copyright 2019-2022,2023 Thomas E. Dickey                                *
 * Copyright 2006-2014,2017 Free Software Foundation, Inc.                  *
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
 * $Id: echochar.c,v 1.26 2023/05/27 20:13:10 tom Exp $
 *
 * Demonstrate the echochar function (compare to dots.c).
 * Thomas Dickey - 2006/11/4
 */

#include <test.priv.h>

#include <time.h>

static bool interrupted = FALSE;
static long total_chars = 0;
static time_t started;

static void
cleanup(void)
{
    stop_curses();

    printf("\n\n%ld total cells, rate %.2f/sec\n",
	   total_chars,
	   ((double) (total_chars) / (double) (time((time_t *) 0) - started)));
}

static void
onsig(int n GCC_UNUSED)
{
    interrupted = TRUE;
}

static double
ranf(void)
{
    long r = (rand() & 077777);
    return ((double) r / 32768.);
}

static void
set_color(const char *const my_pairs, int fg, int bg)
{
    int pair = (fg * COLORS) + bg;
    if (pair < COLOR_PAIRS) {
	if (!my_pairs[pair]) {
	    init_pair((short) pair,
		      (short) fg,
		      (short) bg);
	}
	attron(COLOR_PAIR(pair));
    }
}

static void
usage(int ok)
{
    static const char *msg[] =
    {
	"Usage: echochar"
	,""
	,USAGE_COMMON
	,"Options:"
	," -r       use addch/refresh rather than echochar()"
    };
    size_t n;

    for (n = 0; n < SIZEOF(msg); n++)
	fprintf(stderr, "%s\n", msg[n]);

    ExitProgram(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
/* *INDENT-OFF* */
VERSION_COMMON()
/* *INDENT-ON* */

int
main(int argc, char *argv[])
{
    int ch;
    double r;
    double c;
    bool use_colors;
    bool opt_r = FALSE;
    char *my_pairs = 0;
    int last_fg = 0;
    int last_bg = 0;

    while ((ch = getopt(argc, argv, OPTS_COMMON "r")) != -1) {
	switch (ch) {
	case 'r':
	    opt_r = TRUE;
	    break;
	case OPTS_VERSION:
	    show_version(argv);
	    ExitProgram(EXIT_SUCCESS);
	default:
	    usage(ch == OPTS_USAGE);
	    /* NOTREACHED */
	}
    }

    InitAndCatch(initscr(), onsig);

    use_colors = has_colors();
    if (use_colors) {
	start_color();
	if (COLOR_PAIRS > 0) {
	    my_pairs = typeCalloc(char, (size_t) COLOR_PAIRS);
	}
	use_colors = (my_pairs != 0);
    }

    srand((unsigned) time(0));

    curs_set(0);

    r = (double) (LINES - 4);
    c = (double) (COLS - 4);
    started = time((time_t *) 0);

    while (!interrupted) {
	int x = (int) (c * ranf()) + 2;
	int y = (int) (r * ranf()) + 2;
	int p = (ranf() > 0.9) ? '*' : ' ';

	move(y, x);
	if (use_colors > 0) {
	    int z = (int) (ranf() * COLORS);
	    if (ranf() > 0.01) {
		set_color(my_pairs, z, last_bg);
		last_fg = z;
	    } else {
		set_color(my_pairs, last_fg, z);
		last_bg = z;
		napms(1);
	    }
	} else {
	    if (ranf() <= 0.01) {
		if (ranf() > 0.6)
		    attron(A_REVERSE);
		else
		    attroff(A_REVERSE);
		napms(1);
	    }
	}
	if (opt_r) {
	    AddCh(UChar(p));
	    refresh();
	} else {
	    echochar(UChar(p));
	}
	++total_chars;
    }
    cleanup();
    free(my_pairs);
    ExitProgram(EXIT_SUCCESS);
}
