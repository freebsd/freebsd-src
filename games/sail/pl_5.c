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
static char sccsid[] = "@(#)pl_5.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/sail/pl_5.c,v 1.6 1999/11/30 03:49:37 billf Exp $";
#endif /* not lint */

#include <string.h>
#include "player.h"

#define turnfirst(x) (*x == 'r' || *x == 'l')

acceptmove()
{
	int ta;
	int ma;
	char af;
	int moved = 0;
	int vma, dir;
	char prompt[60];
	char buf[60], last = '\0';
	char *p;

	if (!mc->crew3 || snagged(ms) || !windspeed) {
		Signal("Unable to move", (struct ship *)0);
		return;
	}

	ta = maxturns(ms, &af);
	ma = maxmove(ms, mf->dir, 0);
	(void) sprintf(prompt, "move (%d,%c%d): ", ma, af ? '\'' : ' ', ta);
	sgetstr(prompt, buf, sizeof buf);
	dir = mf->dir;
	vma = ma;
	for (p = buf; *p; p++)
		switch (*p) {
		case 'l':
			dir -= 2;
		case 'r':
			if (++dir == 0)
				dir = 8;
			else if (dir == 9)
				dir = 1;
			if (last == 't') {
				Signal("Ship can't turn that fast.",
					(struct ship *)0);
				*p-- = '\0';
			}
			last = 't';
			ma--;
			ta--;
			vma = min(ma, maxmove(ms, dir, 0));
			if (ta < 0 && moved || vma < 0 && moved)
				*p-- = '\0';
			break;
		case 'b':
			ma--;
			vma--;
			last = 'b';
			if (ta < 0 && moved || vma < 0 && moved)
				*p-- = '\0';
			break;
		case '0':
		case 'd':
			*p-- = '\0';
			break;
		case '\n':
			*p-- = '\0';
			break;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
			if (last == '0') {
				Signal("Can't move that fast.",
					(struct ship *)0);
				*p-- = '\0';
			}
			last = '0';
			moved = 1;
			ma -= *p - '0';
			vma -= *p - '0';
			if (ta < 0 && moved || vma < 0 && moved)
				*p-- = '\0';
			break;
		default:
			if (!isspace(*p)) {
				Signal("Input error.", (struct ship *)0);
				*p-- = '\0';
			}
		}
	if (ta < 0 && moved || vma < 0 && moved
	    || af && turnfirst(buf) && moved) {
		Signal("Movement error.", (struct ship *)0);
		if (ta < 0 && moved) {
			if (mf->FS == 1) {
				Write(W_FS, ms, 0, 0, 0, 0, 0);
				Signal("No hands to set full sails.",
					(struct ship *)0);
			}
		} else if (ma >= 0)
			buf[1] = '\0';
	}
	if (af && !moved) {
		if (mf->FS == 1) {
			Write(W_FS, ms, 0, 0, 0, 0, 0);
			Signal("No hands to set full sails.",
				(struct ship *)0);
		}
	}
	if (*buf)
		(void) strcpy(movebuf, buf);
	else
		(void) strcpy(movebuf, "d");
	Write(W_MOVE, ms, 1, (int)movebuf, 0, 0, 0);
	Signal("Helm: %s.", (struct ship *)0, movebuf);
}

acceptboard()
{
	struct ship *sp;
	int n;
	int crew[3];
	int men = 0;
	char c;

	crew[0] = mc->crew1;
	crew[1] = mc->crew2;
	crew[2] = mc->crew3;
	for (n = 0; n < NBP; n++) {
		if (mf->OBP[n].turnsent)
			    men += mf->OBP[n].mensent;
	}
	for (n = 0; n < NBP; n++) {
		if (mf->DBP[n].turnsent)
			    men += mf->DBP[n].mensent;
	}
	if (men) {
		crew[0] = men/100 ? 0 : crew[0] != 0;
		crew[1] = (men%100)/10 ? 0 : crew[1] != 0;
		crew[2] = men%10 ? 0 : crew[2] != 0;
	} else {
		crew[0] = crew[0] != 0;
		crew[1] = crew[1] != 0;
		crew[2] = crew[2] != 0;
	}
	foreachship(sp) {
		if (sp == ms || sp->file->dir == 0 || range(ms, sp) > 1)
			continue;
		if (ms->nationality == capship(sp)->nationality)
			continue;
		if (meleeing(ms, sp) && crew[2]) {
			c = sgetch("How many more to board the %s (%c%c)? ",
				sp, 1);
			parties(crew, sp, 0, c);
		} else if ((fouled2(ms, sp) || grappled2(ms, sp)) && crew[2]) {
			c = sgetch("Crew sections to board the %s (%c%c) (3 max) ?", sp, 1);
			parties(crew, sp, 0, c);
		}
	}
	if (crew[2]) {
		c = sgetch("How many sections to repel boarders? ",
			(struct ship *)0, 1);
		parties(crew, ms, 1, c);
	}
	blockalarm();
	draw_slot();
	unblockalarm();
}

parties(crew, to, isdefense, buf)
struct ship *to;
int crew[3];
char isdefense;
char buf;
{
	int k, j, men;
	struct BP *ptr;
	int temp[3];

	for (k = 0; k < 3; k++)
		temp[k] = crew[k];
	if (isdigit(buf)) {
		ptr = isdefense ? to->file->DBP : to->file->OBP;
		for (j = 0; j < NBP && ptr[j].turnsent; j++)
			;
		if (!ptr[j].turnsent && buf > '0') {
			men = 0;
			for (k = 0; k < 3 && buf > '0'; k++) {
				men += crew[k]
					* (k == 0 ? 100 : (k == 1 ? 10 : 1));
				crew[k] = 0;
				if (men)
					buf--;
			}
			if (buf > '0')
				Signal("Sending all crew sections.",
					(struct ship *)0);
			Write(isdefense ? W_DBP : W_OBP, ms, 0,
				j, turn, to->file->index, men);
			if (isdefense) {
				(void) wmove(slot_w, 2, 0);
				for (k=0; k < NBP; k++)
					if (temp[k] && !crew[k])
						(void) waddch(slot_w, k + '1');
					else
						(void) wmove(slot_w, 2, 1 + k);
				(void) mvwaddstr(slot_w, 3, 0, "DBP");
				makesignal(ms, "repelling boarders",
					(struct ship *)0);
			} else {
				(void) wmove(slot_w, 0, 0);
				for (k=0; k < NBP; k++)
					if (temp[k] && !crew[k])
						(void) waddch(slot_w, k + '1');
					else
						(void) wmove(slot_w, 0, 1 + k);
				(void) mvwaddstr(slot_w, 1, 0, "OBP");
				makesignal(ms, "boarding the %s (%c%c)", to);
			}
			blockalarm();
			(void) wrefresh(slot_w);
			unblockalarm();
		} else
			Signal("Sending no crew sections.", (struct ship *)0);
	}
}
