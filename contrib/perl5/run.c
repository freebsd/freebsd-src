/*    run.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#include "EXTERN.h"
#include "perl.h"

/*
 * "Away now, Shadowfax!  Run, greatheart, run as you have never run before!
 * Now we are come to the lands where you were foaled, and every stone you
 * know.  Run now!  Hope is in speed!"  --Gandalf
 */

#ifdef PERL_OBJECT
#define CALLOP this->*PL_op
#else
#define CALLOP *PL_op
#endif

int
runops_standard(void)
{
    dTHR;

    while ( PL_op = (CALLOP->op_ppaddr)(ARGS) ) ;

    TAINT_NOT;
    return 0;
}

#ifdef DEBUGGING

dEXT char **watchaddr = 0;
dEXT char *watchok;

#ifndef PERL_OBJECT
static void debprof _((OP*o));
#endif

#endif	/* DEBUGGING */

int
runops_debug(void)
{
#ifdef DEBUGGING
    dTHR;
    if (!PL_op) {
	warn("NULL OP IN RUN");
	return 0;
    }

    do {
	if (PL_debug) {
	    if (watchaddr != 0 && *watchaddr != watchok)
		PerlIO_printf(Perl_debug_log, "WARNING: %lx changed from %lx to %lx\n",
		    (long)watchaddr, (long)watchok, (long)*watchaddr);
	    DEBUG_s(debstack());
	    DEBUG_t(debop(PL_op));
	    DEBUG_P(debprof(PL_op));
	}
    } while ( PL_op = (CALLOP->op_ppaddr)(ARGS) );

    TAINT_NOT;
    return 0;
#else
    return runops_standard();
#endif	/* DEBUGGING */
}

I32
debop(OP *o)
{
#ifdef DEBUGGING
    SV *sv;
    STRLEN n_a;
    deb("%s", op_name[o->op_type]);
    switch (o->op_type) {
    case OP_CONST:
	PerlIO_printf(Perl_debug_log, "(%s)", SvPEEK(cSVOPo->op_sv));
	break;
    case OP_GVSV:
    case OP_GV:
	if (cGVOPo->op_gv) {
	    sv = NEWSV(0,0);
	    gv_fullname3(sv, cGVOPo->op_gv, Nullch);
	    PerlIO_printf(Perl_debug_log, "(%s)", SvPV(sv, n_a));
	    SvREFCNT_dec(sv);
	}
	else
	    PerlIO_printf(Perl_debug_log, "(NULL)");
	break;
    default:
	break;
    }
    PerlIO_printf(Perl_debug_log, "\n");
#endif	/* DEBUGGING */
    return 0;
}

void
watch(char **addr)
{
#ifdef DEBUGGING
    watchaddr = addr;
    watchok = *addr;
    PerlIO_printf(Perl_debug_log, "WATCHING, %lx is currently %lx\n",
	(long)watchaddr, (long)watchok);
#endif	/* DEBUGGING */
}

STATIC void
debprof(OP *o)
{
#ifdef DEBUGGING
    if (!PL_profiledata)
	Newz(000, PL_profiledata, MAXO, U32);
    ++PL_profiledata[o->op_type];
#endif /* DEBUGGING */
}

void
debprofdump(void)
{
#ifdef DEBUGGING
    unsigned i;
    if (!PL_profiledata)
	return;
    for (i = 0; i < MAXO; i++) {
	if (PL_profiledata[i])
	    PerlIO_printf(Perl_debug_log,
			  "%5lu %s\n", (unsigned long)PL_profiledata[i],
                                       op_name[i]);
    }
#endif	/* DEBUGGING */
}
