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
  
/*	from OpenSolaris "ti.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)ti.c	1.3 (gritter) 7/23/05
 */

 /* ti.c: classify line intersections */
# include "t..c"
/* determine local environment for intersections */
int 
interv(int i, int c)
{
int ku, kl;
if (c>=ncol || c == 0)
	{
	if (dboxflg)
		{
		if (i==0) return(BOT);
		if (i>=nlin) return(TOP);
		return(THRU);
		}
	if (c>=ncol)
		return(0);
	}
ku = i>0 ? lefdata(i-1,c) : 0;
if (i+1 >= nlin)
	kl=0;
else
kl = lefdata(allh(i) ? i+1 : i, c);
if (ku==2 && kl==2) return(THRU);
if (ku ==2) return(TOP);
if (kl==BOT) return(2);
return(0);
}
int 
interh(int i, int c)
{
int kl, kr;
if (fullbot[i]== '=' || (dboxflg && (i==0 || i>= nlin-1)))
	{
	if (c==ncol)
		return(LEFT);
	if (c==0)
		return(RIGHT);
	return(THRU);
	}
if (i>=nlin) return(0);
kl = c>0 ? thish (i,c-1) : 0;
if (kl<=1 && i>0 && allh(up1(i)))
	kl = c>0 ? thish(up1(i),c-1) : 0;
kr = thish(i,c);
if (kr<=1 && i>0 && allh(up1(i)))
	kr = c>0 ? thish(up1(i), c) : 0;
if (kl== '=' && kr ==  '=') return(THRU);
if (kl== '=') return(LEFT);
if (kr== '=') return(RIGHT);
return(0);
}
int 
up1(int i)
{
i--;
while (instead[i] && i>0) i--;
return(i);
}
