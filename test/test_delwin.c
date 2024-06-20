/****************************************************************************
 * Copyright 2022,2023 Thomas E. Dickey                                     *
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
 * $Id: test_delwin.c,v 1.5 2023/05/27 20:34:51 tom Exp $
 */
#include <test.priv.h>

#define STATUS 10

static SCREEN *my_screen;

static void
show_rc(const char *what, const char *explain, int rc)
{
    printw("%s : %d (%s)\n", what, rc, explain);
}

static void
next_step(WINDOW *win)
{
    int ch = wgetch(win);
    if (ch == QUIT || ch == ESCAPE) {
	endwin();
	/* use this to verify if delscreen frees all memory */
	delscreen(my_screen);
	exit(EXIT_FAILURE);
    }
}

static void
usage(int ok)
{
    static const char *msg[] =
    {
	"Usage: test_delwin [options]"
	,""
	,USAGE_COMMON
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
main(int argc, char **argv)
{
    WINDOW *parent, *child1;
    int rc;
    int ch;

    while ((ch = getopt(argc, argv, OPTS_COMMON)) != -1) {
	switch (ch) {
	case OPTS_VERSION:
	    show_version(argv);
	    ExitProgram(EXIT_SUCCESS);
	default:
	    usage(ch == OPTS_USAGE);
	    /* NOTREACHED */
	}
    }
    if (optind < argc)
	usage(FALSE);

    if ((my_screen = newterm(NULL, stdout, stdin)) == NULL)
	ExitProgram(EXIT_FAILURE);

    noecho();
    cbreak();

    refresh();
    wsetscrreg(stdscr, 0, STATUS - 1);
    scrollok(stdscr, TRUE);

    parent = newwin(0, 0, STATUS, 0);
    box(parent, 0, 0);
    wrefresh(parent);
    next_step(parent);

    printw("New window %p    %s\n", (void *) parent, "Top window");
    mvwprintw(parent, 1, 1, "Top window");
    wrefresh(parent);
    next_step(stdscr);

    child1 = derwin(parent, LINES - STATUS - 4, COLS - 4, 2, 2);
    box(child1, 0, 0);
    mvwprintw(child1, 1, 1, "Sub window");
    wrefresh(child1);

    printw("Sub window %p    %s\n", (void *) child1, "Hello world!");
    next_step(stdscr);

    show_rc("Deleted parent",
	    "should fail, it still has a subwindow",
	    delwin(parent));
    next_step(stdscr);
    show_rc("Deleted child1",
	    "should succeed",
	    rc = delwin(child1));
    next_step(stdscr);
    if (rc == OK) {
	wclrtobot(parent);
	box(parent, 0, 0);
	next_step(parent);
    }
    show_rc("Deleted parent",
	    "should succeed, it has no subwindow now",
	    rc = delwin(parent));
    next_step(stdscr);
    if (rc == OK) {
	touchwin(stdscr);
	next_step(stdscr);
    }
    show_rc("Deleted parent",
	    "should fail, may dump core",
	    delwin(parent));
    next_step(stdscr);
    endwin();
    ExitProgram(EXIT_SUCCESS);
}
