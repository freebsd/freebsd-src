#include   <_G_config.h>
#if !_G_HAVE_CURSES

#include <iostream.h>
int main ()
{
  cerr << "(CursesWindow is not supported on this system)";
}
#else /* _G_HAVE_CURSES */

#include <CursesW.h>

// a simple test/demo for CursesWindow

int main()
{
    CursesWindow big(23, 79, 0, 0);
    CursesWindow corner(10, 10, 0, 0);
    CursesWindow small(10, 10, 5, 5);
    CursesWindow sub(big, 10, 10, (big.height()>>1)-5, (big.width()>>1)-5);
    CursesWindow sub2(big, 5, 5, big.height()-6, big.width()-6);
    CursesWindow subsub(sub, 5, 5, 1, 1, 'r');

    int i;
    char c='A';

    big.box('B','B');

    sub.box('|','-');
    for (i=1;i<10;++i)
      sub.mvaddch(i, i, '*');
    for (i=1;i<10;++i)
      sub.mvaddch(10-i, i, '*');

    big.refresh();

    big.mvprintw(0,0,"begx=%d,maxx=%d,begy=%d,maxy=%d,height=%d,width=%d",
                 big.begx(), big.maxx(), big.begy(), big.maxy(),
                 big.height(), big.width());
    big.refresh();

    sub2.box('2', '2');
    subsub.box('s', 's');

    big.refresh();

    i=13;
    const char * cptr = "Cstar";

    long l = 0xffffffff;
    double d= 3.1415926;
    float f= 10.0/d;

    big.mvprintw(2,2,"printw test:%d, %c, %s, %ld, %lf, %f\n",i,c,cptr,l,d,f);
    big.refresh();

    corner.box('c','c');
    big.mvprintw(5,20,"enter an int:");
    big.refresh();
    big.scanw("%d",&i);
    big.move(6,20);
    big.printw("number = %d\n",i);
    big.refresh();
    corner.refresh();

    small.box('S','S'); small.refresh();
    big.mvprintw(20,20,"enter a char:");
    big.refresh();
    big.scanw("%c",&c);
    big.move(21,20);
    big.printw("char = %c\n",c);
    small.box(c, c);
    big.refresh();
    small.refresh();
    corner.overlay(small);
    big.overwrite(corner);
    corner.refresh();
}
#endif /* _G_HAVE_CURSES */

