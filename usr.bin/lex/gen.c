/* gen - actual generation (writing) of flex scanners */

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
    "@(#) $Header: /a/cvs/386BSD/src/usr.bin/lex/gen.c,v 1.2 1993/06/29 03:27:10 nate Exp $ (LBL)";
#endif

#include "flexdef.h"


/* declare functions that have forward references */

void gen_next_state PROTO((int));
void genecs PROTO(());
void indent_put2s PROTO((char [], char []));
void indent_puts PROTO((char []));


static int indent_level = 0; /* each level is 4 spaces */

#define indent_up() (++indent_level)
#define indent_down() (--indent_level)
#define set_indent(indent_val) indent_level = indent_val

/* *everything* is done in terms of arrays starting at 1, so provide
 * a null entry for the zero element of all C arrays
 */
static char C_short_decl[] = "static const short int %s[%d] =\n    {   0,\n";
static char C_long_decl[] = "static const long int %s[%d] =\n    {   0,\n";
static char C_state_decl[] =
	"static const yy_state_type %s[%d] =\n    {   0,\n";


/* indent to the current level */

void do_indent()

    {
    register int i = indent_level * 4;

    while ( i >= 8 )
	{
	putchar( '\t' );
	i -= 8;
	}
    
    while ( i > 0 )
	{
	putchar( ' ' );
	--i;
	}
    }


/* generate the code to keep backtracking information */

void gen_backtracking()

    {
    if ( reject || num_backtracking == 0 )
	return;

    if ( fullspd )
	indent_puts( "if ( yy_current_state[-1].yy_nxt )" );
    else
	indent_puts( "if ( yy_accept[yy_current_state] )" );

    indent_up();
    indent_puts( "{" );
    indent_puts( "yy_last_accepting_state = yy_current_state;" );
    indent_puts( "yy_last_accepting_cpos = yy_cp;" );
    indent_puts( "}" );
    indent_down();
    }


/* generate the code to perform the backtrack */

void gen_bt_action()

    {
    if ( reject || num_backtracking == 0 )
	return;

    set_indent( 3 );

    indent_puts( "case 0: /* must backtrack */" );
    indent_puts( "/* undo the effects of YY_DO_BEFORE_ACTION */" );
    indent_puts( "*yy_cp = yy_hold_char;" );

    if ( fullspd || fulltbl )
	indent_puts( "yy_cp = yy_last_accepting_cpos + 1;" );
    else
	/* backtracking info for compressed tables is taken \after/
	 * yy_cp has been incremented for the next state
	 */
	indent_puts( "yy_cp = yy_last_accepting_cpos;" );

    indent_puts( "yy_current_state = yy_last_accepting_state;" );
    indent_puts( "goto yy_find_action;" );
    putchar( '\n' );

    set_indent( 0 );
    }


/* genctbl - generates full speed compressed transition table
 *
 * synopsis
 *     genctbl();
 */

void genctbl()

    {
    register int i;
    int end_of_buffer_action = num_rules + 1;

    /* table of verify for transition and offset to next state */
    printf( "static const struct yy_trans_info yy_transition[%d] =\n",
	    tblend + numecs + 1 );
    printf( "    {\n" );
    
    /* We want the transition to be represented as the offset to the
     * next state, not the actual state number, which is what it currently is.
     * The offset is base[nxt[i]] - base[chk[i]].  That's just the
     * difference between the starting points of the two involved states
     * (to - from).
     *
     * first, though, we need to find some way to put in our end-of-buffer
     * flags and states.  We do this by making a state with absolutely no
     * transitions.  We put it at the end of the table.
     */
    /* at this point, we're guaranteed that there's enough room in nxt[]
     * and chk[] to hold tblend + numecs entries.  We need just two slots.
     * One for the action and one for the end-of-buffer transition.  We
     * now *assume* that we're guaranteed the only character we'll try to
     * index this nxt/chk pair with is EOB, i.e., 0, so we don't have to
     * make sure there's room for jam entries for other characters.
     */

    base[lastdfa + 1] = tblend + 2;
    nxt[tblend + 1] = end_of_buffer_action;
    chk[tblend + 1] = numecs + 1;
    chk[tblend + 2] = 1; /* anything but EOB */
    nxt[tblend + 2] = 0; /* so that "make test" won't show arb. differences */

    /* make sure every state has a end-of-buffer transition and an action # */
    for ( i = 0; i <= lastdfa; ++i )
	{
	register int anum = dfaacc[i].dfaacc_state;

	chk[base[i]] = EOB_POSITION;
	chk[base[i] - 1] = ACTION_POSITION;
	nxt[base[i] - 1] = anum;	/* action number */
	}

    for ( i = 0; i <= tblend; ++i )
	{
	if ( chk[i] == EOB_POSITION )
	    transition_struct_out( 0, base[lastdfa + 1] - i );

	else if ( chk[i] == ACTION_POSITION )
	    transition_struct_out( 0, nxt[i] );

	else if ( chk[i] > numecs || chk[i] == 0 )
	    transition_struct_out( 0, 0 );		/* unused slot */

	else	/* verify, transition */
	    transition_struct_out( chk[i], base[nxt[i]] - (i - chk[i]) );
	}


    /* here's the final, end-of-buffer state */
    transition_struct_out( chk[tblend + 1], nxt[tblend + 1] );
    transition_struct_out( chk[tblend + 2], nxt[tblend + 2] );

    printf( "    };\n" );
    printf( "\n" );

    /* table of pointers to start states */
    printf( "static const struct yy_trans_info *yy_start_state_list[%d] =\n",
	    lastsc * 2 + 1 );
    printf( "    {\n" );

    for ( i = 0; i <= lastsc * 2; ++i )
	printf( "    &yy_transition[%d],\n", base[i] );

    dataend();

    if ( useecs )
	genecs();
    }


/* generate equivalence-class tables */

void genecs()

    {
    register int i, j;
    static char C_char_decl[] = "static const %s %s[%d] =\n    {   0,\n";
    int numrows;
    Char clower();

    if ( numecs < csize )
	printf( C_char_decl, "YY_CHAR", "yy_ec", csize );
    else
	printf( C_char_decl, "short", "yy_ec", csize );

    for ( i = 1; i < csize; ++i )
	{
	if ( caseins && (i >= 'A') && (i <= 'Z') )
	    ecgroup[i] = ecgroup[clower( i )];

	ecgroup[i] = abs( ecgroup[i] );
	mkdata( ecgroup[i] );
	}

    dataend();

    if ( trace )
	{
	char *readable_form();

	fputs( "\n\nEquivalence Classes:\n\n", stderr );

	numrows = csize / 8;

	for ( j = 0; j < numrows; ++j )
	    {
	    for ( i = j; i < csize; i = i + numrows )
		{
		fprintf( stderr, "%4s = %-2d", readable_form( i ), ecgroup[i] );

		putc( ' ', stderr );
		}

	    putc( '\n', stderr );
	    }
	}
    }


/* generate the code to find the action number */

void gen_find_action()

    {
    if ( fullspd )
	indent_puts( "yy_act = yy_current_state[-1].yy_nxt;" );

    else if ( fulltbl )
	indent_puts( "yy_act = yy_accept[yy_current_state];" );

    else if ( reject )
	{
	indent_puts( "yy_current_state = *--yy_state_ptr;" );
	indent_puts( "yy_lp = yy_accept[yy_current_state];" );

	puts( "find_rule: /* we branch to this label when backtracking */" );

	indent_puts( "for ( ; ; ) /* until we find what rule we matched */" );

	indent_up();

	indent_puts( "{" );

	indent_puts( "if ( yy_lp && yy_lp < yy_accept[yy_current_state + 1] )" );
	indent_up();
	indent_puts( "{" );
	indent_puts( "yy_act = yy_acclist[yy_lp];" );

	if ( variable_trailing_context_rules )
	    {
	    indent_puts( "if ( yy_act & YY_TRAILING_HEAD_MASK ||" );
	    indent_puts( "     yy_looking_for_trail_begin )" );
	    indent_up();
	    indent_puts( "{" );

	    indent_puts( "if ( yy_act == yy_looking_for_trail_begin )" );
	    indent_up();
	    indent_puts( "{" );
	    indent_puts( "yy_looking_for_trail_begin = 0;" );
	    indent_puts( "yy_act &= ~YY_TRAILING_HEAD_MASK;" );
	    indent_puts( "break;" );
	    indent_puts( "}" );
	    indent_down();

	    indent_puts( "}" );
	    indent_down();

	    indent_puts( "else if ( yy_act & YY_TRAILING_MASK )" );
	    indent_up();
	    indent_puts( "{" );
	    indent_puts(
		"yy_looking_for_trail_begin = yy_act & ~YY_TRAILING_MASK;" );
	    indent_puts(
		"yy_looking_for_trail_begin |= YY_TRAILING_HEAD_MASK;" );

	    if ( real_reject )
		{
		/* remember matched text in case we back up due to REJECT */
		indent_puts( "yy_full_match = yy_cp;" );
		indent_puts( "yy_full_state = yy_state_ptr;" );
		indent_puts( "yy_full_lp = yy_lp;" );
		}

	    indent_puts( "}" );
	    indent_down();

	    indent_puts( "else" );
	    indent_up();
	    indent_puts( "{" );
	    indent_puts( "yy_full_match = yy_cp;" );
	    indent_puts( "yy_full_state = yy_state_ptr;" );
	    indent_puts( "yy_full_lp = yy_lp;" );
	    indent_puts( "break;" );
	    indent_puts( "}" );
	    indent_down();

	    indent_puts( "++yy_lp;" );
	    indent_puts( "goto find_rule;" );
	    }

	else
	    {
	    /* remember matched text in case we back up due to trailing context
	     * plus REJECT
	     */
	    indent_up();
	    indent_puts( "{" );
	    indent_puts( "yy_full_match = yy_cp;" );
	    indent_puts( "break;" );
	    indent_puts( "}" );
	    indent_down();
	    }

	indent_puts( "}" );
	indent_down();

	indent_puts( "--yy_cp;" );

	/* we could consolidate the following two lines with those at
	 * the beginning, but at the cost of complaints that we're
	 * branching inside a loop
	 */
	indent_puts( "yy_current_state = *--yy_state_ptr;" );
	indent_puts( "yy_lp = yy_accept[yy_current_state];" );

	indent_puts( "}" );

	indent_down();
	}

    else
	/* compressed */
	indent_puts( "yy_act = yy_accept[yy_current_state];" );
    }


/* genftbl - generates full transition table
 *
 * synopsis
 *     genftbl();
 */

void genftbl()

    {
    register int i;
    int end_of_buffer_action = num_rules + 1;

    printf( C_short_decl, "yy_accept", lastdfa + 1 );


    dfaacc[end_of_buffer_state].dfaacc_state = end_of_buffer_action;

    for ( i = 1; i <= lastdfa; ++i )
	{
	register int anum = dfaacc[i].dfaacc_state;

	mkdata( anum );

	if ( trace && anum )
	    fprintf( stderr, "state # %d accepts: [%d]\n", i, anum );
	}

    dataend();

    if ( useecs )
	genecs();

    /* don't have to dump the actual full table entries - they were created
     * on-the-fly
     */
    }


/* generate the code to find the next compressed-table state */

void gen_next_compressed_state( char_map )
char *char_map;

    {
    indent_put2s( "register YY_CHAR yy_c = %s;", char_map );

    /* save the backtracking info \before/ computing the next state
     * because we always compute one more state than needed - we
     * always proceed until we reach a jam state
     */
    gen_backtracking();

    indent_puts(
    "while ( yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state )" );
    indent_up();
    indent_puts( "{" );
    indent_puts( "yy_current_state = yy_def[yy_current_state];" );

    if ( usemecs )
	{
	/* we've arrange it so that templates are never chained
	 * to one another.  This means we can afford make a
	 * very simple test to see if we need to convert to
	 * yy_c's meta-equivalence class without worrying
	 * about erroneously looking up the meta-equivalence
	 * class twice
	 */
	do_indent();
	/* lastdfa + 2 is the beginning of the templates */
	printf( "if ( yy_current_state >= %d )\n", lastdfa + 2 );

	indent_up();
	indent_puts( "yy_c = yy_meta[yy_c];" );
	indent_down();
	}

    indent_puts( "}" );
    indent_down();

    indent_puts(
	"yy_current_state = yy_nxt[yy_base[yy_current_state] + yy_c];" );
    }


/* generate the code to find the next match */

void gen_next_match()

    {
    /* NOTE - changes in here should be reflected in gen_next_state() and
     * gen_NUL_trans()
     */
    char *char_map = useecs ? "yy_ec[*yy_cp]" : "*yy_cp";
    char *char_map_2 = useecs ? "yy_ec[*++yy_cp]" : "*++yy_cp";
    
    if ( fulltbl )
	{
	indent_put2s(
	    "while ( (yy_current_state = yy_nxt[yy_current_state][%s]) > 0 )",
		char_map );

	indent_up();

	if ( num_backtracking > 0 )
	    {
	    indent_puts( "{" );
	    gen_backtracking();
	    putchar( '\n' );
	    }

	indent_puts( "++yy_cp;" );

	if ( num_backtracking > 0 )
	    indent_puts( "}" );

	indent_down();

	putchar( '\n' );
	indent_puts( "yy_current_state = -yy_current_state;" );
	}

    else if ( fullspd )
	{
	indent_puts( "{" );
	indent_puts( "register const struct yy_trans_info *yy_trans_info;\n" );
	indent_puts( "register YY_CHAR yy_c;\n" );
	indent_put2s( "for ( yy_c = %s;", char_map );
	indent_puts(
	"      (yy_trans_info = &yy_current_state[yy_c])->yy_verify == yy_c;" );
	indent_put2s( "      yy_c = %s )", char_map_2 );

	indent_up();

	if ( num_backtracking > 0 )
	    indent_puts( "{" );

	indent_puts( "yy_current_state += yy_trans_info->yy_nxt;" );

	if ( num_backtracking > 0 )
	    {
	    putchar( '\n' );
	    gen_backtracking();
	    indent_puts( "}" );
	    }

	indent_down();
	indent_puts( "}" );
	}

    else
	{ /* compressed */
	indent_puts( "do" );

	indent_up();
	indent_puts( "{" );

	gen_next_state( false );

	indent_puts( "++yy_cp;" );

	indent_puts( "}" );
	indent_down();

	do_indent();

	if ( interactive )
	    printf( "while ( yy_base[yy_current_state] != %d );\n", jambase );
	else
	    printf( "while ( yy_current_state != %d );\n", jamstate );

	if ( ! reject && ! interactive )
	    {
	    /* do the guaranteed-needed backtrack to figure out the match */
	    indent_puts( "yy_cp = yy_last_accepting_cpos;" );
	    indent_puts( "yy_current_state = yy_last_accepting_state;" );
	    }
	}
    }


/* generate the code to find the next state */

void gen_next_state( worry_about_NULs )
int worry_about_NULs;

    { /* NOTE - changes in here should be reflected in get_next_match() */
    char char_map[256];

    if ( worry_about_NULs && ! nultrans )
	{
	if ( useecs )
	    (void) sprintf( char_map, "(*yy_cp ? yy_ec[*yy_cp] : %d)", NUL_ec );
	else
	    (void) sprintf( char_map, "(*yy_cp ? *yy_cp : %d)", NUL_ec );
	}

    else
	(void) strcpy( char_map, useecs ? "yy_ec[*yy_cp]" : "*yy_cp" );

    if ( worry_about_NULs && nultrans )
	{
	if ( ! fulltbl && ! fullspd )
	    /* compressed tables backtrack *before* they match */
	    gen_backtracking();

	indent_puts( "if ( *yy_cp )" );
	indent_up();
	indent_puts( "{" );
	}
   
    if ( fulltbl )
	indent_put2s( "yy_current_state = yy_nxt[yy_current_state][%s];", 
		char_map );
    
    else if ( fullspd )
	indent_put2s( "yy_current_state += yy_current_state[%s].yy_nxt;",
		    char_map );

    else
	gen_next_compressed_state( char_map );

    if ( worry_about_NULs && nultrans )
	{
	indent_puts( "}" );
	indent_down();
	indent_puts( "else" );
	indent_up();
	indent_puts( "yy_current_state = yy_NUL_trans[yy_current_state];" );
	indent_down();
	}
    
    if ( fullspd || fulltbl )
	gen_backtracking();

    if ( reject )
	indent_puts( "*yy_state_ptr++ = yy_current_state;" );
    }


/* generate the code to make a NUL transition */

void gen_NUL_trans()

    { /* NOTE - changes in here should be reflected in get_next_match() */
    int need_backtracking = (num_backtracking > 0 && ! reject);

    if ( need_backtracking )
	/* we'll need yy_cp lying around for the gen_backtracking() */
	indent_puts( "register YY_CHAR *yy_cp = yy_c_buf_p;" );

    putchar( '\n' );

    if ( nultrans )
	{
	indent_puts( "yy_current_state = yy_NUL_trans[yy_current_state];" );
	indent_puts( "yy_is_jam = (yy_current_state == 0);" );
	}

    else if ( fulltbl )
	{
	do_indent();
	printf( "yy_current_state = yy_nxt[yy_current_state][%d];\n",
		NUL_ec );
	indent_puts( "yy_is_jam = (yy_current_state <= 0);" );
	}

    else if ( fullspd )
	{
	do_indent();
	printf( "register int yy_c = %d;\n", NUL_ec );

	indent_puts(
	    "register const struct yy_trans_info *yy_trans_info;\n" );
	indent_puts( "yy_trans_info = &yy_current_state[yy_c];" );
	indent_puts( "yy_current_state += yy_trans_info->yy_nxt;" );

	indent_puts( "yy_is_jam = (yy_trans_info->yy_verify != yy_c);" );
	}

    else
	{
	char NUL_ec_str[20];

	(void) sprintf( NUL_ec_str, "%d", NUL_ec );
	gen_next_compressed_state( NUL_ec_str );

	if ( reject )
	    indent_puts( "*yy_state_ptr++ = yy_current_state;" );

	do_indent();

	printf( "yy_is_jam = (yy_current_state == %d);\n", jamstate );
	}

    /* if we've entered an accepting state, backtrack; note that
     * compressed tables have *already* done such backtracking, so
     * we needn't bother with it again
     */
    if ( need_backtracking && (fullspd || fulltbl) )
	{
	putchar( '\n' );
	indent_puts( "if ( ! yy_is_jam )" );
	indent_up();
	indent_puts( "{" );
	gen_backtracking();
	indent_puts( "}" );
	indent_down();
	}
    }


/* generate the code to find the start state */

void gen_start_state()

    {
    if ( fullspd )
	indent_put2s( "yy_current_state = yy_start_state_list[yy_start%s];",
		bol_needed ? " + (yy_bp[-1] == '\\n' ? 1 : 0)" : "" );

    else
	{
	indent_puts( "yy_current_state = yy_start;" );

	if ( bol_needed )
	    {
	    indent_puts( "if ( yy_bp[-1] == '\\n' )" );
	    indent_up();
	    indent_puts( "++yy_current_state;" );
	    indent_down();
	    }

	if ( reject )
	    {
	    /* set up for storing up states */
	    indent_puts( "yy_state_ptr = yy_state_buf;" );
	    indent_puts( "*yy_state_ptr++ = yy_current_state;" );
	    }
	}
    }


/* gentabs - generate data statements for the transition tables
 *
 * synopsis
 *    gentabs();
 */

void gentabs()

    {
    int i, j, k, *accset, nacc, *acc_array, total_states;
    int end_of_buffer_action = num_rules + 1;

    /* *everything* is done in terms of arrays starting at 1, so provide
     * a null entry for the zero element of all C arrays
     */
    static char C_char_decl[] =
	"static const YY_CHAR %s[%d] =\n    {   0,\n";

    acc_array = allocate_integer_array( current_max_dfas );
    nummt = 0;

    /* the compressed table format jams by entering the "jam state",
     * losing information about the previous state in the process.
     * In order to recover the previous state, we effectively need
     * to keep backtracking information.
     */
    ++num_backtracking;

    if ( reject )
	{
	/* write out accepting list and pointer list
	 *
	 * first we generate the "yy_acclist" array.  In the process, we compute
	 * the indices that will go into the "yy_accept" array, and save the
	 * indices in the dfaacc array
	 */
	int EOB_accepting_list[2];

	/* set up accepting structures for the End Of Buffer state */
	EOB_accepting_list[0] = 0;
	EOB_accepting_list[1] = end_of_buffer_action;
	accsiz[end_of_buffer_state] = 1;
	dfaacc[end_of_buffer_state].dfaacc_set = EOB_accepting_list;

	printf( C_short_decl, "yy_acclist", max( numas, 1 ) + 1 );

	j = 1;	/* index into "yy_acclist" array */

	for ( i = 1; i <= lastdfa; ++i )
	    {
	    acc_array[i] = j;

	    if ( accsiz[i] != 0 )
		{
		accset = dfaacc[i].dfaacc_set;
		nacc = accsiz[i];

		if ( trace )
		    fprintf( stderr, "state # %d accepts: ", i );

		for ( k = 1; k <= nacc; ++k )
		    {
		    int accnum = accset[k];

		    ++j;

		    if ( variable_trailing_context_rules &&
			 ! (accnum & YY_TRAILING_HEAD_MASK) &&
			 accnum > 0 && accnum <= num_rules &&
			 rule_type[accnum] == RULE_VARIABLE )
			{
			/* special hack to flag accepting number as part
			 * of trailing context rule
			 */
			accnum |= YY_TRAILING_MASK;
			}

		    mkdata( accnum );

		    if ( trace )
			{
			fprintf( stderr, "[%d]", accset[k] );

			if ( k < nacc )
			    fputs( ", ", stderr );
			else
			    putc( '\n', stderr );
			}
		    }
		}
	    }

	/* add accepting number for the "jam" state */
	acc_array[i] = j;

	dataend();
	}

    else
	{
	dfaacc[end_of_buffer_state].dfaacc_state = end_of_buffer_action;

	for ( i = 1; i <= lastdfa; ++i )
	    acc_array[i] = dfaacc[i].dfaacc_state;

	/* add accepting number for jam state */
	acc_array[i] = 0;
	}

    /* spit out "yy_accept" array.  If we're doing "reject", it'll be pointers
     * into the "yy_acclist" array.  Otherwise it's actual accepting numbers.
     * In either case, we just dump the numbers.
     */

    /* "lastdfa + 2" is the size of "yy_accept"; includes room for C arrays
     * beginning at 0 and for "jam" state
     */
    k = lastdfa + 2;

    if ( reject )
	/* we put a "cap" on the table associating lists of accepting
	 * numbers with state numbers.  This is needed because we tell
	 * where the end of an accepting list is by looking at where
	 * the list for the next state starts.
	 */
	++k;

    printf( C_short_decl, "yy_accept", k );

    for ( i = 1; i <= lastdfa; ++i )
	{
	mkdata( acc_array[i] );

	if ( ! reject && trace && acc_array[i] )
	    fprintf( stderr, "state # %d accepts: [%d]\n", i, acc_array[i] );
	}

    /* add entry for "jam" state */
    mkdata( acc_array[i] );

    if ( reject )
	/* add "cap" for the list */
	mkdata( acc_array[i] );

    dataend();

    if ( useecs )
	genecs();

    if ( usemecs )
	{
	/* write out meta-equivalence classes (used to index templates with) */

	if ( trace )
	    fputs( "\n\nMeta-Equivalence Classes:\n", stderr );

	printf( C_char_decl, "yy_meta", numecs + 1 );

	for ( i = 1; i <= numecs; ++i )
	    {
	    if ( trace )
		fprintf( stderr, "%d = %d\n", i, abs( tecbck[i] ) );

	    mkdata( abs( tecbck[i] ) );
	    }

	dataend();
	}

    total_states = lastdfa + numtemps;

    printf( tblend > MAX_SHORT ? C_long_decl : C_short_decl,
	    "yy_base", total_states + 1 );

    for ( i = 1; i <= lastdfa; ++i )
	{
	register int d = def[i];

	if ( base[i] == JAMSTATE )
	    base[i] = jambase;

	if ( d == JAMSTATE )
	    def[i] = jamstate;

	else if ( d < 0 )
	    {
	    /* template reference */
	    ++tmpuses;
	    def[i] = lastdfa - d + 1;
	    }

	mkdata( base[i] );
	}

    /* generate jam state's base index */
    mkdata( base[i] );

    for ( ++i /* skip jam state */; i <= total_states; ++i )
	{
	mkdata( base[i] );
	def[i] = jamstate;
	}

    dataend();

    printf( tblend > MAX_SHORT ? C_long_decl : C_short_decl,
	    "yy_def", total_states + 1 );

    for ( i = 1; i <= total_states; ++i )
	mkdata( def[i] );

    dataend();

    printf( lastdfa > MAX_SHORT ? C_long_decl : C_short_decl,
	    "yy_nxt", tblend + 1 );

    for ( i = 1; i <= tblend; ++i )
	{
	if ( nxt[i] == 0 || chk[i] == 0 )
	    nxt[i] = jamstate;	/* new state is the JAM state */

	mkdata( nxt[i] );
	}

    dataend();

    printf( lastdfa > MAX_SHORT ? C_long_decl : C_short_decl,
	    "yy_chk", tblend + 1 );

    for ( i = 1; i <= tblend; ++i )
	{
	if ( chk[i] == 0 )
	    ++nummt;

	mkdata( chk[i] );
	}

    dataend();
    }


/* write out a formatted string (with a secondary string argument) at the
 * current indentation level, adding a final newline
 */

void indent_put2s( fmt, arg )
char fmt[], arg[];

    {
    do_indent();
    printf( fmt, arg );
    putchar( '\n' );
    }


/* write out a string at the current indentation level, adding a final
 * newline
 */

void indent_puts( str )
char str[];

    {
    do_indent();
    puts( str );
    }


/* make_tables - generate transition tables
 *
 * synopsis
 *     make_tables();
 *
 * Generates transition tables and finishes generating output file
 */

void make_tables()

    {
    register int i;
    int did_eof_rule = false;

    skelout();

    /* first, take care of YY_DO_BEFORE_ACTION depending on yymore being used */
    set_indent( 2 );

    if ( yymore_used )
	{
	indent_puts( "yytext -= yy_more_len; \\" );
	indent_puts( "yyleng = yy_cp - yytext; \\" );
	}

    else
	indent_puts( "yyleng = yy_cp - yy_bp; \\" );

    set_indent( 0 );
    
    skelout();


    printf( "#define YY_END_OF_BUFFER %d\n", num_rules + 1 );

    if ( fullspd )
	{ /* need to define the transet type as a size large
	   * enough to hold the biggest offset
	   */
	int total_table_size = tblend + numecs + 1;
	char *trans_offset_type =
	    total_table_size > MAX_SHORT ? "long" : "short";

	set_indent( 0 );
	indent_puts( "struct yy_trans_info" );
	indent_up();
        indent_puts( "{" );
        indent_puts( "short yy_verify;" );

        /* in cases where its sister yy_verify *is* a "yes, there is a
	 * transition", yy_nxt is the offset (in records) to the next state.
	 * In most cases where there is no transition, the value of yy_nxt
	 * is irrelevant.  If yy_nxt is the -1th  record of a state, though,
	 * then yy_nxt is the action number for that state
         */

        indent_put2s( "%s yy_nxt;", trans_offset_type );
        indent_puts( "};" );
	indent_down();

	indent_puts( "typedef const struct yy_trans_info *yy_state_type;" );
	}
    
    else
	indent_puts( "typedef int yy_state_type;" );

    if ( fullspd )
	genctbl();

    else if ( fulltbl )
	genftbl();

    else
	gentabs();

    if ( num_backtracking > 0 )
	{
	indent_puts( "static yy_state_type yy_last_accepting_state;" );
	indent_puts( "static YY_CHAR *yy_last_accepting_cpos;\n" );
	}

    if ( nultrans )
	{
	printf( C_state_decl, "yy_NUL_trans", lastdfa + 1 );

	for ( i = 1; i <= lastdfa; ++i )
	    {
	    if ( fullspd )
		{
		if ( nultrans )
		    printf( "    &yy_transition[%d],\n", base[i] );
		else
		    printf( "    0,\n" );
		}
	    
	    else
		mkdata( nultrans[i] );
	    }

	dataend();
	}

    if ( ddebug )
	{ /* spit out table mapping rules to line numbers */
	indent_puts( "extern int yy_flex_debug;" );
	indent_puts( "int yy_flex_debug = 1;\n" );

	printf( C_short_decl, "yy_rule_linenum", num_rules );
	for ( i = 1; i < num_rules; ++i )
	    mkdata( rule_linenum[i] );
	dataend();
	}

    if ( reject )
	{
	/* declare state buffer variables */
	puts(
	"static yy_state_type yy_state_buf[YY_BUF_SIZE + 2], *yy_state_ptr;" );
	puts( "static YY_CHAR *yy_full_match;" );
	puts( "static int yy_lp;" );

	if ( variable_trailing_context_rules )
	    {
	    puts( "static int yy_looking_for_trail_begin = 0;" );
	    puts( "static int yy_full_lp;" );
	    puts( "static int *yy_full_state;" );
	    printf( "#define YY_TRAILING_MASK 0x%x\n",
		    (unsigned int) YY_TRAILING_MASK );
	    printf( "#define YY_TRAILING_HEAD_MASK 0x%x\n",
		    (unsigned int) YY_TRAILING_HEAD_MASK );
	    }

	puts( "#define REJECT \\" );
        puts( "{ \\" );
        puts(
	"*yy_cp = yy_hold_char; /* undo effects of setting up yytext */ \\" );
        puts(
	    "yy_cp = yy_full_match; /* restore poss. backed-over text */ \\" );

	if ( variable_trailing_context_rules )
	    {
	    puts( "yy_lp = yy_full_lp; /* restore orig. accepting pos. */ \\" );
	    puts(
		"yy_state_ptr = yy_full_state; /* restore orig. state */ \\" );
	    puts(
	    "yy_current_state = *yy_state_ptr; /* restore curr. state */ \\" );
	    }

        puts( "++yy_lp; \\" );
        puts( "goto find_rule; \\" );
        puts( "}" );
	}
    
    else
	{
	puts( "/* the intent behind this definition is that it'll catch" );
	puts( " * any uses of REJECT which flex missed" );
	puts( " */" );
	puts( "#define REJECT reject_used_but_not_detected" );
	}
    
    if ( yymore_used )
	{
	indent_puts( "static int yy_more_flag = 0;" );
	indent_puts( "static int yy_doing_yy_more = 0;" );
	indent_puts( "static int yy_more_len = 0;" );
	indent_puts(
	    "#define yymore() { yy_more_flag = 1; }" );
	indent_puts(
	    "#define YY_MORE_ADJ (yy_doing_yy_more ? yy_more_len : 0)" );
	}

    else
	{
	indent_puts( "#define yymore() yymore_used_but_not_detected" );
	indent_puts( "#define YY_MORE_ADJ 0" );
	}

    skelout();

    if ( ferror( temp_action_file ) )
	flexfatal( "error occurred when writing temporary action file" );

    else if ( fclose( temp_action_file ) )
	flexfatal( "error occurred when closing temporary action file" );

    temp_action_file = fopen( action_file_name, "r" );

    if ( temp_action_file == NULL )
	flexfatal( "could not re-open temporary action file" );

    /* copy prolog from action_file to output file */
    action_out();

    skelout();

    set_indent( 2 );

    if ( yymore_used )
	{
	indent_puts( "yy_more_len = 0;" );
	indent_puts( "yy_doing_yy_more = yy_more_flag;" );
	indent_puts( "if ( yy_doing_yy_more )" );
	indent_up();
	indent_puts( "{" );
	indent_puts( "yy_more_len = yyleng;" );
	indent_puts( "yy_more_flag = 0;" );
	indent_puts( "}" );
	indent_down();
	}

    skelout();

    gen_start_state();

    /* note, don't use any indentation */
    puts( "yy_match:" );
    gen_next_match();

    skelout();
    set_indent( 2 );
    gen_find_action();

    skelout();
    if ( ddebug )
	{
	indent_puts( "if ( yy_flex_debug )" );
	indent_up();

	indent_puts( "{" );
	indent_puts( "if ( yy_act == 0 )" );
	indent_up();
	indent_puts( "fprintf( stderr, \"--scanner backtracking\\n\" );" );
	indent_down();

	do_indent();
	printf( "else if ( yy_act < %d )\n", num_rules );
	indent_up();
	indent_puts(
	"fprintf( stderr, \"--accepting rule at line %d (\\\"%s\\\")\\n\"," );
	indent_puts( "         yy_rule_linenum[yy_act], yytext );" );
	indent_down();

	do_indent();
	printf( "else if ( yy_act == %d )\n", num_rules );
	indent_up();
	indent_puts(
	"fprintf( stderr, \"--accepting default rule (\\\"%s\\\")\\n\"," );
	indent_puts( "         yytext );" );
	indent_down();

	do_indent();
	printf( "else if ( yy_act == %d )\n", num_rules + 1 );
	indent_up();
	indent_puts( "fprintf( stderr, \"--(end of buffer or a NUL)\\n\" );" );
	indent_down();

	do_indent();
	printf( "else\n" );
	indent_up();
	indent_puts( "fprintf( stderr, \"--EOF\\n\" );" );
	indent_down();

	indent_puts( "}" );
	indent_down();
	}

    /* copy actions from action_file to output file */
    skelout();
    indent_up();
    gen_bt_action();
    action_out();

    /* generate cases for any missing EOF rules */
    for ( i = 1; i <= lastsc; ++i )
	if ( ! sceof[i] )
	    {
	    do_indent();
	    printf( "case YY_STATE_EOF(%s):\n", scname[i] );
	    did_eof_rule = true;
	    }
    
    if ( did_eof_rule )
	{
	indent_up();
	indent_puts( "yyterminate();" );
	indent_down();
	}


    /* generate code for handling NUL's, if needed */

    /* first, deal with backtracking and setting up yy_cp if the scanner
     * finds that it should JAM on the NUL
     */
    skelout();
    set_indent( 7 );

    if ( fullspd || fulltbl )
	indent_puts( "yy_cp = yy_c_buf_p;" );
    
    else
	{ /* compressed table */
	if ( ! reject && ! interactive )
	    {
	    /* do the guaranteed-needed backtrack to figure out the match */
	    indent_puts( "yy_cp = yy_last_accepting_cpos;" );
	    indent_puts( "yy_current_state = yy_last_accepting_state;" );
	    }
	}


    /* generate code for yy_get_previous_state() */
    set_indent( 1 );
    skelout();

    if ( bol_needed )
	indent_puts( "register YY_CHAR *yy_bp = yytext;\n" );

    gen_start_state();

    set_indent( 2 );
    skelout();
    gen_next_state( true );

    set_indent( 1 );
    skelout();
    gen_NUL_trans();

    skelout();

    /* copy remainder of input to output */

    line_directive_out( stdout );
    (void) flexscan(); /* copy remainder of input to output */
    }
