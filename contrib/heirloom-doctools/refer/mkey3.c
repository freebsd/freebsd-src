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

/*	from OpenSolaris "mkey3.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)mkey3.c	1.3 (gritter) 10/22/05
 */

#include <stdio.h>
#include <string.h>
#include "refer..c"
#define COMNUM 500
#define COMTSIZE 997

char *comname = REFDIR "/eign";
static int cgate = 0;
extern char *comname;
int comcount = 100;
static char cbuf[COMNUM*9];
static char *cwds[COMTSIZE];
static char *cbp;

int
common (char *s)
{
	if (cgate==0) cominit();
	return (c_look(s, 1));
}

void
cominit(void)
{
	int i;
	FILE *f;
	cgate=1;
	f = fopen(comname, "r");
	if (f==NULL) return;
	cbp=cbuf;
	for(i=0; i<comcount; i++)
	{
		if (fgets(cbp, 15, f)==NULL)
			break;
		trimnl(cbp);
		c_look (cbp, 0);
		while (*cbp++);
	}
	fclose(f);
}

int
c_look (char *s, int fl)
{
	int h;
	h = hash(s) % (COMTSIZE);
	while (cwds[h] != 0)
	{
		if (strcmp(s, cwds[h])==0)
			return(1);
		h = (h+1) % (COMTSIZE);
	}
	if (fl==0)
		cwds[h] = s;
	return(0);
}
