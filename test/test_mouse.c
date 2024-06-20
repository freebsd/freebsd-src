/****************************************************************************
 * Copyright 2022-2023,2024 Thomas E. Dickey                                *
 * Copyright 2022 Leonid S. Usov <leonid.s.usov at gmail.com>               *
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
 ****************************************************************************/
/*
 * $Id: test_mouse.c,v 1.31 2024/03/30 20:45:31 tom Exp $
 *
 * Author: Leonid S Usov
 *
 * Observe mouse events in the raw terminal or parsed ncurses modes
 */

#include <test.priv.h>

#if defined(NCURSES_MOUSE_VERSION) && !defined(_NC_WINDOWS)

static int logoffset = 0;

static void
raw_loop(void)
{
    char *xtermcap;

    printf("Entering raw mode. Ctrl-c to quit.\n");

    newterm(NULL, stdout, stdin);
    raw();
    xtermcap = tigetstr("XM");
    if (!VALID_STRING(xtermcap)) {
	fprintf(stderr, "couldn't get XM terminfo");
	return;
    }

    putp(tgoto(xtermcap, 1, 1));
    fflush(stdout);

    while (1) {
	int c = getc(stdin);
	const char *pretty;

	if (c == -1 || c == '\003') {
	    break;
	} else if (c == '\033') {
	    printf("\r\n\\E");
	} else if ((pretty = unctrl((chtype) c)) != NULL) {
	    printf("%s", pretty);
	} else if (isprint(c)) {
	    printf("%c", c);
	} else {
	    printf("{%x}", UChar(c));
	}
    }

    putp(tgoto(xtermcap, 0, 0));
    fflush(stdout);
    noraw();
}

static void logw(const char *fmt, ...) GCC_PRINTFLIKE(1, 2);

static void
logw(const char *fmt, ...)
{
    int row = getcury(stdscr);
    va_list args;

    va_start(args, fmt);
    wmove(stdscr, row++, 0);
    vw_printw(stdscr, fmt, args);
    va_end(args);

    clrtoeol();

    row %= (getmaxy(stdscr) - logoffset);
    if (row < logoffset) {
	row = logoffset;
    }

    wmove(stdscr, row, 0);
    wprintw(stdscr, ">");
    clrtoeol();
}

static void
cooked_loop(char *my_environ, int interval)
{
    MEVENT event;

    initscr();
    noecho();
    cbreak();			/* Line buffering disabled; pass everything */
    nonl();
    keypad(stdscr, TRUE);

    /* Get all the mouse events */
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(interval);

    logw("Ctrl-c to quit");
    logw("--------------");
    if (my_environ)
	logw("%s", my_environ);
    logoffset = getcury(stdscr);

    while (1) {
	int c = getch();

	switch (c) {
	case KEY_MOUSE:
	    if (getmouse(&event) == OK) {
		unsigned btn;
		mmask_t events;
#if NCURSES_MOUSE_VERSION > 1
		const unsigned max_btn = 5;
#else
		const unsigned max_btn = 4;
#endif
		const mmask_t btn_mask = (NCURSES_BUTTON_RELEASED |
					  NCURSES_BUTTON_PRESSED |
					  NCURSES_BUTTON_CLICKED |
					  NCURSES_DOUBLE_CLICKED |
					  NCURSES_TRIPLE_CLICKED);
		bool found = FALSE;
		for (btn = 1; btn <= max_btn; btn++) {
		    events = (mmask_t) (event.bstate
					& NCURSES_MOUSE_MASK(btn, btn_mask));
		    if (events == 0)
			continue;
#define ShowQ(btn,name) \
	(((event.bstate & NCURSES_MOUSE_MASK(btn, NCURSES_ ## name)) != 0) \
	 ? (" " #name) \
	 : "")
#define ShowM(name) \
	(((event.bstate & NCURSES_MOUSE_MASK(btn, BUTTON_ ## name)) != 0) \
	 ? (" " #name) \
	 : "")
#define ShowP() \
	 ((event.bstate & REPORT_MOUSE_POSITION) != 0 \
	  ? " position" \
	  : "")
		    logw("[%08lX] button %d%s%s%s%s%s%s%s%s%s @ %d, %d",
			 (unsigned long) events,
			 btn,
			 ShowQ(btn, BUTTON_RELEASED),
			 ShowQ(btn, BUTTON_PRESSED),
			 ShowQ(btn, BUTTON_CLICKED),
			 ShowQ(btn, DOUBLE_CLICKED),
			 ShowQ(btn, TRIPLE_CLICKED),
			 ShowM(SHIFT),
			 ShowM(CTRL),
			 ShowM(ALT),
			 ShowP(),
			 event.y, event.x);
		    found = TRUE;
		}
		/*
		 * A position report need not have a button associated with it.
		 * The modifiers probably are unused.
		 */
		if (!found && (event.bstate & REPORT_MOUSE_POSITION)) {
		    logw("[%08lX]%s%s%s%s @ %d, %d",
			 (unsigned long) events,
			 ShowM(SHIFT),
			 ShowM(CTRL),
			 ShowM(ALT),
			 ShowP(),
			 event.y, event.x);
		}
	    }
	    break;
	case '\003':
	    goto end;
	default:
	    logw("got another char: 0x%x", UChar(c));
	}
	refresh();
    }
  end:
    endwin();
}

static void
usage(int ok)
{
    static const char *msg[] =
    {
	"Usage: test_mouse [options]"
	,""
	,"Test mouse events.  These examples for $TERM demonstrate xterm"
	,"features:"
	,"    xterm"
	,"    xterm-1002"
	,"    xterm-1003"
	,""
	,USAGE_COMMON
	,"Options:"
	," -r       show raw input stream, injecting a new line before every ESC"
	," -i n     set mouse interval to n; default is 0 (no double-clicks)"
	," -T term  use terminal description other than $TERM"
    };
    unsigned n;
    for (n = 0; n < sizeof(msg) / sizeof(char *); ++n) {
	fprintf(stderr, "%s\n", msg[n]);
    }
    ExitProgram(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
/* *INDENT-OFF* */
VERSION_COMMON()
/* *INDENT-ON* */

int
main(int argc, char *argv[])
{
    bool rawmode = FALSE;
    int interval = 0;
    int ch;
    size_t my_len;
    char *my_environ = NULL;
    const char *term_format = "TERM=%s";

    while ((ch = getopt(argc, argv, OPTS_COMMON "i:rT:")) != -1) {
	switch (ch) {
	case 'i':
	    interval = atoi(optarg);
	    break;
	case 'r':
	    rawmode = TRUE;
	    break;
	case 'T':
	    my_len = strlen(term_format) + strlen(optarg) + 1;
	    my_environ = malloc(my_len);
	    if (my_environ != NULL) {
		_nc_SPRINTF(my_environ, _nc_SLIMIT(my_len) term_format, optarg);
		putenv(my_environ);
	    }
	    break;
	case OPTS_VERSION:
	    show_version(argv);
	    ExitProgram(EXIT_SUCCESS);
	default:
	    usage(ch == OPTS_USAGE);
	    /* NOTREACHED */
	}
    }
    if (optind < argc) {
	usage(FALSE);
	ExitProgram(EXIT_FAILURE);
    }

    if (rawmode) {
	raw_loop();
    } else {
	cooked_loop(my_environ, interval);
    }

    ExitProgram(EXIT_SUCCESS);
}
#else
int
main(void)
{
    printf("This program requires the ncurses library\n");
    ExitProgram(EXIT_FAILURE);
}
#endif
