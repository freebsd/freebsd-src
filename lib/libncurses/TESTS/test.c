
#include <ncurses.h>

main()
{
int x, y;

  initscr();
  cbreak();
  nodelay(stdscr, TRUE);

  for (y = 0; y < 43; y++)
  for (x =0; x < 132; x++) {
  move(y,x);
    printw("X");
    refresh();
    if (!getch()) {
    	beep();
    	sleep(1);
    }
  }

  nocbreak();
  endwin();
}


