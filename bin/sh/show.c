/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
/*static char sccsid[] = "from: @(#)show.c	5.2 (Berkeley) 4/12/91";*/
static char rcsid[] = "show.c,v 1.4 1993/08/01 18:58:00 mycroft Exp";
#endif /* not lint */

#include <stdio.h>
#include "shell.h"
#include "parser.h"
#include "nodes.h"
#include "mystring.h"


#ifdef DEBUG
static shtree(), shcmd(), sharg(), indent();


showtree(n)
	union node *n;
	{
	trputs("showtree called\n");
	shtree(n, 1, NULL, stdout);
}


static
shtree(n, ind, pfx, fp)
	union node *n;
	char *pfx;
	FILE *fp;
	{
	struct nodelist *lp;
	char *s;

	indent(ind, pfx, fp);
	switch(n->type) {
	case NSEMI:
		s = "; ";
		goto binop;
	case NAND:
		s = " && ";
		goto binop;
	case NOR:
		s = " || ";
binop:
		shtree(n->nbinary.ch1, ind, NULL, fp);
	   /*    if (ind < 0) */
			fputs(s, fp);
		shtree(n->nbinary.ch2, ind, NULL, fp);
		break;
	case NCMD:
		shcmd(n, fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	case NPIPE:
		for (lp = n->npipe.cmdlist ; lp ; lp = lp->next) {
			shcmd(lp->n, fp);
			if (lp->next)
				fputs(" | ", fp);
		}
		if (n->npipe.backgnd)
			fputs(" &", fp);
		if (ind >= 0)
			putc('\n', fp);
		break;
	default:
		fprintf(fp, "<node type %d>", n->type);
		if (ind >= 0)
			putc('\n', fp);
		break;
	}
}



static
shcmd(cmd, fp)
	union node *cmd;
	FILE *fp;
	{
	union node *np;
	int first;
	char *s;
	int dftfd;

	first = 1;
	for (np = cmd->ncmd.args ; np ; np = np->narg.next) {
		if (! first)
			putchar(' ');
		sharg(np, fp);
		first = 0;
	}
	for (np = cmd->ncmd.redirect ; np ; np = np->nfile.next) {
		if (! first)
			putchar(' ');
		switch (np->nfile.type) {
			case NTO:	s = ">";  dftfd = 1; break;
			case NAPPEND:	s = ">>"; dftfd = 1; break;
			case NTOFD:	s = ">&"; dftfd = 1; break;
			case NFROM:	s = "<";  dftfd = 0; break;
			case NFROMFD:	s = "<&"; dftfd = 0; break;
		}
		if (np->nfile.fd != dftfd)
			fprintf(fp, "%d", np->nfile.fd);
		fputs(s, fp);
		if (np->nfile.type == NTOFD || np->nfile.type == NFROMFD) {
			fprintf(fp, "%d", np->ndup.dupfd);
		} else {
			sharg(np->nfile.fname, fp);
		}
		first = 0;
	}
}



static
sharg(arg, fp)
	union node *arg;
	FILE *fp;
	{
	char *p;
	struct nodelist *bqlist;
	int subtype;

	if (arg->type != NARG) {
		printf("<node type %d>\n", arg->type);
		fflush(stdout);
		abort();
	}
	bqlist = arg->narg.backquote;
	for (p = arg->narg.text ; *p ; p++) {
		switch (*p) {
		case CTLESC:
			putc(*++p, fp);
			break;
		case CTLVAR:
			putc('$', fp);
			putc('{', fp);
			subtype = *++p;
			while (*p != '=')
				putc(*p++, fp);
			if (subtype & VSNUL)
				putc(':', fp);
			switch (subtype & VSTYPE) {
			case VSNORMAL:
				putc('}', fp);
				break;
			case VSMINUS:
				putc('-', fp);
				break;
			case VSPLUS:
				putc('+', fp);
				break;
			case VSQUESTION:
				putc('?', fp);
				break;
			case VSASSIGN:
				putc('=', fp);
				break;
			default:
				printf("<subtype %d>", subtype);
			}
			break;
		case CTLENDVAR:
		     putc('}', fp);
		     break;
		case CTLBACKQ:
		case CTLBACKQ|CTLQUOTE:
			putc('$', fp);
			putc('(', fp);
			shtree(bqlist->n, -1, NULL, fp);
			putc(')', fp);
			break;
		default:
			putc(*p, fp);
			break;
		}
	}
}


static
indent(amount, pfx, fp)
	char *pfx;
	FILE *fp;
	{
	int i;

	for (i = 0 ; i < amount ; i++) {
		if (pfx && i == amount - 1)
			fputs(pfx, fp);
		putc('\t', fp);
	}
}
#endif



/*
 * Debugging stuff.
 */


FILE *tracefile;

#if DEBUG == 2
int debug = 1;
#else
int debug = 0;
#endif


trputc(c) {
#ifdef DEBUG
	if (tracefile == NULL)
		return;
	putc(c, tracefile);
	if (c == '\n')
		fflush(tracefile);
#endif
}


trace(fmt, a1, a2, a3, a4, a5, a6, a7, a8)
	char *fmt;
	{
#ifdef DEBUG
	if (tracefile == NULL)
		return;
	fprintf(tracefile, fmt, a1, a2, a3, a4, a5, a6, a7, a8);
	if (strchr(fmt, '\n'))
		fflush(tracefile);
#endif
}


trputs(s)
	char *s;
	{
#ifdef DEBUG
	if (tracefile == NULL)
		return;
	fputs(s, tracefile);
	if (strchr(s, '\n'))
		fflush(tracefile);
#endif
}


trstring(s)
	char *s;
	{
	register char *p;
	char c;

#ifdef DEBUG
	if (tracefile == NULL)
		return;
	putc('"', tracefile);
	for (p = s ; *p ; p++) {
		switch (*p) {
		case '\n':  c = 'n';  goto backslash;
		case '\t':  c = 't';  goto backslash;
		case '\r':  c = 'r';  goto backslash;
		case '"':  c = '"';  goto backslash;
		case '\\':  c = '\\';  goto backslash;
		case CTLESC:  c = 'e';  goto backslash;
		case CTLVAR:  c = 'v';  goto backslash;
		case CTLVAR+CTLQUOTE:  c = 'V';  goto backslash;
		case CTLBACKQ:  c = 'q';  goto backslash;
		case CTLBACKQ+CTLQUOTE:  c = 'Q';  goto backslash;
backslash:	  putc('\\', tracefile);
			putc(c, tracefile);
			break;
		default:
			if (*p >= ' ' && *p <= '~')
				putc(*p, tracefile);
			else {
				putc('\\', tracefile);
				putc(*p >> 6 & 03, tracefile);
				putc(*p >> 3 & 07, tracefile);
				putc(*p & 07, tracefile);
			}
			break;
		}
	}
	putc('"', tracefile);
#endif
}


trargs(ap)
	char **ap;
	{
#ifdef DEBUG
	if (tracefile == NULL)
		return;
	while (*ap) {
		trstring(*ap++);
		if (*ap)
			putc(' ', tracefile);
		else
			putc('\n', tracefile);
	}
	fflush(tracefile);
#endif
}


opentrace() {
	char s[100];
	char *p;
	char *getenv();
	int flags;

#ifdef DEBUG
	if (!debug)
		return;
	if ((p = getenv("HOME")) == NULL) {
		if (getuid() == 0)
			p = "/";
		else
			p = "/tmp";
	}
	scopy(p, s);
	strcat(s, "/trace");
	if ((tracefile = fopen(s, "a")) == NULL) {
		fprintf(stderr, "Can't open %s\n", s);
		return;
	}
#ifdef O_APPEND
	if ((flags = fcntl(fileno(tracefile), F_GETFL, 0)) >= 0)
		fcntl(fileno(tracefile), F_SETFL, flags | O_APPEND);
#endif
	fputs("\nTracing started.\n", tracefile);
	fflush(tracefile);
#endif
}
