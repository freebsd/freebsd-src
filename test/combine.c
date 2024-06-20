/****************************************************************************
 * Copyright 2021,2022 Thomas E. Dickey                                     *
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
 * $Id: combine.c,v 1.23 2022/12/10 22:28:50 tom Exp $
 */

#include <test.priv.h>

#if USE_WIDEC_SUPPORT

#include <wctype.h>
#include <dump_window.h>
#include <popup_msg.h>

static int c_opt;
static int r_opt;

static int
next_char(int value)
{
    do {
	++value;
    } while (!iswprint((wint_t) value));
    return value;
}

static int
prev_char(int value)
{
    do {
	--value;
    } while (!iswprint((wint_t) value));
    return value;
}

static void
do_row(int row, int base_ch, int over_ch)
{
    int col = 0;
    bool done = FALSE;
    bool reverse = (r_opt && !(row % 2));

    move(row, col);
    printw("[U+%04X]", over_ch);
    do {
	if (c_opt) {
	    wchar_t source[2];
	    cchar_t target;
	    attr_t attr = reverse ? A_REVERSE : A_NORMAL;

	    source[1] = 0;

	    source[0] = (wchar_t) base_ch;
	    setcchar(&target, source, attr, 0, NULL);
	    add_wch(&target);

	    source[0] = (wchar_t) over_ch;
	    setcchar(&target, source, attr, 0, NULL);
	    add_wch(&target);
	} else {
	    wchar_t data[3];

	    data[0] = (wchar_t) base_ch;
	    data[1] = (wchar_t) over_ch;
	    data[2] = 0;
	    if (reverse)
		attr_on(A_REVERSE, NULL);
	    addwstr(data);
	    if (reverse)
		attr_off(A_REVERSE, NULL);
	}
	col = getcurx(stdscr);
	base_ch = next_char(base_ch);
	done = (col + 1 >= COLS);
    } while (!done);
}

#define LAST_OVER 0x6f

static int
next_over(int value)
{
    if (++value > LAST_OVER)
	value = 0;
    return value;
}

static int
prev_over(int value)
{
    if (--value < 0)
	value = LAST_OVER;
    return value;
}

static void
do_all(int left_at, int over_it)
{
    int row;

    for (row = 0; row < LINES; ++row) {
	do_row(row, left_at, 0x300 + over_it);
	over_it = next_over(over_it);
    }
}

static void
show_help(WINDOW *current)
{
    /* *INDENT-OFF* */
    static struct {
	int	key;
	CONST_FMT char * msg;
    } help[] = {
	{ HELP_KEY_1,	"Show this screen" },
	{ CTRL('L'),	"Repaint screen" },
	{ '$',		"Scroll to end of combining-character range" },
	{ '+',		"Scroll to next combining-character in range" },
	{ KEY_DOWN,	"(same as \"+\")" },
	{ '-',		"Scroll to previous combining-character in range" },
	{ KEY_UP,	"(same as \"-\")" },
	{ '0',		"Scroll to beginning of combining-character range" },
	{ 'c',		"Toggle command-line option \"-c\"" },
	{ 'd',		"Dump screen using scr_dump unless \"-l\" option used" },
	{ 'h',		"Scroll test-data left one column" },
	{ 'j',		"Scroll test-data down one row" },
	{ 'k',		"Scroll test-data up one row" },
	{ 'l',		"Scroll test-data right one column" },
	{ 'q',		"Quit" },
	{ ESCAPE,	"(same as \"q\")" },
	{ QUIT,		"(same as \"q\")" },
	{ 'r',		"Toggle command-line option \"-r\"" },
    };
    /* *INDENT-ON* */

    char **msgs = typeCalloc(char *, SIZEOF(help) + 3);
    size_t s;
    int d = 0;

    msgs[d++] = strdup("Test diacritic combining-characters range "
		       "U+0300..U+036F");
    msgs[d++] = strdup("");
    for (s = 0; s < SIZEOF(help); ++s) {
	char *name = strdup(keyname(help[s].key));
	size_t need = (11 + strlen(name) + strlen(help[s].msg));
	msgs[d] = typeMalloc(char, need);
	_nc_SPRINTF(msgs[d], _nc_SLIMIT(need) "%-10s%s", name, help[s].msg);
	free(name);
	++d;
    }
    popup_msg2(current, msgs);
    for (s = 0; msgs[s] != 0; ++s) {
	free(msgs[s]);
    }
    free(msgs);
}

static void
usage(int ok)
{
    static const char *msg[] =
    {
	"Usage: combine [options]"
	,""
	,USAGE_COMMON
	,"Demonstrate combining-characters."
	,""
	,"Options:"
	," -c       use cchar_t data rather than wchar_t string"
	," -l FILE  log window-dumps to this file"
	," -r       draw even-numbered rows in reverse-video"
    };
    unsigned n;
    for (n = 0; n < SIZEOF(msg); ++n) {
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
    int ch;
    int left_at = ' ';
    int over_it = 0;
    bool done = FALSE;
    bool log_option = FALSE;
    const char *dump_log = "combine.log";

    while ((ch = getopt(argc, argv, OPTS_COMMON "cl:r")) != -1) {
	switch (ch) {
	case 'c':
	    c_opt = TRUE;
	    break;
	case 'l':
	    log_option = TRUE;
	    if (!open_dump(optarg))
		usage(FALSE);
	    break;
	case 'r':
	    r_opt = TRUE;
	    break;
	case OPTS_VERSION:
	    show_version(argv);
	    ExitProgram(EXIT_SUCCESS);
	default:
	    usage(ch == OPTS_USAGE);
	    /* NOTREACHED */
	}
    }

    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    do {
	do_all(left_at, over_it);
	switch (getch()) {
	case HELP_KEY_1:
	    show_help(stdscr);
	    break;
	case 'q':
	case QUIT:
	case ESCAPE:
	    done = TRUE;
	    break;
	case CTRL('L'):
	    redrawwin(stdscr);
	    break;
	case 'd':
	    if (log_option)
		dump_window(stdscr);
#if HAVE_SCR_DUMP
	    else
		scr_dump(dump_log);
#endif
	    break;
	case 'h':
	    if (left_at > ' ')
		left_at = prev_char(left_at);
	    break;
	case 'l':
	    left_at = next_char(left_at);
	    break;
	case 'c':
	    c_opt = !c_opt;
	    break;
	case 'r':
	    r_opt = !r_opt;
	    break;
	case KEY_HOME:
	case '0':
	    over_it = 0;
	    break;
	case KEY_END:
	case '$':
	    over_it = LAST_OVER;
	    break;
	case KEY_UP:
	case 'k':
	case '-':
	    over_it = prev_over(over_it);
	    break;
	case KEY_DOWN:
	case 'j':
	case '+':
	    over_it = next_over(over_it);
	    break;
	}
    } while (!done);

    endwin();

    ExitProgram(EXIT_SUCCESS);
}
#else
int
main(void)
{
    printf("This program requires wide-curses functions\n");
    ExitProgram(EXIT_FAILURE);
}
#endif
