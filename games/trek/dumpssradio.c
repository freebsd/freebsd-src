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
static char sccsid[] = "@(#)dumpssradio.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

# include	"trek.h"

/**
 **	output hidden distress calls
 **/

dumpssradio()
{
	register struct event	*e;
	register int		j;
	register int		chkrest;

	chkrest = 0;
	for (j = 0; j < MAXEVENTS; j++)
	{
		e = &Event[j];
		/* if it is not hidden, then just ignore it */
		if ((e->evcode & E_HIDDEN) == 0)
			continue;
		if (e->evcode & E_GHOST)
		{
			unschedule(e);
			printf("Starsystem %s in quadrant %d,%d is no longer distressed\n",
				systemname(e), e->x, e->y);
			continue;
		}

		switch (e->evcode)
		{

		  case E_KDESB:
			printf("Starbase in quadrant %d,%d is under attack\n",
				e->x, e->y);
			chkrest++;
			break;

		  case E_ENSLV:
		  case E_REPRO:
			printf("Starsystem %s in quadrant %d,%d is distressed\n",
				systemname(e), e->x, e->y);
			chkrest++;
			break;

		}
	}

	return (chkrest);
}
