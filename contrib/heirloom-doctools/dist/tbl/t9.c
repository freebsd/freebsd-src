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

/*	from OpenSolaris "t9.c	1.6	05/06/02 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t9.c	1.7 (gritter) 2/26/06
 */

 /* t9.c: write lines for tables over MAXLIN lines */
# include "t..c"

static int useln;

int
yetmore(void) {
	size_t linesize = MAXSTR;
	int i;
	for(useln=0; useln<MAXLIN && table[useln]==0; useln++);
	if (useln>=MAXLIN)
		return error("Weird.  No data in table.");
	table[0]=table[useln];
	for(useln=nlin-1; useln>=0 && (fullbot[useln] || instead[useln]);
	    useln--);
	if (useln<0)
		return error("Weird.  No real lines in table.");
	if (domore(leftover) == -1)
		return -1;
	while (gets1(&cbase, &cspace, &linesize) && (cstore=cspace) &&
	    (i = domore(cstore)))
		if (i == -1)
			return -1;
	last =cstore;
	return 0;
}

int 
domore(char *dataln) {
	int icol, ch;
	if (cprefix("TE", dataln))
		return(0);
	if (dataln[0] == '.' && !isdigit((unsigned char)dataln[1])) {
		puts(dataln);
		return(1);
	}
	instead[0]=0;
	fullbot[0]=0;
	if (dataln[1]==0)
	switch(dataln[0]) {
	case '_': fullbot[0]= '-'; putline(useln,0);  return(1);
	case '=': fullbot[0]= '='; putline(useln, 0); return(1);
	}
	for (icol = 0; icol <ncol; icol++) {
		table[0][icol].col = dataln;
		table[0][icol].rcol=0;
		for(; (ch= *dataln) != '\0' && ch != tab; dataln++)
			;
		*dataln++ = '\0';
		switch(ctype(useln,icol)) {
		case 'n':
			if ((table[0][icol].rcol = maknew(table[0][icol].col))
			    == (char *)-1)
				return -1;
			break;
		case 'a':
			table[0][icol].rcol = table[0][icol].col;
			table[0][icol].col= "";
			break;
		}
		while (ctype(useln,icol+1)== 's') /* spanning */
			table[0][++icol].col = "";
		if (ch == '\0') break;
	}
	while (++icol <ncol)
		table[0][icol].col = "";
	putline(useln,0);
	return(1);
}
