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
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)colrm.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>
/*
COLRM removes unwanted columns from a file
	Jeff Schriebman  UC Berkeley 11-74
*/


main(argc,argv)
char **argv;
{
	register c, ct, first, last;

	first = 0;
	last = 0;
	if (argc > 1)
		first = getn(*++argv);
	if (argc > 2)
		last = getn(*++argv);

start:
	ct = 0;
loop1:
	c = getc(stdin);
	if (feof(stdin))
		goto fin;
	if (c == '\t')
		ct = (ct + 8) & ~7;
	else if (c == '\b')
		ct = ct ? ct - 1 : 0;
	else
		ct++;
	if (c == '\n') {
		putc(c, stdout);
		goto start;
	}
	if (!first || ct < first) {
		putc(c, stdout);
		goto loop1;
	}

/* Loop getting rid of characters */
	while (!last || ct < last) {
		c = getc(stdin);
		if (feof(stdin))
			goto fin;
		if (c == '\n') {
			putc(c, stdout);
			goto start;
		}
		if (c == '\t')
			ct = (ct + 8) & ~7;
		else if (c == '\b')
			ct = ct ? ct - 1 : 0;
		else
			ct++;
	}

/* Output last of the line */
	for (;;) {
		c = getc(stdin);
		if (feof(stdin))
			break;
		putc(c, stdout);
		if (c == '\n')
			goto start;
	}
fin:
	fflush(stdout);
	exit(0);
}

getn(ap)
char *ap;
{
	register int n,c;
	register char *p;

	p = ap;
	n = 0;
	while ((c = *p++) >= '0' && c <= '9')
		n = n*10 + c - '0';
	return(n);
}
