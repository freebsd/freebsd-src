%{
/*
 * Copyright (c) 2015, Carsten Kunze
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <string.h>
#include "dhtml.h"
#include "tr_out.h"
#include "char.h"
#include "main.h"
static int i0;
%}
%x X FONT_NUM FONT_NAM T X_X LINK ULINK ANCHOR
WS  [ ]
NWS [^ \n]
%%
<X>f{NWS}*{WS}+		{ BEGIN FONT_NUM; }
<X>T{NWS}*{WS}+		{ BEGIN T; }
<X>X{NWS}*{WS}+		{ BEGIN X_X; }
<X>[^fTX].*		{ BEGIN 0; }
<FONT_NUM>[0-9]+{WS}+	{ i0 = atoi(yytext);
			  BEGIN FONT_NAM; }
<FONT_NAM>{NWS}+	{ out_x_f(i0, yytext);
			  BEGIN 0; }
<T>{NWS}+		{ out_x_T(yytext);
			  BEGIN 0; }
<X_X>Link{WS}+		{ BEGIN LINK; }
<X_X>ULink{WS}+		{ BEGIN ULINK; }
<X_X>U?Link{WS}*\n	{ out_end_link();
			  BEGIN 0; }
<X_X>Anchor{WS}+	{ BEGIN ANCHOR; }
<X_X>LC_TYPE.*		{ BEGIN 0; }
<X_X>[^AL].*		{ BEGIN 0; }
<LINK>.+		{ out_begin_link(yytext);
			  BEGIN 0; }
<ULINK>.+		{ out_begin_ulink(yytext);
			  BEGIN 0; }
<ANCHOR>.+		{ out_anchor(yytext);
			  BEGIN 0; }
c.			{ char_c(yytext[1]); }
[0-9][0-9].		{ char_c(yytext[2]); }
C{NWS}+			{ char_C(yytext+1); }
w			{ out_w(); }
H-?[0-9]+		{ ; }
h-?[0-9]+		{ out_h(atoi(yytext+1)); }
V-?[0-9]+		{ out_V(atoi(yytext+1)); }
v-?[0-9]+		{ fprintf(stderr, "Ignore v %s\n", yytext+1); }
f[0-9]+			{ out_f(atoi(yytext+1)); }
N[0-9]+			{ char_N(atoi(yytext+1)); }
n[0-9]+{WS}+[0-9]+	{ out_n(atoi(yytext+1)); }
s[0-9]+			{ out_s(atoi(yytext+1)); }
p[0-9]+			{ ; }
x{WS}+			{ BEGIN X; }
D.+			{ ; }
\n			{ ; }
.			{ fprintf(stderr, "Ignore '%c'\n", *yytext); }
%%
int
yywrap(void) {
	return 1;
}
void
run_lex(void) {
	yylex();
}
