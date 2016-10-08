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
  
/*	from OpenSolaris "text.c	1.6	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)text.c	1.8 (gritter) 1/13/08
 */

/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */

#include "e.h"
#include "y.tab.h"

extern YYSTYPE yyval;

int	csp;
int	psp;
#define	CSSIZE	400
char	cs[420];

int	lf, rf;	/* temporary spots for left and right fonts */

void
text(int t,char *p1) {
	int c;
	char *p;
	tbl *tp;
	extern tbl *restbl;

	yyval.token = oalloc();
	ebase[yyval.token] = 0;
#ifndef NEQN
	eht[yyval.token] = VERT(EM(1.0, EFFPS(ps)));	/* ht in machine units */
#else /* NEQN */
	eht[yyval.token] = VERT(2);	/* 2 half-spaces */
#endif /* NEQN */
	lfont[yyval.token] = rfont[yyval.token] = ROM;
	if (t == QTEXT)
		p = p1;
	else if ( t == SPACE )
		p = "\\ ";
	else if ( t == THIN )
		p = "\\|";
	else if ( t == TAB )
		p = "\\t";
	else if ((tp = lookup(&restbl, p1, NULL)) != NULL)
		p = tp->defn;
	else {
		lf = rf = 0;
		for (csp=psp=0; (c=p1[psp++])!='\0';) {
			rf = trans(c, p1);
			if (lf == 0)
				lf = rf;	/* save first */
			if (csp>CSSIZE)
				error(FATAL, "converted token %.25s... too long" ,p1);
		}
		cs[csp] = '\0';
		p = cs;
		lfont[yyval.token] = lf;
		rfont[yyval.token] = rf;
	}
#ifndef	NEQN
	if(dbg)printf(".\t%dtext: S%d <- %s; b=%g,h=%g,lf=%c,rf=%c\n",
		t, yyval.token, p, ebase[yyval.token], eht[yyval.token], lfont[yyval.token], rfont[yyval.token]);
#else	/* NEQN */
	if(dbg)printf(".\t%dtext: S%d <- %s; b=%d,h=%d,lf=%c,rf=%c\n",
		t, yyval.token, p, ebase[yyval.token], eht[yyval.token], lfont[yyval.token], rfont[yyval.token]);
#endif	/* NEQN */
	printf(".ds %d \"%s\n", yyval.token, p);
}

int
trans(int c,char *p1) {
	int f;
	int half = 0;
	f = ROM;
	switch( c) {
	case ')':
		half = 1;
		/*FALLTHRU*/
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	case ':': case ';': case '!': case '%':
	case '(': case '[': case ']':
		if (rf == ITAL)
			shim(half);
		roman(c); break;
	case ',':
		roman(c);
		shim(0);
		break;
	case '.':
		if (rf == ROM)
			roman(c);
		else
			cs[csp++] = c;
		f = rf;
		break;
	case '|':
		if (rf == ITAL)
			shim(0);
		shim(0); roman(c); shim(0); break;
	case '=':
		if (rf == ITAL)
			shim(0);
		name4('e','q');
		f |= OP;
		break;
	case '+':
		if (rf == ITAL)
			shim(0);
		name4('p', 'l');
		f |= OP;
		break;
	case '>': case '<':
		if (rf == ITAL)
			shim(0);
		if (p1[psp]=='=') {	/* look ahead for == <= >= */
			name4(c,'=');
			psp++;
		} else {
			cs[csp++] = c;  
		}
		f |= OP;
		break;
	case '-':
		if (rf == ITAL)
			shim(0);
		if (p1[psp]=='>') {
			name4('-','>'); psp++;
		} else {
			name4('m','i');
		}
		f |= OP;
		break;
	case '/':
		if (rf == ITAL)
			shim(0);
		name4('s','l');
		f |= OP;
		break;
	case '~': case ' ':
		shim(0); shim(0); break;
	case '^':
		shim(0); break;
	case '\\':	/* troff - pass 2 or 3 more chars */
		if (rf == ITAL)
			shim(0);
		cs[csp++] = c; cs[csp++] = c = p1[psp++]; cs[csp++] = p1[psp++];
		if (c=='(') cs[csp++] = p1[psp++];
		if (c=='*' && cs[csp-1] == '(') {
			cs[csp++] = p1[psp++];
			cs[csp++] = p1[psp++];
		} else if (c == '[' || (c == '*' && cs[csp-1] == '[')) {
			do
				cs[csp++] = p1[psp++];
			while (p1[psp-1] != ' ' && p1[psp-1] != '\t' &&
					p1[psp-1] != '\n' && p1[psp-1] != ']');
			if (cs[csp-1] != ']') {
				csp--;
				psp--;
			}
		}
		break;
	case '\'':
		cs[csp++] = '\\'; cs[csp++] = 'f'; cs[csp++] = rf==ITAL ? ITAL : ROM;
		name4('f','m');
		cs[csp++] = '\\'; cs[csp++] = 'f'; cs[csp++] = 'P';
		f = rf==ITAL ? ITAL : ROM;
		break;

	case 'f':
		if (ft == ITAL) {
			cs[csp++] = '\\'; cs[csp++] = '^';
			cs[csp++] = 'f';
			cs[csp++] = '\\'; cs[csp++] = '|';	/* trying | instead of ^ */
			f = ITAL;
		}
		else
			cs[csp++] = 'f';
		break;
	case 'j':
		if (ft == ITAL) {
			cs[csp++] = '\\'; cs[csp++] = '^';
			cs[csp++] = 'j';
			f = ITAL;
		}
		else
			cs[csp++] = 'j';
		break;
	default:
		cs[csp++] = c;
		f = ft==ITAL ? ITAL : ROM;
		break;
	}
	return(f);
}

void
shim(int small) {
	cs[csp++] = '\\'; cs[csp++] = small ? '^' : '|';
}

void
roman(int c) {
	cs[csp++] = '\\'; cs[csp++] = 'f'; cs[csp++] = ROM;
	cs[csp++] = c;
	cs[csp++] = '\\'; cs[csp++] = 'f'; cs[csp++] = 'P';
}

void
name4(int c1,int c2) {
	cs[csp++] = '\\';
	cs[csp++] = '(';
	cs[csp++] = c1;
	cs[csp++] = c2;
}
