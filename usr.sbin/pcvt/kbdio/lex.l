%{
/*
 * Copyright (c) 1994 Joerg Wunsch
 *
 * All rights reserved.
 *
 * This program is free software.
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
 *	This product includes software developed by Joerg Wunsch
 * 4. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ident "$FreeBSD$"

/*
 * $Log: lex.l,v $
 * Revision 1.2  1994/09/18  19:48:45  j
 * Added the symbolic values for kbd cmd byte.
 *
 * Revision 1.1  1994/09/18  12:57:13  j
 * Initial revision
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "y.tab.h"

extern YYSTYPE yylval;

%}

D	[0-9a-fA-F]

%%

all		{ return ALL; }
cmd		{ return CMD; }
data		{ return DATA; }
defaults	{ return DEFAULTS; }
echo		{ return ECHOC; }
enable		{ return ENABLE; }
expr		{ return EXPR; }
help		{ return HELP; }
id		{ return ID; }
led		{ return LED; }
make		{ return MAKE; }
only		{ return ONLY; }
release		{ return RELEASE; }
resend		{ return RESEND; }
reset		{ return RESET; }
scan		{ return SCAN; }
status		{ return STATUS; }
typematic	{ return TYPEMATIC; }
what		{ return WHAT; }

		/* numeric values */
clklow		{ yylval.num = 0x10; return NUM; }
ignpar		{ yylval.num = 0x20; return NUM; }
irq		{ yylval.num = 0x01; return NUM; }
ovrinh		{ yylval.num = 0x08; return NUM; }
scconv		{ yylval.num = 0x40; return NUM; }
test		{ yylval.num = 0x04; return NUM; }

{D}({D}*)\.	{ sscanf(yytext, "%d", &yylval.num); return NUM; }

{D}({D}*)	{ sscanf(yytext, "%x", &yylval.num); return NUM; }

[ \t]		{ /* ignore */ }

\n		{ return NEWLINE; }

.		{ return yytext[0]; }
