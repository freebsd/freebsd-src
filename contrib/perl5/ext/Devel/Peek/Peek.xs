#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

SV *
DeadCode(pTHX)
{
#ifdef PURIFY
    return Nullsv;
#else
    SV* sva;
    SV* sv, *dbg;
    SV* ret = newRV_noinc((SV*)newAV());
    register SV* svend;
    int tm = 0, tref = 0, ts = 0, ta = 0, tas = 0;

    for (sva = PL_sv_arenaroot; sva; sva = (SV*)SvANY(sva)) {
	svend = &sva[SvREFCNT(sva)];
	for (sv = sva + 1; sv < svend; ++sv) {
	    if (SvTYPE(sv) == SVt_PVCV) {
		CV *cv = (CV*)sv;
		AV* padlist = CvPADLIST(cv), *argav;
		SV** svp;
		SV** pad;
		int i = 0, j, levelm, totm = 0, levelref, totref = 0;
		int levels, tots = 0, levela, tota = 0, levelas, totas = 0;
		int dumpit = 0;

		if (CvXSUB(sv)) {
		    continue;		/* XSUB */
		}
		if (!CvGV(sv)) {
		    continue;		/* file-level scope. */
		}
		if (!CvROOT(cv)) {
		    /* PerlIO_printf(Perl_debug_log, "  no root?!\n"); */
		    continue;		/* autoloading stub. */
		}
		do_gvgv_dump(0, Perl_debug_log, "GVGV::GV", CvGV(sv));
		if (CvDEPTH(cv)) {
		    PerlIO_printf(Perl_debug_log, "  busy\n");
		    continue;
		}
		svp = AvARRAY(padlist);
		while (++i <= AvFILL(padlist)) { /* Depth. */
		    SV **args;
		    
		    pad = AvARRAY((AV*)svp[i]);
		    argav = (AV*)pad[0];
		    if (!argav || (SV*)argav == &PL_sv_undef) {
			PerlIO_printf(Perl_debug_log, "    closure-template\n");
			continue;
		    }
		    args = AvARRAY(argav);
		    levelm = levels = levelref = levelas = 0;
		    levela = sizeof(SV*) * (AvMAX(argav) + 1);
		    if (AvREAL(argav)) {
			for (j = 0; j < AvFILL(argav); j++) {
			    if (SvROK(args[j])) {
				PerlIO_printf(Perl_debug_log, "     ref in args!\n");
				levelref++;
			    }
			    /* else if (SvPOK(args[j]) && SvPVX(args[j])) { */
			    else if (SvTYPE(args[j]) >= SVt_PV && SvLEN(args[j])) {
				levelas += SvLEN(args[j])/SvREFCNT(args[j]);
			    }
			}
		    }
		    for (j = 1; j < AvFILL((AV*)svp[1]); j++) {	/* Vars. */
			if (SvROK(pad[j])) {
			    levelref++;
			    do_sv_dump(0, Perl_debug_log, pad[j], 0, 4, 0, 0);
			    dumpit = 1;
			}
			/* else if (SvPOK(pad[j]) && SvPVX(pad[j])) { */
			else if (SvTYPE(pad[j]) >= SVt_PVAV) {
			    if (!SvPADMY(pad[j])) {
				levelref++;
				do_sv_dump(0, Perl_debug_log, pad[j], 0, 4, 0, 0);
				dumpit = 1;
			    }
			}
			else if (SvTYPE(pad[j]) >= SVt_PV && SvLEN(pad[j])) {
			    levels++;
			    levelm += SvLEN(pad[j])/SvREFCNT(pad[j]);
				/* Dump(pad[j],4); */
			}
		    }
		    PerlIO_printf(Perl_debug_log, "    level %i: refs: %i, strings: %i in %i,\targsarray: %i, argsstrings: %i\n", 
			    i, levelref, levelm, levels, levela, levelas);
		    totm += levelm;
		    tota += levela;
		    totas += levelas;
		    tots += levels;
		    totref += levelref;
		    if (dumpit)
			do_sv_dump(0, Perl_debug_log, (SV*)cv, 0, 2, 0, 0);
		}
		if (AvFILL(padlist) > 1) {
		    PerlIO_printf(Perl_debug_log, "  total: refs: %i, strings: %i in %i,\targsarrays: %i, argsstrings: %i\n", 
			    totref, totm, tots, tota, totas);
		}
		tref += totref;
		tm += totm;
		ts += tots;
		ta += tota;
		tas += totas;
	    }
	}
    }
    PerlIO_printf(Perl_debug_log, "total: refs: %i, strings: %i in %i\targsarray: %i, argsstrings: %i\n", tref, tm, ts, ta, tas);

    return ret;
#endif /* !PURIFY */
}

#if defined(PERL_DEBUGGING_MSTATS) || defined(DEBUGGING_MSTATS) \
	|| (defined(MYMALLOC) && !defined(PLAIN_MALLOC))
#   define mstat(str) dump_mstats(str)
#else
#   define mstat(str) \
	PerlIO_printf(Perl_debug_log, "%s: perl not compiled with DEBUGGING_MSTATS\n",str);
#endif

#if defined(PERL_DEBUGGING_MSTATS) || defined(DEBUGGING_MSTATS) \
	|| (defined(MYMALLOC) && !defined(PLAIN_MALLOC))

/* Very coarse overestimate, 2-per-power-of-2, one more to determine NBUCKETS. */
#  define _NBUCKETS (2*8*IVSIZE+1)

struct mstats_buffer 
{
    perl_mstats_t buffer;
    UV buf[_NBUCKETS*4];
};

void
_fill_mstats(struct mstats_buffer *b, int level)
{
    dTHX;
    b->buffer.nfree  = b->buf;
    b->buffer.ntotal = b->buf + _NBUCKETS;
    b->buffer.bucket_mem_size = b->buf + 2*_NBUCKETS;
    b->buffer.bucket_available_size = b->buf + 3*_NBUCKETS;
    Zero(b->buf, (level ? 4*_NBUCKETS: 2*_NBUCKETS), unsigned long);
    get_mstats(&(b->buffer), _NBUCKETS, level);
}

void
fill_mstats(SV *sv, int level)
{
    dTHX;
    int nbuckets;
    struct mstats_buffer buf;

    if (SvREADONLY(sv))
	croak("Cannot modify a readonly value");
    SvGROW(sv, sizeof(struct mstats_buffer)+1);
    _fill_mstats((struct mstats_buffer*)SvPVX(sv),level);
    SvCUR_set(sv, sizeof(struct mstats_buffer));
    *SvEND(sv) = '\0';
    SvPOK_only(sv);
}

void
_mstats_to_hv(HV *hv, struct mstats_buffer *b, int level)
{
    dTHX;
    SV **svp;
    int type;

    svp = hv_fetch(hv, "topbucket", 9, 1);
    sv_setiv(*svp, b->buffer.topbucket);

    svp = hv_fetch(hv, "topbucket_ev", 12, 1);
    sv_setiv(*svp, b->buffer.topbucket_ev);

    svp = hv_fetch(hv, "topbucket_odd", 13, 1);
    sv_setiv(*svp, b->buffer.topbucket_odd);

    svp = hv_fetch(hv, "totfree", 7, 1);
    sv_setiv(*svp, b->buffer.totfree);

    svp = hv_fetch(hv, "total", 5, 1);
    sv_setiv(*svp, b->buffer.total);

    svp = hv_fetch(hv, "total_chain", 11, 1);
    sv_setiv(*svp, b->buffer.total_chain);

    svp = hv_fetch(hv, "total_sbrk", 10, 1);
    sv_setiv(*svp, b->buffer.total_sbrk);

    svp = hv_fetch(hv, "sbrks", 5, 1);
    sv_setiv(*svp, b->buffer.sbrks);

    svp = hv_fetch(hv, "sbrk_good", 9, 1);
    sv_setiv(*svp, b->buffer.sbrk_good);

    svp = hv_fetch(hv, "sbrk_slack", 10, 1);
    sv_setiv(*svp, b->buffer.sbrk_slack);

    svp = hv_fetch(hv, "start_slack", 11, 1);
    sv_setiv(*svp, b->buffer.start_slack);

    svp = hv_fetch(hv, "sbrked_remains", 14, 1);
    sv_setiv(*svp, b->buffer.sbrked_remains);
    
    svp = hv_fetch(hv, "minbucket", 9, 1);
    sv_setiv(*svp, b->buffer.minbucket);
    
    svp = hv_fetch(hv, "nbuckets", 8, 1);
    sv_setiv(*svp, b->buffer.nbuckets);

    if (_NBUCKETS < b->buffer.nbuckets) 
	warn("FIXME: internal mstats buffer too short");
    
    for (type = 0; type < (level ? 4 : 2); type++) {
	UV *p, *p1;
	AV *av;
	int i;
	static const char *types[4] = { 
	    "free", "used", "mem_size", "available_size"    
	};

	svp = hv_fetch(hv, types[type], strlen(types[type]), 1);

	if (SvOK(*svp) && !(SvROK(*svp) && SvTYPE(SvRV(*svp)) == SVt_PVAV))
	    croak("Unexpected value for the key '%s' in the mstats hash", types[type]);
	if (!SvOK(*svp)) {
	    av = newAV();
	    SvUPGRADE(*svp, SVt_RV);
	    SvRV(*svp) = (SV*)av;
	    SvROK_on(*svp);
	} else
	    av = (AV*)SvRV(*svp);

	av_extend(av, b->buffer.nbuckets - 1);
	/* XXXX What is the official way to reduce the size of the array? */
	switch (type) {
	case 0:
	    p = b->buffer.nfree;
	    break;
	case 1:
	    p = b->buffer.ntotal;
	    p1 = b->buffer.nfree;
	    break;
	case 2:
	    p = b->buffer.bucket_mem_size;
	    break;
	case 3:
	    p = b->buffer.bucket_available_size;
	    break;
	}
	for (i = 0; i < b->buffer.nbuckets; i++) {
	    svp = av_fetch(av, i, 1);
	    if (type == 1)
		sv_setiv(*svp, p[i]-p1[i]);
	    else
		sv_setuv(*svp, p[i]);
	}
    }
}
void
mstats_fillhash(SV *sv, int level)
{
    struct mstats_buffer buf;

    if (!(SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVHV))
	croak("Not a hash reference");
    _fill_mstats(&buf, level);
    _mstats_to_hv((HV *)SvRV(sv), &buf, level);
}
void
mstats2hash(SV *sv, SV *rv, int level)
{
    if (!(SvROK(rv) && SvTYPE(SvRV(rv)) == SVt_PVHV))
	croak("Not a hash reference");
    if (!SvPOK(sv))
	croak("Undefined value when expecting mstats buffer");
    if (SvCUR(sv) != sizeof(struct mstats_buffer))
	croak("Wrong size for a value with a mstats buffer");
    _mstats_to_hv((HV *)SvRV(rv), (struct mstats_buffer*)SvPVX(sv), level);
}
#else	/* !( defined(PERL_DEBUGGING_MSTATS) || defined(DEBUGGING_MSTATS) \ ) */ 
void
fill_mstats(SV *sv, int level)
{
    croak("Cannot report mstats without Perl malloc");
}
void
mstats_fillhash(SV *sv, int level)
{
    croak("Cannot report mstats without Perl malloc");
}
void
mstats2hash(SV *sv, SV *rv, int level)
{
    croak("Cannot report mstats without Perl malloc");
}
#endif	/* defined(PERL_DEBUGGING_MSTATS) || defined(DEBUGGING_MSTATS)... */ 

#define _CvGV(cv)					\
	(SvROK(cv) && (SvTYPE(SvRV(cv))==SVt_PVCV)	\
	 ? SvREFCNT_inc(CvGV((CV*)SvRV(cv))) : &PL_sv_undef)

MODULE = Devel::Peek		PACKAGE = Devel::Peek

void
mstat(str="Devel::Peek::mstat: ")
char *str

void
fill_mstats(SV *sv, int level = 0)

void
mstats_fillhash(SV *sv, int level = 0)
    PROTOTYPE: \%;$

void
mstats2hash(SV *sv, SV *rv, int level = 0)
    PROTOTYPE: $\%;$

void
Dump(sv,lim=4)
SV *	sv
I32	lim
PPCODE:
{
    SV *pv_lim_sv = perl_get_sv("Devel::Peek::pv_limit", FALSE);
    STRLEN pv_lim = pv_lim_sv ? SvIV(pv_lim_sv) : 0;
    SV *dumpop = perl_get_sv("Devel::Peek::dump_ops", FALSE);
    I32 save_dumpindent = PL_dumpindent;
    PL_dumpindent = 2;
    do_sv_dump(0, Perl_debug_log, sv, 0, lim, dumpop && SvTRUE(dumpop), pv_lim);
    PL_dumpindent = save_dumpindent;
}

void
DumpArray(lim,...)
I32	lim
PPCODE:
{
    long i;
    SV *pv_lim_sv = perl_get_sv("Devel::Peek::pv_limit", FALSE);
    STRLEN pv_lim = pv_lim_sv ? SvIV(pv_lim_sv) : 0;
    SV *dumpop = perl_get_sv("Devel::Peek::dump_ops", FALSE);
    I32 save_dumpindent = PL_dumpindent;
    PL_dumpindent = 2;

    for (i=1; i<items; i++) {
	PerlIO_printf(Perl_debug_log, "Elt No. %ld  0x%"UVxf"\n", i - 1, PTR2UV(ST(i)));
	do_sv_dump(0, Perl_debug_log, ST(i), 0, lim, dumpop && SvTRUE(dumpop), pv_lim);
    }
    PL_dumpindent = save_dumpindent;
}

void
DumpProg()
PPCODE:
{
    warn("dumpindent is %d", (int)PL_dumpindent);
    if (PL_main_root)
	op_dump(PL_main_root);
}

I32
SvREFCNT(sv)
SV *	sv

# PPCODE needed since otherwise sv_2mortal is inserted that will kill the value.

SV *
SvREFCNT_inc(sv)
SV *	sv
PPCODE:
{
    RETVAL = SvREFCNT_inc(sv);
    PUSHs(RETVAL);
}

# PPCODE needed since by default it is void

void
SvREFCNT_dec(sv)
SV *	sv
PPCODE:
{
    SvREFCNT_dec(sv);
    PUSHs(sv);
}

SV *
DeadCode()
CODE:
    RETVAL = DeadCode(aTHX);
OUTPUT:
    RETVAL

MODULE = Devel::Peek		PACKAGE = Devel::Peek	PREFIX = _

SV *
_CvGV(cv)
    SV *cv
