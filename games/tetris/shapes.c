/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek and Darren F. Provine.
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
 *
 *	@(#)shapes.c	8.1 (Berkeley) 5/31/93
 */

/*
 * Tetris shapes and related routines.
 *
 * Note that the first 7 are `well known'.
 */

#include <sys/cdefs.h>
#include "tetris.h"

#define	TL	-B_COLS-1	/* top left */
#define	TC	-B_COLS		/* top center */
#define	TR	-B_COLS+1	/* top right */
#define	ML	-1		/* middle left */
#define	MR	1		/* middle right */
#define	BL	B_COLS-1	/* bottom left */
#define	BC	B_COLS		/* bottom center */
#define	BR	B_COLS+1	/* bottom right */

struct shape shapes[] = {
	/* 0*/	7,	TL, TC, MR,
	/* 1*/	8,	TC, TR, ML,
	/* 2*/	9,	ML, MR, BC,
	/* 3*/	3,	TL, TC, ML,
	/* 4*/	12,	ML, BL, MR,
	/* 5*/	15,	ML, BR, MR,
	/* 6*/	18,	ML, MR, /* sticks out */ 2,
	/* 7*/	0,	TC, ML, BL,
	/* 8*/	1,	TC, MR, BR,
	/* 9*/	10,	TC, MR, BC,
	/*10*/	11,	TC, ML, MR,
	/*11*/	2,	TC, ML, BC,
	/*12*/	13,	TC, BC, BR,
	/*13*/	14,	TR, ML, MR,
	/*14*/	4,	TL, TC, BC,
	/*15*/	16,	TR, TC, BC,
	/*16*/	17,	TL, MR, ML,
	/*17*/	5,	TC, BC, BL,
	/*18*/	6,	TC, BC, /* sticks out */ 2*B_COLS,
};

/*
 * Return true iff the given shape fits in the given position,
 * taking the current board into account.
 */
int
fits_in(shape, pos)
	struct shape *shape;
	register int pos;
{
	register int *o = shape->off;

	if (board[pos] || board[pos + *o++] || board[pos + *o++] ||
	    board[pos + *o])
		return 0;
	return 1;
}

/*
 * Write the given shape into the current board, turning it on
 * if `onoff' is 1, and off if `onoff' is 0.
 */
void
place(shape, pos, onoff)
	struct shape *shape;
	register int pos, onoff;
{
	register int *o = shape->off;

	board[pos] = onoff;
	board[pos + *o++] = onoff;
	board[pos + *o++] = onoff;
	board[pos + *o] = onoff;
}
