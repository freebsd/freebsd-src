/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)arcs.c	5.6 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdlib.h>
#include "gprof.h"

    /*
     *	add (or just increment) an arc
     */
addarc( parentp , childp , count )
    nltype	*parentp;
    nltype	*childp;
    long	count;
{
    arctype		*arcp;

#   ifdef DEBUG
	if ( debug & TALLYDEBUG ) {
	    printf( "[addarc] %d arcs from %s to %s\n" ,
		    count , parentp -> name , childp -> name );
	}
#   endif DEBUG
    arcp = arclookup( parentp , childp );
    if ( arcp != 0 ) {
	    /*
	     *	a hit:  just increment the count.
	     */
#	ifdef DEBUG
	    if ( debug & TALLYDEBUG ) {
		printf( "[tally] hit %d += %d\n" ,
			arcp -> arc_count , count );
	    }
#	endif DEBUG
	arcp -> arc_count += count;
	return;
    }
    arcp = (arctype *) calloc( 1 , sizeof *arcp );
    arcp -> arc_parentp = parentp;
    arcp -> arc_childp = childp;
    arcp -> arc_count = count;
	/*
	 *	prepend this child to the children of this parent
	 */
    arcp -> arc_childlist = parentp -> children;
    parentp -> children = arcp;
	/*
	 *	prepend this parent to the parents of this child
	 */
    arcp -> arc_parentlist = childp -> parents;
    childp -> parents = arcp;
}

    /*
     *	the code below topologically sorts the graph (collapsing cycles),
     *	and propagates time bottom up and flags top down.
     */

    /*
     *	the topologically sorted name list pointers
     */
nltype	**topsortnlp;

topcmp( npp1 , npp2 )
    nltype	**npp1;
    nltype	**npp2;
{
    return (*npp1) -> toporder - (*npp2) -> toporder;
}

nltype **
doarcs()
{
    nltype	*parentp, **timesortnlp;
    arctype	*arcp;
    long	index;

	/*
	 *	initialize various things:
	 *	    zero out child times.
	 *	    count self-recursive calls.
	 *	    indicate that nothing is on cycles.
	 */
    for ( parentp = nl ; parentp < npe ; parentp++ ) {
	parentp -> childtime = 0.0;
	arcp = arclookup( parentp , parentp );
	if ( arcp != 0 ) {
	    parentp -> ncall -= arcp -> arc_count;
	    parentp -> selfcalls = arcp -> arc_count;
	} else {
	    parentp -> selfcalls = 0;
	}
	parentp -> propfraction = 0.0;
	parentp -> propself = 0.0;
	parentp -> propchild = 0.0;
	parentp -> printflag = FALSE;
	parentp -> toporder = DFN_NAN;
	parentp -> cycleno = 0;
	parentp -> cyclehead = parentp;
	parentp -> cnext = 0;
	if ( cflag ) {
	    findcall( parentp , parentp -> value , (parentp+1) -> value );
	}
    }
	/*
	 *	topologically order things
	 *	if any node is unnumbered,
	 *	    number it and any of its descendents.
	 */
    for ( parentp = nl ; parentp < npe ; parentp++ ) {
	if ( parentp -> toporder == DFN_NAN ) {
	    dfn( parentp );
	}
    }
	/*
	 *	link together nodes on the same cycle
	 */
    cyclelink();
	/*
	 *	Sort the symbol table in reverse topological order
	 */
    topsortnlp = (nltype **) calloc( nname , sizeof(nltype *) );
    if ( topsortnlp == (nltype **) 0 ) {
	fprintf( stderr , "[doarcs] ran out of memory for topo sorting\n" );
    }
    for ( index = 0 ; index < nname ; index += 1 ) {
	topsortnlp[ index ] = &nl[ index ];
    }
    qsort( topsortnlp , nname , sizeof(nltype *) , topcmp );
#   ifdef DEBUG
	if ( debug & DFNDEBUG ) {
	    printf( "[doarcs] topological sort listing\n" );
	    for ( index = 0 ; index < nname ; index += 1 ) {
		printf( "[doarcs] " );
		printf( "%d:" , topsortnlp[ index ] -> toporder );
		printname( topsortnlp[ index ] );
		printf( "\n" );
	    }
	}
#   endif DEBUG
	/*
	 *	starting from the topological top,
	 *	propagate print flags to children.
	 *	also, calculate propagation fractions.
	 *	this happens before time propagation
	 *	since time propagation uses the fractions.
	 */
    doflags();
	/*
	 *	starting from the topological bottom, 
	 *	propogate children times up to parents.
	 */
    dotime();
	/*
	 *	Now, sort by propself + propchild.
	 *	sorting both the regular function names
	 *	and cycle headers.
	 */
    timesortnlp = (nltype **) calloc( nname + ncycle , sizeof(nltype *) );
    if ( timesortnlp == (nltype **) 0 ) {
	fprintf( stderr , "%s: ran out of memory for sorting\n" , whoami );
    }
    for ( index = 0 ; index < nname ; index++ ) {
	timesortnlp[index] = &nl[index];
    }
    for ( index = 1 ; index <= ncycle ; index++ ) {
	timesortnlp[nname+index-1] = &cyclenl[index];
    }
    qsort( timesortnlp , nname + ncycle , sizeof(nltype *) , totalcmp );
    for ( index = 0 ; index < nname + ncycle ; index++ ) {
	timesortnlp[ index ] -> index = index + 1;
    }
    return( timesortnlp );
}

dotime()
{
    int	index;

    cycletime();
    for ( index = 0 ; index < nname ; index += 1 ) {
	timepropagate( topsortnlp[ index ] );
    }
}

timepropagate( parentp )
    nltype	*parentp;
{
    arctype	*arcp;
    nltype	*childp;
    double	share;
    double	propshare;

    if ( parentp -> propfraction == 0.0 ) {
	return;
    }
	/*
	 *	gather time from children of this parent.
	 */
    for ( arcp = parentp -> children ; arcp ; arcp = arcp -> arc_childlist ) {
	childp = arcp -> arc_childp;
	if ( arcp -> arc_count == 0 ) {
	    continue;
	}
	if ( childp == parentp ) {
	    continue;
	}
	if ( childp -> propfraction == 0.0 ) {
	    continue;
	}
	if ( childp -> cyclehead != childp ) {
	    if ( parentp -> cycleno == childp -> cycleno ) {
		continue;
	    }
	    if ( parentp -> toporder <= childp -> toporder ) {
		fprintf( stderr , "[propagate] toporder botches\n" );
	    }
	    childp = childp -> cyclehead;
	} else {
	    if ( parentp -> toporder <= childp -> toporder ) {
		fprintf( stderr , "[propagate] toporder botches\n" );
		continue;
	    }
	}
	if ( childp -> ncall == 0 ) {
	    continue;
	}
	    /*
	     *	distribute time for this arc
	     */
	arcp -> arc_time = childp -> time
			        * ( ( (double) arcp -> arc_count ) /
				    ( (double) childp -> ncall ) );
	arcp -> arc_childtime = childp -> childtime
			        * ( ( (double) arcp -> arc_count ) /
				    ( (double) childp -> ncall ) );
	share = arcp -> arc_time + arcp -> arc_childtime;
	parentp -> childtime += share;
	    /*
	     *	( 1 - propfraction ) gets lost along the way
	     */
	propshare = parentp -> propfraction * share;
	    /*
	     *	fix things for printing
	     */
	parentp -> propchild += propshare;
	arcp -> arc_time *= parentp -> propfraction;
	arcp -> arc_childtime *= parentp -> propfraction;
	    /*
	     *	add this share to the parent's cycle header, if any.
	     */
	if ( parentp -> cyclehead != parentp ) {
	    parentp -> cyclehead -> childtime += share;
	    parentp -> cyclehead -> propchild += propshare;
	}
#	ifdef DEBUG
	    if ( debug & PROPDEBUG ) {
		printf( "[dotime] child \t" );
		printname( childp );
		printf( " with %f %f %d/%d\n" ,
			childp -> time , childp -> childtime ,
			arcp -> arc_count , childp -> ncall );
		printf( "[dotime] parent\t" );
		printname( parentp );
		printf( "\n[dotime] share %f\n" , share );
	    }
#	endif DEBUG
    }
}

cyclelink()
{
    register nltype	*nlp;
    register nltype	*cyclenlp;
    int			cycle;
    nltype		*memberp;
    arctype		*arcp;

	/*
	 *	Count the number of cycles, and initialze the cycle lists
	 */
    ncycle = 0;
    for ( nlp = nl ; nlp < npe ; nlp++ ) {
	    /*
	     *	this is how you find unattached cycles
	     */
	if ( nlp -> cyclehead == nlp && nlp -> cnext != 0 ) {
	    ncycle += 1;
	}
    }
	/*
	 *	cyclenl is indexed by cycle number:
	 *	i.e. it is origin 1, not origin 0.
	 */
    cyclenl = (nltype *) calloc( ncycle + 1 , sizeof( nltype ) );
    if ( cyclenl == 0 ) {
	fprintf( stderr , "%s: No room for %d bytes of cycle headers\n" ,
		whoami , ( ncycle + 1 ) * sizeof( nltype ) );
	done();
    }
	/*
	 *	now link cycles to true cycleheads,
	 *	number them, accumulate the data for the cycle
	 */
    cycle = 0;
    for ( nlp = nl ; nlp < npe ; nlp++ ) {
	if ( !( nlp -> cyclehead == nlp && nlp -> cnext != 0 ) ) {
	    continue;
	}
	cycle += 1;
	cyclenlp = &cyclenl[cycle];
        cyclenlp -> name = 0;		/* the name */
        cyclenlp -> value = 0;		/* the pc entry point */
        cyclenlp -> time = 0.0;		/* ticks in this routine */
        cyclenlp -> childtime = 0.0;	/* cumulative ticks in children */
	cyclenlp -> ncall = 0;		/* how many times called */
	cyclenlp -> selfcalls = 0;	/* how many calls to self */
	cyclenlp -> propfraction = 0.0;	/* what % of time propagates */
	cyclenlp -> propself = 0.0;	/* how much self time propagates */
	cyclenlp -> propchild = 0.0;	/* how much child time propagates */
	cyclenlp -> printflag = TRUE;	/* should this be printed? */
	cyclenlp -> index = 0;		/* index in the graph list */
	cyclenlp -> toporder = DFN_NAN;	/* graph call chain top-sort order */
	cyclenlp -> cycleno = cycle;	/* internal number of cycle on */
	cyclenlp -> cyclehead = cyclenlp;	/* pointer to head of cycle */
	cyclenlp -> cnext = nlp;	/* pointer to next member of cycle */
	cyclenlp -> parents = 0;	/* list of caller arcs */
	cyclenlp -> children = 0;	/* list of callee arcs */
#	ifdef DEBUG
	    if ( debug & CYCLEDEBUG ) {
		printf( "[cyclelink] " );
		printname( nlp );
		printf( " is the head of cycle %d\n" , cycle );
	    }
#	endif DEBUG
	    /*
	     *	link members to cycle header
	     */
	for ( memberp = nlp ; memberp ; memberp = memberp -> cnext ) { 
	    memberp -> cycleno = cycle;
	    memberp -> cyclehead = cyclenlp;
	}
	    /*
	     *	count calls from outside the cycle
	     *	and those among cycle members
	     */
	for ( memberp = nlp ; memberp ; memberp = memberp -> cnext ) {
	    for ( arcp=memberp->parents ; arcp ; arcp=arcp->arc_parentlist ) {
		if ( arcp -> arc_parentp == memberp ) {
		    continue;
		}
		if ( arcp -> arc_parentp -> cycleno == cycle ) {
		    cyclenlp -> selfcalls += arcp -> arc_count;
		} else {
		    cyclenlp -> ncall += arcp -> arc_count;
		}
	    }
	}
    }
}

cycletime()
{
    int			cycle;
    nltype		*cyclenlp;
    nltype		*childp;

    for ( cycle = 1 ; cycle <= ncycle ; cycle += 1 ) {
	cyclenlp = &cyclenl[ cycle ];
	for ( childp = cyclenlp -> cnext ; childp ; childp = childp -> cnext ) {
	    if ( childp -> propfraction == 0.0 ) {
		    /*
		     * all members have the same propfraction except those
		     *	that were excluded with -E
		     */
		continue;
	    }
	    cyclenlp -> time += childp -> time;
	}
	cyclenlp -> propself = cyclenlp -> propfraction * cyclenlp -> time;
    }
}

    /*
     *	in one top to bottom pass over the topologically sorted namelist
     *	propagate:
     *		printflag as the union of parents' printflags
     *		propfraction as the sum of fractional parents' propfractions
     *	and while we're here, sum time for functions.
     */
doflags()
{
    int		index;
    nltype	*childp;
    nltype	*oldhead;

    oldhead = 0;
    for ( index = nname-1 ; index >= 0 ; index -= 1 ) {
	childp = topsortnlp[ index ];
	    /*
	     *	if we haven't done this function or cycle,
	     *	inherit things from parent.
	     *	this way, we are linear in the number of arcs
	     *	since we do all members of a cycle (and the cycle itself)
	     *	as we hit the first member of the cycle.
	     */
	if ( childp -> cyclehead != oldhead ) {
	    oldhead = childp -> cyclehead;
	    inheritflags( childp );
	}
#	ifdef DEBUG
	    if ( debug & PROPDEBUG ) {
		printf( "[doflags] " );
		printname( childp );
		printf( " inherits printflag %d and propfraction %f\n" ,
			childp -> printflag , childp -> propfraction );
	    }
#	endif DEBUG
	if ( ! childp -> printflag ) {
		/*
		 *	printflag is off
		 *	it gets turned on by
		 *	being on -f list,
		 *	or there not being any -f list and not being on -e list.
		 */
	    if (   onlist( flist , childp -> name )
		|| ( !fflag && !onlist( elist , childp -> name ) ) ) {
		childp -> printflag = TRUE;
	    }
	} else {
		/*
		 *	this function has printing parents:
		 *	maybe someone wants to shut it up
		 *	by putting it on -e list.  (but favor -f over -e)
		 */
	    if (  ( !onlist( flist , childp -> name ) )
		&& onlist( elist , childp -> name ) ) {
		childp -> printflag = FALSE;
	    }
	}
	if ( childp -> propfraction == 0.0 ) {
		/*
		 *	no parents to pass time to.
		 *	collect time from children if
		 *	its on -F list,
		 *	or there isn't any -F list and its not on -E list.
		 */
	    if ( onlist( Flist , childp -> name )
		|| ( !Fflag && !onlist( Elist , childp -> name ) ) ) {
		    childp -> propfraction = 1.0;
	    }
	} else {
		/*
		 *	it has parents to pass time to, 
		 *	but maybe someone wants to shut it up
		 *	by puttting it on -E list.  (but favor -F over -E)
		 */
	    if (  !onlist( Flist , childp -> name )
		&& onlist( Elist , childp -> name ) ) {
		childp -> propfraction = 0.0;
	    }
	}
	childp -> propself = childp -> time * childp -> propfraction;
	printtime += childp -> propself;
#	ifdef DEBUG
	    if ( debug & PROPDEBUG ) {
		printf( "[doflags] " );
		printname( childp );
		printf( " ends up with printflag %d and propfraction %f\n" ,
			childp -> printflag , childp -> propfraction );
		printf( "time %f propself %f printtime %f\n" ,
			childp -> time , childp -> propself , printtime );
	    }
#	endif DEBUG
    }
}

    /*
     *	check if any parent of this child
     *	(or outside parents of this cycle)
     *	have their print flags on and set the 
     *	print flag of the child (cycle) appropriately.
     *	similarly, deal with propagation fractions from parents.
     */
inheritflags( childp )
    nltype	*childp;
{
    nltype	*headp;
    arctype	*arcp;
    nltype	*parentp;
    nltype	*memp;

    headp = childp -> cyclehead;
    if ( childp == headp ) {
	    /*
	     *	just a regular child, check its parents
	     */
	childp -> printflag = FALSE;
	childp -> propfraction = 0.0;
	for (arcp = childp -> parents ; arcp ; arcp = arcp -> arc_parentlist) {
	    parentp = arcp -> arc_parentp;
	    if ( childp == parentp ) {
		continue;
	    }
	    childp -> printflag |= parentp -> printflag;
		/*
		 *	if the child was never actually called
		 *	(e.g. this arc is static (and all others are, too))
		 *	no time propagates along this arc.
		 */
	    if ( childp -> ncall ) {
		childp -> propfraction += parentp -> propfraction
					    * ( ( (double) arcp -> arc_count )
					      / ( (double) childp -> ncall ) );
	    }
	}
    } else {
	    /*
	     *	its a member of a cycle, look at all parents from 
	     *	outside the cycle
	     */
	headp -> printflag = FALSE;
	headp -> propfraction = 0.0;
	for ( memp = headp -> cnext ; memp ; memp = memp -> cnext ) {
	    for (arcp = memp->parents ; arcp ; arcp = arcp->arc_parentlist) {
		if ( arcp -> arc_parentp -> cyclehead == headp ) {
		    continue;
		}
		parentp = arcp -> arc_parentp;
		headp -> printflag |= parentp -> printflag;
		    /*
		     *	if the cycle was never actually called
		     *	(e.g. this arc is static (and all others are, too))
		     *	no time propagates along this arc.
		     */
		if ( headp -> ncall ) {
		    headp -> propfraction += parentp -> propfraction
					    * ( ( (double) arcp -> arc_count )
					      / ( (double) headp -> ncall ) );
		}
	    }
	}
	for ( memp = headp ; memp ; memp = memp -> cnext ) {
	    memp -> printflag = headp -> printflag;
	    memp -> propfraction = headp -> propfraction;
	}
    }
}
