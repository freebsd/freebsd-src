/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)tahoe.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include	"gprof.h"

    /*
     *	a namelist entry to be the child of indirect callf
     */
nltype	indirectchild = {
	"(*)" ,				/* the name */
	(unsigned long) 0 ,		/* the pc entry point */
	(unsigned long) 0 ,		/* entry point aligned to histogram */
	(double) 0.0 ,			/* ticks in this routine */
	(double) 0.0 ,			/* cumulative ticks in children */
	(long) 0 ,			/* how many times called */
	(long) 0 ,			/* how many calls to self */
	(double) 1.0 ,			/* propagation fraction */
	(double) 0.0 ,			/* self propagation time */
	(double) 0.0 ,			/* child propagation time */
	(bool) 0 ,			/* print flag */
	(int) 0 ,			/* index in the graph list */
	(int) 0 , 			/* graph call chain top-sort order */
	(int) 0 ,			/* internal number of cycle on */
	(struct nl *) &indirectchild ,	/* pointer to head of cycle */
	(struct nl *) 0 ,		/* pointer to next member of cycle */
	(arctype *) 0 ,			/* list of caller arcs */
	(arctype *) 0 			/* list of callee arcs */
    };

operandenum
operandmode( modep )
    unsigned char	*modep;
{
    long	usesreg = ((long)*modep) & 0xf;

    switch ( ((long)*modep) >> 4 ) {
	case 0:
	case 1:
	case 2:
	case 3:
	    return literal;
	case 4:
	    return indexed;
	case 5:
	    return reg;
	case 6:
	    return regdef;
	case 7:
	    return autodec;
	case 8:
	    return ( usesreg != 0xe ? autoinc : immediate );
	case 9:
	    return ( usesreg != PC ? autoincdef : absolute );
	case 10:
	    return ( usesreg != PC ? bytedisp : byterel );
	case 11:
	    return ( usesreg != PC ? bytedispdef : bytereldef );
	case 12:
	    return ( usesreg != PC ? worddisp : wordrel );
	case 13:
	    return ( usesreg != PC ? worddispdef : wordreldef );
	case 14:
	    return ( usesreg != PC ? longdisp : longrel );
	case 15:
	    return ( usesreg != PC ? longdispdef : longreldef );
    }
    /* NOTREACHED */
}

char *
operandname( mode )
    operandenum	mode;
{

    switch ( mode ) {
	case literal:
	    return "literal";
	case indexed:
	    return "indexed";
	case reg:
	    return "register";
	case regdef:
	    return "register deferred";
	case autodec:
	    return "autodecrement";
	case autoinc:
	    return "autoincrement";
	case autoincdef:
	    return "autoincrement deferred";
	case bytedisp:
	    return "byte displacement";
	case bytedispdef:
	    return "byte displacement deferred";
	case byterel:
	    return "byte relative";
	case bytereldef:
	    return "byte relative deferred";
	case worddisp:
	    return "word displacement";
	case worddispdef:
	    return "word displacement deferred";
	case wordrel:
	    return "word relative";
	case wordreldef:
	    return "word relative deferred";
	case immediate:
	    return "immediate";
	case absolute:
	    return "absolute";
	case longdisp:
	    return "long displacement";
	case longdispdef:
	    return "long displacement deferred";
	case longrel:
	    return "long relative";
	case longreldef:
	    return "long relative deferred";
    }
    /* NOTREACHED */
}

long
operandlength( modep )
    unsigned char	*modep;
{

    switch ( operandmode( modep ) ) {
	case literal:
	case reg:
	case regdef:
	case autodec:
	case autoinc:
	case autoincdef:
	    return 1;
	case bytedisp:
	case bytedispdef:
	case byterel:
	case bytereldef:
	    return 2;
	case worddisp:
	case worddispdef:
	case wordrel:
	case wordreldef:
	    return 3;
	case immediate:
	case absolute:
	case longdisp:
	case longdispdef:
	case longrel:
	case longreldef:
	    return 5;
	case indexed:
	    return 1+operandlength( modep + 1 );
    }
    /* NOTREACHED */
}

unsigned long
reladdr( modep )
    char	*modep;
{
    operandenum	mode = operandmode( modep );
    char	*cp;
    short	*sp;
    long	*lp;
    int		i;
    long	value = 0;

    cp = modep;
    cp += 1;			/* skip over the mode */
    switch ( mode ) {
	default:
	    fprintf( stderr , "[reladdr] not relative address\n" );
	    return (unsigned long) modep;
	case byterel:
	    return (unsigned long) ( cp + sizeof *cp + *cp );
	case wordrel:
	    for (i = 0; i < sizeof *sp; i++)
		value = (value << 8) + (cp[i] & 0xff);
	    return (unsigned long) ( cp + sizeof *sp + value );
	case longrel:
	    for (i = 0; i < sizeof *lp; i++)
		value = (value << 8) + (cp[i] & 0xff);
	    return (unsigned long) ( cp + sizeof *lp + value );
    }
}

findcall( parentp , p_lowpc , p_highpc )
    nltype		*parentp;
    unsigned long	p_lowpc;
    unsigned long	p_highpc;
{
    unsigned char	*instructp;
    long		length;
    nltype		*childp;
    operandenum		mode;
    operandenum		firstmode;
    unsigned long	destpc;

    if ( textspace == 0 ) {
	return;
    }
    if ( p_lowpc < s_lowpc ) {
	p_lowpc = s_lowpc;
    }
    if ( p_highpc > s_highpc ) {
	p_highpc = s_highpc;
    }
#   ifdef DEBUG
	if ( debug & CALLDEBUG ) {
	    printf( "[findcall] %s: 0x%x to 0x%x\n" ,
		    parentp -> name , p_lowpc , p_highpc );
	}
#   endif DEBUG
    for (   instructp = textspace + p_lowpc ;
	    instructp < textspace + p_highpc ;
	    instructp += length ) {
	length = 1;
	if ( *instructp == CALLF ) {
		/*
		 *	maybe a callf, better check it out.
		 *	skip the count of the number of arguments.
		 */
#	    ifdef DEBUG
		if ( debug & CALLDEBUG ) {
		    printf( "[findcall]\t0x%x:callf" , instructp - textspace );
		}
#	    endif DEBUG
	    firstmode = operandmode( instructp+length );
	    switch ( firstmode ) {
		case literal:
		case immediate:
		    break;
		default:
		    goto botched;
	    }
	    length += operandlength( instructp+length );
	    mode = operandmode( instructp + length );
#	    ifdef DEBUG
		if ( debug & CALLDEBUG ) {
		    printf( "\tfirst operand is %s", operandname( firstmode ) );
		    printf( "\tsecond operand is %s\n" , operandname( mode ) );
		}
#	    endif DEBUG
	    switch ( mode ) {
		case regdef:
		case bytedispdef:
		case worddispdef:
		case longdispdef:
		case bytereldef:
		case wordreldef:
		case longreldef:
			/*
			 *	indirect call: call through pointer
			 *	either	*d(r)	as a parameter or local
			 *		(r)	as a return value
			 *		*f	as a global pointer
			 *	[are there others that we miss?,
			 *	 e.g. arrays of pointers to functions???]
			 */
		    addarc( parentp , &indirectchild , (long) 0 );
		    length += operandlength( instructp + length );
		    continue;
		case byterel:
		case wordrel:
		case longrel:
			/*
			 *	regular pc relative addressing
			 *	check that this is the address of
			 *	a function.
			 */
		    destpc = reladdr( instructp+length )
				- (unsigned long) textspace;
		    if ( destpc >= s_lowpc && destpc <= s_highpc ) {
			childp = nllookup( destpc );
#			ifdef DEBUG
			    if ( debug & CALLDEBUG ) {
				printf( "[findcall]\tdestpc 0x%x" , destpc );
				printf( " childp->name %s" , childp -> name );
				printf( " childp->value 0x%x\n" ,
					childp -> value );
			    }
#			endif DEBUG
			if ( childp -> value == destpc ) {
				/*
				 *	a hit
				 */
			    addarc( parentp , childp , (long) 0 );
			    length += operandlength( instructp + length );
			    continue;
			}
			goto botched;
		    }
			/*
			 *	else:
			 *	it looked like a callf,
			 *	but it wasn't to anywhere.
			 */
		    goto botched;
		default:
		botched:
			/*
			 *	something funny going on.
			 */
#		    ifdef DEBUG
			if ( debug & CALLDEBUG ) {
			    printf( "[findcall]\tbut it's a botch\n" );
			}
#		    endif DEBUG
		    length = 1;
		    continue;
	    }
	}
    }
}
