#include <ncurses.h>

main()
  {
  int c,i;
  initscr();
  cbreak();
  noecho();
#if 1
  wtimeout(stdscr,1000);
#endif
  scrollok(stdscr,TRUE);
  for (c='A';c<='Z';c++)
    for (i=0;i<25;i++)
      {
      move(i,i);
      addch(c);
      refresh();
      }
  move (0,0);
  while ((c=wgetch(stdscr))!='A')
    {
    if (c == EOF) printw(">>wait for keypress<<");
    else printw(">>%c<<\n",c);
    refresh();
    }
  endwin();
  }

