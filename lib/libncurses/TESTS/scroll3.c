#include <ncurses.h>

main(int argc, char **argv)
{
    WINDOW *w, *x;

    initscr();
    w = newwin(0, 0, 0, 0);
    if (argc > 2)
		x = newwin(0, 0, 0, 0);
    scrollok(w, TRUE);
    if (argc > 2)
		scrollok(x, TRUE);
    idlok(w, TRUE);
    if (argc > 2)
		idlok(x, TRUE);
    wmove(w, LINES - 1, 0);
    waddstr(w, "test 1 in w");
    wrefresh(w);
    sleep(1);
    if (argc == 2 || argc == 4)
    {
		waddch(w, '\n');
		sleep(1);
		waddch(w, '\n');
		sleep(1);
		waddch(w, '\n');
		sleep(1);
		beep();
		wrefresh(w);
    }
    sleep(1);
    if (argc > 2)
    {
		wmove(x, LINES - 1, 0);
		waddstr(x, "test 2 in x");
		sleep(1);
		waddch(x, '\n');
		sleep(1);
		waddch(x, '\n');
		sleep(1);
		waddch(x, '\n');
		sleep(1);
		beep();
		wrefresh(w);
		sleep(1);
    }
    endwin();
    return 0;
}

