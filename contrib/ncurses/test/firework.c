/*
 * $Id: firework.c,v 1.11 1997/11/01 19:36:25 tom Exp $
 */
#include <test.priv.h>

#include <term.h>	/* for tparm() */

#include <signal.h>
#include <ctype.h>
#include <time.h>

static int get_colour(chtype *);
static void explode(int row, int col);
static void showit(void);
static RETSIGTYPE onsig(int sig);

static int my_bg = COLOR_BLACK;

int main(
	int argc GCC_UNUSED,
	char *argv[] GCC_UNUSED)
{
int j;
int start,end,row,diff,flag = 0,direction;
unsigned seed;

	for (j=SIGHUP;j<=SIGTERM;j++)
		if (signal(j,SIG_IGN)!=SIG_IGN)
			signal(j,onsig);

	initscr();
	if (has_colors()) {
		start_color();
#ifdef NCURSES_VERSION
		if (use_default_colors() == OK)
			my_bg = -1;
#endif
	}
	curs_set(0);

       seed = time((time_t *)0);
       srand(seed);
       cbreak();
       for (;;) {
            do {
                start = rand() % (COLS -3);
                end = rand() % (COLS - 3);
                start = (start < 2) ? 2 : start;
                end = (end < 2) ? 2 : end;
                direction = (start > end) ? -1 : 1;
                diff = abs(start-end);
            } while (diff<2 || diff>=LINES-2);
            attrset(A_NORMAL);
            for (row=0;row<diff;row++) {
                mvprintw(LINES - row,start + (row * direction),
                    (direction < 0) ? "\\" : "/");
                if (flag++) {
                    showit();
                    erase();
                    flag = 0;
                }
            }
            if (flag++) {
                showit();
                flag = 0;
            }
            seed = time((time_t *)0);
            srand(seed);
            explode(LINES-row,start+(diff*direction));
            erase();
            showit();
       }
}

static RETSIGTYPE
onsig(int n GCC_UNUSED)
{
    curs_set(1);
    endwin();
    exit(EXIT_FAILURE);
}

static
void explode(int row, int col)
{
       chtype bold;
       erase();
       mvprintw(row,col,"-");
       showit();

       init_pair(1,get_colour(&bold),my_bg);
       attrset(COLOR_PAIR(1) | bold);
       mvprintw(row-1,col-1," - ");
       mvprintw(row,col-1,"-+-");
       mvprintw(row+1,col-1," - ");
       showit();

       init_pair(1,get_colour(&bold),my_bg);
       attrset(COLOR_PAIR(1) | bold);
       mvprintw(row-2,col-2," --- ");
       mvprintw(row-1,col-2,"-+++-");
       mvprintw(row,  col-2,"-+#+-");
       mvprintw(row+1,col-2,"-+++-");
       mvprintw(row+2,col-2," --- ");
       showit();

       init_pair(1,get_colour(&bold),my_bg);
       attrset(COLOR_PAIR(1) | bold);
       mvprintw(row-2,col-2," +++ ");
       mvprintw(row-1,col-2,"++#++");
       mvprintw(row,  col-2,"+# #+");
       mvprintw(row+1,col-2,"++#++");
       mvprintw(row+2,col-2," +++ ");
       showit();

       init_pair(1,get_colour(&bold),my_bg);
       attrset(COLOR_PAIR(1) | bold);
       mvprintw(row-2,col-2,"  #  ");
       mvprintw(row-1,col-2,"## ##");
       mvprintw(row,  col-2,"#   #");
       mvprintw(row+1,col-2,"## ##");
       mvprintw(row+2,col-2,"  #  ");
       showit();

       init_pair(1,get_colour(&bold),my_bg);
       attrset(COLOR_PAIR(1) | bold);
       mvprintw(row-2,col-2," # # ");
       mvprintw(row-1,col-2,"#   #");
       mvprintw(row,  col-2,"     ");
       mvprintw(row+1,col-2,"#   #");
       mvprintw(row+2,col-2," # # ");
       showit();
}

static
int get_colour(chtype *bold)
{
	int attr;
	attr = (rand() % 16) + 1;

	*bold = A_NORMAL;
	if (attr > 8) {
		*bold = A_BOLD;
		attr &= 7;
	}
	return(attr);
}

static void
showit(void)
{
	refresh();
	napms(120);
}
