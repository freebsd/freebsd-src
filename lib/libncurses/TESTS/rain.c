#include <ncurses.h>
#include <signal.h>
/* rain 11/3/1980 EPS/CITHEP */

#define cursor(col,row) move(row,col)

float ranf();
void onsig();

main(argc,argv)
int argc;
char *argv[];
{
int x, y, j;
static int xpos[5], ypos[5];
float r;
float c;

    for (j=SIGHUP;j<=SIGTERM;j++)
	if (signal(j,SIG_IGN)!=SIG_IGN) signal(j,onsig);

    initscr();
    nl();
    noecho();
    r = (float)(LINES - 4);
    c = (float)(COLS - 4);
    for (j=5;--j>=0;) {
		xpos[j]=(int)(c* ranf())+2;
		ypos[j]=(int)(r* ranf())+2;
    }
    for (j=0;;) {
		x=(int)(c*ranf())+2;
		y=(int)(r*ranf())+2;

		cursor(x,y); addch('.');

		cursor(xpos[j],ypos[j]); addch('o');

		if (j==0) j=4; else --j;
		cursor(xpos[j],ypos[j]); addch('O');

		if (j==0) j=4; else --j;
		cursor(xpos[j],ypos[j]-1);
		addch('-');
		cursor(xpos[j]-1,ypos[j]);
		addstr("|.|");
		cursor(xpos[j],ypos[j]+1);
		addch('-');

		if (j==0) j=4; else --j;
		cursor(xpos[j],ypos[j]-2);
		addch('-');
		cursor(xpos[j]-1,ypos[j]-1);
		addstr("/ \\");
		cursor(xpos[j]-2,ypos[j]);
		addstr("| O |");
		cursor(xpos[j]-1,ypos[j]+1);
		addstr("\\ /");
		cursor(xpos[j],ypos[j]+2);
		addch('-');

		if (j==0) j=4; else --j;
		cursor(xpos[j],ypos[j]-2);
		addch(' ');
		cursor(xpos[j]-1,ypos[j]-1);
		addstr("   ");
		cursor(xpos[j]-2,ypos[j]);
		addstr("     ");
		cursor(xpos[j]-1,ypos[j]+1);
		addstr("   ");
		cursor(xpos[j],ypos[j]+2);
		addch(' ');
		xpos[j]=x; ypos[j]=y;
		refresh();
    }
}

void
onsig(n)
int n;
{
    endwin();
    exit(0);
}

float
ranf()
{
    float rv;
    long r = rand();

    r &= 077777;
    rv =((float)r/32767.);
    return rv;
}

