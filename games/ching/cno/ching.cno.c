/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guy Harris.
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
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ching.cno.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 * cno - Read a question, cast a change, and output the line values to the
 * standard output for processing by "phx".
 */
#include <stdio.h>
#include "ching.h"

long	now;		/* current time */

unsigned seed;		/* seed for random number generator */
unsigned getrand();

char	*change();
char	string[6+1];	/* where the actual change string is put */

int	table[2][2][2] = {
	{ { OYIN,  YYANG,}, { YYANG, YYIN,} },
	{ { YYANG, YYIN,},  { YYIN,  OYANG,} },
};

main()
{
	FILE *logf;

	time(&now);
	seed = (int)now + getquest() + getgid() + getuid() + getpid();	/* randomize */
	printf("%s\n", change());
}

/*
 * Hash the question by adding all the characters together.
 */
int
getquest()
{
	int result;
	register int c;

	result = 0;
	while ((c = getchar()) != EOF)
		result += c;
	return(result);
}

/*
 * Get a set of six lines making up a change.
 */
char *
change()
{
	register int i;

	for (i = 0; i < 6; i++)
		string[i] = table[getrnum()&01][getrnum()&01][getrnum()&01] + '0';
	string[i] = '\0';
	return(string);
}

/*
 * Get a number more random than what getrand() gives.
 */
getrnum()
{
	return((getrand())>>(getrand()%17));
}

/*
 * Get a random number.
 */
unsigned
getrand()
{
	return(seed = (seed*13077) + 6925);
}
