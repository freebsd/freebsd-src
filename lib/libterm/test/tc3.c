/*-
 * Copyright (c) 1983 The Regents of the University of California.
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
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tc3.c	5.4 (Berkeley) 4/12/91";
#endif /* not lint */

/*
 * tc3 [term]
 * Dummy program to test out termlib.  Input two numbers (row and col)
 * and it prints out the tgoto string generated.
 */
#include <stdio.h>
char buf[1024];
char *getenv(), *tgetstr();
char *rdchar();
char *tgoto();
char *CM;
char cmbuff[30];
char *x;
char *UP;
char *tgout;

main(argc, argv) char **argv; {
	char *p;
	int rc;
	int row, col;

	if (argc < 2)
		p = getenv("TERM");
	else
		p = argv[1];
	rc = tgetent(buf,p);
	x = cmbuff;
	UP = tgetstr("up", &x);
	printf("UP = %x = ", UP); pr(UP); printf("\n");
	if (UP && *UP==0)
		UP = 0;
	CM = tgetstr("cm", &x);
	printf("CM = "); pr(CM); printf("\n");
	for (;;) {
		if (scanf("%d %d", &row, &col) < 2)
			exit(0);
		tgout = tgoto(CM, col, row);
		pr(tgout);
		printf("\n");
	}
}

pr(p)
register char *p;
{
	for (; *p; p++)
		printf("%s", rdchar(*p));
}

/*
 * rdchar() returns a readable representation of an ASCII character
 * using ^ for control, ' for meta.
 */
#include <ctype.h>
char *rdchar(c)
char c;
{
	static char ret[4];
	register char *p = ret;

	if ((c&0377) > 0177)
		*p++ = '\'';
	c &= 0177;
	if (!isprint(c))
		*p++ = '^';
	*p++ = (isprint(c) ?  c  : c^0100);
	*p = 0;
	return (ret);
}
