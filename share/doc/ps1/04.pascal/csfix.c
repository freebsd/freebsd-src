/*-
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
/*static char sccsid[] = "from: @(#)csfix.c	6.3 (Berkeley) 4/17/91";*/
static char rcsid[] = "csfix.c,v 1.2 1993/08/01 18:23:36 mycroft Exp";
#endif /* not lint */

#include <stdio.h>
/*
 * csfix - fix constant spacing for error message flags in troff
 *
 * Bill Joy UCB September 11, 1977
 *
 * This would be better written in snobol!
 *
 * Normally fixes error flags in a pi listing
 * Optional - causes fixing of '---' and initial blank widthin a pxp profile.
 */

char	flag, dflag;

main(argc, argv)
	int argc;
	char *argv[];
{

	argc--, argv++;
	if (argc > 0 && argv[0][0] == '-' && argv[0][1] == 'd')
		dflag++, argc--, argv++;
	if (argc > 0 && argv[0][0] == '-')
		flag++, argc--, argv++;
	if (argc != 0) {
		write(2, "Usage: csfix\n", 13);
		exit(1);
	}
	while (getline()) {
		if (errline()) {
			flag ? fixpxp() : reformat();
			continue;
		}
		if (flag) {
			fixdigits();
			continue;
		}
		if (spwarn())
			continue;
		if (nontriv())
			save();
		if (dflag)
			fixdigits();
		else
			putline();
	}
	exit(0);
}

char	line[160], flagee[160], *digitty();

getline()
{
	register char *cp, c;

	for (cp = line, c = getchar(); c != '\n' && c != EOF; c = getchar())
		*cp++ = c;
	if (c == EOF)
		return (0);
	*cp = 0;
	return (1);
}

errline()
{
	register int i;
	register char *cp;

	for (cp = line; cp[0] && cp[1] && cp[2]; cp++)
		if (cp[0] == '-' && cp[1] == '-' && cp[2] == '-')
			return (1);
	return (0);
}

reformat()
{
	register char *cp, c, *tail;

	printf("%2.2s", line);
	if (line[0] != 'w')
		printf("\\l'\\w`w `u-\\w`%2.2s`u '", line);
	for (cp = line; *cp != 0 && *cp != '^'; cp++)
		continue;
	tail = cp + 1;
	if (cp[-1] == '\b' && cp[-2] == '|')
		cp -= 2;
	c = flagee[cp - line];
	flagee[cp - line] = 0;
	printf("\\l'\\w`%s`u-\\w`w `u\\&\\(rh'", flagee);
	flagee[cp - line] = c;
	if (c == '\0')
		c = flagee[cp - line - 1];
	printf("\\l'(\\w`%c`u-\\w`^`u)/2 '", c);
	printf("\\(ua");
	printf("\\l'(\\w`%c`u-\\w`^`u)/2 '", c);
	printf("\\l'\\w`---`u\\&\\(rh'%s\n", tail+3);
}

nontriv()
{

	switch (line[0]) {
		case 'E':
		case 'e':
		case 'w':
		case 's':
		case 0:
			return (0);
	}
	return (1);
}

save()
{

	strcpy(flagee, line);
}

putline()
{

	printf("%s\n", flag ? digitty(0) : line);
}

spwarn()
{

	if (line[0] != ' ' || line[1] != ' ' || line[2] != 'w')
		return (0);
	printf("  \\l'(\\w`E`u-\\w`w`u)/2 'w\\l'(\\w`E`u-\\w`w`u)/2 '");
	printf(&line[3]);
	printf("\n");
	return (1);
}

fixpxp()
{
	register char *cp;

	for (cp = line; *cp != '-'; cp++)
		continue;
	*cp = 0;
	printf("%s\\l'\\w`\\0\\0\\0\\0`u-\\w`.`u\\&\\(rh'%s\n", digitty(1), cp + 3);
}

char *
digitty(yup)
	char yup;
{
	register char *cp, *dp, *lp;

	for (lp = line; *lp && *lp != '|'; lp++)
		continue;
	if (yup == 0 && !*lp)
		return (line);
	for (cp = line, dp = flagee; cp < lp; cp++)
		if (*cp == ' ')
			*dp++ = '\\', *dp++ = '0';
		else
			*dp++ = *cp;
	strcpy(dp, cp);
	return (flagee);
}

fixdigits()
{
	register char *cp, c;

	for (cp = line; *cp == ' ' || *cp >= '0' && *cp <= '9'; cp++)
		continue;
	c = *cp, *cp = 0;
	digitty(1);
	*cp = c;
	printf("%s%s\n", flagee, cp);
}
