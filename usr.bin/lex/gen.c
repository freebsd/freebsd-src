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

/* $Header: /home/daffy/u0/vern/flex/RCS/gen.c,v 2.56 96/05/25 20:43:38 vern Exp $ */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "flexdef.h"


/* declare functions that have forward references */

void gen_next_state PROTO((int));
void genecs PROTO((void));
void indent_put2s PROTO((char [], char []));
void indent_puts PROTO((char []));


static int indent_level = 0; /* each level is 8 spaces */

#define indent_up() (++indent_level)
#define indent_down() (--indent_level)
#define set_indent(indent_val) indent_level = indent_val

/* Almost everything is done in terms of arrays starting at 1, so provide
 * a null entry for the zero element of all C arrays.  (The exception
 * to this is that the fast table representation generally uses the
 * 0 elements of its arrays, too.)
 */
static char C_int_decl[] = "static yyconst int %s[%d] =\n    {   0,\n";
static char C_short_decl[] = "static yyconst short int %s[%d] =\n    {   0,\n";
static char C_long_decl[] = "static yyconst long int %s[%d] =\n    {   0,\n";
static char C_state_decl[] =
	"static yyconst yy_state_type %s[%d] =\n    {   0,\n";


/* Indent to the current level. */

void do_indent()
	{
	register int i = indent_level * 8;

	while ( i >= 8 )
		{
		outc( '\t' );
		i -= 8;
		}

	while ( i > 0 )
		{
		outc( ' ' );
		--i;
		}
	}


/* Generate the code to keep backing-up information. */

void gen_backing_up()
	{
	if ( reject || num_backing_up == 0 )
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


/* Generate the code to perform the backing up. */

void gen_bu_action()
	{
	if ( reject || num_backing_up == 0 )
		return;

	set_indent( 3 );

	indent_puts( "case 0: /* must back up */" );
	indent_puts( "/* undo the effects of YY_DO_BEFORE_ACTION */" );
	indent_puts( "*yy_cp = yy_hold_char;" );

	if ( fullspd || fulltbl )
		indent_puts( "yy_cp = yy_last_accepting_cpos + 1;" );
	else
		/* Backing-up info for compressed tables is taken \after/
		 * yy_cp has been incremented for the next state.
		 */
		indent_puts( "yy_cp = yy_last_accepting_cpos;" );

	indent_puts( "yy_current_state = yy_last_accepting_state;" );
	indent_puts( "goto yy_find_action;" );
	outc( '\n' );

	set_indent( 0 );
	}


/* genctbl - generates full speed compressed transition table */

void genctbl()
	{
	register int i;
	int end_of_buffer_action = num_rules + 1;

	/* Table of verify for transition and offset to next state. */
	out_dec( "static yyconst struct yy_trans_info yy_transition[%d] =\n",
		tblend + numecs + 1 );
	outn( "    {" );

	/* We want the transition to be represented as the offset to the
	 * next state, not the actual state number, which is what it currently
	 * is.  The offset is base[nxt[i]] - (base of current state)].  That's
	 * just the difference between the starting points of the two involved
	 * states (to - from).
	 *
	 * First, though, we need to find some way to put in our end-of-buffer
	 * flags and states.  We do this by making a state with absolutely no
	 * transitions.  We put it at the end of the table.
	 */

	/* We need to have room in nxt/chk for two more slots: One for the
	 * action and one for the end-of-buffer transition.  We now *assume*
	 * that we're guaranteed the only character we'll try to index this
	 * nxt/chk pair with is EOB, i.e., 0, so we don't have to make sure
	 * there's room for jam entries for other characters.
	 */

	while ( tblend + 2 >= current_max_xpairs )
		expand_nxt_chk();

	while ( lastdfa + 1 >= current_max_dfas )
		increase_max_dfas();

	base[lastdfa + 1] = tblend + 2;
	nxt[tblend + 1] = end_of_buffer_action;
	chk[tblend + 1] = numecs + 1;
	chk[tblend + 2] = 1; /* anything but EOB */

	/* So that "make test" won't show arb. differences. */
	nxt[tblend + 2] = 0;

	/* Make sure every state has an end-of-buffer transition and an
	 * action #.
	 */
	for ( i = 0; i <= lastdfa; ++i )
		{
		int anum = dfaacc[i].dfaacc_state;
		int offset = base[i];

		chk[offset] = EOB_POSITION;
		chk[offset - 1] = ACTION_POSITION;
		nxt[offset - 1] = anum;	/* action number */
		}

	for ( i = 0; i <= tblend; ++i )
		{
		if ( chk[i] == EOB_POSITION )
			transition_struct_out( 0, base[lastdfa + 1] - i );

		else if ( chk[i] == ACTION_POSITION )
			transition_struct_out( 0, nxt[i] );

		else if ( chk[i] > numecs || chk[i] == 0 )
			transition_struct_out( 0, 0 );	/* unused slot */

		else	/* verify, transition */
			transition_struct_out( chk[i],
						base[nxt[i]] - (i - chk[i]) );
		}


	/* Here's the final, end-of-buffer state. */
	transition_struct_out( chk[tblend + 1], nxt[tblend + 1] );
	transition_struct_out( chk[tblend + 2], nxt[tblend + 2] );

	outn( "    };\n" );

	/* Table of pointers to start states. */
	out_dec(
	"static yyconst struct yy_trans_info *yy_start_state_list[%d] =\n",
		lastsc * 2 + 1 );
	outn( "    {" );	/* } so vi doesn't get confused */

	for ( i = 0; i <= lastsc * 2; ++i )
		out_dec( "    &yy_transition[%d],\n", base[i] );

	dataend();

	if ( useecs )
		genecs();
	}


/* Generate equivalence-class tables. */

void genecs()
	{
	register int i, j;
	int numrows;

	out_str_dec( C_int_decl, "yy_ec", csize );

	for ( i = 1; i < csize; ++i )
		{
		if ( caseins && (i >= 'A') && (i <= 'Z') )
			ecgroup[i] = ecgroup[clower( i )];

		ecgroup[i] = ABS( ecgroup[i] );
		mkdata( ecgroup[i] );
		}

	dataend();

	if ( trace )
		{
		fputs( _( "\n\nEquivalence Classes:\n\n" ), stderr );

		numrows = csize / 8;

		for ( j = 0; j < numrows; ++j )
			{
			for ( i = j; i < csize; i = i + numrows )
				{
				fprintf( stderr, "%4s = %-2d",
					readable_form( i ), ecgroup[i] );

				putc( ' ', stderr );
				}

			putc( '\n', stderr );
			}
		}
	}


/* Generate the code to find the action number. */

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

		outn(
		"goto find_rule; /* avoid `defined but not used' warning */");
		outn(
		"find_rule: /* we branch to this label when backing up */" );

		indent_puts(
		"for ( ; ; ) /* until we find what rule we matched */" );

		indent_up();

		indent_puts( "{" );

		indent_puts(
		"if ( yy_lp && yy_lp < yy_accept[yy_current_state + 1] )" );
		indent_up();
		indent_puts( "{" );
		indent_puts( "yy_act = yy_acclist[yy_lp];" );

		if ( variable_trailing_context_rules )
			{
			indent_puts( "if ( yy_act & YY_TRAILING_HEAD_MASK ||" );
			indent_puts( "     yy_looking_for_trail_begin )" );
			indent_up();
			indent_puts( "{" );

			indent_puts(
				"if ( yy_act == yy_looking_for_trail_begin )" );
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
				/* Remember matched text in case we back up
				 * due to REJECT.
				 */
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
			/* Remember matched text in case we back up due to
			 * trailing context plus REJECT.
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

		/* We could consolidate the following two lines with those at
		 * the beginning, but at the cost of complaints that we're
		 * branching inside a loop.
		 */
		indent_puts( "yy_current_state = *--yy_state_ptr;" );
		indent_puts( "yy_lp = yy_accept[yy_current_state];" );

		indent_puts( "}" );

		indent_down();
		}

	else
		{ /* compressed */
		indent_puts( "yy_act = yy_accept[yy_current_state];" );

		if ( interactive && ! reject )
			{
			/* Do the guaranteed-needed backing up to figure out
			 * the match.
			 */
			indent_puts( "if ( yy_act == 0 )" );
			indent_up();
			indent_puts( "{ /* have to back up */" );
			indent_puts( "yy_cp = yy_last_accepting_cpos;" );
			indent_puts(
				"yy_current_state = yy_last_accepting_state;" );
			indent_puts( "yy_act = yy_accept[yy_current_state];" );
			indent_puts( "}" );
			indent_down();
			}
		}
	}


/* genftbl - generate full transition table */

void genftbl()
	{
	register int i;
	int end_of_buffer_action = num_rules + 1;

	out_str_dec( long_align ? C_long_decl : C_short_decl,
		"yy_accept", lastdfa + 1 );

	dfaacc[end_of_buffer_state].dfaacc_state = end_of_buffer_action;

	for ( i = 1; i <= lastdfa; ++i )
		{
		register int anum = dfaacc[i].dfaacc_state;

		mkdata( anum );

		if ( trace && anum )
			fprintf( stderr, _( "state # %d accepts: [%d]\n" ),
				i, anum );
		}

	dataend();

	if ( useecs )
		genecs();

	/* Don't have to dump the actual full table entries - they were
	 * created on-the-fly.
	 */
	}


/* Generate the code to find the next compressed-table state. */

void gen_next_compressed_state( char_map )
char *char_map;
	{
	indent_put2s( "register YY_CHAR yy_c = %s;", char_map );

	/* Save the backing-up info \before/ computing the next state
	 * because we always compute one more state than needed - we
	 * always proceed until we reach a jam state
	 */
	gen_backing_up();

	indent_puts(
"while ( yy_chk[yy_base[yy_current_state] + yy_c] != yy_current_state )" );
	indent_up();
	indent_puts( "{" );
	indent_puts( "yy_current_state = (int) yy_def[yy_current_state];" );

	if ( usemecs )
		{
		/* We've arrange it so that templates are never chained
		 * to one another.  This means we can afford to make a
		 * very simple test to see if we need to convert to
		 * yy_c's meta-equivalence class without worrying
		 * about erroneously looking up the meta-equivalence
		 * class twice
		 */
		do_indent();

		/* lastdfa + 2 is the beginning of the templates */
		out_dec( "if ( yy_current_state >= %d )\n", lastdfa + 2 );

		indent_up();
		indent_puts( "yy_c = yy_meta[(unsigned int) yy_c];" );
		indent_down();
		}

	indent_puts( "}" );
	indent_down();

	indent_puts(
"yy_current_state = yy_nxt[yy_base[yy_current_state] + (unsigned int) yy_c];" );
	}


/* Generate the code to find the next match. */

void gen_next_match()
	{
	/* NOTE - changes in here should be reflected in gen_next_state() and
	 * gen_NUL_trans().
	 */
	char *char_map = useecs ?
				"yy_ec[YY_SC_TO_UI(*yy_cp)]" :
				"YY_SC_TO_UI(*yy_cp)";

	char *char_map_2 = useecs ?
				"yy_ec[YY_SC_TO_UI(*++yy_cp)]" :
				"YY_SC_TO_UI(*++yy_cp)";

	if ( fulltbl )
		{
		indent_put2s(
	"while ( (yy_current_state = yy_nxt[yy_current_state][%s]) > 0 )",
				char_map );

		indent_up();

		if ( num_backing_up > 0 )
			{
			indent_puts( "{" );	/* } for vi */
			gen_backing_up();
			outc( '\n' );
			}

		indent_puts( "++yy_cp;" );

		if ( num_backing_up > 0 )
			/* { for vi */
			indent_puts( "}" );

		indent_down();

		outc( '\n' );
		indent_puts( "yy_current_state = -yy_current_state;" );
		}

	else if ( fullspd )
		{
		indent_puts( "{" );	/* } for vi */
		indent_puts(
		"register yyconst struct yy_trans_info *yy_trans_info;\n" );
		indent_puts( "register YY_CHAR yy_c;\n" );
		indent_put2s( "for ( yy_c = %s;", char_map );
		indent_puts(
	"      (yy_trans_info = &yy_current_state[(unsigned int) yy_c])->" );
		indent_puts( "yy_verify == yy_c;" );
		indent_put2s( "      yy_c = %s )", char_map_2 );

		indent_up();

		if ( num_backing_up > 0 )
			indent_puts( "{" );	/* } for vi */

		indent_puts( "yy_current_state += yy_trans_info->yy_nxt;" );

		if ( num_backing_up > 0 )
			{
			outc( '\n' );
			gen_backing_up();	/* { for vi */
			indent_puts( "}" );
			}

		indent_down();	/* { for vi */
		indent_puts( "}" );
		}

	else
		{ /* compressed */
		indent_puts( "do" );

		indent_up();
		indent_puts( "{" );	/* } for vi */

		gen_next_state( false );

		indent_puts( "++yy_cp;" );

		/* { for vi */
		indent_puts( "}" );
		indent_down();

		do_indent();

		if ( interactive )
			out_dec( "while ( yy_base[yy_current_state] != %d );\n",
				jambase );
		else
			out_dec( "while ( yy_current_state != %d );\n",
				jamstate );

		if ( ! reject && ! interactive )
			{
			/* Do the guaranteed-needed backing up to figure out
			 * the match.
			 */
			indent_puts( "yy_cp = yy_last_accepting_cpos;" );
			indent_puts(
				"yy_current_state = yy_last_accepting_state;" );
			}
		}
	}


/* Generate the code to find the next state. */

void gen_next_state( worry_about_NULs )
int worry_about_NULs;
	{ /* NOTE - changes in here should be reflected in gen_next_match() */
	char char_map[256];

	if ( worry_about_NULs && ! nultrans )
		{
		if ( useecs )
			(void) sprintf( char_map,
				"(*yy_cp ? yy_ec[YY_SC_TO_UI(*yy_cp)] : %d)",
					NUL_ec );
		else
			(void) sprintf( char_map,
				"(*yy_cp ? YY_SC_TO_UI(*yy_cp) : %d)", NUL_ec );
		}

	else
		strcpy( char_map, useecs ?
			"yy_ec[YY_SC_TO_UI(*yy_cp)]" : "YY_SC_TO_UI(*yy_cp)" );

	if ( worry_about_NULs && nultrans )
		{
		if ( ! fulltbl && ! fullspd )
			/* Compressed tables back up *before* they match. */
			gen_backing_up();

		indent_puts( "if ( *yy_cp )" );
		indent_up();
		indent_puts( "{" );	/* } for vi */
		}

	if ( fulltbl )
		indent_put2s(
			"yy_current_state = yy_nxt[yy_current_state][%s];", 
				char_map );

	else if ( fullspd )
		indent_put2s(
			"yy_current_state += yy_current_state[%s].yy_nxt;",
				char_map );

	else
		gen_next_compressed_state( char_map );

	if ( worry_about_NULs && nultrans )
		{
		/* { for vi */
		indent_puts( "}" );
		indent_down();
		indent_puts( "else" );
		indent_up();
		indent_puts(
			"yy_current_state = yy_NUL_trans[yy_current_state];" );
		indent_down();
		}

	if ( fullspd || fulltbl )
		gen_backing_up();

	if ( reject )
		indent_puts( "*yy_state_ptr++ = yy_current_state;" );
	}


/* Generate the code to make a NUL transition. */

void gen_NUL_trans()
	{ /* NOTE - changes in here should be reflected in gen_next_match() */
	/* Only generate a definition for "yy_cp" if we'll generate code
	 * that uses it.  Otherwise lint and the like complain.
	 */
	int need_backing_up = (num_backing_up > 0 && ! reject);

	if ( need_backing_up && (! nultrans || fullspd || fulltbl) )
		/* We're going to need yy_cp lying around for the call
		 * below to gen_backing_up().
		 */
		indent_puts( "register char *yy_cp = yy_c_buf_p;" );

	outc( '\n' );

	if ( nultrans )
		{
		indent_puts(
			"yy_current_state = yy_NUL_trans[yy_current_state];" );
		indent_puts( "yy_is_jam = (yy_current_state == 0);" );
		}

	else if ( fulltbl )
		{
		do_indent();
		out_dec( "yy_current_state = yy_nxt[yy_current_state][%d];\n",
			NUL_ec );
		indent_puts( "yy_is_jam = (yy_current_state <= 0);" );
		}

	else if ( fullspd )
		{
		do_indent();
		out_dec( "register int yy_c = %d;\n", NUL_ec );

		indent_puts(
		"register yyconst struct yy_trans_info *yy_trans_info;\n" );
		indent_puts(
		"yy_trans_info = &yy_current_state[(unsigned int) yy_c];" );
		indent_puts( "yy_current_state += yy_trans_info->yy_nxt;" );

		indent_puts(
			"yy_is_jam = (yy_trans_info->yy_verify != yy_c);" );
		}

	else
		{
		char NUL_ec_str[20];

		(void) sprintf( NUL_ec_str, "%d", NUL_ec );
		gen_next_compressed_state( NUL_ec_str );

		do_indent();
		out_dec( "yy_is_jam = (yy_current_state == %d);\n", jamstate );

		if ( reject )
			{
			/* Only stack this state if it's a transition we
			 * actually make.  If we stack it on a jam, then
			 * the state stack and yy_c_buf_p get out of sync.
			 */
			indent_puts( "if ( ! yy_is_jam )" );
			indent_up();
			indent_puts( "*yy_state_ptr++ = yy_current_state;" );
			indent_down();
			}
		}

	/* If we've entered an accepting state, back up; note that
	 * compressed tables have *already* done such backing up, so
	 * we needn't bother with it again.
	 */
	if ( need_backing_up && (fullspd || fulltbl) )
		{
		outc( '\n' );
		indent_puts( "if ( ! yy_is_jam )" );
		indent_up();
		indent_puts( "{" );
		gen_backing_up();
		indent_puts( "}" );
		indent_down();
		}
	}


/* Generate the code to find the start state. */

void gen_start_state()
	{
	if ( fullspd )
		{
		if ( bol_needed )
			{
			indent_puts(
	"yy_current_state = yy_start_state_list[yy_start + YY_AT_BOL()];" );
			}
		else
			indent_puts(
			"yy_current_state = yy_start_state_list[yy_start];" );
		}

	else
		{
		indent_puts( "yy_current_state = yy_start;" );

		if ( bol_needed )
			indent_puts( "yy_current_state += YY_AT_BOL();" );

		if ( reject )
			{
			/* Set up for storing up states. */
			indent_puts( "yy_state_ptr = yy_state_buf;" );
			indent_puts( "*yy_state_ptr++ = yy_current_state;" );
			}
		}
	}


/* gentabs - generate data statements for the transition tables */

void gentabs()
	{
	int i, j, k, *accset, nacc, *acc_array, total_states;
	int end_of_buffer_action = num_rules + 1;

	acc_array = allocate_integer_array( current_max_dfas );
	nummt = 0;

	/* The compressed table format jams by entering the "jam state",
	 * losing information about the previous state in the process.
	 * In order to recover the previous state, we effectively need
	 * to keep backing-up information.
	 */
	++num_backing_up;

	if ( reject )
		{
		/* Write out accepting list and pointer list.
		 *
		 * First we generate the "yy_acclist" array.  In the process,
		 * we compute the indices that will go into the "yy_accept"
		 * array, and save the indices in the dfaacc array.
		 */
		int EOB_accepting_list[2];

		/* Set up accepting structures for the End Of Buffer state. */
		EOB_accepting_list[0] = 0;
		EOB_accepting_list[1] = end_of_buffer_action;
		accsiz[end_of_buffer_state] = 1;
		dfaacc[end_of_buffer_state].dfaacc_set = EOB_accepting_list;

		out_str_dec( long_align ? C_long_decl : C_short_decl,
			"yy_acclist", MAX( numas, 1 ) + 1 );

		j = 1;	/* index into "yy_acclist" array */

		for ( i = 1; i <= lastdfa; ++i )
			{
			acc_array[i] = j;

			if ( accsiz[i] != 0 )
				{
				accset = dfaacc[i].dfaacc_set;
				nacc = accsiz[i];

				if ( trace )
					fprintf( stderr,
						_( "state # %d accepts: " ),
						i );

				for ( k = 1; k <= nacc; ++k )
					{
					int accnum = accset[k];

					++j;

					if ( variable_trailing_context_rules &&
					  ! (accnum & YY_TRAILING_HEAD_MASK) &&
					   accnum > 0 && accnum <= num_rules &&
					  rule_type[accnum] == RULE_VARIABLE )
						{
						/* Special hack to flag
						 * accepting number as part
						 * of trailing context rule.
						 */
						accnum |= YY_TRAILING_MASK;
						}

					mkdata( accnum );

					if ( trace )
						{
						fprintf( stderr, "[%d]",
							accset[k] );

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

	/* Spit out "yy_accept" array.  If we're doing "reject", it'll be
	 * pointers into the "yy_acclist" array.  Otherwise it's actual
	 * accepting numbers.  In either case, we just dump the numbers.
	 */

	/* "lastdfa + 2" is the size of "yy_accept"; includes room for C arrays
	 * beginning at 0 and for "jam" state.
	 */
	k = lastdfa + 2;

	if ( reject )
		/* We put a "cap" on the table associating lists of accepting
		 * numbers with state numbers.  This is needed because we tell
		 * where the end of an accepting list is by looking at where
		 * the list for the next state starts.
		 */
		++k;

	out_str_dec( long_align ? C_long_decl : C_short_decl, "yy_accept", k );

	for ( i = 1; i <= lastdfa; ++i )
		{
		mkdata( acc_array[i] );

		if ( ! reject && trace && acc_array[i] )
			fprintf( stderr, _( "state # %d accepts: [%d]\n" ),
				i, acc_array[i] );
		}

	/* Add entry for "jam" state. */
	mkdata( acc_array[i] );

	if ( reject )
		/* Add "cap" for the list. */
		mkdata( acc_array[i] );

	dataend();

	if ( useecs )
		genecs();

	if ( usemecs )
		{
		/* Write out meta-equivalence classes (used to index
		 * templates with).
		 */

		if ( trace )
			fputs( _( "\n\nMeta-Equivalence Classes:\n" ),
			      stderr );

		out_str_dec( C_int_decl, "yy_meta", numecs + 1 );

		for ( i = 1; i <= numecs; ++i )
			{
			if ( trace )
				fprintf( stderr, "%d = %d\n",
					i, ABS( tecbck[i] ) );

			mkdata( ABS( tecbck[i] ) );
			}

		dataend();
		}

	total_states = lastdfa + numtemps;

	out_str_dec( (tblend >= MAX_SHORT || long_align) ?
			C_long_decl : C_short_decl,
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
			/* Template reference. */
			++tmpuses;
			def[i] = lastdfa - d + 1;
			}

		mkdata( base[i] );
		}

	/* Generate jam state's base index. */
	mkdata( base[i] );

	for ( ++i /* skip jam state */; i <= total_states; ++i )
		{
		mkdata( base[i] );
		def[i] = jamstate;
		}

	dataend();

	out_str_dec( (total_states >= MAX_SHORT || long_align) ?
			C_long_decl : C_short_decl,
		"yy_def", total_states + 1 );

	for ( i = 1; i <= total_states; ++i )
		mkdata( def[i] );

	dataend();

	out_str_dec( (total_states >= MAX_SHORT || long_align) ?
			C_long_decl : C_short_decl,
		"yy_nxt", tblend + 1 );

	for ( i = 1; i <= tblend; ++i )
		{
		/* Note, the order of the following test is important.
		 * If chk[i] is 0, then nxt[i] is undefined.
		 */
		if ( chk[i] == 0 || nxt[i] == 0 )
			nxt[i] = jamstate;	/* new state is the JAM state */

		mkdata( nxt[i] );
		}

	dataend();

	out_str_dec( (total_states >= MAX_SHORT || long_align) ?
			C_long_decl : C_short_decl,
		"yy_chk", tblend + 1 );

	for ( i = 1; i <= tblend; ++i )
		{
		if ( chk[i] == 0 )
			++nummt;

		mkdata( chk[i] );
		}

	dataend();
	}


/* Write out a formatted string (with a secondary string argument) at the
 * current indentation level, adding a final newline.
 */

void indent_put2s( fmt, arg )
char fmt[], arg[];
	{
	do_indent();
	out_str( fmt, arg );
	outn( "" );
	}


/* Write out a string at the current indentation level, adding a final
 * newline.
 */

void indent_puts( str )
char str[];
	{
	do_indent();
	outn( str );
	}


/* make_tables - generate transition tables and finishes generating output file
 */

void make_tables()
	{
	register int i;
	int did_eof_rule = false;

	skelout();

	/* First, take care of YY_DO_BEFORE_ACTION depending on yymore
	 * being used.
	 */
	set_indent( 1 );

	if ( yymore_used && ! yytext_is_array )
		{
		indent_puts( "yytext_ptr -= yy_more_len; \\" );
		indent_puts( "yyleng = (int) (yy_cp - yytext_ptr); \\" );
		}

	else
		indent_puts( "yyleng = (int) (yy_cp - yy_bp); \\" );

	/* Now also deal with copying yytext_ptr to yytext if needed. */
	skelout();
	if ( yytext_is_array )
		{
		if ( yymore_used )
			indent_puts(
				"if ( yyleng + yy_more_offset >= YYLMAX ) \\" );
		else
			indent_puts( "if ( yyleng >= YYLMAX ) \\" );

		indent_up();
		indent_puts(
		"YY_FATAL_ERROR( \"token too large, exceeds YYLMAX\" ); \\" );
		indent_down();

		if ( yymore_used )
			{
			indent_puts(
"yy_flex_strncpy( &yytext[yy_more_offset], yytext_ptr, yyleng + 1 ); \\" );
			indent_puts( "yyleng += yy_more_offset; \\" );
			indent_puts(
				"yy_prev_more_offset = yy_more_offset; \\" );
			indent_puts( "yy_more_offset = 0; \\" );
			}
		else
			{
			indent_puts(
		"yy_flex_strncpy( yytext, yytext_ptr, yyleng + 1 ); \\" );
			}
		}

	set_indent( 0 );

	skelout();


	out_dec( "#define YY_NUM_RULES %d\n", num_rules );
	out_dec( "#define YY_END_OF_BUFFER %d\n", num_rules + 1 );

	if ( fullspd )
		{
		/* Need to define the transet type as a size large
		 * enough to hold the biggest offset.
		 */
		int total_table_size = tblend + numecs + 1;
		char *trans_offset_type =
			(total_table_size >= MAX_SHORT || long_align) ?
				"long" : "short";

		set_indent( 0 );
		indent_puts( "struct yy_trans_info" );
		indent_up();
		indent_puts( "{" ); 	/* } for vi */

		if ( long_align )
			indent_puts( "long yy_verify;" );
		else
			indent_puts( "short yy_verify;" );

		/* In cases where its sister yy_verify *is* a "yes, there is
		 * a transition", yy_nxt is the offset (in records) to the
		 * next state.  In most cases where there is no transition,
		 * the value of yy_nxt is irrelevant.  If yy_nxt is the -1th
		 * record of a state, though, then yy_nxt is the action number
		 * for that state.
		 */

		indent_put2s( "%s yy_nxt;", trans_offset_type );
		indent_puts( "};" );
		indent_down();
		}

	if ( fullspd )
		genctbl();
	else if ( fulltbl )
		genftbl();
	else
		gentabs();

	/* Definitions for backing up.  We don't need them if REJECT
	 * is being used because then we use an alternative backin-up
	 * technique instead.
	 */
	if ( num_backing_up > 0 && ! reject )
		{
		if ( ! C_plus_plus )
			{
			indent_puts(
			"static yy_state_type yy_last_accepting_state;" );
			indent_puts(
				"static char *yy_last_accepting_cpos;\n" );
			}
		}

	if ( nultrans )
		{
		out_str_dec( C_state_decl, "yy_NUL_trans", lastdfa + 1 );

		for ( i = 1; i <= lastdfa; ++i )
			{
			if ( fullspd )
				out_dec( "    &yy_transition[%d],\n", base[i] );
			else
				mkdata( nultrans[i] );
			}

		dataend();
		}

	if ( ddebug )
		{ /* Spit out table mapping rules to line numbers. */
		if ( ! C_plus_plus )
			{
			indent_puts( "extern int yy_flex_debug;" );
			indent_puts( "int yy_flex_debug = 1;\n" );
			}

		out_str_dec( long_align ? C_long_decl : C_short_decl,
			"yy_rule_linenum", num_rules );
		for ( i = 1; i < num_rules; ++i )
			mkdata( rule_linenum[i] );
		dataend();
		}

	if ( reject )
		{
		/* Declare state buffer variables. */
		if ( ! C_plus_plus )
			{
			outn(
	"static yy_state_type yy_state_buf[YY_BUF_SIZE + 2], *yy_state_ptr;" );
			outn( "static char *yy_full_match;" );
			outn( "static int yy_lp;" );
			}

		if ( variable_trailing_context_rules )
			{
			if ( ! C_plus_plus )
				{
				outn(
				"static int yy_looking_for_trail_begin = 0;" );
				outn( "static int yy_full_lp;" );
				outn( "static int *yy_full_state;" );
				}

			out_hex( "#define YY_TRAILING_MASK 0x%x\n",
				(unsigned int) YY_TRAILING_MASK );
			out_hex( "#define YY_TRAILING_HEAD_MASK 0x%x\n",
				(unsigned int) YY_TRAILING_HEAD_MASK );
			}

		outn( "#define REJECT \\" );
		outn( "{ \\" );		/* } for vi */
		outn(
	"*yy_cp = yy_hold_char; /* undo effects of setting up yytext */ \\" );
		outn(
	"yy_cp = yy_full_match; /* restore poss. backed-over text */ \\" );

		if ( variable_trailing_context_rules )
			{
			outn(
		"yy_lp = yy_full_lp; /* restore orig. accepting pos. */ \\" );
			outn(
		"yy_state_ptr = yy_full_state; /* restore orig. state */ \\" );
			outn(
	"yy_current_state = *yy_state_ptr; /* restore curr. state */ \\" );
			}

		outn( "++yy_lp; \\" );
		outn( "goto find_rule; \\" );
		/* { for vi */
		outn( "}" );
		}

	else
		{
		outn(
		"/* The intent behind this definition is that it'll catch" );
		outn( " * any uses of REJECT which flex missed." );
		outn( " */" );
		outn( "#define REJECT reject_used_but_not_detected" );
		}

	if ( yymore_used )
		{
		if ( ! C_plus_plus )
			{
			if ( yytext_is_array )
				{
				indent_puts( "static int yy_more_offset = 0;" );
				indent_puts(
					"static int yy_prev_more_offset = 0;" );
				}
			else
				{
				indent_puts( "static int yy_more_flag = 0;" );
				indent_puts( "static int yy_more_len = 0;" );
				}
			}

		if ( yytext_is_array )
			{
			indent_puts(
	"#define yymore() (yy_more_offset = yy_flex_strlen( yytext ))" );
			indent_puts( "#define YY_NEED_STRLEN" );
			indent_puts( "#define YY_MORE_ADJ 0" );
			indent_puts( "#define YY_RESTORE_YY_MORE_OFFSET \\" );
			indent_up();
			indent_puts( "{ \\" );
			indent_puts( "yy_more_offset = yy_prev_more_offset; \\" );
			indent_puts( "yyleng -= yy_more_offset; \\" );
			indent_puts( "}" );
			indent_down();
			}
		else
			{
			indent_puts( "#define yymore() (yy_more_flag = 1)" );
			indent_puts( "#define YY_MORE_ADJ yy_more_len" );
			indent_puts( "#define YY_RESTORE_YY_MORE_OFFSET" );
			}
		}

	else
		{
		indent_puts( "#define yymore() yymore_used_but_not_detected" );
		indent_puts( "#define YY_MORE_ADJ 0" );
		indent_puts( "#define YY_RESTORE_YY_MORE_OFFSET" );
		}

	if ( ! C_plus_plus )
		{
		if ( yytext_is_array )
			{
			outn( "#ifndef YYLMAX" );
			outn( "#define YYLMAX 8192" );
			outn( "#endif\n" );
			outn( "char yytext[YYLMAX];" );
			outn( "char *yytext_ptr;" );
			}

		else
			outn( "char *yytext;" );
		}

	out( &action_array[defs1_offset] );

	line_directive_out( stdout, 0 );

	skelout();

	if ( ! C_plus_plus )
		{
		if ( use_read )
			{
			outn(
"\tif ( (result = read( fileno(yyin), (char *) buf, max_size )) < 0 ) \\" );
			outn(
		"\t\tYY_FATAL_ERROR( \"input in flex scanner failed\" );" );
			}

		else
			{
			outn(
			"\tif ( yy_current_buffer->yy_is_interactive ) \\" );
			outn( "\t\t{ \\" );
			outn( "\t\tint c = '*', n; \\" );
			outn( "\t\tfor ( n = 0; n < max_size && \\" );
	outn( "\t\t\t     (c = getc( yyin )) != EOF && c != '\\n'; ++n ) \\" );
			outn( "\t\t\tbuf[n] = (char) c; \\" );
			outn( "\t\tif ( c == '\\n' ) \\" );
			outn( "\t\t\tbuf[n++] = (char) c; \\" );
			outn( "\t\tif ( c == EOF && ferror( yyin ) ) \\" );
			outn(
	"\t\t\tYY_FATAL_ERROR( \"input in flex scanner failed\" ); \\" );
			outn( "\t\tresult = n; \\" );
			outn( "\t\t} \\" );
			outn(
	"\telse if ( ((result = fread( buf, 1, max_size, yyin )) == 0) \\" );
			outn( "\t\t  && ferror( yyin ) ) \\" );
			outn(
		"\t\tYY_FATAL_ERROR( \"input in flex scanner failed\" );" );
			}
		}

	skelout();

	indent_puts( "#define YY_RULE_SETUP \\" );
	indent_up();
	if ( bol_needed )
		{
		indent_puts( "if ( yyleng > 0 ) \\" );
		indent_up();
		indent_puts( "yy_current_buffer->yy_at_bol = \\" );
		indent_puts( "\t\t(yytext[yyleng - 1] == '\\n'); \\" );
		indent_down();
		}
	indent_puts( "YY_USER_ACTION" );
	indent_down();

	skelout();

	/* Copy prolog to output file. */
	out( &action_array[prolog_offset] );

	line_directive_out( stdout, 0 );

	skelout();

	set_indent( 2 );

	if ( yymore_used && ! yytext_is_array )
		{
		indent_puts( "yy_more_len = 0;" );
		indent_puts( "if ( yy_more_flag )" );
		indent_up();
		indent_puts( "{" );
		indent_puts( "yy_more_len = yy_c_buf_p - yytext_ptr;" );
		indent_puts( "yy_more_flag = 0;" );
		indent_puts( "}" );
		indent_down();
		}

	skelout();

	gen_start_state();

	/* Note, don't use any indentation. */
	outn( "yy_match:" );
	gen_next_match();

	skelout();
	set_indent( 2 );
	gen_find_action();

	skelout();
	if ( do_yylineno )
		{
		indent_puts( "if ( yy_act != YY_END_OF_BUFFER )" );
		indent_up();
		indent_puts( "{" );
		indent_puts( "int yyl;" );
		indent_puts( "for ( yyl = 0; yyl < yyleng; ++yyl )" );
		indent_up();
		indent_puts( "if ( yytext[yyl] == '\\n' )" );
		indent_up();
		indent_puts( "++yylineno;" );
		indent_down();
		indent_down();
		indent_puts( "}" );
		indent_down();
		}

	skelout();
	if ( ddebug )
		{
		indent_puts( "if ( yy_flex_debug )" );
		indent_up();

		indent_puts( "{" );
		indent_puts( "if ( yy_act == 0 )" );
		indent_up();
		indent_puts( C_plus_plus ?
			"cerr << \"--scanner backing up\\n\";" :
			"fprintf( stderr, \"--scanner backing up\\n\" );" );
		indent_down();

		do_indent();
		out_dec( "else if ( yy_act < %d )\n", num_rules );
		indent_up();

		if ( C_plus_plus )
			{
			indent_puts(
	"cerr << \"--accepting rule at line \" << yy_rule_linenum[yy_act] <<" );
			indent_puts(
			"         \"(\\\"\" << yytext << \"\\\")\\n\";" );
			}
		else
			{
			indent_puts(
	"fprintf( stderr, \"--accepting rule at line %d (\\\"%s\\\")\\n\"," );

			indent_puts(
				"         yy_rule_linenum[yy_act], yytext );" );
			}

		indent_down();

		do_indent();
		out_dec( "else if ( yy_act == %d )\n", num_rules );
		indent_up();

		if ( C_plus_plus )
			{
			indent_puts(
"cerr << \"--accepting default rule (\\\"\" << yytext << \"\\\")\\n\";" );
			}
		else
			{
			indent_puts(
	"fprintf( stderr, \"--accepting default rule (\\\"%s\\\")\\n\"," );
			indent_puts( "         yytext );" );
			}

		indent_down();

		do_indent();
		out_dec( "else if ( yy_act == %d )\n", num_rules + 1 );
		indent_up();

		indent_puts( C_plus_plus ?
			"cerr << \"--(end of buffer or a NUL)\\n\";" :
		"fprintf( stderr, \"--(end of buffer or a NUL)\\n\" );" );

		indent_down();

		do_indent();
		outn( "else" );
		indent_up();

		if ( C_plus_plus )
			{
			indent_puts(
	"cerr << \"--EOF (start condition \" << YY_START << \")\\n\";" );
			}
		else
			{
			indent_puts(
	"fprintf( stderr, \"--EOF (start condition %d)\\n\", YY_START );" );
			}

		indent_down();

		indent_puts( "}" );
		indent_down();
		}

	/* Copy actions to output file. */
	skelout();
	indent_up();
	gen_bu_action();
	out( &action_array[action_offset] );

	line_directive_out( stdout, 0 );

	/* generate cases for any missing EOF rules */
	for ( i = 1; i <= lastsc; ++i )
		if ( ! sceof[i] )
			{
			do_indent();
			out_str( "case YY_STATE_EOF(%s):\n", scname[i] );
			did_eof_rule = true;
			}

	if ( did_eof_rule )
		{
		indent_up();
		indent_puts( "yyterminate();" );
		indent_down();
		}


	/* Generate code for handling NUL's, if needed. */

	/* First, deal with backing up and setting up yy_cp if the scanner
	 * finds that it should JAM on the NUL.
	 */
	skelout();
	set_indent( 4 );

	if ( fullspd || fulltbl )
		indent_puts( "yy_cp = yy_c_buf_p;" );

	else
		{ /* compressed table */
		if ( ! reject && ! interactive )
			{
			/* Do the guaranteed-needed backing up to figure
			 * out the match.
			 */
			indent_puts( "yy_cp = yy_last_accepting_cpos;" );
			indent_puts(
				"yy_current_state = yy_last_accepting_state;" );
			}

		else
			/* Still need to initialize yy_cp, though
			 * yy_current_state was set up by
			 * yy_get_previous_state().
			 */
			indent_puts( "yy_cp = yy_c_buf_p;" );
		}


	/* Generate code for yy_get_previous_state(). */
	set_indent( 1 );
	skelout();

	gen_start_state();

	set_indent( 2 );
	skelout();
	gen_next_state( true );

	set_indent( 1 );
	skelout();
	gen_NUL_trans();

	skelout();
	if ( do_yylineno )
		{ /* update yylineno inside of unput() */
		indent_puts( "if ( c == '\\n' )" );
		indent_up();
		indent_puts( "--yylineno;" );
		indent_down();
		}

	skelout();
	/* Update BOL and yylineno inside of input(). */
	if ( bol_needed )
		{
		indent_puts( "yy_current_buffer->yy_at_bol = (c == '\\n');" );
		if ( do_yylineno )
			{
			indent_puts( "if ( yy_current_buffer->yy_at_bol )" );
			indent_up();
			indent_puts( "++yylineno;" );
			indent_down();
			}
		}

	else if ( do_yylineno )
		{
		indent_puts( "if ( c == '\\n' )" );
		indent_up();
		indent_puts( "++yylineno;" );
		indent_down();
		}

	skelout();

	/* Copy remainder of input to output. */

	line_directive_out( stdout, 1 );

	if ( sectnum == 3 )
		(void) flexscan(); /* copy remainder of input to output */
	}
