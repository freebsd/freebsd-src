/* tblcmp - table compression routines */

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
    "@(#) $Header: /a/cvs/386BSD/src/usr.bin/lex/tblcmp.c,v 1.2 1993/06/29 03:27:20 nate Exp $ (LBL)";
#endif

#include "flexdef.h"


/* declarations for functions that have forward references */

void mkentry PROTO((register int*, int, int, int, int));
void mkprot PROTO((int[], int, int));
void mktemplate PROTO((int[], int, int));
void mv2front PROTO((int));
int tbldiff PROTO((int[], int, int[]));


/* bldtbl - build table entries for dfa state
 *
 * synopsis
 *   int state[numecs], statenum, totaltrans, comstate, comfreq;
 *   bldtbl( state, statenum, totaltrans, comstate, comfreq );
 *
 * State is the statenum'th dfa state.  It is indexed by equivalence class and
 * gives the number of the state to enter for a given equivalence class.
 * totaltrans is the total number of transitions out of the state.  Comstate
 * is that state which is the destination of the most transitions out of State.
 * Comfreq is how many transitions there are out of State to Comstate.
 *
 * A note on terminology:
 *    "protos" are transition tables which have a high probability of
 * either being redundant (a state processed later will have an identical
 * transition table) or nearly redundant (a state processed later will have
 * many of the same out-transitions).  A "most recently used" queue of
 * protos is kept around with the hope that most states will find a proto
 * which is similar enough to be usable, and therefore compacting the
 * output tables.
 *    "templates" are a special type of proto.  If a transition table is
 * homogeneous or nearly homogeneous (all transitions go to the same
 * destination) then the odds are good that future states will also go
 * to the same destination state on basically the same character set.
 * These homogeneous states are so common when dealing with large rule
 * sets that they merit special attention.  If the transition table were
 * simply made into a proto, then (typically) each subsequent, similar
 * state will differ from the proto for two out-transitions.  One of these
 * out-transitions will be that character on which the proto does not go
 * to the common destination, and one will be that character on which the
 * state does not go to the common destination.  Templates, on the other
 * hand, go to the common state on EVERY transition character, and therefore
 * cost only one difference.
 */

void bldtbl( state, statenum, totaltrans, comstate, comfreq )
int state[], statenum, totaltrans, comstate, comfreq;

    {
    int extptr, extrct[2][CSIZE + 1];
    int mindiff, minprot, i, d;
    int checkcom;

    /* If extptr is 0 then the first array of extrct holds the result of the
     * "best difference" to date, which is those transitions which occur in
     * "state" but not in the proto which, to date, has the fewest differences
     * between itself and "state".  If extptr is 1 then the second array of
     * extrct hold the best difference.  The two arrays are toggled
     * between so that the best difference to date can be kept around and
     * also a difference just created by checking against a candidate "best"
     * proto.
     */

    extptr = 0;

    /* if the state has too few out-transitions, don't bother trying to
     * compact its tables
     */

    if ( (totaltrans * 100) < (numecs * PROTO_SIZE_PERCENTAGE) )
	mkentry( state, numecs, statenum, JAMSTATE, totaltrans );

    else
	{
	/* checkcom is true if we should only check "state" against
	 * protos which have the same "comstate" value
	 */

	checkcom = comfreq * 100 > totaltrans * CHECK_COM_PERCENTAGE;

	minprot = firstprot;
	mindiff = totaltrans;

	if ( checkcom )
	    {
	    /* find first proto which has the same "comstate" */
	    for ( i = firstprot; i != NIL; i = protnext[i] )
		if ( protcomst[i] == comstate )
		    {
		    minprot = i;
		    mindiff = tbldiff( state, minprot, extrct[extptr] );
		    break;
		    }
	    }

	else
	    {
	    /* since we've decided that the most common destination out
	     * of "state" does not occur with a high enough frequency,
	     * we set the "comstate" to zero, assuring that if this state
	     * is entered into the proto list, it will not be considered
	     * a template.
	     */
	    comstate = 0;

	    if ( firstprot != NIL )
		{
		minprot = firstprot;
		mindiff = tbldiff( state, minprot, extrct[extptr] );
		}
	    }

	/* we now have the first interesting proto in "minprot".  If
	 * it matches within the tolerances set for the first proto,
	 * we don't want to bother scanning the rest of the proto list
	 * to see if we have any other reasonable matches.
	 */

	if ( mindiff * 100 > totaltrans * FIRST_MATCH_DIFF_PERCENTAGE )
	    { /* not a good enough match.  Scan the rest of the protos */
	    for ( i = minprot; i != NIL; i = protnext[i] )
		{
		d = tbldiff( state, i, extrct[1 - extptr] );
		if ( d < mindiff )
		    {
		    extptr = 1 - extptr;
		    mindiff = d;
		    minprot = i;
		    }
		}
	    }

	/* check if the proto we've decided on as our best bet is close
	 * enough to the state we want to match to be usable
	 */

	if ( mindiff * 100 > totaltrans * ACCEPTABLE_DIFF_PERCENTAGE )
	    {
	    /* no good.  If the state is homogeneous enough, we make a
	     * template out of it.  Otherwise, we make a proto.
	     */

	    if ( comfreq * 100 >= totaltrans * TEMPLATE_SAME_PERCENTAGE )
		mktemplate( state, statenum, comstate );

	    else
		{
		mkprot( state, statenum, comstate );
		mkentry( state, numecs, statenum, JAMSTATE, totaltrans );
		}
	    }

	else
	    { /* use the proto */
	    mkentry( extrct[extptr], numecs, statenum,
		     prottbl[minprot], mindiff );

	    /* if this state was sufficiently different from the proto
	     * we built it from, make it, too, a proto
	     */

	    if ( mindiff * 100 >= totaltrans * NEW_PROTO_DIFF_PERCENTAGE )
		mkprot( state, statenum, comstate );

	    /* since mkprot added a new proto to the proto queue, it's possible
	     * that "minprot" is no longer on the proto queue (if it happened
	     * to have been the last entry, it would have been bumped off).
	     * If it's not there, then the new proto took its physical place
	     * (though logically the new proto is at the beginning of the
	     * queue), so in that case the following call will do nothing.
	     */

	    mv2front( minprot );
	    }
	}
    }


/* cmptmps - compress template table entries
 *
 * synopsis
 *    cmptmps();
 *
 *  template tables are compressed by using the 'template equivalence
 *  classes', which are collections of transition character equivalence
 *  classes which always appear together in templates - really meta-equivalence
 *  classes.  until this point, the tables for templates have been stored
 *  up at the top end of the nxt array; they will now be compressed and have
 *  table entries made for them.
 */

void cmptmps()

    {
    int tmpstorage[CSIZE + 1];
    register int *tmp = tmpstorage, i, j;
    int totaltrans, trans;

    peakpairs = numtemps * numecs + tblend;

    if ( usemecs )
	{
	/* create equivalence classes base on data gathered on template
	 * transitions
	 */

	nummecs = cre8ecs( tecfwd, tecbck, numecs );
	}
    
    else
	nummecs = numecs;

    if ( lastdfa + numtemps + 1 >= current_max_dfas )
	increase_max_dfas();

    /* loop through each template */

    for ( i = 1; i <= numtemps; ++i )
	{
	totaltrans = 0;	/* number of non-jam transitions out of this template */

	for ( j = 1; j <= numecs; ++j )
	    {
	    trans = tnxt[numecs * i + j];

	    if ( usemecs )
		{
		/* the absolute value of tecbck is the meta-equivalence class
		 * of a given equivalence class, as set up by cre8ecs
		 */
		if ( tecbck[j] > 0 )
		    {
		    tmp[tecbck[j]] = trans;

		    if ( trans > 0 )
			++totaltrans;
		    }
		}

	    else
		{
		tmp[j] = trans;

		if ( trans > 0 )
		    ++totaltrans;
		}
	    }

	/* it is assumed (in a rather subtle way) in the skeleton that
	 * if we're using meta-equivalence classes, the def[] entry for
	 * all templates is the jam template, i.e., templates never default
	 * to other non-jam table entries (e.g., another template)
	 */

	/* leave room for the jam-state after the last real state */
	mkentry( tmp, nummecs, lastdfa + i + 1, JAMSTATE, totaltrans );
	}
    }



/* expand_nxt_chk - expand the next check arrays */

void expand_nxt_chk()

    {
    register int old_max = current_max_xpairs;

    current_max_xpairs += MAX_XPAIRS_INCREMENT;

    ++num_reallocs;

    nxt = reallocate_integer_array( nxt, current_max_xpairs );
    chk = reallocate_integer_array( chk, current_max_xpairs );

    bzero( (char *) (chk + old_max),
	   MAX_XPAIRS_INCREMENT * sizeof( int ) / sizeof( char ) );
    }


/* find_table_space - finds a space in the table for a state to be placed
 *
 * synopsis
 *     int *state, numtrans, block_start;
 *     int find_table_space();
 *
 *     block_start = find_table_space( state, numtrans );
 *
 * State is the state to be added to the full speed transition table.
 * Numtrans is the number of out-transitions for the state.
 *
 * find_table_space() returns the position of the start of the first block (in
 * chk) able to accommodate the state
 *
 * In determining if a state will or will not fit, find_table_space() must take
 * into account the fact that an end-of-buffer state will be added at [0],
 * and an action number will be added in [-1].
 */

int find_table_space( state, numtrans )
int *state, numtrans;
    
    {
    /* firstfree is the position of the first possible occurrence of two
     * consecutive unused records in the chk and nxt arrays
     */
    register int i;
    register int *state_ptr, *chk_ptr;
    register int *ptr_to_last_entry_in_state;

    /* if there are too many out-transitions, put the state at the end of
     * nxt and chk
     */
    if ( numtrans > MAX_XTIONS_FULL_INTERIOR_FIT )
	{
	/* if table is empty, return the first available spot in chk/nxt,
	 * which should be 1
	 */
	if ( tblend < 2 )
	    return ( 1 );

	i = tblend - numecs;	/* start searching for table space near the
				 * end of chk/nxt arrays
				 */
	}

    else
	i = firstfree;		/* start searching for table space from the
				 * beginning (skipping only the elements
				 * which will definitely not hold the new
				 * state)
				 */

    while ( 1 )		/* loops until a space is found */
	{
	if ( i + numecs >= current_max_xpairs )
	    expand_nxt_chk();

	/* loops until space for end-of-buffer and action number are found */
	while ( 1 )
	    {
	    if ( chk[i - 1] == 0 )	/* check for action number space */
		{
		if ( chk[i] == 0 )	/* check for end-of-buffer space */
		    break;

		else
		    i += 2;	/* since i != 0, there is no use checking to
				 * see if (++i) - 1 == 0, because that's the
				 * same as i == 0, so we skip a space
				 */
		}

	    else
		++i;

	    if ( i + numecs >= current_max_xpairs )
		expand_nxt_chk();
	    }

	/* if we started search from the beginning, store the new firstfree for
	 * the next call of find_table_space()
	 */
	if ( numtrans <= MAX_XTIONS_FULL_INTERIOR_FIT )
	    firstfree = i + 1;

	/* check to see if all elements in chk (and therefore nxt) that are
	 * needed for the new state have not yet been taken
	 */

	state_ptr = &state[1];
	ptr_to_last_entry_in_state = &chk[i + numecs + 1];

	for ( chk_ptr = &chk[i + 1]; chk_ptr != ptr_to_last_entry_in_state;
	      ++chk_ptr )
	    if ( *(state_ptr++) != 0 && *chk_ptr != 0 )
		break;

	if ( chk_ptr == ptr_to_last_entry_in_state )
	    return ( i );

	else
	    ++i;
	}
    }


/* inittbl - initialize transition tables
 *
 * synopsis
 *   inittbl();
 *
 * Initializes "firstfree" to be one beyond the end of the table.  Initializes
 * all "chk" entries to be zero.  Note that templates are built in their
 * own tbase/tdef tables.  They are shifted down to be contiguous
 * with the non-template entries during table generation.
 */
void inittbl()

    {
    register int i;

    bzero( (char *) chk, current_max_xpairs * sizeof( int ) / sizeof( char ) );

    tblend = 0;
    firstfree = tblend + 1;
    numtemps = 0;

    if ( usemecs )
	{
	/* set up doubly-linked meta-equivalence classes
	 * these are sets of equivalence classes which all have identical
	 * transitions out of TEMPLATES
	 */

	tecbck[1] = NIL;

	for ( i = 2; i <= numecs; ++i )
	    {
	    tecbck[i] = i - 1;
	    tecfwd[i - 1] = i;
	    }

	tecfwd[numecs] = NIL;
	}
    }


/* mkdeftbl - make the default, "jam" table entries
 *
 * synopsis
 *   mkdeftbl();
 */

void mkdeftbl()

    {
    int i;

    jamstate = lastdfa + 1;

    ++tblend; /* room for transition on end-of-buffer character */

    if ( tblend + numecs >= current_max_xpairs )
	expand_nxt_chk();

    /* add in default end-of-buffer transition */
    nxt[tblend] = end_of_buffer_state;
    chk[tblend] = jamstate;

    for ( i = 1; i <= numecs; ++i )
	{
	nxt[tblend + i] = 0;
	chk[tblend + i] = jamstate;
	}

    jambase = tblend;

    base[jamstate] = jambase;
    def[jamstate] = 0;

    tblend += numecs;
    ++numtemps;
    }


/* mkentry - create base/def and nxt/chk entries for transition array
 *
 * synopsis
 *   int state[numchars + 1], numchars, statenum, deflink, totaltrans;
 *   mkentry( state, numchars, statenum, deflink, totaltrans );
 *
 * "state" is a transition array "numchars" characters in size, "statenum"
 * is the offset to be used into the base/def tables, and "deflink" is the
 * entry to put in the "def" table entry.  If "deflink" is equal to
 * "JAMSTATE", then no attempt will be made to fit zero entries of "state"
 * (i.e., jam entries) into the table.  It is assumed that by linking to
 * "JAMSTATE" they will be taken care of.  In any case, entries in "state"
 * marking transitions to "SAME_TRANS" are treated as though they will be
 * taken care of by whereever "deflink" points.  "totaltrans" is the total
 * number of transitions out of the state.  If it is below a certain threshold,
 * the tables are searched for an interior spot that will accommodate the
 * state array.
 */

void mkentry( state, numchars, statenum, deflink, totaltrans )
register int *state;
int numchars, statenum, deflink, totaltrans;

    {
    register int minec, maxec, i, baseaddr;
    int tblbase, tbllast;

    if ( totaltrans == 0 )
	{ /* there are no out-transitions */
	if ( deflink == JAMSTATE )
	    base[statenum] = JAMSTATE;
	else
	    base[statenum] = 0;

	def[statenum] = deflink;
	return;
	}

    for ( minec = 1; minec <= numchars; ++minec )
	{
	if ( state[minec] != SAME_TRANS )
	    if ( state[minec] != 0 || deflink != JAMSTATE )
		break;
	}

    if ( totaltrans == 1 )
	{
	/* there's only one out-transition.  Save it for later to fill
	 * in holes in the tables.
	 */
	stack1( statenum, minec, state[minec], deflink );
	return;
	}

    for ( maxec = numchars; maxec > 0; --maxec )
	{
	if ( state[maxec] != SAME_TRANS )
	    if ( state[maxec] != 0 || deflink != JAMSTATE )
		break;
	}

    /* Whether we try to fit the state table in the middle of the table
     * entries we have already generated, or if we just take the state
     * table at the end of the nxt/chk tables, we must make sure that we
     * have a valid base address (i.e., non-negative).  Note that not only are
     * negative base addresses dangerous at run-time (because indexing the
     * next array with one and a low-valued character might generate an
     * array-out-of-bounds error message), but at compile-time negative
     * base addresses denote TEMPLATES.
     */

    /* find the first transition of state that we need to worry about. */
    if ( totaltrans * 100 <= numchars * INTERIOR_FIT_PERCENTAGE )
	{ /* attempt to squeeze it into the middle of the tabls */
	baseaddr = firstfree;

	while ( baseaddr < minec )
	    {
	    /* using baseaddr would result in a negative base address below
	     * find the next free slot
	     */
	    for ( ++baseaddr; chk[baseaddr] != 0; ++baseaddr )
		;
	    }

	if ( baseaddr + maxec - minec + 1 >= current_max_xpairs )
	    expand_nxt_chk();

	for ( i = minec; i <= maxec; ++i )
	    if ( state[i] != SAME_TRANS )
		if ( state[i] != 0 || deflink != JAMSTATE )
		    if ( chk[baseaddr + i - minec] != 0 )
			{ /* baseaddr unsuitable - find another */
			for ( ++baseaddr;
			      baseaddr < current_max_xpairs &&
			      chk[baseaddr] != 0;
			      ++baseaddr )
			    ;

			if ( baseaddr + maxec - minec + 1 >=
			     current_max_xpairs )
			    expand_nxt_chk();

			/* reset the loop counter so we'll start all
			 * over again next time it's incremented
			 */

			i = minec - 1;
			}
	}

    else
	{
	/* ensure that the base address we eventually generate is
	 * non-negative
	 */
	baseaddr = max( tblend + 1, minec );
	}

    tblbase = baseaddr - minec;
    tbllast = tblbase + maxec;

    if ( tbllast + 1 >= current_max_xpairs )
	expand_nxt_chk();

    base[statenum] = tblbase;
    def[statenum] = deflink;

    for ( i = minec; i <= maxec; ++i )
	if ( state[i] != SAME_TRANS )
	    if ( state[i] != 0 || deflink != JAMSTATE )
		{
		nxt[tblbase + i] = state[i];
		chk[tblbase + i] = statenum;
		}

    if ( baseaddr == firstfree )
	/* find next free slot in tables */
	for ( ++firstfree; chk[firstfree] != 0; ++firstfree )
	    ;

    tblend = max( tblend, tbllast );
    }


/* mk1tbl - create table entries for a state (or state fragment) which
 *            has only one out-transition
 *
 * synopsis
 *   int state, sym, onenxt, onedef;
 *   mk1tbl( state, sym, onenxt, onedef );
 */

void mk1tbl( state, sym, onenxt, onedef )
int state, sym, onenxt, onedef;

    {
    if ( firstfree < sym )
	firstfree = sym;

    while ( chk[firstfree] != 0 )
	if ( ++firstfree >= current_max_xpairs )
	    expand_nxt_chk();

    base[state] = firstfree - sym;
    def[state] = onedef;
    chk[firstfree] = state;
    nxt[firstfree] = onenxt;

    if ( firstfree > tblend )
	{
	tblend = firstfree++;

	if ( firstfree >= current_max_xpairs )
	    expand_nxt_chk();
	}
    }


/* mkprot - create new proto entry
 *
 * synopsis
 *   int state[], statenum, comstate;
 *   mkprot( state, statenum, comstate );
 */

void mkprot( state, statenum, comstate )
int state[], statenum, comstate;

    {
    int i, slot, tblbase;

    if ( ++numprots >= MSP || numecs * numprots >= PROT_SAVE_SIZE )
	{
	/* gotta make room for the new proto by dropping last entry in
	 * the queue
	 */
	slot = lastprot;
	lastprot = protprev[lastprot];
	protnext[lastprot] = NIL;
	}

    else
	slot = numprots;

    protnext[slot] = firstprot;

    if ( firstprot != NIL )
	protprev[firstprot] = slot;

    firstprot = slot;
    prottbl[slot] = statenum;
    protcomst[slot] = comstate;

    /* copy state into save area so it can be compared with rapidly */
    tblbase = numecs * (slot - 1);

    for ( i = 1; i <= numecs; ++i )
	protsave[tblbase + i] = state[i];
    }


/* mktemplate - create a template entry based on a state, and connect the state
 *              to it
 *
 * synopsis
 *   int state[], statenum, comstate, totaltrans;
 *   mktemplate( state, statenum, comstate, totaltrans );
 */

void mktemplate( state, statenum, comstate )
int state[], statenum, comstate;

    {
    int i, numdiff, tmpbase, tmp[CSIZE + 1];
    Char transset[CSIZE + 1];
    int tsptr;

    ++numtemps;

    tsptr = 0;

    /* calculate where we will temporarily store the transition table
     * of the template in the tnxt[] array.  The final transition table
     * gets created by cmptmps()
     */

    tmpbase = numtemps * numecs;

    if ( tmpbase + numecs >= current_max_template_xpairs )
	{
	current_max_template_xpairs += MAX_TEMPLATE_XPAIRS_INCREMENT;

	++num_reallocs;

	tnxt = reallocate_integer_array( tnxt, current_max_template_xpairs );
	}

    for ( i = 1; i <= numecs; ++i )
	if ( state[i] == 0 )
	    tnxt[tmpbase + i] = 0;
	else
	    {
	    transset[tsptr++] = i;
	    tnxt[tmpbase + i] = comstate;
	    }

    if ( usemecs )
	mkeccl( transset, tsptr, tecfwd, tecbck, numecs, 0 );

    mkprot( tnxt + tmpbase, -numtemps, comstate );

    /* we rely on the fact that mkprot adds things to the beginning
     * of the proto queue
     */

    numdiff = tbldiff( state, firstprot, tmp );
    mkentry( tmp, numecs, statenum, -numtemps, numdiff );
    }


/* mv2front - move proto queue element to front of queue
 *
 * synopsis
 *   int qelm;
 *   mv2front( qelm );
 */

void mv2front( qelm )
int qelm;

    {
    if ( firstprot != qelm )
	{
	if ( qelm == lastprot )
	    lastprot = protprev[lastprot];

	protnext[protprev[qelm]] = protnext[qelm];

	if ( protnext[qelm] != NIL )
	    protprev[protnext[qelm]] = protprev[qelm];

	protprev[qelm] = NIL;
	protnext[qelm] = firstprot;
	protprev[firstprot] = qelm;
	firstprot = qelm;
	}
    }


/* place_state - place a state into full speed transition table
 *
 * synopsis
 *     int *state, statenum, transnum;
 *     place_state( state, statenum, transnum );
 *
 * State is the statenum'th state.  It is indexed by equivalence class and
 * gives the number of the state to enter for a given equivalence class.
 * Transnum is the number of out-transitions for the state.
 */

void place_state( state, statenum, transnum )
int *state, statenum, transnum;

    {
    register int i;
    register int *state_ptr;
    int position = find_table_space( state, transnum );

    /* base is the table of start positions */
    base[statenum] = position;

    /* put in action number marker; this non-zero number makes sure that
     * find_table_space() knows that this position in chk/nxt is taken
     * and should not be used for another accepting number in another state
     */
    chk[position - 1] = 1;

    /* put in end-of-buffer marker; this is for the same purposes as above */
    chk[position] = 1;

    /* place the state into chk and nxt */
    state_ptr = &state[1];

    for ( i = 1; i <= numecs; ++i, ++state_ptr )
	if ( *state_ptr != 0 )
	    {
	    chk[position + i] = i;
	    nxt[position + i] = *state_ptr;
	    }

    if ( position + numecs > tblend )
	tblend = position + numecs;
    }


/* stack1 - save states with only one out-transition to be processed later
 *
 * synopsis
 *   int statenum, sym, nextstate, deflink;
 *   stack1( statenum, sym, nextstate, deflink );
 *
 * if there's room for another state one the "one-transition" stack, the
 * state is pushed onto it, to be processed later by mk1tbl.  If there's
 * no room, we process the sucker right now.
 */

void stack1( statenum, sym, nextstate, deflink )
int statenum, sym, nextstate, deflink;

    {
    if ( onesp >= ONE_STACK_SIZE - 1 )
	mk1tbl( statenum, sym, nextstate, deflink );

    else
	{
	++onesp;
	onestate[onesp] = statenum;
	onesym[onesp] = sym;
	onenext[onesp] = nextstate;
	onedef[onesp] = deflink;
	}
    }


/* tbldiff - compute differences between two state tables
 *
 * synopsis
 *   int state[], pr, ext[];
 *   int tbldiff, numdifferences;
 *   numdifferences = tbldiff( state, pr, ext )
 *
 * "state" is the state array which is to be extracted from the pr'th
 * proto.  "pr" is both the number of the proto we are extracting from
 * and an index into the save area where we can find the proto's complete
 * state table.  Each entry in "state" which differs from the corresponding
 * entry of "pr" will appear in "ext".
 * Entries which are the same in both "state" and "pr" will be marked
 * as transitions to "SAME_TRANS" in "ext".  The total number of differences
 * between "state" and "pr" is returned as function value.  Note that this
 * number is "numecs" minus the number of "SAME_TRANS" entries in "ext".
 */

int tbldiff( state, pr, ext )
int state[], pr, ext[];

    {
    register int i, *sp = state, *ep = ext, *protp;
    register int numdiff = 0;

    protp = &protsave[numecs * (pr - 1)];

    for ( i = numecs; i > 0; --i )
	{
	if ( *++protp == *++sp )
	    *++ep = SAME_TRANS;
	else
	    {
	    *++ep = *sp;
	    ++numdiff;
	    }
	}

    return ( numdiff );
    }
