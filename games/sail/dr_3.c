/*
 * Copyright (c) 1983, 1993
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
static char sccsid[] = "@(#)dr_3.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/sail/dr_3.c,v 1.6 1999/11/30 03:49:32 billf Exp $";
#endif /* not lint */

#include "driver.h"

moveall()		/* move all comp ships */
{
	struct ship *sp, *sq;		/* r11, r10 */
	int n;				/* r9 */
	int k, l;			/* r8, r7 */
	int row[NSHIP], col[NSHIP], dir[NSHIP], drift[NSHIP];
	char moved[NSHIP];

	/*
	 * first try to create moves for OUR ships
	 */
	foreachship(sp) {
		struct ship *closest;
		int ma, ta;
		char af;

		if (sp->file->captain[0] || sp->file->dir == 0)
			continue;
		if (!sp->file->struck && windspeed && !snagged(sp)
		    && sp->specs->crew3) {
			ta = maxturns(sp, &af);
			ma = maxmove(sp, sp->file->dir, 0);
			closest = closestenemy(sp, 0, 0);
			if (closest == 0)
				*sp->file->movebuf = '\0';
			else
				closeon(sp, closest, sp->file->movebuf,
					ta, ma, af);
		} else
			*sp->file->movebuf = '\0';
	}
	/*
	 * Then execute the moves for ALL ships (dead ones too),
	 * checking for collisions and snags at each step.
	 * The old positions are saved in row[], col[], dir[].
	 * At the end, we compare and write out the changes.
	 */
	n = 0;
	foreachship(sp) {
		if (snagged(sp))
			(void) strcpy(sp->file->movebuf, "d");
		else
			if (*sp->file->movebuf != 'd')
				(void) strcat(sp->file->movebuf, "d");
		row[n] = sp->file->row;
		col[n] = sp->file->col;
		dir[n] = sp->file->dir;
		drift[n] = sp->file->drift;
		moved[n] = 0;
		n++;
	}
	/*
	 * Now resolve collisions.
	 * This is the tough part.
	 */
	for (k = 0; stillmoving(k); k++) {
		/*
		 * Step once.
		 * And propagate the nulls at the end of sp->file->movebuf.
		 */
		n = 0;
		foreachship(sp) {
			if (!sp->file->movebuf[k])
				sp->file->movebuf[k+1] = '\0';
			else if (sp->file->dir)
				step(sp->file->movebuf[k], sp, &moved[n]);
			n++;
		}
		/*
		 * The real stuff.
		 */
		n = 0;
		foreachship(sp) {
			if (sp->file->dir == 0 || isolated(sp))
				goto cont1;
			l = 0;
			foreachship(sq) {
				char snap = 0;

				if (sp == sq)
					goto cont2;
				if (sq->file->dir == 0)
					goto cont2;
				if (!push(sp, sq))
					goto cont2;
				if (snagged2(sp, sq) && range(sp, sq) > 1)
					snap++;
				if (!range(sp, sq) && !fouled2(sp, sq)) {
					makesignal(sp,
						"collision with %s (%c%c)", sq);
					if (die() < 4) {
						makesignal(sp,
							"fouled with %s (%c%c)",
							sq);
						Write(W_FOUL, sp, 0, l, 0, 0, 0);
						Write(W_FOUL, sq, 0, n, 0, 0, 0);
					}
					snap++;
				}
				if (snap) {
					sp->file->movebuf[k + 1] = 0;
					sq->file->movebuf[k + 1] = 0;
					sq->file->row = sp->file->row - 1;
					if (sp->file->dir == 1
					    || sp->file->dir == 5)
						sq->file->col =
							sp->file->col - 1;
					else
						sq->file->col = sp->file->col;
					sq->file->dir = sp->file->dir;
				}
			cont2:
				l++;
			}
		cont1:
			n++;
		}
	}
	/*
	 * Clear old moves.  And write out new pos.
	 */
	n = 0;
	foreachship(sp) {
		if (sp->file->dir != 0) {
			*sp->file->movebuf = 0;
			if (row[n] != sp->file->row)
				Write(W_ROW, sp, 0, sp->file->row, 0, 0, 0);
			if (col[n] != sp->file->col)
				Write(W_COL, sp, 0, sp->file->col, 0, 0, 0);
			if (dir[n] != sp->file->dir)
				Write(W_DIR, sp, 0, sp->file->dir, 0, 0, 0);
			if (drift[n] != sp->file->drift)
				Write(W_DRIFT, sp, 0, sp->file->drift, 0, 0, 0);
		}
		n++;
	}
}

stillmoving(k)
int k;
{
	struct ship *sp;

	foreachship(sp)
		if (sp->file->movebuf[k])
			return 1;
	return 0;
}

isolated(ship)
struct ship *ship;
{
	struct ship *sp;

	foreachship(sp) {
		if (ship != sp && range(ship, sp) <= 10)
			return 0;
	}
	return 1;
}

push(from, to)
struct ship *from, *to;
{
	int bs, sb;

	sb = to->specs->guns;
	bs = from->specs->guns;
	if (sb > bs)
		return 1;
	if (sb < bs)
		return 0;
	return from < to;
}

step(com, sp, moved)
char com;
struct ship *sp;
char *moved;
{
	int dist;

	switch (com) {
	case 'r':
		if (++sp->file->dir == 9)
			sp->file->dir = 1;
		break;
	case 'l':
		if (--sp->file->dir == 0)
			sp->file->dir = 8;
		break;
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
		if (sp->file->dir % 2 == 0)
			dist = dtab[com - '0'];
		else
			dist = com - '0';
		sp->file->row -= dr[sp->file->dir] * dist;
		sp->file->col -= dc[sp->file->dir] * dist;
		*moved = 1;
		break;
	case 'b':
		break;
	case 'd':
		if (!*moved) {
			if (windspeed != 0 && ++sp->file->drift > 2 &&
			    (sp->specs->class >= 3 && !snagged(sp)
			     || (turn & 1) == 0)) {
				sp->file->row -= dr[winddir];
				sp->file->col -= dc[winddir];
			}
		} else
			sp->file->drift = 0;
		break;
	}
}

sendbp(from, to, sections, isdefense)
struct ship *from, *to;
int sections;
char isdefense;
{
	int n;
	struct BP *bp;

	bp = isdefense ? from->file->DBP : from->file->OBP;
	for (n = 0; n < NBP && bp[n].turnsent; n++)
		;
	if (n < NBP && sections) {
		Write(isdefense ? W_DBP : W_OBP, from, 0,
			n, turn, to->file->index, sections);
		if (isdefense)
			makesignal(from, "repelling boarders",
				(struct ship *)0);
		else
			makesignal(from, "boarding the %s (%c%c)", to);
	}
}

toughmelee(ship, to, isdefense, count)
struct ship *ship, *to;
int isdefense, count;
{
	struct BP *bp;
	int obp = 0;
	int n, OBP = 0, DBP = 0, dbp = 0;
	int qual;

	qual = ship->specs->qual;
	bp = isdefense ? ship->file->DBP : ship->file->OBP;
	for (n = 0; n < NBP; n++, bp++) {
		if (bp->turnsent && (to == bp->toship || isdefense)) {
			obp += bp->mensent / 100
				? ship->specs->crew1 * qual : 0;
			obp += (bp->mensent % 100)/10
				? ship->specs->crew2 * qual : 0;
			obp += bp->mensent % 10
				? ship->specs->crew3 * qual : 0;
		}
	}
	if (count || isdefense)
		return obp;
	OBP = toughmelee(to, ship, 0, count + 1);
	dbp = toughmelee(ship, to, 1, count + 1);
	DBP = toughmelee(to, ship, 1, count + 1);
	if (OBP > obp + 10 || OBP + DBP >= obp + dbp + 10)
		return 1;
	else
		return 0;
}

reload()
{
	struct ship *sp;

	foreachship(sp) {
		sp->file->loadwith = 0;
	}
}

checksails()
{
	struct ship *sp;
	int rig, full;
	struct ship *close;

	foreachship(sp) {
		if (sp->file->captain[0] != 0)
			continue;
		rig = sp->specs->rig1;
		if (windspeed == 6 || windspeed == 5 && sp->specs->class > 4)
			rig = 0;
		if (rig && sp->specs->crew3) {
			close = closestenemy(sp, 0, 0);
			if (close != 0) {
				if (range(sp, close) > 9)
					full = 1;
				else
					full = 0;
			} else
				full = 0;
		} else
			full = 0;
		if ((sp->file->FS != 0) != full)
			Write(W_FS, sp, 0, full, 0, 0, 0);
	}
}
