.\" Copyright (c) 1980 The Regents of the University of California.
.\" All rights reserved.
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
.\"	from: @(#)twinkle1.c	6.3 (Berkeley) 4/17/91
.\"	twinkle1.c,v 1.2 1993/08/01 18:23:15 mycroft Exp
.\"
# include	<curses.h>
# include	<signal.h>

/*
 * the idea for this program was a product of the imagination of
 * Kurt Schoens.  Not responsible for minds lost or stolen.
 */

# define	NCOLS	80
# define	NLINES	24
# define	MAXPATTERNS	4

typedef struct {
	int	y, x;
} LOCS;

LOCS	Layout[NCOLS * NLINES];	/* current board layout */

int	Pattern,		/* current pattern number */
	Numstars;		/* number of stars in pattern */

char	*getenv();

int	die();

main()
{
	srand(getpid());		/* initialize random sequence */

	initscr();
	signal(SIGINT, die);
	noecho();
	nonl();
	leaveok(stdscr, TRUE);
	scrollok(stdscr, FALSE);

	for (;;) {
		makeboard();		/* make the board setup */
		puton('*');		/* put on '*'s */
		puton(' ');		/* cover up with ' 's */
	}
}

/*
 * On program exit, move the cursor to the lower left corner by
 * direct addressing, since current location is not guaranteed.
 * We lie and say we used to be at the upper right corner to guarantee
 * absolute addressing.
 */
die()
{
	signal(SIGINT, SIG_IGN);
	mvcur(0, COLS - 1, LINES - 1, 0);
	endwin();
	exit(0);
}


/*
 * Make the current board setup.  It picks a random pattern and
 * calls ison() to determine if the character is on that pattern
 * or not.
 */
makeboard()
{
	reg int		y, x;
	reg LOCS	*lp;

	Pattern = rand() % MAXPATTERNS;
	lp = Layout;
	for (y = 0; y < NLINES; y++)
		for (x = 0; x < NCOLS; x++)
			if (ison(y, x)) {
				lp->y = y;
				lp->x = x;
				lp++;
			}
	Numstars = lp - Layout;
}

/*
 * Return TRUE if (y, x) is on the current pattern.
 */
ison(y, x)
reg int	y, x; {

	switch (Pattern) {
	  case 0:	/* alternating lines */
		return !(y & 01);
	  case 1:	/* box */
		if (x >= LINES && y >= NCOLS)
			return FALSE;
		if (y < 3 || y >= NLINES - 3)
			return TRUE;
		return (x < 3 || x >= NCOLS - 3);
	  case 2:	/* holy pattern! */
		return ((x + y) & 01);
	  case 3:	/* bar across center */
		return (y >= 9 && y <= 15);
	}
	/* NOTREACHED */
}

puton(ch)
reg char	ch;
{
	reg LOCS	*lp;
	reg int		r;
	reg LOCS	*end;
	LOCS		temp;

	end = &Layout[Numstars];
	for (lp = Layout; lp < end; lp++) {
		r = rand() % Numstars;
		temp = *lp;
		*lp = Layout[r];
		Layout[r] = temp;
	}

	for (lp = Layout; lp < end; lp++) {
		mvaddch(lp->y, lp->x, ch);
		refresh();
	}
}
