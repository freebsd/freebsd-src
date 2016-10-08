/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
     
/*
 * Copyright (c) 1983-1988, 2001 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	from OpenSolaris "io.c	1.10	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)io.c	1.13 (gritter) 1/13/08
 */

# include "e.h"
#include <stdarg.h>
#include <stdlib.h>
#include <libgen.h>

char	*in;	/* input buffer */
size_t	insize;	/* input buffer size */
int noeqn;

int
main(int argc,char **argv) {

	progname = basename(argv[0]);
	eqnexit(eqn(argc, argv));
	/*NOTREACHED*/
	return 0;
}

void
eqnexit(int n) {
#ifdef gcos
	if (n)
		fprintf(stderr, "run terminated due to eqn error\n");
	exit(0);
#endif
	exit(n);
}

int
eqn(int argc,char **argv) {
	int i, type;

	setfile(argc,argv);
	init_tbl();	/* install keywords in tables */
	while ((type=getline(&in, &insize)) != EOF) {
		eqline = linect;
		if (type == lefteq)
			do_inline();
		else if (*in == '.') {
			char *p;
			printf("%s",in);
			for (p = in + 1; *p == ' ' || *p == '\t'; p++);
			if (!*p || *p != 'E' || p[1] != 'Q') continue;
			for (i=11; i<100; used[i++]=0);
			printf(".nr 99 \\n(.s\n.nr 98 \\n(.f\n");
			printf(".if \\n(.X .nrf 99 \\n(.s\n");
			markline = 0;
			init();
			yyparse();
			if (eqnreg>0) {
				printf(".nr %d \\w'\\*(%d'\n", eqnreg, eqnreg);
				/* printf(".if \\n(%d>\\n(.l .tm too-long eqn, file %s, between lines %d-%d\n",	*/
				/*	eqnreg, svargv[ifile], eqline, linect);	*/
				printf(".nr MK %d\n", markline);	/* for -ms macros */
				printf(".if %d>\\n(.v .ne %du\n", eqnht, eqnht);
				printf(".rn %d 10\n", eqnreg);
				if(!noeqn)printf("\\*(10\n");
			}
			printf(".ps \\n(99\n.ft \\n(98\n");
			printf(".EN");
			if (lastchar == EOF) {
				putchar('\n');
				break;
			}
			if (putchar(lastchar) != '\n')
				while (putchar(gtc()) != '\n');
		} else
			printf("%s",in);
	}
	return(0);
}

int
getline(char **sp, size_t *np) {
	register int c, n = 0, esc = 0, par = 0, brack = 0;
	char *xp;
	for (;;) {
		c = gtc();
		if (n+1 >= *np) {
			xp = realloc(*sp, *np += 128);
			if (xp == NULL) {
				error( !FATAL, "input line too long: %.20s\n",
						in);
				xp[--n] = '\0';
				break;
			}
			*sp = xp;
		}
		(*sp)[n++] = c;
		if (c=='\\')
			esc++;
		else {
			if (c=='\n' || c==EOF ||
					(c==lefteq && !esc && !par && !brack))
				break;
			if (par)
				par--;
			if (brack && c == ']')
				brack = 0;
			if (esc) {
				switch (c) {
				case '*':
				case 'f':
				case 'g':
				case 'k':
				case 'n':
				case 'P':
				case 'V':
				case 'Y':
					break;
				case '(':
					par += 2;
					break;
				case '[':
					brack++;
					break;
				default:
					esc = 0;
				}
			}
		}
	}
	if (c==lefteq && !esc)
		n--;
	(*sp)[n++] = '\0';
	return(c);
}

void
do_inline(void) {
	int ds;

	printf(".nr 99 \\n(.s\n.nr 98 \\n(.f\n");
	printf(".if \\n(.X .nrf 99 \\n(.s\n");
	ds = oalloc();
	printf(".rm %d \n", ds);
	do{
		if (*in)
			printf(".as %d \"%s\n", ds, in);
		init();
		yyparse();
		if (eqnreg > 0) {
			printf(".as %d \\*(%d\n", ds, eqnreg);
			ofree(eqnreg);
		}
		printf(".ps \\n(99\n.ft \\n(98\n");
	} while (getline(&in, &insize) == lefteq);
	if (*in)
		printf(".as %d \"%s", ds, in);
	printf(".ps \\n(99\n.ft \\n(98\n");
	printf("\\*(%d\n", ds);
	ofree(ds);
}

void
putout(int p1) {
#ifndef	NEQN
	float before, after;
	if(dbg)printf(".\tanswer <- S%d, h=%g,b=%g\n",p1, eht[p1], ebase[p1]);
#else	/* NEQN */
	int before, after;
	if(dbg)printf(".\tanswer <- S%d, h=%d,b=%d\n",p1, eht[p1], ebase[p1]);
#endif	/* NEQN */
	eqnht = eht[p1];
	printf(".ds %d ", p1);
	/* suppposed to leave room for a subscript or superscript */
#ifndef NEQN
	before = eht[p1] - ebase[p1] - VERT(EM(1.2, ps));
#else /* NEQN */
	before = eht[p1] - ebase[p1] - VERT(3);	/* 3 = 1.5 lines */
#endif /* NEQN */
	if (spaceval != NULL)
		printf("\\x'0-%s'", spaceval);
	else if (before > 0)
#ifndef	NEQN
		printf("\\x'0-%gp'", before);
#else	/* NEQN */
		printf("\\x'0-%du'", before);
#endif	/* NEQN */
	printf("\\f%c\\s%s\\*(%d%s\n",
		gfont, tsize(gsize), p1, ital(rfont[p1]) ? "\\|" : "");
	printf(".ie \\n(.X=0 .as %d \\s\\n(99\n", p1);
	printf(".el .as %d \\s[\\n(99]\n", p1);
	printf(".as %d \\f\\n(98", p1);
#ifndef NEQN
	after = ebase[p1] - VERT(EM(0.2, ps));
#else /* NEQN */
	after = ebase[p1] - VERT(1);
#endif /* NEQN */
	if (spaceval == NULL && after > 0)
#ifndef	NEQN
		printf("\\x'%gp'", after);
#else	/* NEQN */
		printf("\\x'%du'", after);
#endif	/* NEQN */
	putchar('\n');
	eqnreg = p1;
	if (spaceval != NULL) {
		free(spaceval);
		spaceval = NULL;
	}

}

float
max(float i,float j) {
	return (i>j ? i : j);
}

int
oalloc(void) {
	int i;
	for (i=11; i<100; i++)
		if (used[i]++ == 0) return(i);
	error( FATAL, "no eqn strings left", i);
	return(0);
}

void
ofree(int n) {
	used[n] = 0;
}

void
setps(float p) {
	printf(".ps %g\n", EFFPS(p));
}

void
nrwid(int n1, float p, int n2) {
	printf(".nr %d \\w'\\s%s\\*(%d'\n", n1, tsize(EFFPS(p)), n2);
}

void
setfile(int argc, char **argv) {
	static char *nullstr = "-";

	svargc = --argc;
	svargv = argv;
	while (svargc > 0 && svargv[1][0] == '-') {
		switch (svargv[1][1]) {

		case 'd': lefteq=svargv[1][2]; righteq=svargv[1][3]; break;
		case 's': gsize = atof(&svargv[1][2]); break;
		case 'p': deltaps = atof(&svargv[1][2]); break;
		case 'f': gfont = svargv[1][2]; break;
		case 'e': noeqn++; break;
		case 'r': /*resolution = atoi(&svargv[1][2]);*/ break;
		case 0:	goto endargs; 
		default: dbg = 1;
		}
		svargc--;
		svargv++;
	}
  endargs:
	ifile = 1;
	linect = 1;
	if (svargc <= 0) {
		curfile = stdin;
		svargv[1] = nullstr;
	}
	else
		openinfile();	/* opens up the first input file */
}

void
yyerror(char *unused) {;}

void
init(void) {
	ct = 0;
	ps = gsize;
	ft = gfont;
	setps(ps);
	printf(".ft %c\n", ft);
}

void
error(int fatal, const char *s1, ...) {
	va_list ap;

	if (fatal>0)
		printf("%s fatal error: ", progname);
	va_start(ap, s1);
	vfprintf(stdout, s1, ap);
	va_end(ap);
	printf("\nfile %s, between lines %d and %d\n",
		 svargv[ifile], eqline, linect);
	fprintf(stderr, "%s: ", progname);
	if (fatal>0)
		fprintf(stderr, "fatal error: ");
	va_start(ap, s1);
	vfprintf(stderr, s1, ap);
	va_end(ap);
	fprintf(stderr, "\nfile %s, between lines %d and %d\n",
		 svargv[ifile], eqline, linect);
	if (fatal > 0)
		eqnexit(1);
}
