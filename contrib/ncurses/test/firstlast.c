/*
 * This test was written by Alexander V. Lukyanov to demonstrate difference
 * between ncurses 4.1 and SVR4 curses
 *
 * $Id: firstlast.c,v 1.3 2001/09/15 21:46:34 tom Exp $
 */

#include <test.priv.h>

static void
fill(WINDOW *w, const char *str)
{
    const char *s;
    for (;;) {
	for (s = str; *s; s++) {
	    if (waddch(w, *s) == ERR) {
		wmove(w, 0, 0);
		return;
	    }
	}
    }
}

int
main(
	int argc GCC_UNUSED,
	char *argv[]GCC_UNUSED)
{
    WINDOW *large, *small;
    initscr();
    noecho();

    large = newwin(20, 60, 2, 10);
    small = newwin(10, 30, 7, 25);

    /* test 1 - addch */
    fill(large, "LargeWindow");

    refresh();
    wrefresh(large);
    wrefresh(small);

    mvwaddstr(small, 5, 5, "   Test <place to change> String   ");
    wrefresh(small);
    getch();

    touchwin(large);
    wrefresh(large);

    mvwaddstr(small, 5, 5, "   Test <***************> String   ");
    wrefresh(small);

    /* DIFFERENCE! */
    getch();

    /* test 2: erase */
    erase();
    refresh();
    getch();

    /* test 3: clrtoeol */
    werase(small);
    wrefresh(small);
    touchwin(large);
    wrefresh(large);
    wmove(small, 5, 0);
    waddstr(small, " clrtoeol>");
    wclrtoeol(small);
    wrefresh(small);

    /* DIFFERENCE! */ ;
    getch();

    /* test 4: clrtobot */
    werase(small);
    wrefresh(small);
    touchwin(large);
    wrefresh(large);
    wmove(small, 5, 3);
    waddstr(small, " clrtobot>");
    wclrtobot(small);
    wrefresh(small);

    /* DIFFERENCE! */
    getch();

    endwin();

    ExitProgram(EXIT_SUCCESS);
}
