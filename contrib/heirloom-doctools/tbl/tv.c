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
  
/*	from OpenSolaris "tv.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tv.c	1.4 (gritter) 9/8/06
 */

 /* tv.c: draw vertical lines */
# include "t..c"
# include <inttypes.h>
void
drawvert(int start, int end, int c, int lwid)
{
	char *exb=0, *ext=0;
	int tp=0, sl, ln, pos, epb, ept, vm;
	end++;
	vm='v';
	/* note: nr 35 has value of 1m outside of linesize */
	while (instead[end]) end++;
	for(ln=0; ln<lwid; ln++)
	{
		epb=ept=0;
		pos = 2*ln-lwid+1;
		if (pos!=tp) fprintf(tabout, "\\h'%dp'", pos-tp);
		tp = pos;
		if (end<nlin)
		{
			if (fullbot[end]|| (!instead[end] && allh(end)))
				epb=2;
			else
			switch (midbar(end,c))
			{
			case '-':
				exb = "1v-.5m"; break;
			case '=':
				exb = "1v-.5m";
				epb = 1; break;
			}
		}
		if (lwid>1)
		switch(interh(end, c))
		{
		case THRU: epb -= 1; break;
		case RIGHT: epb += (ln==0 ? 1 : -1); break;
		case LEFT: epb += (ln==1 ? 1 : -1); break;
		}
		if (lwid==1)
			switch(interh(end,c))
			{
			case THRU: epb -= 1; break;
			case RIGHT: case LEFT: epb += 1; break;
			}
		if (start>0)
		{
			sl = start-1;
			while (sl>=0 && instead[sl]) sl--;
			if (sl>=0 && (fullbot[sl] || allh(sl)))
				ept=0;
			else
				if (sl>=0)
					switch(midbar(sl,c))
					{
					case '-':
						ext = ".5m"; break;
					case '=':
						ext= ".5m"; ept = -1; break;
					default:
						vm = 'm'; break;
					}
				else
					ept = -4;
		}
		else if (start==0 && allh(0))
		{
			ept=0;
			vm = 'm';
		}
		if (lwid>1)
			switch(interh(start,c))
			{
			case THRU: ept += 1; break;
			case LEFT: ept += (ln==0 ? 1 : -1); break;
			case RIGHT: ept += (ln==1 ? 1 : -1); break;
			}
		else if (lwid==1)
			switch(interh(start,c))
			{
			case THRU: ept += 1; break;
			case LEFT: case RIGHT: ept -= 1; break;
			}
		if (exb)
			fprintf(tabout, "\\v'%s'", exb);
		if (epb)
			fprintf(tabout, "\\v'%dp'", epb);
		if (graphics)
			fprintf(tabout, "\\v'\\n(#Du'");
		else
			fprintf(tabout, "\\s\\n(%d",LSIZE);
		if (linsize)
			fprintf(tabout, "\\v'-\\n(%dp/6u'", LSIZE);
		fprintf(tabout, "\\h'-\\n(#~u'"); /* adjustment for T450 nroff boxes */
		if (graphics)
			fprintf(tabout, "\\D'l 0 |\\n(#%cu-%s", linestop[start]+'a'-1, vm=='v'? "1v" : "\\n(35u");
		else
			fprintf(tabout, "\\L'|\\n(#%cu-%s", linestop[start]+'a'-1, vm=='v'? "1v" : "\\n(35u");
		if (ext)
			fprintf(tabout, "-(%s)",ext);
		if (exb)
			fprintf(tabout, "-(%s)", exb);
		pos = ept-epb;
		if (pos)
			fprintf(tabout, "%s%dp", pos>=0? "+" : "", pos);
		/* the string #d is either "nl" or ".d" depending
		   on diversions; on GCOS not the same */
		fprintf(tabout, "'");
		if (graphics)
			fprintf(tabout, "\\v'-\\n(#Du'");
		else
			fprintf(tabout, "\\s0");
		fprintf(tabout, "\\v'\\n(\\*(#du-\\n(#%cu+%s", linestop[start]+'a'-1,vm=='v' ? "1v" : "\\n(35u");
		if (ext)
			fprintf(tabout, "+%s",ext);
		if (ept)
			fprintf(tabout, "%s%dp", (-ept)>0 ? "+" : "", (-ept));
		fprintf(tabout, "'");
		if (linsize)
			fprintf(tabout, "\\v'\\n(%dp/6u'", LSIZE);
	}
}


int 
midbar(int i, int c)
{
	int k;
	k = midbcol(i,c);
	if (k==0 && c>0)
		k = midbcol(i, c-1);
	return(k);
}
int 
midbcol(int i, int c)
{
	int ct;
	while ( (ct=ctype(i,c)) == 's')
		c--;
	if (ct=='-' || ct == '=')
		return(ct);
	if ((ct=barent(table[i][c].col)))
		return(ct);
	return(0);
}

int 
barent(char *s)
{
	if (s==0) return (1);
	if (!point((intptr_t)s)) return(1);
	if (s[0]== '\\') s++;
	if (s[1]!= 0)
		return(0);
	switch(s[0])
	{
	case '_':
		return('-');
	case '=':
		return('=');
	}
	return(0);
}
