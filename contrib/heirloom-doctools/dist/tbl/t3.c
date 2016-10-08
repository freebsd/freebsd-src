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
  
/*	from OpenSolaris "t3.c	1.5	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t3.c	1.12 (gritter) 9/11/06
 */

 /* t3.c: interpret commands affecting whole table */
# include "t..c"
# include <string.h>
# include <stdlib.h>
struct optstr {char *optnam; int *optadd;} options [] = {
	{ "expand", &expflg },
	{ "EXPAND", &expflg },
	{ "center", &ctrflg },
	{ "CENTER", &ctrflg },
	{ "box", &boxflg },
	{ "BOX", &boxflg },
	{ "allbox", &allflg },
	{ "ALLBOX", &allflg },
	{ "doublebox", &dboxflg },
	{ "DOUBLEBOX", &dboxflg },
	{ "frame", &boxflg },
	{ "FRAME", &boxflg },
	{ "doubleframe", &dboxflg },
	{ "DOUBLEFRAME", &dboxflg },
	{ "tab", &tab },
	{ "TAB", &tab },
	{ "linesize", &linsize },
	{ "LINESIZE", &linsize },
	{ "decimalpoint", &decimalpoint },
	{ "DECIMALPOINT", &decimalpoint },
	{ "delim", &delim1 },
	{ "DELIM", &delim1 },
	{ "graphics", &graphics },
	{ "GRAPICS", &graphics },
	{ "nokeep", &nokeep },
	{ "NOKEEP", &nokeep },
	{ "left", NULL },
	{ NULL, NULL }
};

int
getcomm(void) {
	char *line = NULL, *cp, nb[25], *t;
	size_t linesize = 0;
	struct optstr *lp;
	int c, ci, found;
	for(lp= options; lp->optadd; lp++)
		*(lp->optadd) = 0;
	texname = texstr[texct=0];
	texct2 = -1;
	tab = '\t';
	decimalpoint = '.';
	if (pr1403) graphics = 0;
	else graphics = Graphics;
	printf(".nr %d \\n(.s\n", LSIZE);
	gets1(&line, &line, &linesize);
	/* see if this is a command line */
	if (strchr(line,';') == NULL) {
		backrest(line);
		free(line);
		return 0;
	}
	for(cp=line; (c = *cp) != ';'; cp++) {
		if (!letter(c)) continue;
		found=0;
		for(lp= options; lp->optnam; lp++) {
			if (prefix(lp->optnam, cp)) {
				cp += strlen(lp->optnam);
				if (letter(*cp))
					return
					    error("Misspelled global option");
				while (*cp==' ')cp++;
				t=nb;
				if ( *cp == '(')
					while ((ci= *++cp) != ')')
						*t++ = ci;
				else cp--;
				*t++ = 0; *t=0;
				if (!lp->optadd)
					goto found;
				*(lp->optadd) = 1;
				if (lp->optadd == &tab || lp->optadd ==
				    &decimalpoint) {
					if (nb[0])
						*(lp->optadd) = nb[0];
				}
				if (lp->optadd == &linsize)
					printf(".nr %d %s\n", LSIZE, nb);
				if (lp->optadd == &delim1) {
					delim1 = nb[0];
					delim2 = nb[1];
				}
found:
				found=1;
				break;
			}
		}
		if (!found)
			return error("Illegal option");
	}
	cp++;
	backrest(cp);
	free(line);
	return 0;
}

void
backrest(char *cp) {
	char *s;
	for(s=cp; *s; s++);
	un1getc('\n');
	while (s>cp)
		un1getc(*--s);
}
