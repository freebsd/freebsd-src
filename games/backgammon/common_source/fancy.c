/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)fancy.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include "back.h"

char	PC;			/* padding character */
char	*BC;			/* backspace sequence */
char	*CD;			/* clear to end of screen sequence */
char	*CE;			/* clear to end of line sequence */
char	*CL;			/* clear screen sequence */
char	*CM;			/* cursor movement instructions */
char	*HO;			/* home cursor sequence */
char	*MC;			/* column cursor movement map */
char	*ML;			/* row cursor movement map */
char	*ND;			/* forward cursor sequence */
char	*UP;			/* up cursor sequence */

int	lHO;			/* length of HO */
int	lBC;			/* length of BC */
int	lND;			/* length of ND */
int	lUP;			/* length of UP */
int	CO;			/* number of columns */
int	LI;			/* number of lines */
int	*linect;		/* array of lengths of lines on screen
				   (the actual screen is not stored) */

				/* two letter codes */
char	tcap[] = "bccdceclcmhomcmlndup";
				/* corresponding strings */
char	**tstr[] = { &BC, &CD, &CE, &CL, &CM, &HO, &MC, &ML, &ND, &UP };

int	buffnum;		/* pointer to output buffer */

char	tbuf[1024];		/* buffer for decoded termcap entries */

int	oldb[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int	oldr;
int	oldw;
						/* "real" cursor positions, so
						 * it knows when to reposition.
						 * These are -1 if curr and curc
						 * are accurate */
int	realr;
int	realc;

void	addbuf();

fboard ()  {
	register int	i, j, l;

	curmove (0,0);				/* do top line */
	for (i = 0; i < 53; i++)
		fancyc ('_');

	curmove (15,0);				/* do botttom line */
	for (i = 0; i < 53; i++)
		fancyc ('_');

	l = 1;					/* do vertical lines */
	for (i = 52; i > -1; i -= 28)  {
		curmove ( (l == 1? 1: 15) ,i);
		fancyc ('|');
		for (j = 0; j < 14; j++)  {
			curmove (curr+l,curc-1);
			fancyc ('|');
		}
		if (i == 24)
			i += 32;
		l = -l;				/* alternate directions */
	}

	curmove (2,1);				/* label positions 13-18 */
	for (i = 13; i < 18; i++)  {
		fancyc ('1');
		fancyc ((i % 10)+'0');
		curmove (curr,curc+2);
	}
	fancyc ('1');
	fancyc ('8');

	curmove (2,29);				/* label positions 19-24 */
	fancyc ('1');
	fancyc ('9');
	for (i = 20; i < 25; i++)  {
		curmove (curr,curc+2);
		fancyc ('2');
		fancyc ((i % 10)+'0');
	}

	curmove (14,1);				/* label positions 12-7 */
	fancyc ('1');
	fancyc ('2');
	for (i = 11; i > 6; i--)  {
		curmove (curr,curc+2);
		fancyc (i > 9? '1': ' ');
		fancyc ((i % 10)+'0');
	}

	curmove (14,30);			/* label positions 6-1 */
	fancyc ('6');
	for (i = 5; i > 0; i--) {
		curmove (curr,curc+3);
		fancyc (i+'0');
	}

	for (i = 12; i > 6; i--)		/* print positions 12-7 */
		if (board[i])
			bsect (board[i],13,1+4*(12-i),-1);

	if (board[0])				/* print red men on bar */
		bsect (board[0],13,25,-1);

	for (i = 6; i > 0; i--)			/* print positions 6-1 */
		if (board[i])
			bsect (board[i],13,29+4*(6-i),-1);

	l = (off[1] < 0? off[1]+15: off[1]);	/* print white's home */
	bsect (l,3,54,1);

	curmove (8,25);				/* print the word BAR */
	fancyc ('B');
	fancyc ('A');
	fancyc ('R');

	for (i = 13; i < 19; i++)		/* print positions 13-18 */
		if (board[i])
			bsect (board[i],3,1+4*(i-13),1);

	if (board[25])				/* print white's men on bar */
		bsect (board[25],3,25,1);

	for (i = 19; i < 25; i++)		/* print positions 19-24 */
		if (board[i])
			bsect (board[i],3,29+4*(i-19),1);

	l = (off[0] < 0? off[0]+15: off[0]);	/* print red's home */
	bsect (-l,13,54,-1);

	for (i = 0; i < 26; i++)		/* save board position
						 * for refresh later */
		oldb[i] = board[i];
	oldr = (off[1] < 0? off[1]+15: off[1]);
	oldw = -(off[0] < 0? off[0]+15: off[0]);
}

/*
 * bsect (b,rpos,cpos,cnext)
 *	Print the contents of a board position.  "b" has the value of the
 * position, "rpos" is the row to start printing, "cpos" is the column to
 * start printing, and "cnext" is positive if the position starts at the top
 * and negative if it starts at the bottom.  The value of "cpos" is checked
 * to see if the position is a player's home, since those are printed
 * differently.
 */

bsect (b,rpos,cpos,cnext)
int	b;					/* contents of position */
int	rpos;					/* row of position */
int	cpos;					/* column of position */
int	cnext;					/* direction of position */

{
	register int	j;			/* index */
	register int	n;			/* number of men on position */
	register int	bct;			/* counter */
	int		k;			/* index */
	char		pc;			/* color of men on position */

	n = abs(b);				/* initialize n and pc */
	pc = (b > 0? 'r': 'w');

	if (n < 6 && cpos < 54)			/* position cursor at start */
		curmove (rpos,cpos+1);
	else
		curmove (rpos,cpos);

	for (j = 0; j < 5; j++)  {		/* print position row by row */

		for (k = 0; k < 15; k += 5)		/* print men */
			if (n > j+k)
				fancyc (pc);

		if (j < 4)  {				/* figure how far to
							 * back up for next
							 * row */
			if (n < 6)  {			/* stop if none left */
				if (j+1 == n)
					break;
				bct = 1;		/* single column */
			} else  {
				if (n < 11)  {		/* two columns */
					if (cpos == 54)  {	/* home pos */
						if (j+5 >= n)
							bct = 1;
						else
							bct = 2;
					}
					if (cpos < 54)  {	/* not home */
						if (j+6 >= n)
							bct = 1;
						else
							bct = 2;
					}
				} else  {		/* three columns */
					if (j+10 >= n)
						bct = 2;
					else
						bct = 3;
				}
			}
			curmove (curr+cnext,curc-bct);	/* reposition cursor */
		}
	}
}

refresh()  {
	register int	i, r, c;

	r = curr;				/* save current position */
	c = curc;

	for (i = 12; i > 6; i--)		/* fix positions 12-7 */
		if (board[i] != oldb[i])  {
			fixpos (oldb[i],board[i],13,1+(12-i)*4,-1);
			oldb[i] = board[i];
		}

	if (board[0] != oldb[0])  {		/* fix red men on bar */
		fixpos (oldb[0],board[0],13,25,-1);
		oldb[0] = board[0];
	}

	for (i = 6; i > 0; i--)			/* fix positions 6-1 */
		if (board[i] != oldb[i])  {
			fixpos (oldb[i],board[i],13,29+(6-i)*4,-1);
			oldb[i] = board[i];
		}

	i = -(off[0] < 0? off[0]+15: off[0]);	/* fix white's home */
	if (oldw != i)  {
		fixpos (oldw,i,13,54,-1);
		oldw = i;
	}

	for (i = 13; i < 19; i++)		/* fix positions 13-18 */
		if (board[i] != oldb[i])  {
			fixpos (oldb[i],board[i],3,1+(i-13)*4,1);
			oldb[i] = board[i];
		}

	if (board[25] != oldb[25])  {		/* fix white men on bar */
		fixpos (oldb[25],board[25],3,25,1);
		oldb[25] = board[25];
	}

	for (i = 19; i < 25; i++)		/* fix positions 19-24 */
		if (board[i] != oldb[i])  {
			fixpos (oldb[i],board[i],3,29+(i-19)*4,1);
			oldb[i] = board[i];
		}

	i = (off[1] < 0? off[1]+15: off[1]);	/* fix red's home */
	if (oldr != i)  {
		fixpos (oldr,i,3,54,1);
		oldr = i;
	}

	curmove (r,c);				/* return to saved position */
	newpos();
	buflush();
}

fixpos (old,new,r,c,inc)
int	old, new, r, c, inc;

{
	register int	o, n, nv;
	int		ov, nc;
	char		col;

	if (old*new >= 0)  {
		ov = abs(old);
		nv = abs(new);
		col = (old+new > 0? 'r': 'w');
		o = (ov-1)/5;
		n = (nv-1)/5;
		if (o == n)  {
			if (o == 2)
				nc = c+2;
			if (o == 1)
				nc = c < 54? c: c+1;
			if (o == 0)
				nc = c < 54? c+1: c;
			if (ov > nv)
				fixcol (r+inc*(nv-n*5),nc,abs(ov-nv),' ',inc);
			else
				fixcol (r+inc*(ov-o*5),nc,abs(ov-nv),col,inc);
			return;
		} else  {
			if (c < 54)  {
				if (o+n == 1)  {
					if (n)  {
						fixcol (r,c,abs(nv-5),col,inc);
						if (ov != 5)
							fixcol (r+inc*ov,c+1,abs(ov-5),col,inc);
					} else  {
						fixcol (r,c,abs(ov-5),' ',inc);
						if (nv != 5)
							fixcol (r+inc*nv,c+1,abs(nv-5),' ',inc);
					}
					return;
				}
				if (n == 2)  {
					if (ov != 10)
						fixcol (r+inc*(ov-5),c,abs(ov-10),col,inc);
					fixcol (r,c+2,abs(nv-10),col,inc);
				} else  {
					if (nv != 10)
						fixcol (r+inc*(nv-5),c,abs(nv-10),' ',inc);
					fixcol (r,c+2,abs(ov-10),' ',inc);
				}
				return;
			}
			if (n > o)  {
				fixcol (r+inc*(ov%5),c+o,abs(5*n-ov),col,inc);
				if (nv != 5*n)
					fixcol (r,c+n,abs(5*n-nv),col,inc);
			} else  {
				fixcol (r+inc*(nv%5),c+n,abs(5*n-nv),' ',inc);
				if (ov != 5*o)
					fixcol (r,c+o,abs(5*o-ov),' ',inc);
			}
			return;
		}
	}
	nv = abs(new);
	fixcol (r,c+1,nv,new > 0? 'r': 'w',inc);
	if (abs(old) <= abs(new))
		return;
	fixcol (r+inc*new,c+1,abs(old+new),' ',inc);
}

fixcol (r,c,l,ch,inc)
register int	l, ch;
int		r, c, inc;

{
	register int	i;

	curmove (r,c);
	fancyc (ch);
	for (i = 1; i < l; i++)  {
		curmove (curr+inc,curc-1);
		fancyc (ch);
	}
}

curmove (r,c)
register int	r, c;

{
	if (curr == r && curc == c)
		return;
	if (realr == -1)  {
		realr = curr;
		realc = curc;
	}
	curr = r;
	curc = c;
}

newpos ()  {
	register int	r;		/* destination row */
	register int	c;		/* destination column */
	register int	mode = -1;	/* mode of movement */

	int	count = 1000;		/* character count */
	int	i;			/* index */
	int	j;			/* index */
	int	n;			/* temporary variable */
	char	*m;			/* string containing CM movement */


	if (realr == -1)		/* see if already there */
		return;

	r = curr;			/* set current and dest. positions */
	c = curc;
	curr = realr;
	curc = realc;

					/* double check position */
	if (curr == r && curc == c)  {
		realr = realc = -1;
		return;
	}

	if (CM)  {			/* try CM to get there */
		mode = 0;
		m = (char *)tgoto (CM,c,r);
		count = strlen (m);
	}

					/* try HO and local movement */
	if (HO && (n = r+c*lND+lHO) < count)  {
		mode = 1;
		count = n;
	}

					/* try various LF combinations */
	if (r >= curr)  {
						/* CR, LF, and ND */
		if ((n = (r-curr)+c*lND+1) < count)  {
			mode = 2;
			count = n;
		}
						/* LF, ND */
		if (c >= curc && (n = (r-curr)+(c-curc)*lND) < count)  {
			mode = 3;
			count = n;
		}
						/* LF, BS */
		if (c < curc && (n = (r-curr)+(curc-c)*lBC) < count)  {
			mode = 4;
			count = n;
		}
	}

					/* try corresponding UP combinations */
	if (r < curr)  {
						/* CR, UP, and ND */
		if ((n = (curr-r)*lUP+c*lND+1) < count)  {
			mode = 5;
			count = n;
		}
						/* UP and ND */
		if (c >= curc && (n = (curr-r)*lUP+(c-curc)*lND) < count)  {
			mode = 6;
			count = n;
		}
						/* UP and BS */
		if (c < curc && (n = (curr-r)*lUP+(curc-c)*lBC) < count)  {
			mode = 7;
			count = n;
		}
	}

						/* space over */
	if (curr == r && c > curc && linect[r] < curc && c-curc < count)
		mode = 8;

	switch (mode)  {

	case -1:				/* error! */
		write (2,"\r\nInternal cursor error.\r\n",26);
		getout();

						/* direct cursor motion */
	case  0:
		tputs (m,abs(curr-r),addbuf);
		break;

						/* relative to "home" */
	case  1:
		tputs (HO,r,addbuf);
		for (i = 0; i < r; i++)
			addbuf ('\012');
		for (i = 0; i < c; i++)
			tputs (ND,1,addbuf);
		break;

						/* CR and down and over */
	case  2:
		addbuf ('\015');
		for (i = 0; i < r-curr; i++)
			addbuf ('\012');
		for (i = 0; i < c; i++)
			tputs (ND,1,addbuf);
		break;
	
						/* down and over */
	case  3:
		for (i = 0; i < r-curr; i++)
			addbuf ('\012');
		for (i = 0; i < c-curc; i++)
			tputs (ND,1,addbuf);
		break;
	
						/* down and back */
	case  4:
		for (i = 0; i < r-curr; i++)
			addbuf ('\012');
		for (i = 0; i < curc-c; i++)
			addbuf ('\010');
		break;
	
						/* CR and up and over */
	case  5:
		addbuf ('\015');
		for (i = 0; i < curr-r; i++)
			tputs (UP,1,addbuf);
		for (i = 0; i < c; i++)
			tputs (ND,1,addbuf);
		break;
	
						/* up and over */
	case  6:
		for (i = 0; i < curr-r; i++)
			tputs (UP,1,addbuf);
		for (i = 0; i < c-curc; i++)
			tputs (ND,1,addbuf);
		break;
	
						/* up and back */
	case  7:
		for (i = 0; i < curr-r; i++)
			tputs (UP,1,addbuf);
		for (i = 0; i < curc-c; i++)  {
			if (BC)
				tputs (BC,1,addbuf);
			else
				addbuf ('\010');
		}
		break;

						/* safe space */
	case  8:
		for (i = 0; i < c-curc; i++)
			addbuf (' ');
	}

						/* fix positions */
	curr = r;
	curc = c;
	realr = -1;
	realc = -1;
}

clear ()  {
	register int	i;

					/* double space if can't clear */
	if (CL == 0)  {
		writel ("\n\n");
		return;
	}

	curr = curc = 0;		/* fix position markers */
	realr = realc = -1;
	for (i = 0; i < 24; i++)	/* clear line counts */
		linect[i] = -1;
	buffnum = -1;			/* ignore leftover buffer contents */
	tputs (CL,CO,addbuf);		/* put CL in buffer */
}

tos ()  {				/* home cursor */
	curmove (0,0);
}

fancyc (c)
register char	c;			/* character to output */
{
	register int	sp;		/* counts spaces in a tab */

	if (c == '\007')  {		/* bells go in blindly */
		addbuf (c);
		return;
	}

					/* process tabs, use spaces if the
					 * the tab should be erasing things,
					 * otherwise use cursor movement
					 * routines.  Note this does not use
					 * hardware tabs at all. */
	if (c == '\t')  {
		sp = (curc+8) & (~ 7);		/* compute spaces */
						/* check line length */
		if (linect[curr] >= curc || sp < 4)  {
			for (; sp > curc; sp--)
				addbuf (' ');
			curc = sp;		/* fix curc */
		} else
			curmove (curr,sp);
		return;
	}

					/* do newline be calling newline */
	if (c == '\n')  {
		newline();
		return;
	}

					/* ignore any other control chars */
	if (c < ' ')
		return;

					/* if an erasing space or non-space,
					 * just add it to buffer.  Otherwise
					 * use cursor movement routine, so that
					 * multiple spaces will be grouped
					 * together */
	if (c > ' ' || linect[curr] >= curc)  {
		newpos ();			/* make sure position correct */
		addbuf (c);			/* add character to buffer */
						/* fix line length */
		if (c == ' ' && linect[curr] == curc)
			linect[curr]--;
		else if (linect[curr] < curc)
			linect[curr] = curc;
		curc++;				/* fix curc */
	} else
					/* use cursor movement routine */
		curmove (curr,curc+1);
}

clend()  {
	register int	i;
	register char	*s;


	if (CD)  {
		tputs (CD,CO-curr,addbuf);
		for (i = curr; i < LI; i++)
			linect[i] = -1;
		return;
	}

	curmove (i = curr,0);
	cline();
	while (curr < LI-1)  {
		curmove (curr+1,0);
		if (linect[curr] > -1)
			cline ();
	}
	curmove (i,0);
}

cline ()  {
	register int	i;
	register int	c;
	register char	*s;

	if (curc > linect[curr])
		return;
	newpos ();
	if (CE)  {
		tputs (CE,1,addbuf);
		linect[curr] = curc-1;
	} else  {
		c = curc-1;
		while (linect[curr] > c)  {
			addbuf (' ');
			curc++;
			linect[curr]--;
		}
		curmove (curr,c+1);
	}
}

newline ()  {
	cline();
	if (curr == LI-1)
		curmove (begscr,0);
	else
		curmove (curr+1,0);
}

getcaps (s)
register char	*s;

{
	register char	*code;		/* two letter code */
	register char	***cap;		/* pointer to cap string */
	char		*bufp;		/* pointer to cap buffer */
	char		tentry[1024];	/* temporary uncoded caps buffer */

	tgetent (tentry,s);		/* get uncoded termcap entry */

	LI = tgetnum ("li");		/* get number of lines */
	if (LI == -1)
		LI = 12;
	CO = tgetnum ("co");		/* get number of columns */
	if (CO == -1)
		CO = 65;

	bufp = tbuf;			/* get padding character */
	tgetstr ("pc",&bufp);
	if (bufp != tbuf)
		PC = *tbuf;
	else
		PC = 0;

	bufp = tbuf;			/* get string entries */
	cap = tstr;
	for (code = tcap; *code; code += 2)
		**cap++ = (char *)tgetstr (code,&bufp);

					/* get pertinent lengths */
	if (HO)
		lHO = strlen (HO);
	if (BC)
		lBC = strlen (BC);
	else
		lBC = 1;
	if (UP)
		lUP = strlen (UP);
	if (ND)
		lND = strlen (ND);
	if (LI < 24 || CO < 72 || !(CL && UP && ND))
		return (0);
	linect = (int *)calloc (LI+1,sizeof(int));
	return (1);
}
