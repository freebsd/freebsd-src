/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)dumpme.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"trek.h"

/*
**  Dump the starship somewhere in the galaxy
**
**	Parameter is zero if bounce off of negative energy barrier,
**	one if through a black hole
**
**	Note that the quadrant is NOT initialized here.  This must
**	be done from the calling routine.
**
**	Repair of devices must be deferred.
*/

dumpme(flag)
int	flag;
{
	register int		f;
	double			x;
	register struct event	*e;
	register int		i;

	f = flag;
	Ship.quadx = ranf(NQUADS);
	Ship.quady = ranf(NQUADS);
	Ship.sectx = ranf(NSECTS);
	Ship.secty = ranf(NSECTS);
	x += 1.5 * franf();
	Move.time += x;
	if (f)
	{
		printf("%s falls into a black hole.\n", Ship.shipname);
	}
	else
	{
		printf("Computer applies full reverse power to avoid hitting the\n");
		printf("   negative energy barrier.  A space warp was entered.\n");
	}
	/* bump repair dates forward */
	for (i = 0; i < MAXEVENTS; i++)
	{
		e = &Event[i];
		if (e->evcode != E_FIXDV)
			continue;
		reschedule(e, (e->date - Now.date) + x);
	}
	events(1);
	printf("You are now in quadrant %d,%d.  It is stardate %.2f\n",
		Ship.quadx, Ship.quady, Now.date);
	Move.time = 0;
}
