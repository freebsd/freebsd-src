/*
 * Copyright 1983-2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*	from OpenSolaris "t5.c	1.6	05/06/02 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t5.c	1.8 (gritter) 10/2/07
 */

 /* t5.c: read data for table */
# include <stdlib.h>
# include <string.h>
# include "t..c"
# include <inttypes.h>

static int morelines(int);

int
gettbl(void)
{
	int icol, ch;
	char *ocbase;
	size_t linesize = MAXSTR;
	morelines(0);
	if (!(cbase = cstore = cspace = chspace()))
		return -1;
	textflg=0;
	for (nlin=nslin=0; ocbase=cbase, gets1(&cbase, &cstore, &linesize); nlin++)
	{
		cspace += cbase - ocbase;
		stynum[nlin]=nslin;
		if (cprefix("TE", cstore))
		{
			leftover=0;
			break;
		}
		if (cprefix("TC", cstore) || cprefix("T&", cstore))
		{
			if (readspec())
				return -1;
			nslin++;
		}
		if (nlin>=MAXLIN && !morelines(nlin))
		{
			leftover=cstore;
			break;
		}
		table[nlin] = NULL;
		fullbot[nlin]=0;
		if (cstore[0] == '.' && !isdigit((unsigned char)cstore[1]))
		{
			instead[nlin] = cstore;
			while (*cstore++);
			continue;
		}
		else instead[nlin] = 0;
		if (nodata(nlin))
		{
			if ((ch = oneh(nlin)))
				fullbot[nlin]= ch;
			nlin++;
			nslin++;
			instead[nlin]=(char *)0;
			fullbot[nlin]=0;
		}
		if ((table[nlin] = alocv((ncol+2)*sizeof(table[0][0])))
		    == (struct colstr *)-1)
			return -1;
		if (cstore[1]==0)
			switch(cstore[0])
			{
			case '_': fullbot[nlin]= '-'; continue;
			case '=': fullbot[nlin]= '='; continue;
			}
		stynum[nlin] = nslin;
		nslin = min(nslin+1, nclin-1);
		for (icol = 0; icol <ncol; icol++)
		{
			table[nlin][icol].col = cstore;
			table[nlin][icol].rcol=0;
			ch=1;
			if (match(cstore, "T{")) { /* text follows */
				/* get_text was originally gettext and was renamed */
				if ((table[nlin][icol].col =
				    get_text(cstore, nlin, icol,
				    font[stynum[nlin]][icol],
				    csize[stynum[nlin]][icol])) == (char *)-1)
					return -1;
			} else {
				for(; (ch= *cstore) != '\0' && ch != tab; cstore++)
					;
				*cstore++ = '\0';
				switch(ctype(nlin,icol)) /* numerical or alpha, subcol */
				{
				case 'n':
					if ((table[nlin][icol].rcol =
					    maknew(table[nlin][icol].col))
					    == (char *)-1)
						return -1;
					break;
				case 'a':
					table[nlin][icol].rcol = table[nlin][icol].col;
					table[nlin][icol].col = "";
					break;
				}
			}
			while (ctype(nlin,icol+1)== 's') /* spanning */
				table[nlin][++icol].col = "";
			if (ch == '\0') break;
		}
		while (++icol <ncol+2)
		{
			table[nlin][icol].col = "";
			table [nlin][icol].rcol=0;
		}
		while (*cstore != '\0')
			cstore++;
		if (cstore-cspace > MAXCHS)
			if (!(cbase = cstore = cspace = chspace()))
				return -1;
	}
	last = cstore;
	if (permute())
		return -1;
	if (textflg) untext();
	return 0;
}

int 
nodata(int il)
{
	int c;
	for (c=0; c<ncol;c++)
	{
		switch(ctype(il,c))
		{
		case 'c': case 'n': case 'r': case 'l': case 's': case 'a':
			return(0);
		}
	}
	return(1);
}
int 
oneh(int lin)
{
	int k, icol;
	k = ctype(lin,0);
	for(icol=1; icol<ncol; icol++)
	{
		if (k != ctype(lin,icol))
			return(0);
	}
	return(k);
}

# define SPAN "\\^"

int
permute(void)
{
	int irow, jcol, is;
	char *start, *strig;
	for(jcol=0; jcol<ncol; jcol++)
	{
		for(irow=1; irow<nlin; irow++)
		{
			if (vspand(irow,jcol,0))
			{
				is = prev(irow);
				if (is<0)
					return error("Vertical spanning in "
					    "first row not allowed");
				start = table[is][jcol].col;
				strig = table[is][jcol].rcol;
				while (irow<nlin &&vspand(irow,jcol,0))
					irow++;
				table[--irow][jcol].col = start;
				table[irow][jcol].rcol = strig;
				while (is<irow)
				{
					table[is][jcol].rcol =0;
					table[is][jcol].col= SPAN;
					is = next(is);
				}
			}
		}
	}
	return 0;
}

int 
vspand(int ir, int ij, int ifform)
{
	if (ir<0) return(0);
	if (ir>=nlin)return(0);
	if (instead[ir]) return(0);
	if (ifform==0 && ctype(ir,ij)=='^') return(1);
	if (table[ir]==0) return(0);
	if (table[ir][ij].rcol!=0) return(0);
	if (fullbot[ir]) return(0);
	return(vspen(table[ir][ij].col));
}
int 
vspen(char *s)
{
	if (s==0) return(0);
	if (!point((intptr_t)s)) return(0);
	return(match(s, SPAN));
}
static int
morelines(int n)
{
	int inc = 200, maxlin;
	void *vp;
	if (n>MAXLIN) return(1);
	while ((maxlin = MAXLIN + inc) < n) inc *= 2;
	if ((vp = realloc(table, maxlin * sizeof *table)) == NULL)
		return(0);
	table = vp;
	if ((vp = realloc(stynum, (maxlin + 1) * sizeof *stynum)) == NULL)
		return(0);
	stynum = vp;
	if ((vp = realloc(fullbot, maxlin * sizeof *fullbot)) == NULL)
		return(0);
	fullbot = vp;
	memset(&fullbot[MAXLIN], 0, inc * sizeof *fullbot);
	if ((vp = realloc(instead, maxlin * sizeof *instead)) == NULL)
		return(0);
	instead = vp;
	memset(&instead[MAXLIN], 0, inc * sizeof *instead);
	if ((vp = realloc(linestop, maxlin * sizeof *linestop)) == NULL)
		return(0);
	linestop = vp;
	MAXLIN = maxlin;
	return(1);
}
