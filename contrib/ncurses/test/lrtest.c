/*
 * Test lower-right-hand corner access
 *
 * originally by Eric S. Raymond <esr@thyrsus.com>, written for animation
 * and resizing -TD
 *
 * This can't be part of the ncurses test-program, because ncurses rips off the
 * bottom line to do labels.
 *
 * $Id: lrtest.c,v 0.17 2002/04/06 20:45:22 tom Exp $
 */

#include <test.priv.h>

typedef struct {
    int y, x, mode, dir, inc;
    chtype value;
} MARK;

/*
 * Make a couple of markers go 'round the border to demonstrate that we can
 * really write to all positions properly.
 */
static void
show(MARK * m)
{
    mvaddch(m->y, m->x, m->value);
    if (m->mode == 0) {		/* along the x-direction */
	m->x += m->inc;
	if (m->x >= COLS) {
	    m->x = COLS - 1;
	    m->inc = -m->dir * m->inc;
	    m->y += m->inc;
	    m->mode = 1;
	} else if (m->x < 0) {
	    m->x = 0;
	    m->inc = -m->dir * m->inc;
	    m->y += m->inc;
	    m->mode = 1;
	}
    } else {			/* along the y-direction */
	m->y += m->inc;
	if (m->y >= LINES) {
	    m->y = LINES - 1;
	    m->inc = m->dir * m->inc;
	    m->x += m->inc;
	    m->mode = 0;
	} else if (m->y < 0) {
	    m->y = 0;
	    m->inc = m->dir * m->inc;
	    m->x += m->inc;
	    m->mode = 0;
	}
    }
}

int
main(
	int argc GCC_UNUSED,
	char *argv[]GCC_UNUSED)
{
    static MARK marks[] =
    {
	{0, 0, 0, -1, 1, '+' | A_BOLD},
	{0, 0, 1, 1, 2, 'X'},
	{0, 0, 1, -1, 3, 'Y'},
	{0, 8, 0, -1, 1, '+' | A_BOLD},
	{0, 9, 0, -1, 1, '+' | A_BOLD},
	{1, 0, 1, 1, 1, '*' | A_REVERSE},
	{2, 0, 1, 1, 1, '*' | A_REVERSE}
    };

    initscr();
    noecho();
    cbreak();
    nodelay(stdscr, TRUE);
    curs_set(0);

#ifdef KEY_RESIZE
    keypad(stdscr, TRUE);
  restart:
#endif
    move(LINES / 2 - 1, 4);
    if (!(has_ic()
    /* see PutCharLR() */
	  || auto_right_margin
	  || (enter_am_mode && exit_am_mode))) {
	addstr("Your terminal lacks the capabilities needed to address the\n");
	move(LINES / 2, 4);
	addstr("lower-right-hand corner of the screen.\n");
    } else {
	addstr("This is a test of access to the lower right corner.\n");
	move(LINES / 2, 4);
	addstr("If the top of the box is missing, the test failed.\n");
	move(LINES / 2 + 1, 4);
	addstr("Please report this (with a copy of your terminfo entry).\n");
	move(LINES / 2 + 2, 4);
	addstr("to the ncurses maintainers, at bug-ncurses@gnu.org.\n");
    }

    for (;;) {
	int ch;
	unsigned n;

	box(stdscr, 0, 0);
	for (n = 0; n < SIZEOF(marks); n++) {
	    show(&marks[n]);
	}

	if ((ch = getch()) > 0) {
	    if (ch == 'q')
		break;
	    else if (ch == 's')
		nodelay(stdscr, FALSE);
	    else if (ch == ' ')
		nodelay(stdscr, TRUE);
#ifdef KEY_RESIZE
	    else if (ch == KEY_RESIZE) {
		for (n = 0; n < SIZEOF(marks); n++) {
		    if (marks[n].mode == 0) {	/* moving along x-direction */
			if (marks[n].y)
			    marks[n].y = LINES - 1;
		    } else {
			if (marks[n].x)
			    marks[n].x = COLS - 1;
		    }
		}
		flash();
		erase();
		wrefresh(curscr);
		goto restart;
	    }
#endif
	}
	napms(50);
	refresh();
    }

    curs_set(1);
    endwin();
    ExitProgram(EXIT_SUCCESS);
}

/* lrtest.c ends here */
