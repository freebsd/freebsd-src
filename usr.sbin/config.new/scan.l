%{
/* 
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *
 *	@(#)scan.l	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "y.tab.h"

int	yyline;
const char *yyfile;
const char *lastfile;

int	include __P((const char *, int));

/*
 * Data for returning to previous files from include files.
 */
struct incl {
	struct	incl *in_prev;	/* previous includes in effect, if any */
	YY_BUFFER_STATE in_buf;	/* previous lex state */
	const char *in_fname;	/* previous file name */
	int	in_lineno;	/* previous line number */
	int	in_preveof;	/* previous eoftoken */
};
static struct incl *incl;
static int eoftoken;		/* current EOF token */
static void endinclude __P((void));

#define	yywrap() 1

%}

PATH	[-/A-Za-z0-9_.]*[./][-/A-Za-z0-9_.]*
WORD	[A-Za-z_][-A-Za-z_0-9]*

%%

		/* plain keywords */
and		{ return AND; }
at		{ return AT; }
compile-with	{ return COMPILE_WITH; }
config		{ return CONFIG; }
define		{ return DEFINE; }
device		{ return DEVICE; }
dumps		{ return DUMPS; }
flags		{ return FLAGS; }
file		{ return XFILE; }
include		{ return INCLUDE; }
machine		{ return XMACHINE; }
major		{ return MAJOR; }
makeoptions	{ return MAKEOPTIONS; }
maxusers	{ return MAXUSERS; }
minor		{ return MINOR; }
on		{ return ON; }
options		{ return OPTIONS; }
"pseudo-device"	{ return PSEUDO_DEVICE; }
root		{ return ROOT; }
swap		{ return SWAP; }
vector		{ return VECTOR; }

		/* keywords with values */
config-dependent { yylval.val = FI_CONFIGDEP; return FFLAG; }
device-driver	{ yylval.val = FI_DRIVER; return FFLAG; }
needs-count	{ yylval.val = FI_NEEDSCOUNT; return FFLAG; }
needs-flag	{ yylval.val = FI_NEEDSFLAG; return FFLAG; }

		/* all the rest */
{PATH}		{ yylval.str = intern(yytext); return PATHNAME; }
{WORD}		{ yylval.str = intern(yytext); return WORD; }

\"[^"]+/\" {
		yylval.str = intern(yytext + 1);
		(void)input();	/* eat closing quote */
		return WORD;
	}
0[0-7]*	{
		yylval.val = strtol(yytext, NULL, 8);
		return NUMBER;
	}
0[xX][0-9a-fA-F]+ {
		yylval.val = strtol(yytext + 2, NULL, 16);
		return NUMBER;
	}
[1-9][0-9]* {
		yylval.val = strtol(yytext, NULL, 10);
		return NUMBER;
	}
\n/[ \t] {
		yyline++;
	}
\n	{
		yyline++;
		return '\n';
	}
#.*	{ /* ignored (comment) */; }
[ \t]*	{ /* ignored (white space) */; }
.	{ return yytext[0]; }
<<EOF>> {
		int tok;

		tok = eoftoken;
		eoftoken = YY_NULL;
		if (incl != NULL)
			endinclude();
		return (tok);
	}

%%

/*
 * Open the "main" file (conffile).
 */
int
firstfile(fname)
	const char *fname;
{

	if ((yyin = fopen(fname, "r")) == NULL)
		return (-1);
	yyfile = conffile = fname;
	yyline = 1;
	eoftoken = YY_NULL;
	return (0);
}

/*
 * Open the named file for inclusion at the current point.  Returns 0 on
 * success (file opened and previous state pushed), nonzero on failure
 * (fopen failed, complaint made).  The `ateof' parameter controls the
 * token to be returned at the end of the include file (typically '\n'
 * or ENDFILE).
 */
int
include(fname, ateof)
	const char *fname;
	int ateof;
{
	register FILE *fp;
	register struct incl *in;

	if ((fp = fopen(fname, "r")) == NULL) {
		error("cannot open %s for reading: %s\n",
		    fname, strerror(errno));
		return (-1);
	}
	in = emalloc(sizeof *in);
	in->in_prev = incl;
	in->in_buf = YY_CURRENT_BUFFER;
	in->in_fname = yyfile;
	in->in_lineno = yyline;
	in->in_preveof = eoftoken;
	incl = in;
	yy_switch_to_buffer(yy_create_buffer(fp, YY_BUF_SIZE));
	yyfile = intern(fname);
	yyline = 1;
	eoftoken = ateof;
	return (0);
}

/*
 * Terminate the most recent inclusion.
 */
static void
endinclude()
{
	register struct incl *in;

	if ((in = incl) == NULL)
		panic("endinclude");
	incl = in->in_prev;
	lastfile = yyfile;
	yy_delete_buffer(YY_CURRENT_BUFFER);
	(void)fclose(yyin);
	yy_switch_to_buffer(in->in_buf);
	yyfile = in->in_fname;
	yyline = in->in_lineno;
	eoftoken = in->in_preveof;
	free(in);
}

/*
 * Return the current line number.  If yacc has looked ahead and caused
 * us to consume a newline, we have to subtract one.  yychar is yacc's
 * token lookahead, so we can tell.
 */
int
currentline()
{
	extern int yychar;

	return (yyline - (yychar == '\n'));
}
