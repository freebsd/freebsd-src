/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/lib/libncurses/TESTS/newdemo.c,v 1.2 1995/05/30 05:47:07 rgrimes Exp $
 *
 *  newdemo.c	-	A demo program using PDCurses. The program illustrate
 *  	 		the use of colours for text output.
 */

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <ncurses.h>

#define delay_output(x)

/*
 *  The Australian map
 */
char    *AusMap[16] =
{
    "           A           A ",
    "    N.T. AAAAA       AAAA ",
    "     AAAAAAAAAAA  AAAAAAAA ",
    "   AAAAAAAAAAAAAAAAAAAAAAAAA Qld.",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAA ",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA ",
    " AAAAAAAAAAAAAAAAAAAAAAAAAAAA ",
    "   AAAAAAAAAAAAAAAAAAAAAAAAA N.S.W.",
    "W.A. AAAAAAAAA      AAAAAA Vic.",
    "       AAA   S.A.     AA",
    "                       A  Tas.",
    ""
};

/*
 *  Funny messages
 */
#define NMESSAGES   6

char    *messages[] =
{
    "Hello from the Land Down Under",
    "The Land of crocs. and a big Red Rock",
    "Where the sunflower runs along the highways",
    "the dusty red roads lead one to loneliness",
    "Blue sky in the morning and",
    "freezing nights and twinkling stars",
    ""
};

/*
 *  Main driver
 */
main()
{
WINDOW  *win;
int     w, x, y, i, j, c, len;
time_t  t;
char    buffer[80], *message;
int     width, height;
chtype  save[80];
void    trap();

    initscr();
    start_color();
    cbreak();
    signal(SIGINT, trap);
    width  = 48;
    height = 14;                /* Create a drawing window */
    win = newwin(height, width, (LINES-height)/2, (COLS-width)/2);
    if(win == NULL)
    {   endwin();
        return 1;
    }

    while(1)
    {   init_pair(1,COLOR_WHITE,COLOR_BLUE);
        wattrset(win, COLOR_PAIR(1));
        werase(win);

        init_pair(2,COLOR_RED,COLOR_RED);
        wattrset(win, COLOR_PAIR(2));
        box(win, ACS_VLINE, ACS_HLINE);
        wrefresh(win);
                                /* Do ramdom output of a character */
        wattrset(win, COLOR_PAIR(1));
        c = 'a';
        for(i=0; i < 5000; ++i)
        {   x = rand() % (width-2)  + 1;
            y = rand() % (height-2) + 1;
            mvwaddch(win, y, x, c);
            wrefresh(win);
            nodelay(win,TRUE);
            if (wgetch(win) != ERR)
                break;
            if(i == 2000)
            {   c = 'b';
                init_pair(3,COLOR_CYAN,COLOR_YELLOW);
                wattron(win, COLOR_PAIR(3));
            }
        }

        SubWinTest(win);
                                /* Erase and draw green window */
        init_pair(4,COLOR_YELLOW,COLOR_GREEN);
        wattrset(win, COLOR_PAIR(4) | A_BOLD);
        werase(win);
        wrefresh(win);
                                /* Draw RED bounding box */
        wattrset(win, COLOR_PAIR(2));
        box(win, ' ', ' ');
        wrefresh(win);
                                /* Display Australia map */
	wattrset(win, COLOR_PAIR(4) | A_BOLD);
        i = 0;
        while(*AusMap[i])
	{   mvwaddstr(win, i+1, 8, AusMap[i]);
            wrefresh(win);
            delay_output(100);
            ++i;
        }

        init_pair(5,COLOR_BLUE,COLOR_WHITE);
        wattrset(win, COLOR_PAIR(5) | A_BLINK);
	mvwaddstr(win, height-2, 6, " PDCurses 2.1 for DOS, OS/2 and Unix");
	wrefresh(win);

				/* Draw running messages */
	init_pair(6,COLOR_YELLOW,COLOR_WHITE);
	wattrset(win, COLOR_PAIR(6));
	message = messages[0];
	len = strlen(message);
	j = 0;
	i = 2;
	w = width-2;
        while(j < NMESSAGES)
        {   strncpy(buffer, message, w - i);
            buffer[w-i] = 0;
	    mvwaddstr(win, height/2, i, buffer);
            if(w - i < len)
            {   memset(buffer, ' ', i);
                strcpy(buffer, message + (w - i));
                buffer[strlen(buffer)]   = ' ';
                buffer[i-2] = '\0';
                mvwaddstr(win, height/2, 2, buffer);
	    }
            wrefresh(win);
            nodelay(win,TRUE);
            if (wgetch(win) != ERR)
            {   flushinp();
		break;
            }
            mvwaddch(win, height/2, i, ' ');
            i = ++i % w;
            if(i < 2)
            {   message = messages[++j%NMESSAGES];
                memset(buffer, ' ', w-2);
		buffer[w-2] = 0;
                mvwaddstr(win, height/2, 2, buffer);
                i = 2;
            }
	    delay_output(300);
        }

        j = 0;
                                /*  Draw running As across in RED */
        init_pair(7,COLOR_RED,COLOR_GREEN);
        wattron(win, COLOR_PAIR(7));
	for(i=2; i < width - 4; ++i)
        {   c = mvwinch(win, 4, i);
            save[j++] = c;
            c = c & 0x7f;
	    mvwaddch(win, 4, i, c);
        }
        wrefresh(win);

                                /* Put a message up wait for a key */
        i = height-2;
        wattrset(win, COLOR_PAIR(5));
	mvwaddstr(win, i, 5, " Type a key to continue or 'Q' to quit ");
        wrefresh(win);

        if(WaitForUser() == 1)
	    break;

        j = 0;                  /* Restore the old line */
        for(i=2; i < width - 4; ++i)
            mvwaddch(win, 4, i, save[j++]);
        wrefresh(win);

	BouncingBalls(win);
                                /* Put a message up wait for a key */
        i = height-2;
        wattrset(win, COLOR_PAIR(5));
	mvwaddstr(win, i, 5, " Type a key to continue or 'Q' to quit ");
        wrefresh(win);
        if(WaitForUser() == 1)
            break;
    }
exit:
    endwin();
    return 0;
}

/*
 * Test sub windows
 */
SubWinTest(WINDOW *win)
{
int     w, h, sw, sh, bx, by;
WINDOW  *swin1, *swin2, *swin3;

    w  = win->_maxx;
    h  = win->_maxy;
    bx = win->_begx;
    by = win->_begy;
    sw = w / 3;
    sh = h / 3;
    if((swin1 = subwin(win, sh, sw, by+3, bx+5)) == NULL)
        return  1;
    if((swin2 = subwin(win, sh, sw, by+4, bx+8)) == NULL)
        return  1;
    if((swin3 = subwin(win, sh, sw, by+5, bx+11)) == NULL)
	return  1;

    init_pair(8,COLOR_RED,COLOR_BLUE);
    wattrset(swin1, COLOR_PAIR(8));
    werase(swin1);
    mvwaddstr(swin1, 0, 3, "Sub-window 1");
    wrefresh(swin1);

    init_pair(8,COLOR_CYAN,COLOR_MAGENTA);
    wattrset(swin2, COLOR_PAIR(8));
    werase(swin2);
    mvwaddstr(swin2, 0, 3, "Sub-window 2");
    wrefresh(swin2);

    init_pair(8,COLOR_YELLOW,COLOR_GREEN);
    wattrset(swin3, COLOR_PAIR(8));
    werase(swin3);
    mvwaddstr(swin3, 0, 3, "Sub-window 3");
    wrefresh(swin3);

    delwin(swin1);
    delwin(swin2);
    delwin(swin3);
    WaitForUser();
    return  0;
}

/*
 *  Bouncing balls
 */
BouncingBalls(WINDOW *win)
{
chtype     c1, c2, c3;
int	w, h;
int     x1, y1, xd1, yd1;
int     x2, y2, xd2, yd2;
int     x3, y3, xd3, yd3;

    w    = win->_maxx;
    h    = win->_maxy;
    x1   = 2 + rand() % (w - 4);
    y1   = 2 + rand() % (h - 4);
    x2   = 2 + rand() % (w - 4);
    y2   = 2 + rand() % (h - 4);
    x3   = 2 + rand() % (w - 4);
    y3   = 2 + rand() % (h - 4);
    xd1  = 1; yd1 = 1;
    xd2  = 1; yd2 = 0;
    xd3  = 0; yd3 = 1;
    nodelay(win,TRUE);
    while(wgetch(win) == ERR)
    {   x1 = xd1 > 0 ? ++x1 : --x1;
        if(x1 <= 1 || x1 >= w - 2)
            xd1 = xd1 ? 0 : 1;
        y1 = yd1 > 0 ? ++y1 : --y1;
        if(y1 <= 1 || y1 >= h - 2)
	    yd1 = yd1 ? 0 : 1;

        x2 = xd2 > 0 ? ++x2 : --x2;
        if(x2 <= 1 || x2 >= w - 2)
            xd2 = xd2 ? 0 : 1;
        y2 = yd2 > 0 ? ++y2 : --y2;
        if(y2 <= 1 || y2 >= h - 2)
            yd2 = yd2 ? 0 : 1;

        x3 = xd3 > 0 ? ++x3 : --x3;
        if(x3 <= 1 || x3 >= w - 2)
	    xd3 = xd3 ? 0 : 1;
        y3 = yd3 > 0 ? ++y3 : --y3;
        if(y3 <= 1 || y3 >= h - 2)
            yd3 = yd3 ? 0 : 1;

        c1 = mvwinch(win, y1, x1);
        c2 = mvwinch(win, y2, x2);
        c3 = mvwinch(win, y3, x3);

        init_pair(8,COLOR_RED,COLOR_BLUE);
        wattrset(win, COLOR_PAIR(8));
	mvwaddch(win, y1, x1, 'O');
        init_pair(8,COLOR_BLUE,COLOR_RED);
        wattrset(win, COLOR_PAIR(8));
        mvwaddch(win, y2, x2, '*');
        init_pair(8,COLOR_YELLOW,COLOR_WHITE);
        wattrset(win, COLOR_PAIR(8));
        mvwaddch(win, y3, x3, '@');
        wmove(win, 0, 0);
        wrefresh(win);
	mvwaddch(win, y1, x1, c1);
	mvwaddch(win, y2, x2, c2);
	mvwaddch(win, y3, x3, c3);
	delay_output(150);
    }
    return 0;
}

/*
 *  Wait for user
 */
int WaitForUser()
{
 time_t  t;
 chtype key;

 nodelay(stdscr,TRUE);
 t = time((time_t *)0);
 while(1)
   {
    if ((key = getch()) != ERR)
      {
       if (key  == 'q' || key == 'Q')
          return  1;
       else
          return  0;
      }
    if (time((time_t *)0) - t > 5)
       return  0;
   }
}

/*
 *  Trap interrupt
 */
void trap()
{
    endwin();
    exit(0);
}

/*  End of DEMO.C */
