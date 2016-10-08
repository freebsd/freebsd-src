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
  
/*	from OpenSolaris "lex.c	1.6	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)lex.c	1.7 (gritter) 11/21/07
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

#include "e.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "y.tab.h"

extern YYSTYPE yyval;

#define	SSIZE	400
char	token[SSIZE];
int	sp;
#define	putbak(c)	*ip++ = c;
#define	PUSHBACK	300	/* maximum pushback characters */
char	ibuf[PUSHBACK+SSIZE];	/* pushback buffer for definitions, etc. */
char	*ip	= ibuf;

int
gtc(void) {
  loop:
	if (ip > ibuf)
		return(*--ip);	/* already present */
	lastchar = getc(curfile);
	if (lastchar=='\n')
		linect++;
	if (lastchar != EOF)
		return(lastchar);
	if (++ifile > svargc) {
		return(EOF);
	}
	fclose(curfile);
	linect = 1;
	if (openinfile() == 0)
		goto loop;
	return(EOF);
}
/*
 *	open file indexed by ifile in svargv, return non zero if fail
 */
int
openinfile(void)
{
	if (strcmp(svargv[ifile], "-") == 0){
		curfile = stdin;
		return(0);
	} else if ((curfile=fopen(svargv[ifile], "r")) != NULL){
		return(0);
	}
	error(FATAL, "can't open file %s", svargv[ifile]);
	return(1);
}

void
pbstr(register char *str)
{
	register char *p;

	p = str;
	while (*p++);
	--p;
	if (ip >= &ibuf[PUSHBACK])
		error( FATAL, "pushback overflow");
	while (p > str)
		putbak(*--p);
}

int
yylex(void) {
	register int c;
	tbl *tp;
	extern tbl *keytbl[], *deftbl[];

  beg:
	while ((c=gtc())==' ' || c=='\n')
		;
	yylval.token = c;
	switch(c) {

	case EOF:
		return(EOF);
	case '~':
		return(SPACE);
	case '^':
		return(THIN);
	case '\t':
		return(TAB);
	case '{':
		return('{');
	case '}':
		return('}');
	case '"':
		for (sp=0; (c=gtc())!='"' && c != '\n'; ) {
			if (c == '\\')
				if ((c = gtc()) != '"')
					token[sp++] = '\\';
			token[sp++] = c;
			if (sp>=SSIZE)
				error(FATAL, "quoted string %.20s... too long", token);
		}
		token[sp]='\0';
		yylval.str = &token[0];
		if (c == '\n')
			error(!FATAL, "missing \" in %.20s", token);
		return(QTEXT);
	}
	if (c==righteq)
		return(EOF);

	putbak(c);
	if (getstr(token, SSIZE)) return EOF;
	if (dbg)printf(".\tlex token = |%s|\n", token);
	if ((tp = lookup(deftbl, token, NULL)) != NULL) {
		putbak(' ');
		pbstr(tp->defn);
		putbak(' ');
		if (dbg)
			printf(".\tfound %s|=%s|\n", token, tp->defn);
	}
	else if ((tp = lookup(keytbl, token, NULL)) == NULL) {
		if(dbg)printf(".\t%s is not a keyword\n", token);
		return(CONTIG);
	}
	else if ((intptr_t)tp->defn == DEFINE || (intptr_t)tp->defn == NDEFINE || (intptr_t)tp->defn == TDEFINE)
		define((intptr_t)tp->defn);
	else if (tp->defn == (char *) DELIM)
		delim();
	else if (tp->defn == (char *) GSIZE)
		globsize();
	else if (tp->defn == (char *) GFONT)
		globfont();
	else if (tp->defn == (char *) INCLUDE)
		include();
	else if (tp->defn == (char *) SPACE)
		space();
	else {
		return((intptr_t) tp->defn);
	}
	goto beg;
}

/* returns: 1 if ".{WS}+EN" found, 0 else */
int
getstr(char *s, register int n) {
	register int c;
	register char *p;
	enum { INI = 0, OTH, SP, C1, C2, PB } st = INI;

	p = s;
	while ((c = gtc()) == ' ' || c == '\n')
		;
	if (c == EOF) {
		*s = 0;
		return 0;
	}
	while (((c != ' ' && c != '\t') || st == SP) && c != '\n' && c != '{'
	    && c != '}' && c != '"' && c != '~' && c != '^' && c != righteq) {
		if (c == '\\')
			if ((c = gtc()) != '"')
				*p++ = '\\';
		switch (st) {
		case INI:
			st = c == '.' ? SP : OTH;
			break;
		case SP:
			if (c == 'E') st = C1;
			else if (c != ' ' && c != '\t') st = PB;
			break;
		case C1:
			st = c == 'N' ? C2 : PB;
			break;
		case C2:
			st = PB;
			break;
		default: ;
		}
		*p++ = c;
		if (st == PB)
			goto TF;
		else {
			if (--n <= 0)
				error(FATAL, "token %.20s... too long", s);
			c = gtc();
		}
	}
	if (c=='{' || c=='}' || c=='"' || c=='~' || c=='^' || c=='\t' || c==righteq)
		putbak(c);
TF:
	if (st == SP || st == C1 || st == PB) {
		while (--p != s) putbak(*p);
		p++;
	}
	*p = '\0';
	yylval.str = s;
	return st == C2;
}

int
cstr(char *s, int quote, int maxs) {
	int del, c, i;

	s[0] = 0;
	while((del=gtc()) == ' ' || del == '\t');
	if (quote)
		for (i=0; (c=gtc()) != del && c != EOF;) {
			s[i++] = c;
			if (i >= maxs)
				return(1);	/* disaster */
		}
	else {
		if (del == '\n')
			return (1);
		s[0] = del;
		for (i=1; (c=gtc())!=' ' && c!= '\t' && c!='\n' && c!=EOF;) {
			s[i++]=c;
			if (i >= maxs)
				return(1);	/* disaster */
		}
	}
	s[i] = '\0';
	if (c == EOF)
		error(FATAL, "Unexpected end of input at %.20s", s);
	return(0);
}

void
define(int type) {
	char *p1, *p2;
	extern tbl *deftbl[];

	getstr(token, SSIZE);	/* get name */
	if (type != DEFINE) {
		cstr(token, 1, SSIZE);	/* skip the definition too */
		return;
	}
	p1 = strsave(token);
	if (cstr(token, 1, SSIZE))
		error(FATAL, "Unterminated definition at %.20s", token);
	p2 = strsave(token);
	lookup(deftbl, p1, p2);
	if (dbg)printf(".\tname %s defined as %s\n", p1, p2);
}

char    *spaceval   = NULL;

void
space(void) /* collect line of form "space amt" to replace \x in output */
{
	getstr(token, SSIZE);
	spaceval = strsave(token);
	if (dbg) printf(".\tsetting space to %s\n", token);
}


char *
strsave(char *s)
{
	register char *q;
	size_t l;

	l = strlen(s)+1;
	q = malloc(l);
	if (q == NULL)
		error(FATAL, "out of space in strsave on %s", s);
	n_strcpy(q, s, l);
	return(q);
}

void
include(void) {
	error(!FATAL, "Include not yet implemented");
}

void
delim(void) {
	yyval.token = eqnreg = 0;
	if (cstr(token, 0, SSIZE) || token[0] & 0200 || token[1] & 0200)
		error(FATAL, "Bizarre delimiters at %.20s", token);
	lefteq = token[0];
	righteq = token[1];
	if (lefteq == 'o' && righteq == 'f')
		lefteq = righteq = '\0';
}
