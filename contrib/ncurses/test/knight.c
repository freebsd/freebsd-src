/*
 * Knight's Tour - a brain game
 *
 * The original of this game was anonymous.  It had an unbelievably bogus
 * interface, you actually had to enter square coordinates!  Redesign by
 * Eric S. Raymond <esr@snark.thyrsus.com> July 22 1995.  Mouse support
 * added September 20th 1995.
 *
 * $Id: knight.c,v 1.14 1997/08/20 16:22:38 hjl Exp $
 */

#include <test.priv.h>

#include <ctype.h>
#include <signal.h>
#include <string.h>

/* board size */
#define BDEPTH	8
#define BWIDTH	8

/* where to start the instructions */
#define INSTRY	2
#define INSTRX	35

/* corner of board */
#define BOARDY	2
#define BOARDX	0

/* notification line */
#define NOTIFYY	21

/* virtual color values */
#define TRAIL_COLOR	1
#define PLUS_COLOR	2
#define MINUS_COLOR	3

#define CX(x)	(2 + 4 * (x))
#define CY(y)	(1 + 2 * (y))
#define cellmove(y, x)	wmove(boardwin, CY(y), CX(x))
#define CXINV(x)	(((x) - 1) / 4)
#define CYINV(y)	(((y) - 2) / 2)

typedef struct
{
    short	x, y;
}
cell;

static short	board[BDEPTH][BWIDTH];	/* the squares */
static int	rw,col;			/* current row and column */
static int	lastrow,lastcol;   	/* last location visited */
static cell	history[BDEPTH*BWIDTH];	/* choice history */
static int	movecount;		/* count of moves so far */
static WINDOW	*boardwin;		/* the board window */
static WINDOW	*helpwin;		/* the help window */
static WINDOW	*msgwin;		/* the message window */
static chtype	trail = '#';		/* trail character */
static chtype	plus = '+';		/* cursor hot-spot character */
static chtype	minus = '-';		/* possible-move character */
static chtype	oldch;

static void init(void);
static void play(void);
static void dosquares(void);
static void drawmove(char, int, int, int, int);
static bool evalmove(int, int);
static bool chkmoves(void);
static bool chksqr(int, int);
static int  iabs(int);

int main(
	int argc GCC_UNUSED,
	char *argv[] GCC_UNUSED)
{
    init();

    play();

    endwin();
    return EXIT_SUCCESS;
}

static void init (void)
{
    srand ((unsigned)getpid());
    initscr ();
    cbreak ();			/* immediate char return */
    noecho ();			/* no immediate echo */
    boardwin = newwin(BDEPTH * 2 + 1, BWIDTH * 4 + 1, BOARDY, BOARDX);
    helpwin = newwin(0, 0, INSTRY, INSTRX);
    msgwin = newwin(1, INSTRX-1, NOTIFYY, 0);
    scrollok(msgwin, TRUE);
    keypad(boardwin, TRUE);

    if (has_colors())
    {
	int bg = COLOR_BLACK;

	start_color();
#ifdef NCURSES_VERSION
	if (use_default_colors() == OK)
	    bg = -1;
#endif

	(void) init_pair(TRAIL_COLOR, COLOR_CYAN,  bg);
	(void) init_pair(PLUS_COLOR,  COLOR_RED,   bg);
	(void) init_pair(MINUS_COLOR, COLOR_GREEN, bg);

	trail |= COLOR_PAIR(TRAIL_COLOR);
	plus  |= COLOR_PAIR(PLUS_COLOR);
	minus |= COLOR_PAIR(MINUS_COLOR);
    }

#ifdef NCURSES_MOUSE_VERSION
    (void) mousemask(BUTTON1_CLICKED, (mmask_t *)NULL);
#endif /* NCURSES_MOUSE_VERSION*/

    oldch = minus;
}

static void help1(void)
/* game explanation -- initial help screen */
{
    (void)waddstr(helpwin, "Knight's move is a solitaire puzzle.  Your\n");
    (void)waddstr(helpwin, "objective is to visit each square of the  \n");
    (void)waddstr(helpwin, "chessboard exactly once by making knight's\n");
    (void)waddstr(helpwin, "moves (one square right or left followed  \n");
    (void)waddstr(helpwin, "by two squares up or down, or two squares \n");
    (void)waddstr(helpwin, "right or left followed by one square up or\n");
    (void)waddstr(helpwin, "down).  You may start anywhere.\n\n");

    (void)waddstr(helpwin, "Use arrow keys to move the cursor around.\n");
    (void)waddstr(helpwin, "When you want to move your knight to the \n");
    (void)waddstr(helpwin, "cursor location, press <space> or Enter.\n");
    (void)waddstr(helpwin, "Illegal moves will be rejected with an  \n");
    (void)waddstr(helpwin, "audible beep.\n\n");
    (void)waddstr(helpwin, "The program will detect if you solve the\n");
    (void)waddstr(helpwin, "puzzle; also inform you when you run out\n");
    (void)waddstr(helpwin, "of legal moves.\n\n");

    (void)mvwaddstr(helpwin, NOTIFYY-INSTRY, 0,
		    "Press `?' to go to keystroke help."); 
}

static void help2(void)
/* keystroke help screen */
{
    (void)waddstr(helpwin, "Possible moves are shown with `-'.\n\n");

    (void)waddstr(helpwin, "You can move around with the arrow keys or\n");
    (void)waddstr(helpwin, "with the rogue/hack movement keys.  Other\n");
    (void)waddstr(helpwin, "commands allow you to undo moves or redraw.\n");
    (void)waddstr(helpwin, "Your mouse may work; try left-button to\n");
    (void)waddstr(helpwin, "move to the square under the pointer.\n\n");

    (void)waddstr(helpwin, "x,q -- exit             y k u    7 8 9\n");
    (void)waddstr(helpwin, "r -- redraw screen       \\|/      \\|/ \n");
    (void)waddstr(helpwin, "u -- undo move          h-+-l    4-+-6\n");
    (void)waddstr(helpwin, "                         /|\\      /|\\ \n");
    (void)waddstr(helpwin, "                        b j n    1 2 3\n");

    (void)waddstr(helpwin,"\nYou can place your knight on the selected\n");
    (void)waddstr(helpwin, "square with spacebar, Enter, or the keypad\n");
    (void)waddstr(helpwin, "center key.  You can quit with `x' or `q'.\n");

    (void)mvwaddstr(helpwin, NOTIFYY-INSTRY, 0,
		    "Press `?' to go to game explanation"); 
}

static void play (void)
/* play the game */
{
    bool		keyhelp; /* TRUE if keystroke help is up */
    int	c, ny = 0, nx = 0;
    int i, j, count;

    do {
	   /* clear screen and draw board */
	   werase(boardwin);
	   werase(helpwin);
	   werase(msgwin);
	   dosquares();
	   help1();
	   wnoutrefresh(stdscr);
	   wnoutrefresh(helpwin);
	   wnoutrefresh(msgwin);
	   wnoutrefresh(boardwin);
	   doupdate();

	   for (i = 0; i < BDEPTH; i++)
	       for (j = 0; j < BWIDTH; j++)
	       {
		   board[i][j] = FALSE;
		   cellmove(i, j);
		   waddch(boardwin, minus);
	       }
	   memset(history, '\0', sizeof(history));
	   history[0].y = history[0].x = -1;
	   lastrow = lastcol = -2;
	   movecount = 1;
	   keyhelp = FALSE;

	   for (;;)
	   {
	       if (rw != lastrow || col != lastcol)
	       {
		   if (lastrow >= 0 && lastcol >= 0)
		   {
		       cellmove(lastrow, lastcol);
		       if (board[lastrow][lastcol])
			   waddch(boardwin, trail);
		       else
			   waddch(boardwin, oldch);
		   }

		   cellmove(rw, col);
		   oldch = winch(boardwin);

		   lastrow = rw;
		   lastcol= col;
	       }
	       cellmove(rw, col);
	       waddch(boardwin, plus);
	       cellmove(rw, col);

	       wrefresh(msgwin);

	       c = wgetch(boardwin);

	       werase(msgwin);

	       switch (c)
	       {
	       case 'k': case '8':
	       case KEY_UP:
		   ny = rw+BDEPTH-1; nx = col;
		   break;
	       case 'j': case '2':
	       case KEY_DOWN:
		   ny = rw+1;        nx = col;
		   break;
	       case 'h': case '4':
	       case KEY_LEFT:
		   ny = rw;          nx = col+BWIDTH-1;
		   break;
	       case 'l': case '6':
	       case KEY_RIGHT:
		   ny = rw;          nx = col+1;
		   break;
	       case 'y': case '7':
	       case KEY_A1:
		   ny = rw+BDEPTH-1; nx = col+BWIDTH-1;
		   break;
	       case 'b': case '1':
	       case KEY_C1:
		   ny = rw+1;        nx = col+BWIDTH-1;
		   break;
	       case 'u': case '9':
	       case KEY_A3:
		   ny = rw+BDEPTH-1; nx = col+1;
		   break;
	       case 'n': case '3':
	       case KEY_C3:
		   ny = rw+1;        nx = col+1;
		   break;

#ifdef NCURSES_MOUSE_VERSION
	       case KEY_MOUSE:
		   {
		       MEVENT	myevent;

		       getmouse(&myevent);
		       if (myevent.y >= CY(0) && myevent.y <= CY(BDEPTH)
			   && myevent.x >= CX(0) && myevent.x <= CX(BWIDTH))
		       {
			   nx = CXINV(myevent.x);
			   ny = CYINV(myevent.y);
			   ungetch('\n');
			   break;
		       }
		       else
		       {
			   beep();
			   continue;
		       }
		   }
#endif /* NCURSES_MOUSE_VERSION */

	       case KEY_B2:
	       case '\n':
	       case ' ':
		   if (evalmove(rw, col))
		   {
		       drawmove(trail,
				history[movecount-1].y, history[movecount-1].x,
				rw, col);
		       history[movecount].y = rw; 
		       history[movecount].x = col; 
		       movecount++;

		       if (!chkmoves()) 
			   goto dropout;
		   }
		   else
		       beep();
		   break;

	       case KEY_REDO:
	       case '\f':
	       case 'r':
		   clearok(curscr, TRUE);
		   wnoutrefresh(stdscr);
		   wnoutrefresh(boardwin);
		   wnoutrefresh(msgwin);
		   wnoutrefresh(helpwin);
		   doupdate();
		   break;

	       case KEY_UNDO:
	       case KEY_BACKSPACE:
	       case '\b':
		   if (movecount == 1)
		   {
		       ny = lastrow;
		       nx = lastcol;
		       waddstr(msgwin, "\nNo previous move.");
		       beep();
		   }
		   else
		   {
		       int oldy = history[movecount-1].y;
		       int oldx = history[movecount-1].x;

		       board[oldy][oldx] = FALSE;
		       --movecount;
		       ny = history[movecount-1].y;
		       nx = history[movecount-1].x;
		       drawmove(' ', oldy, oldx, ny, nx);

		       /* avoid problems if we just changed the current cell */
		       cellmove(lastrow, lastcol);
		       oldch = winch(boardwin);
		   }
		   break;

	       case 'q':
	       case 'x':
		   goto dropout;

	       case '?':
		   werase(helpwin);
		   if (keyhelp)
		   {
		       help1();
		       keyhelp = FALSE;
		   }
		   else
		   {
		       help2();
		       keyhelp = TRUE;
		   }
		   wrefresh(helpwin);
		   break;

	       default:
		   beep();
		   break;
	       }

	       col = nx % BWIDTH;
	       rw = ny % BDEPTH;
	   }

       dropout:
	   count = 0;
	   for (i = 0; i < BDEPTH; i++)
	       for (j = 0; j < BWIDTH; j++)
		   if (board[i][j] != 0)
		       count += 1;
	   if (count == (BWIDTH * BDEPTH))
	       wprintw(msgwin, "\nYou won.  Care to try again? ");
	   else
	       wprintw(msgwin, "\n%d squares filled.  Try again? ", count);
       } while
	   (tolower(wgetch(msgwin)) == 'y');
}

static void dosquares (void)
{
    int i, j;

    mvaddstr(0, 20, "KNIGHT'S MOVE -- a logical solitaire");

    move(BOARDY,BOARDX);
    waddch(boardwin, ACS_ULCORNER);
    for (j = 0; j < 7; j++)
    {
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_TTEE);
    }
    waddch(boardwin, ACS_HLINE);
    waddch(boardwin, ACS_HLINE);
    waddch(boardwin, ACS_HLINE);
    waddch(boardwin, ACS_URCORNER);

    for (i = 1; i < BDEPTH; i++)
    {
	move(BOARDY + i * 2 - 1, BOARDX);
	waddch(boardwin, ACS_VLINE); 
	for (j = 0; j < BWIDTH; j++)
	{
	    waddch(boardwin, ' ');
	    waddch(boardwin, ' ');
	    waddch(boardwin, ' ');
	    waddch(boardwin, ACS_VLINE);
	}
	move(BOARDY + i * 2, BOARDX);
	waddch(boardwin, ACS_LTEE); 
	for (j = 0; j < BWIDTH - 1; j++)
	{
	    waddch(boardwin, ACS_HLINE);
	    waddch(boardwin, ACS_HLINE);
	    waddch(boardwin, ACS_HLINE);
	    waddch(boardwin, ACS_PLUS);
	}
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_RTEE);
    }

    move(BOARDY + i * 2 - 1, BOARDX);
    waddch(boardwin, ACS_VLINE);
    for (j = 0; j < BWIDTH; j++)
    {
	waddch(boardwin, ' ');
	waddch(boardwin, ' ');
	waddch(boardwin, ' ');
	waddch(boardwin, ACS_VLINE);
    }

    move(BOARDY + i * 2, BOARDX);
    waddch(boardwin, ACS_LLCORNER);
    for (j = 0; j < BWIDTH - 1; j++)
    {
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_HLINE);
	waddch(boardwin, ACS_BTEE);
    }
    waddch(boardwin, ACS_HLINE);
    waddch(boardwin, ACS_HLINE);
    waddch(boardwin, ACS_HLINE);
    waddch(boardwin, ACS_LRCORNER);
}

static void mark_possibles(int prow, int pcol, chtype mark)
{
    if (chksqr(prow+2,pcol+1)){cellmove(prow+2,pcol+1);waddch(boardwin,mark);};
    if (chksqr(prow+2,pcol-1)){cellmove(prow+2,pcol-1);waddch(boardwin,mark);};
    if (chksqr(prow-2,pcol+1)){cellmove(prow-2,pcol+1);waddch(boardwin,mark);};
    if (chksqr(prow-2,pcol-1)){cellmove(prow-2,pcol-1);waddch(boardwin,mark);};
    if (chksqr(prow+1,pcol+2)){cellmove(prow+1,pcol+2);waddch(boardwin,mark);};
    if (chksqr(prow+1,pcol-2)){cellmove(prow+1,pcol-2);waddch(boardwin,mark);};
    if (chksqr(prow-1,pcol+2)){cellmove(prow-1,pcol+2);waddch(boardwin,mark);};
    if (chksqr(prow-1,pcol-2)){cellmove(prow-1,pcol-2);waddch(boardwin,mark);};
}

static void drawmove(char tchar, int oldy, int oldx, int row, int column)
/* place the stars, update board & currents */
{
    if (movecount <= 1)
    {
	int i, j;

	for (i = 0; i < BDEPTH; i++)
	    for (j = 0; j < BWIDTH; j++)
	    {
		cellmove(i, j);
		if (winch(boardwin) == minus)
		    waddch(boardwin, movecount ? ' ' : minus);
	    }
    }
    else
    {
	cellmove(oldy, oldx);
	waddch(boardwin, '\b');
	waddch(boardwin, tchar);
	waddch(boardwin, tchar);
	waddch(boardwin, tchar);
	mark_possibles(oldy, oldx, ' ');
    }

    if (row != -1 && column != -1)
    {
	cellmove(row, column);
	waddch(boardwin, '\b');
	waddch(boardwin, trail);
	waddch(boardwin, trail);
	waddch(boardwin, trail);
	mark_possibles(row, column, minus);
	board[row][column] = TRUE;
    }

    wprintw(msgwin, "\nMove %d", movecount);
}

static bool evalmove(int row, int column)
/* evaluate move */
{
    if (movecount == 1)
	return(TRUE);
    else if (board[row][column] == TRUE)
    {
	waddstr(msgwin, "\nYou've already been there.");
	return(FALSE);
    }
    else
    {
	int	rdif = iabs(row  - history[movecount-1].y);
	int	cdif = iabs(column - history[movecount-1].x);

	if (!((rdif == 1) && (cdif == 2)) && !((rdif == 2) && (cdif == 1)))
	{
	    waddstr(msgwin, "\nThat's not a legal knight's move.");
	    return(FALSE);
	}
    }

    return(TRUE);
}

static bool chkmoves (void)
/* check to see if valid moves are available */
{
    if (chksqr(rw+2,col+1)) return(TRUE);
    if (chksqr(rw+2,col-1)) return(TRUE);
    if (chksqr(rw-2,col+1)) return(TRUE);
    if (chksqr(rw-2,col-1)) return(TRUE);
    if (chksqr(rw+1,col+2)) return(TRUE);
    if (chksqr(rw+1,col-2)) return(TRUE);
    if (chksqr(rw-1,col+2)) return(TRUE);
    if (chksqr(rw-1,col-2)) return(TRUE);
    return (FALSE);
}

static int iabs(int num)
{
	if (num < 0) return (-num);
		else return (num);
}

static bool chksqr (int r1, int c1)
{
    if ((r1 < 0) || (r1 > BDEPTH - 1))
	return(FALSE);
    if ((c1 < 0) || (c1 > BWIDTH - 1))
	return(FALSE);
    return ((!board[r1][c1]) ? TRUE : FALSE);
}

/* knight.c ends here */
