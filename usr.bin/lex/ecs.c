/* ecs - equivalence class routines */

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
    "@(#) $Header: /a/cvs/386BSD/src/usr.bin/lex/ecs.c,v 1.2 1993/06/29 03:27:07 nate Exp $ (LBL)";
#endif

#include "flexdef.h"

/* ccl2ecl - convert character classes to set of equivalence classes
 *
 * synopsis
 *    ccl2ecl();
 */

void ccl2ecl()

    {
    int i, ich, newlen, cclp, ccls, cclmec;

    for ( i = 1; i <= lastccl; ++i )
	{
	/* we loop through each character class, and for each character
	 * in the class, add the character's equivalence class to the
	 * new "character" class we are creating.  Thus when we are all
	 * done, character classes will really consist of collections
	 * of equivalence classes
	 */

	newlen = 0;
	cclp = cclmap[i];

	for ( ccls = 0; ccls < ccllen[i]; ++ccls )
	    {
	    ich = ccltbl[cclp + ccls];
	    cclmec = ecgroup[ich];

	    if ( xlation && cclmec < 0 )
		{
		/* special hack--if we're doing %t tables then it's
		 * possible that no representative of this character's
		 * equivalence class is in the ccl.  So waiting till
		 * we see the representative would be disastrous.  Instead,
		 * we add this character's equivalence class anyway, if it's
		 * not already present.
		 */
		int j;

		/* this loop makes this whole process n^2; but we don't
		 * really care about %t performance anyway
		 */
		for ( j = 0; j < newlen; ++j )
		    if ( ccltbl[cclp + j] == -cclmec )
			break;

		if ( j >= newlen )
		    { /* no representative yet, add this one in */
		    ccltbl[cclp + newlen] = -cclmec;
		    ++newlen;
		    }
		}

	    else if ( cclmec > 0 )
		{
		ccltbl[cclp + newlen] = cclmec;
		++newlen;
		}
	    }

	ccllen[i] = newlen;
	}
    }


/* cre8ecs - associate equivalence class numbers with class members
 *
 * synopsis
 *    int cre8ecs();
 *    number of classes = cre8ecs( fwd, bck, num );
 *
 *  fwd is the forward linked-list of equivalence class members.  bck
 *  is the backward linked-list, and num is the number of class members.
 *
 *  Returned is the number of classes.
 */

int cre8ecs( fwd, bck, num )
int fwd[], bck[], num;

    {
    int i, j, numcl;

    numcl = 0;

    /* create equivalence class numbers.  From now on, abs( bck(x) )
     * is the equivalence class number for object x.  If bck(x)
     * is positive, then x is the representative of its equivalence
     * class.
     */
    for ( i = 1; i <= num; ++i )
	if ( bck[i] == NIL )
	    {
	    bck[i] = ++numcl;
	    for ( j = fwd[i]; j != NIL; j = fwd[j] )
		bck[j] = -numcl;
	    }

    return ( numcl );
    }


/* ecs_from_xlation - associate equivalence class numbers using %t table
 *
 * synopsis
 *    numecs = ecs_from_xlation( ecmap );
 *
 *  Upon return, ecmap will map each character code to its equivalence
 *  class.  The mapping will be positive if the character is the representative
 *  of its class, negative otherwise.
 *
 *  Returns the number of equivalence classes used.
 */

int ecs_from_xlation( ecmap )
int ecmap[];

    {
    int i;
    int nul_is_alone = false;
    int did_default_xlation_class = false;

    if ( xlation[0] != 0 )
	{
	/* if NUL shares its translation with other characters, choose one
	 * of the other characters as the representative for the equivalence
	 * class.  This allows a cheap test later to see whether we can
	 * do away with NUL's equivalence class.
	 */
	for ( i = 1; i < csize; ++i )
	    if ( xlation[i] == -xlation[0] )
		{
		xlation[i] = xlation[0];
		ecmap[0] = -xlation[0];
		break;
		}

	if ( i >= csize )
	    /* didn't find a companion character--remember this fact */
	    nul_is_alone = true;
	}

    for ( i = 1; i < csize; ++i )
	if ( xlation[i] == 0 )
	    {
	    if ( did_default_xlation_class )
		ecmap[i] = -num_xlations;
	    
	    else
		{
		/* make an equivalence class for those characters not
		 * specified in the %t table
		 */
		++num_xlations;
		ecmap[i] = num_xlations;
		did_default_xlation_class = true;
		}
	    }

	else
	    ecmap[i] = xlation[i];

    if ( nul_is_alone )
	/* force NUL's equivalence class to be the last one */
	{
	++num_xlations;
	ecmap[0] = num_xlations;

	/* there's actually a bug here: if someone is fanatic enough to
	 * put every character in its own translation class, then right
	 * now we just promoted NUL's equivalence class to be csize + 1;
	 * we can handle NUL's class number being == csize (by instead
	 * putting it in its own table), but we can't handle some *other*
	 * character having to be put in its own table, too.  So in
	 * this case we bail out.
	 */
	if ( num_xlations > csize )
	    flexfatal( "too many %t classes!" );
	}

    return num_xlations;
    }


/* mkeccl - update equivalence classes based on character class xtions
 *
 * synopsis
 *    Char ccls[];
 *    int lenccl, fwd[llsiz], bck[llsiz], llsiz, NUL_mapping;
 *    mkeccl( ccls, lenccl, fwd, bck, llsiz, NUL_mapping );
 *
 * where ccls contains the elements of the character class, lenccl is the
 * number of elements in the ccl, fwd is the forward link-list of equivalent
 * characters, bck is the backward link-list, and llsiz size of the link-list
 *
 * NUL_mapping is the value which NUL (0) should be mapped to.
 */

void mkeccl( ccls, lenccl, fwd, bck, llsiz, NUL_mapping )
Char ccls[];
int lenccl, fwd[], bck[], llsiz, NUL_mapping;

    {
    int cclp, oldec, newec;
    int cclm, i, j;
    static unsigned char cclflags[CSIZE];	/* initialized to all '\0' */

    /* note that it doesn't matter whether or not the character class is
     * negated.  The same results will be obtained in either case.
     */

    cclp = 0;

    while ( cclp < lenccl )
	{
	cclm = ccls[cclp];

	if ( NUL_mapping && cclm == 0 )
	    cclm = NUL_mapping;

	oldec = bck[cclm];
	newec = cclm;

	j = cclp + 1;

	for ( i = fwd[cclm]; i != NIL && i <= llsiz; i = fwd[i] )
	    { /* look for the symbol in the character class */
	    for ( ; j < lenccl; ++j )
		{
		register int ccl_char;

		if ( NUL_mapping && ccls[j] == 0 )
		    ccl_char = NUL_mapping;
		else
		    ccl_char = ccls[j];

		if ( ccl_char > i )
		    break;

		if ( ccl_char == i && ! cclflags[j] )
		    {
		    /* we found an old companion of cclm in the ccl.
		     * link it into the new equivalence class and flag it as
		     * having been processed
		     */

		    bck[i] = newec;
		    fwd[newec] = i;
		    newec = i;
		    cclflags[j] = 1;	/* set flag so we don't reprocess */

		    /* get next equivalence class member */
		    /* continue 2 */
		    goto next_pt;
		    }
		}

	    /* symbol isn't in character class.  Put it in the old equivalence
	     * class
	     */

	    bck[i] = oldec;

	    if ( oldec != NIL )
		fwd[oldec] = i;

	    oldec = i;
next_pt:
	    ;
	    }

	if ( bck[cclm] != NIL || oldec != bck[cclm] )
	    {
	    bck[cclm] = NIL;
	    fwd[oldec] = NIL;
	    }

	fwd[newec] = NIL;

	/* find next ccl member to process */

	for ( ++cclp; cclflags[cclp] && cclp < lenccl; ++cclp )
	    {
	    /* reset "doesn't need processing" flag */
	    cclflags[cclp] = 0;
	    }
	}
    }


/* mkechar - create equivalence class for single character
 *
 * synopsis
 *    int tch, fwd[], bck[];
 *    mkechar( tch, fwd, bck );
 */

void mkechar( tch, fwd, bck )
int tch, fwd[], bck[];

    {
    /* if until now the character has been a proper subset of
     * an equivalence class, break it away to create a new ec
     */

    if ( fwd[tch] != NIL )
	bck[fwd[tch]] = bck[tch];

    if ( bck[tch] != NIL )
	fwd[bck[tch]] = fwd[tch];

    fwd[tch] = NIL;
    bck[tch] = NIL;
    }
