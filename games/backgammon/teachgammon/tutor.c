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
#if 0
static char sccsid[] = "@(#)tutor.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/backgammon/teachgammon/tutor.c,v 1.5 1999/11/30 03:48:31 billf Exp $";
#endif /* not lint */

#include "back.h"
#include "tutor.h"

extern int	maxmoves;
extern const char	*const finis[];

extern const struct situatn	test[];

static const char	better[] = "That is a legal move, but there is a better one.\n";

void
tutor ()  {
	int	i, j;

	i = 0;
	begscr = 18;
	cturn = -1;
	home = 0;
	bar = 25;
	inptr = &in[0];
	inopp = &in[1];
	offptr = &off[0];
	offopp = &off[1];
	Colorptr = &color[0];
	colorptr = &color[2];
	colen = 5;
	wrboard();

	while (1)  {
		if (! brdeq(test[i].brd,board))  {
			if (tflag && curr == 23)
				curmove (18,0);
			writel (better);
			nexturn();
			movback (mvlim);
			if (tflag)  {
				refresh();
				clrest ();
			}
			if ((! tflag) || curr == 19)  {
				proll();
				writec ('\t');
			}
			else
				curmove (curr > 19? curr-2: curr+4,25);
			getmove();
			if (cturn == 0)
				leave();
			continue;
		}
		if (tflag)
			curmove (18,0);
		text (*test[i].com);
		if (! tflag)
			writec ('\n');
		if (i == maxmoves)
			break;
		D0 = test[i].roll1;
		D1 = test[i].roll2;
		d0 = 0;
		mvlim = 0;
		for (j = 0; j < 4; j++)  {
			if (test[i].mp[j] == test[i].mg[j])
				break;
			p[j] = test[i].mp[j];
			g[j] = test[i].mg[j];
			mvlim++;
		}
		if (mvlim)
			for (j = 0; j < mvlim; j++)
				if (makmove(j))
					writel ("AARGH!!!\n");
		if (tflag)
			refresh();
		nexturn();
		D0 = test[i].new1;
		D1 = test[i].new2;
		d0 = 0;
		i++;
		mvlim = movallow();
		if (mvlim)  {
			if (tflag)
				clrest();
			proll();
			writec('\t');
			getmove();
			if (tflag)
				refresh();
			if (cturn == 0)
				leave();
		}
	}
	leave();
}

clrest ()  {
	int	r, c, j;

	r = curr;
	c = curc;
	for (j = r+1; j < 24; j++)  {
		curmove (j,0);
		cline();
	}
	curmove (r,c);
}

int
brdeq (b1,b2)
const int  *b1, *b2;

{
	const int  *e;

	e = b1+26;
	while (b1 < e)
		if (*b1++ != *b2++)
			return(0);
	return(1);
}
