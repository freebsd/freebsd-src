/****************************************************************************
 * Copyright 2023 Thomas E. Dickey                                          *
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
 * $Id: test_endwin.c,v 1.2 2023/11/10 15:17:19 tom Exp $
 */
#include <test.priv.h>

static void
usage(int ok)
{
    static const char *msg[] =
    {
	"Usage: test_endwin [options]"
	,""
	,"Options:"
	," -e   call endwin() an extra time"
	," -i   call initscr() before endwin()"
	," -n   call newterm() before endwin()"
	," -r   call refresh() before endwin()"
	," -s   call getch() after endwin(), to refresh screen"
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

#define status(opt,name,rc) if (opt) printf(" %s: %s", name, (rc) == OK ? "OK" : "ERR")

int
main(int argc, char *argv[])
{
    int ch;
    int rc_r = OK;
    int rc_e1 = OK;
    int rc_e2 = OK;
    int rc_e3 = OK;
    SCREEN *sp = NULL;
    bool opt_e = FALSE;
    bool opt_i = FALSE;
    bool opt_n = FALSE;
    bool opt_r = FALSE;
    bool opt_s = FALSE;

    while ((ch = getopt(argc, argv, "einrs" OPTS_COMMON)) != -1) {
	switch (ch) {
	case 'e':
	    opt_e = TRUE;
	    break;
	case 'i':
	    opt_i = TRUE;
	    break;
	case 'n':
	    opt_n = TRUE;
	    break;
	case 'r':
	    opt_r = TRUE;
	    break;
	case 's':
	    opt_s = TRUE;
	    break;
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
    if (opt_i && opt_n)
	usage(TRUE);

    if (opt_i) {
	initscr();
    } else if (opt_n) {
	sp = newterm(NULL, stdout, stdin);
    }
    if (opt_r) {
	rc_r = refresh();
    }
    rc_e1 = endwin();
    if (opt_e) {
	rc_e2 = endwin();
    }
    if (opt_s) {
	getch();
	rc_e3 = endwin();
    }
    printf("status:");
    status(opt_i, "initscr(-i)", OK);
    status(opt_n, "newterm(-n)", (sp != NULL) ? OK : ERR);
    status(opt_r, "refresh(-r)", rc_r);
    status(TRUE, "endwin", rc_e1);
    status(opt_e, "endwin(-e)", rc_e2);
    status(opt_s, "endwin(-s)", rc_e3);
    printf("\n");
    ExitProgram(EXIT_SUCCESS);
}
