/*
 * (C) Copyright David Gibson <dwg@au1.ibm.com>, IBM Corporation.  2005, 2008.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *                                                                   USA
 */

%option noyywrap nounput noinput never-interactive

%x BYTESTRING
%x PROPNODENAME

PROPNODECHAR	[a-zA-Z0-9,._+*#?@-]
PATHCHAR	({PROPNODECHAR}|[/])
LABEL		[a-zA-Z_][a-zA-Z0-9_]*
STRING		\"([^\\"]|\\.)*\"
WS		[[:space:]]
COMMENT		"/*"([^*]|\*+[^*/])*\*+"/"
LINECOMMENT	"//".*\n
GAP		({WS}|{COMMENT}|{LINECOMMENT})*

%{
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <errno.h>
#include <assert.h>
#include <fnmatch.h>

#include "srcpos.h"
#include "util.h"

static int v1_tagged; /* = 0 */
static int cbase = 16;
static int saw_hyphen; /* = 0 */
static unsigned long long last_val;
static char *last_name; /* = NULL */

static const struct {
	const char *pattern;
	int obase, width;
} guess_table[] = {
	{ "*-frequency", 10, 0 },
	{ "num-*", 10, 0 },
	{ "#*-cells", 10, 0 },
	{ "*cache-line-size", 10, 0 },
	{ "*cache-block-size", 10, 0 },
	{ "*cache-size", 10, 0 },
	{ "*cache-sets", 10, 0 },
	{ "cell-index", 10, 0 },
	{ "bank-width", 10, 0 },
	{ "*-fifo-size", 10, 0 },
	{ "*-frame-size", 10, 0 },
	{ "*-channel", 10, 0 },
	{ "current-speed", 10, 0 },
	{ "phy-map", 16, 8 },
	{ "dcr-reg", 16, 3 },
	{ "reg", 16, 8 },
	{ "ranges", 16, 8},
};
%}

%%
<*>"/include/"{GAP}{STRING}	ECHO;

<*>\"([^\\"]|\\.)*\"	ECHO;

<*>"/dts-v1/"	{
			die("Input dts file is already version 1\n");
		}

<*>"/memreserve/"	{
			if (!v1_tagged) {
				fprintf(yyout, "/dts-v1/;\n\n");
				v1_tagged = 1;
			}

			ECHO;
			BEGIN(INITIAL);
		}

<*>{LABEL}:		ECHO;

<INITIAL>[bodh]# {
			if (*yytext == 'b')
				cbase = 2;
			else if (*yytext == 'o')
				cbase = 8;
			else if (*yytext == 'd')
				cbase = 10;
			else
				cbase = 16;
		}

<INITIAL>[0-9a-fA-F]+	{
			unsigned long long val;
			int obase = 16, width = 0;
			int i;

			val = strtoull(yytext, NULL, cbase);

			if (saw_hyphen)
				val = val - last_val + 1;

			if (last_name) {
				for (i = 0; i < ARRAY_SIZE(guess_table); i++)
					if (fnmatch(guess_table[i].pattern,
					    last_name, 0) == 0) {
						obase = guess_table[i].obase;
						width = guess_table[i].width;
					}
			} else {
				obase = 16;
				width = 16;
			}

			if (cbase != 16)
				obase = cbase;

			switch (obase) {
			case 2:
			case 16:
				fprintf(yyout, "0x%0*llx", width, val);
				break;
			case 8:
				fprintf(yyout, "0%0*llo", width, val);
				break;
			case 10:
				fprintf(yyout, "%*llu", width, val);
				break;
			}

			cbase = 16;
			last_val = val;
			saw_hyphen = 0;
		}

\&{LABEL}		ECHO;

"&{/"{PATHCHAR}+\}	ECHO;

<INITIAL>"&/"{PATHCHAR}+ fprintf(yyout, "&{/%s}", yytext + 2);

<BYTESTRING>[0-9a-fA-F]{2} ECHO;

<BYTESTRING>"]"	{
			ECHO;
			BEGIN(INITIAL);
		}

<PROPNODENAME>{PROPNODECHAR}+ {
			ECHO;
			last_name = xstrdup(yytext);
			BEGIN(INITIAL);
		}

<*>{GAP}	ECHO;

<*>-		{	/* Hack to convert old style memreserves */
			saw_hyphen = 1;
			fprintf(yyout, " ");
		}

<*>.		{
			if (!v1_tagged) {
				fprintf(yyout, "/dts-v1/;\n\n");
				v1_tagged = 1;
			}

			ECHO;
			if (yytext[0] == '[') {
				BEGIN(BYTESTRING);
			}
			if ((yytext[0] == '{')
			    || (yytext[0] == ';')) {
				BEGIN(PROPNODENAME);
			}
		}

%%
/* Usage related data. */
static const char usage_synopsis[] = "convert-dtsv0 [options] <v0 dts file>...";
static const char usage_short_opts[] = "" USAGE_COMMON_SHORT_OPTS;
static struct option const usage_long_opts[] = {
	USAGE_COMMON_LONG_OPTS
};
static const char * const usage_opts_help[] = {
	USAGE_COMMON_OPTS_HELP
};

static void convert_file(const char *fname)
{
	const char suffix[] = "v1";
	int len = strlen(fname);
	char *newname;

	newname = xmalloc(len + sizeof(suffix));
	memcpy(newname, fname, len);
	memcpy(newname + len, suffix, sizeof(suffix));

	yyin = fopen(fname, "r");
	if (!yyin)
		die("Couldn't open input file %s: %s\n",
		    fname, strerror(errno));

	yyout = fopen(newname, "w");
	if (!yyout)
		die("Couldn't open output file %s: %s\n",
		    newname, strerror(errno));

	while(yylex())
		;

	free(newname);
}

int main(int argc, char *argv[])
{
	int opt;
	int i;

	while ((opt = util_getopt_long()) != EOF) {
		switch (opt) {
		case_USAGE_COMMON_FLAGS
		}
	}
	if (argc < 2)
		usage("missing filename");

	for (i = 1; i < argc; i++) {
		fprintf(stderr, "Converting %s from dts v0 to dts v1\n", argv[i]);
		convert_file(argv[i]);
	}

	exit(0);
}
