#include <ncurses.h>

int main()
{
int i, j;

	initscr();

	for (i = 0; i < LINES; i++) {
		j = mvaddch(i, COLS - 1, 'A' + i);
	}
	refresh();
	endwin();
	exit(0);
}

