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
  
/*	from OpenSolaris "t2.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t2.c	1.3 (gritter) 7/23/05
 */

 /* t2.c:  subroutine sequencing for one table */
# include "t..c"
void
tableput(void) {
	saveline();
	savefill();
	ifdivert();
	cleanfc();
	if (getcomm())
		return;
	if (getspec())
		return;
	if (gettbl())
		return;
	getstop();
	checkuse();
	if (choochar())
		return;
	maktab();
	if (runout())
		return;
	release();
	rstofill();
	endoff();
	restline();
}
