/*
 * Copyright (c) 1980 The Regents of the University of California.
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
"@(#) Copyright (c) 1980 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)retest.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

#include <ctype.h>

int l_onecase = 0;
char * _start;
char * _escaped;
char * convexp();
char * expmatch();
main()
{
    char reg[132];
    char *ireg;
    char str[132];
    char *match;
    char matstr[132];
    char c;

    while (1) {
	printf ("\nexpr: ");
	scanf ("%s", reg);
	ireg = convexp(reg);
	match = ireg;
	while(*match) {
	    switch (*match) {

	    case '\\':
	    case '(':
	    case ')':
	    case '|':
		printf ("%c", *match);
		break;

	    default:
		if (isalnum(*match))
		    printf("%c", *match);
		else
		    printf ("<%03o>", *match);
		break;
	    }
	    match++;
	}
	printf("\n");
	getchar();
	while(1) {
	    printf ("string: ");
	    match = str;
	    while ((c = getchar()) != '\n')
		*match++ = c;
	    *match = 0;
	    if (str[0] == '#')
		break;
	    matstr[0] = 0;
	    _start = str;
	    _escaped = 0;
	    match = expmatch (str, ireg, matstr);
	    if (match == 0)
		printf ("FAILED\n");
	    else
		printf ("match\nmatstr = %s\n", matstr);
	}

    }
}
