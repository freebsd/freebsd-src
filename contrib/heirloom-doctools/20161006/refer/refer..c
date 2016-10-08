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

/*	from OpenSolaris "refer..c	1.3	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)refer..c	1.5 (gritter) 12/25/06
 */

#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "global.h"
#ifndef	EUC
#undef	getw
#define	getw(f)		getc(f)
#undef	putw
#define	putw(c, f)	putc(c, f)
#endif	/* !EUC */

#define FLAG 003
#define AFLAG 007
#define NRFTXT 10000
#define NRFTBL 500
#define NTFILE 20
#define QLEN 512
#define ANSLEN 4000
#define TAGLEN 400
#define NSERCH 20
#define MXSIG 200		/* max bytes in aggregate signal */

extern FILE *in;
extern int endpush, sort, labels, keywant, bare;
extern int biblio, science, postpunct;
extern char *smallcaps;
extern char *comname;
extern char *keystr;
extern char *convert;
extern int authrev;
extern int nmlen, dtlen;
extern char *rdata[], **search;
extern int refnum;
extern char *reftable[];
extern char *rtp, reftext[];
extern int sep;
extern char tfile[];
extern char gfile[];
extern char ofile[];
extern char hidenam[];
extern char *Ifile; extern int Iline;
extern FILE *fo, *ftemp;

/* deliv2.c */
int hash(const char *);
void err(const char *, ...);
int prefix(const char *, const char *);
char *mindex(const char *, int);
void *zalloc(int, int);
/* glue1.c */
void huntmain(int, char **);
char *todir(char *);
int setfrom(int);
/* glue2.c */
void savedir(void);
void restodir(void);
/* glue3.c */
int corout(char *, char *, char *, char *, int);
int callhunt(char *, char *, char *, int);
int dodeliv(char *, char *, char *, int);
/* glue4.c */
int grepcall(char *, char *, char *);
void clfgrep(void);
/* glue5.c */
int fgrep(int, char **);
/* hunt1.c */
char *todir(char *);
int setfrom(int);
/* hunt2.c */
int doquery(long *, int, FILE *, int, char **, unsigned *);
long getl(FILE *);
void putl(long, FILE *);
int hcomp(int, int);
int hexch(int, int);
/* hunt3.c */
int getq(char **);
/* hunt5.c */
void result(unsigned *, int, FILE *);
long gdate(FILE *);
/* hunt6.c */
int baddrop(unsigned *, int, FILE *, int, char **, char *, int);
int auxil(char *, char *);
/* hunt7.c */
int findline(char *, char **, int, long);
/* hunt8.c */
void runbib(const char *);
int makefgrep(char *);
int ckexist(const char *, const char *);
FILE *iopen(const char *, const char *);
/* hunt9.c */
void remote(const char *, const char *);
/* inv2.c */
int newkeys(FILE *, FILE *, FILE *, int, FILE *, int *);
char *trimnl(char *);
/* inv3.c */
int getargs(char *, char **);
/* inv5.c */
int recopy(FILE *, FILE *, FILE *, int);
/* inv6.c */
void whash(FILE *, FILE *, FILE *, int, int, long *, int *);
void putl(long, FILE *);
long getl(FILE *);
/* mkey2.c */
void dofile(FILE *, char *);
int outkey(char *, int, int);
long grec(char *, FILE *);
char *trimnl(char *);
void chkey(int, char *);
/* mkey3.c */
int common(char *);
void cominit(void);
int c_look(char *, int);
/* refer2.c */
void doref(char *);
int newline(const char *);
void choices(char *);
int control(int);
/* refer3.c */
int corout(char *, char *, char *, char *, int);
/* refer4.c */
void output(const char *);
void append(char *);
void flout(void);
char *trimnl(char *);
/* refer5.c */
void putsig(int, char **, int, char *, char *, int);
char *fpar(int, char **, char *, size_t, int, int, int);
void putkey(int, char **, int, char *);
void tokeytab(const char *, int);
int keylet(char *, int);
void mycpy(char *, const char *);
void mycpy2(char *, const char *, int);
void initadd(char *, const char *, const char *);
char *artskp(char *);
/* refer6.c */
void putref(int, char **);
int tabs(char **, char *);
char *class(int, char **);
int hastype(int, char **, int);
char *caps(char *, char *);
char *revauth(char *, char *);
int last(const char *);
/* refer7.c */
int chkdup(const char *);
void dumpold(void);
void recopy1(char *);
void condense(int *, int, char *);
int wswap(const void *, const void *);
/* refer8.c */
char *input(char *, size_t);
char *lookat(void);
void addch(char *, int);
/* shell.c */
void shell(int, int (*)(int, int), int (*)(int, int));
/* tick.c */
void tick(void);
void tock(void);
