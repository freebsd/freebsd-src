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
static char sccsid[] = "@(#)wizard.c	8.1 (Berkeley) 6/2/93";
#endif /* not lint */

/*      Re-coding of advent in C: privileged operations                 */

# include "hdr.h"

datime(d,t)
int *d,*t;
{       int tvec[2],*tptr;
	int *localtime();

	time(tvec);
	tptr=localtime(tvec);
	*d=tptr[7]+365*(tptr[5]-77);    /* day since 1977  (mod leap)   */
	/* bug: this will overflow in the year 2066 AD                  */
	/* it will be attributed to Wm the C's millenial celebration    */
	*t=tptr[2]*60+tptr[1];          /* and minutes since midnite    */
}                                       /* pretty painless              */


char magic[6];

poof()
{
	strcpy(magic, DECR(d,w,a,r,f));
	latncy = 45;
}

Start(n)
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

wizard()                /* not as complex as advent/10 (for now)        */
{       register int wiz;
	char *word,*x;
	if (!yesm(16,0,7)) return(FALSE);
	mspeak(17);
	getin(&word,&x);
	if (!weq(word,magic))
	{       mspeak(20);
		return(FALSE);
	}
	mspeak(19);
	return(TRUE);
}

ciao(cmdfile)
char *cmdfile;
{       register char *c;
	register int outfd, size;
	char fname[80], buf[512];
	extern unsigned filesize;

	printf("What would you like to call the saved version?\n");
	for (c=fname;; c++)
		if ((*c=getchar())=='\n') break;
	*c=0;
	if (save(fname) != 0) return;           /* Save failed */
	printf("To resume, say \"adventure %s\".\n", fname);
	printf("\"With these rooms I might now have been familiarly acquainted.\"\n");
	exit(0);
}


ran(range)
int range;
{
	long rand(), i;

	i = rand() % range;
	return(i);
}
