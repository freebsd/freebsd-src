.\" Copyright (c) 1980, 1993
.\"	 The Regents of the University of California.  All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\" 3. All advertising materials mentioning features or use of this software
.\"    must display the following acknowledgement:
.\"	This product includes software developed by the University of
.\"	California, Berkeley and its contributors.
.\" 4. Neither the name of the University nor the names of its contributors
.\"    may be used to endorse or promote products derived from this software
.\"    without specific prior written permission.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"
.\"	@(#)life.c	8.1 (Berkeley) 6/8/93
.\"
# include	<curses.h>
# include	<signal.h>

/*
 *	Run a life game.  This is a demonstration program for
 * the Screen Updating section of the -lcurses cursor package.
 */

typedef struct lst_st {			/* linked list element */
	int		y, x;		/* (y, x) position of piece */
	struct lst_st	*next, *last;	/* doubly linked */
} LIST;

LIST	*Head;			/* head of linked list */

int	die();

main(ac, av)
int	ac;
char	*av[];
{
	evalargs(ac, av);		/* evaluate arguments */

	initscr();			/* initialize screen package */
	signal(SIGINT, die);		/* set to restore tty stats */
	cbreak();			/* set for char-by-char */
	noecho();			/*	input */
	nonl();				/* for optimization */

	getstart();			/* get starting position */
	for (;;) {
		prboard();		/* print out current board */
		update();		/* update board position */
	}
}

/*
 * This is the routine which is called when rubout is hit.
 * It resets the tty stats to their original values.  This
 * is the normal way of leaving the program.
 */
die()
{
	signal(SIGINT, SIG_IGN);		/* ignore rubouts */
	mvcur(0, COLS - 1, LINES - 1, 0);	/* go to bottom of screen */
	endwin();				/* set terminal to good state */
	exit(0);
}

/*
 * Get the starting position from the user.  They keys u, i, o, j, l,
 * m, ,, and . are used for moving their relative directions from the
 * k key.  Thus, u move diagonally up to the left, , moves directly down,
 * etc.  x places a piece at the current position, " " takes it away.
 * The input can also be from a file.  The list is built after the
 * board setup is ready.
 */
getstart()
{
	reg char	c;
	reg int		x, y;
	auto char	buf[100];

	box(stdscr, '|', '_');		/* box in the screen */
	move(1, 1);			/* move to upper left corner */

	for (;;) {
		refresh();		/* print current position */
		if ((c = getch()) == 'q')
			break;
		switch (c) {
		  case 'u':
		  case 'i':
		  case 'o':
		  case 'j':
		  case 'l':
		  case 'm':
		  case ',':
		  case '.':
			adjustyx(c);
			break;
		  case 'f':
			mvaddstr(0, 0, "File name: ");
			getstr(buf);
			readfile(buf);
			break;
		  case 'x':
			addch('X');
			break;
		  case ' ':
			addch(' ');
			break;
		}
	}

	if (Head != NULL)			/* start new list */
		dellist(Head);
	Head = malloc(sizeof (LIST)); 

	/*
	 * loop through the screen looking for 'x's, and add a list
	 * element for each one
	 */
	for (y = 1; y < LINES - 1; y++)
		for (x = 1; x < COLS - 1; x++) {
			move(y, x);
			if (inch() == 'x')
				addlist(y, x);
		}
}

/*
 * Print out the current board position from the linked list
 */
prboard() {

	reg LIST	*hp;

	erase();			/* clear out last position */
	box(stdscr, '|', '_');		/* box in the screen */

	/*
	 * go through the list adding each piece to the newly
	 * blank board
	 */
	for (hp = Head; hp; hp = hp->next)
		mvaddch(hp->y, hp->x, 'X');

	refresh();
}
