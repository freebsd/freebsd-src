/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The game adventure was originally written in Fortran by Will Crowther
 * and Don Woods.  It was later translated to C and enhanced by Jim
 * Gillogly.  This code is derived from software contributed to Berkeley
 * by Jim Gillogly at The Rand Corporation.
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
static char sccsid[] = "@(#)wizard.c	8.1 (Berkeley) 6/2/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/adventure/wizard.c,v 1.10 1999/12/19 00:21:51 billf Exp $";
#endif /* not lint */

/*      Re-coding of advent in C: privileged operations                 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
# include "hdr.h"

static int wizard (void);

void
datime(d,t)
int *d,*t;
{       struct tm *tptr;
	time_t tvec;

	time(&tvec);
	tptr=localtime(&tvec);
	/* day since 1977 */
	*d = (tptr->tm_yday + 365 * (tptr->tm_year - 77)
		+ (tptr->tm_year - 77) / 4 - (tptr->tm_year - 1) / 100
		+ (tptr->tm_year + 299) / 400);
	/* bug: this will overflow in the year 2066 AD (with 16 bit int) */
	/* it will be attributed to Wm the C's millenial celebration    */
	/* and minutes since midnite */
	*t=tptr->tm_hour*60+tptr->tm_min;
}


char magic[6];

void
poof()
{
	strcpy(magic, DECR(d,w,a,r,f));
	latncy = 45;
}

int
Start()
{       int d,t,delay;

	datime(&d,&t);
	delay=(d-saved)*1440+(t-savet); /* good for about a month     */

	if (delay >= latncy)
	{       saved = -1;
		return(FALSE);
	}
	printf("This adventure was suspended a mere %d minute%s ago.",
		delay, delay == 1? "" : "s");
	if (delay <= latncy/3)
	{       mspeak(2);
		exit(0);
	}
	mspeak(8);
	if (!wizard())
	{       mspeak(9);
		exit(0);
	}
	saved = -1;
	return(FALSE);
}

static int
wizard()                /* not as complex as advent/10 (for now)        */
{
	char *word,*x;
	if (!yesm(16,0,7)) return(FALSE);
	mspeak(17);
	getin(&word,&x);
	if (strncmp(word,magic,5))
	{       mspeak(20);
		return(FALSE);
	}
	mspeak(19);
	return(TRUE);
}

void
ciao()
{       char *c;
	char fname[80];

	printf("What would you like to call the saved version?\n");
	/* XXX - should use fgetln to avoid arbitrary limit */
	for (c = fname; c < fname + sizeof fname - 1; c++) {
		int ch;
		ch = getchar();
		if (ch == '\n' || ch == EOF)
			break;
		*c = ch;
	}
	*c=0;
	if (save(fname) != 0) return;           /* Save failed */
	printf("To resume, say \"adventure %s\".\n", fname);
	printf("\"With these rooms I might now have been familiarly acquainted.\"\n");
	exit(0);
}


int
ran(range)
int range;
{
	int i;

	i = random() % range;
	return(i);
}
