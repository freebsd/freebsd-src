/*
 * Copyright (c) 1983-1998 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */
  
/*	from OpenSolaris "t7.c	1.5	05/06/02 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t7.c	1.8 (gritter) 10/25/06
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
     
 /* t7.c: control to write table entries */
# include "t..c"
# include <inttypes.h>
# define realsplit ((ct=='a'||ct=='n') && table[ldata][c].rcol)

int
runout(void)
{
	int i;
	if ((boxflg || allflg || dboxflg) && !nokeep) need();
	if (ctrflg)
	{
		fprintf(tabout, ".nr #I \\n(.i\n");
		fprintf(tabout, ".in +(\\n(.lu-\\n(TWu-\\n(.iu)/2u\n");
	}
	fprintf(tabout, ".fc %c %c\n", F1, F2);
	fprintf(tabout, ".nr #T 0-1\n");
	deftail();
	for(i=0; i<nlin; i++)
		putline(i,i);
	if (leftover)
		if (yetmore())
			return -1;
	fprintf(tabout, ".fc\n");
	fprintf(tabout, ".nr T. 1\n");
	fprintf(tabout, ".T# 1\n");
	if (ctrflg)
		fprintf(tabout, ".in \\n(#Iu\n");
	return 0;
}

void
runtabs(int lform, int ldata)
{
	int c, ct, vforml, lf;
	fprintf(tabout, ".ta ");
	for(c=0; c<ncol; c++)
		{
		vforml=lform;
		for (lf=prev(lform); lf>=0 && vspen(table[lf][c].col);
		    lf=prev(lf))
			vforml=lf;
		if (fspan(vforml,c))
			continue;
		switch(ct=ctype(vforml,c))
			{
			case 'n':
			case 'a':
				if (table[ldata][c].rcol)
				  if (lused[c]) /*Zero field width*/
					fprintf(tabout, "\\n(%du ",c+CMID);
			case 'c':
			case 'l':
			case 'r':
			    if (realsplit? rused[c]: (used[c]+lused[c]))
				fprintf(tabout, "\\n(%du ",c+CRIGHT);
				continue;
			case 's':
				if (lspan(lform, c))
					fprintf(tabout, "\\n(%du ", c+CRIGHT);
				continue;
			}
		}
	fprintf(tabout, "\n");
}

int
ifline(char *s) {
	if (!point((intptr_t)s) || !*s)
		return 0;
	if (*s == '\\')
		s++;
	if (s[1])
		return 0;
	if (*s == '_')
		return '-';
	if (*s == '=')
		return '=';
	return 0;
}

void
need(void)
{
	int texlin, horlin, i;

	for(texlin = horlin = i = 0; i < nlin; i++) {
		if (allh(i))
			horlin++;
		else if (instead[i] != 0)
			continue;
		else
			texlin++;
	}
	/*
	 * For nroff runs, we need to reserve space for the full height of the
	 * horizontal rules.  If we don't reserve sufficient height, we'll have
	 * problems trying to draw the vertical lines across the page boundary.
	 */
	(void) fprintf(tabout, ".ie n .ne %dv\n", 2 * texlin + 2 * horlin + 2);
	(void) fprintf(tabout, ".el .ne %dv\n", texlin + 1);
}

void
deftail(void)
{
	int i, c, lf, lwid;
	for(i=0; i<MAXHEAD; i++)
		if (linestop[i])
			fprintf(tabout, ".nr #%c 0-1\n", linestop[i]+'a'-1);
	fprintf(tabout, ".nr #a 0-1\n");
	fprintf(tabout, ".eo\n");
	fprintf(tabout, ".de T# 00\n");
	fprintf(tabout, ".ds #d .d\n");
	fprintf(tabout, ".if \\(ts\\n(.z\\(ts\\(ts .ds #d nl\n");
	fprintf(tabout, ".mk ##\n");
	fprintf(tabout, ".nr ## -1v\n");
	if (graphics)
		svgraph();
	fprintf(tabout, ".ls 1\n");
	for(i=0; i<MAXHEAD; i++)
		if (linestop[i])
			fprintf(tabout, ".if \\n(#T>=0 .nr #%c \\n(#T\n",linestop[i]+'a'-1);
	if (boxflg || allflg || dboxflg) { /* bottom of table line */
		if (!nflm)
			fprintf(tabout, ".if \\n(T. .vs \\n(.vu-\\n(.sp\n");
		fprintf(tabout, ".if \\n(T. ");
		drawline(nlin,0,ncol, dboxflg ? '=' : '-',1,0);
		fprintf(tabout, "\n");
		if (!nflm)
			fprintf(tabout, ".if \\n(T. .vs\n");
		/* T. is really an argument to a macro but because of 
		   eqn we don't dare pass it as an argument and reference by $1 */
	}
	if (!(utf8 || tlp)) {
		for(c=0; c<ncol; c++)
		{
			if (nlin>0 && (lf=left(nlin-1,c, &lwid))>=0)
			{
				fprintf(tabout, ".if \\n(#%c>=0 .sp -1\n",linestop[lf]+'a'-1);
				fprintf(tabout, ".if \\n(#%c>=0 ", linestop[lf]+'a'-1);
				tohcol(c);
				drawvert(lf, nlin-1, c, lwid);
				fprintf(tabout, "\\h'|\\n(TWu'\n");
			}
		}
		if (boxflg || allflg || dboxflg) /* right hand line */
		{
			fprintf(tabout, ".if \\n(#a>=0 .sp -1\n");
			fprintf(tabout, ".if \\n(#a>=0 \\h'|\\n(TWu'");
			drawvert (0, nlin-1, ncol, dboxflg? 2 : 1);
			fprintf(tabout, "\n");
		}
	}
	fprintf(tabout, ".ls\n");
	fprintf(tabout, ".00\n");
	fprintf(tabout, ".ec\n");
}
