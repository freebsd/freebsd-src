#include <ncurses.h>

main()
{
    int i;
    WINDOW *win;

    initscr();
    noecho();
    nonl();
    cbreak();

    scrollok(stdscr, TRUE);

    for (i=0; i<25; i++)
      printw("This is in the background, this should be left alone!\n");

    refresh();
    win=newwin(10,50,6,20);
    scrollok(win,TRUE);
    wmove(win,0,0);
    whline(win,0,49);
    wmove(win,9,0);
    whline(win,0,49);
    wrefresh(win);
    wattrset(win,A_STANDOUT);
    mvwprintw(win, 0, 14, " Scrolling Demo! ");
    mvwprintw(win, 9, 14, " Scrolling Demo! ");
    wattroff(win,A_STANDOUT);
    wsetscrreg(win, 1,8);

    mvwprintw(win, 0, 5, " DOWN ");
    wmove(win,1,0);
    wrefresh(win);

	getch();
    for (i=0; i<25; i++){ wprintw(win, "This is window line test (%d).\n", i);
     if (i%2) wattrset(win,A_BOLD); else wattroff(win,A_BOLD);
     wrefresh(win); }

    mvwprintw(win, 0, 5, "  UP  ");
    wrefresh(win);
	getch();
    for (i=0; i<25; i++) { wmove(win,1,0); winsertln(win);
     if (i%2) wattrset(win,A_BOLD); else wattroff(win,A_BOLD);
     wprintw(win, "Scrolling backwards! (%d)\n", i); wrefresh(win); }

    mvwprintw(win, 0, 5, " DONE "); wrefresh(win);

    endwin();
}

