%{
/*-
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)lang.l	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.sbin/config/lang.l,v 1.27 1999/11/09 07:20:22 peter Exp $
 */

#include <ctype.h>
#include <string.h>
#include "y.tab.h"
#include "config.h"

#define YY_NO_UNPUT

/*
 * Key word table
 */

struct kt {
	char *kt_name;
	int kt_val;
} key_words[] = {
	{ "at",		AT },
	{ "bus",	BUS },
	{ "conflicts",	CONFLICTS },
	{ "config",	CONFIG },
	{ "controller",	CONTROLLER },
	{ "cpu",	CPU },
	{ "device",	DEVICE },
	{ "disable",	DISABLE },
	{ "disk",	DISK },
	{ "drive",	DRIVE },
	{ "drq",	DRQ },
	{ "flags",	FLAGS },
	{ "ident",	IDENT },
	{ "iomem",	IOMEM },
	{ "iosiz",	IOSIZ },
	{ "irq",	IRQ },
	{ "machine",	ARCH }, /* MACHINE is defined in /sys/param.h */
	{ "makeoptions", MAKEOPTIONS },
	{ "maxusers",	MAXUSERS },
	{ "nexus",	NEXUS },
	{ "option",	OPTIONS },
	{ "options",	OPTIONS },
	{ "port",	PORT },
	{ "pseudo-device",PSEUDO_DEVICE },
	{ "tape",	TAPE },
	{ "target",	TARGET },
	{ "unit",	UNIT },
	{ 0, 0 },
};


int kw_lookup __P((char *));
int octal __P((char *));
int hex __P((char *));

%}
WORD	[A-Za-z_][-A-Za-z_]*
ID	[A-Za-z_][-A-Za-z_0-9]*
%START NONUM TOEOL
%%
<NONUM>{WORD}	{
			int i;

			BEGIN 0;
			if ((i = kw_lookup(yytext)) == -1)
			{
				yylval.str = strdup(yytext);
				return ID;
			}
			return i;
		}
<INITIAL>{WORD}/[0-9]* {
			int i;

			if ((i = kw_lookup(yytext)) == -1)
				REJECT;
			if (i == CONTROLLER || i == DEVICE || i == DISK || i == TAPE ||
			    i == PSEUDO_DEVICE || i == AT)
				BEGIN NONUM;
			return i;
		}
<INITIAL>{ID}	{
			BEGIN 0;
			yylval.str = strdup(yytext);
			return ID;
		}
\\\"[^"]+\\\"	{
			BEGIN 0;
			yytext[yyleng-2] = '"';
			yytext[yyleng-1] = '\0';
			yylval.str = strdup(yytext + 1);
			return ID;
		}
\"[^"]+\"	{
			BEGIN 0;
			yytext[yyleng-1] = '\0';
			yylval.str = strdup(yytext + 1);
			return ID;
		}
<TOEOL>[^# \t\n]*	{
			BEGIN 0;
			yylval.str = strdup(yytext);
			return ID;
		}
0[0-7]*		{
			yylval.val = octal(yytext);
			return NUMBER;
		}
0x[0-9a-fA-F]+	{
			yylval.val = hex(yytext);
			return NUMBER;
		}
-?[1-9][0-9]*	{
			yylval.val = atoi(yytext);
			return NUMBER;
		}
[0-9]"."[0-9]*	{
			yylval.val = (int) (60 * atof(yytext) + 0.5);
			return FPNUMBER;
		}
"?"		{
			yylval.val = -1;
			return NUMBER;
		}
\n/[ \t]	{
			yyline++;
		}
\n		{
			yyline++;
			return SEMICOLON;
		}
#.*		{	/* Ignored (comment) */;	}
[ \t\f]*	{	/* Ignored (white space) */;	}
";"		{	return SEMICOLON;		}
","		{	return COMMA;			}
"="		{	BEGIN TOEOL; return EQUALS;	}
"@"		{	return AT;			}
.		{	return yytext[0];		}

%%
/*
 * kw_lookup
 *	Look up a string in the keyword table.  Returns a -1 if the
 *	string is not a keyword otherwise it returns the keyword number
 */

int
kw_lookup(word)
register char *word;
{
	register struct kt *kp;

	for (kp = key_words; kp->kt_name != 0; kp++)
		if (eq(word, kp->kt_name))
			return kp->kt_val;
	return -1;
}

/*
 * Number conversion routines
 */

int
octal(str)
char *str;
{
	int num;

	(void) sscanf(str, "%o", &num);
	return num;
}

int
hex(str)
char *str;
{
	int num;

	(void) sscanf(str+2, "%x", &num);
	return num;
}
