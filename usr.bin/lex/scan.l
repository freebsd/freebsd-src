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

/* $Header: /home/daffy/u0/vern/flex/RCS/scan.l,v 2.56 95/04/24 12:17:19 vern Exp $ */
/* $FreeBSD: src/usr.bin/lex/scan.l,v 1.5 1999/10/27 07:56:46 obrien Exp $ */

#include "flexdef.h"
#include "parse.h"

#define ACTION_ECHO add_action( yytext )
#define ACTION_IFDEF(def, should_define) \
	{ \
	if ( should_define ) \
		action_define( def, 1 ); \
	}

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

%option caseless nodefault outfile="scan.c" stack noyy_top_state
%option nostdinit

%x SECT2 SECT2PROLOG SECT3 CODEBLOCK PICKUPDEF SC CARETISBOL NUM QUOTE
%x FIRSTCCL CCL ACTION RECOVER COMMENT ACTION_STRING PERCENT_BRACE_ACTION
%x OPTION LINEDIR

WS		[[:blank:]]+
OPTWS		[[:blank:]]*
NOT_WS		[^[:blank:]\n]

NL		\r?\n

NAME		([[:alpha:]_][[:alnum:]_-]*)
NOT_NAME	[^[:alpha:]_*\n]+

SCNAME		{NAME}

ESCSEQ		(\\([^\n]|[0-7]{1,3}|x[[:xdigit:]]{1,2}))

FIRST_CCL_CHAR	([^\\\n]|{ESCSEQ})
CCL_CHAR	([^\\\n\]]|{ESCSEQ})
CCL_EXPR	("[:"[[:alpha:]]+":]")

LEXOPT		[aceknopr]

%%
	static int bracelevel, didadef, indented_code;
	static int doing_rule_action = false;
	static int option_sense;

	int doing_codeblock = false;
	int i;
	Char nmdef[MAXLINE], myesc();


<INITIAL>{
	^{WS}		indented_code = true; BEGIN(CODEBLOCK);
	^"/*"		ACTION_ECHO; yy_push_state( COMMENT );
	^#{OPTWS}line{WS}	yy_push_state( LINEDIR );
	^"%s"{NAME}?	return SCDECL;
	^"%x"{NAME}?	return XSCDECL;
	^"%{".*{NL}	{
			++linenum;
			line_directive_out( (FILE *) 0, 1 );
			indented_code = false;
			BEGIN(CODEBLOCK);
			}

	{WS}		/* discard */

	^"%%".*		{
			sectnum = 2;
			bracelevel = 0;
			mark_defs1();
			line_directive_out( (FILE *) 0, 1 );
			BEGIN(SECT2PROLOG);
			return SECTEND;
			}

	^"%pointer".*{NL}	yytext_is_array = false; ++linenum;
	^"%array".*{NL}		yytext_is_array = true; ++linenum;

	^"%option"	BEGIN(OPTION); return OPTION_OP;

	^"%"{LEXOPT}{OPTWS}[[:digit:]]*{OPTWS}{NL}	++linenum; /* ignore */
	^"%"{LEXOPT}{WS}.*{NL}	++linenum;	/* ignore */

	^"%"[^sxaceknopr{}].*	synerr( _( "unrecognized '%' directive" ) );

	^{NAME}		{
			strcpy( nmstr, yytext );
			didadef = false;
			BEGIN(PICKUPDEF);
			}

	{SCNAME}	RETURNNAME;
	^{OPTWS}{NL}	++linenum; /* allows blank lines in section 1 */
	{OPTWS}{NL}	ACTION_ECHO; ++linenum; /* maybe end of comment line */
}


<COMMENT>{
	"*/"		ACTION_ECHO; yy_pop_state();
	"*"		ACTION_ECHO;
	[^*\n]+		ACTION_ECHO;
	[^*\n]*{NL}	++linenum; ACTION_ECHO;
}

<LINEDIR>{
	\n		yy_pop_state();
	[[:digit:]]+	linenum = myctoi( yytext );

	\"[^"\n]*\"	{
			flex_free( (void *) infilename );
			infilename = copy_string( yytext + 1 );
			infilename[strlen( infilename ) - 1] = '\0';
			}
	.		/* ignore spurious characters */
}

<CODEBLOCK>{
	^"%}".*{NL}	++linenum; BEGIN(INITIAL);

	{NAME}|{NOT_NAME}|.	ACTION_ECHO;

	{NL}		{
			++linenum;
			ACTION_ECHO;
			if ( indented_code )
				BEGIN(INITIAL);
			}
}


<PICKUPDEF>{
	{WS}		/* separates name and definition */

	{NOT_WS}.*	{
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

	{NL}		{
			if ( ! didadef )
				synerr( _( "incomplete name definition" ) );
			BEGIN(INITIAL);
			++linenum;
			}
}


<OPTION>{
	{NL}		++linenum; BEGIN(INITIAL);
	{WS}		option_sense = true;

	"="		return '=';

	no		option_sense = ! option_sense;

	7bit		csize = option_sense ? 128 : 256;
	8bit		csize = option_sense ? 256 : 128;

	align		long_align = option_sense;
	always-interactive	{
			action_define( "YY_ALWAYS_INTERACTIVE", option_sense );
			}
	array		yytext_is_array = option_sense;
	backup		backing_up_report = option_sense;
	batch		interactive = ! option_sense;
	"c++"		C_plus_plus = option_sense;
	caseful|case-sensitive		caseins = ! option_sense;
	caseless|case-insensitive	caseins = option_sense;
	debug		ddebug = option_sense;
	default		spprdflt = ! option_sense;
	ecs		useecs = option_sense;
	fast		{
			useecs = usemecs = false;
			use_read = fullspd = true;
			}
	full		{
			useecs = usemecs = false;
			use_read = fulltbl = true;
			}
	input		ACTION_IFDEF("YY_NO_INPUT", ! option_sense);
	interactive	interactive = option_sense;
	lex-compat	lex_compat = option_sense;
	main		{
			action_define( "YY_MAIN", option_sense );
			do_yywrap = ! option_sense;
			}
	meta-ecs	usemecs = option_sense;
	never-interactive	{
			action_define( "YY_NEVER_INTERACTIVE", option_sense );
			}
	perf-report	performance_report += option_sense ? 1 : -1;
	pointer		yytext_is_array = ! option_sense;
	read		use_read = option_sense;
	reject		reject_really_used = option_sense;
	stack		action_define( "YY_STACK_USED", option_sense );
	stdinit		do_stdinit = option_sense;
	stdout		use_stdout = option_sense;
	unput		ACTION_IFDEF("YY_NO_UNPUT", ! option_sense);
	verbose		printstats = option_sense;
	warn		nowarn = ! option_sense;
	yylineno	do_yylineno = option_sense;
	yymore		yymore_really_used = option_sense;
	yywrap		do_yywrap = option_sense;

	yy_push_state	ACTION_IFDEF("YY_NO_PUSH_STATE", ! option_sense);
	yy_pop_state	ACTION_IFDEF("YY_NO_POP_STATE", ! option_sense);
	yy_top_state	ACTION_IFDEF("YY_NO_TOP_STATE", ! option_sense);

	yy_scan_buffer	ACTION_IFDEF("YY_NO_SCAN_BUFFER", ! option_sense);
	yy_scan_bytes	ACTION_IFDEF("YY_NO_SCAN_BYTES", ! option_sense);
	yy_scan_string	ACTION_IFDEF("YY_NO_SCAN_STRING", ! option_sense);

	outfile		return OPT_OUTFILE;
	prefix		return OPT_PREFIX;
	yyclass		return OPT_YYCLASS;

	\"[^"\n]*\"	{
			strcpy( nmstr, yytext + 1 );
			nmstr[strlen( nmstr ) - 1] = '\0';
			return NAME;
			}

	(([a-mo-z]|n[a-np-z])[[:alpha:]\-+]*)|.	{
			format_synerr( _( "unrecognized %%option: %s" ),
				yytext );
			BEGIN(RECOVER);
			}
}

<RECOVER>.*{NL}		++linenum; BEGIN(INITIAL);


<SECT2PROLOG>{
	^"%{".*	++bracelevel; yyless( 2 );	/* eat only %{ */
	^"%}".*	--bracelevel; yyless( 2 );	/* eat only %} */

	^{WS}.*	ACTION_ECHO;	/* indented code in prolog */

	^{NOT_WS}.*	{	/* non-indented code */
			if ( bracelevel <= 0 )
				{ /* not in %{ ... %} */
				yyless( 0 );	/* put it all back */
				yy_set_bol( 1 );
				mark_prolog();
				BEGIN(SECT2);
				}
			else
				ACTION_ECHO;
			}

	.*		ACTION_ECHO;
	{NL}	++linenum; ACTION_ECHO;

	<<EOF>>		{
			mark_prolog();
			sectnum = 0;
			yyterminate(); /* to stop the parser */
			}
}

<SECT2>{
	^{OPTWS}{NL}	++linenum; /* allow blank lines in section 2 */

	^{OPTWS}"%{"	{
			indented_code = false;
			doing_codeblock = true;
			bracelevel = 1;
			BEGIN(PERCENT_BRACE_ACTION);
			}

	^{OPTWS}"<"	BEGIN(SC); return '<';
	^{OPTWS}"^"	return '^';
	\"		BEGIN(QUOTE); return '"';
	"{"/[[:digit:]]	BEGIN(NUM); return '{';
	"$"/([[:blank:]]|{NL})	return '$';

	{WS}"%{"		{
			bracelevel = 1;
			BEGIN(PERCENT_BRACE_ACTION);

			if ( in_rule )
				{
				doing_rule_action = true;
				in_rule = false;
				return '\n';
				}
			}
	{WS}"|".*{NL}	continued_action = true; ++linenum; return '\n';

	^{WS}"/*"	{
			yyless( yyleng - 2 );	/* put back '/', '*' */
			bracelevel = 0;
			continued_action = false;
			BEGIN(ACTION);
			}

	^{WS}		/* allow indented rules */

	{WS}		{
			/* This rule is separate from the one below because
			 * otherwise we get variable trailing context, so
			 * we can't build the scanner using -{f,F}.
			 */
			bracelevel = 0;
			continued_action = false;
			BEGIN(ACTION);

			if ( in_rule )
				{
				doing_rule_action = true;
				in_rule = false;
				return '\n';
				}
			}

	{OPTWS}{NL}	{
			bracelevel = 0;
			continued_action = false;
			BEGIN(ACTION);
			unput( '\n' );	/* so <ACTION> sees it */

			if ( in_rule )
				{
				doing_rule_action = true;
				in_rule = false;
				return '\n';
				}
			}

	^{OPTWS}"<<EOF>>"	|
	"<<EOF>>"	return EOF_OP;

	^"%%".*		{
			sectnum = 3;
			BEGIN(SECT3);
			yyterminate(); /* to stop the parser */
			}

	"["({FIRST_CCL_CHAR}|{CCL_EXPR})({CCL_CHAR}|{CCL_EXPR})*	{
			int cclval;

			strcpy( nmstr, yytext );

			/* Check to see if we've already encountered this
			 * ccl.
			 */
			if ( (cclval = ccllookup( (Char *) nmstr )) != 0 )
				{
				if ( input() != ']' )
					synerr( _( "bad character class" ) );

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

	"{"{NAME}"}"	{
			register Char *nmdefptr;
			Char *ndlookup();

			strcpy( nmstr, yytext + 1 );
			nmstr[yyleng - 2] = '\0';  /* chop trailing brace */

			if ( (nmdefptr = ndlookup( nmstr )) == 0 )
				format_synerr(
					_( "undefined definition {%s}" ),
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

	[/|*+?.(){}]	return (unsigned char) yytext[0];
	.		RETURNCHAR;
}


<SC>{
	[,*]		return (unsigned char) yytext[0];
	">"		BEGIN(SECT2); return '>';
	">"/^		BEGIN(CARETISBOL); return '>';
	{SCNAME}	RETURNNAME;
	.		{
			format_synerr( _( "bad <start condition>: %s" ),
				yytext );
			}
}

<CARETISBOL>"^"		BEGIN(SECT2); return '^';


<QUOTE>{
	[^"\n]		RETURNCHAR;
	\"		BEGIN(SECT2); return '"';

	{NL}		{
			synerr( _( "missing quote" ) );
			BEGIN(SECT2);
			++linenum;
			return '"';
			}
}


<FIRSTCCL>{
	"^"/[^-\]\n]	BEGIN(CCL); return '^';
	"^"/("-"|"]")	return '^';
	.		BEGIN(CCL); RETURNCHAR;
}

<CCL>{
	-/[^\]\n]	return '-';
	[^\]\n]		RETURNCHAR;
	"]"		BEGIN(SECT2); return ']';
	.|{NL}		{
			synerr( _( "bad character class" ) );
			BEGIN(SECT2);
			return ']';
			}
}

<FIRSTCCL,CCL>{
	"[:alnum:]"	BEGIN(CCL); return CCE_ALNUM;
	"[:alpha:]"	BEGIN(CCL); return CCE_ALPHA;
	"[:blank:]"	BEGIN(CCL); return CCE_BLANK;
	"[:cntrl:]"	BEGIN(CCL); return CCE_CNTRL;
	"[:digit:]"	BEGIN(CCL); return CCE_DIGIT;
	"[:graph:]"	BEGIN(CCL); return CCE_GRAPH;
	"[:lower:]"	BEGIN(CCL); return CCE_LOWER;
	"[:print:]"	BEGIN(CCL); return CCE_PRINT;
	"[:punct:]"	BEGIN(CCL); return CCE_PUNCT;
	"[:space:]"	BEGIN(CCL); return CCE_SPACE;
	"[:upper:]"	BEGIN(CCL); return CCE_UPPER;
	"[:xdigit:]"	BEGIN(CCL); return CCE_XDIGIT;
	{CCL_EXPR}	{
			format_synerr(
				_( "bad character class expression: %s" ),
					yytext );
			BEGIN(CCL); return CCE_ALNUM;
			}
}

<NUM>{
	[[:digit:]]+	{
			yylval = myctoi( yytext );
			return NUMBER;
			}

	","		return ',';
	"}"		BEGIN(SECT2); return '}';

	.		{
			synerr( _( "bad character inside {}'s" ) );
			BEGIN(SECT2);
			return '}';
			}

	{NL}		{
			synerr( _( "missing }" ) );
			BEGIN(SECT2);
			++linenum;
			return '}';
			}
}


<PERCENT_BRACE_ACTION>{
	{OPTWS}"%}".*		bracelevel = 0;

	<ACTION>"/*"		ACTION_ECHO; yy_push_state( COMMENT );

	<CODEBLOCK,ACTION>{
		"reject"	{
			ACTION_ECHO;
			CHECK_REJECT(yytext);
			}
		"yymore"	{
			ACTION_ECHO;
			CHECK_YYMORE(yytext);
			}
	}

	{NAME}|{NOT_NAME}|.	ACTION_ECHO;
	{NL}		{
			++linenum;
			ACTION_ECHO;
			if ( bracelevel == 0 ||
			     (doing_codeblock && indented_code) )
				{
				if ( doing_rule_action )
					add_action( "\tYY_BREAK\n" );

				doing_rule_action = doing_codeblock = false;
				BEGIN(SECT2);
				}
			}
}


	/* Reject and YYmore() are checked for above, in PERCENT_BRACE_ACTION */
<ACTION>{
	"{"		ACTION_ECHO; ++bracelevel;
	"}"		ACTION_ECHO; --bracelevel;
	[^[:alpha:]_{}"'/\n]+	ACTION_ECHO;
	{NAME}		ACTION_ECHO;
	"'"([^'\\\n]|\\.)*"'"	ACTION_ECHO; /* character constant */
	\"		ACTION_ECHO; BEGIN(ACTION_STRING);
	{NL}		{
			++linenum;
			ACTION_ECHO;
			if ( bracelevel == 0 )
				{
				if ( doing_rule_action )
					add_action( "\tYY_BREAK\n" );

				doing_rule_action = false;
				BEGIN(SECT2);
				}
			}
	.		ACTION_ECHO;
}

<ACTION_STRING>{
	[^"\\\n]+	ACTION_ECHO;
	\\.		ACTION_ECHO;
	{NL}		++linenum; ACTION_ECHO;
	\"		ACTION_ECHO; BEGIN(ACTION);
	.		ACTION_ECHO;
}

<COMMENT,ACTION,ACTION_STRING><<EOF>>	{
			synerr( _( "EOF encountered inside an action" ) );
			yyterminate();
			}


<SECT2,QUOTE,FIRSTCCL,CCL>{ESCSEQ}	{
			yylval = myesc( (Char *) yytext );

			if ( YY_START == FIRSTCCL )
				BEGIN(CCL);

			return CHAR;
			}


<SECT3>{
	.*(\n?)		ECHO;
	<<EOF>>		sectnum = 0; yyterminate();
}

<*>.|\n			format_synerr( _( "bad character: %s" ), yytext );

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
	if ( file && strcmp( file, "-" ) )
		{
		infilename = copy_string( file );
		yyin = fopen( infilename, "r" );

		if ( yyin == NULL )
			lerrsf( _( "can't open %s" ), file );
		}

	else
		{
		yyin = stdin;
		infilename = copy_string( "<stdin>" );
		}

	linenum = 1;
	}


/* Wrapper routines for accessing the scanner's malloc routines. */

void *flex_alloc( size )
size_t size;
	{
	return (void *) malloc( size );
	}

void *flex_realloc( ptr, size )
void *ptr;
size_t size;
	{
	return (void *) realloc( ptr, size );
	}

void flex_free( ptr )
void *ptr;
	{
	if ( ptr )
		free( ptr );
	}
