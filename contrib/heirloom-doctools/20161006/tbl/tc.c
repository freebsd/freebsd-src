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
  
/*	from OpenSolaris "tc.c	1.5	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)tc.c	1.5 (gritter) 10/15/05
 */

 /* tc.c: find character not in table to delimit fields */
# include "t..c"
# include <inttypes.h>

int
choochar(void) {
	/* choose funny characters to delimit fields */
	int had[128], ilin, icol, k;
	char *s;
	for(icol=0; icol<128; icol++)
		had[icol]=0;
	F1 = F2 = 0;
	for(ilin=0;ilin<nlin;ilin++) {
		if (instead[ilin]) continue;
		if (fullbot[ilin]) continue;
		for(icol=0; icol<ncol; icol++) {
			k = ctype(ilin, icol);
			if (k==0 || k == '-' || k == '=')
				continue;
			s = table[ilin][icol].col;
			if (point((intptr_t)s))
				while (*s) {
					if (*s > 0 && *(unsigned char *)s <= 127)
						had[(int)*s++]=1;
					else
						s++;
				}
			s=table[ilin][icol].rcol;
			if (point((intptr_t)s))
				while (*s) {
					if (*s > 0 && *(unsigned char *)s <= 127)
						had[(int)*s++]=1;
					else
						s++;
				}
		}
	}
	/* choose first funny character */
	for(s="\002\003\005\006\007!%&#/?,:;<=>@`^~_{}+-*ABCDEFGHIJKMNOPQRSTUVWXYZabcdefgjkoqrstwxyz";
	    *s; s++) {
		if (had[(int)*s]==0) {
			F1= *s;
			had[F1]=1;
			break;
		}
	}
	/* choose second funny character */
	for(s="\002\003\005\006\007:_~^`@;,<=>#%&!/?{}+-*ABCDEFGHIJKMNOPQRSTUVWXZabcdefgjkoqrstuwxyz";
	    *s; s++) {
		if (had[(int)*s]==0) {
			F2= *s;
			break;
		}
	}
	if (F1==0 || F2==0)
		return error(
		    "couldn't find characters to use for delimiters");
	return 0;
}

int 
point(int s) {
	return s >= 4096 || s < 0;
}
