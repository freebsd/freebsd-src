%{
/*
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *	@(#)fsi_lex.l	8.2 (Berkeley) 2/17/94
 *
 * $Id: fsi_lex.l,v 5.2.2.1 1992/02/09 15:09:36 jsp beta $
 *
 */

/*
 * Lexical analyzer for fsinfo.
 * TODO: Needs rewriting.
 */

static int xinput();
static void xunput();

#ifdef FLEX_SCANNER
static int yylineno;
/* Flex support with help from Vern Paxson <vern@helios.ee.lbl.gov> */
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size)		\
{						\
	int i;					\
	for (i = 0; i < max_size; i++) {	\
		int ch = xinput(i == 0);	\
		if (ch == 0)			\
			break;			\
		buf[i] = ch;			\
	}					\
	result = i;				\
}

#define	INIT_STATE {				\
		switch ((yy_start - 1) / 2) {	\
		case 0:				\
			BEGIN F;		\
			break;			\
		}				\
}


#else
/*
 * Using old lex...
 */
#undef unput
#define unput(ch) xunput(ch)
#undef input
#define input() xinput(1)

#define	INIT_STATE {				\
		switch (yybgin - yysvec - 1) {	\
		case 0:				\
			BEGIN F;		\
			break;			\
		}				\
}

#endif /* FLEX_SCANNER */

#include "../fsinfo/fsinfo.h"
#include "fsi_gram.h"
#include <ctype.h>

static char *filename;
static char *optr;
static char ostr[1024];
static find_resword();
static unsigned char ibuf[64];
static unsigned char *iptr = ibuf;
static int quoted;
static int lastch, nextch = '\n';
YYSTYPE yylval;

struct r {
	char *rw;
	int tok;
} rr[] = {
	{ "->", tEQ },
	{ "arch", tARCH },
	{ "as", tAS },
	{ "automount", tAUTOMOUNT },
	{ "cluster", tCLUSTER },
	{ "config", tCONFIG },
	{ "dumpset", tDUMPSET },
	{ "exportfs", tEXPORTFS },
	{ "freq", tFREQ },
	{ "from", tFROM },
	{ "fs", tFS },
	{ "fstype", tFSTYPE },
	{ "host", tHOST },
	{ "hwaddr", tHWADDR },
	{ "inaddr", tINADDR },
	{ "localhost", tLOCALHOST },
	{ "log", tLOG },
	{ "mount", tMOUNT },
	{ "netif", tNETIF },
	{ "netmask", tNETMASK },
	{ "opts", tOPTS },
	{ "os", tOS },
	{ "passno", tPASSNO },
	{ "sel", tSEL },
	{ "volname", tVOLNAME },
	{ 0, 0 },
};
#define	NRES_WORDS (sizeof(rr)/sizeof(rr[0])-1)

%}

%start F Q

%%
		INIT_STATE; /* witchcraft */

<F>[^ \t\n"={}]+	{ return find_resword(yytext); }
<F>[ \t]		;
<F>"\n"			{ yylineno++; }
<F>[={}]		{ return *yytext; }

<F>\"			{ BEGIN Q; optr = ostr; quoted = 1; }
<Q>\n			{ yylineno++; yyerror("\" expected"); BEGIN F; }
<Q>\\b			{ *optr++ = '\b'; /* escape */ }
<Q>\\t			{ *optr++ = '\t'; /* escape */ }
<Q>\\\"			{ *optr++ = '\"'; /* escape */ }
<Q>\\\\			{ *optr++ = '\\'; /* escape */ }
<Q>\\\n			{ yylineno++; /* continue */ }
<Q>\\r			{ *optr++ = '\r'; /* escape */ }
<Q>\\n			{ *optr++ = '\n'; /* escape */ }
<Q>\\f			{ *optr++ = '\f'; /* escape */ }
<Q>"\\ "		{ *optr++ = ' '; /* force space */ }
<Q>\\.			{ yyerror("Unknown \\ sequence"); }
<Q>([ \t]|"\\\n"){2,}	{ char *p = yytext-1; while (p = strchr(p+1, '\n')) yylineno++; }
<Q>\"			{ BEGIN F; quoted = 0;
				*optr = '\0';
				yylval.s = strdup(ostr);
				return tSTR;
			}
<Q>.			{ *optr++ = *yytext; }

%%

static int find_resword(s)
char *s;
{
	int tok = 0;

	int l = 0, m = NRES_WORDS/2, h = NRES_WORDS-1;
	int rc = 0;
	
	m = NRES_WORDS/2;

#define FSTRCMP(p, q) ((*(p) == *(q)) ? strcmp((p)+1, (q)+1) : *(p) - *(q))

	while ((l <= h) && (rc = FSTRCMP(s, rr[m].rw))) {
		/*fprintf(stderr, "failed to cmp(%s, %s), %d, %d, %d\n", s, rr[m].rw, l, m, h);*/
		if (rc < 0)
			h = m - 1;
		else
			l = m + 1;
		m = (h + l) / 2;
	}

	if (rc == 0)
		tok = rr[m].tok;

	switch (tok) {
	case tLOCALHOST:
		s = "${host}";
		/* fall through... */
	case 0:
		yylval.s = strdup(s);
		tok = tSTR;
		/* fall through... */
	default:
		return tok;
	}

}

int yyerror(s, s1, s2, s3, s4)
char *s;
char *s1, *s2, *s3, *s4;
{
	col_cleanup(0);
	fprintf(stderr, "%s:%d: ", filename ? filename : "/dev/stdin", yylineno);
	fprintf(stderr, s, s1, s2, s3, s4);
	fputc('\n', stderr);
	parse_errors++;
}

ioloc *current_location()
{
	ioloc *ip = ALLOC(ioloc);
	ip->i_line = yylineno;
	ip->i_file = filename;
	return ip;
}

#ifdef FLEX_SCANNER
#undef yywrap
#endif

int yywrap()
{
static int first = 1;
	if (first) {
		char prog[16*1024];
		strcpy(prog, "for file in ");
		while (*++g_argv) {
			if (access(*g_argv, 4) < 0) {
				error("\"%s\": Cannot open for reading", *g_argv);
				file_io_errors++;
			} else {
				strcat(prog, *g_argv);
				strcat(prog, " ");
			}
		}
		strcat(prog, "; do /lib/cpp ");
		strcat(prog, idvbuf);
		strcat(prog, " -DHOSTNAME=\'");
		strcat(prog, hostname);
		strcat(prog, "\' \"$file\"; done");
		yyin = popen(prog, "r");
		if (yyin) {
			/*if (filename) free(filename);*/
			filename = strdup("unknown");
			yylineno = 1;
			first = 0;
			return 0;
		} else {
			perror(prog);
		}
	}

	if (!first && yyin && pclose(yyin) != 0)
		parse_errors++;

	return 1;
}

#define xgetc(fp) ((iptr > ibuf) ? (*--iptr) : (lastch = nextch, nextch = getc(fp), (nextch == EOF ? nextch = lastch, EOF : nextch)))

static int xinput(need)
int need;
{
static int c_comment = 0;
	int ch, ch2;

	do {
		ch = xgetc(yyin);
		/* fprintf(stderr, "ch = %c, %#x, %d\n", ch, ibuf,iptr-ibuf); */
		if (ch == EOF) return 0;
		if (quoted)
			return ch;
		if (c_comment) {
			ch2 = ch;
			do {
				if (ch2 == '\n') {
					nextch = '\n';
					return ch2;
				}
				/* C style comment */
				do {
					ch2 = getc(yyin);
					if (ch2 == '\n') {
						nextch = '\n';
						return ch2;
					}
				} while (ch2 != '*' && ch2 != EOF);

				while (ch2 == '*')
					ch2 = getc(yyin);
			} while (ch2 != '/' && ch2 != EOF);
			c_comment = 0;
			if (ch2 == EOF)
				break;
			continue;
		}

		if (ch == '#') {
			/*log("lastch = '%c' (%#x)", lastch, lastch);*/
			if (lastch == '\n') {
				char fname[MAXPATHLEN];
				char *fptr;
				if (!need) {
					xunput('#');
					nextch = '\n';
					return 0;
				}
				fname[0] = '\0';
				/* Skip past space */
				do {
					ch2 = getc(yyin);
				} while (ch2 != EOF && ch2 != '\n' && !isdigit(ch2));
				if (isdigit(ch2)) {
					/* Read in line number */
					fptr = fname;
					do {
						*fptr++ = ch2;
						ch2 = getc(yyin);
					} while (isdigit(ch2));
					*fptr = '\0';
					if (fptr != fname)
						yylineno = atoi(fname) - 1;
				}
				/* Skip past space */
				while (ch2 != EOF && ch2 != '\"' && ch2 != '\n')
					ch2 = getc(yyin);
				if (ch2 == '\"') {
					/* Read file name */
					fptr = fname;
					ch2 = getc(yyin);
					while (ch2 != '\"' && ch2 != EOF && ch2 != EOF) {
						*fptr++ = ch2;
						ch2 = getc(yyin);
					}
					*fptr = '\0';
					if (fname[0]) {
						log("Setting filename to \"%s\"", fname);
						/*if (filename) free(filename);*/
						filename = strdup(fname);
					}
				}
				while (ch2 != '\n' && ch2 != EOF)
					ch2 = getc(yyin);
			} else do {
				ch2 = getc(yyin);
			} while (ch2 != '\n' && ch2 != EOF);
			if (ch2 == '\n') {
				nextch = '\n';
				return ch2;
			}
		} else if (ch == '/') {
			ch2 = getc(yyin);
			if (ch2 == '/') {
				/* C++ style comment */
				do {
					ch2 = getc(yyin);
				} while (ch2 != '\n' && ch2 != EOF);
				if (ch2 == '\n') {
					nextch = '\n';
					return ch2;
				}
			} else if (ch2 == '*') {
				c_comment = 1;
				continue;
			} else {
				xunput(ch2);
				return ch;
			}
		} else {
			return ch;
		}
	} while (ch2 != EOF);
	error("End of file within comment");
	return 0;
}

static void xunput(c)
int c;
{
	if (c && c != EOF) {
		if (iptr == ibuf + sizeof(ibuf) - 1)
		 	fatal("Out of space in lexical pushback");
		*iptr++ = c;
	}
}
