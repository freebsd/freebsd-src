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
static char sccsid[] = "@(#)destruct.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

# include	"trek.h"

/*
**  Self Destruct Sequence
**
**	The computer starts up the self destruct sequence.  Obviously,
**	if the computer is out nothing can happen.  You get a countdown
**	and a request for password.  This must match the password that
**	you entered at the start of the game.
**
**	You get to destroy things when you blow up; hence, it is
**	possible to win the game by destructing if you take the last
**	Klingon with you.
**
**	By the way, the \032 in the message is a ^Z, which is because
**	the terminal in my office is an ADM-3, which uses that char-
**	acter to clear the screen.  I also stick in a \014 (form feed)
**	because that clears some other screens.
**
**	Uses trace flag 41
*/

destruct()
{
	char		checkpass[15];
	register int	i, j;
	double		zap;

	if (damaged(COMPUTER))
		return (out(COMPUTER));
	printf("\n\07 --- WORKING ---\07\n");
	sleep(3);
	/* output the count 10 9 8 7 6 */
	for (i = 10; i > 5; i--)
	{
		for (j = 10;  j > i; j--)
			printf("   ");
		printf("%d\n", i);
		sleep(1);
	}
	/* check for password on new line only */
	skiptonl(0);
	getstrpar("Enter password verification", checkpass, 14, 0);
	sleep(2);
	if (!sequal(checkpass, Game.passwd))
		return (printf("Self destruct sequence aborted\n"));
	printf("Password verified; self destruct sequence continues:\n");
	sleep(2);
	/* output count 5 4 3 2 1 0 */
	for (i = 5; i >= 0; i--)
	{
		sleep(1);
		for (j = 5; j > i; j--)
			printf("   ");
		printf("%d\n", i);
	}
	sleep(2);
	printf("\032\014***** %s destroyed *****\n", Ship.shipname);
	Game.killed = 1;
	/* let's see what we can blow up!!!! */
	zap = 20.0 * Ship.energy;
	Game.deaths += Ship.crew;
	for (i = 0; i < Etc.nkling; )
	{
		if (Etc.klingon[i].power * Etc.klingon[i].dist <= zap)
			killk(Etc.klingon[i].x, Etc.klingon[i].y);
		else
			i++;
	}
	/* if we didn't kill the last Klingon (detected by killk), */
	/* then we lose.... */
	lose(L_DSTRCT);
}
