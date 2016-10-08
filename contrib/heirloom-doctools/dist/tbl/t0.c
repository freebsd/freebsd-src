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
  
/*	from OpenSolaris "t0.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t0.c	1.12 (gritter) 9/11/06
 */

 /* t0.c: storage allocation */
# include "t..c"
int MAXLIN;
int MAXCOL;
int MAXHEAD;
int expflg = 0;
int xcolflg;
int ctrflg = 0;
int boxflg = 0;
int dboxflg = 0;
int decimalpoint = '.';
int tab = '\t';
int linsize;
int pr1403;
int graphics;
int Graphics;
int delim1, delim2;
int *evenup, evenflg;	/* evenup[MAXCOL] */
int F1 = 0;
int F2 = 0;
int allflg = 0;
char *leftover = 0;
int textflg = 0;
int left1flg = 0;
int rightl = 0;
char *cstore, *cspace, *cbase;
char *last;
struct colstr **table;	/* *table[MAXLIN] */
int **style;	/* style[MAXHEAD][MAXCOL] */
int **ctop;	/* ctop[MAXHEAD][MAXCOL] */
char ***font;	/* font[MAXHEAD][MAXCOL][CLLEN] */
char ***csize;	/* csize[MAXHEAD][MAXCOL][20] */
char ***vsize;	/* vsize[MAXHEAD][MAXCOL][20] */
int **lefline;	/* lefline[MAXHEAD][MAXCOL] */
char **cll;	/* cll[MAXCOL][CLLEN] */
int *xcol;
/*char *rpt[MAXHEAD][MAXCOL];*/
/*char rpttx[MAXRPT];*/
int *stynum;	/* stynum[MAXLIN+1] */
int nslin, nclin;
int *sep;	/* sep[MAXCOL] */
int *fullbot;	/* fullbot[MAXLIN] */
char **instead;	/* *instead[MAXLIN] */
int *used, *lused, *rused;	/* MAXCOL */
int *linestop;	/* linestop[MAXLIN] */
int *topat;	/* topat[MAXLIN] */
int nlin, ncol;
int iline = 1;
char *ifile = "Input";
intptr_t texname = 'a';
int texct = 0;
int texct2 = -1;
char texstr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWYXZ0123456789";
int linstart;
int nokeep;
char *exstore, *exlim;
const char *progname;
FILE *tabin  /*= stdin */;
FILE *tabout  /* = stdout */;
int utf8;
int tlp;
int nflm;
