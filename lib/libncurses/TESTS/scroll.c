#include <ncurses.h>

main()
{
int i;

	initscr();
	noecho();
	scrollok(stdscr, TRUE);
	setscrreg(0, 9);
	mvaddstr(0,0,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	mvaddstr(1,0,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
	mvaddstr(2,0,"ccccccccccccccccccccccccccccc");
	mvaddstr(3,0,"ddddddddddddddddddddddddddddd");
	mvaddstr(4,0,"eeeeeeeeeeeeeeeeeeeeeeeeeeeee");
	mvaddstr(5,0,"fffffffffffffffffffffffffffff");
	mvaddstr(6,0,"ggggggggggggggggggggggggggggg");
	mvaddstr(7,0,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
	mvaddstr(8,0,"iiiiiiiiiiiiiiiiiiiiiiiiiiiii");
	mvaddstr(9,0,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
	refresh();
	for (i = 0; i < 4; i++) {
		getch();
		scrl(-1);
		refresh();
	}
	getch();
	endwin();

}
