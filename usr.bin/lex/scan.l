/* scan.l - scanner for flex input */

%{
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Vern Paxson.
 * 
 * The United States Government has rights in this work pursuant
 * to contract no. DE-AC03-76SF00098 between the United States
 * Department of Energy and the University of California.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.bin/lex/scan.l,v 1.1.1.1 1994/08/24 13:10:31 csgr Exp $ */

#include "flexdef.h"
#include "parse.h"

#define ACTION_ECHO add_action( yytext )
#define MARK_END_OF_PROLOG mark_prolog();

#define YY_DECL \
	int flexscan()

#define RETURNCHAR \
	yylval = (unsigned char) yytext[0]; \
	return CHAR;

#define RETURNNAME \
	strcpy( nmstr, yytext ); \
	return NAME;

#define PUT_BACK_STRING(str, start) \
	for ( i = strlen( str ) - 1; i >= start; --i ) \
		unput((str)[i])

#define CHECK_REJECT(str) \
	if ( all_upper( str ) ) \
		reject = true;

#define CHECK_YYMORE(str) \
	if ( all_lower( str ) ) \
		yymore_used = true;
%}

%x SECT2 SECT2PROLOG SECT3 CODEBLOCK PICKUPDEF SC CARETISBOL NUM QUOTE
%x FIRSTCCL CCL ACTION RECOVER BRACEERROR C_COMMENT ACTION_COMMENT
%x ACTION_STRING PERCENT_BRACE_ACTION USED_LIST CODEBLOCK_2

WS		[ \t]+
OPTWS		[ \t]*
NOT_WS		[^ \t\n]

NL		(\n|\r\n|\n\r)

NAME		([a-z_][a-z_0-9-]*)
NOT_NAME	[^a-z_*\n]+

SCNAME		{NAME}

ESCSEQ		(\\([^\n]|[0-9]{1,3}|x[0-9a-f]{1,2}))

FIRST_CCL_CHAR	([^\\\n]|{ESCSEQ})
CCL_CHAR	([^\\\n\]]|{ESCSEQ})

%%
	static int bracelevel, didadef, indented_code, checking_used;

	int doing_codeblock = false;
	int i;
	Char nmdef[MAXLINE], myesc();


^{WS}			indented_code = true; BEGIN(CODEBLOCK);
^"/*"			ACTION_ECHO; BEGIN(C_COMMENT);
^"%s"{NAME}?		return SCDECL;
^"%x"{NAME}?		return XSCDECL;
^"%{".*{NL}		{
			++linenum;
			line_directive_out( (FILE *) 0 );
			indented_code = false;
			BEGIN(CODEBLOCK);
			}

{WS}			return WHITESPACE;

^"%%".*			{
			sectnum = 2;
			bracelevel = 0;
			mark_defs1();
			line_directive_out( (FILE *) 0 );
			BEGIN(SECT2PROLOG);
			return SECTEND;
			}

^"%pointer".*{NL}	{
			if ( lex_compat )
				warn( "%pointer incompatible with -l option" );
			else
				yytext_is_array = false;
			++linenum;
			}
^"%array".*{NL}		{
			if ( C_plus_plus )
				warn( "%array incompatible with -+ option" );
			else
				yytext_is_array = true;
			++linenum;
			}

^"%used"		{
			warn( "%used/%unused have been deprecated" );
			checking_used = REALLY_USED; BEGIN(USED_LIST);
			}
^"%unused"		{
			warn( "%used/%unused have been deprecated" );
			checking_used = REALLY_NOT_USED; BEGIN(USED_LIST);
			}


^"%"[aceknopr]{OPTWS}[0-9]*{OPTWS}{NL}	++linenum;	/* ignore */

^"%"[^sxanpekotcru{}].*	synerr( "unrecognized '%' directive" );

^{NAME}			{
			strcpy( nmstr, yytext );
			didadef = false;
			BEGIN(PICKUPDEF);
			}

{SCNAME}		RETURNNAME;
^{OPTWS}{NL}		++linenum; /* allows blank lines in section 1 */
{OPTWS}{NL}		++linenum; return '\n';


<C_COMMENT>"*/"		ACTION_ECHO; BEGIN(INITIAL);
<C_COMMENT>"*/".*{NL}	++linenum; ACTION_ECHO; BEGIN(INITIAL);
<C_COMMENT>[^*\n]+	ACTION_ECHO;
<C_COMMENT>"*"		ACTION_ECHO;
<C_COMMENT>{NL}		++linenum; ACTION_ECHO;


<CODEBLOCK>^"%}".*{NL}	++linenum; BEGIN(INITIAL);
<CODEBLOCK>"reject"	ACTION_ECHO; CHECK_REJECT(yytext);
<CODEBLOCK>"yymore"	ACTION_ECHO; CHECK_YYMORE(yytext);
<CODEBLOCK>{NAME}|{NOT_NAME}|.	ACTION_ECHO;
<CODEBLOCK>{NL}		{
			++linenum;
			ACTION_ECHO;
			if ( indented_code )
				BEGIN(INITIAL);
			}


<PICKUPDEF>{WS}		/* separates name and definition */

<PICKUPDEF>{NOT_WS}.*	{
			strcpy( (char *) nmdef, yytext );

			/* Skip trailing whitespace. */
			for ( i = strlen( (char *) nmdef ) - 1;
			      i >= 0 && (nmdef[i] == ' ' || nmdef[i] == '\t');
			      --i )
				;

			nmdef[i + 1] = '\0';

			ndinstal( nmstr, nmdef );
			didadef = true;
			}

<PICKUPDEF>{NL}		{
			if ( ! didadef )
				synerr( "incomplete name definition" );
			BEGIN(INITIAL);
			++linenum;
			}

<RECOVER>.*{NL}		++linenum; BEGIN(INITIAL); RETURNNAME;


<USED_LIST>{NL}		++linenum; BEGIN(INITIAL);
<USED_LIST>{WS}
<USED_LIST>"reject"	{
			if ( all_upper( yytext ) )
				reject_really_used = checking_used;
			else
				synerr(
				"unrecognized %used/%unused construct" );
			}
<USED_LIST>"yymore"	{
			if ( all_lower( yytext ) )
				yymore_really_used = checking_used;
			else
				synerr(
				"unrecognized %used/%unused construct" );
			}
<USED_LIST>{NOT_WS}+	synerr( "unrecognized %used/%unused construct" );


<SECT2PROLOG>^"%{".*	++bracelevel; yyless( 2 );	/* eat only %{ */
<SECT2PROLOG>^"%}".*	--bracelevel; yyless( 2 );	/* eat only %} */

<SECT2PROLOG>^{WS}.*	ACTION_ECHO;	/* indented code in prolog */

<SECT2PROLOG>^{NOT_WS}.*	{	/* non-indented code */
			if ( bracelevel <= 0 )
				{ /* not in %{ ... %} */
				yyless( 0 );	/* put it all back */
				mark_prolog();
				BEGIN(SECT2);
				}
			else
				ACTION_ECHO;
			}

<SECT2PROLOG>.*		ACTION_ECHO;
<SECT2PROLOG>{NL}	++linenum; ACTION_ECHO;

<SECT2PROLOG><<EOF>>	{
			mark_prolog();
			sectnum = 0;
			yyterminate(); /* to stop the parser */
			}

<SECT2>^{OPTWS}{NL}	++linenum; /* allow blank lines in section 2 */

<SECT2>^({WS}|"%{")	{
			indented_code = (yytext[0] != '%');
			doing_codeblock = true;
			bracelevel = 1;

			if ( indented_code )
				ACTION_ECHO;

			BEGIN(CODEBLOCK_2);
			}

<SECT2>^"<"		BEGIN(SC); return '<';
<SECT2>^"^"		return '^';
<SECT2>\"		BEGIN(QUOTE); return '"';
<SECT2>"{"/[0-9]		BEGIN(NUM); return '{';
<SECT2>"{"[^0-9\n][^}\n]*	BEGIN(BRACEERROR);
<SECT2>"$"/([ \t]|{NL})	return '$';

<SECT2>{WS}"%{"		{
			bracelevel = 1;
			BEGIN(PERCENT_BRACE_ACTION);
			return '\n';
			}
<SECT2>{WS}"|".*{NL}	continued_action = true; ++linenum; return '\n';

<SECT2>{WS}		{
			/* This rule is separate from the one below because
			 * otherwise we get variable trailing context, so
			 * we can't build the scanner using -{f,F}.
			 */
			bracelevel = 0;
			continued_action = false;
			BEGIN(ACTION);
			return '\n';
			}

<SECT2>{OPTWS}{NL}	{
			bracelevel = 0;
			continued_action = false;
			BEGIN(ACTION);
			unput( '\n' );	/* so <ACTION> sees it */
			return '\n';
			}

<SECT2>"<<EOF>>"	return EOF_OP;

<SECT2>^"%%".*		{
			sectnum = 3;
			BEGIN(SECT3);
			yyterminate(); /* to stop the parser */
			}

<SECT2>"["{FIRST_CCL_CHAR}{CCL_CHAR}*	{
			int cclval;

			strcpy( nmstr, yytext );

			/* Check to see if we've already encountered this
			 * ccl.
			 */
			if ( (cclval = ccllookup( (Char *) nmstr )) )
				{
				if ( input() != ']' )
					synerr( "bad character class" );

				yylval = cclval;
				++cclreuse;
				return PREVCCL;
				}
			else
				{
				/* We fudge a bit.  We know that this ccl will
				 * soon be numbered as lastccl + 1 by cclinit.
				 */
				cclinstal( (Char *) nmstr, lastccl + 1 );

				/* Push back everything but the leading bracket
				 * so the ccl can be rescanned.
				 */
				yyless( 1 );

				BEGIN(FIRSTCCL);
				return '[';
				}
			}

<SECT2>"{"{NAME}"}"	{
			register Char *nmdefptr;
			Char *ndlookup();

			strcpy( nmstr, yytext + 1 );
			nmstr[yyleng - 2] = '\0';  /* chop trailing brace */

			if ( ! (nmdefptr = ndlookup( nmstr )) )
				format_synerr( "undefined definition {%s}",
						nmstr );

			else
				{ /* push back name surrounded by ()'s */
				int len = strlen( (char *) nmdefptr );

				if ( lex_compat || nmdefptr[0] == '^' ||
				     (len > 0 && nmdefptr[len - 1] == '$') )
					{ /* don't use ()'s after all */
					PUT_BACK_STRING((char *) nmdefptr, 0);

					if ( nmdefptr[0] == '^' )
						BEGIN(CARETISBOL);
					}

				else
					{
					unput(')');
					PUT_BACK_STRING((char *) nmdefptr, 0);
					unput('(');
					}
				}
			}

<SECT2>[/|*+?.()]	return (unsigned char) yytext[0];
<SECT2>.		RETURNCHAR;


<SC>[,*]		return (unsigned char) yytext[0];
<SC>">"			BEGIN(SECT2); return '>';
<SC>">"/^		BEGIN(CARETISBOL); return '>';
<SC>{SCNAME}		RETURNNAME;
<SC>.			{
			format_synerr( "bad <start condition>: %s", yytext );
			}

<CARETISBOL>"^"		BEGIN(SECT2); return '^';


<QUOTE>[^"\n]		RETURNCHAR;
<QUOTE>\"		BEGIN(SECT2); return '"';

<QUOTE>{NL}		{
			synerr( "missing quote" );
			BEGIN(SECT2);
			++linenum;
			return '"';
			}


<FIRSTCCL>"^"/[^-\]\n]	BEGIN(CCL); return '^';
<FIRSTCCL>"^"/("-"|"]")	return '^';
<FIRSTCCL>.		BEGIN(CCL); RETURNCHAR;

<CCL>-/[^\]\n]		return '-';
<CCL>[^\]\n]		RETURNCHAR;
<CCL>"]"		BEGIN(SECT2); return ']';
<CCL>.|{NL}		{
			synerr( "bad character class" );
			BEGIN(SECT2);
			return ']';
			}


<NUM>[0-9]+		{
			yylval = myctoi( yytext );
			return NUMBER;
			}

<NUM>","		return ',';
<NUM>"}"		BEGIN(SECT2); return '}';

<NUM>.			{
			synerr( "bad character inside {}'s" );
			BEGIN(SECT2);
			return '}';
			}

<NUM>{NL}		{
			synerr( "missing }" );
			BEGIN(SECT2);
			++linenum;
			return '}';
			}


<BRACEERROR>"}"		synerr( "bad name in {}'s" ); BEGIN(SECT2);
<BRACEERROR>{NL}	synerr( "missing }" ); ++linenum; BEGIN(SECT2);


<CODEBLOCK_2>"/*"	ACTION_ECHO; BEGIN(ACTION_COMMENT);
<PERCENT_BRACE_ACTION,CODEBLOCK_2>{OPTWS}"%}".*		bracelevel = 0;
<PERCENT_BRACE_ACTION,CODEBLOCK_2,ACTION>"reject"	{
			ACTION_ECHO;
			CHECK_REJECT(yytext);
			}
<PERCENT_BRACE_ACTION,CODEBLOCK_2,ACTION>"yymore"	{
			ACTION_ECHO;
			CHECK_YYMORE(yytext);
			}
<PERCENT_BRACE_ACTION,CODEBLOCK_2>{NAME}|{NOT_NAME}|.	ACTION_ECHO;
<PERCENT_BRACE_ACTION,CODEBLOCK_2>{NL}			{
			++linenum;
			ACTION_ECHO;
			if ( bracelevel == 0 ||
			     (doing_codeblock && indented_code) )
				{
				if ( ! doing_codeblock )
					add_action( "\tYY_BREAK\n" );
				
				doing_codeblock = false;
				BEGIN(SECT2);
				}
			}


	/* Reject and YYmore() are checked for above, in PERCENT_BRACE_ACTION */
<ACTION>"{"		ACTION_ECHO; ++bracelevel;
<ACTION>"}"		ACTION_ECHO; --bracelevel;
<ACTION>[^a-z_{}"'/\n]+	ACTION_ECHO;
<ACTION>{NAME}		ACTION_ECHO;
<ACTION>"/*"		ACTION_ECHO; BEGIN(ACTION_COMMENT);
<ACTION>"'"([^'\\\n]|\\.)*"'"	ACTION_ECHO; /* character constant */
<ACTION>\"		ACTION_ECHO; BEGIN(ACTION_STRING);
<ACTION>{NL}		{
			++linenum;
			ACTION_ECHO;
			if ( bracelevel == 0 )
				{
				add_action( "\tYY_BREAK\n" );
				BEGIN(SECT2);
				}
			}
<ACTION>.		ACTION_ECHO;

<ACTION_COMMENT>"*/"	{
			ACTION_ECHO;
			if ( doing_codeblock )
				BEGIN(CODEBLOCK_2);
			else
				BEGIN(ACTION);
			}

<ACTION_COMMENT>"*"	ACTION_ECHO;
<ACTION_COMMENT>[^*\n]+	ACTION_ECHO;
<ACTION_COMMENT>[^*\n]*{NL}	++linenum; ACTION_ECHO;

<ACTION_STRING>[^"\\\n]+	ACTION_ECHO;
<ACTION_STRING>\\.	ACTION_ECHO;
<ACTION_STRING>{NL}	++linenum; ACTION_ECHO;
<ACTION_STRING>\"	ACTION_ECHO; BEGIN(ACTION);
<ACTION_STRING>.	ACTION_ECHO;

<ACTION,ACTION_COMMENT,ACTION_STRING><<EOF>>	{
			synerr( "EOF encountered inside an action" );
			yyterminate();
			}


<SECT2,QUOTE,CCL>{ESCSEQ}	{
			yylval = myesc( (Char *) yytext );
			return CHAR;
			}

<FIRSTCCL>{ESCSEQ}	{
			yylval = myesc( (Char *) yytext );
			BEGIN(CCL);
			return CHAR;
			}


<SECT3>.*(\n?)		ECHO;
<SECT3><<EOF>>		sectnum = 0; yyterminate();

<*>.|\n			format_synerr( "bad character: %s", yytext );

%%


int yywrap()
	{
	if ( --num_input_files > 0 )
		{
		set_input_file( *++input_files );
		return 0;
		}

	else
		return 1;
	}


/* set_input_file - open the given file (if NULL, stdin) for scanning */

void set_input_file( file )
char *file;
	{
	if ( file )
		{
		infilename = file;
		yyin = fopen( infilename, "r" );

		if ( yyin == NULL )
			lerrsf( "can't open %s", file );
		}

	else
		{
		yyin = stdin;
		infilename = "<stdin>";
		}
	}


/* Wrapper routines for accessing the scanner's malloc routines. */

void *flex_alloc( size )
unsigned int size;
	{
	return yy_flex_alloc( size );
	}

void *flex_realloc( ptr, size )
void *ptr;
unsigned int size;
	{
	return yy_flex_realloc( ptr, size );
	}

void flex_free( ptr )
void *ptr;
	{
	yy_flex_free( ptr );
	}
