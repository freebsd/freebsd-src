/* Knights Tour - a brain game */

#include <ncurses.h>
#include <signal.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef __FreeBSD__
#define srand48 srandom
#define lrand48 random
#endif

short   board [64];     /* the squares */
char    row, column;    /* input characters */
int     rw,col;         /* numeric equivalent of row and column */
int     curow,curcol;   /* current row and column integers */
int     rdif, cdif;     /* difference between input and current */
int     j;              /* index into board */

char	script[]={"'_)//,/(-)/__/__(_<_(__),|/|/_///_/_<//_/_)__o__o'______///_(--_(_)___,(_/,_/__(_\0"};

int	ypos[] ={1,0,1,2,3,0,1,2,2,3,3,2,2,3,2,2,3,3,3,3,3,2,3,3,2,4,5,5,
	4,3,3,2,1,2,3,3,3,2,1,3,3,2,3,3,3,2,1,1,0,1,4,4,4,4,4,4,5,6,7,7,
	7,6,6,6,7,7,7,6,6,6,6,7,7,7,6,7,7,6,6,7,7};

int	xpos[]={0,1,2,1,0,5,4,3,2,4,6,7,8,8,9,10,10,11,12,13,14,15,15,16,
	16,16,15,14,15,17,18,19,20,20,20,21,22,23,24,23,25,26,27,26,28,
	13,23,25,27,27,2,3,4,5,6,7,4,3,2,1,0,1,2,5,4,5,6,6,7,8,9,8,9,10,
	11,11,12,13,14,14,15};

static char *instructions[] = 
{
"     Knight's Tour is a board game for one player.   It is played on",
"an eight by eight board and is based on the allowable moves that a knight",
"can make in the game of chess.  For those who are unfamiliar with the",
"game, a knight may move either on a row or a column but not diagonally.",
"He may move one square in any direction and two squares in a perpendicular",
"direction >or< two squares in any direction and one square in a",
"perpendicular direction.  He may not, of course, move off the board.",
"",
"     At the beginning of a game you will be asked to either choose a",
"starting square or allow the computer to select a random location.",
"Squares are designated by a letter-number combination where the row is",
"specified by a letter A-H and the numbers 1-8 define a column.  Invalid",
"entries are ignored and illegal moves produce a beep at the terminal.",
"",
"     The objective is to visit every square on the board.  When you claim",
"a square a marker is placed on it to show where you've been.  You may",
"not revisit a square that you've landed on before.",
"",
"     After each move the program checks to see if you have any legal",
"moves left.  If not, the game ends and your squares are counted.  If",
"you've made all the squares you win the game.  Otherwise, you are told",
"the number of squares you did make.",
"END"
};

void init(void);
int play(void);
void drawboard(void);
void dosquares(void);
void getfirst(void); 
void getrc(void);
void putstars(void);
int evalmove(void);
int chkmoves(void);
int endgame(void);
int chksqr(int, int);
void instruct(void);
void title(int, int);

int
main ()
{
	init ();
	for (;;)  
		if (!play ()) {
			endwin ();
			exit (0);
		}
}

void
init ()
{

	srand48 (getpid());
	initscr ();
	cbreak ();              /* immediate char return */
	noecho ();              /* no immediate echo */
	title (1,23);
	mvaddstr (23, 25, "Would you like instructions? ");
	refresh();
	if ((toupper(getch())) == 'Y') 
		instruct();
	clear ();
}

int 
play ()
{
	drawboard ();           /* clear screen and drawboard */
	for (j = 0; j < 64; j++) board[j]=0;
	getfirst ();            /* get the starting square */
	for (;;) {
		getrc();
		if (evalmove()) {
			putstars ();
			if (!chkmoves()) 
				return (endgame ());
		}
		else beep();
	}
}

void
drawboard ()
{
	erase ();
	dosquares ();
	refresh ();
	mvaddstr (0, 7, "1   2   3   4   5   6   7   8");
	for (j = 0; j < 8; j++) mvaddch (2*j+2, 3, j + 'A');
	refresh ();
	mvaddstr (20,  5, "ROW:");
	mvaddstr (20, 27, "COLUMN:");
	mvaddstr (14, 49, "CURRENT ROW");
	mvaddstr (16, 49, "CURRENT COL");
	mvaddstr (22,  5, "A - H or Q to quit");
	mvaddstr (22, 27, "1 - 8 or ESC to cancel row");
	refresh ();
	title (1,40);
}

void
dosquares ()
{
	mvaddstr (1, 6, "-------------------------------");
	for (j = 1; j < 9; j++){
		mvaddstr (2*j, 5,  "|   |   |   |   |   |   |   |   |");
		mvaddstr (2*j+1, 6, "-------------------------------");
	}
}

void
getfirst ()                             /* get first square */
{
	mvaddstr (23, 25, "(S)elect or (R)andom "); refresh ();
	do {
		row = toupper(getch());
	} while ((row != 'S') && (row != 'R'));
	if (row == 'R') {
		rw = lrand48() % 8;
		col = lrand48() % 8;
		j = 8* rw + col;
		row = rw + 'A';
		column = col + '1';
	}
	else {
		mvaddstr (23, 25, "Enter starting row and column");
		refresh ();
		getrc();                        /* get row and column */
	}
	putstars ();
	move (23, 0);
	clrtobot();
}       

void
getrc ()                                /* get row and column */
{
	noecho ();
	do {
		mvaddstr (20, 35, "  ");
		refresh ();
		do {
			mvaddch (20, 11, ' ');
			move (20, 11);
			refresh ();
			row=toupper(getch());
			if (row == 'Q') {
				endwin ();
				exit (1);
			}
		} while ((row < 'A') || (row > 'H'));
		addch (row);
		move (20, 35);
		refresh ();
		do {
			column=getch();
			if (column == '\033') break;
		} while ((column < '1') || (column > '8'));
		if (column != '\033') addch (column);
	} while (column == '\033');
	refresh();
	rw = row - 'A';
	col= column - '1';
	j = 8 * rw + col;
}

void
putstars ()                     /* place the stars, update board & currents */
{
	mvaddch (2*curow+2, 38, ' ');
	mvaddch (2*rw+2, 38, '<');
	mvaddch (18, curcol*4+7, ' ');
	mvaddch (18, col*4+7, '^');
	curow = rw;
	curcol= col;
	mvaddstr (2 * rw + 2, 4*col+6, "***");
	mvaddch (14, 61, row);
	mvaddch (16, 61, column);
	refresh ();
	board[j] = 1;
}

int
evalmove()                      /* convert row and column to integers */
		                /* and evaluate move */
{
	rdif = rw - curow;
	cdif = col - curcol;
	rdif = abs(rw  - curow);
	cdif = abs(col - curcol);
	refresh ();
	if ((rdif == 1) && (cdif == 2)) if (board [j] == 0) return (1);
	if ((rdif == 2) && (cdif == 1)) if (board [j] == 0) return (1);
	return (0);
}

int
chkmoves ()                     /* check to see if valid moves are available */
{
	if (chksqr(2,1))   return (1);
	if (chksqr(2,-1))  return (1);
	if (chksqr(-2,1))  return (1);
	if (chksqr(-2,-1)) return (1);
	if (chksqr(1,2))   return (1);
	if (chksqr(1,-2))  return (1);
	if (chksqr(-1,2))  return (1);
	if (chksqr(-1,-2)) return (1);
	return (0);
}

int
endgame ()                      /* check for filled board or not */
{
	rw = 0;
	for (j = 0; j < 64; j++) if (board[j] != 0) rw+=1;
	if (rw == 64) mvaddstr (20, 20, "Congratulations !! You got 'em all");
		else mvprintw (20, 20, "You have ended up with %2d squares",rw);
	mvaddstr (21, 25, "Play again ? (y/n) ");
	refresh ();
	if ((row=tolower(getch())) == 'y') return (1);
		else return (0);
}

#ifndef abs
abs(num)
int	num;
{
	if (num < 0) return (-num);
		else return (num);
}
#endif

int
chksqr (int n1, int n2)
{
int	r1, c1;

	r1 = rw + n1;
	c1 = col + n2;
	if ((r1<0) || (r1>7)) return (0);
	if ((c1<0) || (c1>7)) return (0);
	if (board[r1*8+c1] == 0) return (1);
		else return (0);
}

void
instruct()
{
int i;

	clear ();
	for (i=0;;i++) {
		if ((strcmp(instructions[i],"END"))) mvaddstr (i, 0, instructions[i]);
			else {
				mvaddstr (23, 25, "Ready to play ? (y/n) ");
				refresh();
				if (toupper(getch()) == 'Y') {
					clear ();
					return;
				} else {
					clear ();
					refresh ();
					endwin ();
					exit (0);
				}
			}
	}
}

void
title (y,x)
int	y,x;
{
char c;

	j = 0;
	do {
		c = script[j];
		if (c == 0) break ;
		mvaddch (ypos[j]+y, xpos[j]+x, c);
		j++;
	} while (c != 0);
	refresh ();
}


