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
  
/*	from OpenSolaris "tf.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tf.c	1.9 (gritter) 9/9/06
 */

 /* tf.c: save and restore fill mode around table */
# include "t..c"
void
savefill(void)
{
/* remembers various things: fill mode, vs, ps in mac 35 (SF) */
fprintf(tabout, ".de %d 00\n",SF);
fprintf(tabout, ".ps \\n(.s\n");
fprintf(tabout, ".vs \\n(.vu\n");
fprintf(tabout, ".in \\n(.iu\n");
fprintf(tabout, ".if \\n(.u .fi\n");
fprintf(tabout, ".if \\n(.j .ad\n");
fprintf(tabout, ".if \\n(.j=0 .na\n");
fprintf(tabout, ".00\n");
fprintf(tabout, ".nf\n");
/* set obx offset if useful */
fprintf(tabout, ".nr #~ 0\n");
fprintf(tabout, ".if n .nr #~ 0.6n\n");
}
void
rstofill(void)
{
fprintf(tabout, ".%d\n",SF);
}
void
endoff(void)
{
int i;
	warnoff();
	for(i=0; i<MAXHEAD; i++)
		if (linestop[i])
			fprintf(tabout, ".nr #%c 0\n", 'a'+i);
	for(i=0; i<texct; i++)
		fprintf(tabout, ".rm %c+\n",texstr[i]);
	for(i=300; i<=texct2; i++)
		fprintf(tabout, ".do rm %d+\n",i);
	warnon();
fprintf(tabout,".lf %d %s\n", iline - 1, ifile);
fprintf(tabout, "%s\n", last);
}
void
ifdivert(void)
{
fprintf(tabout, ".ds #d .d\n");
fprintf(tabout, ".if \\(ts\\n(.z\\(ts\\(ts .ds #d nl\n");
}
void
saveline(void)
{
fprintf(tabout,".lf 2 table-at-line-%d-of-%s\n", iline-1, ifile);
fprintf(tabout, ".de 00\n..\n");
fprintf(tabout, ".do nr w. \\n[.warn]\n");
warnoff();
fprintf(tabout, ".if \\n+(b.=1 .nr d. \\n(.c-\\n(c.-1\n");
warnon();
linstart=iline;
}
void
restline(void)
{
warnoff();
fprintf(tabout,".if \\n-(b.=0 .nr c. \\n(.c-\\n(d.-%d\n", iline-linstart);
warnon();
fprintf(tabout,".lf %d %s\n", iline, ifile);
linstart = 0;
}
void
cleanfc(void)
{
fprintf(tabout, ".fc\n");
}
void
warnoff(void)
{
fprintf(tabout, ".if \\n(.X>0 .do warn -mac -reg\n");
}
void
warnon(void)
{
fprintf(tabout, ".if \\n(.X>0 .do warn \\n(w.\n");
}
void
svgraph(void)
{
fprintf(tabout, ".nr #D .2m\n");
}
