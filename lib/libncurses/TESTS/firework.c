#include <stdio.h>
#include <signal.h>
#include <ncurses.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>

void explode();

int main()
{
int start,end,row,diff,flag,direction,seed;

       initscr();
       if (has_colors())
          start_color();
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
                    refresh();
                    erase();
                    flag = 0;
                }
            }
            if (flag++) {
                refresh();
                flag = 0;
            }
            seed = time((time_t *)0);
            srand(seed);
            explode(LINES-row,start+(diff*direction));
            erase();
            refresh();
       }
       endwin();
       exit(0);
}

void explode(int row, int col)
{
       erase();
       mvprintw(row,col,"-");
       refresh();

       init_pair(1,get_colour(),COLOR_BLACK);
       attrset(COLOR_PAIR(1));
       mvprintw(row-1,col-1," - ");
       mvprintw(row,col-1,"-+-");
       mvprintw(row+1,col-1," - ");
       refresh();

       init_pair(1,get_colour(),COLOR_BLACK);
       attrset(COLOR_PAIR(1));
       mvprintw(row-2,col-2," --- ");
       mvprintw(row-1,col-2,"-+++-");
       mvprintw(row,  col-2,"-+#+-");
       mvprintw(row+1,col-2,"-+++-");
       mvprintw(row+2,col-2," --- ");
       refresh();

       init_pair(1,get_colour(),COLOR_BLACK);
       attrset(COLOR_PAIR(1));
       mvprintw(row-2,col-2," +++ ");
       mvprintw(row-1,col-2,"++#++");
       mvprintw(row,  col-2,"+# #+");
       mvprintw(row+1,col-2,"++#++");
       mvprintw(row+2,col-2," +++ ");
       refresh();

       init_pair(1,get_colour(),COLOR_BLACK);
       attrset(COLOR_PAIR(1));
       mvprintw(row-2,col-2,"  #  ");
       mvprintw(row-1,col-2,"## ##");
       mvprintw(row,  col-2,"#   #");
       mvprintw(row+1,col-2,"## ##");
       mvprintw(row+2,col-2,"  #  ");
       refresh();

       init_pair(1,get_colour(),COLOR_BLACK);
       attrset(COLOR_PAIR(1));
       mvprintw(row-2,col-2," # # ");
       mvprintw(row-1,col-2,"#   #");
       mvprintw(row,  col-2,"     ");
       mvprintw(row+1,col-2,"#   #");
       mvprintw(row+2,col-2," # # ");
       refresh();
       return;
}

int get_colour()
{
 int attr;
       attr = (rand() % 16)+1;
       if (attr == 1 || attr == 9)
          attr = COLOR_RED;
       if (attr > 8)
          attr |= A_BOLD;
       return(attr);
}
