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
static char sccsid[] = "@(#)warp.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

# include	"trek.h"

/*
**  MOVE UNDER WARP POWER
**
**	This is both the "move" and the "ram" commands, differing
**	only in the flag 'fl'.  It is also used for automatic
**	emergency override mode, when 'fl' is < 0 and 'c' and 'd'
**	are the course and distance to be moved.  If 'fl' >= 0,
**	the course and distance are asked of the captain.
**
**	The guts of this routine are in the routine move(), which
**	is shared with impulse().  Also, the working part of this
**	routine is very small; the rest is to handle the slight chance
**	that you may be moving at some riduculous speed.  In that
**	case, there is code to handle time warps, etc.
*/

warp(fl, c, d)
int	fl, c;
double	d;
{
	int			course;
	double			power;
	double			dist;
	double			time;
	double			speed;
	double			frac;
	register int		percent;
	register int		i;
	extern double		move();

	if (Ship.cond == DOCKED)
		return (printf("%s is docked\n", Ship.shipname));
	if (damaged(WARP))
	{
		return (out(WARP));
	}
	if (fl < 0)
	{
		course = c;
		dist = d;
	}
	else
		if (getcodi(&course, &dist))
			return;

	/* check to see that we are not using an absurd amount of power */
	power = (dist + 0.05) * Ship.warp3;
	percent = 100 * power / Ship.energy + 0.5;
	if (percent >= 85)
	{
		printf("Scotty: That would consume %d%% of our remaining energy.\n",
			percent);
		if (!getynpar("Are you sure that is wise"))
			return;
	}

	/* compute the speed we will move at, and the time it will take */
	speed = Ship.warp2 / Param.warptime;
	time = dist / speed;

	/* check to see that that value is not ridiculous */
	percent = 100 * time / Now.time + 0.5;
	if (percent >= 85)
	{
		printf("Spock: That would take %d%% of our remaining time.\n",
			percent);
		if (!getynpar("Are you sure that is wise"))
			return;
	}

	/* compute how far we will go if we get damages */
	if (Ship.warp > 6.0 && ranf(100) < 20 + 15 * (Ship.warp - 6.0))
	{
		frac = franf();
		dist *= frac;
		time *= frac;
		damage(WARP, (frac + 1.0) * Ship.warp * (franf() + 0.25) * 0.20);
	}

	/* do the move */
	Move.time = move(fl, course, time, speed);

	/* see how far we actually went, and decrement energy appropriately */
	dist = Move.time * speed;
	Ship.energy -= dist * Ship.warp3 * (Ship.shldup + 1);

	/* test for bizarre events */
	if (Ship.warp <= 9.0)
		return;
	printf("\n\n  ___ Speed exceeding warp nine ___\n\n");
	sleep(2);
	printf("Ship's safety systems malfunction\n");
	sleep(2);
	printf("Crew experiencing extreme sensory distortion\n");
	sleep(4);
	if (ranf(100) >= 100 * dist)
	{
		return (printf("Equilibrium restored -- all systems normal\n"));
	}

	/* select a bizzare thing to happen to us */
	percent = ranf(100);
	if (percent < 70)
	{
		/* time warp */
		if (percent < 35 || !Game.snap)
		{
			/* positive time warp */
			time = (Ship.warp - 8.0) * dist * (franf() + 1.0);
			Now.date += time;
			printf("Positive time portal entered -- it is now Stardate %.2f\n",
				Now.date);
			for (i = 0; i < MAXEVENTS; i++)
			{
				percent = Event[i].evcode;
				if (percent == E_FIXDV || percent == E_LRTB)
					Event[i].date += time;
			}
			return;
		}

		/* s/he got lucky: a negative time portal */
		time = Now.date;
		i = (int) Etc.snapshot;
		bmove(i, Quad, sizeof Quad);
		bmove(i += sizeof Quad, Event, sizeof Event);
		bmove(i += sizeof Event, &Now, sizeof Now);
		printf("Negative time portal entered -- it is now Stardate %.2f\n",
			Now.date);
		for (i = 0; i < MAXEVENTS; i++)
			if (Event[i].evcode == E_FIXDV)
				reschedule(&Event[i], Event[i].date - time);
		return;
	}

	/* test for just a lot of damage */
	if (percent < 80)
		lose(L_TOOFAST);
	printf("Equilibrium restored -- extreme damage occured to ship systems\n");
	for (i = 0; i < NDEV; i++)
		damage(i, (3.0 * (franf() + franf()) + 1.0) * Param.damfac[i]);
	Ship.shldup = 0;
}
