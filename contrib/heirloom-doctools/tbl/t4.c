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
  
/*	from OpenSolaris "t4.c	1.10	05/06/02 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 * Portions Copyright (c) 2015 Carsten Kunze
 *
 * Sccsid @(#)t4.c	1.7 (gritter) 9/8/06
 */

 /* t4.c: read table specification */
# include "t..c"
# include <stdlib.h>
# include <string.h>
int oncol;
static int morecols(int);
static int moreheads(int);
static void initspec(int);
static void inithead(int, int);

int
getspec(void) {
	int i;
	moreheads(0);
	morecols(0);
	initspec(0);
	nclin=ncol=0;
	oncol =0;
	left1flg=rightl=0;
	if (readspec())
		return -1;
	fprintf(tabout, ".rm");
	for(i=0; i<ncol; i++)
		fprintf(tabout, " %02d", 80+i);
	fprintf(tabout, "\n");
	return 0;
}

int
readspec(void)
{
	int icol, c, sawchar, stopc, i;
	char sn[10], *snp, *temp;
	sawchar=icol=0;
	while ((c=get1char()))
	{
		switch(c)
		{
		default:
			if (c != tab)
				return error("bad table specification character");
		case ' ': /* note this is also case tab */
			continue;
		case '\n':
			if(sawchar==0) continue;
		case ',':
		case '.': /* end of table specification */
			ncol = max(ncol, icol);
			if (lefline[nclin][ncol]>0) {ncol++; rightl++;};
			if(sawchar)
				nclin++;
			if (nclin>=MAXHEAD && !moreheads(nclin))
				return error("too many lines in specification");
			icol=0;
			if (ncol==0 || nclin==0)
				return error("no specification");
			if (c== '.')
			{
				while ((c=get1char()) && c != '\n')
					if (c != ' ' && c != '\t')
						return error(
						    "dot not last character on format line");
				/* fix up sep - default is 3 except at edge */
				for(icol=0; icol<ncol; icol++)
					if (sep[icol]<0)
						sep[icol] =  icol+1<ncol ? 3 : 1;
				if (oncol == 0)
					oncol = ncol;
				else if (oncol +2 <ncol)
					return error("tried to widen table in T&, not allowed");
				return 0;
			}
			sawchar=0;
			continue;
		case 'C': case 'S': case 'R': case 'N': case 'L':  case 'A':
			c += ('a'-'A');
		case '_': if (c=='_') c= '-';
		case '=': case '-':
		case '^':
		case 'c': case 's': case 'n': case 'r': case 'l':  case 'a':
			style[nclin][icol]=c;
			if (c== 's' && icol<=0)
				return error("first column can not be S-type");
			if (c=='s' && style[nclin][icol-1] == 'a')
			{
				fprintf(tabout,
				    ".tm warning: can't span a-type cols, changed to l\n");
				style[nclin][icol-1] = 'l';
			}
			if (c=='s' && style[nclin][icol-1] == 'n')
			{
				fprintf(tabout,
				    ".tm warning: can't span n-type cols, changed to c\n");
				style[nclin][icol-1] = 'c';
			}
			icol++;
			if (c=='^' && nclin<=0)
				return error("first row can not contain vertical span");
			if (icol>=MAXCOL && !morecols(icol))
				return error("too many columns in table");
			sawchar=1;
			continue;
		case 'b': case 'i':
			c += 'A'-'a';
			/* FALLTHRU */
		case 'B': case 'I':
			if (sawchar == 0)
				continue;
			if (icol==0) continue;
			snp=font[nclin][icol-1];
			snp[0]= (c=='I' ? '2' : '3');
			snp[1]=0;
			continue;
		case 't': case 'T':
			if (sawchar == 0) {
				continue;
			}
			if (icol>0)
			ctop[nclin][icol-1] = 1;
			continue;
		case 'd': case 'D':
			if (sawchar == 0)
				continue;
			if (icol>0)
			ctop[nclin][icol-1] = -1;
			continue;
		case 'f': case 'F':
			if (sawchar == 0)
				continue;
			if (icol==0) continue;
			snp=font[nclin][icol-1];
			snp[0]=snp[1]=stopc=0;
			for(i=0; i<CLLEN; i++)
			{
				if (stopc==0 && i==2) break;
				do
					c = get1char();
				while (i==0 && c==' ');
				if (i==0 && c=='(')
				{
					stopc=')';
					c = get1char();
				}
				if (c==0) break;
				if (c==stopc) {stopc=0; break;}
				if (stopc==0)  if (c==' ' || c== tab ) break;
				if (c=='.'){un1getc(c); break;}
				if (c=='\n'){un1getc(c); break;}
				snp[i] = c;
				if (c>= '0' && c<= '9') break;
			}
			if (stopc) if (get1char()!=stopc)
				return error("Nonterminated font name");
			continue;
		case 'P': case 'p':
			if (sawchar == 0)
				continue;
			if (icol<=0) continue;
			temp = snp = csize[nclin][icol-1];
			while ((c = get1char()))
			{
				if (c== ' ' || c== tab || c=='\n') break;
				if (c=='-' || c == '+')
					if (snp>temp)
						break;
					else
						*snp++=c;
				else
				if (digit(c))
					*snp++ = c;
				else break;
				if (snp-temp>20)
					return error("point size too large");
			}
			*snp = 0;
			if (atoi(temp)>36)
				return error("point size unreasonable");
			un1getc (c);
			continue;
		case 'V': case 'v':
			if (sawchar == 0)
				continue;
			if (icol<=0) continue;
			temp = snp = vsize[nclin][icol-1];
			while ((c = get1char()))
			{
				if (c== ' ' || c== tab || c=='\n') break;
				if (c=='-' || c == '+')
					if (snp>temp)
						break;
					else
						*snp++=c;
				else
				if (digit(c))
					*snp++ = c;
				else break;
				if (snp-temp>20)
					return error(
					"vertical spacing value too large");
			}
			*snp=0;
			un1getc(c);
			continue;
		case 'w': case 'W':
			if (sawchar == 0) {
				/*
				 * This should be an error case.
				 * However, for the backward-compatibility,
				 * treat as if 'c' was specified.
				 */
				style[nclin][icol] = 'c';
				icol++;
				if (icol >= MAXCOL && !morecols(icol)) {
					return error("too many columns in table");
				}
				sawchar = 1;
			}

			snp = cll [icol-1];
			/* Dale Smith didn't like this check 
			 * possible to have two text blocks
			 *  of different widths now ....
			if (*snp)
				{
				fprintf(tabout, 
				"Ignored second width specification");
				continue;
				}
			* end commented out code ... */
			stopc=0;
			while ((c = get1char()))
			{
				if (snp==cll[icol-1] && c==' ')
					continue;
				if (snp==cll[icol-1] && c=='(')
				{
					stopc = ')';
					continue;
				}
				if ( !stopc && (c>'9' || c< '0'))
					break;
				if (stopc && c== stopc)
					break;
				if (snp-cll[icol-1]>CLLEN)
					return error ("column width too long");
				*snp++ =c;
			}
			*snp=0;
			if (!stopc)
				un1getc(c);
			continue;
		case 'x': case 'X':
			if (!sawchar || icol < 1) break;
			xcol[icol-1] = 1;
			xcolflg++;
			expflg = 0;
			break;
		case 'e': case 'E':
			if (sawchar == 0)
				continue;
			if (icol<1) continue;
			evenup[icol-1]=1;
			evenflg=1;
			continue;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9': 
			sn[0] = c;
			snp=sn+1;
			while (digit(*snp++ = c = get1char()))
				;
			un1getc(c);
			sep[icol-1] = max(sep[icol-1], numb(sn));
			continue;
		case '|':
			lefline[nclin][icol]++;
			if (icol==0) left1flg=1;
			continue;
		}
	}
	return error("EOF reading table specification");
}

static int
morecols(int n)
{
	int i, j, inc = 10, maxcol;
	void *vp;
	if (n < MAXCOL) return(1);
	while ((maxcol = MAXCOL + inc) < n) inc *= 2;
	for (i=0; i<MAXHEAD; i++)
	{
		if ((vp = realloc(style[i], maxcol * sizeof **style)) == NULL)
			return(0);
		style[i] = vp;
		if ((vp = realloc(ctop[i], maxcol * sizeof **ctop)) == NULL)
			return(0);
		ctop[i] = vp;
		if ((vp = realloc(font[i], maxcol * sizeof **font)) == NULL)
			return(0);
		font[i] = vp;
		if ((vp = realloc(csize[i], maxcol * sizeof **csize)) == NULL)
			return(0);
		csize[i] = vp;
		if ((vp = realloc(vsize[i], maxcol * sizeof **vsize)) == NULL)
			return(0);
		vsize[i] = vp;
		if ((vp = realloc(lefline[i], maxcol * sizeof **lefline)) == NULL)
			return(0);
		lefline[i] = vp;
		for (j=MAXCOL; j<maxcol; j++)
		{
			if ((font[i][j] = calloc(CLLEN, sizeof ***font)) == NULL)
				return(0);
			if ((csize[i][j] = calloc(20, sizeof ***csize)) == NULL)
				return(0);
			if ((vsize[i][j] = calloc(20, sizeof ***vsize)) == NULL)
				return(0);
		}
	}
	if ((vp = realloc(cll, maxcol * sizeof *cll)) == NULL)
		return(0);
	cll = vp;
	for (j=MAXCOL; j<maxcol; j++)
		if ((cll[j] = calloc(CLLEN, sizeof **cll)) == NULL)
			return(0);
	if ((vp = realloc(xcol, maxcol * sizeof(*xcol))) == NULL)
		return 0;
	xcol = vp;
	if ((vp = realloc(evenup, maxcol * sizeof *evenup)) == NULL)
		return(0);
	evenup = vp;
	if ((vp = realloc(sep, maxcol * sizeof *sep)) == NULL)
		return(0);
	sep = vp;
	if ((vp = realloc(used, maxcol * sizeof *used)) == NULL)
		return(0);
	used = vp;
	if ((vp = realloc(lused, maxcol * sizeof *lused)) == NULL)
		return(0);
	lused = vp;
	if ((vp = realloc(rused, maxcol * sizeof *rused)) == NULL)
		return(0);
	rused = vp;
	if ((vp = realloc(topat, maxcol * sizeof *topat)) == NULL)
		return(0);
	topat = vp;
	MAXCOL = maxcol;
	initspec(MAXCOL - inc);
	return(1);
}
static int
moreheads(int n)
{
	int i, j, inc = 10, maxhead;
	void *vp;
	if (n<MAXHEAD) return(1);
	while ((maxhead = MAXHEAD + inc) < n) inc *= 2;
	if ((vp = realloc(style, maxhead * sizeof *style)) == NULL)
		return(0);
	style = vp;
	if ((vp = realloc(ctop, maxhead * sizeof *ctop)) == NULL)
		return(0);
	ctop = vp;
	if ((vp = realloc(font, maxhead * sizeof *font)) == NULL)
		return(0);
	font = vp;
	if ((vp = realloc(csize, maxhead * sizeof *csize)) == NULL)
		return(0);
	csize = vp;
	if ((vp = realloc(vsize, maxhead * sizeof *vsize)) == NULL)
		return(0);
	vsize = vp;
	if ((vp = realloc(lefline, maxhead * sizeof *lefline)) == NULL)
		return(0);
	lefline = vp;
	if (MAXCOL == 0)
	{
		memset(style, 0, maxhead * sizeof *style);
		memset(ctop, 0, maxhead * sizeof *ctop);
		memset(font, 0, maxhead * sizeof *font);
		memset(csize, 0, maxhead * sizeof *csize);
		memset(vsize, 0, maxhead * sizeof *vsize);
		memset(lefline, 0, maxhead * sizeof *lefline);
	}
	if (MAXCOL) for (i=MAXHEAD; i<maxhead; i++)
	{
		if ((vp = calloc(MAXCOL, sizeof **style)) == NULL)
			return(0);
		style[i] = vp;
		if ((vp = calloc(MAXCOL, sizeof **ctop)) == NULL)
			return(0);
		ctop[i] = vp;
		if ((vp = calloc(MAXCOL, sizeof **font)) == NULL)
			return(0);
		font[i] = vp;
		if ((vp = calloc(MAXCOL, sizeof **csize)) == NULL)
			return(0);
		csize[i] = vp;
		if ((vp = calloc(MAXCOL, sizeof **vsize)) == NULL)
			return(0);
		vsize[i] = vp;
		if ((vp = calloc(MAXCOL, sizeof **lefline)) == NULL)
			return(0);
		lefline[i] = vp;
		for (j=0; j<MAXCOL; j++)
		{
			if ((vp = calloc(CLLEN, sizeof ***font)) == NULL)
				return(0);
			font[i][j] = vp;
			if ((vp = calloc(20, sizeof ***csize)) == NULL)
				return(0);
			csize[i][j] = vp;
			if ((vp = calloc(20, sizeof ***vsize)) == NULL)
				return(0);
			vsize[i][j] = vp;
		}
	}
	MAXHEAD = maxhead;
	for (j=0; j<MAXCOL; j++)
		inithead(MAXHEAD-inc, j);
	return(1);
}
static void
initspec(int scol)
{
	int icol;
	for(icol=scol; icol<MAXCOL; icol++)
	{
		sep[icol]= -1;
		evenup[icol]=0;
		cll[icol][0]=0;
		xcol[icol] = 0;
		inithead(0, icol);
	}
	xcolflg = 0;
}
static void
inithead(int shead, int icol)
{
int i;
for(i=shead; i<MAXHEAD; i++)
	{
	csize[i][icol][0]=0;
	vsize[i][icol][0]=0;
	font[i][icol][0] = lefline[i][icol] = 0;
	ctop[i][icol]=0;
	style[i][icol]= 'l';
	}
}
