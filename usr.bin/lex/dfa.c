/* dfa - DFA construction routines */

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
    "@(#) $Header: /a/cvs/386BSD/src/usr.bin/lex/dfa.c,v 1.2 1993/06/29 03:27:06 nate Exp $ (LBL)";
#endif

#include "flexdef.h"


/* declare functions that have forward references */

void dump_associated_rules PROTO((FILE*, int));
void dump_transitions PROTO((FILE*, int[]));
void sympartition PROTO((int[], int, int[], int[]));
int symfollowset PROTO((int[], int, int, int[]));


/* check_for_backtracking - check a DFA state for backtracking
 *
 * synopsis
 *     int ds, state[numecs];
 *     check_for_backtracking( ds, state );
 *
 * ds is the number of the state to check and state[] is its out-transitions,
 * indexed by equivalence class, and state_rules[] is the set of rules
 * associated with this state
 */

void check_for_backtracking( ds, state )
int ds;
int state[];

    {
    if ( (reject && ! dfaacc[ds].dfaacc_set) || ! dfaacc[ds].dfaacc_state )
	{ /* state is non-accepting */
	++num_backtracking;

	if ( backtrack_report )
	    {
	    fprintf( backtrack_file, "State #%d is non-accepting -\n", ds );

	    /* identify the state */
	    dump_associated_rules( backtrack_file, ds );

	    /* now identify it further using the out- and jam-transitions */
	    dump_transitions( backtrack_file, state );

	    putc( '\n', backtrack_file );
	    }
	}
    }


/* check_trailing_context - check to see if NFA state set constitutes
 *                          "dangerous" trailing context
 *
 * synopsis
 *    int nfa_states[num_states+1], num_states;
 *    int accset[nacc+1], nacc;
 *    check_trailing_context( nfa_states, num_states, accset, nacc );
 *
 * NOTES
 *    Trailing context is "dangerous" if both the head and the trailing
 *  part are of variable size \and/ there's a DFA state which contains
 *  both an accepting state for the head part of the rule and NFA states
 *  which occur after the beginning of the trailing context.
 *  When such a rule is matched, it's impossible to tell if having been
 *  in the DFA state indicates the beginning of the trailing context
 *  or further-along scanning of the pattern.  In these cases, a warning
 *  message is issued.
 *
 *    nfa_states[1 .. num_states] is the list of NFA states in the DFA.
 *    accset[1 .. nacc] is the list of accepting numbers for the DFA state.
 */

void check_trailing_context( nfa_states, num_states, accset, nacc )
int *nfa_states, num_states;
int *accset;
register int nacc;

    {
    register int i, j;

    for ( i = 1; i <= num_states; ++i )
	{
	int ns = nfa_states[i];
	register int type = state_type[ns];
	register int ar = assoc_rule[ns];

	if ( type == STATE_NORMAL || rule_type[ar] != RULE_VARIABLE )
	    { /* do nothing */
	    }

	else if ( type == STATE_TRAILING_CONTEXT )
	    {
	    /* potential trouble.  Scan set of accepting numbers for
	     * the one marking the end of the "head".  We assume that
	     * this looping will be fairly cheap since it's rare that
	     * an accepting number set is large.
	     */
	    for ( j = 1; j <= nacc; ++j )
		if ( accset[j] & YY_TRAILING_HEAD_MASK )
		    {
		    fprintf( stderr,
		     "%s: Dangerous trailing context in rule at line %d\n",
			     program_name, rule_linenum[ar] );
		    return;
		    }
	    }
	}
    }


/* dump_associated_rules - list the rules associated with a DFA state
 *
 * synopisis
 *     int ds;
 *     FILE *file;
 *     dump_associated_rules( file, ds );
 *
 * goes through the set of NFA states associated with the DFA and
 * extracts the first MAX_ASSOC_RULES unique rules, sorts them,
 * and writes a report to the given file
 */

void dump_associated_rules( file, ds )
FILE *file;
int ds;

    {
    register int i, j;
    register int num_associated_rules = 0;
    int rule_set[MAX_ASSOC_RULES + 1];
    int *dset = dss[ds];
    int size = dfasiz[ds];
    
    for ( i = 1; i <= size; ++i )
	{
	register rule_num = rule_linenum[assoc_rule[dset[i]]];

	for ( j = 1; j <= num_associated_rules; ++j )
	    if ( rule_num == rule_set[j] )
		break;

	if ( j > num_associated_rules )
	    { /* new rule */
	    if ( num_associated_rules < MAX_ASSOC_RULES )
		rule_set[++num_associated_rules] = rule_num;
	    }
	}

    bubble( rule_set, num_associated_rules );

    fprintf( file, " associated rule line numbers:" );

    for ( i = 1; i <= num_associated_rules; ++i )
	{
	if ( i % 8 == 1 )
	    putc( '\n', file );
	
	fprintf( file, "\t%d", rule_set[i] );
	}
    
    putc( '\n', file );
    }


/* dump_transitions - list the transitions associated with a DFA state
 *
 * synopisis
 *     int state[numecs];
 *     FILE *file;
 *     dump_transitions( file, state );
 *
 * goes through the set of out-transitions and lists them in human-readable
 * form (i.e., not as equivalence classes); also lists jam transitions
 * (i.e., all those which are not out-transitions, plus EOF).  The dump
 * is done to the given file.
 */

void dump_transitions( file, state )
FILE *file;
int state[];

    {
    register int i, ec;
    int out_char_set[CSIZE];

    for ( i = 0; i < csize; ++i )
	{
	ec = abs( ecgroup[i] );
	out_char_set[i] = state[ec];
	}
    
    fprintf( file, " out-transitions: " );

    list_character_set( file, out_char_set );

    /* now invert the members of the set to get the jam transitions */
    for ( i = 0; i < csize; ++i )
	out_char_set[i] = ! out_char_set[i];

    fprintf( file, "\n jam-transitions: EOF " );

    list_character_set( file, out_char_set );

    putc( '\n', file );
    }


/* epsclosure - construct the epsilon closure of a set of ndfa states
 *
 * synopsis
 *    int t[current_max_dfa_size], numstates, accset[num_rules + 1], nacc;
 *    int hashval;
 *    int *epsclosure();
 *    t = epsclosure( t, &numstates, accset, &nacc, &hashval );
 *
 * NOTES
 *    the epsilon closure is the set of all states reachable by an arbitrary
 *  number of epsilon transitions which themselves do not have epsilon
 *  transitions going out, unioned with the set of states which have non-null
 *  accepting numbers.  t is an array of size numstates of nfa state numbers.
 *  Upon return, t holds the epsilon closure and numstates is updated.  accset
 *  holds a list of the accepting numbers, and the size of accset is given
 *  by nacc.  t may be subjected to reallocation if it is not large enough
 *  to hold the epsilon closure.
 *
 *    hashval is the hash value for the dfa corresponding to the state set
 */

int *epsclosure( t, ns_addr, accset, nacc_addr, hv_addr )
int *t, *ns_addr, accset[], *nacc_addr, *hv_addr;

    {
    register int stkpos, ns, tsp;
    int numstates = *ns_addr, nacc, hashval, transsym, nfaccnum;
    int stkend, nstate;
    static int did_stk_init = false, *stk; 

#define MARK_STATE(state) \
	trans1[state] = trans1[state] - MARKER_DIFFERENCE;

#define IS_MARKED(state) (trans1[state] < 0)

#define UNMARK_STATE(state) \
	trans1[state] = trans1[state] + MARKER_DIFFERENCE;

#define CHECK_ACCEPT(state) \
	{ \
	nfaccnum = accptnum[state]; \
	if ( nfaccnum != NIL ) \
	    accset[++nacc] = nfaccnum; \
	}

#define DO_REALLOCATION \
	{ \
	current_max_dfa_size += MAX_DFA_SIZE_INCREMENT; \
	++num_reallocs; \
	t = reallocate_integer_array( t, current_max_dfa_size ); \
	stk = reallocate_integer_array( stk, current_max_dfa_size ); \
	} \

#define PUT_ON_STACK(state) \
	{ \
	if ( ++stkend >= current_max_dfa_size ) \
	    DO_REALLOCATION \
	stk[stkend] = state; \
	MARK_STATE(state) \
	}

#define ADD_STATE(state) \
	{ \
	if ( ++numstates >= current_max_dfa_size ) \
	    DO_REALLOCATION \
	t[numstates] = state; \
	hashval = hashval + state; \
	}

#define STACK_STATE(state) \
	{ \
	PUT_ON_STACK(state) \
	CHECK_ACCEPT(state) \
	if ( nfaccnum != NIL || transchar[state] != SYM_EPSILON ) \
	    ADD_STATE(state) \
	}

    if ( ! did_stk_init )
	{
	stk = allocate_integer_array( current_max_dfa_size );
	did_stk_init = true;
	}

    nacc = stkend = hashval = 0;

    for ( nstate = 1; nstate <= numstates; ++nstate )
	{
	ns = t[nstate];

	/* the state could be marked if we've already pushed it onto
	 * the stack
	 */
	if ( ! IS_MARKED(ns) )
	    PUT_ON_STACK(ns)

	CHECK_ACCEPT(ns)
	hashval = hashval + ns;
	}

    for ( stkpos = 1; stkpos <= stkend; ++stkpos )
	{
	ns = stk[stkpos];
	transsym = transchar[ns];

	if ( transsym == SYM_EPSILON )
	    {
	    tsp = trans1[ns] + MARKER_DIFFERENCE;

	    if ( tsp != NO_TRANSITION )
		{
		if ( ! IS_MARKED(tsp) )
		    STACK_STATE(tsp)

		tsp = trans2[ns];

		if ( tsp != NO_TRANSITION )
		    if ( ! IS_MARKED(tsp) )
			STACK_STATE(tsp)
		}
	    }
	}

    /* clear out "visit" markers */

    for ( stkpos = 1; stkpos <= stkend; ++stkpos )
	{
	if ( IS_MARKED(stk[stkpos]) )
	    {
	    UNMARK_STATE(stk[stkpos])
	    }
	else
	    flexfatal( "consistency check failed in epsclosure()" );
	}

    *ns_addr = numstates;
    *hv_addr = hashval;
    *nacc_addr = nacc;

    return ( t );
    }


/* increase_max_dfas - increase the maximum number of DFAs */

void increase_max_dfas()

    {
    current_max_dfas += MAX_DFAS_INCREMENT;

    ++num_reallocs;

    base = reallocate_integer_array( base, current_max_dfas );
    def = reallocate_integer_array( def, current_max_dfas );
    dfasiz = reallocate_integer_array( dfasiz, current_max_dfas );
    accsiz = reallocate_integer_array( accsiz, current_max_dfas );
    dhash = reallocate_integer_array( dhash, current_max_dfas );
    dss = reallocate_int_ptr_array( dss, current_max_dfas );
    dfaacc = reallocate_dfaacc_union( dfaacc, current_max_dfas );

    if ( nultrans )
	nultrans = reallocate_integer_array( nultrans, current_max_dfas );
    }


/* ntod - convert an ndfa to a dfa
 *
 * synopsis
 *    ntod();
 *
 *  creates the dfa corresponding to the ndfa we've constructed.  the
 *  dfa starts out in state #1.
 */

void ntod()

    {
    int *accset, ds, nacc, newds;
    int sym, hashval, numstates, dsize;
    int num_full_table_rows;	/* used only for -f */
    int *nset, *dset;
    int targptr, totaltrans, i, comstate, comfreq, targ;
    int *epsclosure(), snstods(), symlist[CSIZE + 1];
    int num_start_states;
    int todo_head, todo_next;

    /* note that the following are indexed by *equivalence classes*
     * and not by characters.  Since equivalence classes are indexed
     * beginning with 1, even if the scanner accepts NUL's, this
     * means that (since every character is potentially in its own
     * equivalence class) these arrays must have room for indices
     * from 1 to CSIZE, so their size must be CSIZE + 1.
     */
    int duplist[CSIZE + 1], state[CSIZE + 1];
    int targfreq[CSIZE + 1], targstate[CSIZE + 1];

    /* this is so find_table_space(...) will know where to start looking in
     * chk/nxt for unused records for space to put in the state
     */
    if ( fullspd )
	firstfree = 0;

    accset = allocate_integer_array( num_rules + 1 );
    nset = allocate_integer_array( current_max_dfa_size );

    /* the "todo" queue is represented by the head, which is the DFA
     * state currently being processed, and the "next", which is the
     * next DFA state number available (not in use).  We depend on the
     * fact that snstods() returns DFA's \in increasing order/, and thus
     * need only know the bounds of the dfas to be processed.
     */
    todo_head = todo_next = 0;

    for ( i = 0; i <= csize; ++i )
	{
	duplist[i] = NIL;
	symlist[i] = false;
	}

    for ( i = 0; i <= num_rules; ++i )
	accset[i] = NIL;

    if ( trace )
	{
	dumpnfa( scset[1] );
	fputs( "\n\nDFA Dump:\n\n", stderr );
	}

    inittbl();

    /* check to see whether we should build a separate table for transitions
     * on NUL characters.  We don't do this for full-speed (-F) scanners,
     * since for them we don't have a simple state number lying around with
     * which to index the table.  We also don't bother doing it for scanners
     * unless (1) NUL is in its own equivalence class (indicated by a
     * positive value of ecgroup[NUL]), (2) NUL's equilvalence class is
     * the last equivalence class, and (3) the number of equivalence classes
     * is the same as the number of characters.  This latter case comes about
     * when useecs is false or when its true but every character still
     * manages to land in its own class (unlikely, but it's cheap to check
     * for).  If all these things are true then the character code needed
     * to represent NUL's equivalence class for indexing the tables is
     * going to take one more bit than the number of characters, and therefore
     * we won't be assured of being able to fit it into a YY_CHAR variable.
     * This rules out storing the transitions in a compressed table, since
     * the code for interpreting them uses a YY_CHAR variable (perhaps it
     * should just use an integer, though; this is worth pondering ... ###).
     *
     * Finally, for full tables, we want the number of entries in the
     * table to be a power of two so the array references go fast (it
     * will just take a shift to compute the major index).  If encoding
     * NUL's transitions in the table will spoil this, we give it its
     * own table (note that this will be the case if we're not using
     * equivalence classes).
     */

    /* note that the test for ecgroup[0] == numecs below accomplishes
     * both (1) and (2) above
     */
    if ( ! fullspd && ecgroup[0] == numecs )
	{ /* NUL is alone in its equivalence class, which is the last one */
	int use_NUL_table = (numecs == csize);

	if ( fulltbl && ! use_NUL_table )
	    { /* we still may want to use the table if numecs is a power of 2 */
	    int power_of_two;

	    for ( power_of_two = 1; power_of_two <= csize; power_of_two *= 2 )
		if ( numecs == power_of_two )
		    {
		    use_NUL_table = true;
		    break;
		    }
	    }

	if ( use_NUL_table )
	    nultrans = allocate_integer_array( current_max_dfas );
	    /* from now on, nultrans != nil indicates that we're
	     * saving null transitions for later, separate encoding
	     */
	}


    if ( fullspd )
	{
	for ( i = 0; i <= numecs; ++i )
	    state[i] = 0;
	place_state( state, 0, 0 );
	}

    else if ( fulltbl )
	{
	if ( nultrans )
	    /* we won't be including NUL's transitions in the table,
	     * so build it for entries from 0 .. numecs - 1
	     */
	    num_full_table_rows = numecs;

	else
	    /* take into account the fact that we'll be including
	     * the NUL entries in the transition table.  Build it
	     * from 0 .. numecs.
	     */
	    num_full_table_rows = numecs + 1;

	/* declare it "short" because it's a real long-shot that that
	 * won't be large enough.
	 */
	printf( "static short int yy_nxt[][%d] =\n    {\n",
		/* '}' so vi doesn't get too confused */
		num_full_table_rows );

	/* generate 0 entries for state #0 */
	for ( i = 0; i < num_full_table_rows; ++i )
	    mk2data( 0 );

	/* force ',' and dataflush() next call to mk2data */
	datapos = NUMDATAITEMS;

	/* force extra blank line next dataflush() */
	dataline = NUMDATALINES;
	}

    /* create the first states */

    num_start_states = lastsc * 2;

    for ( i = 1; i <= num_start_states; ++i )
	{
	numstates = 1;

	/* for each start condition, make one state for the case when
	 * we're at the beginning of the line (the '%' operator) and
	 * one for the case when we're not
	 */
	if ( i % 2 == 1 )
	    nset[numstates] = scset[(i / 2) + 1];
	else
	    nset[numstates] = mkbranch( scbol[i / 2], scset[i / 2] );

	nset = epsclosure( nset, &numstates, accset, &nacc, &hashval );

	if ( snstods( nset, numstates, accset, nacc, hashval, &ds ) )
	    {
	    numas += nacc;
	    totnst += numstates;
	    ++todo_next;

	    if ( variable_trailing_context_rules && nacc > 0 )
		check_trailing_context( nset, numstates, accset, nacc );
	    }
	}

    if ( ! fullspd )
	{
	if ( ! snstods( nset, 0, accset, 0, 0, &end_of_buffer_state ) )
	    flexfatal( "could not create unique end-of-buffer state" );

	++numas;
	++num_start_states;
	++todo_next;
	}

    while ( todo_head < todo_next )
	{
	targptr = 0;
	totaltrans = 0;

	for ( i = 1; i <= numecs; ++i )
	    state[i] = 0;

	ds = ++todo_head;

	dset = dss[ds];
	dsize = dfasiz[ds];

	if ( trace )
	    fprintf( stderr, "state # %d:\n", ds );

	sympartition( dset, dsize, symlist, duplist );

	for ( sym = 1; sym <= numecs; ++sym )
	    {
	    if ( symlist[sym] )
		{
		symlist[sym] = 0;

		if ( duplist[sym] == NIL )
		    { /* symbol has unique out-transitions */
		    numstates = symfollowset( dset, dsize, sym, nset );
		    nset = epsclosure( nset, &numstates, accset,
				       &nacc, &hashval );

		    if ( snstods( nset, numstates, accset,
				  nacc, hashval, &newds ) )
			{
			totnst = totnst + numstates;
			++todo_next;
			numas += nacc;

			if ( variable_trailing_context_rules && nacc > 0 )
			    check_trailing_context( nset, numstates,
				accset, nacc );
			}

		    state[sym] = newds;

		    if ( trace )
			fprintf( stderr, "\t%d\t%d\n", sym, newds );

		    targfreq[++targptr] = 1;
		    targstate[targptr] = newds;
		    ++numuniq;
		    }

		else
		    {
		    /* sym's equivalence class has the same transitions
		     * as duplist(sym)'s equivalence class
		     */
		    targ = state[duplist[sym]];
		    state[sym] = targ;

		    if ( trace )
			fprintf( stderr, "\t%d\t%d\n", sym, targ );

		    /* update frequency count for destination state */

		    i = 0;
		    while ( targstate[++i] != targ )
			;

		    ++targfreq[i];
		    ++numdup;
		    }

		++totaltrans;
		duplist[sym] = NIL;
		}
	    }

	numsnpairs = numsnpairs + totaltrans;

	if ( caseins && ! useecs )
	    {
	    register int j;

	    for ( i = 'A', j = 'a'; i <= 'Z'; ++i, ++j )
		state[i] = state[j];
	    }

	if ( ds > num_start_states )
	    check_for_backtracking( ds, state );

	if ( nultrans )
	    {
	    nultrans[ds] = state[NUL_ec];
	    state[NUL_ec] = 0;	/* remove transition */
	    }

	if ( fulltbl )
	    {
	    /* supply array's 0-element */
	    if ( ds == end_of_buffer_state )
		mk2data( -end_of_buffer_state );
	    else
		mk2data( end_of_buffer_state );

	    for ( i = 1; i < num_full_table_rows; ++i )
		/* jams are marked by negative of state number */
		mk2data( state[i] ? state[i] : -ds );

	    /* force ',' and dataflush() next call to mk2data */
	    datapos = NUMDATAITEMS;

	    /* force extra blank line next dataflush() */
	    dataline = NUMDATALINES;
	    }

        else if ( fullspd )
	    place_state( state, ds, totaltrans );

	else if ( ds == end_of_buffer_state )
	    /* special case this state to make sure it does what it's
	     * supposed to, i.e., jam on end-of-buffer
	     */
	    stack1( ds, 0, 0, JAMSTATE );

	else /* normal, compressed state */
	    {
	    /* determine which destination state is the most common, and
	     * how many transitions to it there are
	     */

	    comfreq = 0;
	    comstate = 0;

	    for ( i = 1; i <= targptr; ++i )
		if ( targfreq[i] > comfreq )
		    {
		    comfreq = targfreq[i];
		    comstate = targstate[i];
		    }

	    bldtbl( state, ds, totaltrans, comstate, comfreq );
	    }
	}

    if ( fulltbl )
	dataend();

    else if ( ! fullspd )
	{
	cmptmps();  /* create compressed template entries */

	/* create tables for all the states with only one out-transition */
	while ( onesp > 0 )
	    {
	    mk1tbl( onestate[onesp], onesym[onesp], onenext[onesp],
		    onedef[onesp] );
	    --onesp;
	    }

	mkdeftbl();
	}
    }


/* snstods - converts a set of ndfa states into a dfa state
 *
 * synopsis
 *    int sns[numstates], numstates, newds, accset[num_rules + 1], nacc, hashval;
 *    int snstods();
 *    is_new_state = snstods( sns, numstates, accset, nacc, hashval, &newds );
 *
 * on return, the dfa state number is in newds.
 */

int snstods( sns, numstates, accset, nacc, hashval, newds_addr )
int sns[], numstates, accset[], nacc, hashval, *newds_addr;

    {
    int didsort = 0;
    register int i, j;
    int newds, *oldsns;

    for ( i = 1; i <= lastdfa; ++i )
	if ( hashval == dhash[i] )
	    {
	    if ( numstates == dfasiz[i] )
		{
		oldsns = dss[i];

		if ( ! didsort )
		    {
		    /* we sort the states in sns so we can compare it to
		     * oldsns quickly.  we use bubble because there probably
		     * aren't very many states
		     */
		    bubble( sns, numstates );
		    didsort = 1;
		    }

		for ( j = 1; j <= numstates; ++j )
		    if ( sns[j] != oldsns[j] )
			break;

		if ( j > numstates )
		    {
		    ++dfaeql;
		    *newds_addr = i;
		    return ( 0 );
		    }

		++hshcol;
		}

	    else
		++hshsave;
	    }

    /* make a new dfa */

    if ( ++lastdfa >= current_max_dfas )
	increase_max_dfas();

    newds = lastdfa;

    dss[newds] = (int *) malloc( (unsigned) ((numstates + 1) * sizeof( int )) );

    if ( ! dss[newds] )
	flexfatal( "dynamic memory failure in snstods()" );

    /* if we haven't already sorted the states in sns, we do so now, so that
     * future comparisons with it can be made quickly
     */

    if ( ! didsort )
	bubble( sns, numstates );

    for ( i = 1; i <= numstates; ++i )
	dss[newds][i] = sns[i];

    dfasiz[newds] = numstates;
    dhash[newds] = hashval;

    if ( nacc == 0 )
	{
	if ( reject )
	    dfaacc[newds].dfaacc_set = (int *) 0;
	else
	    dfaacc[newds].dfaacc_state = 0;

	accsiz[newds] = 0;
	}

    else if ( reject )
	{
	/* we sort the accepting set in increasing order so the disambiguating
	 * rule that the first rule listed is considered match in the event of
	 * ties will work.  We use a bubble sort since the list is probably
	 * quite small.
	 */

	bubble( accset, nacc );

	dfaacc[newds].dfaacc_set =
	    (int *) malloc( (unsigned) ((nacc + 1) * sizeof( int )) );

	if ( ! dfaacc[newds].dfaacc_set )
	    flexfatal( "dynamic memory failure in snstods()" );

	/* save the accepting set for later */
	for ( i = 1; i <= nacc; ++i )
	    dfaacc[newds].dfaacc_set[i] = accset[i];

	accsiz[newds] = nacc;
	}

    else
	{ /* find lowest numbered rule so the disambiguating rule will work */
	j = num_rules + 1;

	for ( i = 1; i <= nacc; ++i )
	    if ( accset[i] < j )
		j = accset[i];

	dfaacc[newds].dfaacc_state = j;
	}

    *newds_addr = newds;

    return ( 1 );
    }


/* symfollowset - follow the symbol transitions one step
 *
 * synopsis
 *    int ds[current_max_dfa_size], dsize, transsym;
 *    int nset[current_max_dfa_size], numstates;
 *    numstates = symfollowset( ds, dsize, transsym, nset );
 */

int symfollowset( ds, dsize, transsym, nset )
int ds[], dsize, transsym, nset[];

    {
    int ns, tsp, sym, i, j, lenccl, ch, numstates;
    int ccllist;

    numstates = 0;

    for ( i = 1; i <= dsize; ++i )
	{ /* for each nfa state ns in the state set of ds */
	ns = ds[i];
	sym = transchar[ns];
	tsp = trans1[ns];

	if ( sym < 0 )
	    { /* it's a character class */
	    sym = -sym;
	    ccllist = cclmap[sym];
	    lenccl = ccllen[sym];

	    if ( cclng[sym] )
		{
		for ( j = 0; j < lenccl; ++j )
		    { /* loop through negated character class */
		    ch = ccltbl[ccllist + j];

		    if ( ch == 0 )
			ch = NUL_ec;

		    if ( ch > transsym )
			break;	/* transsym isn't in negated ccl */

		    else if ( ch == transsym )
			/* next 2 */ goto bottom;
		    }

		/* didn't find transsym in ccl */
		nset[++numstates] = tsp;
		}

	    else
		for ( j = 0; j < lenccl; ++j )
		    {
		    ch = ccltbl[ccllist + j];

		    if ( ch == 0 )
			ch = NUL_ec;

		    if ( ch > transsym )
			break;

		    else if ( ch == transsym )
			{
			nset[++numstates] = tsp;
			break;
			}
		    }
	    }

	else if ( sym >= 'A' && sym <= 'Z' && caseins )
	    flexfatal( "consistency check failed in symfollowset" );

	else if ( sym == SYM_EPSILON )
	    { /* do nothing */
	    }

	else if ( abs( ecgroup[sym] ) == transsym )
	    nset[++numstates] = tsp;

bottom:
	;
	}

    return ( numstates );
    }


/* sympartition - partition characters with same out-transitions
 *
 * synopsis
 *    integer ds[current_max_dfa_size], numstates, duplist[numecs];
 *    symlist[numecs];
 *    sympartition( ds, numstates, symlist, duplist );
 */

void sympartition( ds, numstates, symlist, duplist )
int ds[], numstates, duplist[];
int symlist[];

    {
    int tch, i, j, k, ns, dupfwd[CSIZE + 1], lenccl, cclp, ich;

    /* partitioning is done by creating equivalence classes for those
     * characters which have out-transitions from the given state.  Thus
     * we are really creating equivalence classes of equivalence classes.
     */

    for ( i = 1; i <= numecs; ++i )
	{ /* initialize equivalence class list */
	duplist[i] = i - 1;
	dupfwd[i] = i + 1;
	}

    duplist[1] = NIL;
    dupfwd[numecs] = NIL;

    for ( i = 1; i <= numstates; ++i )
	{
	ns = ds[i];
	tch = transchar[ns];

	if ( tch != SYM_EPSILON )
	    {
	    if ( tch < -lastccl || tch >= csize )
		{
		if ( tch >= csize && tch <= CSIZE )
		    flexerror( "scanner requires -8 flag" );

		else
		    flexfatal(
			"bad transition character detected in sympartition()" );
		}

	    if ( tch >= 0 )
		{ /* character transition */
		/* abs() needed for fake %t ec's */
		int ec = abs( ecgroup[tch] );

		mkechar( ec, dupfwd, duplist );
		symlist[ec] = 1;
		}

	    else
		{ /* character class */
		tch = -tch;

		lenccl = ccllen[tch];
		cclp = cclmap[tch];
		mkeccl( ccltbl + cclp, lenccl, dupfwd, duplist, numecs,
			NUL_ec );

		if ( cclng[tch] )
		    {
		    j = 0;

		    for ( k = 0; k < lenccl; ++k )
			{
			ich = ccltbl[cclp + k];

			if ( ich == 0 )
			    ich = NUL_ec;

			for ( ++j; j < ich; ++j )
			    symlist[j] = 1;
			}

		    for ( ++j; j <= numecs; ++j )
			symlist[j] = 1;
		    }

		else
		    for ( k = 0; k < lenccl; ++k )
			{
			ich = ccltbl[cclp + k];

			if ( ich == 0 )
			    ich = NUL_ec;

			symlist[ich] = 1;
			}
		}
	    }
	}
    }
