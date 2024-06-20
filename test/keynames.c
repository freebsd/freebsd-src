/****************************************************************************
 * Copyright 2020,2022 Thomas E. Dickey                                     *
 * Copyright 1998-2006,2008 Free Software Foundation, Inc.                  *
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
 * $Id: keynames.c,v 1.13 2022/12/04 00:40:11 tom Exp $
 */

#include <test.priv.h>

static void
usage(int ok)
{
    static const char *msg[] =
    {
	"Usage: keynames"
	,""
	,USAGE_COMMON
	,"Options:"
	," -m       call meta(TRUE) in initialization"
	," -s       call newterm, etc., to complete initialization"
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
    int n;
    bool do_setup = FALSE;
    bool do_meta = FALSE;

    setlocale(LC_ALL, "");

    while ((ch = getopt(argc, argv, OPTS_COMMON "ms")) != -1) {
	switch (ch) {
	case 'm':
	    do_meta = TRUE;
	    break;
	case 's':
	    do_setup = TRUE;
	    break;
	case OPTS_VERSION:
	    show_version(argv);
	    ExitProgram(EXIT_SUCCESS);
	default:
	    usage(ch == OPTS_USAGE);
	    /* NOTREACHED */
	}
    }

    if (do_setup) {
	/*
	 * Get the terminfo entry into memory, and tell ncurses that we want to
	 * use function keys.  That will make it add any user-defined keys that
	 * appear in the terminfo.
	 */
	newterm(getenv("TERM"), stderr, stdin);
	keypad(stdscr, TRUE);
	if (do_meta)
	    meta(stdscr, TRUE);
	endwin();
    }

    for (n = -1; n < KEY_MAX + 512; n++) {
	const char *result = keyname(n);
	if (result != 0)
	    printf("%d(%5o):%s\n", n, n, result);
    }
    ExitProgram(EXIT_SUCCESS);
}
