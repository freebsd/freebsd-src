/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Copyright (c) 1983, 1984 1985, 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*	from OpenSolaris "refer4.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)refer4.c	1.3 (gritter) 10/22/05
 */

#include "refer..c"
#include <locale.h>
#include <string.h>

#define punctuat(c) (c=='.' || c=='?' || c=='!' || c==',' || c==';' || c==':')

static int gate = 0;
static char buff[BUFSIZ];

void
output(const char *s)
{
	if (gate)
		fputs(buff,ftemp);
	else
		gate = 1;
	if (strlen(s) > sizeof buff)
		err("one buff too big (%d)!", sizeof buff);
	n_strcpy(buff, s, sizeof(buff));
}

void
append(char *s)
{
	char *p;
	int lch;

	trimnl(buff);
	for (p = buff; *p; p++)
		;
	lch = *--p;
	if (postpunct && punctuat(lch))
		*p = 0;
	else /* pre-punctuation */
		switch (lch) {
		case '.': 
		case '?':
		case '!':
		case ',':
		case ';':
		case ':':
			*p++ = lch;
			*p = 0;
		}
	n_strcat(buff, s, sizeof(buff));
	if (postpunct)
		switch(lch) {
		case '.': 
		case '?':
		case '!':
		case ',':
		case ';':
		case ':':
			for(p = buff; *p; p++)
				;
			if (*--p == '\n')
				*p = 0;
			*p++ = lch;
			*p++ = '\n';
			*p = 0;
		}
	if (strlen(buff) > BUFSIZ)
		err("output buff too long (%d)", BUFSIZ);
}

void
flout(void)
{
	if (gate)
		fputs(buff,ftemp);
	gate = 0;
}

char *
trimnl(char *ln)
{
	register char *p = ln;

	while (*p)
		p++;
	p--;
	if (*p == '\n')
		*p = 0;
	return(ln);
}
