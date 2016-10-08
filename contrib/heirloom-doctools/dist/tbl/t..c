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
  
/*	from OpenSolaris "t..c	1.4	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t..c	1.19 (gritter) 9/11/06
 */

/* t..c : external declarations */

# include <stdio.h>
# include <ctype.h>
# include <inttypes.h>

# if defined (__GLIBC__) && defined (_IO_getc_unlocked)
# undef getc
# define getc(f) _IO_getc_unlocked(f)
# endif

# define MAXCHS 2000
# define MAXSTR 1024
# define MAXRPT 100
# define CLLEN 100
# define SHORTLINE 4
# define BIGBUF 8192
extern int MAXLIN;
extern int MAXCOL;
extern int MAXHEAD;
extern int nlin, ncol, iline, nclin, nslin;
extern int **style;
extern int **ctop;
extern char ***font;
extern char ***csize;
extern char ***vsize;
extern char **cll;
extern int *xcol;
extern int *stynum;
extern int F1, F2;
extern int **lefline;
extern int *fullbot;
extern char **instead;
extern int expflg;
extern int xcolflg;
extern int ctrflg;
extern int evenflg;
extern int *evenup;
extern int boxflg;
extern int dboxflg;
extern int decimalpoint;
extern int linsize;
extern int tab;
extern int pr1403;
extern int graphics;
extern int Graphics;
extern int linsize, delim1, delim2;
extern int allflg;
extern int textflg;
extern int left1flg;
extern int rightl;
struct colstr {char *col, *rcol;};
extern struct colstr **table;
extern char *cspace, *cstore, *cbase;
extern char *exstore, *exlim;
extern int *sep;
extern int *used, *lused, *rused;
extern int *linestop;
extern char *leftover;
extern char *last, *ifile;
extern int *topat;
extern intptr_t texname;
extern int texct;
extern int texct2;
extern char texstr[];
extern int linstart;
extern int nokeep;

extern const char *progname;
extern int utf8;
extern int tlp;
extern int nflm;

extern FILE *tabin, *tabout;
# define CRIGHT 80
# define CLEFT 40
# define CMID 60
# define S1 31
# define S2 32
# define TMP 38
# define SF 35
# define SL 34
# define LSIZE 33
# define SIND 37
# define SVS 36
/* this refers to the relative position of lines */
# define LEFT 1
# define RIGHT 2
# define THRU 3
# define TOP 1
# define BOT 2

/* t1.c */
int tbl(int, char *[]);
void setinp(int, char **);
int swapin(void);
/* t2.c */
void tableput(void);
/* t3.c */
int getcomm(void);
void backrest(char *);
/* t4.c */
int getspec(void);
int readspec(void);
/* t5.c */
int gettbl(void);
int nodata(int);
int oneh(int);
int permute(void);
int vspand(int, int, int);
int vspen(char *);
/* t6.c */
void maktab(void);
void wide(char *, char *, char *);
int filler(char *);
/* t7.c */
int runout(void);
void runtabs(int, int);
int ifline(char *);
void need(void);
void deftail(void);
/* t8.c */
void putline(int, int);
void puttext(char *, char *, char *);
void funnies(int, int);
void putfont(char *);
void putsize(char *);
/* t9.c */
int yetmore(void);
int domore(char *);
/* tb.c */
void checkuse(void);
int real(char *);
char *chspace(void);
void updspace(char *, char *, int);
struct colstr *alocv(int);
void release(void);
/* tc.c */
int choochar(void);
int point(int);
/* te.c */
int error(char *);
char *errmsg(int);
char *gets1(char **, char **, size_t *);
void un1getc(int);
int get1char(void);
/* tf.c */
void savefill(void);
void rstofill(void);
void endoff(void);
void ifdivert(void);
void saveline(void);
void restline(void);
void cleanfc(void);
void warnon(void);
void warnoff(void);
void svgraph(void);
/* tg.c */
char *get_text(char *, int, int, char *, char *);
void untext(void);
char *nreg(char *, size_t, const char *, int);
/* ti.c */
int interv(int, int);
int interh(int, int);
int up1(int);
/* tm.c */
char *maknew(char *);
int ineqn(char *, char *);
/* ts.c */
int match(char *, char *);
int prefix(char *, char *);
int cprefix(char *, char *);
int letter(int);
int numb(char *);
int digit(int);
int max(int, int);
void tcopy(char *, char *);
/* tt.c */
int ctype(int, int);
int min(int, int);
int fspan(int, int);
int lspan(int, int);
int ctspan(int, int);
void tohcol(int);
int allh(int);
int thish(int, int);
/* tu.c */
void makeline(int, int, int);
void fullwide(int, int);
void drawline(int, int, int, int, int, int);
void getstop(void);
int left(int, int, int *);
int lefdata(int, int);
int next(int);
int prev(int);
/* tv.c */
void drawvert(int, int, int, int);
int midbar(int, int);
int midbcol(int, int);
int barent(char *);
