
/* parse.y - parser for flex input */

%token CHAR NUMBER SECTEND SCDECL XSCDECL WHITESPACE NAME PREVCCL EOF_OP

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

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /a/cvs/386BSD/src/usr.bin/lex/parse.y,v 1.2 1993/06/29 03:27:18 nate Exp $ (LBL)";
#endif

#include "flexdef.h"

int pat, scnum, eps, headcnt, trailcnt, anyccl, lastchar, i, actvp, rulelen;
int trlcontxt, xcluflg, cclsorted, varlength, variable_trail_rule;
Char clower();
void build_eof_action();
void yyerror();

static int madeany = false;  /* whether we've made the '.' character class */
int previous_continued_action;	/* whether the previous rule's action was '|' */

%}

%%
goal            :  initlex sect1 sect1end sect2 initforrule
			{ /* add default rule */
			int def_rule;

			pat = cclinit();
			cclnegate( pat );

			def_rule = mkstate( -pat );

			finish_rule( def_rule, false, 0, 0 );

			for ( i = 1; i <= lastsc; ++i )
			    scset[i] = mkbranch( scset[i], def_rule );

			if ( spprdflt )
			    fputs( "YY_FATAL_ERROR( \"flex scanner jammed\" )",
				   temp_action_file );
			else
			    fputs( "ECHO", temp_action_file );

			fputs( ";\n\tYY_BREAK\n", temp_action_file );
			}
		;

initlex         :
			{
			/* initialize for processing rules */

			/* create default DFA start condition */
			scinstal( "INITIAL", false );
			}
		;

sect1		:  sect1 startconddecl WHITESPACE namelist1 '\n'
		|
		|  error '\n'
			{ synerr( "unknown error processing section 1" ); }
		;

sect1end	:  SECTEND
		;

startconddecl   :  SCDECL
			{
			/* these productions are separate from the s1object
			 * rule because the semantics must be done before
			 * we parse the remainder of an s1object
			 */

			xcluflg = false;
			}

		|  XSCDECL
			{ xcluflg = true; }
		;

namelist1	:  namelist1 WHITESPACE NAME
			{ scinstal( nmstr, xcluflg ); }

		|  NAME
			{ scinstal( nmstr, xcluflg ); }

		|  error
                        { synerr( "bad start condition list" ); }
		;

sect2           :  sect2 initforrule flexrule '\n'
		|
		;

initforrule     :
			{
			/* initialize for a parse of one rule */
			trlcontxt = variable_trail_rule = varlength = false;
			trailcnt = headcnt = rulelen = 0;
			current_state_type = STATE_NORMAL;
			previous_continued_action = continued_action;
			new_rule();
			}
		;

flexrule        :  scon '^' rule
                        {
			pat = $3;
			finish_rule( pat, variable_trail_rule,
				     headcnt, trailcnt );

			for ( i = 1; i <= actvp; ++i )
			    scbol[actvsc[i]] =
				mkbranch( scbol[actvsc[i]], pat );

			if ( ! bol_needed )
			    {
			    bol_needed = true;

			    if ( performance_report )
				pinpoint_message( 
			    "'^' operator results in sub-optimal performance" );
			    }
			}

		|  scon rule
                        {
			pat = $2;
			finish_rule( pat, variable_trail_rule,
				     headcnt, trailcnt );

			for ( i = 1; i <= actvp; ++i )
			    scset[actvsc[i]] =
				mkbranch( scset[actvsc[i]], pat );
			}

                |  '^' rule
			{
			pat = $2;
			finish_rule( pat, variable_trail_rule,
				     headcnt, trailcnt );

			/* add to all non-exclusive start conditions,
			 * including the default (0) start condition
			 */

			for ( i = 1; i <= lastsc; ++i )
			    if ( ! scxclu[i] )
				scbol[i] = mkbranch( scbol[i], pat );

			if ( ! bol_needed )
			    {
			    bol_needed = true;

			    if ( performance_report )
				pinpoint_message(
			    "'^' operator results in sub-optimal performance" );
			    }
			}

                |  rule
			{
			pat = $1;
			finish_rule( pat, variable_trail_rule,
				     headcnt, trailcnt );

			for ( i = 1; i <= lastsc; ++i )
			    if ( ! scxclu[i] )
				scset[i] = mkbranch( scset[i], pat );
			}

                |  scon EOF_OP
			{ build_eof_action(); }

                |  EOF_OP
			{
			/* this EOF applies to all start conditions
			 * which don't already have EOF actions
			 */
			actvp = 0;

			for ( i = 1; i <= lastsc; ++i )
			    if ( ! sceof[i] )
				actvsc[++actvp] = i;

			if ( actvp == 0 )
			    pinpoint_message(
		"warning - all start conditions already have <<EOF>> rules" );

			else
			    build_eof_action();
			}

                |  error
			{ synerr( "unrecognized rule" ); }
		;

scon            :  '<' namelist2 '>'
		;

namelist2       :  namelist2 ',' NAME
                        {
			if ( (scnum = sclookup( nmstr )) == 0 )
			    format_pinpoint_message(
				"undeclared start condition %s", nmstr );

			else
			    actvsc[++actvp] = scnum;
			}

		|  NAME
			{
			if ( (scnum = sclookup( nmstr )) == 0 )
			    format_pinpoint_message(
				"undeclared start condition %s", nmstr );
			else
			    actvsc[actvp = 1] = scnum;
			}

		|  error
			{ synerr( "bad start condition list" ); }
		;

rule            :  re2 re
			{
			if ( transchar[lastst[$2]] != SYM_EPSILON )
			    /* provide final transition \now/ so it
			     * will be marked as a trailing context
			     * state
			     */
			    $2 = link_machines( $2, mkstate( SYM_EPSILON ) );

			mark_beginning_as_normal( $2 );
			current_state_type = STATE_NORMAL;

			if ( previous_continued_action )
			    {
			    /* we need to treat this as variable trailing
			     * context so that the backup does not happen
			     * in the action but before the action switch
			     * statement.  If the backup happens in the
			     * action, then the rules "falling into" this
			     * one's action will *also* do the backup,
			     * erroneously.
			     */
			    if ( ! varlength || headcnt != 0 )
				{
				fprintf( stderr,
    "%s: warning - trailing context rule at line %d made variable because\n",
					 program_name, linenum );
				fprintf( stderr,
					 "      of preceding '|' action\n" );
				}

			    /* mark as variable */
			    varlength = true;
			    headcnt = 0;
			    }

			if ( varlength && headcnt == 0 )
			    { /* variable trailing context rule */
			    /* mark the first part of the rule as the accepting
			     * "head" part of a trailing context rule
			     */
			    /* by the way, we didn't do this at the beginning
			     * of this production because back then
			     * current_state_type was set up for a trail
			     * rule, and add_accept() can create a new
			     * state ...
			     */
			    add_accept( $1, num_rules | YY_TRAILING_HEAD_MASK );
			    variable_trail_rule = true;
			    }
			
			else
			    trailcnt = rulelen;

			$$ = link_machines( $1, $2 );
			}

		|  re2 re '$'
			{ synerr( "trailing context used twice" ); }

		|  re '$'
                        {
			if ( trlcontxt )
			    {
			    synerr( "trailing context used twice" );
			    $$ = mkstate( SYM_EPSILON );
			    }

			else if ( previous_continued_action )
			    {
			    /* see the comment in the rule for "re2 re"
			     * above
			     */
			    if ( ! varlength || headcnt != 0 )
				{
				fprintf( stderr,
    "%s: warning - trailing context rule at line %d made variable because\n",
					 program_name, linenum );
				fprintf( stderr,
					 "      of preceding '|' action\n" );
				}

			    /* mark as variable */
			    varlength = true;
			    headcnt = 0;
			    }

			if ( varlength && headcnt == 0 )
			    {
			    /* again, see the comment in the rule for "re2 re"
			     * above
			     */
			    add_accept( $1, num_rules | YY_TRAILING_HEAD_MASK );
			    variable_trail_rule = true;
			    }

			else
			    {
			    if ( ! varlength )
				headcnt = rulelen;

			    ++rulelen;
			    trailcnt = 1;
			    }

			trlcontxt = true;

			eps = mkstate( SYM_EPSILON );
			$$ = link_machines( $1,
				 link_machines( eps, mkstate( '\n' ) ) );
			}

		|  re
			{
		        $$ = $1;

			if ( trlcontxt )
			    {
			    if ( varlength && headcnt == 0 )
				/* both head and trail are variable-length */
				variable_trail_rule = true;
			    else
				trailcnt = rulelen;
			    }
		        }
		;


re              :  re '|' series
                        {
			varlength = true;
			$$ = mkor( $1, $3 );
			}

		|  series
			{ $$ = $1; }
		;


re2		:  re '/'
			{
			/* this rule is written separately so
			 * the reduction will occur before the trailing
			 * series is parsed
			 */

			if ( trlcontxt )
			    synerr( "trailing context used twice" );
			else
			    trlcontxt = true;

			if ( varlength )
			    /* we hope the trailing context is fixed-length */
			    varlength = false;
			else
			    headcnt = rulelen;

			rulelen = 0;

			current_state_type = STATE_TRAILING_CONTEXT;
			$$ = $1;
			}
		;

series          :  series singleton
                        {
			/* this is where concatenation of adjacent patterns
			 * gets done
			 */
			$$ = link_machines( $1, $2 );
			}

		|  singleton
			{ $$ = $1; }
		;

singleton       :  singleton '*'
                        {
			varlength = true;

			$$ = mkclos( $1 );
			}

		|  singleton '+'
			{
			varlength = true;

			$$ = mkposcl( $1 );
			}

		|  singleton '?'
			{
			varlength = true;

			$$ = mkopt( $1 );
			}

		|  singleton '{' NUMBER ',' NUMBER '}'
			{
			varlength = true;

			if ( $3 > $5 || $3 < 0 )
			    {
			    synerr( "bad iteration values" );
			    $$ = $1;
			    }
			else
			    {
			    if ( $3 == 0 )
				{
				if ( $5 <= 0 )
				    {
				    synerr( "bad iteration values" );
				    $$ = $1;
				    }
				else
				    $$ = mkopt( mkrep( $1, 1, $5 ) );
				}
			    else
				$$ = mkrep( $1, $3, $5 );
			    }
			}

		|  singleton '{' NUMBER ',' '}'
			{
			varlength = true;

			if ( $3 <= 0 )
			    {
			    synerr( "iteration value must be positive" );
			    $$ = $1;
			    }

			else
			    $$ = mkrep( $1, $3, INFINITY );
			}

		|  singleton '{' NUMBER '}'
			{
			/* the singleton could be something like "(foo)",
			 * in which case we have no idea what its length
			 * is, so we punt here.
			 */
			varlength = true;

			if ( $3 <= 0 )
			    {
			    synerr( "iteration value must be positive" );
			    $$ = $1;
			    }

			else
			    $$ = link_machines( $1, copysingl( $1, $3 - 1 ) );
			}

		|  '.'
			{
			if ( ! madeany )
			    {
			    /* create the '.' character class */
			    anyccl = cclinit();
			    ccladd( anyccl, '\n' );
			    cclnegate( anyccl );

			    if ( useecs )
				mkeccl( ccltbl + cclmap[anyccl],
					ccllen[anyccl], nextecm,
					ecgroup, csize, csize );

			    madeany = true;
			    }

			++rulelen;

			$$ = mkstate( -anyccl );
			}

		|  fullccl
			{
			if ( ! cclsorted )
			    /* sort characters for fast searching.  We use a
			     * shell sort since this list could be large.
			     */
			    cshell( ccltbl + cclmap[$1], ccllen[$1], true );

			if ( useecs )
			    mkeccl( ccltbl + cclmap[$1], ccllen[$1],
				    nextecm, ecgroup, csize, csize );

			++rulelen;

			$$ = mkstate( -$1 );
			}

		|  PREVCCL
			{
			++rulelen;

			$$ = mkstate( -$1 );
			}

		|  '"' string '"'
			{ $$ = $2; }

		|  '(' re ')'
			{ $$ = $2; }

		|  CHAR
			{
			++rulelen;

			if ( caseins && $1 >= 'A' && $1 <= 'Z' )
			    $1 = clower( $1 );

			$$ = mkstate( $1 );
			}
		;

fullccl		:  '[' ccl ']'
			{ $$ = $2; }

		|  '[' '^' ccl ']'
			{
			/* *Sigh* - to be compatible Unix lex, negated ccls
			 * match newlines
			 */
#ifdef NOTDEF
			ccladd( $3, '\n' ); /* negated ccls don't match '\n' */
			cclsorted = false; /* because we added the newline */
#endif
			cclnegate( $3 );
			$$ = $3;
			}
		;

ccl             :  ccl CHAR '-' CHAR
                        {
			if ( $2 > $4 )
			    synerr( "negative range in character class" );

			else
			    {
			    if ( caseins )
				{
				if ( $2 >= 'A' && $2 <= 'Z' )
				    $2 = clower( $2 );
				if ( $4 >= 'A' && $4 <= 'Z' )
				    $4 = clower( $4 );
				}

			    for ( i = $2; i <= $4; ++i )
			        ccladd( $1, i );

			    /* keep track if this ccl is staying in alphabetical
			     * order
			     */
			    cclsorted = cclsorted && ($2 > lastchar);
			    lastchar = $4;
			    }

			$$ = $1;
			}

		|  ccl CHAR
		        {
			if ( caseins )
			    if ( $2 >= 'A' && $2 <= 'Z' )
				$2 = clower( $2 );

			ccladd( $1, $2 );
			cclsorted = cclsorted && ($2 > lastchar);
			lastchar = $2;
			$$ = $1;
			}

		|
			{
			cclsorted = true;
			lastchar = 0;
			$$ = cclinit();
			}
		;

string		:  string CHAR
                        {
			if ( caseins )
			    if ( $2 >= 'A' && $2 <= 'Z' )
				$2 = clower( $2 );

			++rulelen;

			$$ = link_machines( $1, mkstate( $2 ) );
			}

		|
			{ $$ = mkstate( SYM_EPSILON ); }
		;

%%


/* build_eof_action - build the "<<EOF>>" action for the active start
 *                    conditions
 */

void build_eof_action()

    {
    register int i;

    for ( i = 1; i <= actvp; ++i )
	{
	if ( sceof[actvsc[i]] )
	    format_pinpoint_message(
		"multiple <<EOF>> rules for start condition %s",
		    scname[actvsc[i]] );

	else
	    {
	    sceof[actvsc[i]] = true;
	    fprintf( temp_action_file, "case YY_STATE_EOF(%s):\n",
		     scname[actvsc[i]] );
	    }
	}

    line_directive_out( temp_action_file );
    }


/* synerr - report a syntax error */

void synerr( str )
char str[];

    {
    syntaxerror = true;
    pinpoint_message( str );
    }


/* format_pinpoint_message - write out a message formatted with one string,
 *			     pinpointing its location
 */

void format_pinpoint_message( msg, arg )
char msg[], arg[];

    {
    char errmsg[MAXLINE];

    (void) sprintf( errmsg, msg, arg );
    pinpoint_message( errmsg );
    }


/* pinpoint_message - write out a message, pinpointing its location */

void pinpoint_message( str )
char str[];

    {
    fprintf( stderr, "\"%s\", line %d: %s\n", infilename, linenum, str );
    }


/* yyerror - eat up an error message from the parser;
 *	     currently, messages are ignore
 */

void yyerror( msg )
char msg[];

    {
    }
