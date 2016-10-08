%{
/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/pic/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)picl.l	1.9 (gritter) 8/6/06	*/
%}
%Start A str def xsc br thru sh
%e 1700
%k 150
%a 1800
%o 1600
%p 5000
%n 700

%{
#ifndef	FLEX_SCANNER
#undef	input
#undef	unput
#endif	/* !FLEX_SCANNER */
/* #include <stdio.h> lex puts one out for us */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "pic.h"
#include "y.tab.h"

#ifdef	FLEX_SCANNER
#undef	YY_INPUT
#define	YY_INPUT(buf, result, max_size) { \
	int	c = xxinput(); \
	result = (c == EOF || c == 0) ? YY_NULL : (buf[0] = c, 1); \
}
#define	witchcraft	YY_START
#else	/* !FLEX_SCANNER */
#define	witchcraft	yybgin-yysvec-1
#endif	/* !FLEX_SCANNER */

extern	char	*filename;
extern	struct	symtab	symtab[];

void	pbstr(char *);
void	dodef(struct symtab *stp);
void	undefine(char *s);
void	shell_init(void), shell_exec(void), shell_text(char *);
void	endfor(void);

int	yyback(int *, int);
int	yylook(void);
int	yywrap(void);

#define	CADD	cbuf[clen++]=yytext[0]; \
		if (clen>=CBUFLEN-1) { WARNING("string too long", cbuf); BEGIN A; }
#define	CBUFLEN	500
char	cbuf[CBUFLEN];
int	c, clen, cflag, delim;
int	ifsw	= 0;	/* 1 if if statement in progress */
%}

A	[a-zA-Z_]
B	[a-zA-Z0-9_]
D	[0-9]
WS	[ \t]
FWS	([ \t]|\\\n)

%%
	switch (witchcraft) {
	case 0:
		BEGIN A;
		break;
	case xsc:
		BEGIN A;
		return('}');
	case br:
		BEGIN A;
		return(']');
	}

<A>{WS}		;
<A>"\\"\n	;
<A>\n		{ return(ST); }
<A>";"		{ return(ST); }
<A>"}"		{ BEGIN xsc; return(ST); }
<A>"]"		{ BEGIN br; return(ST); }
<A>"{"{WS}*(#.*)?\n+	{ return(yylval.i = yytext[0]); }

<A>^"."{WS}*"PS".*	{ if (curfile == infile) WARNING(".PS found inside .PS/.PE"); }
<A>^"."{WS}*"PE".*	{ if (curfile == infile) {
			yylval.p = PEstring = tostring(yytext);
			return(EOF);
		  }
		}
<A>^['.].*	{ yylval.p = tostring(yytext); return(TROFF); }

<A>print	return(yylval.i = PRINT);
<A>box		return(yylval.i = BOX);
<A>circle	return(yylval.i = CIRCLE);
<A>arc		return(yylval.i = ARC);
<A>ellipse	return(yylval.i = ELLIPSE);
<A>arrow	return(yylval.i = ARROW);
<A>spline	return(yylval.i = SPLINE);
<A>line		return(yylval.i = LINE);
<A>move		return(yylval.i = MOVE);
<A>"[]"		return(yylval.i = BLOCK);
<A>reset	return(RESET);
<A>sprintf	return(SPRINTF);

<A>same		return(SAME);
<A>between	return(BETWEEN);
<A>and		return(AND);

<A>of		;
<A>the		;
<A>way		;

<A>"."(e|east)		{ yylval.i = EAST; return(CORNER); }
<A>"."(r|right)		{ yylval.i = EAST; return(CORNER); }
<A>"."(w|west)		{ yylval.i = WEST; return(CORNER); }
<A>"."(l|left)		{ yylval.i = WEST; return(CORNER); }
<A>"."(n|north)		{ yylval.i = NORTH; return(CORNER); }
<A>"."(t|top)		{ yylval.i = NORTH; return(CORNER); }
<A>"."(s|south)		{ yylval.i = SOUTH; return(CORNER); }
<A>"."(b|bot|bottom)	{ yylval.i = SOUTH; return(CORNER); }
<A>"."(c|center)	{ yylval.i = CENTER; return(CORNER); }
<A>".start"		{ yylval.i = START; return(CORNER); }
<A>".end"		{ yylval.i = END; return(CORNER); }
<A>".ne"		{ yylval.i = NE; return(CORNER); }
<A>".se"		{ yylval.i = SE; return(CORNER); }
<A>".nw"		{ yylval.i = NW; return(CORNER); }
<A>".sw"		{ yylval.i = SW; return(CORNER); }

<A>top{FWS}+of		{ yylval.i = NORTH; return(CORNER); }
<A>north{FWS}+of	{ yylval.i = NORTH; return(CORNER); }
<A>bottom{FWS}+of	{ yylval.i = SOUTH; return(CORNER); }
<A>south{FWS}+of	{ yylval.i = SOUTH; return(CORNER); }
<A>left{FWS}+of		{ yylval.i = WEST; return(CORNER); }
<A>west{FWS}+of		{ yylval.i = WEST; return(CORNER); }
<A>right{FWS}+of	{ yylval.i = EAST; return(CORNER); }
<A>east{FWS}+of		{ yylval.i = EAST; return(CORNER); }
<A>center{FWS}+of	{ yylval.i = CENTER; return(CORNER); }
<A>start{FWS}+of	{ yylval.i = START; return(CORNER); }
<A>end{FWS}+of		{ yylval.i = END; return(CORNER); }
<A>upper{FWS}right{FWS}+of	{ yylval.i = NE; return(CORNER); }
<A>lower{FWS}right{FWS}+of	{ yylval.i = SE; return(CORNER); }
<A>upper{FWS}left{FWS}+of	{ yylval.i = NW; return(CORNER); }
<A>lower{FWS}left{FWS}+of	{ yylval.i = SW; return(CORNER); }

<A>height|ht	{ yylval.i = HEIGHT; return(ATTR); }
<A>width|wid	{ yylval.i = WIDTH; return(ATTR); }
<A>radius|rad	{ yylval.i = RADIUS; return(ATTR); }
<A>diameter|diam { yylval.i = DIAMETER; return(ATTR); }
<A>size		{ yylval.i = SIZE; return(ATTR); }
<A>left		{ yylval.i = LEFT; return(DIR); }
<A>right	{ yylval.i = RIGHT; return(DIR); }
<A>up		{ yylval.i = UP; return(DIR); }
<A>down		{ yylval.i = DOWN; return(DIR); }
<A>cw		{ yylval.i = CW; return(ATTR); }
<A>clockwise	{ yylval.i = CW; return(ATTR); }
<A>ccw		{ yylval.i = CCW; return(ATTR); }
<A>invis(ible)?	{ yylval.i = INVIS; return(ATTR); }
<A>noedge	{ yylval.i = INVIS; return ATTR; }
<A>fill		return(yylval.i = FILL);
<A>solid	;
<A>dot(ted)?	return(yylval.i = DOT);
<A>dash(ed)?	return(yylval.i = DASH);
<A>chop		return(yylval.i = CHOP);

<A>spread	{ yylval.i = SPREAD; return TEXTATTR; }
<A>ljust	{ yylval.i = LJUST; return TEXTATTR; }
<A>rjust	{ yylval.i = RJUST; return TEXTATTR; }
<A>above	{ yylval.i = ABOVE; return TEXTATTR; }
<A>below	{ yylval.i = BELOW; return TEXTATTR; }
<A>center	{ yylval.i = CENTER; return TEXTATTR; }

<A>"<-"		{ yylval.i = HEAD1; return(HEAD); }
<A>"->"		{ yylval.i = HEAD2; return(HEAD); }
<A>"<->"	{ yylval.i = HEAD12; return(HEAD); }

<A>".x"			return(yylval.i = DOTX);
<A>".y"			return(yylval.i = DOTY);
<A>"."(ht|height)	return(yylval.i = DOTHT);
<A>"."(wid|width)	return(yylval.i = DOTWID);
<A>"."(rad|radius)	return(yylval.i = DOTRAD);

<A>from		return(yylval.i = FROM);
<A>to		return(yylval.i = TO);
<A>at		return(yylval.i = AT);
<A>by		return(yylval.i = BY);
<A>with		return(yylval.i = WITH);
<A>last		return(yylval.i = LAST);

<A>log		return(LOG);
<A>exp		return(EXP);
<A>sin		return(SIN);
<A>cos		return(COS);
<A>atan2	return(ATAN2);
<A>sqrt		return(SQRT);
<A>rand		return(RAND);
<A>max		return(MAX);
<A>min		return(MIN);
<A>int		return(INT);

<A>"=="		return(EQ);
<A>">="		return(GE);
<A>"<="		return(LE);
<A>"!="		return(NEQ);
<A>">"		return(GT);
<A>"<"		return(LT);
<A>"&&"		return(ANDAND);
<A>"||"		return(OROR);
<A>"!"		return(NOT);	

<A>Here		return(yylval.i = HERE);

<A>for		return(FOR);
<A>^Endfor\n	{ endfor(); }
<A>do		{ yylval.p = delimstr("loop body"); return(DOSTR); }

<A>copy|include		return(COPY);
<A>(thru|through){WS}+	{ BEGIN thru; return(THRU); }
<thru>{A}{B}*|.		{ yylval.st = copythru(yytext); BEGIN A; return(DEFNAME); }
<A>until		return(UNTIL);

<A>if		{ ifsw = 1; return(IF); }
<A>then		{ if (!ifsw) { yylval.i = THEN; return(ATTR); }
		  yylval.p = delimstr("then part"); ifsw = 0;
		  return(THENSTR); }
<A>else		{ yylval.p = delimstr("else part"); return(ELSESTR); }

<A>sh{WS}+	{ BEGIN sh;
		  if ((delim = input()) == '{') delim = '}';	/* no nested {} */
		  shell_init(); }
<sh>{A}{B}*	{ struct symtab *p;
		  if (yytext[0] == delim) {
			shell_exec();
			BEGIN A;
		  } else {
			p = lookup(yytext);
			if (p != NULL && p->s_type == DEFNAME) {
				c = input();
				xxunput(c);
				if (c == '(')
					dodef(p);
				else
					pbstr(p->s_val.p);
			} else
				shell_text(yytext);
		  }
		}
<sh>.|\n	{ if (yytext[0] == delim) {
			shell_exec();
			BEGIN A;
		  } else
			shell_text(yytext);
		}

<A>define{WS}+		{ BEGIN def; }
<def>{A}{B}*		{ definition(yytext); BEGIN A; }
<A>undef(ine)?{WS}+{A}{B}*	{ undefine(yytext); }

<A>first		{ yylval.i = 1; return(NTH); }
<A>{D}+(th|nd|rd|st)	{ yylval.i = atoi(yytext); return(NTH); }
<A>({D}+("."?){D}*|"."{D}+)((e|E)("+"|-)?{D}+)?i? {
		  	yylval.f = atof(yytext); return(NUMBER); }

<A>{A}{B}* {	struct symtab *p;
		p = lookup(yytext);
		if (p != NULL && p->s_type == DEFNAME) {
			c = input();
			xxunput(c);
			if (c == '(')	/* it's name(...) */
				dodef(p);
			else {	/* no argument list */
				pbstr(p->s_val.p);
				dprintf("pushing back `%s'\n", p->s_val.p);
			}
		} else if (islower((int)yytext[0])) {
			yylval.p = tostring(yytext);
			return(VARNAME);
		} else {
			yylval.p = tostring(yytext);
			return(PLACENAME);
		}
	}

<A>\"		{ BEGIN str; clen=0; }
<str>\"		{ cbuf[clen]=0; yylval.p = tostring(cbuf); BEGIN A; return(TEXT); }
<str>\n		{ cbuf[clen]=0; WARNING("missing quote in string \"%s\"", cbuf);
				BEGIN A; return(ST); }
<str>"\\\""	{ cbuf[clen++]='"'; }
<str>"\\"t	{ cbuf[clen++]='\t'; }
<str>"\\\\"	{ cbuf[clen++]='\\'; }
<str>.		{ CADD; }

<A>#.*		;

<A>.		return(yylval.i = yytext[0]);

%%

#ifdef	FLEX_SCANNER
void xxcruft(void)
{
	unput(0);
}
#endif	/* FLEX_SCANNER */
