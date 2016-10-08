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
  
/*	from OpenSolaris "tt.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tt.c	1.3 (gritter) 7/23/05
 */

 /* tt.c: subroutines for drawing horizontal lines */
# include "t..c"
# include <inttypes.h>
int 
ctype(int il, int ic)
{
	if (instead[il])
		return(0);
	if (fullbot[il])
		return(0);
	il = stynum[il];
	return(style[il][ic]);
}
int 
min(int a, int b)
{
return(a<b ? a : b);
}
int 
fspan(int i, int c)
{
c++;
return(c<ncol && ctype(i,c)=='s');
}
int 
lspan(int i, int c)
{
int k;
if (ctype(i,c) != 's') return(0);
c++;
if (c < ncol && ctype(i,c)== 's') 
	return(0);
for(k=0; ctype(i,--c) == 's'; k++);
return(k);
}
int 
ctspan(int i, int c)
{
int k;
c++;
for(k=1; c<ncol && ctype(i,c)=='s'; k++)
	c++;
return(k);
}
void
tohcol(int ic)
{
	if (ic==0)
		fprintf(tabout, "\\h'|0'");
	else
		fprintf(tabout, "\\h'(|\\n(%du+|\\n(%du)/2u'", ic+CLEFT, ic+CRIGHT-1);
}
int 
allh(int i)
{
	/* return true if every element in line i is horizontal */
	/* also at least one must be horizontl */
	int c, one, k;
	if (fullbot[i]) return(1);
	for(one=c=0; c<ncol; c++)
	{
		k = thish(i,c);
		if (k==0) return(0);
		if (k==1) continue;
		one=1;
	}
	return(one);
}
int 
thish(int i, int c)
{
	int t;
	char *s;
	struct colstr *pc;
	if (c<0)return(0);
	if (i<0) return(0);
	t = ctype(i,c);
	if (t=='_' || t == '-')
		return('-');
	if (t=='=')return('=');
	if (t=='^') return(1);
	if (fullbot[i] )
		return(fullbot[i]);
	if (t=='s') return(thish(i,c-1));
	if (t==0) return(1);
	pc = &table[i][c];
	s = (t=='a' ? pc->rcol : pc->col);
	if (s==0 || (point((intptr_t)s) && *s==0))
		return(1);
	if (vspen(s)) return(1);
	if ((t=barent( s)))
		return(t);
	return(0);
}
