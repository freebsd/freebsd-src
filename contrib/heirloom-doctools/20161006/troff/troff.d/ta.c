/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "ta.c	1.10	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 */
#if __GNUC__ >= 3 && __GNUC_MINOR__ >= 4 || __GNUC__ >= 4
#define	USED	__attribute__ ((used))
#elif defined __GNUC__
#define	USED	__attribute__ ((unused))
#else
#define	USED
#endif
static const char sccsid[] USED = "@(#)/usr/ucb/ta.sl	1.8 (gritter) 12/25/06";

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

/*
 *	drive hp2621 terminal 
 *	just to see stuff quickly. like troff -a
 */

/*
output language from troff:
all numbers are character strings

sn	size in points
fn	font as number from 1-n
cx	ascii character x
Cxyz	funny char xyz. terminated by white space
Hn	go to absolute horizontal position n
Vn	go to absolute vertical position n (down is positive)
hn	go n units horizontally (relative)
vn	ditto vertically
nnc	move right nn (exactly 2 digits!), then print c
		(this wart is an optimization that shrinks output file size
		 about 35% and run-time about 15% while preserving ascii-ness)
w	paddable word space - no action needed
nb a	end of line (information only -- no action needed)
	b = space before line, a = after
pn	begin page n
#...\n	comment
Dt ...\n	draw operation 't':
	Dl x y		line from here by x,y
	Dc d		circle of diameter d with left side here
	De x y		ellipse of axes x,y with left side here
	Da x y u v	arc counter-clockwise from here to u,v from center
				with center x,y from here
	D~ x y x y ...	wiggly line by x,y then x,y ...
x ...\n	device control functions:
	x i	init
	x T s	name of device is s
	x r n h v	resolution is n/inch
		h = min horizontal motion, v = min vert
	x p	pause (can restart)
	x s	stop -- done for ever
	x t	generate trailer
	x f n s	font position n contains font s
	x H n	set character height to n
	x S n	set character slant to n

	Subcommands like "i" are often spelled out like "init".
*/

#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<sys/wait.h>
#include	<stdio.h>
#include	<signal.h>
#include	<ctype.h>
#include	<stdarg.h>

#include "dev.h"
#define	NFONT	10

int	output	= 0;	/* do we do output at all? */
int	nolist	= 0;	/* output page list if > 0 */
int	olist[20];	/* pairs of page numbers */

int	erase	= 1;
float	aspect	= 1.5;	/* default aspect ratio */
int	wflag	= 0;	/* wait, looping, for new input if on */
void	(*sigint)(int);
void	(*sigquit)(int);
void	done(void);

struct dev dev;
struct font *fontbase[NFONT];
short	psizes[]	={ 11, 16, 22, 36, 0};	/* approx sizes available */
short	*pstab		= psizes;
int	nsizes	= 1;
int	nfonts;
int	smnt;	/* index of first special font */
int	nchtab;
char	*chname;
short	*chtab;
char	*fitab[NFONT];
char	*widthtab[NFONT];	/* widtab would be a better name */
char	*codetab[NFONT];	/* device codes */

#define	FATAL	1
#define	BMASK	0377
int	dbg	= 0;
int	res	= 972;		/* input assumed computed according to this resolution */
				/* initial value to avoid 0 divide */
FILE	*tf;			/* output file */
char	*fontdir	= FNTDIR;
#define	devname	troff_devname
char	devname[20]	= "hp2621";

FILE *fp;		/* input file pointer */

int	nowait = 0;	/* 0 => wait at bottom of each page */


void drawline(int, int, char *);
void drawwig(char *);
void drawcirc(int);
void drawarc(int, int, int, int);
void drawellip(int, int);

void outlist(char *);
int in_olist(int);
void conv(register FILE *);
void devcntrl(FILE *);
void fileinit(void);
void fontprint(int);
void loadcode(int, int);
void loadfont(int, char *);
void error(int, const char *, ...);
void t_init(int);
void t_push(void);
void t_pop(void);
void t_page(int);
void putpage(void);
void t_newline(void);
int t_size(int);
int t_font(char *);
void t_text(char *);
void t_reset(int);
void t_trailer(void);
void hgoto(int);
void hmot(int);
void hflush(void);
void vgoto(int);
void vmot(int);
void put1s(char *);
void put1(int);
void setsize(double);
void t_fp(int, char *);
void setfont(int);
void done(void);
void callunix(char []);
int readch(void);

static int sget(char *, size_t, FILE *);

int
main(int argc, char **argv)
{
	char buf[BUFSIZ];

	fp = stdin;
	tf = stdout;
	setbuf(stdout, buf);
	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 'a':
			aspect = atof(&argv[1][2]);
			break;
		case 'e':
			erase = 0;
			break;
		case 'o':
			outlist(&argv[1][2]);
			break;
		case 'd':
			dbg = atoi(&argv[1][2]);
			if (dbg == 0) dbg = 1;
			break;
		case 'w':	/* no wait at bottom of page */
			nowait = 1;
			break;
		}
		argc--;
		argv++;
	}

	if (argc <= 1)
		conv(stdin);
	else
		while (--argc > 0) {
			if (strcmp(*++argv, "-") == 0)
				fp = stdin;
			else if ((fp = fopen(*argv, "r")) == NULL)
				error(FATAL, "can't open %s", *argv);
			conv(fp);
			fclose(fp);
		}
	done();
	/*NOTREACHED*/
	return 0;
}

void
outlist(char *s)	/* process list of page numbers to be printed */
{
	int n1, n2, i;

	nolist = 0;
	while (*s && nolist < sizeof olist/sizeof *olist - 1) {
		n1 = 0;
		if (isdigit((unsigned char)*s))
			do
				n1 = 10 * n1 + *s++ - '0';
			while (isdigit((unsigned char)*s));
		else
			n1 = -9999;
		n2 = n1;
		if (*s == '-') {
			s++;
			n2 = 0;
			if (isdigit((unsigned char)*s))
				do
					n2 = 10 * n2 + *s++ - '0';
				while (isdigit((unsigned char)*s));
			else
				n2 = 9999;
		}
		olist[nolist++] = n1;
		olist[nolist++] = n2;
		if (*s != '\0')
			s++;
	}
	olist[nolist] = 0;
	if (dbg)
		for (i=0; i<nolist; i += 2)
			printf("%3d %3d\n", olist[i], olist[i+1]);
}

int
in_olist(int n)	/* is n in olist? */
{
	int i;

	if (nolist == 0)
		return(1);	/* everything is included */
	for (i = 0; i < nolist; i += 2)
		if (n >= olist[i] && n <= olist[i+1])
			return(1);
	return(0);
}

void
conv(register FILE *fp)
{
	register int c, k;
	int m, n, n1, m1;
	char str[4096], buf[4096];

	while ((c = getc(fp)) != EOF) {
		switch (c) {
		case '\n':	/* when input is text */
		case ' ':
		case 0:		/* occasional noise creeps in */
			break;
		case '{':	/* push down current environment */
			t_push();
			break;
		case '}':
			t_pop();
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/* two motion digits plus a character */
			hmot((c-'0')*10 + getc(fp)-'0');
			put1(getc(fp));
			break;
		case 'c':	/* single ascii character */
			put1(getc(fp));
			break;
		case 'C':
			sget(str, sizeof str, fp);
			put1s(str);
			break;
		case 't':	/* straight text */
			fgets(buf, sizeof(buf), fp);
			t_text(buf);
			break;
		case 'D':	/* draw function */
			fgets(buf, sizeof(buf), fp);
			switch (buf[0]) {
			case 'l':	/* draw a line */
				sscanf(buf+1, "%d %d", &n, &m);
				drawline(n, m, ".");
				break;
			case 'c':	/* circle */
				sscanf(buf+1, "%d", &n);
				drawcirc(n);
				break;
			case 'e':	/* ellipse */
				sscanf(buf+1, "%d %d", &m, &n);
				drawellip(m, n);
				break;
			case 'a':	/* arc */
				sscanf(buf+1, "%d %d %d %d", &n, &m, &n1, &m1);
				drawarc(n, m, n1, m1);
				break;
			case '~':	/* wiggly line */
				drawwig(buf+1);
				break;
			default:
				error(FATAL, "unknown drawing function %s\n", buf);
				break;
			}
			break;
		case 's':
			fscanf(fp, "%d", &n);
			if (n == -23) {
				float	f;
				fscanf(fp, "%f", &f);
				setsize(f);
			} else
				setsize(t_size(n));/* ignore fractional sizes */
			break;
		case 'f':
			sget(str, sizeof str, fp);
			setfont(t_font(str));
			break;
		case 'H':	/* absolute horizontal motion */
			/* fscanf(fp, "%d", &n); */
			while ((c = getc(fp)) == ' ')
				;
			k = 0;
			do {
				k = 10 * k + c - '0';
			} while (isdigit(c = getc(fp)));
			ungetc(c, fp);
			hgoto(k);
			break;
		case 'h':	/* relative horizontal motion */
			/* fscanf(fp, "%d", &n); */
			while ((c = getc(fp)) == ' ')
				;
			k = 0;
			do {
				k = 10 * k + c - '0';
			} while (isdigit(c = getc(fp)));
			ungetc(c, fp);
			hmot(k);
			break;
		case 'w':	/* word space */
			putc(' ', stdout);
			break;
		case 'V':
			fscanf(fp, "%d", &n);
			vgoto(n);
			break;
		case 'v':
			fscanf(fp, "%d", &n);
			vmot(n);
			break;
		case 'p':	/* new page */
			fscanf(fp, "%d", &n);
			t_page(n);
			break;
		case 'n':	/* end of line */
			while (getc(fp) != '\n')
				;
			t_newline();
			break;
		case '#':	/* comment */
			while (getc(fp) != '\n')
				;
			break;
		case 'x':	/* device control */
			devcntrl(fp);
			break;
		default:
			error(!FATAL, "unknown input character %o %c\n", c, c);
			done();
		}
	}
}

void
devcntrl(FILE *fp)	/* interpret device control functions */
{
	char str[4096];
	int n;

	sget(str, sizeof str, fp);
	switch (str[0]) {	/* crude for now */
	case 'i':	/* initialize */
		fileinit();
		t_init(0);
		break;
	case 'T':	/* device name */
		sget(devname, sizeof devname, fp);
		break;
	case 't':	/* trailer */
		t_trailer();
		break;
	case 'p':	/* pause -- can restart */
		t_reset('p');
		break;
	case 's':	/* stop */
		t_reset('s');
		break;
	case 'r':	/* resolution assumed when prepared */
		fscanf(fp, "%d", &res);
		break;
	case 'f':	/* font used */
		fscanf(fp, "%d", &n);
		sget(str, sizeof str, fp);
		loadfont(n, str);
		break;
	}
	while (getc(fp) != '\n')	/* skip rest of input line */
		;
}

void
fileinit(void)	/* read in font and code files, etc. */
{
}

void
fontprint(int i)	/* debugging print of font i (0,...) */
{
}

void
loadcode(int n, int nw)	/* load codetab on position n (0...); #chars is nw */
{
}

void
loadfont(int n, char *s)	/* load font info for font s on position n (1...) */
{
}

void
error(int f, const char *s, ...)
{
	va_list ap;

	fprintf(stderr, "ta: ");
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	if (f)
		exit(1);
}


/*
	Here beginneth all the stuff that really depends
	on the 202 (we hope).
*/


#define	ESC	033
#define	HOME	'H'
#define	CLEAR	'J'
#define	FF	014

int	size	= 1;
int	font	= 1;		/* current font */
int	hpos;		/* horizontal position where we are supposed to be next (left = 0) */
int	vpos;		/* current vertical position (down positive) */

int	horig;		/* h origin of current block; hpos rel to this */
int	vorig;		/* v origin of current block; vpos rel to this */

int	DX	= 10;	/* step size in x for drawing */
int	DY	= 10;	/* step size in y for drawing */
int	drawdot	= '.';	/* draw with this character */
int	drawsize = 1;	/* shrink by this factor when drawing */

void
t_init(int reinit)	/* initialize device */
{
	fflush(stdout);
	hpos = vpos = 0;
}

#define	MAXSTATE	5

struct state {
	int	ssize;
	int	sfont;
	int	shpos;
	int	svpos;
	int	shorig;
	int	svorig;
};
struct	state	state[MAXSTATE];
struct	state	*statep = state;

void
t_push(void)	/* begin a new block */
{
	hflush();
	statep->ssize = size;
	statep->sfont = font;
	statep->shorig = horig;
	statep->svorig = vorig;
	statep->shpos = hpos;
	statep->svpos = vpos;
	horig = hpos;
	vorig = vpos;
	hpos = vpos = 0;
	if (statep++ >= state+MAXSTATE)
		error(FATAL, "{ nested too deep");
	hpos = vpos = 0;
}

void
t_pop(void)	/* pop to previous state */
{
	if (--statep < state)
		error(FATAL, "extra }");
	size = statep->ssize;
	font = statep->sfont;
	hpos = statep->shpos;
	vpos = statep->svpos;
	horig = statep->shorig;
	vorig = statep->svorig;
}

int	np;	/* number of pages seen */
int	npmax;	/* high-water mark of np */
int	pgnum[40];	/* their actual numbers */
long	pgadr[40];	/* their seek addresses */

void
t_page(int n)	/* do whatever new page functions */
{
	int m, i;
	char buf[1024], *bp;

	pgnum[np++] = n;
	pgadr[np] = ftell(fp);
	if (np > npmax)
		npmax = np;
	if (output == 0) {
		output = in_olist(n);
		t_init(1);
		return;
	}
	/* have just printed something, and seen p<n> for next one */
	putpage();
	fflush(stdout);
	if (nowait)
		return;

  next:
	for (bp = buf; (*bp = readch()); )
		if (*bp++ == '\n' || bp >= &buf[sizeof buf - 1])
			break;
	*bp = 0;
	switch (buf[0]) {
	case 0:
		done();
		break;
	case '\n':
		output = in_olist(n);
		t_init(1);
		return;
	case '!':
		callunix(&buf[1]);
		fputs("!\n", stderr);
		break;
	case 'e':
		erase = 1 - erase;
		break;
	case 'w':
		wflag = 1 - wflag;
		break;
	case 'a':
		aspect = atof(&buf[1]);
		break;
	case '-':
	case 'p':
		m = atoi(&buf[1]) + 1;
		if (fp == stdin) {
			fputs("you can't; it's not a file\n", stderr);
			break;
		}
		if (np - m <= 0) {
			fputs("too far back\n", stderr);
			break;
		}
		np -= m;
		fseek(fp, pgadr[np], 0);
		output = 1;
		t_init(1);
		return;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		m = atoi(&buf[0]);
		for (i = 0; i < npmax; i++)
			if (m == pgnum[i])
				break;
		if (i >= npmax || fp == stdin) {
			fputs("you can't\n", stderr);
			break;
		}
		np = i + 1;
		fseek(fp, pgadr[np], 0);
		output = 1;
		t_init(1);
		return;
	case 'o':
		outlist(&buf[1]);
		output = 0;
		t_init(1);
		return;
	case '?':
		fputs("!cmd	unix cmd\n", stderr);
		fputs("p	print this page again\n", stderr);
		fputs("-n	go back n pages\n", stderr);
		fputs("n	print page n (previously printed)\n", stderr);
		fputs("o...	set the -o output list to ...\n", stderr);
		fputs("en	n=0 -> don't erase; n=1 -> erase\n", stderr);
		fputs("an	sets aspect ratio to n\n", stderr);
		break;
	default:
		fputs("?\n", stderr);
		break;
	}
	goto next;
}

void
putpage(void)
{
	fflush(stdout);
}

void
t_newline(void)	/* do whatever for the end of a line */
{
	printf("\n");
	hpos = 0;
}

int
t_size(int n)	/* convert integer to internal size number*/
{
	return 0;
}

int
t_font(char *s)	/* convert string to internal font number */
{
	return 0;
}

void
t_text(char *s)	/* print string s as text */
{
	int c, w=0;
	char str[100];

	if (!output)
		return;
	while ((c = *s++) != '\n') {
		if (c == '\\') {
			switch (c = *s++) {
			case '\\':
			case 'e':
				put1('\\');
				break;
			case '(':
				str[0] = *s++;
				str[1] = *s++;
				str[2] = '\0';
				put1s(str);
				break;
			}
		} else {
			put1(c);
		}
		hmot(w);
	}
}

void
t_reset(int c)
{
	output = 1;
	fflush(stdout);
	if (c == 's')
		t_page(9999);
}

void
t_trailer(void)
{
}

void
hgoto(int n)
{
	hpos = n;	/* this is where we want to be */
			/* before printing a character, */
			/* have to make sure it's true */
}

void
hmot(int n)	/* generate n units of horizontal motion */
{
	hgoto(hpos + n);
}

void
hflush(void)	/* actual horizontal output occurs here */
{
}

void
vgoto(int n)
{
	vpos = n;
}

void
vmot(int n)	/* generate n units of vertical motion */
{
	vgoto(vpos + n);	/* ignores rounding */
}

void
put1s(char *s)	/* s is a funny char name */
{
	int i;
	char *p;
	extern char *spectab[];
	static char prev[10] = "";
	static int previ;

	if (!output)
		return;
	if (strcmp(s, prev) != 0) {
		previ = -1;
		for (i = 0; spectab[i] != 0; i += 2)
			if (strcmp(spectab[i], s) == 0) {
				n_strcpy(prev, s, sizeof(prev));
				previ = i;
				break;
			}
	}
	if (previ >= 0) {
		for (p = spectab[previ+1]; *p; p++)
			putc(*p, stdout);
	} else
		prev[0] = 0;
}

void
put1(int c)	/* output char c */
{
	if (!output)
		return;
	putc(c, stdout);
}

void
setsize(double n)	/* set point size to n (internal) */
{
}

void
t_fp(int n, char *s)	/* font position n now contains font s */
{
}

void
setfont(int n)	/* set font to n */
{
}

void done(void)
{
	output = 1;
	putpage();
	fflush(stdout);
	exit(0);
}

void
callunix(char line[])
{
	int rc, status, unixpid;
	if( (unixpid=fork())==0 ) {
		signal(SIGINT,sigint); signal(SIGQUIT,sigquit);
		close(0); dup(2);
		execl("/bin/sh", "-sh", "-c", line, NULL);
		exit(255);
	}
	else if(unixpid == -1)
		return;
	else{	signal(SIGINT, SIG_IGN); signal(SIGQUIT, SIG_IGN);
		while( (rc = wait(&status)) != unixpid && rc != -1 ) ;
		signal(SIGINT,(void(*)(int))done); signal(SIGQUIT,(void(*)(int))sigquit);
	}
}

int
readch(void){
	char c;
	if (read(2,&c,1)<1) c=0;
	return(c);
}

char *spectab[] ={
	"em", "-",
	"hy", "-",
	"en", "-",
	"ru", "_",
	"l.", ".",
	"br", "|",
	"vr", "|",
	"fm", "'",
	"or", "|",
	0, 0,
};

static int
sget(char *buf, size_t size, FILE *fp)
{
	int	c, n = 0;

	do
		c = getc(fp);
	while (isspace(c));
	if (c != EOF) do {
		if (n+1 < size)
			buf[n++] = c;
		c = getc(fp);
	} while (c != EOF && !isspace(c));
	ungetc(c, fp);
	buf[n] = 0;
	return n > 1 ? 1 : c == EOF ? EOF : 0;
}
