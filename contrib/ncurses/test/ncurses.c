/****************************************************************************
 * Copyright (c) 1998-2001,2002 Free Software Foundation, Inc.              *
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

NAME
   ncurses.c --- ncurses library exerciser

SYNOPSIS
   ncurses

DESCRIPTION
   An interactive test module for the ncurses library.

AUTHOR
   Author: Eric S. Raymond <esr@snark.thyrsus.com> 1993
           Thomas E. Dickey (beginning revision 1.27 in 1996).

$Id: ncurses.c,v 1.173 2002/06/16 00:29:27 tom Exp $

***************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include <test.priv.h>

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#if HAVE_GETTIMEOFDAY
#if HAVE_SYS_TIME_H && HAVE_SYS_TIME_SELECT
#include <sys/time.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif

#if HAVE_PANEL_H && HAVE_LIBPANEL
#define USE_LIBPANEL 1
#include <panel.h>
#else
#define USE_LIBPANEL 0
#endif

#if HAVE_MENU_H && HAVE_LIBMENU
#define USE_LIBMENU 1
#include <menu.h>
#else
#define USE_LIBMENU 0
#endif

#if HAVE_FORM_H && HAVE_LIBFORM
#define USE_LIBFORM 1
#include <form.h>
#else
#define USE_LIBFORM 0
#endif

#ifdef NCURSES_VERSION

#ifdef TRACE
static int save_trace = TRACE_ORDINARY | TRACE_CALLS;
extern int _nc_tracing;
#endif

#else

#define mmask_t chtype		/* not specified in XSI */

#ifdef CURSES_ACS_ARRAY
#define ACS_S3          (CURSES_ACS_ARRAY['p'])		/* scan line 3 */
#define ACS_S7          (CURSES_ACS_ARRAY['r'])		/* scan line 7 */
#define ACS_LEQUAL      (CURSES_ACS_ARRAY['y'])		/* less/equal */
#define ACS_GEQUAL      (CURSES_ACS_ARRAY['z'])		/* greater/equal */
#define ACS_PI          (CURSES_ACS_ARRAY['{'])		/* Pi */
#define ACS_NEQUAL      (CURSES_ACS_ARRAY['|'])		/* not equal */
#define ACS_STERLING    (CURSES_ACS_ARRAY['}'])		/* UK pound sign */
#else
#define ACS_S3          (A_ALTCHARSET + 'p')	/* scan line 3 */
#define ACS_S7          (A_ALTCHARSET + 'r')	/* scan line 7 */
#define ACS_LEQUAL      (A_ALTCHARSET + 'y')	/* less/equal */
#define ACS_GEQUAL      (A_ALTCHARSET + 'z')	/* greater/equal */
#define ACS_PI          (A_ALTCHARSET + '{')	/* Pi */
#define ACS_NEQUAL      (A_ALTCHARSET + '|')	/* not equal */
#define ACS_STERLING    (A_ALTCHARSET + '}')	/* UK pound sign */
#endif

#ifdef CURSES_WACS_ARRAY
#define WACS_S3         (&(CURSES_WACS_ARRAY['p']))	/* scan line 3 */
#define WACS_S7         (&(CURSES_WACS_ARRAY['r']))	/* scan line 7 */
#define WACS_LEQUAL     (&(CURSES_WACS_ARRAY['y']))	/* less/equal */
#define WACS_GEQUAL     (&(CURSES_WACS_ARRAY['z']))	/* greater/equal */
#define WACS_PI         (&(CURSES_WACS_ARRAY['{']))	/* Pi */
#define WACS_NEQUAL     (&(CURSES_WACS_ARRAY['|']))	/* not equal */
#define WACS_STERLING   (&(CURSES_WACS_ARRAY['}']))	/* UK pound sign */
#endif

#endif

#define P(string)	printw("%s\n", string)
#ifdef CTRL
#undef CTRL
#endif
#define CTRL(x)		((x) & 0x1f)

#define QUIT		CTRL('Q')
#define ESCAPE		CTRL('[')
#define BLANK		' '	/* this is the background character */

#undef max_colors
static int max_colors;		/* the actual number of colors we'll use */

#undef max_pairs
static int max_pairs;		/* ...and the number of color pairs */

/* The behavior of mvhline, mvvline for negative/zero length is unspecified,
 * though we can rely on negative x/y values to stop the macro.
 */
static void
do_h_line(int y, int x, chtype c, int to)
{
    if ((to) > (x))
	mvhline(y, x, c, (to) - (x));
}

static void
do_v_line(int y, int x, chtype c, int to)
{
    if ((to) > (y))
	mvvline(y, x, c, (to) - (y));
}

/* Common function to allow ^T to toggle trace-mode in the middle of a test
 * so that trace-files can be made smaller.
 */
static int
wGetchar(WINDOW *win)
{
    int c;
#ifdef TRACE
    while ((c = wgetch(win)) == CTRL('T')) {
	if (_nc_tracing) {
	    save_trace = _nc_tracing;
	    _tracef("TOGGLE-TRACING OFF");
	    _nc_tracing = 0;
	} else {
	    _nc_tracing = save_trace;
	}
	trace(_nc_tracing);
	if (_nc_tracing)
	    _tracef("TOGGLE-TRACING ON");
    }
#else
    c = wgetch(win);
#endif
    return c;
}
#define Getchar() wGetchar(stdscr)

#if USE_WIDEC_SUPPORT
static int
wGet_wchar(WINDOW *win, wint_t * result)
{
    int c;
#ifdef TRACE
    while ((c = wget_wch(win, result)) == CTRL('T')) {
	if (_nc_tracing) {
	    save_trace = _nc_tracing;
	    _tracef("TOGGLE-TRACING OFF");
	    _nc_tracing = 0;
	} else {
	    _nc_tracing = save_trace;
	}
	trace(_nc_tracing);
	if (_nc_tracing)
	    _tracef("TOGGLE-TRACING ON");
    }
#else
    c = wget_wch(win, result);
#endif
    return c;
}
#define Get_wchar(result) wGet_wchar(stdscr, result)

#endif

static void
Pause(void)
{
    move(LINES - 1, 0);
    addstr("Press any key to continue... ");
    (void) Getchar();
}

static void
Cannot(const char *what)
{
    printw("\nThis %s terminal %s\n\n", getenv("TERM"), what);
    Pause();
}

static void
ShellOut(bool message)
{
    if (message)
	addstr("Shelling out...");
    def_prog_mode();
    endwin();
    system("sh");
    if (message)
	addstr("returned from shellout.\n");
    refresh();
}

#ifdef NCURSES_MOUSE_VERSION
static const char *
mouse_decode(MEVENT const *ep)
{
    static char buf[80];

    (void) sprintf(buf, "id %2d  at (%2d, %2d, %2d) state %4lx = {",
		   ep->id, ep->x, ep->y, ep->z, ep->bstate);

#define SHOW(m, s) if ((ep->bstate & m)==m) {strcat(buf,s); strcat(buf, ", ");}
    SHOW(BUTTON1_RELEASED, "release-1");
    SHOW(BUTTON1_PRESSED, "press-1");
    SHOW(BUTTON1_CLICKED, "click-1");
    SHOW(BUTTON1_DOUBLE_CLICKED, "doubleclick-1");
    SHOW(BUTTON1_TRIPLE_CLICKED, "tripleclick-1");
    SHOW(BUTTON1_RESERVED_EVENT, "reserved-1");
    SHOW(BUTTON2_RELEASED, "release-2");
    SHOW(BUTTON2_PRESSED, "press-2");
    SHOW(BUTTON2_CLICKED, "click-2");
    SHOW(BUTTON2_DOUBLE_CLICKED, "doubleclick-2");
    SHOW(BUTTON2_TRIPLE_CLICKED, "tripleclick-2");
    SHOW(BUTTON2_RESERVED_EVENT, "reserved-2");
    SHOW(BUTTON3_RELEASED, "release-3");
    SHOW(BUTTON3_PRESSED, "press-3");
    SHOW(BUTTON3_CLICKED, "click-3");
    SHOW(BUTTON3_DOUBLE_CLICKED, "doubleclick-3");
    SHOW(BUTTON3_TRIPLE_CLICKED, "tripleclick-3");
    SHOW(BUTTON3_RESERVED_EVENT, "reserved-3");
    SHOW(BUTTON4_RELEASED, "release-4");
    SHOW(BUTTON4_PRESSED, "press-4");
    SHOW(BUTTON4_CLICKED, "click-4");
    SHOW(BUTTON4_DOUBLE_CLICKED, "doubleclick-4");
    SHOW(BUTTON4_TRIPLE_CLICKED, "tripleclick-4");
    SHOW(BUTTON4_RESERVED_EVENT, "reserved-4");
    SHOW(BUTTON_CTRL, "ctrl");
    SHOW(BUTTON_SHIFT, "shift");
    SHOW(BUTTON_ALT, "alt");
    SHOW(ALL_MOUSE_EVENTS, "all-events");
    SHOW(REPORT_MOUSE_POSITION, "position");
#undef SHOW

    if (buf[strlen(buf) - 1] == ' ')
	buf[strlen(buf) - 2] = '\0';
    (void) strcat(buf, "}");
    return (buf);
}
#endif /* NCURSES_MOUSE_VERSION */

/****************************************************************************
 *
 * Character input test
 *
 ****************************************************************************/

static void
setup_getch(WINDOW *win, bool flags[])
{
    keypad(win, flags['k']);	/* should be redundant, but for testing */
    meta(win, flags['m']);	/* force this to a known state */
    if (flags['e'])
	echo();
    else
	noecho();
}

static void
wgetch_help(WINDOW *win, bool flags[])
{
    static const char *help[] =
    {
	"e -- toggle echo mode"
	,"g -- triggers a getstr test"
	,"k -- toggle keypad/literal mode"
	,"m -- toggle meta (7-bit/8-bit) mode"
	,"q -- quit (x also exits)"
	,"s -- shell out\n"
	,"w -- create a new window"
#ifdef SIGTSTP
	,"z -- suspend this process"
#endif
    };
    int y, x;
    unsigned chk = ((SIZEOF(help) + 1) / 2);
    unsigned n;

    getyx(win, y, x);
    move(0, 0);
    printw("Type any key to see its %s value.  Also:\n",
	   flags['k'] ? "keypad" : "literal");
    for (n = 0; n < SIZEOF(help); ++n) {
	int row = 1 + (n % chk);
	int col = (n >= chk) ? COLS / 2 : 0;
	int flg = ((strstr(help[n], "toggle") != 0)
		   && (flags[UChar(*help[n])] != FALSE));
	if (flg)
	    standout();
	mvprintw(row, col, "%s", help[n]);
	if (col == 0)
	    clrtoeol();
	if (flg)
	    standend();
    }
    wrefresh(stdscr);
    wmove(win, y, x);
}

static void
wgetch_wrap(WINDOW *win, int first_y)
{
    int last_y = getmaxy(win) - 1;
    int y = getcury(win) + 1;

    if (y >= last_y)
	y = first_y;
    wmove(win, y, 0);
    wclrtoeol(win);
}

#if defined(NCURSES_VERSION) && defined(KEY_RESIZE)
typedef struct {
    WINDOW *text;
    WINDOW *frame;
} WINSTACK;

static WINSTACK *winstack = 0;
static unsigned len_winstack = 0;

static void
remember_boxes(unsigned level, WINDOW *txt_win, WINDOW *box_win)
{
    unsigned need = (level + 1) * 2;

    if (winstack == 0) {
	len_winstack = 20;
	winstack = malloc(len_winstack * sizeof(WINSTACK));
    } else if (need >= len_winstack) {
	len_winstack = need;
	winstack = realloc(winstack, len_winstack * sizeof(WINSTACK));
    }
    winstack[level].text = txt_win;
    winstack[level].frame = box_win;
}

/*
 * For wgetch_test(), we create pairs of windows - one for a box, one for text.
 * Resize both and paint the box in the parent.
 */
static void
resize_boxes(int level, WINDOW *win)
{
    unsigned n;
    int base = 5;
    int high = LINES - base;
    int wide = COLS;

    touchwin(stdscr);
    wnoutrefresh(stdscr);

    /* FIXME: this chunk should be done in resizeterm() */
    slk_touch();
    slk_clear();
    slk_noutrefresh();

    for (n = 0; (int) n < level; ++n) {
	wresize(winstack[n].frame, high, wide);
	wresize(winstack[n].text, high - 2, wide - 2);
	high -= 2;
	wide -= 2;
	werase(winstack[n].text);
	box(winstack[n].frame, 0, 0);
	wnoutrefresh(winstack[n].frame);
	wprintw(winstack[n].text,
		"size %dx%d\n",
		getmaxy(winstack[n].text),
		getmaxx(winstack[n].text));
	wnoutrefresh(winstack[n].text);
	if (winstack[n].text == win)
	    break;
    }
    doupdate();
}
#else
#define remember_boxes(level,text,frame)	/* nothing */
#endif

static void
wgetch_test(int level, WINDOW *win, int delay)
{
    char buf[BUFSIZ];
    int first_y, first_x;
    int c;
    int incount = 0;
    bool flags[256];
    bool blocking = (delay < 0);
    int y, x;

    memset(flags, FALSE, sizeof(flags));
    flags['k'] = (win == stdscr);

    setup_getch(win, flags);
    wtimeout(win, delay);
    getyx(win, first_y, first_x);

    wgetch_help(win, flags);
    wsetscrreg(win, first_y, getmaxy(win) - 1);
    scrollok(win, TRUE);

    for (;;) {
	while ((c = wGetchar(win)) == ERR) {
	    incount++;
	    if (blocking) {
		(void) wprintw(win, "%05d: input error", incount);
		break;
	    } else {
		(void) wprintw(win, "%05d: input timed out", incount);
	    }
	    wgetch_wrap(win, first_y);
	}
	if (c == ERR && blocking) {
	    wprintw(win, "ERR");
	    wgetch_wrap(win, first_y);
	} else if (c == 'x' || c == 'q') {
	    break;
	} else if (c == 'e') {
	    flags['e'] = !flags['e'];
	    setup_getch(win, flags);
	    wgetch_help(win, flags);
	} else if (c == 'g') {
	    waddstr(win, "getstr test: ");
	    echo();
	    wgetnstr(win, buf, sizeof(buf) - 1);
	    noecho();
	    wprintw(win, "I saw %d characters:\n\t`%s'.", (int) strlen(buf), buf);
	    wclrtoeol(win);
	    wgetch_wrap(win, first_y);
	} else if (c == 'k') {
	    flags['k'] = !flags['k'];
	    setup_getch(win, flags);
	    wgetch_help(win, flags);
	} else if (c == 'm') {
	    flags['m'] = !flags['m'];
	    setup_getch(win, flags);
	    wgetch_help(win, flags);
	} else if (c == 's') {
	    ShellOut(TRUE);
	} else if (c == 'w') {
	    int high = getmaxy(win) - 1 - first_y + 1;
	    int wide = getmaxx(win) - first_x;
	    int old_y, old_x;
	    int new_y = first_y + getbegy(win);
	    int new_x = first_x + getbegx(win);

	    getyx(win, old_y, old_x);
	    if (high > 2 && wide > 2) {
		WINDOW *wb = newwin(high, wide, new_y, new_x);
		WINDOW *wi = newwin(high - 2, wide - 2, new_y + 1, new_x + 1);

		box(wb, 0, 0);
		wrefresh(wb);
		wmove(wi, 0, 0);
		remember_boxes(level, wi, wb);
		wgetch_test(level + 1, wi, delay);
		delwin(wi);
		delwin(wb);

		wgetch_help(win, flags);
		wmove(win, old_y, old_x);
		touchwin(win);
		wrefresh(win);
		doupdate();
	    }
#ifdef SIGTSTP
	} else if (c == 'z') {
	    kill(getpid(), SIGTSTP);
#endif
	} else {
	    wprintw(win, "Key pressed: %04o ", c);
#ifdef NCURSES_MOUSE_VERSION
	    if (c == KEY_MOUSE) {
		MEVENT event;

		getmouse(&event);
		wprintw(win, "KEY_MOUSE, %s", mouse_decode(&event));
		getyx(win, y, x);
		move(event.y, event.x);
		addch('*');
		wmove(win, y, x);
	    } else
#endif /* NCURSES_MOUSE_VERSION */
	    if (c >= KEY_MIN) {
#if defined(NCURSES_VERSION) && defined(KEY_RESIZE)
		if (c == KEY_RESIZE) {
		    resize_boxes(level, win);
		}
#endif
		(void) waddstr(win, keyname(c));
	    } else if (c > 0x80) {
		int c2 = (c & 0x7f);
		if (isprint(c2))
		    (void) wprintw(win, "M-%c", c2);
		else
		    (void) wprintw(win, "M-%s", unctrl(c2));
		waddstr(win, " (high-half character)");
	    } else {
		if (isprint(c))
		    (void) wprintw(win, "%c (ASCII printable character)", c);
		else
		    (void) wprintw(win, "%s (ASCII control character)",
				   unctrl(c));
	    }
	    wgetch_wrap(win, first_y);
	}
    }

    wtimeout(win, -1);
}

static int
begin_getch_test(void)
{
    char buf[BUFSIZ];
    int delay;

    refresh();

#ifdef NCURSES_MOUSE_VERSION
    mousemask(ALL_MOUSE_EVENTS, (mmask_t *) 0);
#endif

    (void) printw("Delay in 10ths of a second (<CR> for blocking input)? ");
    echo();
    getnstr(buf, sizeof(buf) - 1);
    noecho();
    nonl();

    if (isdigit(UChar(buf[0]))) {
	delay = atoi(buf) * 100;
    } else {
	delay = -1;
    }
    raw();
    move(5, 0);
    return delay;
}

static void
finish_getch_test(void)
{
#ifdef NCURSES_MOUSE_VERSION
    mousemask(0, (mmask_t *) 0);
#endif
    erase();
    noraw();
    nl();
    endwin();
}

static void
getch_test(void)
{
    int delay = begin_getch_test();
    wgetch_test(0, stdscr, delay);
    finish_getch_test();
}

#if USE_WIDEC_SUPPORT
/*
 * For wgetch_test(), we create pairs of windows - one for a box, one for text.
 * Resize both and paint the box in the parent.
 */
static void
resize_wide_boxes(int level, WINDOW *win)
{
    unsigned n;
    int base = 5;
    int high = LINES - base;
    int wide = COLS;

    touchwin(stdscr);
    wnoutrefresh(stdscr);

    /* FIXME: this chunk should be done in resizeterm() */
    slk_touch();
    slk_clear();
    slk_noutrefresh();

    for (n = 0; (int) n < level; ++n) {
	wresize(winstack[n].frame, high, wide);
	wresize(winstack[n].text, high - 2, wide - 2);
	high -= 2;
	wide -= 2;
	werase(winstack[n].text);
	box_set(winstack[n].frame, 0, 0);
	wnoutrefresh(winstack[n].frame);
	wprintw(winstack[n].text,
		"size %dx%d\n",
		getmaxy(winstack[n].text),
		getmaxx(winstack[n].text));
	wnoutrefresh(winstack[n].text);
	if (winstack[n].text == win)
	    break;
    }
    doupdate();
}

static void
wget_wch_test(int level, WINDOW *win, int delay)
{
    char buf[BUFSIZ];
    int first_y, first_x;
    wint_t c;
    int incount = 0;
    bool flags[256];
    bool blocking = (delay < 0);
    int y, x, code;

    memset(flags, FALSE, sizeof(flags));
    flags['k'] = (win == stdscr);

    setup_getch(win, flags);
    wtimeout(win, delay);
    getyx(win, first_y, first_x);

    wgetch_help(win, flags);
    wsetscrreg(win, first_y, getmaxy(win) - 1);
    scrollok(win, TRUE);

    for (;;) {
	while ((code = wGet_wchar(win, &c)) == ERR) {
	    incount++;
	    if (blocking) {
		(void) wprintw(win, "%05d: input error", incount);
		break;
	    } else {
		(void) wprintw(win, "%05d: input timed out", incount);
	    }
	    wgetch_wrap(win, first_y);
	}
	if (code == ERR && blocking) {
	    wprintw(win, "ERR");
	    wgetch_wrap(win, first_y);
	} else if (c == 'x' || c == 'q') {
	    break;
	} else if (c == 'e') {
	    flags['e'] = !flags['e'];
	    setup_getch(win, flags);
	    wgetch_help(win, flags);
	} else if (c == 'g') {
	    waddstr(win, "getstr test: ");
	    echo();
	    wgetnstr(win, buf, sizeof(buf) - 1);
	    noecho();
	    wprintw(win, "I saw %d characters:\n\t`%s'.", strlen(buf), buf);
	    wclrtoeol(win);
	    wgetch_wrap(win, first_y);
	} else if (c == 'k') {
	    flags['k'] = !flags['k'];
	    setup_getch(win, flags);
	    wgetch_help(win, flags);
	} else if (c == 'm') {
	    flags['m'] = !flags['m'];
	    setup_getch(win, flags);
	    wgetch_help(win, flags);
	} else if (c == 's') {
	    ShellOut(TRUE);
	} else if (c == 'w') {
	    int high = getmaxy(win) - 1 - first_y + 1;
	    int wide = getmaxx(win) - first_x;
	    int old_y, old_x;
	    int new_y = first_y + getbegy(win);
	    int new_x = first_x + getbegx(win);

	    getyx(win, old_y, old_x);
	    if (high > 2 && wide > 2) {
		WINDOW *wb = newwin(high, wide, new_y, new_x);
		WINDOW *wi = newwin(high - 2, wide - 2, new_y + 1, new_x + 1);

		box_set(wb, 0, 0);
		wrefresh(wb);
		wmove(wi, 0, 0);
		remember_boxes(level, wi, wb);
		wget_wch_test(level + 1, wi, delay);
		delwin(wi);
		delwin(wb);

		wgetch_help(win, flags);
		wmove(win, old_y, old_x);
		touchwin(win);
		wrefresh(win);
	    }
#ifdef SIGTSTP
	} else if (c == 'z') {
	    kill(getpid(), SIGTSTP);
#endif
	} else {
	    wprintw(win, "Key pressed: %04o ", c);
#ifdef NCURSES_MOUSE_VERSION
	    if (c == KEY_MOUSE) {
		MEVENT event;

		getmouse(&event);
		wprintw(win, "KEY_MOUSE, %s", mouse_decode(&event));
		getyx(win, y, x);
		move(event.y, event.x);
		addch('*');
		wmove(win, y, x);
	    } else
#endif /* NCURSES_MOUSE_VERSION */
	    if (code == KEY_CODE_YES) {
#ifdef KEY_RESIZE
		if (c == KEY_RESIZE) {
		    resize_wide_boxes(level, win);
		}
#endif
		(void) waddstr(win, key_name(c));
	    } else {
		if (c < 256 && iscntrl(c)) {
		    (void) wprintw(win, "%s (control character)", unctrl(c));
		} else {
		    wchar_t c2 = c;
		    waddnwstr(win, &c2, 1);
		    (void) wprintw(win, " = %#x (printable character)", c);
		}
	    }
	    wgetch_wrap(win, first_y);
	}
    }

    wtimeout(win, -1);
}

static void
get_wch_test(void)
{
    int delay = begin_getch_test();
    wget_wch_test(0, stdscr, delay);
    finish_getch_test();
}
#endif

/****************************************************************************
 *
 * Character attributes test
 *
 ****************************************************************************/

static int
show_attr(int row, int skip, chtype attr, const char *name)
{
    static const char *string = "abcde fghij klmno pqrst uvwxy z";
    int ncv = tigetnum("ncv");

    mvprintw(row, 8, "%s mode:", name);
    mvprintw(row, 24, "|");
    if (skip)
	printw("%*s", skip, " ");
    attrset(attr);
    /*
     * If we're to write a string in the alternate character set, it is not
     * sufficient to just set A_ALTCHARSET.  We have to perform the mapping
     * that corresponds.  This is not needed for vt100-compatible devices
     * because the acs_map[] is 1:1, but for PC-style devices such as Linux
     * console, the acs_map[] is scattered about the range.
     *
     * The addch/addstr functions do not themselves do this mapping, since it
     * is possible to turn off the A_ALTCHARSET flag for the characters which
     * are added, and it would be an unexpected result to have the mapped
     * characters visible on the screen.
     *
     * This example works because the indices into acs_map[] are mostly from
     * the lowercase characters.
     */
    if (attr & A_ALTCHARSET) {
	const char *s = string;
	while (*s) {
	    int ch = *s++;
#ifdef CURSES_ACS_ARRAY
	    if ((ch = CURSES_ACS_ARRAY[ch]) == 0)
		ch = ' ';
#endif
	    addch(ch);
	}
    } else {
	addstr(string);
    }
    attroff(attr);
    if (skip)
	printw("%*s", skip, " ");
    printw("|");
    if (attr != A_NORMAL) {
	if (!(termattrs() & attr)) {
	    printw(" (N/A)");
	} else if (ncv > 0 && (getbkgd(stdscr) & A_COLOR)) {
	    static const chtype table[] =
	    {
		A_STANDOUT,
		A_UNDERLINE,
		A_REVERSE,
		A_BLINK,
		A_DIM,
		A_BOLD,
		A_INVIS,
		A_PROTECT,
		A_ALTCHARSET
	    };
	    unsigned n;
	    bool found = FALSE;
	    for (n = 0; n < SIZEOF(table); n++) {
		if ((table[n] & attr) != 0
		    && ((1 << n) & ncv) != 0) {
		    found = TRUE;
		    break;
		}
	    }
	    if (found)
		printw(" (NCV)");
	}
    }
    return row + 2;
}

static bool
attr_getc(int *skip, int *fg, int *bg, int *ac)
{
    int ch = Getchar();

    if (isdigit(ch)) {
	*skip = (ch - '0');
    } else if (ch == CTRL('L')) {
	touchwin(stdscr);
	touchwin(curscr);
    } else if (has_colors()) {
	switch (ch) {
	case 'a':
	    *ac = 0;
	    break;
	case 'A':
	    *ac = A_ALTCHARSET;
	    break;
	case 'f':
	    *fg = (*fg + 1);
	    break;
	case 'F':
	    *fg = (*fg - 1);
	    break;
	case 'b':
	    *bg = (*bg + 1);
	    break;
	case 'B':
	    *bg = (*bg - 1);
	    break;
	default:
	    return FALSE;
	}
	if (*fg >= max_colors)
	    *fg = 0;
	if (*fg < 0)
	    *fg = max_colors - 1;
	if (*bg >= max_colors)
	    *bg = 0;
	if (*bg < 0)
	    *bg = max_colors - 1;
    } else {
	switch (ch) {
	case 'a':
	    *ac = 0;
	    break;
	case 'A':
	    *ac = A_ALTCHARSET;
	    break;
	default:
	    return FALSE;
	}
    }
    return TRUE;
}

static void
attr_test(void)
/* test text attributes */
{
    int n;
    int skip = tigetnum("xmc");
    int fg = COLOR_BLACK;	/* color pair 0 is special */
    int bg = COLOR_BLACK;
    int ac = 0;
    bool *pairs = (bool *) calloc(max_pairs, sizeof(bool));
    pairs[0] = TRUE;

    if (skip < 0)
	skip = 0;

    n = skip;			/* make it easy */

    do {
	int row = 2;
	int normal = A_NORMAL | BLANK;

	if (has_colors()) {
	    int pair = (fg * max_colors) + bg;
	    if (!pairs[pair]) {
		init_pair(pair, fg, bg);
		pairs[pair] = TRUE;
	    }
	    normal |= COLOR_PAIR(pair);
	}
	bkgdset(normal);
	erase();

	mvaddstr(0, 20, "Character attribute test display");

	row = show_attr(row, n, ac | A_STANDOUT, "STANDOUT");
	row = show_attr(row, n, ac | A_REVERSE, "REVERSE");
	row = show_attr(row, n, ac | A_BOLD, "BOLD");
	row = show_attr(row, n, ac | A_UNDERLINE, "UNDERLINE");
	row = show_attr(row, n, ac | A_DIM, "DIM");
	row = show_attr(row, n, ac | A_BLINK, "BLINK");
	row = show_attr(row, n, ac | A_PROTECT, "PROTECT");
	row = show_attr(row, n, ac | A_INVIS, "INVISIBLE");
	row = show_attr(row, n, ac | A_NORMAL, "NORMAL");

	mvprintw(row, 8,
		 "This terminal does %shave the magic-cookie glitch",
		 tigetnum("xmc") > -1 ? "" : "not ");
	mvprintw(row + 1, 8,
		 "Enter a digit to set gaps on each side of displayed attributes");
	mvprintw(row + 2, 8,
		 "^L = repaint");
	if (has_colors())
	    printw(".  f/F/b/F toggle colors (now %d/%d), a/A altcharset (%d)",
		   fg, bg, ac != 0);
	else
	    printw(".  a/A altcharset (%d)", ac != 0);

	refresh();
    } while (attr_getc(&n, &fg, &bg, &ac));

    free((char *) pairs);
    bkgdset(A_NORMAL | BLANK);
    erase();
    endwin();
}

/****************************************************************************
 *
 * Color support tests
 *
 ****************************************************************************/

static NCURSES_CONST char *the_color_names[] =
{
    "black",
    "red",
    "green",
    "yellow",
    "blue",
    "magenta",
    "cyan",
    "white",
    "BLACK",
    "RED",
    "GREEN",
    "YELLOW",
    "BLUE",
    "MAGENTA",
    "CYAN",
    "WHITE"
};

static void
show_color_name(int y, int x, int color)
{
    if (max_colors > 8)
	mvprintw(y, x, "%02d   ", color);
    else
	mvaddstr(y, x, the_color_names[color]);
}

static void
color_test(void)
/* generate a color test pattern */
{
    int i;
    int base, top, width;
    NCURSES_CONST char *hello;

    refresh();
    (void) printw("There are %d color pairs\n", COLOR_PAIRS);

    width = (max_colors > 8) ? 4 : 8;
    hello = (max_colors > 8) ? "Test" : "Hello";

    for (base = 0; base < 2; base++) {
	top = (max_colors > 8) ? 0 : base * (max_colors + 3);
	clrtobot();
	(void) mvprintw(top + 1, 0,
			"%dx%d matrix of foreground/background colors, bright *%s*\n",
			max_colors, max_colors,
			base ? "on" : "off");
	for (i = 0; i < max_colors; i++)
	    show_color_name(top + 2, (i + 1) * width, i);
	for (i = 0; i < max_colors; i++)
	    show_color_name(top + 3 + i, 0, i);
	for (i = 1; i < max_pairs; i++) {
	    init_pair(i, i % max_colors, i / max_colors);
	    attron((attr_t) COLOR_PAIR(i));
	    if (base)
		attron((attr_t) A_BOLD);
	    mvaddstr(top + 3 + (i / max_colors), (i % max_colors + 1) *
		     width, hello);
	    attrset(A_NORMAL);
	}
	if ((max_colors > 8) || base)
	    Pause();
    }

    erase();
    endwin();
}

static void
change_color(int current, int field, int value, int usebase)
{
    short red, green, blue;

    if (usebase)
	color_content(current, &red, &green, &blue);
    else
	red = green = blue = 0;

    switch (field) {
    case 0:
	red += value;
	break;
    case 1:
	green += value;
	break;
    case 2:
	blue += value;
	break;
    }

    if (init_color(current, red, green, blue) == ERR)
	beep();
}

static void
color_edit(void)
/* display the color test pattern, without trying to edit colors */
{
    int i, this_c = 0, value = 0, current = 0, field = 0;
    int last_c;

    refresh();

    for (i = 0; i < max_colors; i++)
	init_pair(i, COLOR_WHITE, i);

    mvprintw(LINES - 2, 0, "Number: %d", value);

    do {
	short red, green, blue;

	attron(A_BOLD);
	mvaddstr(0, 20, "Color RGB Value Editing");
	attroff(A_BOLD);

	for (i = 0; i < max_colors; i++) {
	    mvprintw(2 + i, 0, "%c %-8s:",
		     (i == current ? '>' : ' '),
		     (i < (int) SIZEOF(the_color_names)
		      ? the_color_names[i] : ""));
	    attrset(COLOR_PAIR(i));
	    addstr("        ");
	    attrset(A_NORMAL);

	    /*
	     * Note: this refresh should *not* be necessary!  It works around
	     * a bug in attribute handling that apparently causes the A_NORMAL
	     * attribute sets to interfere with the actual emission of the
	     * color setting somehow.  This needs to be fixed.
	     */
	    refresh();

	    color_content(i, &red, &green, &blue);
	    addstr("   R = ");
	    if (current == i && field == 0)
		attron(A_STANDOUT);
	    printw("%04d", red);
	    if (current == i && field == 0)
		attrset(A_NORMAL);
	    addstr(", G = ");
	    if (current == i && field == 1)
		attron(A_STANDOUT);
	    printw("%04d", green);
	    if (current == i && field == 1)
		attrset(A_NORMAL);
	    addstr(", B = ");
	    if (current == i && field == 2)
		attron(A_STANDOUT);
	    printw("%04d", blue);
	    if (current == i && field == 2)
		attrset(A_NORMAL);
	    attrset(A_NORMAL);
	    addstr(")");
	}

	mvaddstr(max_colors + 3, 0,
		 "Use up/down to select a color, left/right to change fields.");
	mvaddstr(max_colors + 4, 0,
		 "Modify field by typing nnn=, nnn-, or nnn+.  ? for help.");

	move(2 + current, 0);

	last_c = this_c;
	this_c = Getchar();
	if (isdigit(this_c) && !isdigit(last_c))
	    value = 0;

	switch (this_c) {
	case KEY_UP:
	    current = (current == 0 ? (max_colors - 1) : current - 1);
	    break;

	case KEY_DOWN:
	    current = (current == (max_colors - 1) ? 0 : current + 1);
	    break;

	case KEY_RIGHT:
	    field = (field == 2 ? 0 : field + 1);
	    break;

	case KEY_LEFT:
	    field = (field == 0 ? 2 : field - 1);
	    break;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    value = value * 10 + (this_c - '0');
	    break;

	case '+':
	    change_color(current, field, value, 1);
	    break;

	case '-':
	    change_color(current, field, -value, 1);
	    break;

	case '=':
	    change_color(current, field, value, 0);
	    break;

	case '?':
	    erase();
	    P("                      RGB Value Editing Help");
	    P("");
	    P("You are in the RGB value editor.  Use the arrow keys to select one of");
	    P("the fields in one of the RGB triples of the current colors; the one");
	    P("currently selected will be reverse-video highlighted.");
	    P("");
	    P("To change a field, enter the digits of the new value; they are echoed");
	    P("as entered.  Finish by typing `='.  The change will take effect instantly.");
	    P("To increment or decrement a value, use the same procedure, but finish");
	    P("with a `+' or `-'.");
	    P("");
	    P("To quit, do `x' or 'q'");

	    Pause();
	    erase();
	    break;

	case 'x':
	case 'q':
	    break;

	default:
	    beep();
	    break;
	}
	mvprintw(LINES - 2, 0, "Number: %d", value);
	clrtoeol();
    } while
	(this_c != 'x' && this_c != 'q');

    erase();
    endwin();
}

/****************************************************************************
 *
 * Soft-key label test
 *
 ****************************************************************************/

static void
slk_test(void)
/* exercise the soft keys */
{
    int c, fmt = 1;
    char buf[9];

    c = CTRL('l');
    do {
	move(0, 0);
	switch (c) {
	case CTRL('l'):
	    erase();
	    attron(A_BOLD);
	    mvaddstr(0, 20, "Soft Key Exerciser");
	    attroff(A_BOLD);

	    move(2, 0);
	    P("Available commands are:");
	    P("");
	    P("^L         -- refresh screen");
	    P("a          -- activate or restore soft keys");
	    P("d          -- disable soft keys");
	    P("c          -- set centered format for labels");
	    P("l          -- set left-justified format for labels");
	    P("r          -- set right-justified format for labels");
	    P("[12345678] -- set label; labels are numbered 1 through 8");
	    P("e          -- erase stdscr (should not erase labels)");
	    P("s          -- test scrolling of shortened screen");
	    P("x, q       -- return to main menu");
	    P("");
	    P("Note: if activating the soft keys causes your terminal to");
	    P("scroll up one line, your terminal auto-scrolls when anything");
	    P("is written to the last screen position.  The ncurses code");
	    P("does not yet handle this gracefully.");
	    refresh();
	    /* fall through */

	case 'a':
	    slk_restore();
	    break;

	case 'e':
	    wclear(stdscr);
	    break;

	case 's':
	    mvprintw(20, 0, "Press Q to stop the scrolling-test: ");
	    while ((c = Getchar()) != 'Q' && (c != ERR))
		addch((chtype) c);
	    break;

	case 'd':
	    slk_clear();
	    break;

	case 'l':
	    fmt = 0;
	    break;

	case 'c':
	    fmt = 1;
	    break;

	case 'r':
	    fmt = 2;
	    break;

	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	    (void) mvaddstr(20, 0, "Please enter the label value: ");
	    echo();
	    wgetnstr(stdscr, buf, 8);
	    noecho();
	    slk_set((c - '0'), buf, fmt);
	    slk_refresh();
	    move(20, 0);
	    clrtoeol();
	    break;

	case 'x':
	case 'q':
	    goto done;

	default:
	    beep();
	}
    } while
	((c = Getchar()) != EOF);

  done:
    erase();
    endwin();
}

/****************************************************************************
 *
 * Alternate character-set stuff
 *
 ****************************************************************************/

/* ISO 6429:  codes 0x80 to 0x9f may be control characters that cause the
 * terminal to perform functions.  The remaining codes can be graphic.
 */
static void
show_upper_chars(int first)
{
    bool C1 = (first == 128);
    int code;
    int last = first + 31;
    int reply;

    erase();
    attron(A_BOLD);
    mvprintw(0, 20, "Display of %s Character Codes %d to %d",
	     C1 ? "C1" : "GR", first, last);
    attroff(A_BOLD);
    refresh();

    for (code = first; code <= last; code++) {
	int row = 4 + ((code - first) % 16);
	int col = ((code - first) / 16) * COLS / 2;
	char tmp[80];
	sprintf(tmp, "%3d (0x%x)", code, code);
	mvprintw(row, col, "%*s: ", COLS / 4, tmp);
	if (C1)
	    nodelay(stdscr, TRUE);
	echochar(code);
	if (C1) {
	    /* (yes, this _is_ crude) */
	    while ((reply = Getchar()) != ERR) {
		addch(reply);
		napms(10);
	    }
	    nodelay(stdscr, FALSE);
	}
    }
}

static void
show_box_chars(void)
{
    erase();
    attron(A_BOLD);
    mvaddstr(0, 20, "Display of the ACS Line-Drawing Set");
    attroff(A_BOLD);
    refresh();
    box(stdscr, 0, 0);
    /* *INDENT-OFF* */
    mvhline(LINES / 2, 0,        ACS_HLINE, COLS);
    mvvline(0,         COLS / 2, ACS_VLINE, LINES);
    mvaddch(0,         COLS / 2, ACS_TTEE);
    mvaddch(LINES / 2, COLS / 2, ACS_PLUS);
    mvaddch(LINES - 1, COLS / 2, ACS_BTEE);
    mvaddch(LINES / 2, 0,        ACS_LTEE);
    mvaddch(LINES / 2, COLS - 1, ACS_RTEE);
    /* *INDENT-ON* */

}

static int
show_1_acs(int n, const char *name, chtype code)
{
    const int height = 16;
    int row = 4 + (n % height);
    int col = (n / height) * COLS / 2;
    mvprintw(row, col, "%*s : ", COLS / 4, name);
    addch(code);
    return n + 1;
}

static void
show_acs_chars(void)
/* display the ACS character set */
{
    int n;

#define BOTH(name) #name, name

    erase();
    attron(A_BOLD);
    mvaddstr(0, 20, "Display of the ACS Character Set");
    attroff(A_BOLD);
    refresh();

    n = show_1_acs(0, BOTH(ACS_ULCORNER));
    n = show_1_acs(n, BOTH(ACS_URCORNER));
    n = show_1_acs(n, BOTH(ACS_LLCORNER));
    n = show_1_acs(n, BOTH(ACS_LRCORNER));

    n = show_1_acs(n, BOTH(ACS_LTEE));
    n = show_1_acs(n, BOTH(ACS_RTEE));
    n = show_1_acs(n, BOTH(ACS_TTEE));
    n = show_1_acs(n, BOTH(ACS_BTEE));

    n = show_1_acs(n, BOTH(ACS_HLINE));
    n = show_1_acs(n, BOTH(ACS_VLINE));

    n = show_1_acs(n, BOTH(ACS_LARROW));
    n = show_1_acs(n, BOTH(ACS_RARROW));
    n = show_1_acs(n, BOTH(ACS_UARROW));
    n = show_1_acs(n, BOTH(ACS_DARROW));

    n = show_1_acs(n, BOTH(ACS_BLOCK));
    n = show_1_acs(n, BOTH(ACS_BOARD));
    n = show_1_acs(n, BOTH(ACS_LANTERN));
    n = show_1_acs(n, BOTH(ACS_BULLET));
    n = show_1_acs(n, BOTH(ACS_CKBOARD));
    n = show_1_acs(n, BOTH(ACS_DEGREE));
    n = show_1_acs(n, BOTH(ACS_DIAMOND));
    n = show_1_acs(n, BOTH(ACS_PLMINUS));
    n = show_1_acs(n, BOTH(ACS_PLUS));

    n = show_1_acs(n, BOTH(ACS_GEQUAL));
    n = show_1_acs(n, BOTH(ACS_NEQUAL));
    n = show_1_acs(n, BOTH(ACS_LEQUAL));

    n = show_1_acs(n, BOTH(ACS_STERLING));
    n = show_1_acs(n, BOTH(ACS_PI));
    n = show_1_acs(n, BOTH(ACS_S1));
    n = show_1_acs(n, BOTH(ACS_S3));
    n = show_1_acs(n, BOTH(ACS_S7));
    n = show_1_acs(n, BOTH(ACS_S9));
}

static void
acs_display(void)
{
    int c = 'a';

    do {
	switch (c) {
	case 'a':
	    show_acs_chars();
	    break;
	case 'b':
	    show_box_chars();
	    break;
	case '0':
	case '1':
	case '2':
	case '3':
	    show_upper_chars((c - '0') * 32 + 128);
	    break;
	}
	mvprintw(LINES - 3, 0,
		 "Note: ANSI terminals may not display C1 characters.");
	mvprintw(LINES - 2, 0,
		 "Select: a=ACS, b=box, 0=C1, 1,2,3=GR characters, q=quit");
	refresh();
    } while ((c = Getchar()) != 'x' && c != 'q');

    Pause();
    erase();
    endwin();
}

#if USE_WIDEC_SUPPORT
static void
show_upper_widechars(int first)
{
    cchar_t temp;
    wchar_t code;
    int last = first + 31;

    erase();
    attron(A_BOLD);
    mvprintw(0, 20, "Display of Character Codes %d to %d", first, last);
    attroff(A_BOLD);
    refresh();

    for (code = first; code <= last; code++) {
	int row = 4 + ((code - first) % 16);
	int col = ((code - first) / 16) * COLS / 2;
	attr_t attrs = A_NORMAL;
	char tmp[80];
	sprintf(tmp, "%3d (0x%x)", code, code);
	mvprintw(row, col, "%*s: ", COLS / 4, tmp);
	setcchar(&temp, &code, attrs, 0, 0);
	echo_wchar(&temp);
    }
}

static int
show_1_wacs(int n, const char *name, const cchar_t * code)
{
    const int height = 16;
    int row = 4 + (n % height);
    int col = (n / height) * COLS / 2;
    mvprintw(row, col, "%*s : ", COLS / 4, name);
    add_wchnstr(code, 1);
    return n + 1;
}

static void
show_wacs_chars(void)
/* display the wide-ACS character set */
{
    int n;

/*#define BOTH2(name) #name, &(name) */
#define BOTH2(name) #name, name

    erase();
    attron(A_BOLD);
    mvaddstr(0, 20, "Display of the Wide-ACS Character Set");
    attroff(A_BOLD);
    refresh();

    n = show_1_wacs(0, BOTH2(WACS_ULCORNER));
    n = show_1_wacs(n, BOTH2(WACS_URCORNER));
    n = show_1_wacs(n, BOTH2(WACS_LLCORNER));
    n = show_1_wacs(n, BOTH2(WACS_LRCORNER));

    n = show_1_wacs(n, BOTH2(WACS_LTEE));
    n = show_1_wacs(n, BOTH2(WACS_RTEE));
    n = show_1_wacs(n, BOTH2(WACS_TTEE));
    n = show_1_wacs(n, BOTH2(WACS_BTEE));

    n = show_1_wacs(n, BOTH2(WACS_HLINE));
    n = show_1_wacs(n, BOTH2(WACS_VLINE));

    n = show_1_wacs(n, BOTH2(WACS_LARROW));
    n = show_1_wacs(n, BOTH2(WACS_RARROW));
    n = show_1_wacs(n, BOTH2(WACS_UARROW));
    n = show_1_wacs(n, BOTH2(WACS_DARROW));

    n = show_1_wacs(n, BOTH2(WACS_BLOCK));
    n = show_1_wacs(n, BOTH2(WACS_BOARD));
    n = show_1_wacs(n, BOTH2(WACS_LANTERN));
    n = show_1_wacs(n, BOTH2(WACS_BULLET));
    n = show_1_wacs(n, BOTH2(WACS_CKBOARD));
    n = show_1_wacs(n, BOTH2(WACS_DEGREE));
    n = show_1_wacs(n, BOTH2(WACS_DIAMOND));
    n = show_1_wacs(n, BOTH2(WACS_PLMINUS));
    n = show_1_wacs(n, BOTH2(WACS_PLUS));

#ifdef CURSES_WACS_ARRAY
    n = show_1_wacs(n, BOTH2(WACS_GEQUAL));
    n = show_1_wacs(n, BOTH2(WACS_NEQUAL));
    n = show_1_wacs(n, BOTH2(WACS_LEQUAL));

    n = show_1_wacs(n, BOTH2(WACS_STERLING));
    n = show_1_wacs(n, BOTH2(WACS_PI));
    n = show_1_wacs(n, BOTH2(WACS_S1));
    n = show_1_wacs(n, BOTH2(WACS_S3));
    n = show_1_wacs(n, BOTH2(WACS_S7));
    n = show_1_wacs(n, BOTH2(WACS_S9));
#endif
}

static void
show_wbox_chars(void)
{
    erase();
    attron(A_BOLD);
    mvaddstr(0, 20, "Display of the Wide-ACS Line-Drawing Set");
    attroff(A_BOLD);
    refresh();
    box_set(stdscr, 0, 0);
    /* *INDENT-OFF* */
    mvhline_set(LINES / 2, 0,        WACS_HLINE, COLS);
    mvvline_set(0,         COLS / 2, WACS_VLINE, LINES);
    mvadd_wch(0,           COLS / 2, WACS_TTEE);
    mvadd_wch(LINES / 2,   COLS / 2, WACS_PLUS);
    mvadd_wch(LINES - 1,   COLS / 2, WACS_BTEE);
    mvadd_wch(LINES / 2,   0,        WACS_LTEE);
    mvadd_wch(LINES / 2,   COLS - 1, WACS_RTEE);
    /* *INDENT-ON* */

}

static int
show_2_wacs(int n, const char *name, char *code)
{
    const int height = 16;
    int row = 4 + (n % height);
    int col = (n / height) * COLS / 2;
    mvprintw(row, col, "%*s : ", COLS / 4, name);
    addstr(code);
    return n + 1;
}

static void
show_utf8_chars(void)
/* display the wide-ACS character set */
{
    int n;

    erase();
    attron(A_BOLD);
    mvaddstr(0, 20, "Display of the Wide-ACS Character Set");
    attroff(A_BOLD);
    refresh();
    /* *INDENT-OFF* */
    n = show_2_wacs(0, "WACS_ULCORNER",	"\342\224\214");
    n = show_2_wacs(n, "WACS_URCORNER",	"\342\224\220");
    n = show_2_wacs(n, "WACS_LLCORNER",	"\342\224\224");
    n = show_2_wacs(n, "WACS_LRCORNER",	"\342\224\230");

    n = show_2_wacs(n, "WACS_LTEE",	"\342\224\234");
    n = show_2_wacs(n, "WACS_RTEE",	"\342\224\244");
    n = show_2_wacs(n, "WACS_TTEE",	"\342\224\254");
    n = show_2_wacs(n, "WACS_BTEE",	"\342\224\264");

    n = show_2_wacs(n, "WACS_HLINE",	"\342\224\200");
    n = show_2_wacs(n, "WACS_VLINE",	"\342\224\202");

    n = show_2_wacs(n, "WACS_LARROW",	"\342\206\220");
    n = show_2_wacs(n, "WACS_RARROW",	"\342\206\222");
    n = show_2_wacs(n, "WACS_UARROW",	"\342\206\221");
    n = show_2_wacs(n, "WACS_DARROW",	"\342\206\223");

    n = show_2_wacs(n, "WACS_STERLING",	"\302\243");

    n = show_2_wacs(n, "WACS_BLOCK",	"\342\226\256");
    n = show_2_wacs(n, "WACS_BOARD",	"\342\226\222");
    n = show_2_wacs(n, "WACS_LANTERN",	"\342\230\203");
    n = show_2_wacs(n, "WACS_BULLET",	"\302\267");
    n = show_2_wacs(n, "WACS_CKBOARD",	"\342\226\222");
    n = show_2_wacs(n, "WACS_DEGREE",	"\302\260");
    n = show_2_wacs(n, "WACS_DIAMOND",	"\342\227\206");
    n = show_2_wacs(n, "WACS_GEQUAL",	"\342\211\245");
    n = show_2_wacs(n, "WACS_NEQUAL",	"\342\211\240");
    n = show_2_wacs(n, "WACS_LEQUAL",	"\342\211\244");
    n = show_2_wacs(n, "WACS_PLMINUS",	"\302\261");
    n = show_2_wacs(n, "WACS_PLUS",	"\342\224\274");
    n = show_2_wacs(n, "WACS_PI",	"\317\200");
    n = show_2_wacs(n, "WACS_S1",	"\342\216\272");
    n = show_2_wacs(n, "WACS_S3",	"\342\216\273");
    n = show_2_wacs(n, "WACS_S7",	"\342\216\274");
    n = show_2_wacs(n, "WACS_S9",	"\342\216\275");
    /* *INDENT-OFF* */
}

static void
wide_acs_display(void)
{
    int c = 'a';

    do {
	switch (c) {
	case 'a':
	    show_wacs_chars();
	    break;
	case 'b':
	    show_wbox_chars();
	    break;
	case 'u':
	    show_utf8_chars();
	    break;
	default:
	    if (isdigit(c))
		show_upper_widechars((c - '0') * 32 + 128);
	    break;
	}
	mvprintw(LINES - 2, 0,
		 "Select: a WACS, b box, u UTF-8, 0-9 non-ASCII characters, q=quit");
	refresh();
    } while ((c = Getchar()) != 'x' && c != 'q');

    Pause();
    erase();
    endwin();
}

#endif

/*
 * Graphic-rendition test (adapted from vttest)
 */
static void
test_sgr_attributes(void)
{
    int pass;

    for (pass = 0; pass < 2; pass++) {
	int normal = ((pass == 0 ? A_NORMAL : A_REVERSE)) | BLANK;

	/* Use non-default colors if possible to exercise bce a little */
	if (has_colors()) {
	    init_pair(1, COLOR_WHITE, COLOR_BLUE);
	    normal |= COLOR_PAIR(1);
	}
	bkgdset(normal);
	erase();
	mvprintw(1, 20, "Graphic rendition test pattern:");

	mvprintw(4, 1, "vanilla");

#define set_sgr(mask) bkgdset((normal^(mask)));
	set_sgr(A_BOLD);
	mvprintw(4, 40, "bold");

	set_sgr(A_UNDERLINE);
	mvprintw(6, 6, "underline");

	set_sgr(A_BOLD | A_UNDERLINE);
	mvprintw(6, 45, "bold underline");

	set_sgr(A_BLINK);
	mvprintw(8, 1, "blink");

	set_sgr(A_BLINK | A_BOLD);
	mvprintw(8, 40, "bold blink");

	set_sgr(A_UNDERLINE | A_BLINK);
	mvprintw(10, 6, "underline blink");

	set_sgr(A_BOLD | A_UNDERLINE | A_BLINK);
	mvprintw(10, 45, "bold underline blink");

	set_sgr(A_REVERSE);
	mvprintw(12, 1, "negative");

	set_sgr(A_BOLD | A_REVERSE);
	mvprintw(12, 40, "bold negative");

	set_sgr(A_UNDERLINE | A_REVERSE);
	mvprintw(14, 6, "underline negative");

	set_sgr(A_BOLD | A_UNDERLINE | A_REVERSE);
	mvprintw(14, 45, "bold underline negative");

	set_sgr(A_BLINK | A_REVERSE);
	mvprintw(16, 1, "blink negative");

	set_sgr(A_BOLD | A_BLINK | A_REVERSE);
	mvprintw(16, 40, "bold blink negative");

	set_sgr(A_UNDERLINE | A_BLINK | A_REVERSE);
	mvprintw(18, 6, "underline blink negative");

	set_sgr(A_BOLD | A_UNDERLINE | A_BLINK | A_REVERSE);
	mvprintw(18, 45, "bold underline blink negative");

	bkgdset(normal);
	mvprintw(LINES - 2, 1, "%s background. ", pass == 0 ? "Dark" :
		 "Light");
	clrtoeol();
	Pause();
    }

    bkgdset(A_NORMAL | BLANK);
    erase();
    endwin();
}

/****************************************************************************
 *
 * Windows and scrolling tester.
 *
 ****************************************************************************/

#define BOTLINES	4	/* number of line stolen from screen bottom */

typedef struct {
    int y, x;
} pair;

#define FRAME struct frame
FRAME
{
    FRAME *next, *last;
    bool do_scroll;
    bool do_keypad;
    WINDOW *wind;
};

#ifdef NCURSES_VERSION
#define keypad_active(win) (win)->_use_keypad
#define scroll_active(win) (win)->_scroll
#else
#define keypad_active(win) FALSE
#define scroll_active(win) FALSE
#endif

/* We need to know if these flags are actually set, so don't look in FRAME.
 * These names are known to work with SVr4 curses as well as ncurses.  The
 * _use_keypad name does not work with Solaris 8.
 */
static bool
HaveKeypad(FRAME * curp)
{
    WINDOW *win = (curp ? curp->wind : stdscr);
    return keypad_active(win);
}

static bool
HaveScroll(FRAME * curp)
{
    WINDOW *win = (curp ? curp->wind : stdscr);
    return scroll_active(win);
}

static void
newwin_legend(FRAME * curp)
{
    static const struct {
	const char *msg;
	int code;
    } legend[] = {
	{
	    "^C = create window", 0
	},
	{
	    "^N = next window", 0
	},
	{
	    "^P = previous window", 0
	},
	{
	    "^F = scroll forward", 0
	},
	{
	    "^B = scroll backward", 0
	},
	{
	    "^K = keypad(%s)", 1
	},
	{
	    "^S = scrollok(%s)", 2
	},
	{
	    "^W = save window to file", 0
	},
	{
	    "^R = restore window", 0
	},
#if HAVE_WRESIZE
	{
	    "^X = resize", 0
	},
#endif
	{
	    "^Q%s = exit", 3
	}
    };
    size_t n;
    int x;
    bool do_keypad = HaveKeypad(curp);
    bool do_scroll = HaveScroll(curp);
    char buf[BUFSIZ];

    move(LINES - 4, 0);
    for (n = 0; n < SIZEOF(legend); n++) {
	switch (legend[n].code) {
	default:
	    strcpy(buf, legend[n].msg);
	    break;
	case 1:
	    sprintf(buf, legend[n].msg, do_keypad ? "yes" : "no");
	    break;
	case 2:
	    sprintf(buf, legend[n].msg, do_scroll ? "yes" : "no");
	    break;
	case 3:
	    sprintf(buf, legend[n].msg, do_keypad ? "/ESC" : "");
	    break;
	}
	x = getcurx(stdscr);
	addstr((COLS < (x + 3 + (int) strlen(buf))) ? "\n" : (n ? ", " : ""));
	addstr(buf);
    }
    clrtoeol();
}

static void
transient(FRAME * curp, NCURSES_CONST char *msg)
{
    newwin_legend(curp);
    if (msg) {
	mvaddstr(LINES - 1, 0, msg);
	refresh();
	napms(1000);
    }

    move(LINES - 1, 0);
    printw("%s characters are echoed, window should %sscroll.",
	   HaveKeypad(curp) ? "Non-arrow" : "All other",
	   HaveScroll(curp) ? "" : "not ");
    clrtoeol();
}

static void
newwin_report(FRAME * curp)
/* report on the cursor's current position, then restore it */
{
    WINDOW *win = (curp != 0) ? curp->wind : stdscr;
    int y, x;

    if (win != stdscr)
	transient(curp, (char *) 0);
    getyx(win, y, x);
    move(LINES - 1, COLS - 17);
    printw("Y = %2d X = %2d", y, x);
    if (win != stdscr)
	refresh();
    else
	wmove(win, y, x);
}

static pair *
selectcell(int uli, int ulj, int lri, int lrj)
/* arrows keys move cursor, return location at current on non-arrow key */
{
    static pair res;		/* result cell */
    int si = lri - uli + 1;	/* depth of the select area */
    int sj = lrj - ulj + 1;	/* width of the select area */
    int i = 0, j = 0;		/* offsets into the select area */

    res.y = uli;
    res.x = ulj;
    for (;;) {
	move(uli + i, ulj + j);
	newwin_report((FRAME *) 0);

	switch (Getchar()) {
	case KEY_UP:
	    i += si - 1;
	    break;
	case KEY_DOWN:
	    i++;
	    break;
	case KEY_LEFT:
	    j += sj - 1;
	    break;
	case KEY_RIGHT:
	    j++;
	    break;
	case QUIT:
	case ESCAPE:
	    return ((pair *) 0);
#ifdef NCURSES_MOUSE_VERSION
	case KEY_MOUSE:
	    {
		MEVENT event;

		getmouse(&event);
		if (event.y > uli && event.x > ulj) {
		    i = event.y - uli;
		    j = event.x - ulj;
		} else {
		    beep();
		    break;
		}
	    }
	    /* FALLTHRU */
#endif
	default:
	    res.y = uli + i;
	    res.x = ulj + j;
	    return (&res);
	}
	i %= si;
	j %= sj;
    }
}

static void
outerbox(pair ul, pair lr, bool onoff)
/* draw or erase a box *outside* the given pair of corners */
{
    mvaddch(ul.y - 1, lr.x - 1, onoff ? ACS_ULCORNER : ' ');
    mvaddch(ul.y - 1, lr.x + 1, onoff ? ACS_URCORNER : ' ');
    mvaddch(lr.y + 1, lr.x + 1, onoff ? ACS_LRCORNER : ' ');
    mvaddch(lr.y + 1, ul.x - 1, onoff ? ACS_LLCORNER : ' ');
    move(ul.y - 1, ul.x);
    hline(onoff ? ACS_HLINE : ' ', lr.x - ul.x + 1);
    move(ul.y, ul.x - 1);
    vline(onoff ? ACS_VLINE : ' ', lr.y - ul.y + 1);
    move(lr.y + 1, ul.x);
    hline(onoff ? ACS_HLINE : ' ', lr.x - ul.x + 1);
    move(ul.y, lr.x + 1);
    vline(onoff ? ACS_VLINE : ' ', lr.y - ul.y + 1);
}

static WINDOW *
getwindow(void)
/* Ask user for a window definition */
{
    WINDOW *rwindow;
    pair ul, lr, *tmp;

    move(0, 0);
    clrtoeol();
    addstr("Use arrows to move cursor, anything else to mark corner 1");
    refresh();
    if ((tmp = selectcell(2, 1, LINES - BOTLINES - 2, COLS - 2)) == (pair *) 0)
	return ((WINDOW *) 0);
    memcpy(&ul, tmp, sizeof(pair));
    mvaddch(ul.y - 1, ul.x - 1, ACS_ULCORNER);
    move(0, 0);
    clrtoeol();
    addstr("Use arrows to move cursor, anything else to mark corner 2");
    refresh();
    if ((tmp = selectcell(ul.y, ul.x, LINES - BOTLINES - 2, COLS - 2)) ==
	(pair *) 0)
	return ((WINDOW *) 0);
    memcpy(&lr, tmp, sizeof(pair));

    rwindow = subwin(stdscr, lr.y - ul.y + 1, lr.x - ul.x + 1, ul.y, ul.x);

    outerbox(ul, lr, TRUE);
    refresh();

    wrefresh(rwindow);

    move(0, 0);
    clrtoeol();
    return (rwindow);
}

static void
newwin_move(FRAME * curp, int dy, int dx)
{
    WINDOW *win = (curp != 0) ? curp->wind : stdscr;
    int cur_y, cur_x;
    int max_y, max_x;

    getyx(win, cur_y, cur_x);
    getmaxyx(win, max_y, max_x);
    if ((cur_x += dx) < 0)
	cur_x = 0;
    else if (cur_x >= max_x)
	cur_x = max_x - 1;
    if ((cur_y += dy) < 0)
	cur_y = 0;
    else if (cur_y >= max_y)
	cur_y = max_y - 1;
    wmove(win, cur_y, cur_x);
}

static FRAME *
delete_framed(FRAME * fp, bool showit)
{
    FRAME *np;

    fp->last->next = fp->next;
    fp->next->last = fp->last;

    if (showit) {
	werase(fp->wind);
	wrefresh(fp->wind);
    }
    delwin(fp->wind);

    np = (fp == fp->next) ? 0 : fp->next;
    free(fp);
    return np;
}

static void
acs_and_scroll(void)
/* Demonstrate windows */
{
    int c, i;
    FILE *fp;
    FRAME *current = (FRAME *) 0, *neww;
    WINDOW *usescr = stdscr;

#define DUMPFILE	"screendump"

#ifdef NCURSES_MOUSE_VERSION
    mousemask(BUTTON1_CLICKED, (mmask_t *) 0);
#endif
    c = CTRL('C');
    raw();
    do {
	transient((FRAME *) 0, (char *) 0);
	switch (c) {
	case CTRL('C'):
	    neww = (FRAME *) calloc(1, sizeof(FRAME));
	    if ((neww->wind = getwindow()) == (WINDOW *) 0)
		goto breakout;

	    if (current == 0) {	/* First element,  */
		neww->next = neww;	/*   so point it at itself */
		neww->last = neww;
	    } else {
		neww->next = current->next;
		neww->last = current;
		neww->last->next = neww;
		neww->next->last = neww;
	    }
	    current = neww;
	    /* SVr4 curses sets the keypad on all newly-created windows to
	     * false.  Someone reported that PDCurses makes new windows inherit
	     * this flag.  Remove the following 'keypad()' call to test this
	     */
	    keypad(current->wind, TRUE);
	    current->do_keypad = HaveKeypad(current);
	    current->do_scroll = HaveScroll(current);
	    break;

	case CTRL('N'):	/* go to next window */
	    if (current)
		current = current->next;
	    break;

	case CTRL('P'):	/* go to previous window */
	    if (current)
		current = current->last;
	    break;

	case CTRL('F'):	/* scroll current window forward */
	    if (current)
		wscrl(current->wind, 1);
	    break;

	case CTRL('B'):	/* scroll current window backwards */
	    if (current)
		wscrl(current->wind, -1);
	    break;

	case CTRL('K'):	/* toggle keypad mode for current */
	    if (current) {
		current->do_keypad = !current->do_keypad;
		keypad(current->wind, current->do_keypad);
	    }
	    break;

	case CTRL('S'):
	    if (current) {
		current->do_scroll = !current->do_scroll;
		scrollok(current->wind, current->do_scroll);
	    }
	    break;

	case CTRL('W'):	/* save and delete window */
	    if (current == current->next)
		break;
	    if ((fp = fopen(DUMPFILE, "w")) == (FILE *) 0)
		transient(current, "Can't open screen dump file");
	    else {
		(void) putwin(current->wind, fp);
		(void) fclose(fp);

		current = delete_framed(current, TRUE);
	    }
	    break;

	case CTRL('R'):	/* restore window */
	    if ((fp = fopen(DUMPFILE, "r")) == (FILE *) 0)
		transient(current, "Can't open screen dump file");
	    else {
		neww = (FRAME *) calloc(1, sizeof(FRAME));

		neww->next = current->next;
		neww->last = current;
		neww->last->next = neww;
		neww->next->last = neww;

		neww->wind = getwin(fp);
		(void) fclose(fp);

		wrefresh(neww->wind);
	    }
	    break;

#if HAVE_WRESIZE
	case CTRL('X'):	/* resize window */
	    if (current) {
		pair *tmp, ul, lr;
		int mx, my;

		move(0, 0);
		clrtoeol();
		addstr("Use arrows to move cursor, anything else to mark new corner");
		refresh();

		getbegyx(current->wind, ul.y, ul.x);

		tmp = selectcell(ul.y, ul.x, LINES - BOTLINES - 2, COLS - 2);
		if (tmp == (pair *) 0) {
		    beep();
		    break;
		}

		getmaxyx(current->wind, lr.y, lr.x);
		lr.y += (ul.y - 1);
		lr.x += (ul.x - 1);
		outerbox(ul, lr, FALSE);
		wnoutrefresh(stdscr);

		/* strictly cosmetic hack for the test */
		getmaxyx(current->wind, my, mx);
		if (my > tmp->y - ul.y) {
		    getyx(current->wind, lr.y, lr.x);
		    wmove(current->wind, tmp->y - ul.y + 1, 0);
		    wclrtobot(current->wind);
		    wmove(current->wind, lr.y, lr.x);
		}
		if (mx > tmp->x - ul.x)
		    for (i = 0; i < my; i++) {
			wmove(current->wind, i, tmp->x - ul.x + 1);
			wclrtoeol(current->wind);
		    }
		wnoutrefresh(current->wind);

		memcpy(&lr, tmp, sizeof(pair));
		(void) wresize(current->wind, lr.y - ul.y + 0, lr.x - ul.x + 0);

		getbegyx(current->wind, ul.y, ul.x);
		getmaxyx(current->wind, lr.y, lr.x);
		lr.y += (ul.y - 1);
		lr.x += (ul.x - 1);
		outerbox(ul, lr, TRUE);
		wnoutrefresh(stdscr);

		wnoutrefresh(current->wind);
		move(0, 0);
		clrtoeol();
		doupdate();
	    }
	    break;
#endif /* HAVE_WRESIZE */

	case KEY_F(10):	/* undocumented --- use this to test area clears */
	    selectcell(0, 0, LINES - 1, COLS - 1);
	    clrtobot();
	    refresh();
	    break;

	case KEY_UP:
	    newwin_move(current, -1, 0);
	    break;
	case KEY_DOWN:
	    newwin_move(current, 1, 0);
	    break;
	case KEY_LEFT:
	    newwin_move(current, 0, -1);
	    break;
	case KEY_RIGHT:
	    newwin_move(current, 0, 1);
	    break;

	case KEY_BACKSPACE:
	    /* FALLTHROUGH */
	case KEY_DC:
	    {
		int y, x;
		getyx(current->wind, y, x);
		if (--x < 0) {
		    if (--y < 0)
			break;
		    x = getmaxx(current->wind) - 1;
		}
		mvwdelch(current->wind, y, x);
	    }
	    break;

	case '\r':
	    c = '\n';
	    /* FALLTHROUGH */

	default:
	    if (current)
		waddch(current->wind, (chtype) c);
	    else
		beep();
	    break;
	}
	newwin_report(current);
	usescr = (current ? current->wind : stdscr);
	wrefresh(usescr);
    } while
	((c = wGetchar(usescr)) != QUIT
	 && !((c == ESCAPE) && (keypad_active(usescr)))
	 && (c != ERR));

  breakout:
    while (current != 0)
	current = delete_framed(current, FALSE);

    scrollok(stdscr, TRUE);	/* reset to driver's default */
#ifdef NCURSES_MOUSE_VERSION
    mousemask(0, (mmask_t *) 0);
#endif
    noraw();
    erase();
    endwin();
}

/****************************************************************************
 *
 * Panels tester
 *
 ****************************************************************************/

#if USE_LIBPANEL
static unsigned long nap_msec = 1;

static NCURSES_CONST char *mod[] =
{
    "test ",
    "TEST ",
    "(**) ",
    "*()* ",
    "<--> ",
    "LAST "
};

/*+-------------------------------------------------------------------------
	wait_a_while(msec)
--------------------------------------------------------------------------*/
static void
wait_a_while(unsigned long msec GCC_UNUSED)
{
#if HAVE_NAPMS
    if (nap_msec == 1)
	wGetchar(stdscr);
    else
	napms(nap_msec);
#else
    if (nap_msec == 1)
	wGetchar(stdscr);
    else if (msec > 1000L)
	sleep((int) msec / 1000L);
    else
	sleep(1);
#endif
}				/* end of wait_a_while */

/*+-------------------------------------------------------------------------
	saywhat(text)
--------------------------------------------------------------------------*/
static void
saywhat(NCURSES_CONST char *text)
{
    wmove(stdscr, LINES - 1, 0);
    wclrtoeol(stdscr);
    waddstr(stdscr, text);
}				/* end of saywhat */

/*+-------------------------------------------------------------------------
	mkpanel(rows,cols,tly,tlx) - alloc a win and panel and associate them
--------------------------------------------------------------------------*/
static PANEL *
mkpanel(int color, int rows, int cols, int tly, int tlx)
{
    WINDOW *win;
    PANEL *pan = 0;

    if ((win = newwin(rows, cols, tly, tlx)) != 0) {
	if ((pan = new_panel(win)) == 0) {
	    delwin(win);
	} else if (has_colors()) {
	    int fg = (color == COLOR_BLUE) ? COLOR_WHITE : COLOR_BLACK;
	    int bg = color;
	    init_pair(color, fg, bg);
	    wbkgdset(win, COLOR_PAIR(color) | ' ');
	} else {
	    wbkgdset(win, A_BOLD | ' ');
	}
    }
    return pan;
}				/* end of mkpanel */

/*+-------------------------------------------------------------------------
	rmpanel(pan)
--------------------------------------------------------------------------*/
static void
rmpanel(PANEL * pan)
{
    WINDOW *win = panel_window(pan);
    del_panel(pan);
    delwin(win);
}				/* end of rmpanel */

/*+-------------------------------------------------------------------------
	pflush()
--------------------------------------------------------------------------*/
static void
pflush(void)
{
    update_panels();
    doupdate();
}				/* end of pflush */

/*+-------------------------------------------------------------------------
	fill_panel(win)
--------------------------------------------------------------------------*/
static void
fill_panel(PANEL * pan)
{
    WINDOW *win = panel_window(pan);
    int num = ((const char *) panel_userptr(pan))[1];
    int y, x;

    wmove(win, 1, 1);
    wprintw(win, "-pan%c-", num);
    wclrtoeol(win);
    box(win, 0, 0);
    for (y = 2; y < getmaxy(win) - 1; y++) {
	for (x = 1; x < getmaxx(win) - 1; x++) {
	    wmove(win, y, x);
	    waddch(win, num);
	}
    }
}				/* end of fill_panel */

static void
demo_panels(void)
{
    int itmp;
    register int y, x;

    refresh();

    for (y = 0; y < LINES - 1; y++) {
	for (x = 0; x < COLS; x++)
	    wprintw(stdscr, "%d", (y + x) % 10);
    }
    for (y = 0; y < 5; y++) {
	PANEL *p1;
	PANEL *p2;
	PANEL *p3;
	PANEL *p4;
	PANEL *p5;

	p1 = mkpanel(COLOR_RED,
		     LINES / 2 - 2,
		     COLS / 8 + 1,
		     0,
		     0);
	set_panel_userptr(p1, "p1");

	p2 = mkpanel(COLOR_GREEN,
		     LINES / 2 + 1,
		     COLS / 7,
		     LINES / 4,
		     COLS / 10);
	set_panel_userptr(p2, "p2");

	p3 = mkpanel(COLOR_YELLOW,
		     LINES / 4,
		     COLS / 10,
		     LINES / 2,
		     COLS / 9);
	set_panel_userptr(p3, "p3");

	p4 = mkpanel(COLOR_BLUE,
		     LINES / 2 - 2,
		     COLS / 8,
		     LINES / 2 - 2,
		     COLS / 3);
	set_panel_userptr(p4, "p4");

	p5 = mkpanel(COLOR_MAGENTA,
		     LINES / 2 - 2,
		     COLS / 8,
		     LINES / 2,
		     COLS / 2 - 2);
	set_panel_userptr(p5, "p5");

	fill_panel(p1);
	fill_panel(p2);
	fill_panel(p3);
	fill_panel(p4);
	fill_panel(p5);
	hide_panel(p4);
	hide_panel(p5);
	pflush();
	saywhat("press any key to continue");
	wait_a_while(nap_msec);

	saywhat("h3 s1 s2 s4 s5; press any key to continue");
	move_panel(p1, 0, 0);
	hide_panel(p3);
	show_panel(p1);
	show_panel(p2);
	show_panel(p4);
	show_panel(p5);
	pflush();
	wait_a_while(nap_msec);

	saywhat("s1; press any key to continue");
	show_panel(p1);
	pflush();
	wait_a_while(nap_msec);

	saywhat("s2; press any key to continue");
	show_panel(p2);
	pflush();
	wait_a_while(nap_msec);

	saywhat("m2; press any key to continue");
	move_panel(p2, LINES / 3 + 1, COLS / 8);
	pflush();
	wait_a_while(nap_msec);

	saywhat("s3;");
	show_panel(p3);
	pflush();
	wait_a_while(nap_msec);

	saywhat("m3; press any key to continue");
	move_panel(p3, LINES / 4 + 1, COLS / 15);
	pflush();
	wait_a_while(nap_msec);

	saywhat("b3; press any key to continue");
	bottom_panel(p3);
	pflush();
	wait_a_while(nap_msec);

	saywhat("s4; press any key to continue");
	show_panel(p4);
	pflush();
	wait_a_while(nap_msec);

	saywhat("s5; press any key to continue");
	show_panel(p5);
	pflush();
	wait_a_while(nap_msec);

	saywhat("t3; press any key to continue");
	top_panel(p3);
	pflush();
	wait_a_while(nap_msec);

	saywhat("t1; press any key to continue");
	top_panel(p1);
	pflush();
	wait_a_while(nap_msec);

	saywhat("t2; press any key to continue");
	top_panel(p2);
	pflush();
	wait_a_while(nap_msec);

	saywhat("t3; press any key to continue");
	top_panel(p3);
	pflush();
	wait_a_while(nap_msec);

	saywhat("t4; press any key to continue");
	top_panel(p4);
	pflush();
	wait_a_while(nap_msec);

	for (itmp = 0; itmp < 6; itmp++) {
	    WINDOW *w4 = panel_window(p4);
	    WINDOW *w5 = panel_window(p5);

	    saywhat("m4; press any key to continue");
	    wmove(w4, LINES / 8, 1);
	    waddstr(w4, mod[itmp]);
	    move_panel(p4, LINES / 6, itmp * (COLS / 8));
	    wmove(w5, LINES / 6, 1);
	    waddstr(w5, mod[itmp]);
	    pflush();
	    wait_a_while(nap_msec);

	    saywhat("m5; press any key to continue");
	    wmove(w4, LINES / 6, 1);
	    waddstr(w4, mod[itmp]);
	    move_panel(p5, LINES / 3 - 1, (itmp * 10) + 6);
	    wmove(w5, LINES / 8, 1);
	    waddstr(w5, mod[itmp]);
	    pflush();
	    wait_a_while(nap_msec);
	}

	saywhat("m4; press any key to continue");
	move_panel(p4, LINES / 6, itmp * (COLS / 8));
	pflush();
	wait_a_while(nap_msec);

	saywhat("t5; press any key to continue");
	top_panel(p5);
	pflush();
	wait_a_while(nap_msec);

	saywhat("t2; press any key to continue");
	top_panel(p2);
	pflush();
	wait_a_while(nap_msec);

	saywhat("t1; press any key to continue");
	top_panel(p1);
	pflush();
	wait_a_while(nap_msec);

	saywhat("d2; press any key to continue");
	rmpanel(p2);
	pflush();
	wait_a_while(nap_msec);

	saywhat("h3; press any key to continue");
	hide_panel(p3);
	pflush();
	wait_a_while(nap_msec);

	saywhat("d1; press any key to continue");
	rmpanel(p1);
	pflush();
	wait_a_while(nap_msec);

	saywhat("d4; press any key to continue");
	rmpanel(p4);
	pflush();
	wait_a_while(nap_msec);

	saywhat("d5; press any key to continue");
	rmpanel(p5);
	pflush();
	wait_a_while(nap_msec);
	if (nap_msec == 1)
	    break;
	nap_msec = 100L;
    }

    erase();
    endwin();
}

/****************************************************************************
 *
 * Pad tester
 *
 ****************************************************************************/

#define GRIDSIZE	3

static bool pending_pan = FALSE;
static bool show_panner_legend = TRUE;

static int
panner_legend(int line)
{
    static const char *const legend[] =
    {
	"Use arrow keys (or U,D,L,R) to pan, q to quit, ! to shell-out.",
	"Use +,- (or j,k) to grow/shrink the panner vertically.",
	"Use <,> (or h,l) to grow/shrink the panner horizontally.",
	"Number repeats.  Toggle legend:?, timer:t, scroll mark:s."
    };
    int n = (SIZEOF(legend) - (LINES - line));
    if (line < LINES && (n >= 0)) {
	move(line, 0);
	if (show_panner_legend)
	    printw("%s", legend[n]);
	clrtoeol();
	return show_panner_legend;
    }
    return FALSE;
}

static void
panner_h_cleanup(int from_y, int from_x, int to_x)
{
    if (!panner_legend(from_y))
	do_h_line(from_y, from_x, ' ', to_x);
}

static void
panner_v_cleanup(int from_y, int from_x, int to_y)
{
    if (!panner_legend(from_y))
	do_v_line(from_y, from_x, ' ', to_y);
}

static void
panner(WINDOW *pad,
       int top_x, int top_y, int porty, int portx,
       int (*pgetc) (WINDOW *))
{
#if HAVE_GETTIMEOFDAY
    struct timeval before, after;
    bool timing = TRUE;
#endif
    bool scrollers = TRUE;
    int basex = 0;
    int basey = 0;
    int pxmax, pymax, lowend, highend, c;

    getmaxyx(pad, pymax, pxmax);
    scrollok(stdscr, FALSE);	/* we don't want stdscr to scroll! */

    c = KEY_REFRESH;
    do {
#ifdef NCURSES_VERSION
	/*
	 * During shell-out, the user may have resized the window.  Adjust
	 * the port size of the pad to accommodate this.  Ncurses automatically
	 * resizes all of the normal windows to fit on the new screen.
	 */
	if (top_x > COLS)
	    top_x = COLS;
	if (portx > COLS)
	    portx = COLS;
	if (top_y > LINES)
	    top_y = LINES;
	if (porty > LINES)
	    porty = LINES;
#endif
	switch (c) {
	case KEY_REFRESH:
	    erase();

	    /* FALLTHRU */
	case '?':
	    if (c == '?')
		show_panner_legend = !show_panner_legend;
	    panner_legend(LINES - 4);
	    panner_legend(LINES - 3);
	    panner_legend(LINES - 2);
	    panner_legend(LINES - 1);
	    break;
#if HAVE_GETTIMEOFDAY
	case 't':
	    timing = !timing;
	    if (!timing)
		panner_legend(LINES - 1);
	    break;
#endif
	case 's':
	    scrollers = !scrollers;
	    break;

	    /* Move the top-left corner of the pad, keeping the bottom-right
	     * corner fixed.
	     */
	case 'h':		/* increase-columns: move left edge to left */
	    if (top_x <= 0)
		beep();
	    else {
		panner_v_cleanup(top_y, top_x, porty);
		top_x--;
	    }
	    break;

	case 'j':		/* decrease-lines: move top-edge down */
	    if (top_y >= porty)
		beep();
	    else {
		panner_h_cleanup(top_y - 1, top_x - (top_x > 0), portx);
		top_y++;
	    }
	    break;

	case 'k':		/* increase-lines: move top-edge up */
	    if (top_y <= 0)
		beep();
	    else {
		top_y--;
		panner_h_cleanup(top_y, top_x, portx);
	    }
	    break;

	case 'l':		/* decrease-columns: move left-edge to right */
	    if (top_x >= portx)
		beep();
	    else {
		panner_v_cleanup(top_y - (top_y > 0), top_x - 1, porty);
		top_x++;
	    }
	    break;

	    /* Move the bottom-right corner of the pad, keeping the top-left
	     * corner fixed.
	     */
	case KEY_IC:		/* increase-columns: move right-edge to right */
	    if (portx >= pxmax || portx >= COLS)
		beep();
	    else {
		panner_v_cleanup(top_y - (top_y > 0), portx - 1, porty);
		++portx;
	    }
	    break;

	case KEY_IL:		/* increase-lines: move bottom-edge down */
	    if (porty >= pymax || porty >= LINES)
		beep();
	    else {
		panner_h_cleanup(porty - 1, top_x - (top_x > 0), portx);
		++porty;
	    }
	    break;

	case KEY_DC:		/* decrease-columns: move bottom edge up */
	    if (portx <= top_x)
		beep();
	    else {
		portx--;
		panner_v_cleanup(top_y - (top_y > 0), portx, porty);
	    }
	    break;

	case KEY_DL:		/* decrease-lines */
	    if (porty <= top_y)
		beep();
	    else {
		porty--;
		panner_h_cleanup(porty, top_x - (top_x > 0), portx);
	    }
	    break;

	case KEY_LEFT:		/* pan leftwards */
	    if (basex > 0)
		basex--;
	    else
		beep();
	    break;

	case KEY_RIGHT:	/* pan rightwards */
	    if (basex + portx - (pymax > porty) < pxmax)
		basex++;
	    else
		beep();
	    break;

	case KEY_UP:		/* pan upwards */
	    if (basey > 0)
		basey--;
	    else
		beep();
	    break;

	case KEY_DOWN:		/* pan downwards */
	    if (basey + porty - (pxmax > portx) < pymax)
		basey++;
	    else
		beep();
	    break;

	case 'H':
	case KEY_HOME:
	case KEY_FIND:
	    basey = 0;
	    break;

	case 'E':
	case KEY_END:
	case KEY_SELECT:
	    basey = pymax - porty;
	    if (basey < 0)
		basey = 0;
	    break;

	default:
	    beep();
	    break;
	}

	mvaddch(top_y - 1, top_x - 1, ACS_ULCORNER);
	do_v_line(top_y, top_x - 1, ACS_VLINE, porty);
	do_h_line(top_y - 1, top_x, ACS_HLINE, portx);

	if (scrollers && (pxmax > portx - 1)) {
	    int length = (portx - top_x - 1);
	    float ratio = ((float) length) / ((float) pxmax);

	    lowend = (int) (top_x + (basex * ratio));
	    highend = (int) (top_x + ((basex + length) * ratio));

	    do_h_line(porty - 1, top_x, ACS_HLINE, lowend);
	    if (highend < portx) {
		attron(A_REVERSE);
		do_h_line(porty - 1, lowend, ' ', highend + 1);
		attroff(A_REVERSE);
		do_h_line(porty - 1, highend + 1, ACS_HLINE, portx);
	    }
	} else
	    do_h_line(porty - 1, top_x, ACS_HLINE, portx);

	if (scrollers && (pymax > porty - 1)) {
	    int length = (porty - top_y - 1);
	    float ratio = ((float) length) / ((float) pymax);

	    lowend = (int) (top_y + (basey * ratio));
	    highend = (int) (top_y + ((basey + length) * ratio));

	    do_v_line(top_y, portx - 1, ACS_VLINE, lowend);
	    if (highend < porty) {
		attron(A_REVERSE);
		do_v_line(lowend, portx - 1, ' ', highend + 1);
		attroff(A_REVERSE);
		do_v_line(highend + 1, portx - 1, ACS_VLINE, porty);
	    }
	} else
	    do_v_line(top_y, portx - 1, ACS_VLINE, porty);

	mvaddch(top_y - 1, portx - 1, ACS_URCORNER);
	mvaddch(porty - 1, top_x - 1, ACS_LLCORNER);
	mvaddch(porty - 1, portx - 1, ACS_LRCORNER);

	if (!pending_pan) {
#if HAVE_GETTIMEOFDAY
	    gettimeofday(&before, 0);
#endif
	    wnoutrefresh(stdscr);

	    pnoutrefresh(pad,
			 basey, basex,
			 top_y, top_x,
			 porty - (pxmax > portx) - 1,
			 portx - (pymax > porty) - 1);

	    doupdate();
#if HAVE_GETTIMEOFDAY
	    if (timing) {
		double elapsed;
		gettimeofday(&after, 0);
		elapsed = (after.tv_sec + after.tv_usec / 1.0e6)
		    - (before.tv_sec + before.tv_usec / 1.0e6);
		move(LINES - 1, COLS - 20);
		printw("Secs: %2.03f", elapsed);
		refresh();
	    }
#endif
	}

    } while
	((c = pgetc(pad)) != KEY_EXIT);

    scrollok(stdscr, TRUE);	/* reset to driver's default */
}

static int
padgetch(WINDOW *win)
{
    static int count;
    static int last;
    int c;

    if ((pending_pan = (count > 0)) != FALSE) {
	count--;
	pending_pan = (count != 0);
    } else {
	for (;;) {
	    switch (c = wGetchar(win)) {
	    case '!':
		ShellOut(FALSE);
		/* FALLTHRU */
	    case CTRL('r'):
		endwin();
		refresh();
		c = KEY_REFRESH;
		break;
	    case CTRL('l'):
		c = KEY_REFRESH;
		break;
	    case 'U':
		c = KEY_UP;
		break;
	    case 'D':
		c = KEY_DOWN;
		break;
	    case 'R':
		c = KEY_RIGHT;
		break;
	    case 'L':
		c = KEY_LEFT;
		break;
	    case '+':
		c = KEY_IL;
		break;
	    case '-':
		c = KEY_DL;
		break;
	    case '>':
		c = KEY_IC;
		break;
	    case '<':
		c = KEY_DC;
		break;
	    case ERR:		/* FALLTHRU */
	    case 'q':
		count = 0;
		c = KEY_EXIT;
		break;
	    default:
		if (c >= '0' && c <= '9') {
		    count = count * 10 + (c - '0');
		    continue;
		}
		break;
	    }
	    last = c;
	    break;
	}
	if (count > 0)
	    count--;
    }
    return (last);
}

#define PAD_HIGH 200
#define PAD_WIDE 200

static void
demo_pad(void)
/* Demonstrate pads. */
{
    int i, j;
    unsigned gridcount = 0;
    WINDOW *panpad = newpad(PAD_HIGH, PAD_WIDE);

    if (panpad == 0) {
	Cannot("cannot create requested pad");
	return;
    }

    for (i = 0; i < PAD_HIGH; i++) {
	for (j = 0; j < PAD_WIDE; j++)
	    if (i % GRIDSIZE == 0 && j % GRIDSIZE == 0) {
		if (i == 0 || j == 0)
		    waddch(panpad, '+');
		else
		    waddch(panpad, (chtype) ('A' + (gridcount++ % 26)));
	    } else if (i % GRIDSIZE == 0)
		waddch(panpad, '-');
	    else if (j % GRIDSIZE == 0)
		waddch(panpad, '|');
	    else
		waddch(panpad, ' ');
    }
    panner_legend(LINES - 4);
    panner_legend(LINES - 3);
    panner_legend(LINES - 2);
    panner_legend(LINES - 1);

    keypad(panpad, TRUE);

    /* Make the pad (initially) narrow enough that a trace file won't wrap.
     * We'll still be able to widen it during a test, since that's required
     * for testing boundaries.
     */
    panner(panpad, 2, 2, LINES - 5, COLS - 15, padgetch);

    delwin(panpad);
    endwin();
    erase();
}
#endif /* USE_LIBPANEL */

/****************************************************************************
 *
 * Tests from John Burnell's PDCurses tester
 *
 ****************************************************************************/

static void
Continue(WINDOW *win)
{
    noecho();
    wmove(win, 10, 1);
    mvwaddstr(win, 10, 1, " Press any key to continue");
    wrefresh(win);
    wGetchar(win);
}

static void
flushinp_test(WINDOW *win)
/* Input test, adapted from John Burnell's PDCurses tester */
{
    int w, h, bx, by, sw, sh, i;

    WINDOW *subWin;
    wclear(win);

    getmaxyx(win, h, w);
    getbegyx(win, by, bx);
    sw = w / 3;
    sh = h / 3;
    if ((subWin = subwin(win, sh, sw, by + h - sh - 2, bx + w - sw - 2)) == 0)
	return;

#ifdef A_COLOR
    if (has_colors()) {
	init_pair(2, COLOR_CYAN, COLOR_BLUE);
	wbkgd(subWin, COLOR_PAIR(2) | ' ');
    }
#endif
    wattrset(subWin, A_BOLD);
    box(subWin, ACS_VLINE, ACS_HLINE);
    mvwaddstr(subWin, 2, 1, "This is a subwindow");
    wrefresh(win);

    /*
     * This used to set 'nocbreak()'.  However, Alexander Lukyanov says that
     * it only happened to "work" on SVr4 because that implementation does not
     * emulate nocbreak+noecho mode, whereas ncurses does.  To get the desired
     * test behavior, we're using 'cbreak()', which will allow a single
     * character to return without needing a newline. - T.Dickey 1997/10/11.
     */
    cbreak();
    mvwaddstr(win, 0, 1, "This is a test of the flushinp() call.");

    mvwaddstr(win, 2, 1, "Type random keys for 5 seconds.");
    mvwaddstr(win, 3, 1,
	      "These should be discarded (not echoed) after the subwindow goes away.");
    wrefresh(win);

    for (i = 0; i < 5; i++) {
	mvwprintw(subWin, 1, 1, "Time = %d", i);
	wrefresh(subWin);
	napms(1000);
	flushinp();
    }

    delwin(subWin);
    werase(win);
    flash();
    wrefresh(win);
    napms(1000);

    mvwaddstr(win, 2, 1,
	      "If you were still typing when the window timer expired,");
    mvwaddstr(win, 3, 1,
	      "or else you typed nothing at all while it was running,");
    mvwaddstr(win, 4, 1,
	      "test was invalid.  You'll see garbage or nothing at all. ");
    mvwaddstr(win, 6, 1, "Press a key");
    wmove(win, 9, 10);
    wrefresh(win);
    echo();
    wGetchar(win);
    flushinp();
    mvwaddstr(win, 12, 0,
	      "If you see any key other than what you typed, flushinp() is broken.");
    Continue(win);

    wmove(win, 9, 10);
    wdelch(win);
    wrefresh(win);
    wmove(win, 12, 0);
    clrtoeol();
    waddstr(win,
	    "What you typed should now have been deleted; if not, wdelch() failed.");
    Continue(win);

    cbreak();
}

/****************************************************************************
 *
 * Menu test
 *
 ****************************************************************************/

#if USE_LIBMENU

#define MENU_Y	8
#define MENU_X	8

static int
menu_virtualize(int c)
{
    if (c == '\n' || c == KEY_EXIT)
	return (MAX_COMMAND + 1);
    else if (c == 'u')
	return (REQ_SCR_ULINE);
    else if (c == 'd')
	return (REQ_SCR_DLINE);
    else if (c == 'b' || c == KEY_NPAGE)
	return (REQ_SCR_UPAGE);
    else if (c == 'f' || c == KEY_PPAGE)
	return (REQ_SCR_DPAGE);
    else if (c == 'n' || c == KEY_DOWN)
	return (REQ_NEXT_ITEM);
    else if (c == 'p' || c == KEY_UP)
	return (REQ_PREV_ITEM);
    else if (c == ' ')
	return (REQ_TOGGLE_ITEM);
    else {
	if (c != KEY_MOUSE)
	    beep();
	return (c);
    }
}

static const char *animals[] =
{
    "Lions", "Tigers", "Bears", "(Oh my!)", "Newts", "Platypi", "Lemurs",
    (char *) 0
};

static void
menu_test(void)
{
    MENU *m;
    ITEM *items[SIZEOF(animals)];
    ITEM **ip = items;
    const char **ap;
    int mrows, mcols, c;
    WINDOW *menuwin;

#ifdef NCURSES_MOUSE_VERSION
    mousemask(ALL_MOUSE_EVENTS, (mmask_t *) 0);
#endif
    mvaddstr(0, 0, "This is the menu test:");
    mvaddstr(2, 0, "  Use up and down arrow to move the select bar.");
    mvaddstr(3, 0, "  'n' and 'p' act like arrows.");
    mvaddstr(4, 0,
	     "  'b' and 'f' scroll up/down (page), 'u' and 'd' (line).");
    mvaddstr(5, 0, "  Press return to exit.");
    refresh();

    for (ap = animals; *ap; ap++)
	*ip++ = new_item(*ap, "");
    *ip = (ITEM *) 0;

    m = new_menu(items);

    set_menu_format(m, (SIZEOF(animals) + 1) / 2, 1);
    scale_menu(m, &mrows, &mcols);

    menuwin = newwin(mrows + 2, mcols + 2, MENU_Y, MENU_X);
    set_menu_win(m, menuwin);
    keypad(menuwin, TRUE);
    box(menuwin, 0, 0);

    set_menu_sub(m, derwin(menuwin, mrows, mcols, 1, 1));

    post_menu(m);

    while ((c = menu_driver(m, menu_virtualize(wGetchar(menuwin)))) != E_UNKNOWN_COMMAND) {
	if (c == E_REQUEST_DENIED)
	    beep();
	continue;
    }

    (void) mvprintw(LINES - 2, 0,
		    "You chose: %s\n", item_name(current_item(m)));
    (void) addstr("Press any key to continue...");
    wGetchar(stdscr);

    unpost_menu(m);
    delwin(menuwin);

    free_menu(m);
    for (ip = items; *ip; ip++)
	free_item(*ip);
#ifdef NCURSES_MOUSE_VERSION
    mousemask(0, (mmask_t *) 0);
#endif
}

#ifdef TRACE
#define T_TBL(name) { #name, name }
static struct {
    const char *name;
    int mask;
} t_tbl[] = {

    T_TBL(TRACE_DISABLE),
	T_TBL(TRACE_TIMES),
	T_TBL(TRACE_TPUTS),
	T_TBL(TRACE_UPDATE),
	T_TBL(TRACE_MOVE),
	T_TBL(TRACE_CHARPUT),
	T_TBL(TRACE_ORDINARY),
	T_TBL(TRACE_CALLS),
	T_TBL(TRACE_VIRTPUT),
	T_TBL(TRACE_IEVENT),
	T_TBL(TRACE_BITS),
	T_TBL(TRACE_ICALLS),
	T_TBL(TRACE_CCALLS),
	T_TBL(TRACE_DATABASE),
	T_TBL(TRACE_ATTRS),
	T_TBL(TRACE_MAXIMUM),
    {
	(char *) 0, 0
    }
};

static char *
tracetrace(int tlevel)
{
    static char *buf;
    int n;

    if (buf == 0) {
	size_t need = 12;
	for (n = 0; t_tbl[n].name != 0; n++)
	    need += strlen(t_tbl[n].name) + 2;
	buf = (char *) malloc(need);
    }
    sprintf(buf, "0x%02x = {", tlevel);
    if (tlevel == 0) {
	sprintf(buf + strlen(buf), "%s, ", t_tbl[0].name);
    } else {
	for (n = 1; t_tbl[n].name != 0; n++)
	    if ((tlevel & t_tbl[n].mask) == t_tbl[n].mask) {
		strcat(buf, t_tbl[n].name);
		strcat(buf, ", ");
	    }
    }
    if (buf[strlen(buf) - 2] == ',')
	buf[strlen(buf) - 2] = '\0';
    return (strcat(buf, "}"));
}

/* fake a dynamically reconfigurable menu using the 0th entry to deselect
 * the others
 */
static int
run_trace_menu(MENU * m)
{
    ITEM **items;
    ITEM *i, **p;

    for (;;) {
	bool changed = FALSE;
	switch (menu_driver(m, menu_virtualize(wGetchar(menu_win(m))))) {
	case E_UNKNOWN_COMMAND:
	    return FALSE;
	default:
	    items = menu_items(m);
	    i = current_item(m);
	    if (i == items[0]) {
		if (item_value(i)) {
		    for (p = items + 1; *p != 0; p++)
			if (item_value(*p)) {
			    set_item_value(*p, FALSE);
			    changed = TRUE;
			}
		}
	    } else {
		for (p = items + 1; *p != 0; p++)
		    if (item_value(*p)) {
			set_item_value(items[0], FALSE);
			changed = TRUE;
			break;
		    }
	    }
	    if (!changed)
		return TRUE;
	}
    }
}

static void
trace_set(void)
/* interactively set the trace level */
{
    MENU *m;
    ITEM *items[SIZEOF(t_tbl)];
    ITEM **ip = items;
    int mrows, mcols, newtrace;
    int n;
    WINDOW *menuwin;

    mvaddstr(0, 0, "Interactively set trace level:");
    mvaddstr(2, 0, "  Press space bar to toggle a selection.");
    mvaddstr(3, 0, "  Use up and down arrow to move the select bar.");
    mvaddstr(4, 0, "  Press return to set the trace level.");
    mvprintw(6, 0, "(Current trace level is %s)", tracetrace(_nc_tracing));

    refresh();

    for (n = 0; t_tbl[n].name != 0; n++)
	*ip++ = new_item(t_tbl[n].name, "");
    *ip = (ITEM *) 0;

    m = new_menu(items);

    set_menu_format(m, 0, 2);
    scale_menu(m, &mrows, &mcols);

    menu_opts_off(m, O_ONEVALUE);
    menuwin = newwin(mrows + 2, mcols + 2, MENU_Y, MENU_X);
    set_menu_win(m, menuwin);
    keypad(menuwin, TRUE);
    box(menuwin, 0, 0);

    set_menu_sub(m, derwin(menuwin, mrows, mcols, 1, 1));

    post_menu(m);

    for (ip = menu_items(m); *ip; ip++) {
	int mask = t_tbl[item_index(*ip)].mask;
	if (mask == 0)
	    set_item_value(*ip, _nc_tracing == 0);
	else if ((mask & _nc_tracing) == mask)
	    set_item_value(*ip, TRUE);
    }

    while (run_trace_menu(m))
	continue;

    newtrace = 0;
    for (ip = menu_items(m); *ip; ip++)
	if (item_value(*ip))
	    newtrace |= t_tbl[item_index(*ip)].mask;
    trace(newtrace);
    _tracef("trace level interactively set to %s", tracetrace(_nc_tracing));

    (void) mvprintw(LINES - 2, 0,
		    "Trace level is %s\n", tracetrace(_nc_tracing));
    (void) addstr("Press any key to continue...");
    wGetchar(stdscr);

    unpost_menu(m);
    delwin(menuwin);

    free_menu(m);
    for (ip = items; *ip; ip++)
	free_item(*ip);
}
#endif /* TRACE */
#endif /* USE_LIBMENU */

/****************************************************************************
 *
 * Forms test
 *
 ****************************************************************************/
#if USE_LIBFORM
static FIELD *
make_label(int frow, int fcol, NCURSES_CONST char *label)
{
    FIELD *f = new_field(1, strlen(label), frow, fcol, 0, 0);

    if (f) {
	set_field_buffer(f, 0, label);
	set_field_opts(f, field_opts(f) & ~O_ACTIVE);
    }
    return (f);
}

static FIELD *
make_field(int frow, int fcol, int rows, int cols, bool secure)
{
    FIELD *f = new_field(rows, cols, frow, fcol, 0, secure ? 1 : 0);

    if (f) {
	set_field_back(f, A_UNDERLINE);
	set_field_userptr(f, (void *) 0);
    }
    return (f);
}

static void
display_form(FORM * f)
{
    WINDOW *w;
    int rows, cols;

    scale_form(f, &rows, &cols);

    if ((w = newwin(rows + 2, cols + 4, 0, 0)) != (WINDOW *) 0) {
	set_form_win(f, w);
	set_form_sub(f, derwin(w, rows, cols, 1, 2));
	box(w, 0, 0);
	keypad(w, TRUE);
    }

    if (post_form(f) != E_OK)
	wrefresh(w);
}

static void
erase_form(FORM * f)
{
    WINDOW *w = form_win(f);
    WINDOW *s = form_sub(f);

    unpost_form(f);
    werase(w);
    wrefresh(w);
    delwin(s);
    delwin(w);
}

static int
edit_secure(FIELD * me, int c)
{
    int rows, cols, frow, fcol, nrow, nbuf;

    if (field_info(me, &rows, &cols, &frow, &fcol, &nrow, &nbuf) == E_OK
	&& nbuf > 0) {
	char temp[80];
	long len;

	strcpy(temp, field_buffer(me, 1));
	len = (long) (char *) field_userptr(me);
	if (c <= KEY_MAX) {
	    if (isgraph(c)) {
		temp[len++] = c;
		temp[len] = 0;
		set_field_buffer(me, 1, temp);
		c = '*';
	    } else {
		c = 0;
	    }
	} else {
	    switch (c) {
	    case REQ_BEG_FIELD:
	    case REQ_CLR_EOF:
	    case REQ_CLR_EOL:
	    case REQ_DEL_LINE:
	    case REQ_DEL_WORD:
	    case REQ_DOWN_CHAR:
	    case REQ_END_FIELD:
	    case REQ_INS_CHAR:
	    case REQ_INS_LINE:
	    case REQ_LEFT_CHAR:
	    case REQ_NEW_LINE:
	    case REQ_NEXT_WORD:
	    case REQ_PREV_WORD:
	    case REQ_RIGHT_CHAR:
	    case REQ_UP_CHAR:
		c = 0;		/* we don't want to do inline editing */
		break;
	    case REQ_CLR_FIELD:
		if (len) {
		    temp[0] = 0;
		    set_field_buffer(me, 1, temp);
		}
		break;
	    case REQ_DEL_CHAR:
	    case REQ_DEL_PREV:
		if (len) {
		    temp[--len] = 0;
		    set_field_buffer(me, 1, temp);
		}
		break;
	    }
	}
	set_field_userptr(me, (void *) len);
    }
    return c;
}

static int
form_virtualize(FORM * f, WINDOW *w)
{
    static const struct {
	int code;
	int result;
    } lookup[] = {
	{
	    CTRL('A'), REQ_NEXT_CHOICE
	},
	{
	    CTRL('B'), REQ_PREV_WORD
	},
	{
	    CTRL('C'), REQ_CLR_EOL
	},
	{
	    CTRL('D'), REQ_DOWN_FIELD
	},
	{
	    CTRL('E'), REQ_END_FIELD
	},
	{
	    CTRL('F'), REQ_NEXT_PAGE
	},
	{
	    CTRL('G'), REQ_DEL_WORD
	},
	{
	    CTRL('H'), REQ_DEL_PREV
	},
	{
	    CTRL('I'), REQ_INS_CHAR
	},
	{
	    CTRL('K'), REQ_CLR_EOF
	},
	{
	    CTRL('L'), REQ_LEFT_FIELD
	},
	{
	    CTRL('M'), REQ_NEW_LINE
	},
	{
	    CTRL('N'), REQ_NEXT_FIELD
	},
	{
	    CTRL('O'), REQ_INS_LINE
	},
	{
	    CTRL('P'), REQ_PREV_FIELD
	},
	{
	    CTRL('R'), REQ_RIGHT_FIELD
	},
	{
	    CTRL('S'), REQ_BEG_FIELD
	},
	{
	    CTRL('U'), REQ_UP_FIELD
	},
	{
	    CTRL('V'), REQ_DEL_CHAR
	},
	{
	    CTRL('W'), REQ_NEXT_WORD
	},
	{
	    CTRL('X'), REQ_CLR_FIELD
	},
	{
	    CTRL('Y'), REQ_DEL_LINE
	},
	{
	    CTRL('Z'), REQ_PREV_CHOICE
	},
	{
	    ESCAPE, MAX_FORM_COMMAND + 1
	},
	{
	    KEY_BACKSPACE, REQ_DEL_PREV
	},
	{
	    KEY_DOWN, REQ_DOWN_CHAR
	},
	{
	    KEY_END, REQ_LAST_FIELD
	},
	{
	    KEY_HOME, REQ_FIRST_FIELD
	},
	{
	    KEY_LEFT, REQ_LEFT_CHAR
	},
	{
	    KEY_LL, REQ_LAST_FIELD
	},
	{
	    KEY_NEXT, REQ_NEXT_FIELD
	},
	{
	    KEY_NPAGE, REQ_NEXT_PAGE
	},
	{
	    KEY_PPAGE, REQ_PREV_PAGE
	},
	{
	    KEY_PREVIOUS, REQ_PREV_FIELD
	},
	{
	    KEY_RIGHT, REQ_RIGHT_CHAR
	},
	{
	    KEY_UP, REQ_UP_CHAR
	},
	{
	    QUIT, MAX_FORM_COMMAND + 1
	}
    };

    static int mode = REQ_INS_MODE;
    int c = wGetchar(w);
    unsigned n;
    FIELD *me = current_field(f);

    if (c == CTRL(']')) {
	if (mode == REQ_INS_MODE)
	    mode = REQ_OVL_MODE;
	else
	    mode = REQ_INS_MODE;
	c = mode;
    } else {
	for (n = 0; n < SIZEOF(lookup); n++) {
	    if (lookup[n].code == c) {
		c = lookup[n].result;
		break;
	    }
	}
    }

    /*
     * Force the field that the user is typing into to be in reverse video,
     * while the other fields are shown underlined.
     */
    if (c <= KEY_MAX) {
	c = edit_secure(me, c);
	set_field_back(me, A_REVERSE);
    } else if (c <= MAX_FORM_COMMAND) {
	c = edit_secure(me, c);
	set_field_back(me, A_UNDERLINE);
    }
    return c;
}

static int
my_form_driver(FORM * form, int c)
{
    if (c == (MAX_FORM_COMMAND + 1)
	&& form_driver(form, REQ_VALIDATION) == E_OK)
	return (TRUE);
    else {
	beep();
	return (FALSE);
    }
}

static void
demo_forms(void)
{
    WINDOW *w;
    FORM *form;
    FIELD *f[12], *secure;
    int finished = 0, c;
    unsigned n = 0;

    move(18, 0);
    addstr("Defined form-traversal keys:   ^Q/ESC- exit form\n");
    addstr("^N   -- go to next field       ^P  -- go to previous field\n");
    addstr("Home -- go to first field      End -- go to last field\n");
    addstr("^L   -- go to field to left    ^R  -- go to field to right\n");
    addstr("^U   -- move upward to field   ^D  -- move downward to field\n");
    addstr("^W   -- go to next word        ^B  -- go to previous word\n");
    addstr("^S   -- go to start of field   ^E  -- go to end of field\n");
    addstr("^H   -- delete previous char   ^Y  -- delete line\n");
    addstr("^G   -- delete current word    ^C  -- clear to end of line\n");
    addstr("^K   -- clear to end of field  ^X  -- clear field\n");
    addstr("Arrow keys move within a field as you would expect.");

    mvaddstr(4, 57, "Forms Entry Test");

    refresh();

    /* describe the form */
    f[n++] = make_label(0, 15, "Sample Form");
    f[n++] = make_label(2, 0, "Last Name");
    f[n++] = make_field(3, 0, 1, 18, FALSE);
    f[n++] = make_label(2, 20, "First Name");
    f[n++] = make_field(3, 20, 1, 12, FALSE);
    f[n++] = make_label(2, 34, "Middle Name");
    f[n++] = make_field(3, 34, 1, 12, FALSE);
    f[n++] = make_label(5, 0, "Comments");
    f[n++] = make_field(6, 0, 4, 46, FALSE);
    f[n++] = make_label(5, 20, "Password:");
    secure =
	f[n++] = make_field(5, 30, 1, 9, TRUE);
    f[n++] = (FIELD *) 0;

    form = new_form(f);

    display_form(form);

    w = form_win(form);
    raw();
    nonl();			/* lets us read ^M's */
    while (!finished) {
	switch (form_driver(form, c = form_virtualize(form, w))) {
	case E_OK:
	    mvaddstr(5, 57, field_buffer(secure, 1));
	    clrtoeol();
	    refresh();
	    break;
	case E_UNKNOWN_COMMAND:
	    finished = my_form_driver(form, c);
	    break;
	default:
	    beep();
	    break;
	}
    }

    erase_form(form);

    free_form(form);
    for (c = 0; f[c] != 0; c++)
	free_field(f[c]);
    noraw();
    nl();
}
#endif /* USE_LIBFORM */

/****************************************************************************
 *
 * Overlap test
 *
 ****************************************************************************/

static void
fillwin(WINDOW *win, char ch)
{
    int y, x;
    int y1, x1;

    getmaxyx(win, y1, x1);
    for (y = 0; y < y1; y++) {
	wmove(win, y, 0);
	for (x = 0; x < x1; x++)
	    waddch(win, ch);
    }
}

static void
crosswin(WINDOW *win, char ch)
{
    int y, x;
    int y1, x1;

    getmaxyx(win, y1, x1);
    for (y = 0; y < y1; y++) {
	for (x = 0; x < x1; x++)
	    if (((x > (x1 - 1) / 3) && (x <= (2 * (x1 - 1)) / 3))
		|| (((y > (y1 - 1) / 3) && (y <= (2 * (y1 - 1)) / 3)))) {
		wmove(win, y, x);
		waddch(win, ch);
	    }
    }
}

static void
overlap_test(void)
/* test effects of overlapping windows */
{
    int ch;

    WINDOW *win1 = newwin(9, 20, 3, 3);
    WINDOW *win2 = newwin(9, 20, 9, 16);

    raw();
    refresh();
    move(0, 0);
    printw("This test shows the behavior of wnoutrefresh() with respect to\n");
    printw("the shared region of two overlapping windows A and B.  The cross\n");
    printw("pattern in each window does not overlap the other.\n");

    move(18, 0);
    printw("a = refresh A, then B, then doupdate. b = refresh B, then A, then doupdaute\n");
    printw("c = fill window A with letter A.      d = fill window B with letter B.\n");
    printw("e = cross pattern in window A.        f = cross pattern in window B.\n");
    printw("g = clear window A.                   h = clear window B.\n");
    printw("i = overwrite A onto B.               j = overwrite B onto A.\n");
    printw("^Q/ESC = terminate test.");

    while ((ch = Getchar()) != QUIT && ch != ESCAPE)
	switch (ch) {
	case 'a':		/* refresh window A first, then B */
	    wnoutrefresh(win1);
	    wnoutrefresh(win2);
	    doupdate();
	    break;

	case 'b':		/* refresh window B first, then A */
	    wnoutrefresh(win2);
	    wnoutrefresh(win1);
	    doupdate();
	    break;

	case 'c':		/* fill window A so it's visible */
	    fillwin(win1, 'A');
	    break;

	case 'd':		/* fill window B so it's visible */
	    fillwin(win2, 'B');
	    break;

	case 'e':		/* cross test pattern in window A */
	    crosswin(win1, 'A');
	    break;

	case 'f':		/* cross test pattern in window A */
	    crosswin(win2, 'B');
	    break;

	case 'g':		/* clear window A */
	    wclear(win1);
	    wmove(win1, 0, 0);
	    break;

	case 'h':		/* clear window B */
	    wclear(win2);
	    wmove(win2, 0, 0);
	    break;

	case 'i':		/* overwrite A onto B */
	    overwrite(win1, win2);
	    break;

	case 'j':		/* overwrite B onto A */
	    overwrite(win2, win1);
	    break;
	}

    delwin(win2);
    delwin(win1);
    erase();
    endwin();
}

/****************************************************************************
 *
 * Main sequence
 *
 ****************************************************************************/

static bool
do_single_test(const char c)
/* perform a single specified test */
{
    switch (c) {
    case 'a':
	getch_test();
	break;

#if USE_WIDEC_SUPPORT
    case 'A':
	get_wch_test();
	break;
#endif

    case 'b':
	attr_test();
	break;

    case 'c':
	if (!has_colors())
	    Cannot("does not support color.");
	else
	    color_test();
	break;

    case 'd':
	if (!has_colors())
	    Cannot("does not support color.");
	else if (!can_change_color())
	    Cannot("has hardwired color values.");
	else
	    color_edit();
	break;

    case 'e':
	slk_test();
	break;

    case 'f':
	acs_display();
	break;

#if USE_WIDEC_SUPPORT
    case 'F':
	wide_acs_display();
	break;
#endif

#if USE_LIBPANEL
    case 'o':
	demo_panels();
	break;
#endif

    case 'g':
	acs_and_scroll();
	break;

    case 'i':
	flushinp_test(stdscr);
	break;

    case 'k':
	test_sgr_attributes();
	break;

#if USE_LIBMENU
    case 'm':
	menu_test();
	break;
#endif

#if USE_LIBPANEL
    case 'p':
	demo_pad();
	break;
#endif

#if USE_LIBFORM
    case 'r':
	demo_forms();
	break;
#endif

    case 's':
	overlap_test();
	break;

#if USE_LIBMENU && defined(TRACE)
    case 't':
	trace_set();
	break;
#endif

    case '?':
	break;

    default:
	return FALSE;
    }

    return TRUE;
}

static void
usage(void)
{
    static const char *const tbl[] =
    {
	"Usage: ncurses [options]"
	,""
	,"Options:"
#ifdef NCURSES_VERSION
	,"  -a f,b   set default-colors (assumed white-on-black)"
	,"  -d       use default-colors if terminal supports them"
#endif
	,"  -e fmt   specify format for soft-keys test (e)"
	,"  -f       rip-off footer line (can repeat)"
	,"  -h       rip-off header line (can repeat)"
	,"  -s msec  specify nominal time for panel-demo (default: 1, to hold)"
#ifdef TRACE
	,"  -t mask  specify default trace-level (may toggle with ^T)"
#endif
    };
    size_t n;
    for (n = 0; n < SIZEOF(tbl); n++)
	fprintf(stderr, "%s\n", tbl[n]);
    ExitProgram(EXIT_FAILURE);
}

static void
set_terminal_modes(void)
{
    noraw();
    cbreak();
    noecho();
    scrollok(stdscr, TRUE);
    idlok(stdscr, TRUE);
    keypad(stdscr, TRUE);
}

#ifdef SIGUSR1
static RETSIGTYPE
announce_sig(int sig)
{
    (void) fprintf(stderr, "Handled signal %d\r\n", sig);
}
#endif

static int
rip_footer(WINDOW *win, int cols)
{
    wbkgd(win, A_REVERSE);
    werase(win);
    wmove(win, 0, 0);
    wprintw(win, "footer: %d columns", cols);
    wnoutrefresh(win);
    return OK;
}

static int
rip_header(WINDOW *win, int cols)
{
    wbkgd(win, A_REVERSE);
    werase(win);
    wmove(win, 0, 0);
    wprintw(win, "header: %d columns", cols);
    wnoutrefresh(win);
    return OK;
}

/*+-------------------------------------------------------------------------
	main(argc,argv)
--------------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
    int command, c;
    int my_e_param = 1;
#ifdef NCURSES_VERSION
    int default_fg = COLOR_WHITE;
    int default_bg = COLOR_BLACK;
    bool assumed_colors = FALSE;
    bool default_colors = FALSE;
#endif

#if HAVE_LOCALE_H
    setlocale(LC_CTYPE, "");
#endif

    while ((c = getopt(argc, argv, "a:de:fhs:t:")) != EOF) {
	switch (c) {
#ifdef NCURSES_VERSION
	case 'a':
	    assumed_colors = TRUE;
	    sscanf(optarg, "%d,%d", &default_fg, &default_bg);
	    break;
	case 'd':
	    default_colors = TRUE;
	    break;
#endif
	case 'e':
	    my_e_param = atoi(optarg);
#ifdef NCURSES_VERSION
	    if (my_e_param > 3)	/* allow extended layouts */
		usage();
#else
	    if (my_e_param > 1)
		usage();
#endif
	    break;
	case 'f':
	    ripoffline(-1, rip_footer);
	    break;
	case 'h':
	    ripoffline(1, rip_header);
	    break;
#if USE_LIBPANEL
	case 's':
	    nap_msec = atol(optarg);
	    break;
#endif
#ifdef TRACE
	case 't':
	    save_trace = atoi(optarg);
	    break;
#endif
	default:
	    usage();
	}
    }

    /*
     * If there's no menus (unlikely for ncurses!), then we'll have to set
     * tracing on initially, just in case the user wants to test something that
     * doesn't involve wGetchar.
     */
#ifdef TRACE
    /* enable debugging */
#if !USE_LIBMENU
    trace(save_trace);
#else
    if (!isatty(fileno(stdin)))
	trace(save_trace);
#endif /* USE_LIBMENU */
#endif /* TRACE */

    /* tell it we're going to play with soft keys */
    slk_init(my_e_param);

#ifdef SIGUSR1
    /* set up null signal catcher so we can see what interrupts to getch do */
    signal(SIGUSR1, announce_sig);
#endif

    /* we must initialize the curses data structure only once */
    initscr();
    bkgdset(BLANK);

    /* tests, in general, will want these modes */
    if (has_colors()) {
	start_color();
#ifdef NCURSES_VERSION_PATCH
	max_colors = COLORS > 16 ? 16 : COLORS;
#if HAVE_USE_DEFAULT_COLORS
	if (default_colors)
	    use_default_colors();
#if NCURSES_VERSION_PATCH >= 20000708
	else if (assumed_colors)
	    assume_default_colors(default_fg, default_bg);
#endif
#endif
#else /* normal SVr4 curses */
	max_colors = COLORS > 8 ? 8 : COLORS;
#endif
	max_pairs = (max_colors * max_colors);
	if (max_pairs < COLOR_PAIRS)
	    max_pairs = COLOR_PAIRS;
    }
    set_terminal_modes();
    def_prog_mode();

    /*
     * Return to terminal mode, so we're guaranteed of being able to
     * select terminal commands even if the capabilities are wrong.
     */
    endwin();

#if HAVE_CURSES_VERSION
    (void) printf("Welcome to %s.  Press ? for help.\n", curses_version());
#elif defined(NCURSES_VERSION_MAJOR) && defined(NCURSES_VERSION_MINOR) && defined(NCURSES_VERSION_PATCH)
    (void) printf("Welcome to ncurses %d.%d.%d.  Press ? for help.\n",
		  NCURSES_VERSION_MAJOR,
		  NCURSES_VERSION_MINOR,
		  NCURSES_VERSION_PATCH);
#else
    (void) puts("Welcome to ncurses.  Press ? for help.");
#endif

    do {
	(void) puts("This is the ncurses main menu");
	(void) puts("a = keyboard and mouse input test");
#if USE_WIDEC_SUPPORT
	(void) puts("A = wide-character keyboard and mouse input test");
#endif
	(void) puts("b = character attribute test");
	(void) puts("c = color test pattern");
	(void) puts("d = edit RGB color values");
	(void) puts("e = exercise soft keys");
	(void) puts("f = display ACS characters");
#if USE_WIDEC_SUPPORT
	(void) puts("F = display Wide-ACS characters");
#endif
	(void) puts("g = display windows and scrolling");
	(void) puts("i = test of flushinp()");
	(void) puts("k = display character attributes");
#if USE_LIBMENU
	(void) puts("m = menu code test");
#endif
#if USE_LIBPANEL
	(void) puts("o = exercise panels library");
	(void) puts("p = exercise pad features");
	(void) puts("q = quit");
#endif
#if USE_LIBFORM
	(void) puts("r = exercise forms code");
#endif
	(void) puts("s = overlapping-refresh test");
#if USE_LIBMENU && defined(TRACE)
	(void) puts("t = set trace level");
#endif
	(void) puts("? = repeat this command summary");

	(void) fputs("> ", stdout);
	(void) fflush(stdout);	/* necessary under SVr4 curses */

	/*
	 * This used to be an 'fgets()' call.  However (on Linux, at least)
	 * mixing stream I/O and 'read()' (used in the library) causes the
	 * input stream to be flushed when switching between the two.
	 */
	command = 0;
	for (;;) {
	    char ch;
	    if (read(fileno(stdin), &ch, 1) <= 0) {
		if (command == 0)
		    command = 'q';
		break;
	    } else if (command == 0 && !isspace(UChar(ch))) {
		command = ch;
	    } else if (ch == '\n' || ch == '\r') {
		if (command != 0)
		    break;
		(void) fputs("> ", stdout);
		(void) fflush(stdout);
	    }
	}

	if (do_single_test(command)) {
	    /*
	     * This may be overkill; it's intended to reset everything back
	     * to the initial terminal modes so that tests don't get in
	     * each other's way.
	     */
	    flushinp();
	    set_terminal_modes();
	    reset_prog_mode();
	    clear();
	    refresh();
	    endwin();
	    if (command == '?') {
		(void) puts("This is the ncurses capability tester.");
		(void)
		    puts("You may select a test from the main menu by typing the");
		(void)
		    puts("key letter of the choice (the letter to left of the =)");
		(void)
		    puts("at the > prompt.  The commands `x' or `q' will exit.");
	    }
	    continue;
	}
    } while
	(command != 'q');

    ExitProgram(EXIT_SUCCESS);
}

/* ncurses.c ends here */
