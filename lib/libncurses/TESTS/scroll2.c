#include <ncurses.h>
#define ROWS 20
#define scroll_window wscrl

main()
{
  int i;
  WINDOW * w;


  initscr();
  cbreak();
  noecho();
  w = newwin (ROWS, 35, 2, 25);
  scrollok(w, TRUE);
  wsetscrreg(w, 0, ROWS-1);

#ifdef LELE
  mvaddstr (0, 0, "With my function");
#else
  mvaddstr (0, 0, "With the original wscrl");
#endif
  refresh();


  for (i=0; i<ROWS-1; i++)
    {
      mvwprintw (w, i, 0, "Line number %d", i);
    }
  mvwaddstr (w, ROWS-1, 0, "Moving one line at a time");
  wrefresh(w);
  for (i = 0; i < 4; i++) {
    getch();
    scroll_window (w, 1);
    wrefresh(w);
  }
  for (i = 0; i < 4; i++) {
    getch();
    scroll_window (w, -1);
    wrefresh(w);
  }
  getch();
  wclear (w);


  for (i=0; i<ROWS-1; i++)
    {
      mvwprintw (w, i, 0, "Line number %d", i);
    }
  mvwaddstr (w, ROWS-1, 0, "Moving two line at a time");
#ifndef LELE
  mvaddstr (0, 30, "** THIS FAILS ON MY MACHINE WITH A BUS ERROR
**");
#endif


  wrefresh(w);
  for (i = 0; i < 4; i++) {
    getch();
    scroll_window (w, 2);
    wrefresh(w);
  }
  for (i = 0; i < 4; i++) {
    getch();
    scroll_window (w, -2);
    wrefresh(w);
  }
  getch();
  wclear (w);
  for (i=0; i<ROWS-1; i++)
    {
      mvwprintw (w, i, 0, "Line number %d", i);
    }
  mvwaddstr (w, ROWS-1, 0, "Moving three line at a time");
  wrefresh(w);
  for (i = 0; i < 4; i++) {
    getch();
    scroll_window (w, 3);
    wrefresh(w);
  }
  for (i = 0; i < 4; i++) {
    getch();
    scroll_window (w, -3);
    wrefresh(w);
  }
  getch();


  endwin();
}

