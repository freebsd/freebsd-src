%{
/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	$Id: arith_lex.l,v 1.2 1994/09/24 02:57:22 davidg Exp $
 */

#ifndef lint
static char sccsid[] = "@(#)arith_lex.l	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include "arith.tab.h"

extern yylval;
extern char *arith_buf, *arith_startbuf;
int arith_wasoper;
#undef YY_INPUT
#define YY_INPUT(buf,result,max) \
	result = (*buf = *arith_buf++) ? 1 : YY_NULL;
%}

%%
[ \t\n]	{ ; }
[0-9]+	{ arith_wasoper = 0; yylval = atol(yytext); return(ARITH_NUM); }
"("	{ arith_wasoper = 1; return(ARITH_LPAREN); }
")"	{ arith_wasoper = 0; return(ARITH_RPAREN); }
"||"	{ arith_wasoper = 1; return(ARITH_OR); }
"&&"	{ arith_wasoper = 1; return(ARITH_AND); }
"=="	{ arith_wasoper = 1; return(ARITH_EQ); }
">"	{ arith_wasoper = 1; return(ARITH_GT); }
">="	{ arith_wasoper = 1; return(ARITH_GEQ); }
"<"	{ arith_wasoper = 1; return(ARITH_LT); }
"<="	{ arith_wasoper = 1; return(ARITH_LEQ); }
"!="	{ arith_wasoper = 1; return(ARITH_NEQ); }
"*"	{ arith_wasoper = 1; return(ARITH_MULT); }
"/"	{ arith_wasoper = 1; return(ARITH_DIV); }
"%"	{ arith_wasoper = 1; return(ARITH_REM); }
"+"	{ if (!arith_wasoper) {	/* ignore unary plus */
		arith_wasoper = 1; 
		return(ARITH_ADD);
	 } 
	}	
"-"	{ if (arith_wasoper) {
		return(ARITH_UNARYMINUS);
	  } else {
		arith_wasoper = 1;
		return(ARITH_SUBT);
	  }
	}
"!"	{ arith_wasoper = 1; return(ARITH_NOT); }
.	{ error("arith: syntax error: \"%s\"\n", arith_startbuf); }
%%

arith_lex_reset() {
	YY_NEW_FILE;
}
