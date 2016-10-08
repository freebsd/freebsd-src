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

/*	from OpenSolaris "glue5.c	1.4	05/06/02 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)glue5.c	1.4 (gritter) 10/2/07
 */


#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include "refer..c"
/*
 * fgrep -- print all lines containing any of a set of keywords
 *
 *	status returns:
 *		0 - ok, and some matches
 *		1 - ok, but no matches
 *		2 - some error
 */
#define	MAXSIZ 700
#define QSIZE 400
struct words {
	char 	inp;
	char	out;
	struct	words *nst;
	struct	words *link;
	struct	words *fail;
} 
*www, *smax, *q;

char	buf[2*BUFSIZ];
int	nsucc;
int	need;
char	*instr;
int	inct;
int	rflag;
int	xargc;
char	**xargv;
int	numwords;
int	nfound;
static int flag = 0;

static void execute(void);
static void cgotofn(void);
static int gch(void);
static void overflo(void);
static void cfail(void);
static int new(struct words *);

int
fgrep(int argc, char **argv)
{
	nsucc = need = inct = rflag = numwords = nfound = 0;
	instr = 0;
	flag = 0;
	if (www==0)
		www = zalloc(MAXSIZ, sizeof (*www));
	if (www==NULL)
		err("Can't get space for machines", 0);
	for (q=www; q<www+MAXSIZ; q++) {
		q->inp =0; q->out =0; q->nst =0; q->link =0; q->fail =0;
	}
	xargc = argc-1;
	xargv = argv+1;
	while (xargc>0 && xargv[0][0]=='-')
		{
		switch(xargv[0][1])
			{
			case 'r': /* return value only */
				rflag++;
				break;
			case 'n': /* number of answers needed */
				need = (intptr_t) xargv[1];
				xargv++; xargc--;
				break;
			case 'i':
				instr = xargv[1];
				inct = (intptr_t) xargv[2]+2;
# if D2
fprintf(stderr,"inct %d xargv.2. %o %d\n",inct, xargv[2],xargv[2]);
# endif
				xargv += 2; xargc -= 2;
				break;
			}
		xargv++; xargc--;
		}
	if (xargc<=0)
		{
		write (2, "bad fgrep call\n", 15);
		exit(2);
		}
# if D1
	fprintf(stderr, "before cgoto\n");
# endif
	cgotofn();
# if D1
	fprintf(stderr, "before cfail\n");
# endif
	cfail();
# if D1
	fprintf(stderr, "before execute instr %.20s\n", instr? instr: "");
	fprintf(stderr, "end of string %d %c %c %c\n", inct,
		instr ? instr[inct-3] : '\0',
		instr ? instr[inct-2] : '\0',
		instr ? instr[inct-1] : '\0');
# endif
	execute();
# if D1
	fprintf(stderr, "returning nsucc %d\n", nsucc);
	fprintf(stderr, "fgrep done www %o\n",www);
# endif
	return(nsucc == 0);
}

static void
execute(void)
{
	register char *p;
	register struct words *c;
	register int ch;
	register int ccount;
	int f;
	char *nlp;
	f=0;
	ccount = instr ? inct : 0;
	nfound=0;
	p = instr ? instr : buf;
	if (need == 0) need = numwords;
	nlp = p;
	c = www;
# if D2
fprintf(stderr, "in execute ccount %d inct %d\n",ccount, inct );
# endif
	for (;;) {
# if D3
fprintf(stderr, "down ccount\n");
# endif
		if (--ccount <= 0) {
# if D2
fprintf(stderr, "ex loop ccount %d instr %o\n",ccount, instr);
# endif
			if (instr) break;
			if (p == &buf[2*BUFSIZ]) p = buf;
			if (p > &buf[BUFSIZ]) {
				if ((ccount = read(f, p, &buf[2*BUFSIZ] - p)) <= 0) break;
			}
			else if ((ccount = read(f, p, BUFSIZ)) <= 0) break;
# if D2
fprintf(stderr, " normal read %d bytres\n", ccount);
{char xx[20]; sprintf(xx, "they are %%.%ds\n", ccount);
fprintf(stderr, xx, p);
}
# endif
		}
nstate:
		ch = *p;
# if D2
fprintf(stderr, "roaming along in ex ch %c c %o\n",ch,c);
# endif
		if (isupper(ch)) ch |= 040;
		if (c->inp == ch) {
			c = c->nst;
		}
		else if (c->link != 0) {
			c = c->link;
			goto nstate;
		}
		else {
			c = c->fail;
			if (c==0) {
				c = www;
istate:
				if (c->inp == ch) {
					c = c->nst;
				}
				else if (c->link != 0) {
					c = c->link;
					goto istate;
				}
			}
			else goto nstate;
		}
		if (c->out && new (c)) {
# if D2
fprintf(stderr, " found: nfound %d need %d\n",nfound,need);
# endif
			if (++nfound >= need)
			{
# if D1
fprintf(stderr, "found, p %o nlp %o ccount %d buf %o buf[2*BUFSIZ] %o\n",p,nlp,ccount,buf,buf+2*BUFSIZ);
# endif
				if (instr==0)
				while (*p++ != '\n') {
# if D3
fprintf(stderr, "down ccount2\n");
# endif
					if (--ccount <= 0) {
						if (p == &buf[2*BUFSIZ]) p = buf;
						if (p > &buf[BUFSIZ]) {
							if ((ccount = read(f, p, &buf[2*BUFSIZ] - p)) <= 0) break;
						}
						else if ((ccount = read(f, p, BUFSIZ)) <= 0) break;
# if D2
fprintf(stderr, " read %d bytes\n",ccount);
{ char xx[20]; sprintf(xx, "they are %%.%ds\n", ccount);
fprintf(stderr, xx, p);
}
# endif
					}
				}
				nsucc = 1;
				if (rflag==0)
					{
# if D2
fprintf(stderr, "p %o nlp %o buf %o\n",p,nlp,buf);
if (p>nlp)
{write (2, "XX\n", 3); write (2, nlp, p-nlp); write (2, "XX\n", 3);}
# endif
					if (p > nlp) write(1, nlp, p-nlp);
					else {
						write(1, nlp, &buf[2*BUFSIZ] - nlp);
						write(1, buf, p-&buf[0]);
						}
					if (p[-1]!= '\n') write (1, "\n", 1);
					}
				if (instr==0)
					{
					nlp = p;
					c = www;
					nfound=0; 
					}
			}
			else
				ccount++;
			continue;
		}
# if D2
fprintf(stderr, "nr end loop p %o\n",p);
# endif
		if (instr)
			p++;
		else
		if (*p++ == '\n')
		{
			nlp = p;
			c = www;
			nfound=0;
		}
	}
	if (instr==0)
		close(f);
}

static void
cgotofn(void) {
	register int c;
	register struct words *s;
	s = smax = www;
nword:	
	for(;;) {
# if D1
	fprintf(stderr, " in for loop c now %o %c\n",c, c>' ' ? c : ' ');
# endif
		if ((c = gch())==0) return;
		else if (c == '\n') {
			s->out = 1;
			s = www;
		}
		else {
loop:	
			if (s->inp == c) {
				s = s->nst;
				continue;
			}
			if (s->inp == 0) goto enter;
			if (s->link == 0) {
				if (smax >= &www[MAXSIZ - 1]) overflo();
				s->link = ++smax;
				s = smax;
				goto enter;
			}
			s = s->link;
			goto loop;
		}
	}

enter:
	do {
		s->inp = c;
		if (smax >= &www[MAXSIZ - 1]) overflo();
		s->nst = ++smax;
		s = smax;
	} 
	while ((c = gch()) != '\n');
	smax->out = 1;
	s = www;
	numwords++;
	goto nword;

}

static int
gch(void)
{
	static char *s;
	if (flag==0)
	{
		flag=1;
		s = *xargv++;
# if D1
	fprintf(stderr, "next arg is %s xargc %d\n", xargc > 0 ? s : "", xargc);
# endif
		if (xargc-- <=0) return(0);
	}
	if (*s) return(*s++);
	for(flag=0; flag<2*BUFSIZ; flag++)
		buf[flag]=0;
	flag=0;
	return('\n');
}

static void
overflo(void) {
	write(2,"wordlist too large\n", 19);
	exit(2);
}
static void
cfail(void) {
	struct words *queue[QSIZE];
	struct words **front, **rear;
	struct words *state;
	register char c;
	register struct words *s;
	s = www;
	front = rear = queue;
init:	
	if ((s->inp) != 0) {
		*rear++ = s->nst;
		if (rear >= &queue[QSIZE - 1]) overflo();
	}
	if ((s = s->link) != 0) {
		goto init;
	}

	while (rear!=front) {
		s = *front;
		if (front == &queue[QSIZE-1])
			front = queue;
		else front++;
cloop:	
		if ((c = s->inp) != 0) {
			*rear = (q = s->nst);
			if (front < rear)
				if (rear >= &queue[QSIZE-1])
					if (front == queue) overflo();
					else rear = queue;
			else rear++;
			else
				if (++rear == front) overflo();
			state = s->fail;
floop:	
			if (state == 0) state = www;
			if (state->inp == c) {
				q->fail = state->nst;
				if ((state->nst)->out == 1) q->out = 1;
				continue;
			}
			else if ((state = state->link) != 0)
				goto floop;
		}
		if ((s = s->link) != 0)
			goto cloop;
	}
}

static struct words *seen[50];
static int
new (struct words *x)
{
	int i;
	for(i=0; i<nfound; i++)
		if (seen[i]==x)
			return(0);
	seen[i]=x;
	return(1);
}
