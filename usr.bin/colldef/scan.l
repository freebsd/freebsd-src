%x string name charmap
%{
/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: scan.l,v 1.1.1.1 1995/02/17 17:29:50 ache Exp $
 */

#include <err.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>
#include "collate.h"
#include "parse.tab.h"

int line_no = 1;
u_char buf[STR_LEN], *ptr;
FILE *map_fp;
YY_BUFFER_STATE main_buf, map_buf;
#ifdef FLEX_DEBUG
YYSTYPE yylval;
#endif /* FLEX_DEBUG */
%}
%%
<INITIAL,charmap>[ \t]	;
\"			{ ptr = buf; BEGIN(string); }
\<			{ ptr = buf; BEGIN(name); }
^#.*\n			line_no++;
^\n			line_no++;
\\\n			line_no++;
\\t			{ yylval.ch = '\t'; return CHAR; }
\\n			{ yylval.ch = '\n'; return CHAR; }
\\b			{ yylval.ch = '\b'; return CHAR; }
\\f			{ yylval.ch = '\f'; return CHAR; }
\\v			{ yylval.ch = '\v'; return CHAR; }
\\r			{ yylval.ch = '\r'; return CHAR; }
\\a			{ yylval.ch = '\a'; return CHAR; }
\\.			{ yylval.ch = yytext[1]; return CHAR; }
<INITIAL,charmap>\n			{ line_no++; return '\n'; }
[;,{}()]		return *yytext;
substitute		return SUBSTITUTE;
with			return WITH;
order			return ORDER;
charmap			BEGIN(charmap);
;[ \t]*\.\.\.[ \t]*;		return RANGE;
\\[0-7]{3}		{
	u_int v;

	sscanf(&yytext[1], "%o", &v);
	yylval.ch = (u_char)v;
	return CHAR;
}
\\x[0-9a-z]{2}		{
	u_int v;

	sscanf(&yytext[2], "%x", &v);
	yylval.ch = (u_char)v;
	return CHAR;
}
[^;,{}() \t\n"<]+	{
	if(yyleng == 1) {
		yylval.ch = *yytext;
		return CHAR;
	}
	if(yyleng > STR_LEN - 1)
		errx(EX_UNAVAILABLE, "chain buffer overflaw near line %u",
		     line_no);
	strcpy(yylval.str, yytext);
	return CHAIN;
}
<name>\\\>		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '>';
}
<string>\\\"		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '"';
}
<name>\>		{
	*ptr = '\0';
	strcpy(yylval.str, buf);
	BEGIN(INITIAL);
	return NAME;
}
<string>\"		{
	*ptr = '\0';
	strcpy(yylval.str, buf);
	BEGIN(INITIAL);
	return STRING;
}
<name,string>.		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = *yytext;
}
<name,string>\\t		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '\t';
}
<name,string>\\b		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '\b';
}
<name,string>\\f		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '\f';
}
<name,string>\\v		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '\v';
}
<name,string>\\n		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '\n';
}
<name,string>\\r		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '\r';
}
<name,string>\\a		{
	if(ptr >= buf + sizeof(buf) - 1)
		errx(EX_UNAVAILABLE, "name/string buffer overflaw near line %u",
		     line_no);
	*ptr++ = '\a';
}
<name,string><<EOF>>	{
	errx(EX_UNAVAILABLE, "unterminated name/string near line %u", line_no);
}
<name,string>\\x[0-9a-f]{2}	{
	u_int v;

	sscanf(&yytext[2], "%x", &v);
	*ptr++ = (u_char)v;
}
<name,string>\\[0-7]{3}	{
	u_int v;

	sscanf(&yytext[1], "%o", &v);
	*ptr++ = (u_char)v;
}
<charmap>[^ \t\n]+	{
	if((map_fp = fopen(yytext, "r")) == 0)
		err(EX_UNAVAILABLE, "can't open charmap file %s near line %u",
		    yytext, line_no);
	map_buf = yy_new_buffer(map_fp, YY_BUF_SIZE);
	main_buf = YY_CURRENT_BUFFER;
	yy_switch_to_buffer(map_buf);
	BEGIN(INITIAL);
}
<charmap><<EOF>>	{
	errx(EX_UNAVAILABLE, "charmap file name expected near line %u",
	     line_no);
}
<<EOF>>			{
	if(map_fp) {
		yy_switch_to_buffer(main_buf);
		yy_delete_buffer(map_buf);
		fclose(map_fp);
		map_fp = 0;
	}
	else
		yyterminate();
}
%%
#ifdef FLEX_DEBUG
main()
{
	while(yylex())
		;
	return 0;
}
#endif /* FLEX_DEBUG */
