/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/grap/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)input.c	1.7 (gritter) 12/25/06	*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "grap.h"
#include "y.tab.h"

#if defined (__GLIBC__) && defined (_IO_getc_unlocked)
#undef	getc
#define	getc(f)	_IO_getc_unlocked(f)
#endif

Infile	infile[10];
Infile	*curfile = infile;

#define	MAXSRC	50
Src	src[MAXSRC];	/* input source stack */
Src	*srcp	= src;

void pushsrc(int type, char *ptr)	/* new input source */
{
	if (++srcp >= src + MAXSRC)
		FATAL("inputs nested too deep");
	srcp->type = type;
	srcp->sp = ptr;
	if (dbg) {
		printf("\n%3d ", (int)(srcp - src));
		switch (srcp->type) {
		case File:
			printf("push file %s\n", ((Infile *)ptr)->fname);
			break;
		case Macro:
			printf("push macro <%s>\n", ptr);
			break;
		case Char:
			printf("push char <%c>\n", *ptr);
			break;
		case Thru:
			printf("push thru\n");
			break;
		case String:
			printf("push string <%s>\n", ptr);
			break;
		case Free:
			printf("push free <%s>\n", ptr);
			break;
		default:
			FATAL("pushed bad type %d", srcp->type);
		}
	}
}

void popsrc(void)	/* restore an old one */
{
	if (srcp <= src)
		FATAL("too many inputs popped");
	if (dbg) {
		printf("%3d ", (int)(srcp - src));
		switch (srcp->type) {
		case File:
			printf("pop file\n");
			break;
		case Macro:
			printf("pop macro\n");
			break;
		case Char:
			printf("pop char <%c>\n", *srcp->sp);
			break;
		case Thru:
			printf("pop thru\n");
			break;
		case String:
			printf("pop string\n");
			break;
		case Free:
			printf("pop free\n");
			break;
		default:
			FATAL("pop weird input %d", srcp->type);
		}
	}
	srcp--;
}

void definition(char *s)	/* collect definition for s and install */
				/* definitions picked up lexically */
{
	char *p;
	Obj *stp;

	p = delimstr("definition");
	stp = lookup(s, 0);
	if (stp != NULL) {	/* it's there before */
		if (stp->type != DEFNAME) {
			WARNING("%s used as variable and definition", s);
			return;
		}
		free(stp->val);
	} else {
		stp = lookup(s, 1);
		stp->type = DEFNAME;
	}
	stp->val = p;
	dprintf("installing %s as `%s'\n", s, p);
}

char *delimstr(char *s)	/* get body of X ... X */
			/* message if too big */
{
	int c, delim, rdelim, n, deep;
	static char *buf = NULL;
	static int nbuf = 0;
	char *p;

	if (buf == NULL)
		buf = grow(buf, "buf", nbuf += 1000, sizeof(buf[0]));
	while ((delim = input()) == ' ' || delim == '\t' || delim == '\n')
		;
	rdelim = baldelim(delim, "{}");		/* could be "(){}[]`'" */
	deep = 1;
	for (p = buf; ; ) {
		c = input();
		if (c == rdelim)
			if (--deep == 0)
				break;
		if (c == delim)
			deep++;
		if (p >= buf + nbuf) {
			n = p - buf;
			buf = grow(buf, "buf", nbuf += 1000, sizeof(buf[0]));
			p = buf + n;
		}
		if (c == EOF)
			FATAL("end of file in %s %c %.20s... %c", s, delim, buf, delim);
		*p++ = c;
	}
	*p = '\0';
	dprintf("delimstr %s %c <%s> %c\n", s, delim, buf, delim);
	return tostring(buf);
}

int baldelim(int c, char *s)	/* replace c by balancing entry in s */
{
	for ( ; *s; s += 2)
		if (*s == c)
			return s[1];
	return c;
}

Arg	args[10];	/* argument frames */
Arg	*argfp = args;	/* frame pointer */
int	argcnt;		/* number of arguments seen so far */

void dodef(Obj *stp)	/* collect args and switch input to defn */
{
	int i, len;
	char *p;
	Arg *ap;

	ap = argfp+1;
	if (ap >= args+10)
		FATAL("arguments too deep");
	argcnt = 0;
	if (input() != '(')
		FATAL("disaster in dodef");
	if (ap->argval == 0)
		ap->argval = malloc(1000);
	for (p = ap->argval; (len = getarg(p)) != -1; p += len) {
		ap->argstk[argcnt++] = p;
		if (input() == ')')
			break;
	}
	for (i = argcnt; i < MAXARGS; i++)
		ap->argstk[i] = "";
	if (dbg)
		for (i = 0; i < argcnt; i++)
			printf("arg %d.%d = <%s>\n", (int)(ap-args), i+1, ap->argstk[i]);
	argfp = ap;
	pushsrc(Macro, stp->val);
}

int getarg(char *p)	/* pick up single argument, store in p, return length */
{
	int n, c, npar;

	n = npar = 0;
	for ( ;; ) {
		c = input();
		if (c == EOF)
			FATAL("end of file in getarg!");
		if (npar == 0 && (c == ',' || c == ')'))
			break;
		if (c == '"')	/* copy quoted stuff intact */
			do {
				*p++ = c;
				n++;
			} while ((c = input()) != '"' && c != EOF);
		else if (c == '(')
			npar++;
		else if (c == ')')
			npar--;
		n++;
		*p++ = c;
	}
	*p = 0;
	unput(c);
	return(n + 1);
}

#define	PBSIZE	2000
char	pbuf[PBSIZE];		/* pushback buffer */
char	*pb	= pbuf-1;	/* next pushed back character */

char	ebuf[200];		/* collect input here for error reporting */
char	*ep	= ebuf;

int	begin	= 0;
extern	int	thru;
extern	Obj	*thrudef;
extern	char	*untilstr;

int input(void)
{
	register int c;

	if (thru && begin) {
		do_thru();
		begin = 0;
	}
	c = nextchar();
	dprintf(" <%c>", c);
	if (ep >= ebuf + sizeof ebuf)
		ep = ebuf;
	return (*ep++ = c) & 0377;
}

int nextchar(void)
{
	register int c = 0;

  loop:
	switch (srcp->type) {
	case Free:	/* free string */
		free(srcp->sp);
		popsrc();
		goto loop;
	case Thru:	/* end of pushed back line */
		begin = 1;
		popsrc();
		c = '\n';
		break;
	case Char:
		if (pb >= pbuf) {
			c = *pb--;
			popsrc();
			break;
		} else {	/* can't happen? */
			popsrc();
			goto loop;
		}
	case String:
		c = *srcp->sp++;
		if (c == '\0') {
			popsrc();
			goto loop;
		} else {
			if (*srcp->sp == '\0')	/* empty, so pop */
				popsrc();
			break;
		}
	case Macro:
		c = *srcp->sp++;
		if (c == '\0') {
			if (--argfp < args)
				FATAL("argfp underflow");
			popsrc();
			goto loop;
		} else if (c == '$' && isdigit((int)*srcp->sp)) {	/* $3 */
			int n = 0;
			while (isdigit((int)*srcp->sp))
				n = 10 * n + *srcp->sp++ - '0';
			if (n > 0 && n <= MAXARGS)
				pushsrc(String, argfp->argstk[n-1]);
			goto loop;
		}
		break;
	case File:
		c = getc(curfile->fin);
		if (c == EOF) {
			if (curfile == infile)
				FATAL("end of file inside .G1/.G2");
			if (curfile->fin != stdin) {
				fclose(curfile->fin);
				free(curfile->fname);	/* assumes allocated */
			}
			curfile--;
			printf(".lf %d %s\n", curfile->lineno, curfile->fname);
			popsrc();
			thru = 0;	/* chicken out */
			thrudef = 0;
			if (untilstr) {
				free(untilstr);
				untilstr = 0;
			}
			goto loop;
		}
		if (c == '\n')
			curfile->lineno++;
		break;
	}
	return c;
}

void do_thru(void)	/* read one line, make into a macro expansion */
{
	int c, i;
	char *p;
	Arg *ap;

	ap = argfp+1;
	if (ap >= args+10)
		FATAL("arguments too deep");
	if (ap->argval == NULL)
		ap->argval = malloc(1000);
	p = ap->argval;
	argcnt = 0;
	c = nextchar();
	if (thru == 0) {	/* end of file was seen, so thru is done */
		unput(c);
		return;
	}
	for ( ; c != '\n' && c != EOF; ) {
		if (c == ' ' || c == '\t') {
			c = nextchar();
			continue;
		}
		if (argcnt >= MAXARGS)
			FATAL("too many fields on input line");
		ap->argstk[argcnt++] = p;
		if (c == '"') {
			do {
				*p++ = c;
				if ((c = nextchar()) == '\\') {
					*p++ = c;
					*p++ = nextchar();
					c = nextchar();
				}
			} while (c != '"' && c != '\n' && c != EOF);
			*p++ = '"';
			if (c == '"')
				c = nextchar();
		} else {
			do {
				*p++ = c;
			} while ((c = nextchar())!=' ' && c!='\t' && c!='\n' && c!=',' && c!=EOF);
			if (c == ',')
				c = nextchar();
		}
		*p++ = '\0';
	}
	if (c == EOF)
		FATAL("unexpected end of file in do_thru");
	if (argcnt == 0) {	/* ignore blank line */
		pushsrc(Thru, (char *) 0);
		return;
	}
	for (i = argcnt; i < MAXARGS; i++)
		ap->argstk[i] = "";
	if (dbg)
		for (i = 0; i < argcnt; i++)
			printf("arg %d.%d = <%s>\n", (int)(ap-args), i+1, ap->argstk[i]);
	if (strcmp(ap->argstk[0], ".G2") == 0) {
		thru = 0;
		thrudef = 0;
		pushsrc(String, "\n.G2\n");
		return;
	}
	if (untilstr && strcmp(ap->argstk[0], untilstr) == 0) {
		thru = 0;
		thrudef = 0;
		free(untilstr);
		untilstr = 0;
		return;
	}
	pushsrc(Thru, (char *) 0);
	dprintf("do_thru pushing back <%s>\n", thrudef->val);
	argfp = ap;
	pushsrc(Macro, thrudef->val);
}

int unput(int c)
{
	if (++pb >= pbuf + sizeof pbuf)
		FATAL("pushback overflow");
	if (--ep < ebuf)
		ep = ebuf + sizeof(ebuf) - 1;
	*pb = c;
	pushsrc(Char, pb);
	return c;
}

void pbstr(char *s)
{
	pushsrc(String, s);
}

double errcheck(double x, char *s)
{
	if (errno == EDOM) {
		errno = 0;
		WARNING("%s argument out of domain", s);
	} else if (errno == ERANGE) {
		errno = 0;
		WARNING("%s result out of range", s);
	}
	return x;
}

char	errbuf[200];

void yyerror(char *s)
{
	extern char *cmdname;
	int ern = errno;	/* cause some libraries clobber it */

	if (synerr)
		return;
	fflush(stdout);
	fprintf(stderr, "%s: %s", cmdname, s);
	if (ern > 0) {
		errno = ern;
		perror("???");
	}
	fprintf(stderr, " near %s:%d\n",
		curfile->fname, curfile->lineno+1);
	eprint();
	synerr = 1;
	errno = 0;
}

void eprint(void)	/* try to print context around error */
{
	char *p, *q;

	p = ep - 1;
	if (p > ebuf && *p == '\n')
		p--;
	for ( ; p >= ebuf && *p != '\n'; p--)
		;
	while (*p == '\n')
		p++;
	fprintf(stderr, " context is\n\t");
	for (q=ep-1; q>=p && *q!=' ' && *q!='\t' && *q!='\n'; q--)
		;
	for (; p < q; p++)
		if (isprint((int)*p))
			putc(*p, stderr);
	fprintf(stderr, " >>> ");
	for (; p < q; p++)
		if (isprint((int)*p))
			putc(*p, stderr);
	fprintf(stderr, " <<< ");
	while (pb >= pbuf)
		putc(*pb--, stderr);
	fgets(ebuf, sizeof ebuf, curfile->fin);
	fprintf(stderr, "%s", ebuf);
	pbstr("\n.G2\n");	/* safety first */
	ep = ebuf;
}

int yywrap(void) {return 1;}

char	*newfile = 0;		/* filename for file copy */
char	*untilstr = 0;		/* string that terminates a thru */
int	thru	= 0;		/* 1 if copying thru macro */
Obj	*thrudef = 0;		/* macro being used */

void copyfile(char *s)	/* remember file to start reading from */
{
	newfile = s;
}

void copydef(Obj *p)	/* remember macro Obj */
{
	thrudef = p;
}

Obj *copythru(char *s)	/* collect the macro name or body for thru */
{
	Obj *p;
	char *q;

	p = lookup(s, 0);
	if (p != NULL) {
		if (p->type == DEFNAME) {
			p->val = addnewline(p->val);
			return p;
		} else
			FATAL("%s used as define and name", s);
	}
	/* have to collect the definition */
	pbstr(s);	/* first char is the delimiter */
	q = delimstr("thru body");
	p = lookup("nameless", 1);
	if (p != NULL)
		if (p->val)
			free(p->val);
	p->type = DEFNAME;
	p->val = q;
	p->val = addnewline(p->val);
	dprintf("installing nameless as `%s'\n", p->val);
	return p;
}

char *addnewline(char *p)	/* add newline to end of p */
{
	int n;

	n = strlen(p);
	if (p[n-1] != '\n') {
		p = realloc(p, n+2);
		p[n] = '\n';
		p[n+1] = '\0';
	}
	return p;
}

void copyuntil(char *s)	/* string that terminates a thru */
{
	untilstr = s;
}

void copy(void)	/* begin input from file, etc. */
{
	FILE *fin;

	if (newfile) {
		if ((fin = fopen(newfile, "r")) == NULL)
			FATAL("can't open file %s", newfile);
		curfile++;
		curfile->fin = fin;
		curfile->fname = tostring(newfile);
		curfile->lineno = 0;
		printf(".lf 1 %s\n", curfile->fname);
		pushsrc(File, curfile->fname);
		newfile = 0;
	}
	if (thrudef) {
		thru = 1;
		begin = 1;	/* wrong place */
	}
}

char	shellbuf[1000], *shellp;

void shell_init(void)	/* set up to interpret a shell command */
{
	fprintf(tfd, "# shell cmd...\n");
	shellp = shellbuf;
}

void shell_text(char *s)	/* add string to command being collected */
{
	/* fprintf(tfd, "#add <%s> to <%s>\n", s, shellbuf); */
	while (*s) {
		if (shellp+5 >= &shellbuf[sizeof shellbuf])
			FATAL("shell command too long");
		if (*s == '\'')	{	/* protect interior quotes */
			*shellp++ = '\'';
			*shellp++ = '\\';
			*shellp++ = '\'';
		}
		*shellp++ = *s++;
	}
}

void shell_exec(void)	/* do it */
{
	/* fprintf(tfd, "# run <%s>\n", shellbuf); */
	*shellp = '\0';
	if (Sflag)
		WARNING("-S inhibited execution of shell command");
	else
		system(shellbuf);
}

#define	LSIZE	128

char *fgetline(char **line, size_t *linesize, size_t *llen, FILE *fp)
{
	int c;
	size_t n = 0;

	if (*line == NULL || *linesize < LSIZE + n + 1)
		*line = realloc(*line, *linesize = LSIZE + n + 1);
	for (;;) {
		if (n >= *linesize - LSIZE / 2)
			*line = realloc(*line, *linesize += LSIZE);
		c = getc(fp);
		if (c != EOF) {
			(*line)[n++] = c;
			(*line)[n] = '\0';
			if (c == '\n')
				break;
		} else {
			if (n > 0)
				break;
			else
				return NULL;
		}
	}
	if (llen)
		*llen = n;
	return *line;
}
