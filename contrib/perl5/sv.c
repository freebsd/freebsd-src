/*    sv.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "I wonder what the Entish is for 'yes' and 'no'," he thought.
 */

#include "EXTERN.h"
#define PERL_IN_SV_C
#include "perl.h"

#define FCALL *f
#define SV_CHECK_THINKFIRST(sv) if (SvTHINKFIRST(sv)) sv_force_normal(sv)

static void do_report_used(pTHXo_ SV *sv);
static void do_clean_objs(pTHXo_ SV *sv);
#ifndef DISABLE_DESTRUCTOR_KLUDGE
static void do_clean_named_objs(pTHXo_ SV *sv);
#endif
static void do_clean_all(pTHXo_ SV *sv);

/*
 * "A time to plant, and a time to uproot what was planted..."
 */

#define plant_SV(p) \
    STMT_START {					\
	SvANY(p) = (void *)PL_sv_root;			\
	SvFLAGS(p) = SVTYPEMASK;			\
	PL_sv_root = (p);				\
	--PL_sv_count;					\
    } STMT_END

/* sv_mutex must be held while calling uproot_SV() */
#define uproot_SV(p) \
    STMT_START {					\
	(p) = PL_sv_root;				\
	PL_sv_root = (SV*)SvANY(p);			\
	++PL_sv_count;					\
    } STMT_END

#define new_SV(p) \
    STMT_START {					\
	LOCK_SV_MUTEX;					\
	if (PL_sv_root)					\
	    uproot_SV(p);				\
	else						\
	    (p) = more_sv();				\
	UNLOCK_SV_MUTEX;				\
	SvANY(p) = 0;					\
	SvREFCNT(p) = 1;				\
	SvFLAGS(p) = 0;					\
    } STMT_END

#ifdef DEBUGGING

#define del_SV(p) \
    STMT_START {					\
	LOCK_SV_MUTEX;					\
	if (PL_debug & 32768)				\
	    del_sv(p);					\
	else						\
	    plant_SV(p);				\
	UNLOCK_SV_MUTEX;				\
    } STMT_END

STATIC void
S_del_sv(pTHX_ SV *p)
{
    if (PL_debug & 32768) {
	SV* sva;
	SV* sv;
	SV* svend;
	int ok = 0;
	for (sva = PL_sv_arenaroot; sva; sva = (SV *) SvANY(sva)) {
	    sv = sva + 1;
	    svend = &sva[SvREFCNT(sva)];
	    if (p >= sv && p < svend)
		ok = 1;
	}
	if (!ok) {
	    if (ckWARN_d(WARN_INTERNAL))	
	        Perl_warner(aTHX_ WARN_INTERNAL,
			    "Attempt to free non-arena SV: 0x%"UVxf,
			    PTR2UV(p));
	    return;
	}
    }
    plant_SV(p);
}

#else /* ! DEBUGGING */

#define del_SV(p)   plant_SV(p)

#endif /* DEBUGGING */

void
Perl_sv_add_arena(pTHX_ char *ptr, U32 size, U32 flags)
{
    SV* sva = (SV*)ptr;
    register SV* sv;
    register SV* svend;
    Zero(ptr, size, char);

    /* The first SV in an arena isn't an SV. */
    SvANY(sva) = (void *) PL_sv_arenaroot;		/* ptr to next arena */
    SvREFCNT(sva) = size / sizeof(SV);		/* number of SV slots */
    SvFLAGS(sva) = flags;			/* FAKE if not to be freed */

    PL_sv_arenaroot = sva;
    PL_sv_root = sva + 1;

    svend = &sva[SvREFCNT(sva) - 1];
    sv = sva + 1;
    while (sv < svend) {
	SvANY(sv) = (void *)(SV*)(sv + 1);
	SvFLAGS(sv) = SVTYPEMASK;
	sv++;
    }
    SvANY(sv) = 0;
    SvFLAGS(sv) = SVTYPEMASK;
}

/* sv_mutex must be held while calling more_sv() */
STATIC SV*
S_more_sv(pTHX)
{
    register SV* sv;

    if (PL_nice_chunk) {
	sv_add_arena(PL_nice_chunk, PL_nice_chunk_size, 0);
	PL_nice_chunk = Nullch;
    }
    else {
	char *chunk;                /* must use New here to match call to */
	New(704,chunk,1008,char);   /* Safefree() in sv_free_arenas()     */
	sv_add_arena(chunk, 1008, 0);
    }
    uproot_SV(sv);
    return sv;
}

STATIC I32
S_visit(pTHX_ SVFUNC_t f)
{
    SV* sva;
    SV* sv;
    register SV* svend;
    I32 visited = 0;

    for (sva = PL_sv_arenaroot; sva; sva = (SV*)SvANY(sva)) {
	svend = &sva[SvREFCNT(sva)];
	for (sv = sva + 1; sv < svend; ++sv) {
	    if (SvTYPE(sv) != SVTYPEMASK && SvREFCNT(sv)) {
		(FCALL)(aTHXo_ sv);
		++visited;
	    }
	}
    }
    return visited;
}

void
Perl_sv_report_used(pTHX)
{
    visit(do_report_used);
}

void
Perl_sv_clean_objs(pTHX)
{
    PL_in_clean_objs = TRUE;
    visit(do_clean_objs);
#ifndef DISABLE_DESTRUCTOR_KLUDGE
    /* some barnacles may yet remain, clinging to typeglobs */
    visit(do_clean_named_objs);
#endif
    PL_in_clean_objs = FALSE;
}

I32
Perl_sv_clean_all(pTHX)
{
    I32 cleaned;
    PL_in_clean_all = TRUE;
    cleaned = visit(do_clean_all);
    PL_in_clean_all = FALSE;
    return cleaned;
}

void
Perl_sv_free_arenas(pTHX)
{
    SV* sva;
    SV* svanext;
    XPV *arena, *arenanext;

    /* Free arenas here, but be careful about fake ones.  (We assume
       contiguity of the fake ones with the corresponding real ones.) */

    for (sva = PL_sv_arenaroot; sva; sva = svanext) {
	svanext = (SV*) SvANY(sva);
	while (svanext && SvFAKE(svanext))
	    svanext = (SV*) SvANY(svanext);

	if (!SvFAKE(sva))
	    Safefree((void *)sva);
    }

    for (arena = PL_xiv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xiv_arenaroot = 0;

    for (arena = PL_xnv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xnv_arenaroot = 0;

    for (arena = PL_xrv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xrv_arenaroot = 0;

    for (arena = PL_xpv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpv_arenaroot = 0;

    for (arena = (XPV*)PL_xpviv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpviv_arenaroot = 0;

    for (arena = (XPV*)PL_xpvnv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpvnv_arenaroot = 0;

    for (arena = (XPV*)PL_xpvcv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpvcv_arenaroot = 0;

    for (arena = (XPV*)PL_xpvav_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpvav_arenaroot = 0;

    for (arena = (XPV*)PL_xpvhv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpvhv_arenaroot = 0;

    for (arena = (XPV*)PL_xpvmg_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpvmg_arenaroot = 0;

    for (arena = (XPV*)PL_xpvlv_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpvlv_arenaroot = 0;

    for (arena = (XPV*)PL_xpvbm_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_xpvbm_arenaroot = 0;

    for (arena = (XPV*)PL_he_arenaroot; arena; arena = arenanext) {
	arenanext = (XPV*)arena->xpv_pv;
	Safefree(arena);
    }
    PL_he_arenaroot = 0;

    if (PL_nice_chunk)
	Safefree(PL_nice_chunk);
    PL_nice_chunk = Nullch;
    PL_nice_chunk_size = 0;
    PL_sv_arenaroot = 0;
    PL_sv_root = 0;
}

void
Perl_report_uninit(pTHX)
{
    if (PL_op)
	Perl_warner(aTHX_ WARN_UNINITIALIZED, PL_warn_uninit,
		    " in ", PL_op_desc[PL_op->op_type]);
    else
	Perl_warner(aTHX_ WARN_UNINITIALIZED, PL_warn_uninit, "", "");
}

STATIC XPVIV*
S_new_xiv(pTHX)
{
    IV* xiv;
    LOCK_SV_MUTEX;
    if (!PL_xiv_root)
	more_xiv();
    xiv = PL_xiv_root;
    /*
     * See comment in more_xiv() -- RAM.
     */
    PL_xiv_root = *(IV**)xiv;
    UNLOCK_SV_MUTEX;
    return (XPVIV*)((char*)xiv - STRUCT_OFFSET(XPVIV, xiv_iv));
}

STATIC void
S_del_xiv(pTHX_ XPVIV *p)
{
    IV* xiv = (IV*)((char*)(p) + STRUCT_OFFSET(XPVIV, xiv_iv));
    LOCK_SV_MUTEX;
    *(IV**)xiv = PL_xiv_root;
    PL_xiv_root = xiv;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xiv(pTHX)
{
    register IV* xiv;
    register IV* xivend;
    XPV* ptr;
    New(705, ptr, 1008/sizeof(XPV), XPV);
    ptr->xpv_pv = (char*)PL_xiv_arenaroot;		/* linked list of xiv arenas */
    PL_xiv_arenaroot = ptr;			/* to keep Purify happy */

    xiv = (IV*) ptr;
    xivend = &xiv[1008 / sizeof(IV) - 1];
    xiv += (sizeof(XPV) - 1) / sizeof(IV) + 1;   /* fudge by size of XPV */
    PL_xiv_root = xiv;
    while (xiv < xivend) {
	*(IV**)xiv = (IV *)(xiv + 1);
	xiv++;
    }
    *(IV**)xiv = 0;
}

STATIC XPVNV*
S_new_xnv(pTHX)
{
    NV* xnv;
    LOCK_SV_MUTEX;
    if (!PL_xnv_root)
	more_xnv();
    xnv = PL_xnv_root;
    PL_xnv_root = *(NV**)xnv;
    UNLOCK_SV_MUTEX;
    return (XPVNV*)((char*)xnv - STRUCT_OFFSET(XPVNV, xnv_nv));
}

STATIC void
S_del_xnv(pTHX_ XPVNV *p)
{
    NV* xnv = (NV*)((char*)(p) + STRUCT_OFFSET(XPVNV, xnv_nv));
    LOCK_SV_MUTEX;
    *(NV**)xnv = PL_xnv_root;
    PL_xnv_root = xnv;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xnv(pTHX)
{
    register NV* xnv;
    register NV* xnvend;
    XPV *ptr;
    New(711, ptr, 1008/sizeof(XPV), XPV);
    ptr->xpv_pv = (char*)PL_xnv_arenaroot;
    PL_xnv_arenaroot = ptr;

    xnv = (NV*) ptr;
    xnvend = &xnv[1008 / sizeof(NV) - 1];
    xnv += (sizeof(XPVIV) - 1) / sizeof(NV) + 1; /* fudge by sizeof XPVIV */
    PL_xnv_root = xnv;
    while (xnv < xnvend) {
	*(NV**)xnv = (NV*)(xnv + 1);
	xnv++;
    }
    *(NV**)xnv = 0;
}

STATIC XRV*
S_new_xrv(pTHX)
{
    XRV* xrv;
    LOCK_SV_MUTEX;
    if (!PL_xrv_root)
	more_xrv();
    xrv = PL_xrv_root;
    PL_xrv_root = (XRV*)xrv->xrv_rv;
    UNLOCK_SV_MUTEX;
    return xrv;
}

STATIC void
S_del_xrv(pTHX_ XRV *p)
{
    LOCK_SV_MUTEX;
    p->xrv_rv = (SV*)PL_xrv_root;
    PL_xrv_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xrv(pTHX)
{
    register XRV* xrv;
    register XRV* xrvend;
    XPV *ptr;
    New(712, ptr, 1008/sizeof(XPV), XPV);
    ptr->xpv_pv = (char*)PL_xrv_arenaroot;
    PL_xrv_arenaroot = ptr;

    xrv = (XRV*) ptr;
    xrvend = &xrv[1008 / sizeof(XRV) - 1];
    xrv += (sizeof(XPV) - 1) / sizeof(XRV) + 1;
    PL_xrv_root = xrv;
    while (xrv < xrvend) {
	xrv->xrv_rv = (SV*)(xrv + 1);
	xrv++;
    }
    xrv->xrv_rv = 0;
}

STATIC XPV*
S_new_xpv(pTHX)
{
    XPV* xpv;
    LOCK_SV_MUTEX;
    if (!PL_xpv_root)
	more_xpv();
    xpv = PL_xpv_root;
    PL_xpv_root = (XPV*)xpv->xpv_pv;
    UNLOCK_SV_MUTEX;
    return xpv;
}

STATIC void
S_del_xpv(pTHX_ XPV *p)
{
    LOCK_SV_MUTEX;
    p->xpv_pv = (char*)PL_xpv_root;
    PL_xpv_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpv(pTHX)
{
    register XPV* xpv;
    register XPV* xpvend;
    New(713, xpv, 1008/sizeof(XPV), XPV);
    xpv->xpv_pv = (char*)PL_xpv_arenaroot;
    PL_xpv_arenaroot = xpv;

    xpvend = &xpv[1008 / sizeof(XPV) - 1];
    PL_xpv_root = ++xpv;
    while (xpv < xpvend) {
	xpv->xpv_pv = (char*)(xpv + 1);
	xpv++;
    }
    xpv->xpv_pv = 0;
}

STATIC XPVIV*
S_new_xpviv(pTHX)
{
    XPVIV* xpviv;
    LOCK_SV_MUTEX;
    if (!PL_xpviv_root)
	more_xpviv();
    xpviv = PL_xpviv_root;
    PL_xpviv_root = (XPVIV*)xpviv->xpv_pv;
    UNLOCK_SV_MUTEX;
    return xpviv;
}

STATIC void
S_del_xpviv(pTHX_ XPVIV *p)
{
    LOCK_SV_MUTEX;
    p->xpv_pv = (char*)PL_xpviv_root;
    PL_xpviv_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpviv(pTHX)
{
    register XPVIV* xpviv;
    register XPVIV* xpvivend;
    New(714, xpviv, 1008/sizeof(XPVIV), XPVIV);
    xpviv->xpv_pv = (char*)PL_xpviv_arenaroot;
    PL_xpviv_arenaroot = xpviv;

    xpvivend = &xpviv[1008 / sizeof(XPVIV) - 1];
    PL_xpviv_root = ++xpviv;
    while (xpviv < xpvivend) {
	xpviv->xpv_pv = (char*)(xpviv + 1);
	xpviv++;
    }
    xpviv->xpv_pv = 0;
}

STATIC XPVNV*
S_new_xpvnv(pTHX)
{
    XPVNV* xpvnv;
    LOCK_SV_MUTEX;
    if (!PL_xpvnv_root)
	more_xpvnv();
    xpvnv = PL_xpvnv_root;
    PL_xpvnv_root = (XPVNV*)xpvnv->xpv_pv;
    UNLOCK_SV_MUTEX;
    return xpvnv;
}

STATIC void
S_del_xpvnv(pTHX_ XPVNV *p)
{
    LOCK_SV_MUTEX;
    p->xpv_pv = (char*)PL_xpvnv_root;
    PL_xpvnv_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpvnv(pTHX)
{
    register XPVNV* xpvnv;
    register XPVNV* xpvnvend;
    New(715, xpvnv, 1008/sizeof(XPVNV), XPVNV);
    xpvnv->xpv_pv = (char*)PL_xpvnv_arenaroot;
    PL_xpvnv_arenaroot = xpvnv;

    xpvnvend = &xpvnv[1008 / sizeof(XPVNV) - 1];
    PL_xpvnv_root = ++xpvnv;
    while (xpvnv < xpvnvend) {
	xpvnv->xpv_pv = (char*)(xpvnv + 1);
	xpvnv++;
    }
    xpvnv->xpv_pv = 0;
}

STATIC XPVCV*
S_new_xpvcv(pTHX)
{
    XPVCV* xpvcv;
    LOCK_SV_MUTEX;
    if (!PL_xpvcv_root)
	more_xpvcv();
    xpvcv = PL_xpvcv_root;
    PL_xpvcv_root = (XPVCV*)xpvcv->xpv_pv;
    UNLOCK_SV_MUTEX;
    return xpvcv;
}

STATIC void
S_del_xpvcv(pTHX_ XPVCV *p)
{
    LOCK_SV_MUTEX;
    p->xpv_pv = (char*)PL_xpvcv_root;
    PL_xpvcv_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpvcv(pTHX)
{
    register XPVCV* xpvcv;
    register XPVCV* xpvcvend;
    New(716, xpvcv, 1008/sizeof(XPVCV), XPVCV);
    xpvcv->xpv_pv = (char*)PL_xpvcv_arenaroot;
    PL_xpvcv_arenaroot = xpvcv;

    xpvcvend = &xpvcv[1008 / sizeof(XPVCV) - 1];
    PL_xpvcv_root = ++xpvcv;
    while (xpvcv < xpvcvend) {
	xpvcv->xpv_pv = (char*)(xpvcv + 1);
	xpvcv++;
    }
    xpvcv->xpv_pv = 0;
}

STATIC XPVAV*
S_new_xpvav(pTHX)
{
    XPVAV* xpvav;
    LOCK_SV_MUTEX;
    if (!PL_xpvav_root)
	more_xpvav();
    xpvav = PL_xpvav_root;
    PL_xpvav_root = (XPVAV*)xpvav->xav_array;
    UNLOCK_SV_MUTEX;
    return xpvav;
}

STATIC void
S_del_xpvav(pTHX_ XPVAV *p)
{
    LOCK_SV_MUTEX;
    p->xav_array = (char*)PL_xpvav_root;
    PL_xpvav_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpvav(pTHX)
{
    register XPVAV* xpvav;
    register XPVAV* xpvavend;
    New(717, xpvav, 1008/sizeof(XPVAV), XPVAV);
    xpvav->xav_array = (char*)PL_xpvav_arenaroot;
    PL_xpvav_arenaroot = xpvav;

    xpvavend = &xpvav[1008 / sizeof(XPVAV) - 1];
    PL_xpvav_root = ++xpvav;
    while (xpvav < xpvavend) {
	xpvav->xav_array = (char*)(xpvav + 1);
	xpvav++;
    }
    xpvav->xav_array = 0;
}

STATIC XPVHV*
S_new_xpvhv(pTHX)
{
    XPVHV* xpvhv;
    LOCK_SV_MUTEX;
    if (!PL_xpvhv_root)
	more_xpvhv();
    xpvhv = PL_xpvhv_root;
    PL_xpvhv_root = (XPVHV*)xpvhv->xhv_array;
    UNLOCK_SV_MUTEX;
    return xpvhv;
}

STATIC void
S_del_xpvhv(pTHX_ XPVHV *p)
{
    LOCK_SV_MUTEX;
    p->xhv_array = (char*)PL_xpvhv_root;
    PL_xpvhv_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpvhv(pTHX)
{
    register XPVHV* xpvhv;
    register XPVHV* xpvhvend;
    New(718, xpvhv, 1008/sizeof(XPVHV), XPVHV);
    xpvhv->xhv_array = (char*)PL_xpvhv_arenaroot;
    PL_xpvhv_arenaroot = xpvhv;

    xpvhvend = &xpvhv[1008 / sizeof(XPVHV) - 1];
    PL_xpvhv_root = ++xpvhv;
    while (xpvhv < xpvhvend) {
	xpvhv->xhv_array = (char*)(xpvhv + 1);
	xpvhv++;
    }
    xpvhv->xhv_array = 0;
}

STATIC XPVMG*
S_new_xpvmg(pTHX)
{
    XPVMG* xpvmg;
    LOCK_SV_MUTEX;
    if (!PL_xpvmg_root)
	more_xpvmg();
    xpvmg = PL_xpvmg_root;
    PL_xpvmg_root = (XPVMG*)xpvmg->xpv_pv;
    UNLOCK_SV_MUTEX;
    return xpvmg;
}

STATIC void
S_del_xpvmg(pTHX_ XPVMG *p)
{
    LOCK_SV_MUTEX;
    p->xpv_pv = (char*)PL_xpvmg_root;
    PL_xpvmg_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpvmg(pTHX)
{
    register XPVMG* xpvmg;
    register XPVMG* xpvmgend;
    New(719, xpvmg, 1008/sizeof(XPVMG), XPVMG);
    xpvmg->xpv_pv = (char*)PL_xpvmg_arenaroot;
    PL_xpvmg_arenaroot = xpvmg;

    xpvmgend = &xpvmg[1008 / sizeof(XPVMG) - 1];
    PL_xpvmg_root = ++xpvmg;
    while (xpvmg < xpvmgend) {
	xpvmg->xpv_pv = (char*)(xpvmg + 1);
	xpvmg++;
    }
    xpvmg->xpv_pv = 0;
}

STATIC XPVLV*
S_new_xpvlv(pTHX)
{
    XPVLV* xpvlv;
    LOCK_SV_MUTEX;
    if (!PL_xpvlv_root)
	more_xpvlv();
    xpvlv = PL_xpvlv_root;
    PL_xpvlv_root = (XPVLV*)xpvlv->xpv_pv;
    UNLOCK_SV_MUTEX;
    return xpvlv;
}

STATIC void
S_del_xpvlv(pTHX_ XPVLV *p)
{
    LOCK_SV_MUTEX;
    p->xpv_pv = (char*)PL_xpvlv_root;
    PL_xpvlv_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpvlv(pTHX)
{
    register XPVLV* xpvlv;
    register XPVLV* xpvlvend;
    New(720, xpvlv, 1008/sizeof(XPVLV), XPVLV);
    xpvlv->xpv_pv = (char*)PL_xpvlv_arenaroot;
    PL_xpvlv_arenaroot = xpvlv;

    xpvlvend = &xpvlv[1008 / sizeof(XPVLV) - 1];
    PL_xpvlv_root = ++xpvlv;
    while (xpvlv < xpvlvend) {
	xpvlv->xpv_pv = (char*)(xpvlv + 1);
	xpvlv++;
    }
    xpvlv->xpv_pv = 0;
}

STATIC XPVBM*
S_new_xpvbm(pTHX)
{
    XPVBM* xpvbm;
    LOCK_SV_MUTEX;
    if (!PL_xpvbm_root)
	more_xpvbm();
    xpvbm = PL_xpvbm_root;
    PL_xpvbm_root = (XPVBM*)xpvbm->xpv_pv;
    UNLOCK_SV_MUTEX;
    return xpvbm;
}

STATIC void
S_del_xpvbm(pTHX_ XPVBM *p)
{
    LOCK_SV_MUTEX;
    p->xpv_pv = (char*)PL_xpvbm_root;
    PL_xpvbm_root = p;
    UNLOCK_SV_MUTEX;
}

STATIC void
S_more_xpvbm(pTHX)
{
    register XPVBM* xpvbm;
    register XPVBM* xpvbmend;
    New(721, xpvbm, 1008/sizeof(XPVBM), XPVBM);
    xpvbm->xpv_pv = (char*)PL_xpvbm_arenaroot;
    PL_xpvbm_arenaroot = xpvbm;

    xpvbmend = &xpvbm[1008 / sizeof(XPVBM) - 1];
    PL_xpvbm_root = ++xpvbm;
    while (xpvbm < xpvbmend) {
	xpvbm->xpv_pv = (char*)(xpvbm + 1);
	xpvbm++;
    }
    xpvbm->xpv_pv = 0;
}

#ifdef LEAKTEST
#  define my_safemalloc(s)	(void*)safexmalloc(717,s)
#  define my_safefree(p)	safexfree((char*)p)
#else
#  define my_safemalloc(s)	(void*)safemalloc(s)
#  define my_safefree(p)	safefree((char*)p)
#endif

#ifdef PURIFY

#define new_XIV()	my_safemalloc(sizeof(XPVIV))
#define del_XIV(p)	my_safefree(p)

#define new_XNV()	my_safemalloc(sizeof(XPVNV))
#define del_XNV(p)	my_safefree(p)

#define new_XRV()	my_safemalloc(sizeof(XRV))
#define del_XRV(p)	my_safefree(p)

#define new_XPV()	my_safemalloc(sizeof(XPV))
#define del_XPV(p)	my_safefree(p)

#define new_XPVIV()	my_safemalloc(sizeof(XPVIV))
#define del_XPVIV(p)	my_safefree(p)

#define new_XPVNV()	my_safemalloc(sizeof(XPVNV))
#define del_XPVNV(p)	my_safefree(p)

#define new_XPVCV()	my_safemalloc(sizeof(XPVCV))
#define del_XPVCV(p)	my_safefree(p)

#define new_XPVAV()	my_safemalloc(sizeof(XPVAV))
#define del_XPVAV(p)	my_safefree(p)

#define new_XPVHV()	my_safemalloc(sizeof(XPVHV))
#define del_XPVHV(p)	my_safefree(p)
  
#define new_XPVMG()	my_safemalloc(sizeof(XPVMG))
#define del_XPVMG(p)	my_safefree(p)

#define new_XPVLV()	my_safemalloc(sizeof(XPVLV))
#define del_XPVLV(p)	my_safefree(p)

#define new_XPVBM()	my_safemalloc(sizeof(XPVBM))
#define del_XPVBM(p)	my_safefree(p)

#else /* !PURIFY */

#define new_XIV()	(void*)new_xiv()
#define del_XIV(p)	del_xiv((XPVIV*) p)

#define new_XNV()	(void*)new_xnv()
#define del_XNV(p)	del_xnv((XPVNV*) p)

#define new_XRV()	(void*)new_xrv()
#define del_XRV(p)	del_xrv((XRV*) p)

#define new_XPV()	(void*)new_xpv()
#define del_XPV(p)	del_xpv((XPV *)p)

#define new_XPVIV()	(void*)new_xpviv()
#define del_XPVIV(p)	del_xpviv((XPVIV *)p)

#define new_XPVNV()	(void*)new_xpvnv()
#define del_XPVNV(p)	del_xpvnv((XPVNV *)p)

#define new_XPVCV()	(void*)new_xpvcv()
#define del_XPVCV(p)	del_xpvcv((XPVCV *)p)

#define new_XPVAV()	(void*)new_xpvav()
#define del_XPVAV(p)	del_xpvav((XPVAV *)p)

#define new_XPVHV()	(void*)new_xpvhv()
#define del_XPVHV(p)	del_xpvhv((XPVHV *)p)
  
#define new_XPVMG()	(void*)new_xpvmg()
#define del_XPVMG(p)	del_xpvmg((XPVMG *)p)

#define new_XPVLV()	(void*)new_xpvlv()
#define del_XPVLV(p)	del_xpvlv((XPVLV *)p)

#define new_XPVBM()	(void*)new_xpvbm()
#define del_XPVBM(p)	del_xpvbm((XPVBM *)p)

#endif /* PURIFY */

#define new_XPVGV()	my_safemalloc(sizeof(XPVGV))
#define del_XPVGV(p)	my_safefree(p)
 
#define new_XPVFM()	my_safemalloc(sizeof(XPVFM))
#define del_XPVFM(p)	my_safefree(p)
  
#define new_XPVIO()	my_safemalloc(sizeof(XPVIO))
#define del_XPVIO(p)	my_safefree(p)

/*
=for apidoc sv_upgrade

Upgrade an SV to a more complex form.  Use C<SvUPGRADE>.  See
C<svtype>.

=cut
*/

bool
Perl_sv_upgrade(pTHX_ register SV *sv, U32 mt)
{
    char*	pv;
    U32		cur;
    U32		len;
    IV		iv;
    NV		nv;
    MAGIC*	magic;
    HV*		stash;

    if (SvTYPE(sv) == mt)
	return TRUE;

    if (mt < SVt_PVIV)
	(void)SvOOK_off(sv);

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	pv	= 0;
	cur	= 0;
	len	= 0;
	iv	= 0;
	nv	= 0.0;
	magic	= 0;
	stash	= 0;
	break;
    case SVt_IV:
	pv	= 0;
	cur	= 0;
	len	= 0;
	iv	= SvIVX(sv);
	nv	= (NV)SvIVX(sv);
	del_XIV(SvANY(sv));
	magic	= 0;
	stash	= 0;
	if (mt == SVt_NV)
	    mt = SVt_PVNV;
	else if (mt < SVt_PVIV)
	    mt = SVt_PVIV;
	break;
    case SVt_NV:
	pv	= 0;
	cur	= 0;
	len	= 0;
	nv	= SvNVX(sv);
	iv	= I_V(nv);
	magic	= 0;
	stash	= 0;
	del_XNV(SvANY(sv));
	SvANY(sv) = 0;
	if (mt < SVt_PVNV)
	    mt = SVt_PVNV;
	break;
    case SVt_RV:
	pv	= (char*)SvRV(sv);
	cur	= 0;
	len	= 0;
	iv	= PTR2IV(pv);
	nv	= PTR2NV(pv);
	del_XRV(SvANY(sv));
	magic	= 0;
	stash	= 0;
	break;
    case SVt_PV:
	pv	= SvPVX(sv);
	cur	= SvCUR(sv);
	len	= SvLEN(sv);
	iv	= 0;
	nv	= 0.0;
	magic	= 0;
	stash	= 0;
	del_XPV(SvANY(sv));
	if (mt <= SVt_IV)
	    mt = SVt_PVIV;
	else if (mt == SVt_NV)
	    mt = SVt_PVNV;
	break;
    case SVt_PVIV:
	pv	= SvPVX(sv);
	cur	= SvCUR(sv);
	len	= SvLEN(sv);
	iv	= SvIVX(sv);
	nv	= 0.0;
	magic	= 0;
	stash	= 0;
	del_XPVIV(SvANY(sv));
	break;
    case SVt_PVNV:
	pv	= SvPVX(sv);
	cur	= SvCUR(sv);
	len	= SvLEN(sv);
	iv	= SvIVX(sv);
	nv	= SvNVX(sv);
	magic	= 0;
	stash	= 0;
	del_XPVNV(SvANY(sv));
	break;
    case SVt_PVMG:
	pv	= SvPVX(sv);
	cur	= SvCUR(sv);
	len	= SvLEN(sv);
	iv	= SvIVX(sv);
	nv	= SvNVX(sv);
	magic	= SvMAGIC(sv);
	stash	= SvSTASH(sv);
	del_XPVMG(SvANY(sv));
	break;
    default:
	Perl_croak(aTHX_ "Can't upgrade that kind of scalar");
    }

    switch (mt) {
    case SVt_NULL:
	Perl_croak(aTHX_ "Can't upgrade to undef");
    case SVt_IV:
	SvANY(sv) = new_XIV();
	SvIVX(sv)	= iv;
	break;
    case SVt_NV:
	SvANY(sv) = new_XNV();
	SvNVX(sv)	= nv;
	break;
    case SVt_RV:
	SvANY(sv) = new_XRV();
	SvRV(sv) = (SV*)pv;
	break;
    case SVt_PV:
	SvANY(sv) = new_XPV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	break;
    case SVt_PVIV:
	SvANY(sv) = new_XPVIV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	if (SvNIOK(sv))
	    (void)SvIOK_on(sv);
	SvNOK_off(sv);
	break;
    case SVt_PVNV:
	SvANY(sv) = new_XPVNV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	break;
    case SVt_PVMG:
	SvANY(sv) = new_XPVMG();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	break;
    case SVt_PVLV:
	SvANY(sv) = new_XPVLV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	LvTARGOFF(sv)	= 0;
	LvTARGLEN(sv)	= 0;
	LvTARG(sv)	= 0;
	LvTYPE(sv)	= 0;
	break;
    case SVt_PVAV:
	SvANY(sv) = new_XPVAV();
	if (pv)
	    Safefree(pv);
	SvPVX(sv)	= 0;
	AvMAX(sv)	= -1;
	AvFILLp(sv)	= -1;
	SvIVX(sv)	= 0;
	SvNVX(sv)	= 0.0;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	AvALLOC(sv)	= 0;
	AvARYLEN(sv)	= 0;
	AvFLAGS(sv)	= 0;
	break;
    case SVt_PVHV:
	SvANY(sv) = new_XPVHV();
	if (pv)
	    Safefree(pv);
	SvPVX(sv)	= 0;
	HvFILL(sv)	= 0;
	HvMAX(sv)	= 0;
	HvKEYS(sv)	= 0;
	SvNVX(sv)	= 0.0;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	HvRITER(sv)	= 0;
	HvEITER(sv)	= 0;
	HvPMROOT(sv)	= 0;
	HvNAME(sv)	= 0;
	break;
    case SVt_PVCV:
	SvANY(sv) = new_XPVCV();
	Zero(SvANY(sv), 1, XPVCV);
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	break;
    case SVt_PVGV:
	SvANY(sv) = new_XPVGV();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	GvGP(sv)	= 0;
	GvNAME(sv)	= 0;
	GvNAMELEN(sv)	= 0;
	GvSTASH(sv)	= 0;
	GvFLAGS(sv)	= 0;
	break;
    case SVt_PVBM:
	SvANY(sv) = new_XPVBM();
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	BmRARE(sv)	= 0;
	BmUSEFUL(sv)	= 0;
	BmPREVIOUS(sv)	= 0;
	break;
    case SVt_PVFM:
	SvANY(sv) = new_XPVFM();
	Zero(SvANY(sv), 1, XPVFM);
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	break;
    case SVt_PVIO:
	SvANY(sv) = new_XPVIO();
	Zero(SvANY(sv), 1, XPVIO);
	SvPVX(sv)	= pv;
	SvCUR(sv)	= cur;
	SvLEN(sv)	= len;
	SvIVX(sv)	= iv;
	SvNVX(sv)	= nv;
	SvMAGIC(sv)	= magic;
	SvSTASH(sv)	= stash;
	IoPAGE_LEN(sv)	= 60;
	break;
    }
    SvFLAGS(sv) &= ~SVTYPEMASK;
    SvFLAGS(sv) |= mt;
    return TRUE;
}

int
Perl_sv_backoff(pTHX_ register SV *sv)
{
    assert(SvOOK(sv));
    if (SvIVX(sv)) {
	char *s = SvPVX(sv);
	SvLEN(sv) += SvIVX(sv);
	SvPVX(sv) -= SvIVX(sv);
	SvIV_set(sv, 0);
	Move(s, SvPVX(sv), SvCUR(sv)+1, char);
    }
    SvFLAGS(sv) &= ~SVf_OOK;
    return 0;
}

/*
=for apidoc sv_grow

Expands the character buffer in the SV.  This will use C<sv_unref> and will
upgrade the SV to C<SVt_PV>.  Returns a pointer to the character buffer.
Use C<SvGROW>.

=cut
*/

char *
Perl_sv_grow(pTHX_ register SV *sv, register STRLEN newlen)
{
    register char *s;

#ifdef HAS_64K_LIMIT
    if (newlen >= 0x10000) {
	PerlIO_printf(Perl_debug_log,
		      "Allocation too large: %"UVxf"\n", (UV)newlen);
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
    if (SvROK(sv))
	sv_unref(sv);
    if (SvTYPE(sv) < SVt_PV) {
	sv_upgrade(sv, SVt_PV);
	s = SvPVX(sv);
    }
    else if (SvOOK(sv)) {	/* pv is offset? */
	sv_backoff(sv);
	s = SvPVX(sv);
	if (newlen > SvLEN(sv))
	    newlen += 10 * (newlen - SvCUR(sv)); /* avoid copy each time */
#ifdef HAS_64K_LIMIT
	if (newlen >= 0x10000)
	    newlen = 0xFFFF;
#endif
    }
    else
	s = SvPVX(sv);
    if (newlen > SvLEN(sv)) {		/* need more room? */
	if (SvLEN(sv) && s) {
#if defined(MYMALLOC) && !defined(LEAKTEST)
	    STRLEN l = malloced_size((void*)SvPVX(sv));
	    if (newlen <= l) {
		SvLEN_set(sv, l);
		return s;
	    } else
#endif
	    Renew(s,newlen,char);
	}
        else
	    New(703,s,newlen,char);
	SvPV_set(sv, s);
        SvLEN_set(sv, newlen);
    }
    return s;
}

/*
=for apidoc sv_setiv

Copies an integer into the given SV.  Does not handle 'set' magic.  See
C<sv_setiv_mg>.

=cut
*/

void
Perl_sv_setiv(pTHX_ register SV *sv, IV i)
{
    SV_CHECK_THINKFIRST(sv);
    switch (SvTYPE(sv)) {
    case SVt_NULL:
	sv_upgrade(sv, SVt_IV);
	break;
    case SVt_NV:
	sv_upgrade(sv, SVt_PVNV);
	break;
    case SVt_RV:
    case SVt_PV:
	sv_upgrade(sv, SVt_PVIV);
	break;

    case SVt_PVGV:
    case SVt_PVAV:
    case SVt_PVHV:
    case SVt_PVCV:
    case SVt_PVFM:
    case SVt_PVIO:
	Perl_croak(aTHX_ "Can't coerce %s to integer in %s", sv_reftype(sv,0),
		   PL_op_desc[PL_op->op_type]);
    }
    (void)SvIOK_only(sv);			/* validate number */
    SvIVX(sv) = i;
    SvTAINT(sv);
}

/*
=for apidoc sv_setiv_mg

Like C<sv_setiv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_setiv_mg(pTHX_ register SV *sv, IV i)
{
    sv_setiv(sv,i);
    SvSETMAGIC(sv);
}

/*
=for apidoc sv_setuv

Copies an unsigned integer into the given SV.  Does not handle 'set' magic.
See C<sv_setuv_mg>.

=cut
*/

void
Perl_sv_setuv(pTHX_ register SV *sv, UV u)
{
    sv_setiv(sv, 0);
    SvIsUV_on(sv);
    SvUVX(sv) = u;
}

/*
=for apidoc sv_setuv_mg

Like C<sv_setuv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_setuv_mg(pTHX_ register SV *sv, UV u)
{
    sv_setuv(sv,u);
    SvSETMAGIC(sv);
}

/*
=for apidoc sv_setnv

Copies a double into the given SV.  Does not handle 'set' magic.  See
C<sv_setnv_mg>.

=cut
*/

void
Perl_sv_setnv(pTHX_ register SV *sv, NV num)
{
    SV_CHECK_THINKFIRST(sv);
    switch (SvTYPE(sv)) {
    case SVt_NULL:
    case SVt_IV:
	sv_upgrade(sv, SVt_NV);
	break;
    case SVt_RV:
    case SVt_PV:
    case SVt_PVIV:
	sv_upgrade(sv, SVt_PVNV);
	break;

    case SVt_PVGV:
    case SVt_PVAV:
    case SVt_PVHV:
    case SVt_PVCV:
    case SVt_PVFM:
    case SVt_PVIO:
	Perl_croak(aTHX_ "Can't coerce %s to number in %s", sv_reftype(sv,0),
		   PL_op_name[PL_op->op_type]);
    }
    SvNVX(sv) = num;
    (void)SvNOK_only(sv);			/* validate number */
    SvTAINT(sv);
}

/*
=for apidoc sv_setnv_mg

Like C<sv_setnv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_setnv_mg(pTHX_ register SV *sv, NV num)
{
    sv_setnv(sv,num);
    SvSETMAGIC(sv);
}

STATIC void
S_not_a_number(pTHX_ SV *sv)
{
    char tmpbuf[64];
    char *d = tmpbuf;
    char *s;
    char *limit = tmpbuf + sizeof(tmpbuf) - 8;
                  /* each *s can expand to 4 chars + "...\0",
                     i.e. need room for 8 chars */

    for (s = SvPVX(sv); *s && d < limit; s++) {
	int ch = *s & 0xFF;
	if (ch & 128 && !isPRINT_LC(ch)) {
	    *d++ = 'M';
	    *d++ = '-';
	    ch &= 127;
	}
	if (ch == '\n') {
	    *d++ = '\\';
	    *d++ = 'n';
	}
	else if (ch == '\r') {
	    *d++ = '\\';
	    *d++ = 'r';
	}
	else if (ch == '\f') {
	    *d++ = '\\';
	    *d++ = 'f';
	}
	else if (ch == '\\') {
	    *d++ = '\\';
	    *d++ = '\\';
	}
	else if (isPRINT_LC(ch))
	    *d++ = ch;
	else {
	    *d++ = '^';
	    *d++ = toCTRL(ch);
	}
    }
    if (*s) {
	*d++ = '.';
	*d++ = '.';
	*d++ = '.';
    }
    *d = '\0';

    if (PL_op)
	Perl_warner(aTHX_ WARN_NUMERIC,
		    "Argument \"%s\" isn't numeric in %s", tmpbuf,
		PL_op_desc[PL_op->op_type]);
    else
	Perl_warner(aTHX_ WARN_NUMERIC,
		    "Argument \"%s\" isn't numeric", tmpbuf);
}

/* the number can be converted to integer with atol() or atoll() */
#define IS_NUMBER_TO_INT_BY_ATOL 0x01
#define IS_NUMBER_TO_INT_BY_ATOF 0x02 /* atol() may be != atof() */
#define IS_NUMBER_NOT_IV	 0x04 /* (IV)atof() may be != atof() */
#define IS_NUMBER_NEG		 0x08 /* not good to cache UV */
#define IS_NUMBER_INFINITY	 0x10 /* this is big */

/* Actually, ISO C leaves conversion of UV to IV undefined, but
   until proven guilty, assume that things are not that bad... */

IV
Perl_sv_2iv(pTHX_ register SV *sv)
{
    if (!sv)
	return 0;
    if (SvGMAGICAL(sv)) {
	mg_get(sv);
	if (SvIOKp(sv))
	    return SvIVX(sv);
	if (SvNOKp(sv)) {
	    return I_V(SvNVX(sv));
	}
	if (SvPOKp(sv) && SvLEN(sv))
	    return asIV(sv);
	if (!SvROK(sv)) {
	    if (!(SvFLAGS(sv) & SVs_PADTMP)) {
		if (ckWARN(WARN_UNINITIALIZED) && !PL_localizing)
		    report_uninit();
	    }
	    return 0;
	}
    }
    if (SvTHINKFIRST(sv)) {
	if (SvROK(sv)) {
	  SV* tmpstr;
          if (SvAMAGIC(sv) && (tmpstr=AMG_CALLun(sv,numer)) &&
                  (SvRV(tmpstr) != SvRV(sv)))
	      return SvIV(tmpstr);
	  return PTR2IV(SvRV(sv));
	}
	if (SvREADONLY(sv) && !SvOK(sv)) {
	    if (ckWARN(WARN_UNINITIALIZED))
		report_uninit();
	    return 0;
	}
    }
    if (SvIOKp(sv)) {
	if (SvIsUV(sv)) {
	    return (IV)(SvUVX(sv));
	}
	else {
	    return SvIVX(sv);
	}
    }
    if (SvNOKp(sv)) {
	/* We can cache the IV/UV value even if it not good enough
	 * to reconstruct NV, since the conversion to PV will prefer
	 * NV over IV/UV.
	 */

	if (SvTYPE(sv) == SVt_NV)
	    sv_upgrade(sv, SVt_PVNV);

	(void)SvIOK_on(sv);
	if (SvNVX(sv) < (NV)IV_MAX + 0.5)
	    SvIVX(sv) = I_V(SvNVX(sv));
	else {
	    SvUVX(sv) = U_V(SvNVX(sv));
	    SvIsUV_on(sv);
	  ret_iv_max:
	    DEBUG_c(PerlIO_printf(Perl_debug_log, 
				  "0x%"UVxf" 2iv(%"UVuf" => %"IVdf") (as unsigned)\n",
				  PTR2UV(sv),
				  SvUVX(sv),
				  SvUVX(sv)));
	    return (IV)SvUVX(sv);
	}
    }
    else if (SvPOKp(sv) && SvLEN(sv)) {
	I32 numtype = looks_like_number(sv);

	/* We want to avoid a possible problem when we cache an IV which
	   may be later translated to an NV, and the resulting NV is not
	   the translation of the initial data.
	  
	   This means that if we cache such an IV, we need to cache the
	   NV as well.  Moreover, we trade speed for space, and do not
	   cache the NV if not needed.
	 */
	if (numtype & IS_NUMBER_NOT_IV) {
	    /* May be not an integer.  Need to cache NV if we cache IV
	     * - otherwise future conversion to NV will be wrong.  */
	    NV d;

	    d = Atof(SvPVX(sv));

	    if (SvTYPE(sv) < SVt_PVNV)
		sv_upgrade(sv, SVt_PVNV);
	    SvNVX(sv) = d;
	    (void)SvNOK_on(sv);
	    (void)SvIOK_on(sv);
#if defined(USE_LONG_DOUBLE)
	    DEBUG_c(PerlIO_printf(Perl_debug_log, "0x%"UVxf" 2nv(%" PERL_PRIgldbl ")\n",
				  PTR2UV(sv), SvNVX(sv)));
#else
	    DEBUG_c(PerlIO_printf(Perl_debug_log, "0x%"UVxf" 2nv(%g)\n",
				  PTR2UV(sv), SvNVX(sv)));
#endif
	    if (SvNVX(sv) < (NV)IV_MAX + 0.5)
		SvIVX(sv) = I_V(SvNVX(sv));
	    else {
		SvUVX(sv) = U_V(SvNVX(sv));
		SvIsUV_on(sv);
		goto ret_iv_max;
	    }
	}
	else {	/* The NV may be reconstructed from IV - safe to cache IV,
		   which may be calculated by atol(). */
	    if (SvTYPE(sv) < SVt_PVIV)
		sv_upgrade(sv, SVt_PVIV);
	    (void)SvIOK_on(sv);
	    SvIVX(sv) = Atol(SvPVX(sv));
	    if (! numtype && ckWARN(WARN_NUMERIC))
		not_a_number(sv);
	}
    }
    else  {
	if (ckWARN(WARN_UNINITIALIZED) && !PL_localizing && !(SvFLAGS(sv) & SVs_PADTMP))
	    report_uninit();
	if (SvTYPE(sv) < SVt_IV)
	    /* Typically the caller expects that sv_any is not NULL now.  */
	    sv_upgrade(sv, SVt_IV);
	return 0;
    }
    DEBUG_c(PerlIO_printf(Perl_debug_log, "0x%"UVxf" 2iv(%"IVdf")\n",
	PTR2UV(sv),SvIVX(sv)));
    return SvIsUV(sv) ? (IV)SvUVX(sv) : SvIVX(sv);
}

UV
Perl_sv_2uv(pTHX_ register SV *sv)
{
    if (!sv)
	return 0;
    if (SvGMAGICAL(sv)) {
	mg_get(sv);
	if (SvIOKp(sv))
	    return SvUVX(sv);
	if (SvNOKp(sv))
	    return U_V(SvNVX(sv));
	if (SvPOKp(sv) && SvLEN(sv))
	    return asUV(sv);
	if (!SvROK(sv)) {
	    if (!(SvFLAGS(sv) & SVs_PADTMP)) {
		if (ckWARN(WARN_UNINITIALIZED) && !PL_localizing)
		    report_uninit();
	    }
	    return 0;
	}
    }
    if (SvTHINKFIRST(sv)) {
	if (SvROK(sv)) {
	  SV* tmpstr;
          if (SvAMAGIC(sv) && (tmpstr=AMG_CALLun(sv,numer)) &&
                  (SvRV(tmpstr) != SvRV(sv)))
	      return SvUV(tmpstr);
	  return PTR2UV(SvRV(sv));
	}
	if (SvREADONLY(sv) && !SvOK(sv)) {
	    if (ckWARN(WARN_UNINITIALIZED))
		report_uninit();
	    return 0;
	}
    }
    if (SvIOKp(sv)) {
	if (SvIsUV(sv)) {
	    return SvUVX(sv);
	}
	else {
	    return (UV)SvIVX(sv);
	}
    }
    if (SvNOKp(sv)) {
	/* We can cache the IV/UV value even if it not good enough
	 * to reconstruct NV, since the conversion to PV will prefer
	 * NV over IV/UV.
	 */
	if (SvTYPE(sv) == SVt_NV)
	    sv_upgrade(sv, SVt_PVNV);
	(void)SvIOK_on(sv);
	if (SvNVX(sv) >= -0.5) {
	    SvIsUV_on(sv);
	    SvUVX(sv) = U_V(SvNVX(sv));
	}
	else {
	    SvIVX(sv) = I_V(SvNVX(sv));
	  ret_zero:
	    DEBUG_c(PerlIO_printf(Perl_debug_log, 
				  "0x%"UVxf" 2uv(%"IVdf" => %"IVdf") (as signed)\n",
				  PTR2UV(sv),
				  SvIVX(sv),
				  (IV)(UV)SvIVX(sv)));
	    return (UV)SvIVX(sv);
	}
    }
    else if (SvPOKp(sv) && SvLEN(sv)) {
	I32 numtype = looks_like_number(sv);

	/* We want to avoid a possible problem when we cache a UV which
	   may be later translated to an NV, and the resulting NV is not
	   the translation of the initial data.
	  
	   This means that if we cache such a UV, we need to cache the
	   NV as well.  Moreover, we trade speed for space, and do not
	   cache the NV if not needed.
	 */
	if (numtype & IS_NUMBER_NOT_IV) {
	    /* May be not an integer.  Need to cache NV if we cache IV
	     * - otherwise future conversion to NV will be wrong.  */
	    NV d;

	    d = Atof(SvPVX(sv));

	    if (SvTYPE(sv) < SVt_PVNV)
		sv_upgrade(sv, SVt_PVNV);
	    SvNVX(sv) = d;
	    (void)SvNOK_on(sv);
	    (void)SvIOK_on(sv);
#if defined(USE_LONG_DOUBLE)
	    DEBUG_c(PerlIO_printf(Perl_debug_log,
				  "0x%"UVxf" 2nv(%" PERL_PRIgldbl ")\n",
				  PTR2UV(sv), SvNVX(sv)));
#else
	    DEBUG_c(PerlIO_printf(Perl_debug_log,
				  "0x%"UVxf" 2nv(%g)\n",
				  PTR2UV(sv), SvNVX(sv)));
#endif
	    if (SvNVX(sv) < -0.5) {
		SvIVX(sv) = I_V(SvNVX(sv));
		goto ret_zero;
	    } else {
		SvUVX(sv) = U_V(SvNVX(sv));
		SvIsUV_on(sv);
	    }
	}
	else if (numtype & IS_NUMBER_NEG) {
	    /* The NV may be reconstructed from IV - safe to cache IV,
	       which may be calculated by atol(). */
	    if (SvTYPE(sv) == SVt_PV)
		sv_upgrade(sv, SVt_PVIV);
	    (void)SvIOK_on(sv);
	    SvIVX(sv) = (IV)Atol(SvPVX(sv));
	}
	else if (numtype) {		/* Non-negative */
	    /* The NV may be reconstructed from UV - safe to cache UV,
	       which may be calculated by strtoul()/atol. */
	    if (SvTYPE(sv) == SVt_PV)
		sv_upgrade(sv, SVt_PVIV);
	    (void)SvIOK_on(sv);
	    (void)SvIsUV_on(sv);
#ifdef HAS_STRTOUL
	    SvUVX(sv) = Strtoul(SvPVX(sv), Null(char**), 10);
#else			/* no atou(), but we know the number fits into IV... */
	    		/* The only problem may be if it is negative... */
	    SvUVX(sv) = (UV)Atol(SvPVX(sv));
#endif
	}
	else {				/* Not a number.  Cache 0. */
	    if (SvTYPE(sv) < SVt_PVIV)
		sv_upgrade(sv, SVt_PVIV);
	    (void)SvIOK_on(sv);
	    (void)SvIsUV_on(sv);
	    SvUVX(sv) = 0;		/* We assume that 0s have the
					   same bitmap in IV and UV. */
	    if (ckWARN(WARN_NUMERIC))
		not_a_number(sv);
	}
    }
    else  {
	if (!(SvFLAGS(sv) & SVs_PADTMP)) {
	    if (ckWARN(WARN_UNINITIALIZED) && !PL_localizing)
		report_uninit();
	}
	if (SvTYPE(sv) < SVt_IV)
	    /* Typically the caller expects that sv_any is not NULL now.  */
	    sv_upgrade(sv, SVt_IV);
	return 0;
    }

    DEBUG_c(PerlIO_printf(Perl_debug_log, "0x%"UVxf" 2uv(%"UVuf")\n",
			  PTR2UV(sv),SvUVX(sv)));
    return SvIsUV(sv) ? SvUVX(sv) : (UV)SvIVX(sv);
}

NV
Perl_sv_2nv(pTHX_ register SV *sv)
{
    if (!sv)
	return 0.0;
    if (SvGMAGICAL(sv)) {
	mg_get(sv);
	if (SvNOKp(sv))
	    return SvNVX(sv);
	if (SvPOKp(sv) && SvLEN(sv)) {
	    if (ckWARN(WARN_NUMERIC) && !SvIOKp(sv) && !looks_like_number(sv))
		not_a_number(sv);
	    return Atof(SvPVX(sv));
	}
	if (SvIOKp(sv)) {
	    if (SvIsUV(sv)) 
		return (NV)SvUVX(sv);
	    else
		return (NV)SvIVX(sv);
	}	
        if (!SvROK(sv)) {
	    if (!(SvFLAGS(sv) & SVs_PADTMP)) {
		if (ckWARN(WARN_UNINITIALIZED) && !PL_localizing)
		    report_uninit();
	    }
            return 0;
        }
    }
    if (SvTHINKFIRST(sv)) {
	if (SvROK(sv)) {
	  SV* tmpstr;
          if (SvAMAGIC(sv) && (tmpstr=AMG_CALLun(sv,numer)) &&
                  (SvRV(tmpstr) != SvRV(sv)))
	      return SvNV(tmpstr);
	  return PTR2NV(SvRV(sv));
	}
	if (SvREADONLY(sv) && !SvOK(sv)) {
	    if (ckWARN(WARN_UNINITIALIZED))
		report_uninit();
	    return 0.0;
	}
    }
    if (SvTYPE(sv) < SVt_NV) {
	if (SvTYPE(sv) == SVt_IV)
	    sv_upgrade(sv, SVt_PVNV);
	else
	    sv_upgrade(sv, SVt_NV);
#if defined(USE_LONG_DOUBLE)
	DEBUG_c({
	    STORE_NUMERIC_LOCAL_SET_STANDARD();
	    PerlIO_printf(Perl_debug_log,
			  "0x%"UVxf" num(%" PERL_PRIgldbl ")\n",
			  PTR2UV(sv), SvNVX(sv));
	    RESTORE_NUMERIC_LOCAL();
	});
#else
	DEBUG_c({
	    STORE_NUMERIC_LOCAL_SET_STANDARD();
	    PerlIO_printf(Perl_debug_log, "0x%"UVxf" num(%g)\n",
			  PTR2UV(sv), SvNVX(sv));
	    RESTORE_NUMERIC_LOCAL();
	});
#endif
    }
    else if (SvTYPE(sv) < SVt_PVNV)
	sv_upgrade(sv, SVt_PVNV);
    if (SvIOKp(sv) &&
	    (!SvPOKp(sv) || !strchr(SvPVX(sv),'.') || !looks_like_number(sv)))
    {
	SvNVX(sv) = SvIsUV(sv) ? (NV)SvUVX(sv) : (NV)SvIVX(sv);
    }
    else if (SvPOKp(sv) && SvLEN(sv)) {
	if (ckWARN(WARN_NUMERIC) && !SvIOKp(sv) && !looks_like_number(sv))
	    not_a_number(sv);
	SvNVX(sv) = Atof(SvPVX(sv));
    }
    else  {
	if (ckWARN(WARN_UNINITIALIZED) && !PL_localizing && !(SvFLAGS(sv) & SVs_PADTMP))
	    report_uninit();
	if (SvTYPE(sv) < SVt_NV)
	    /* Typically the caller expects that sv_any is not NULL now.  */
	    sv_upgrade(sv, SVt_NV);
	return 0.0;
    }
    SvNOK_on(sv);
#if defined(USE_LONG_DOUBLE)
    DEBUG_c({
	STORE_NUMERIC_LOCAL_SET_STANDARD();
	PerlIO_printf(Perl_debug_log, "0x%"UVxf" 2nv(%" PERL_PRIgldbl ")\n",
		      PTR2UV(sv), SvNVX(sv));
	RESTORE_NUMERIC_LOCAL();
    });
#else
    DEBUG_c({
	STORE_NUMERIC_LOCAL_SET_STANDARD();
	PerlIO_printf(Perl_debug_log, "0x%"UVxf" 1nv(%g)\n",
		      PTR2UV(sv), SvNVX(sv));
	RESTORE_NUMERIC_LOCAL();
    });
#endif
    return SvNVX(sv);
}

STATIC IV
S_asIV(pTHX_ SV *sv)
{
    I32 numtype = looks_like_number(sv);
    NV d;

    if (numtype & IS_NUMBER_TO_INT_BY_ATOL)
	return Atol(SvPVX(sv));
    if (!numtype) {
	if (ckWARN(WARN_NUMERIC))
	    not_a_number(sv);
    }
    d = Atof(SvPVX(sv));
    return I_V(d);
}

STATIC UV
S_asUV(pTHX_ SV *sv)
{
    I32 numtype = looks_like_number(sv);

#ifdef HAS_STRTOUL
    if (numtype & IS_NUMBER_TO_INT_BY_ATOL)
	return Strtoul(SvPVX(sv), Null(char**), 10);
#endif
    if (!numtype) {
	if (ckWARN(WARN_NUMERIC))
	    not_a_number(sv);
    }
    return U_V(Atof(SvPVX(sv)));
}

/*
 * Returns a combination of (advisory only - can get false negatives)
 * 	IS_NUMBER_TO_INT_BY_ATOL, IS_NUMBER_TO_INT_BY_ATOF, IS_NUMBER_NOT_IV,
 *	IS_NUMBER_NEG
 * 0 if does not look like number.
 *
 * In fact possible values are 0 and
 * IS_NUMBER_TO_INT_BY_ATOL				123
 * IS_NUMBER_TO_INT_BY_ATOL | IS_NUMBER_NOT_IV		123.1
 * IS_NUMBER_TO_INT_BY_ATOF | IS_NUMBER_NOT_IV		123e0
 * IS_NUMBER_INFINITY
 * with a possible addition of IS_NUMBER_NEG.
 */

/*
=for apidoc looks_like_number

Test if an the content of an SV looks like a number (or is a
number).

=cut
*/

I32
Perl_looks_like_number(pTHX_ SV *sv)
{
    register char *s;
    register char *send;
    register char *sbegin;
    register char *nbegin;
    I32 numtype = 0;
    I32 sawinf  = 0;
    STRLEN len;
#ifdef USE_LOCALE_NUMERIC
    bool specialradix = FALSE;
#endif

    if (SvPOK(sv)) {
	sbegin = SvPVX(sv); 
	len = SvCUR(sv);
    }
    else if (SvPOKp(sv))
	sbegin = SvPV(sv, len);
    else
	return 1;
    send = sbegin + len;

    s = sbegin;
    while (isSPACE(*s))
	s++;
    if (*s == '-') {
	s++;
	numtype = IS_NUMBER_NEG;
    }
    else if (*s == '+')
	s++;

    nbegin = s;
    /*
     * we return IS_NUMBER_TO_INT_BY_ATOL if the number can be converted
     * to _integer_ with atol() and IS_NUMBER_TO_INT_BY_ATOF if you need
     * (int)atof().
     */

    /* next must be digit or the radix separator or beginning of infinity */
    if (isDIGIT(*s)) {
        do {
	    s++;
        } while (isDIGIT(*s));

	if (s - nbegin >= TYPE_DIGITS(IV))	/* Cannot cache ato[ul]() */
	    numtype |= IS_NUMBER_TO_INT_BY_ATOF | IS_NUMBER_NOT_IV;
	else
	    numtype |= IS_NUMBER_TO_INT_BY_ATOL;

        if (*s == '.'
#ifdef USE_LOCALE_NUMERIC
	    || (specialradix = IS_NUMERIC_RADIX(s))
#endif
	    ) {
#ifdef USE_LOCALE_NUMERIC
	    if (specialradix)
		s += SvCUR(PL_numeric_radix_sv);
	    else
#endif
		s++;
	    numtype |= IS_NUMBER_NOT_IV;
            while (isDIGIT(*s))  /* optional digits after the radix */
                s++;
        }
    }
    else if (*s == '.'
#ifdef USE_LOCALE_NUMERIC
	    || (specialradix = IS_NUMERIC_RADIX(s))
#endif
	    ) {
#ifdef USE_LOCALE_NUMERIC
	if (specialradix)
	    s += SvCUR(PL_numeric_radix_sv);
	else
#endif
	    s++;
	numtype |= IS_NUMBER_TO_INT_BY_ATOL | IS_NUMBER_NOT_IV;
        /* no digits before the radix means we need digits after it */
        if (isDIGIT(*s)) {
	    do {
	        s++;
            } while (isDIGIT(*s));
        }
        else
	    return 0;
    }
    else if (*s == 'I' || *s == 'i') {
	s++; if (*s != 'N' && *s != 'n') return 0;
	s++; if (*s != 'F' && *s != 'f') return 0;
	s++; if (*s == 'I' || *s == 'i') {
	    s++; if (*s != 'N' && *s != 'n') return 0;
	    s++; if (*s != 'I' && *s != 'i') return 0;
	    s++; if (*s != 'T' && *s != 't') return 0;
	    s++; if (*s != 'Y' && *s != 'y') return 0;
	}
	sawinf = 1;
    }
    else
        return 0;

    if (sawinf)
	numtype = IS_NUMBER_INFINITY;
    else {
	/* we can have an optional exponent part */
	if (*s == 'e' || *s == 'E') {
	    numtype &= ~IS_NUMBER_NEG;
	    numtype |= IS_NUMBER_TO_INT_BY_ATOF | IS_NUMBER_NOT_IV;
	    s++;
	    if (*s == '+' || *s == '-')
		s++;
	    if (isDIGIT(*s)) {
		do {
		    s++;
		} while (isDIGIT(*s));
	    }
	    else
		return 0;
	}
    }
    while (isSPACE(*s))
	s++;
    if (s >= send)
	return numtype;
    if (len == 10 && memEQ(sbegin, "0 but true", 10))
	return IS_NUMBER_TO_INT_BY_ATOL;
    return 0;
}

char *
Perl_sv_2pv_nolen(pTHX_ register SV *sv)
{
    STRLEN n_a;
    return sv_2pv(sv, &n_a);
}

/* We assume that buf is at least TYPE_CHARS(UV) long. */
static char *
uiv_2buf(char *buf, IV iv, UV uv, int is_uv, char **peob)
{
    char *ptr = buf + TYPE_CHARS(UV);
    char *ebuf = ptr;
    int sign;

    if (is_uv)
	sign = 0;
    else if (iv >= 0) {
	uv = iv;
	sign = 0;
    } else {
	uv = -iv;
	sign = 1;
    }
    do {
	*--ptr = '0' + (uv % 10);
    } while (uv /= 10);
    if (sign)
	*--ptr = '-';
    *peob = ebuf;
    return ptr;
}

char *
Perl_sv_2pv(pTHX_ register SV *sv, STRLEN *lp)
{
    register char *s;
    int olderrno;
    SV *tsv;
    char tbuf[64];	/* Must fit sprintf/Gconvert of longest IV/NV */
    char *tmpbuf = tbuf;

    if (!sv) {
	*lp = 0;
	return "";
    }
    if (SvGMAGICAL(sv)) {
	mg_get(sv);
	if (SvPOKp(sv)) {
	    *lp = SvCUR(sv);
	    return SvPVX(sv);
	}
	if (SvIOKp(sv)) {
	    if (SvIsUV(sv)) 
		(void)sprintf(tmpbuf,"%"UVuf, (UV)SvUVX(sv));
	    else
		(void)sprintf(tmpbuf,"%"IVdf, (IV)SvIVX(sv));
	    tsv = Nullsv;
	    goto tokensave;
	}
	if (SvNOKp(sv)) {
	    Gconvert(SvNVX(sv), NV_DIG, 0, tmpbuf);
	    tsv = Nullsv;
	    goto tokensave;
	}
        if (!SvROK(sv)) {
	    if (!(SvFLAGS(sv) & SVs_PADTMP)) {
		if (ckWARN(WARN_UNINITIALIZED) && !PL_localizing)
		    report_uninit();
	    }
            *lp = 0;
            return "";
        }
    }
    if (SvTHINKFIRST(sv)) {
	if (SvROK(sv)) {
	    SV* tmpstr;
            if (SvAMAGIC(sv) && (tmpstr=AMG_CALLun(sv,string)) &&
                    (SvRV(tmpstr) != SvRV(sv)))
		return SvPV(tmpstr,*lp);
	    sv = (SV*)SvRV(sv);
	    if (!sv)
		s = "NULLREF";
	    else {
		MAGIC *mg;
		
		switch (SvTYPE(sv)) {
		case SVt_PVMG:
		    if ( ((SvFLAGS(sv) &
			   (SVs_OBJECT|SVf_OK|SVs_GMG|SVs_SMG|SVs_RMG)) 
			  == (SVs_OBJECT|SVs_RMG))
			 && strEQ(s=HvNAME(SvSTASH(sv)), "Regexp")
			 && (mg = mg_find(sv, 'r'))) {
			regexp *re = (regexp *)mg->mg_obj;

			if (!mg->mg_ptr) {
			    char *fptr = "msix";
			    char reflags[6];
			    char ch;
			    int left = 0;
			    int right = 4;
 			    U16 reganch = (re->reganch & PMf_COMPILETIME) >> 12;

 			    while((ch = *fptr++)) {
 				if(reganch & 1) {
 				    reflags[left++] = ch;
 				}
 				else {
 				    reflags[right--] = ch;
 				}
 				reganch >>= 1;
 			    }
 			    if(left != 4) {
 				reflags[left] = '-';
 				left = 5;
 			    }

			    mg->mg_len = re->prelen + 4 + left;
			    New(616, mg->mg_ptr, mg->mg_len + 1 + left, char);
			    Copy("(?", mg->mg_ptr, 2, char);
			    Copy(reflags, mg->mg_ptr+2, left, char);
			    Copy(":", mg->mg_ptr+left+2, 1, char);
			    Copy(re->precomp, mg->mg_ptr+3+left, re->prelen, char);
			    mg->mg_ptr[mg->mg_len - 1] = ')';
			    mg->mg_ptr[mg->mg_len] = 0;
			}
			PL_reginterp_cnt += re->program[0].next_off;
			*lp = mg->mg_len;
			return mg->mg_ptr;
		    }
					/* Fall through */
		case SVt_NULL:
		case SVt_IV:
		case SVt_NV:
		case SVt_RV:
		case SVt_PV:
		case SVt_PVIV:
		case SVt_PVNV:
		case SVt_PVBM:	s = "SCALAR";			break;
		case SVt_PVLV:	s = "LVALUE";			break;
		case SVt_PVAV:	s = "ARRAY";			break;
		case SVt_PVHV:	s = "HASH";			break;
		case SVt_PVCV:	s = "CODE";			break;
		case SVt_PVGV:	s = "GLOB";			break;
		case SVt_PVFM:	s = "FORMAT";			break;
		case SVt_PVIO:	s = "IO";			break;
		default:	s = "UNKNOWN";			break;
		}
		tsv = NEWSV(0,0);
		if (SvOBJECT(sv))
		    Perl_sv_setpvf(aTHX_ tsv, "%s=%s", HvNAME(SvSTASH(sv)), s);
		else
		    sv_setpv(tsv, s);
		Perl_sv_catpvf(aTHX_ tsv, "(0x%"UVxf")", PTR2UV(sv));
		goto tokensaveref;
	    }
	    *lp = strlen(s);
	    return s;
	}
	if (SvREADONLY(sv) && !SvOK(sv)) {
	    if (ckWARN(WARN_UNINITIALIZED))
		report_uninit();
	    *lp = 0;
	    return "";
	}
    }
    if (SvNOKp(sv)) {			/* See note in sv_2uv() */
	/* XXXX 64-bit?  IV may have better precision... */
	/* I tried changing this to be 64-bit-aware and
	 * the t/op/numconvert.t became very, very, angry.
	 * --jhi Sep 1999 */
	if (SvTYPE(sv) < SVt_PVNV)
	    sv_upgrade(sv, SVt_PVNV);
	/* The +20 is pure guesswork.  Configure test needed. --jhi */ 
	SvGROW(sv, NV_DIG + 20);
	s = SvPVX(sv);
	olderrno = errno;	/* some Xenix systems wipe out errno here */
#ifdef apollo
	if (SvNVX(sv) == 0.0)
	    (void)strcpy(s,"0");
	else
#endif /*apollo*/
	{
	    Gconvert(SvNVX(sv), NV_DIG, 0, s);
	}
	errno = olderrno;
#ifdef FIXNEGATIVEZERO
        if (*s == '-' && s[1] == '0' && !s[2])
	    strcpy(s,"0");
#endif
	while (*s) s++;
#ifdef hcx
	if (s[-1] == '.')
	    *--s = '\0';
#endif
    }
    else if (SvIOKp(sv)) {
	U32 isIOK = SvIOK(sv);
	U32 isUIOK = SvIsUV(sv);
	char buf[TYPE_CHARS(UV)];
	char *ebuf, *ptr;

	if (SvTYPE(sv) < SVt_PVIV)
	    sv_upgrade(sv, SVt_PVIV);
	if (isUIOK)
	    ptr = uiv_2buf(buf, 0, SvUVX(sv), 1, &ebuf);
	else
	    ptr = uiv_2buf(buf, SvIVX(sv), 0, 0, &ebuf);
	SvGROW(sv, ebuf - ptr + 1);	/* inlined from sv_setpvn */
	Move(ptr,SvPVX(sv),ebuf - ptr,char);
	SvCUR_set(sv, ebuf - ptr);
	s = SvEND(sv);
	*s = '\0';
	if (isIOK)
	    SvIOK_on(sv);
	else
	    SvIOKp_on(sv);
	if (isUIOK)
	    SvIsUV_on(sv);
	SvPOK_on(sv);
    }
    else {
	if (ckWARN(WARN_UNINITIALIZED)
	    && !PL_localizing && !(SvFLAGS(sv) & SVs_PADTMP))
	    report_uninit();
	*lp = 0;
	if (SvTYPE(sv) < SVt_PV)
	    /* Typically the caller expects that sv_any is not NULL now.  */
	    sv_upgrade(sv, SVt_PV);
	return "";
    }
    *lp = s - SvPVX(sv);
    SvCUR_set(sv, *lp);
    SvPOK_on(sv);
    DEBUG_c(PerlIO_printf(Perl_debug_log, "0x%"UVxf" 2pv(%s)\n",
			  PTR2UV(sv),SvPVX(sv)));
    return SvPVX(sv);

  tokensave:
    if (SvROK(sv)) {	/* XXX Skip this when sv_pvn_force calls */
	/* Sneaky stuff here */

      tokensaveref:
	if (!tsv)
	    tsv = newSVpv(tmpbuf, 0);
	sv_2mortal(tsv);
	*lp = SvCUR(tsv);
	return SvPVX(tsv);
    }
    else {
	STRLEN len;
	char *t;

	if (tsv) {
	    sv_2mortal(tsv);
	    t = SvPVX(tsv);
	    len = SvCUR(tsv);
	}
	else {
	    t = tmpbuf;
	    len = strlen(tmpbuf);
	}
#ifdef FIXNEGATIVEZERO
	if (len == 2 && t[0] == '-' && t[1] == '0') {
	    t = "0";
	    len = 1;
	}
#endif
	(void)SvUPGRADE(sv, SVt_PV);
	*lp = len;
	s = SvGROW(sv, len + 1);
	SvCUR_set(sv, len);
	(void)strcpy(s, t);
	SvPOKp_on(sv);
	return s;
    }
}

char *
Perl_sv_2pvbyte_nolen(pTHX_ register SV *sv)
{
    STRLEN n_a;
    return sv_2pvbyte(sv, &n_a);
}

char *
Perl_sv_2pvbyte(pTHX_ register SV *sv, STRLEN *lp)
{
    return sv_2pv(sv,lp);
}

char *
Perl_sv_2pvutf8_nolen(pTHX_ register SV *sv)
{
    STRLEN n_a;
    return sv_2pvutf8(sv, &n_a);
}

char *
Perl_sv_2pvutf8(pTHX_ register SV *sv, STRLEN *lp)
{
    sv_utf8_upgrade(sv);
    return SvPV(sv,*lp);
}
 
/* This function is only called on magical items */
bool
Perl_sv_2bool(pTHX_ register SV *sv)
{
    if (SvGMAGICAL(sv))
	mg_get(sv);

    if (!SvOK(sv))
	return 0;
    if (SvROK(sv)) {
	SV* tmpsv;
        if (SvAMAGIC(sv) && (tmpsv=AMG_CALLun(sv,bool_)) &&
                (SvRV(tmpsv) != SvRV(sv)))
	    return SvTRUE(tmpsv);
      return SvRV(sv) != 0;
    }
    if (SvPOKp(sv)) {
	register XPV* Xpvtmp;
	if ((Xpvtmp = (XPV*)SvANY(sv)) &&
		(*Xpvtmp->xpv_pv > '0' ||
		Xpvtmp->xpv_cur > 1 ||
		(Xpvtmp->xpv_cur && *Xpvtmp->xpv_pv != '0')))
	    return 1;
	else
	    return 0;
    }
    else {
	if (SvIOKp(sv))
	    return SvIVX(sv) != 0;
	else {
	    if (SvNOKp(sv))
		return SvNVX(sv) != 0.0;
	    else
		return FALSE;
	}
    }
}

/*
=for apidoc sv_utf8_upgrade

Convert the PV of an SV to its UTF8-encoded form.

=cut
*/

void
Perl_sv_utf8_upgrade(pTHX_ register SV *sv)
{
    char *s, *t, *e;
    int  hibit = 0;

    if (!sv || !SvPOK(sv) || SvUTF8(sv))
	return;

    /* This function could be much more efficient if we had a FLAG in SVs
     * to signal if there are any hibit chars in the PV.
     * Given that there isn't make loop fast as possible
     */
    s = SvPVX(sv);
    e = SvEND(sv);
    t = s;
    while (t < e) {
	if ((hibit = UTF8_IS_CONTINUED(*t++)))
	    break;
    }

    if (hibit) {
	STRLEN len;

	if (SvREADONLY(sv) && SvFAKE(sv)) {
	    sv_force_normal(sv);
	    s = SvPVX(sv);
	}
	len = SvCUR(sv) + 1; /* Plus the \0 */
	SvPVX(sv) = (char*)bytes_to_utf8((U8*)s, &len);
	SvCUR(sv) = len - 1;
	if (SvLEN(sv) != 0)
	    Safefree(s); /* No longer using what was there before. */
	SvLEN(sv) = len; /* No longer know the real size. */
	SvUTF8_on(sv);
    }
}

/*
=for apidoc sv_utf8_downgrade

Attempt to convert the PV of an SV from UTF8-encoded to byte encoding.
This may not be possible if the PV contains non-byte encoding characters;
if this is the case, either returns false or, if C<fail_ok> is not
true, croaks.

=cut
*/

bool
Perl_sv_utf8_downgrade(pTHX_ register SV* sv, bool fail_ok)
{
    if (SvPOK(sv) && SvUTF8(sv)) {
        if (SvCUR(sv)) {
	    char *s;
	    STRLEN len;

	    if (SvREADONLY(sv) && SvFAKE(sv))
		sv_force_normal(sv);
	    s = SvPV(sv, len);
	    if (!utf8_to_bytes((U8*)s, &len)) {
	        if (fail_ok)
		    return FALSE;
		else {
		    if (PL_op)
		        Perl_croak(aTHX_ "Wide character in %s",
				   PL_op_desc[PL_op->op_type]);
		    else
		        Perl_croak(aTHX_ "Wide character");
		}
	    }
	    SvCUR(sv) = len;
	}
	SvUTF8_off(sv);
    }

    return TRUE;
}

/*
=for apidoc sv_utf8_encode

Convert the PV of an SV to UTF8-encoded, but then turn off the C<SvUTF8>
flag so that it looks like bytes again. Nothing calls this. 

=cut
*/

void
Perl_sv_utf8_encode(pTHX_ register SV *sv)
{
    sv_utf8_upgrade(sv);
    SvUTF8_off(sv);
}

bool
Perl_sv_utf8_decode(pTHX_ register SV *sv)
{
    if (SvPOK(sv)) {
        char *c;
        char *e;
        bool has_utf = FALSE;
        if (!sv_utf8_downgrade(sv, TRUE))
	    return FALSE;

        /* it is actually just a matter of turning the utf8 flag on, but
         * we want to make sure everything inside is valid utf8 first.
         */
        c = SvPVX(sv);
	if (!is_utf8_string((U8*)c, SvCUR(sv)+1))
	    return FALSE;
        e = SvEND(sv);
        while (c < e) {
            if (UTF8_IS_CONTINUED(*c++)) {
		SvUTF8_on(sv);
		break;
	    }
        }
    }
    return TRUE;
}


/* Note: sv_setsv() should not be called with a source string that needs
 * to be reused, since it may destroy the source string if it is marked
 * as temporary.
 */

/*
=for apidoc sv_setsv

Copies the contents of the source SV C<ssv> into the destination SV C<dsv>.
The source SV may be destroyed if it is mortal.  Does not handle 'set'
magic.  See the macro forms C<SvSetSV>, C<SvSetSV_nosteal> and
C<sv_setsv_mg>.

=cut
*/

void
Perl_sv_setsv(pTHX_ SV *dstr, register SV *sstr)
{
    register U32 sflags;
    register int dtype;
    register int stype;

    if (sstr == dstr)
	return;
    SV_CHECK_THINKFIRST(dstr);
    if (!sstr)
	sstr = &PL_sv_undef;
    stype = SvTYPE(sstr);
    dtype = SvTYPE(dstr);

    SvAMAGIC_off(dstr);

    /* There's a lot of redundancy below but we're going for speed here */

    switch (stype) {
    case SVt_NULL:
      undef_sstr:
	if (dtype != SVt_PVGV) {
	    (void)SvOK_off(dstr);
	    return;
	}
	break;
    case SVt_IV:
	if (SvIOK(sstr)) {
	    switch (dtype) {
	    case SVt_NULL:
		sv_upgrade(dstr, SVt_IV);
		break;
	    case SVt_NV:
		sv_upgrade(dstr, SVt_PVNV);
		break;
	    case SVt_RV:
	    case SVt_PV:
		sv_upgrade(dstr, SVt_PVIV);
		break;
	    }
	    (void)SvIOK_only(dstr);
	    SvIVX(dstr) = SvIVX(sstr);
	    if (SvIsUV(sstr))
		SvIsUV_on(dstr);
	    if (SvTAINTED(sstr))
		SvTAINT(dstr);
	    return;
	}
	goto undef_sstr;

    case SVt_NV:
	if (SvNOK(sstr)) {
	    switch (dtype) {
	    case SVt_NULL:
	    case SVt_IV:
		sv_upgrade(dstr, SVt_NV);
		break;
	    case SVt_RV:
	    case SVt_PV:
	    case SVt_PVIV:
		sv_upgrade(dstr, SVt_PVNV);
		break;
	    }
	    SvNVX(dstr) = SvNVX(sstr);
	    (void)SvNOK_only(dstr);
	    if (SvTAINTED(sstr))
		SvTAINT(dstr);
	    return;
	}
	goto undef_sstr;

    case SVt_RV:
	if (dtype < SVt_RV)
	    sv_upgrade(dstr, SVt_RV);
	else if (dtype == SVt_PVGV &&
		 SvTYPE(SvRV(sstr)) == SVt_PVGV) {
	    sstr = SvRV(sstr);
	    if (sstr == dstr) {
		if (GvIMPORTED(dstr) != GVf_IMPORTED
		    && CopSTASH_ne(PL_curcop, GvSTASH(dstr)))
		{
		    GvIMPORTED_on(dstr);
		}
		GvMULTI_on(dstr);
		return;
	    }
	    goto glob_assign;
	}
	break;
    case SVt_PV:
    case SVt_PVFM:
	if (dtype < SVt_PV)
	    sv_upgrade(dstr, SVt_PV);
	break;
    case SVt_PVIV:
	if (dtype < SVt_PVIV)
	    sv_upgrade(dstr, SVt_PVIV);
	break;
    case SVt_PVNV:
	if (dtype < SVt_PVNV)
	    sv_upgrade(dstr, SVt_PVNV);
	break;
    case SVt_PVAV:
    case SVt_PVHV:
    case SVt_PVCV:
    case SVt_PVIO:
	if (PL_op)
	    Perl_croak(aTHX_ "Bizarre copy of %s in %s", sv_reftype(sstr, 0),
		PL_op_name[PL_op->op_type]);
	else
	    Perl_croak(aTHX_ "Bizarre copy of %s", sv_reftype(sstr, 0));
	break;

    case SVt_PVGV:
	if (dtype <= SVt_PVGV) {
  glob_assign:
	    if (dtype != SVt_PVGV) {
		char *name = GvNAME(sstr);
		STRLEN len = GvNAMELEN(sstr);
		sv_upgrade(dstr, SVt_PVGV);
		sv_magic(dstr, dstr, '*', Nullch, 0);
		GvSTASH(dstr) = (HV*)SvREFCNT_inc(GvSTASH(sstr));
		GvNAME(dstr) = savepvn(name, len);
		GvNAMELEN(dstr) = len;
		SvFAKE_on(dstr);	/* can coerce to non-glob */
	    }
	    /* ahem, death to those who redefine active sort subs */
	    else if (PL_curstackinfo->si_type == PERLSI_SORT
		     && GvCV(dstr) && PL_sortcop == CvSTART(GvCV(dstr)))
		Perl_croak(aTHX_ "Can't redefine active sort subroutine %s",
		      GvNAME(dstr));
	    (void)SvOK_off(dstr);
	    GvINTRO_off(dstr);		/* one-shot flag */
	    gp_free((GV*)dstr);
	    GvGP(dstr) = gp_ref(GvGP(sstr));
	    if (SvTAINTED(sstr))
		SvTAINT(dstr);
	    if (GvIMPORTED(dstr) != GVf_IMPORTED
		&& CopSTASH_ne(PL_curcop, GvSTASH(dstr)))
	    {
		GvIMPORTED_on(dstr);
	    }
	    GvMULTI_on(dstr);
	    return;
	}
	/* FALL THROUGH */

    default:
	if (SvGMAGICAL(sstr)) {
	    mg_get(sstr);
	    if (SvTYPE(sstr) != stype) {
		stype = SvTYPE(sstr);
		if (stype == SVt_PVGV && dtype <= SVt_PVGV)
		    goto glob_assign;
	    }
	}
	if (stype == SVt_PVLV)
	    (void)SvUPGRADE(dstr, SVt_PVNV);
	else
	    (void)SvUPGRADE(dstr, stype);
    }

    sflags = SvFLAGS(sstr);

    if (sflags & SVf_ROK) {
	if (dtype >= SVt_PV) {
	    if (dtype == SVt_PVGV) {
		SV *sref = SvREFCNT_inc(SvRV(sstr));
		SV *dref = 0;
		int intro = GvINTRO(dstr);

		if (intro) {
		    GP *gp;
		    gp_free((GV*)dstr);
		    GvINTRO_off(dstr);	/* one-shot flag */
		    Newz(602,gp, 1, GP);
		    GvGP(dstr) = gp_ref(gp);
		    GvSV(dstr) = NEWSV(72,0);
		    GvLINE(dstr) = CopLINE(PL_curcop);
		    GvEGV(dstr) = (GV*)dstr;
		}
		GvMULTI_on(dstr);
		switch (SvTYPE(sref)) {
		case SVt_PVAV:
		    if (intro)
			SAVESPTR(GvAV(dstr));
		    else
			dref = (SV*)GvAV(dstr);
		    GvAV(dstr) = (AV*)sref;
		    if (!GvIMPORTED_AV(dstr)
			&& CopSTASH_ne(PL_curcop, GvSTASH(dstr)))
		    {
			GvIMPORTED_AV_on(dstr);
		    }
		    break;
		case SVt_PVHV:
		    if (intro)
			SAVESPTR(GvHV(dstr));
		    else
			dref = (SV*)GvHV(dstr);
		    GvHV(dstr) = (HV*)sref;
		    if (!GvIMPORTED_HV(dstr)
			&& CopSTASH_ne(PL_curcop, GvSTASH(dstr)))
		    {
			GvIMPORTED_HV_on(dstr);
		    }
		    break;
		case SVt_PVCV:
		    if (intro) {
			if (GvCVGEN(dstr) && GvCV(dstr) != (CV*)sref) {
			    SvREFCNT_dec(GvCV(dstr));
			    GvCV(dstr) = Nullcv;
			    GvCVGEN(dstr) = 0; /* Switch off cacheness. */
			    PL_sub_generation++;
			}
			SAVESPTR(GvCV(dstr));
		    }
		    else
			dref = (SV*)GvCV(dstr);
		    if (GvCV(dstr) != (CV*)sref) {
			CV* cv = GvCV(dstr);
			if (cv) {
			    if (!GvCVGEN((GV*)dstr) &&
				(CvROOT(cv) || CvXSUB(cv)))
			    {
				SV *const_sv = cv_const_sv(cv);
				bool const_changed = TRUE; 
				if(const_sv)
				    const_changed = sv_cmp(const_sv, 
					   op_const_sv(CvSTART((CV*)sref), 
						       Nullcv));
				/* ahem, death to those who redefine
				 * active sort subs */
				if (PL_curstackinfo->si_type == PERLSI_SORT &&
				      PL_sortcop == CvSTART(cv))
				    Perl_croak(aTHX_ 
				    "Can't redefine active sort subroutine %s",
					  GvENAME((GV*)dstr));
				if ((const_changed || const_sv) && ckWARN(WARN_REDEFINE))
				    Perl_warner(aTHX_ WARN_REDEFINE, const_sv ? 
					     "Constant subroutine %s redefined"
					     : "Subroutine %s redefined", 
					     GvENAME((GV*)dstr));
			    }
			    cv_ckproto(cv, (GV*)dstr,
				       SvPOK(sref) ? SvPVX(sref) : Nullch);
			}
			GvCV(dstr) = (CV*)sref;
			GvCVGEN(dstr) = 0; /* Switch off cacheness. */
			GvASSUMECV_on(dstr);
			PL_sub_generation++;
		    }
		    if (!GvIMPORTED_CV(dstr)
			&& CopSTASH_ne(PL_curcop, GvSTASH(dstr)))
		    {
			GvIMPORTED_CV_on(dstr);
		    }
		    break;
		case SVt_PVIO:
		    if (intro)
			SAVESPTR(GvIOp(dstr));
		    else
			dref = (SV*)GvIOp(dstr);
		    GvIOp(dstr) = (IO*)sref;
		    break;
		default:
		    if (intro)
			SAVESPTR(GvSV(dstr));
		    else
			dref = (SV*)GvSV(dstr);
		    GvSV(dstr) = sref;
		    if (!GvIMPORTED_SV(dstr)
			&& CopSTASH_ne(PL_curcop, GvSTASH(dstr)))
		    {
			GvIMPORTED_SV_on(dstr);
		    }
		    break;
		}
		if (dref)
		    SvREFCNT_dec(dref);
		if (intro)
		    SAVEFREESV(sref);
		if (SvTAINTED(sstr))
		    SvTAINT(dstr);
		return;
	    }
	    if (SvPVX(dstr)) {
		(void)SvOOK_off(dstr);		/* backoff */
		if (SvLEN(dstr))
		    Safefree(SvPVX(dstr));
		SvLEN(dstr)=SvCUR(dstr)=0;
	    }
	}
	(void)SvOK_off(dstr);
	SvRV(dstr) = SvREFCNT_inc(SvRV(sstr));
	SvROK_on(dstr);
	if (sflags & SVp_NOK) {
	    SvNOK_on(dstr);
	    SvNVX(dstr) = SvNVX(sstr);
	}
	if (sflags & SVp_IOK) {
	    (void)SvIOK_on(dstr);
	    SvIVX(dstr) = SvIVX(sstr);
	    if (sflags & SVf_IVisUV)
		SvIsUV_on(dstr);
	}
	if (SvAMAGIC(sstr)) {
	    SvAMAGIC_on(dstr);
	}
    }
    else if (sflags & SVp_POK) {

	/*
	 * Check to see if we can just swipe the string.  If so, it's a
	 * possible small lose on short strings, but a big win on long ones.
	 * It might even be a win on short strings if SvPVX(dstr)
	 * has to be allocated and SvPVX(sstr) has to be freed.
	 */

	if (SvTEMP(sstr) &&		/* slated for free anyway? */
	    SvREFCNT(sstr) == 1 && 	/* and no other references to it? */
	    !(sflags & SVf_OOK)) 	/* and not involved in OOK hack? */
	{
	    if (SvPVX(dstr)) {		/* we know that dtype >= SVt_PV */
		if (SvOOK(dstr)) {
		    SvFLAGS(dstr) &= ~SVf_OOK;
		    Safefree(SvPVX(dstr) - SvIVX(dstr));
		}
		else if (SvLEN(dstr))
		    Safefree(SvPVX(dstr));
	    }
	    (void)SvPOK_only(dstr);
	    SvPV_set(dstr, SvPVX(sstr));
	    SvLEN_set(dstr, SvLEN(sstr));
	    SvCUR_set(dstr, SvCUR(sstr));

	    SvTEMP_off(dstr);
	    (void)SvOK_off(sstr);		/* NOTE: nukes most SvFLAGS on sstr */
	    SvPV_set(sstr, Nullch);
	    SvLEN_set(sstr, 0);
	    SvCUR_set(sstr, 0);
	    SvTEMP_off(sstr);
	}
	else {					/* have to copy actual string */
	    STRLEN len = SvCUR(sstr);

	    SvGROW(dstr, len + 1);		/* inlined from sv_setpvn */
	    Move(SvPVX(sstr),SvPVX(dstr),len,char);
	    SvCUR_set(dstr, len);
	    *SvEND(dstr) = '\0';
	    (void)SvPOK_only(dstr);
	}
	if (sflags & SVf_UTF8)
	    SvUTF8_on(dstr);
	/*SUPPRESS 560*/
	if (sflags & SVp_NOK) {
	    SvNOK_on(dstr);
	    SvNVX(dstr) = SvNVX(sstr);
	}
	if (sflags & SVp_IOK) {
	    (void)SvIOK_on(dstr);
	    SvIVX(dstr) = SvIVX(sstr);
	    if (sflags & SVf_IVisUV)
		SvIsUV_on(dstr);
	}
    }
    else if (sflags & SVp_NOK) {
	SvNVX(dstr) = SvNVX(sstr);
	(void)SvNOK_only(dstr);
	if (sflags & SVf_IOK) {
	    (void)SvIOK_on(dstr);
	    SvIVX(dstr) = SvIVX(sstr);
	    /* XXXX Do we want to set IsUV for IV(ROK)?  Be extra safe... */
	    if (sflags & SVf_IVisUV)
		SvIsUV_on(dstr);
	}
    }
    else if (sflags & SVp_IOK) {
	(void)SvIOK_only(dstr);
	SvIVX(dstr) = SvIVX(sstr);
	if (sflags & SVf_IVisUV)
	    SvIsUV_on(dstr);
    }
    else {
	if (dtype == SVt_PVGV) {
	    if (ckWARN(WARN_MISC))
		Perl_warner(aTHX_ WARN_MISC, "Undefined value assigned to typeglob");
	}
	else
	    (void)SvOK_off(dstr);
    }
    if (SvTAINTED(sstr))
	SvTAINT(dstr);
}

/*
=for apidoc sv_setsv_mg

Like C<sv_setsv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_setsv_mg(pTHX_ SV *dstr, register SV *sstr)
{
    sv_setsv(dstr,sstr);
    SvSETMAGIC(dstr);
}

/*
=for apidoc sv_setpvn

Copies a string into an SV.  The C<len> parameter indicates the number of
bytes to be copied.  Does not handle 'set' magic.  See C<sv_setpvn_mg>.

=cut
*/

void
Perl_sv_setpvn(pTHX_ register SV *sv, register const char *ptr, register STRLEN len)
{
    register char *dptr;

    SV_CHECK_THINKFIRST(sv);
    if (!ptr) {
	(void)SvOK_off(sv);
	return;
    }
    else {
        /* len is STRLEN which is unsigned, need to copy to signed */
	IV iv = len;
	assert(iv >= 0);
    }
    (void)SvUPGRADE(sv, SVt_PV);

    SvGROW(sv, len + 1);
    dptr = SvPVX(sv);
    Move(ptr,dptr,len,char);
    dptr[len] = '\0';
    SvCUR_set(sv, len);
    (void)SvPOK_only(sv);		/* validate pointer */
    SvTAINT(sv);
}

/*
=for apidoc sv_setpvn_mg

Like C<sv_setpvn>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_setpvn_mg(pTHX_ register SV *sv, register const char *ptr, register STRLEN len)
{
    sv_setpvn(sv,ptr,len);
    SvSETMAGIC(sv);
}

/*
=for apidoc sv_setpv

Copies a string into an SV.  The string must be null-terminated.  Does not
handle 'set' magic.  See C<sv_setpv_mg>.

=cut
*/

void
Perl_sv_setpv(pTHX_ register SV *sv, register const char *ptr)
{
    register STRLEN len;

    SV_CHECK_THINKFIRST(sv);
    if (!ptr) {
	(void)SvOK_off(sv);
	return;
    }
    len = strlen(ptr);
    (void)SvUPGRADE(sv, SVt_PV);

    SvGROW(sv, len + 1);
    Move(ptr,SvPVX(sv),len+1,char);
    SvCUR_set(sv, len);
    (void)SvPOK_only(sv);		/* validate pointer */
    SvTAINT(sv);
}

/*
=for apidoc sv_setpv_mg

Like C<sv_setpv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_setpv_mg(pTHX_ register SV *sv, register const char *ptr)
{
    sv_setpv(sv,ptr);
    SvSETMAGIC(sv);
}

/*
=for apidoc sv_usepvn

Tells an SV to use C<ptr> to find its string value.  Normally the string is
stored inside the SV but sv_usepvn allows the SV to use an outside string. 
The C<ptr> should point to memory that was allocated by C<malloc>.  The
string length, C<len>, must be supplied.  This function will realloc the
memory pointed to by C<ptr>, so that pointer should not be freed or used by
the programmer after giving it to sv_usepvn.  Does not handle 'set' magic.
See C<sv_usepvn_mg>.

=cut
*/

void
Perl_sv_usepvn(pTHX_ register SV *sv, register char *ptr, register STRLEN len)
{
    SV_CHECK_THINKFIRST(sv);
    (void)SvUPGRADE(sv, SVt_PV);
    if (!ptr) {
	(void)SvOK_off(sv);
	return;
    }
    (void)SvOOK_off(sv);
    if (SvPVX(sv) && SvLEN(sv))
	Safefree(SvPVX(sv));
    Renew(ptr, len+1, char);
    SvPVX(sv) = ptr;
    SvCUR_set(sv, len);
    SvLEN_set(sv, len+1);
    *SvEND(sv) = '\0';
    (void)SvPOK_only(sv);		/* validate pointer */
    SvTAINT(sv);
}

/*
=for apidoc sv_usepvn_mg

Like C<sv_usepvn>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_usepvn_mg(pTHX_ register SV *sv, register char *ptr, register STRLEN len)
{
    sv_usepvn(sv,ptr,len);
    SvSETMAGIC(sv);
}

void
Perl_sv_force_normal(pTHX_ register SV *sv)
{
    if (SvREADONLY(sv)) {
	if (PL_curcop != &PL_compiling)
	    Perl_croak(aTHX_ PL_no_modify);
    }
    if (SvROK(sv))
	sv_unref(sv);
    else if (SvFAKE(sv) && SvTYPE(sv) == SVt_PVGV)
	sv_unglob(sv);
}
    
/*
=for apidoc sv_chop

Efficient removal of characters from the beginning of the string buffer. 
SvPOK(sv) must be true and the C<ptr> must be a pointer to somewhere inside
the string buffer.  The C<ptr> becomes the first character of the adjusted
string.

=cut
*/

void
Perl_sv_chop(pTHX_ register SV *sv, register char *ptr)	/* like set but assuming ptr is in sv */
                
                   
{
    register STRLEN delta;

    if (!ptr || !SvPOKp(sv))
	return;
    SV_CHECK_THINKFIRST(sv);
    if (SvTYPE(sv) < SVt_PVIV)
	sv_upgrade(sv,SVt_PVIV);

    if (!SvOOK(sv)) {
	if (!SvLEN(sv)) { /* make copy of shared string */
	    char *pvx = SvPVX(sv);
	    STRLEN len = SvCUR(sv);
	    SvGROW(sv, len + 1);
	    Move(pvx,SvPVX(sv),len,char);
	    *SvEND(sv) = '\0';
	}
	SvIVX(sv) = 0;
	SvFLAGS(sv) |= SVf_OOK;
    }
    SvFLAGS(sv) &= ~(SVf_IOK|SVf_NOK|SVp_IOK|SVp_NOK|SVf_IVisUV);
    delta = ptr - SvPVX(sv);
    SvLEN(sv) -= delta;
    SvCUR(sv) -= delta;
    SvPVX(sv) += delta;
    SvIVX(sv) += delta;
}

/*
=for apidoc sv_catpvn

Concatenates the string onto the end of the string which is in the SV.  The
C<len> indicates number of bytes to copy.  Handles 'get' magic, but not
'set' magic.  See C<sv_catpvn_mg>.

=cut
*/

void
Perl_sv_catpvn(pTHX_ register SV *sv, register const char *ptr, register STRLEN len)
{
    STRLEN tlen;
    char *junk;

    junk = SvPV_force(sv, tlen);
    SvGROW(sv, tlen + len + 1);
    if (ptr == junk)
	ptr = SvPVX(sv);
    Move(ptr,SvPVX(sv)+tlen,len,char);
    SvCUR(sv) += len;
    *SvEND(sv) = '\0';
    (void)SvPOK_only_UTF8(sv);		/* validate pointer */
    SvTAINT(sv);
}

/*
=for apidoc sv_catpvn_mg

Like C<sv_catpvn>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_catpvn_mg(pTHX_ register SV *sv, register const char *ptr, register STRLEN len)
{
    sv_catpvn(sv,ptr,len);
    SvSETMAGIC(sv);
}

/*
=for apidoc sv_catsv

Concatenates the string from SV C<ssv> onto the end of the string in
SV C<dsv>.  Modifies C<dsv> but not C<ssv>.  Handles 'get' magic, but
not 'set' magic.  See C<sv_catsv_mg>.

=cut */

void
Perl_sv_catsv(pTHX_ SV *dstr, register SV *sstr)
{
    char *spv;
    STRLEN slen;
    if (!sstr)
	return;
    if ((spv = SvPV(sstr, slen))) {
	bool dutf8 = DO_UTF8(dstr);
	bool sutf8 = DO_UTF8(sstr);

	if (dutf8 == sutf8)
	    sv_catpvn(dstr,spv,slen);
	else {
	    if (dutf8) {
		SV* cstr = newSVsv(sstr);
		char *cpv;
		STRLEN clen;

		sv_utf8_upgrade(cstr);
		cpv = SvPV(cstr,clen);
		sv_catpvn(dstr,cpv,clen);
		sv_2mortal(cstr);
	    }
	    else {
		sv_utf8_upgrade(dstr);
		sv_catpvn(dstr,spv,slen);
		SvUTF8_on(dstr);
	    }
	}
    }
}

/*
=for apidoc sv_catsv_mg

Like C<sv_catsv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_catsv_mg(pTHX_ SV *dstr, register SV *sstr)
{
    sv_catsv(dstr,sstr);
    SvSETMAGIC(dstr);
}

/*
=for apidoc sv_catpv

Concatenates the string onto the end of the string which is in the SV.
Handles 'get' magic, but not 'set' magic.  See C<sv_catpv_mg>.

=cut
*/

void
Perl_sv_catpv(pTHX_ register SV *sv, register const char *ptr)
{
    register STRLEN len;
    STRLEN tlen;
    char *junk;

    if (!ptr)
	return;
    junk = SvPV_force(sv, tlen);
    len = strlen(ptr);
    SvGROW(sv, tlen + len + 1);
    if (ptr == junk)
	ptr = SvPVX(sv);
    Move(ptr,SvPVX(sv)+tlen,len+1,char);
    SvCUR(sv) += len;
    (void)SvPOK_only_UTF8(sv);		/* validate pointer */
    SvTAINT(sv);
}

/*
=for apidoc sv_catpv_mg

Like C<sv_catpv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_catpv_mg(pTHX_ register SV *sv, register const char *ptr)
{
    sv_catpv(sv,ptr);
    SvSETMAGIC(sv);
}

SV *
Perl_newSV(pTHX_ STRLEN len)
{
    register SV *sv;
    
    new_SV(sv);
    if (len) {
	sv_upgrade(sv, SVt_PV);
	SvGROW(sv, len + 1);
    }
    return sv;
}

/* name is assumed to contain an SV* if (name && namelen == HEf_SVKEY) */

/*
=for apidoc sv_magic

Adds magic to an SV.

=cut
*/

void
Perl_sv_magic(pTHX_ register SV *sv, SV *obj, int how, const char *name, I32 namlen)
{
    MAGIC* mg;
    
    if (SvREADONLY(sv)) {
	if (PL_curcop != &PL_compiling && !strchr("gBf", how))
	    Perl_croak(aTHX_ PL_no_modify);
    }
    if (SvMAGICAL(sv) || (how == 't' && SvTYPE(sv) >= SVt_PVMG)) {
	if (SvMAGIC(sv) && (mg = mg_find(sv, how))) {
	    if (how == 't')
		mg->mg_len |= 1;
	    return;
	}
    }
    else {
        (void)SvUPGRADE(sv, SVt_PVMG);
    }
    Newz(702,mg, 1, MAGIC);
    mg->mg_moremagic = SvMAGIC(sv);
    SvMAGIC(sv) = mg;

    /* Some magic sontains a reference loop, where the sv and object refer to
       each other.  To prevent a avoid a reference loop that would prevent such
       objects being freed, we look for such loops and if we find one we avoid
       incrementing the object refcount. */
    if (!obj || obj == sv || how == '#' || how == 'r' ||
	(SvTYPE(obj) == SVt_PVGV &&
	    (GvSV(obj) == sv || GvHV(obj) == (HV*)sv || GvAV(obj) == (AV*)sv ||
	    GvCV(obj) == (CV*)sv || GvIOp(obj) == (IO*)sv ||
	    GvFORM(obj) == (CV*)sv)))
    {
	mg->mg_obj = obj;
    }
    else {
	mg->mg_obj = SvREFCNT_inc(obj);
	mg->mg_flags |= MGf_REFCOUNTED;
    }
    mg->mg_type = how;
    mg->mg_len = namlen;
    if (name)
	if (namlen >= 0)
	    mg->mg_ptr = savepvn(name, namlen);
	else if (namlen == HEf_SVKEY)
	    mg->mg_ptr = (char*)SvREFCNT_inc((SV*)name);
    
    switch (how) {
    case 0:
	mg->mg_virtual = &PL_vtbl_sv;
	break;
    case 'A':
        mg->mg_virtual = &PL_vtbl_amagic;
        break;
    case 'a':
        mg->mg_virtual = &PL_vtbl_amagicelem;
        break;
    case 'c':
        mg->mg_virtual = 0;
        break;
    case 'B':
	mg->mg_virtual = &PL_vtbl_bm;
	break;
    case 'D':
	mg->mg_virtual = &PL_vtbl_regdata;
	break;
    case 'd':
	mg->mg_virtual = &PL_vtbl_regdatum;
	break;
    case 'E':
	mg->mg_virtual = &PL_vtbl_env;
	break;
    case 'f':
	mg->mg_virtual = &PL_vtbl_fm;
	break;
    case 'e':
	mg->mg_virtual = &PL_vtbl_envelem;
	break;
    case 'g':
	mg->mg_virtual = &PL_vtbl_mglob;
	break;
    case 'I':
	mg->mg_virtual = &PL_vtbl_isa;
	break;
    case 'i':
	mg->mg_virtual = &PL_vtbl_isaelem;
	break;
    case 'k':
	mg->mg_virtual = &PL_vtbl_nkeys;
	break;
    case 'L':
	SvRMAGICAL_on(sv);
	mg->mg_virtual = 0;
	break;
    case 'l':
	mg->mg_virtual = &PL_vtbl_dbline;
	break;
#ifdef USE_THREADS
    case 'm':
	mg->mg_virtual = &PL_vtbl_mutex;
	break;
#endif /* USE_THREADS */
#ifdef USE_LOCALE_COLLATE
    case 'o':
        mg->mg_virtual = &PL_vtbl_collxfrm;
        break;
#endif /* USE_LOCALE_COLLATE */
    case 'P':
	mg->mg_virtual = &PL_vtbl_pack;
	break;
    case 'p':
    case 'q':
	mg->mg_virtual = &PL_vtbl_packelem;
	break;
    case 'r':
	mg->mg_virtual = &PL_vtbl_regexp;
	break;
    case 'S':
	mg->mg_virtual = &PL_vtbl_sig;
	break;
    case 's':
	mg->mg_virtual = &PL_vtbl_sigelem;
	break;
    case 't':
	mg->mg_virtual = &PL_vtbl_taint;
	mg->mg_len = 1;
	break;
    case 'U':
	mg->mg_virtual = &PL_vtbl_uvar;
	break;
    case 'v':
	mg->mg_virtual = &PL_vtbl_vec;
	break;
    case 'x':
	mg->mg_virtual = &PL_vtbl_substr;
	break;
    case 'y':
	mg->mg_virtual = &PL_vtbl_defelem;
	break;
    case '*':
	mg->mg_virtual = &PL_vtbl_glob;
	break;
    case '#':
	mg->mg_virtual = &PL_vtbl_arylen;
	break;
    case '.':
	mg->mg_virtual = &PL_vtbl_pos;
	break;
    case '<':
	mg->mg_virtual = &PL_vtbl_backref;
	break;
    case '~':	/* Reserved for use by extensions not perl internals.	*/
	/* Useful for attaching extension internal data to perl vars.	*/
	/* Note that multiple extensions may clash if magical scalars	*/
	/* etc holding private data from one are passed to another.	*/
	SvRMAGICAL_on(sv);
	break;
    default:
	Perl_croak(aTHX_ "Don't know how to handle magic of type '%c'", how);
    }
    mg_magical(sv);
    if (SvGMAGICAL(sv))
	SvFLAGS(sv) &= ~(SVf_IOK|SVf_NOK|SVf_POK);
}

/*
=for apidoc sv_unmagic

Removes magic from an SV.

=cut
*/

int
Perl_sv_unmagic(pTHX_ SV *sv, int type)
{
    MAGIC* mg;
    MAGIC** mgp;
    if (SvTYPE(sv) < SVt_PVMG || !SvMAGIC(sv))
	return 0;
    mgp = &SvMAGIC(sv);
    for (mg = *mgp; mg; mg = *mgp) {
	if (mg->mg_type == type) {
	    MGVTBL* vtbl = mg->mg_virtual;
	    *mgp = mg->mg_moremagic;
	    if (vtbl && vtbl->svt_free)
		CALL_FPTR(vtbl->svt_free)(aTHX_ sv, mg);
	    if (mg->mg_ptr && mg->mg_type != 'g')
		if (mg->mg_len >= 0)
		    Safefree(mg->mg_ptr);
		else if (mg->mg_len == HEf_SVKEY)
		    SvREFCNT_dec((SV*)mg->mg_ptr);
	    if (mg->mg_flags & MGf_REFCOUNTED)
		SvREFCNT_dec(mg->mg_obj);
	    Safefree(mg);
	}
	else
	    mgp = &mg->mg_moremagic;
    }
    if (!SvMAGIC(sv)) {
	SvMAGICAL_off(sv);
	SvFLAGS(sv) |= (SvFLAGS(sv) & (SVp_IOK|SVp_NOK|SVp_POK)) >> PRIVSHIFT;
    }

    return 0;
}

/*
=for apidoc sv_rvweaken

Weaken a reference.

=cut
*/

SV *
Perl_sv_rvweaken(pTHX_ SV *sv)
{
    SV *tsv;
    if (!SvOK(sv))  /* let undefs pass */
	return sv;
    if (!SvROK(sv))
	Perl_croak(aTHX_ "Can't weaken a nonreference");
    else if (SvWEAKREF(sv)) {
	if (ckWARN(WARN_MISC))
	    Perl_warner(aTHX_ WARN_MISC, "Reference is already weak");
	return sv;
    }
    tsv = SvRV(sv);
    sv_add_backref(tsv, sv);
    SvWEAKREF_on(sv);
    SvREFCNT_dec(tsv);              
    return sv;
}

void
Perl_sv_add_backref(pTHX_ SV *tsv, SV *sv)
{
    AV *av;
    MAGIC *mg;
    if (SvMAGICAL(tsv) && (mg = mg_find(tsv, '<')))
	av = (AV*)mg->mg_obj;
    else {
	av = newAV();
	sv_magic(tsv, (SV*)av, '<', NULL, 0);
	SvREFCNT_dec(av);           /* for sv_magic */
    }
    av_push(av,sv);
}

void 
Perl_sv_del_backref(pTHX_ SV *sv)
{
    AV *av;
    SV **svp;
    I32 i;
    SV *tsv = SvRV(sv);
    MAGIC *mg;
    if (!SvMAGICAL(tsv) || !(mg = mg_find(tsv, '<')))
	Perl_croak(aTHX_ "panic: del_backref");
    av = (AV *)mg->mg_obj;
    svp = AvARRAY(av);
    i = AvFILLp(av);
    while (i >= 0) {
	if (svp[i] == sv) {
	    svp[i] = &PL_sv_undef; /* XXX */
	}
	i--;
    }
}

/*
=for apidoc sv_insert

Inserts a string at the specified offset/length within the SV. Similar to
the Perl substr() function.

=cut
*/

void
Perl_sv_insert(pTHX_ SV *bigstr, STRLEN offset, STRLEN len, char *little, STRLEN littlelen)
{
    register char *big;
    register char *mid;
    register char *midend;
    register char *bigend;
    register I32 i;
    STRLEN curlen;
    

    if (!bigstr)
	Perl_croak(aTHX_ "Can't modify non-existent substring");
    SvPV_force(bigstr, curlen);
    (void)SvPOK_only_UTF8(bigstr);
    if (offset + len > curlen) {
	SvGROW(bigstr, offset+len+1);
	Zero(SvPVX(bigstr)+curlen, offset+len-curlen, char);
	SvCUR_set(bigstr, offset+len);
    }

    SvTAINT(bigstr);
    i = littlelen - len;
    if (i > 0) {			/* string might grow */
	big = SvGROW(bigstr, SvCUR(bigstr) + i + 1);
	mid = big + offset + len;
	midend = bigend = big + SvCUR(bigstr);
	bigend += i;
	*bigend = '\0';
	while (midend > mid)		/* shove everything down */
	    *--bigend = *--midend;
	Move(little,big+offset,littlelen,char);
	SvCUR(bigstr) += i;
	SvSETMAGIC(bigstr);
	return;
    }
    else if (i == 0) {
	Move(little,SvPVX(bigstr)+offset,len,char);
	SvSETMAGIC(bigstr);
	return;
    }

    big = SvPVX(bigstr);
    mid = big + offset;
    midend = mid + len;
    bigend = big + SvCUR(bigstr);

    if (midend > bigend)
	Perl_croak(aTHX_ "panic: sv_insert");

    if (mid - big > bigend - midend) {	/* faster to shorten from end */
	if (littlelen) {
	    Move(little, mid, littlelen,char);
	    mid += littlelen;
	}
	i = bigend - midend;
	if (i > 0) {
	    Move(midend, mid, i,char);
	    mid += i;
	}
	*mid = '\0';
	SvCUR_set(bigstr, mid - big);
    }
    /*SUPPRESS 560*/
    else if ((i = mid - big)) {	/* faster from front */
	midend -= littlelen;
	mid = midend;
	sv_chop(bigstr,midend-i);
	big += i;
	while (i--)
	    *--midend = *--big;
	if (littlelen)
	    Move(little, mid, littlelen,char);
    }
    else if (littlelen) {
	midend -= littlelen;
	sv_chop(bigstr,midend);
	Move(little,midend,littlelen,char);
    }
    else {
	sv_chop(bigstr,midend);
    }
    SvSETMAGIC(bigstr);
}

/*
=for apidoc sv_replace

Make the first argument a copy of the second, then delete the original.

=cut
*/

void
Perl_sv_replace(pTHX_ register SV *sv, register SV *nsv)
{
    U32 refcnt = SvREFCNT(sv);
    SV_CHECK_THINKFIRST(sv);
    if (SvREFCNT(nsv) != 1 && ckWARN_d(WARN_INTERNAL))
	Perl_warner(aTHX_ WARN_INTERNAL, "Reference miscount in sv_replace()");
    if (SvMAGICAL(sv)) {
	if (SvMAGICAL(nsv))
	    mg_free(nsv);
	else
	    sv_upgrade(nsv, SVt_PVMG);
	SvMAGIC(nsv) = SvMAGIC(sv);
	SvFLAGS(nsv) |= SvMAGICAL(sv);
	SvMAGICAL_off(sv);
	SvMAGIC(sv) = 0;
    }
    SvREFCNT(sv) = 0;
    sv_clear(sv);
    assert(!SvREFCNT(sv));
    StructCopy(nsv,sv,SV);
    SvREFCNT(sv) = refcnt;
    SvFLAGS(nsv) |= SVTYPEMASK;		/* Mark as freed */
    del_SV(nsv);
}

/*
=for apidoc sv_clear

Clear an SV, making it empty. Does not free the memory used by the SV
itself.

=cut
*/

void
Perl_sv_clear(pTHX_ register SV *sv)
{
    HV* stash;
    assert(sv);
    assert(SvREFCNT(sv) == 0);

    if (SvOBJECT(sv)) {
	if (PL_defstash) {		/* Still have a symbol table? */
	    dSP;
	    GV* destructor;
	    SV tmpref;

	    Zero(&tmpref, 1, SV);
	    sv_upgrade(&tmpref, SVt_RV);
	    SvROK_on(&tmpref);
	    SvREADONLY_on(&tmpref);	/* DESTROY() could be naughty */
	    SvREFCNT(&tmpref) = 1;

	    do {
		stash = SvSTASH(sv);
		destructor = gv_fetchmethod(SvSTASH(sv), "DESTROY");
		if (destructor) {
		    ENTER;
		    PUSHSTACKi(PERLSI_DESTROY);
		    SvRV(&tmpref) = SvREFCNT_inc(sv);
		    EXTEND(SP, 2);
		    PUSHMARK(SP);
		    PUSHs(&tmpref);
		    PUTBACK;
		    call_sv((SV*)GvCV(destructor),
			    G_DISCARD|G_EVAL|G_KEEPERR);
		    SvREFCNT(sv)--;
		    POPSTACK;
		    SPAGAIN;
		    LEAVE;
		}
	    } while (SvOBJECT(sv) && SvSTASH(sv) != stash);

	    del_XRV(SvANY(&tmpref));

	    if (SvREFCNT(sv)) {
		if (PL_in_clean_objs)
		    Perl_croak(aTHX_ "DESTROY created new reference to dead object '%s'",
			  HvNAME(stash));
		/* DESTROY gave object new lease on life */
		return;
	    }
	}

	if (SvOBJECT(sv)) {
	    SvREFCNT_dec(SvSTASH(sv));	/* possibly of changed persuasion */
	    SvOBJECT_off(sv);	/* Curse the object. */
	    if (SvTYPE(sv) != SVt_PVIO)
		--PL_sv_objcount;	/* XXX Might want something more general */
	}
    }
    if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv))
	mg_free(sv);
    stash = NULL;
    switch (SvTYPE(sv)) {
    case SVt_PVIO:
	if (IoIFP(sv) &&
	    IoIFP(sv) != PerlIO_stdin() &&
	    IoIFP(sv) != PerlIO_stdout() &&
	    IoIFP(sv) != PerlIO_stderr())
	{
	    io_close((IO*)sv, FALSE);
	}
	if (IoDIRP(sv) && !(IoFLAGS(sv) & IOf_FAKE_DIRP))
	    PerlDir_close(IoDIRP(sv));
	IoDIRP(sv) = (DIR*)NULL;
	Safefree(IoTOP_NAME(sv));
	Safefree(IoFMT_NAME(sv));
	Safefree(IoBOTTOM_NAME(sv));
	/* FALL THROUGH */
    case SVt_PVBM:
	goto freescalar;
    case SVt_PVCV:
    case SVt_PVFM:
	cv_undef((CV*)sv);
	goto freescalar;
    case SVt_PVHV:
	hv_undef((HV*)sv);
	break;
    case SVt_PVAV:
	av_undef((AV*)sv);
	break;
    case SVt_PVLV:
	SvREFCNT_dec(LvTARG(sv));
	goto freescalar;
    case SVt_PVGV:
	gp_free((GV*)sv);
	Safefree(GvNAME(sv));
	/* cannot decrease stash refcount yet, as we might recursively delete
	   ourselves when the refcnt drops to zero. Delay SvREFCNT_dec
	   of stash until current sv is completely gone.
	   -- JohnPC, 27 Mar 1998 */
	stash = GvSTASH(sv);
	/* FALL THROUGH */
    case SVt_PVMG:
    case SVt_PVNV:
    case SVt_PVIV:
      freescalar:
	(void)SvOOK_off(sv);
	/* FALL THROUGH */
    case SVt_PV:
    case SVt_RV:
	if (SvROK(sv)) {
	    if (SvWEAKREF(sv))
	        sv_del_backref(sv);
	    else
	        SvREFCNT_dec(SvRV(sv));
	}
	else if (SvPVX(sv) && SvLEN(sv))
	    Safefree(SvPVX(sv));
	break;
/*
    case SVt_NV:
    case SVt_IV:
    case SVt_NULL:
	break;
*/
    }

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	break;
    case SVt_IV:
	del_XIV(SvANY(sv));
	break;
    case SVt_NV:
	del_XNV(SvANY(sv));
	break;
    case SVt_RV:
	del_XRV(SvANY(sv));
	break;
    case SVt_PV:
	del_XPV(SvANY(sv));
	break;
    case SVt_PVIV:
	del_XPVIV(SvANY(sv));
	break;
    case SVt_PVNV:
	del_XPVNV(SvANY(sv));
	break;
    case SVt_PVMG:
	del_XPVMG(SvANY(sv));
	break;
    case SVt_PVLV:
	del_XPVLV(SvANY(sv));
	break;
    case SVt_PVAV:
	del_XPVAV(SvANY(sv));
	break;
    case SVt_PVHV:
	del_XPVHV(SvANY(sv));
	break;
    case SVt_PVCV:
	del_XPVCV(SvANY(sv));
	break;
    case SVt_PVGV:
	del_XPVGV(SvANY(sv));
	/* code duplication for increased performance. */
	SvFLAGS(sv) &= SVf_BREAK;
	SvFLAGS(sv) |= SVTYPEMASK;
	/* decrease refcount of the stash that owns this GV, if any */
	if (stash)
	    SvREFCNT_dec(stash);
	return; /* not break, SvFLAGS reset already happened */
    case SVt_PVBM:
	del_XPVBM(SvANY(sv));
	break;
    case SVt_PVFM:
	del_XPVFM(SvANY(sv));
	break;
    case SVt_PVIO:
	del_XPVIO(SvANY(sv));
	break;
    }
    SvFLAGS(sv) &= SVf_BREAK;
    SvFLAGS(sv) |= SVTYPEMASK;
}

SV *
Perl_sv_newref(pTHX_ SV *sv)
{
    if (sv)
	ATOMIC_INC(SvREFCNT(sv));
    return sv;
}

/*
=for apidoc sv_free

Free the memory used by an SV.

=cut
*/

void
Perl_sv_free(pTHX_ SV *sv)
{
    int refcount_is_zero;

    if (!sv)
	return;
    if (SvREFCNT(sv) == 0) {
	if (SvFLAGS(sv) & SVf_BREAK)
	    return;
	if (PL_in_clean_all) /* All is fair */
	    return;
	if (SvREADONLY(sv) && SvIMMORTAL(sv)) {
	    /* make sure SvREFCNT(sv)==0 happens very seldom */
	    SvREFCNT(sv) = (~(U32)0)/2;
	    return;
	}
	if (ckWARN_d(WARN_INTERNAL))
	    Perl_warner(aTHX_ WARN_INTERNAL, "Attempt to free unreferenced scalar");
	return;
    }
    ATOMIC_DEC_AND_TEST(refcount_is_zero, SvREFCNT(sv));
    if (!refcount_is_zero)
	return;
#ifdef DEBUGGING
    if (SvTEMP(sv)) {
	if (ckWARN_d(WARN_DEBUGGING))
	    Perl_warner(aTHX_ WARN_DEBUGGING,
			"Attempt to free temp prematurely: SV 0x%"UVxf,
			PTR2UV(sv));
	return;
    }
#endif
    if (SvREADONLY(sv) && SvIMMORTAL(sv)) {
	/* make sure SvREFCNT(sv)==0 happens very seldom */
	SvREFCNT(sv) = (~(U32)0)/2;
	return;
    }
    sv_clear(sv);
    if (! SvREFCNT(sv))
	del_SV(sv);
}

/*
=for apidoc sv_len

Returns the length of the string in the SV.  See also C<SvCUR>.

=cut
*/

STRLEN
Perl_sv_len(pTHX_ register SV *sv)
{
    char *junk;
    STRLEN len;

    if (!sv)
	return 0;

    if (SvGMAGICAL(sv))
	len = mg_length(sv);
    else
	junk = SvPV(sv, len);
    return len;
}

/*
=for apidoc sv_len_utf8

Returns the number of characters in the string in an SV, counting wide
UTF8 bytes as a single character.

=cut
*/

STRLEN
Perl_sv_len_utf8(pTHX_ register SV *sv)
{
    if (!sv)
	return 0;

#ifdef NOTYET
    if (SvGMAGICAL(sv))
	return mg_length(sv);
    else
#endif
    {
	STRLEN len;
	U8 *s = (U8*)SvPV(sv, len);

	return Perl_utf8_length(aTHX_ s, s + len);
    }
}

void
Perl_sv_pos_u2b(pTHX_ register SV *sv, I32* offsetp, I32* lenp)
{
    U8 *start;
    U8 *s;
    U8 *send;
    I32 uoffset = *offsetp;
    STRLEN len;

    if (!sv)
	return;

    start = s = (U8*)SvPV(sv, len);
    send = s + len;
    while (s < send && uoffset--)
	s += UTF8SKIP(s);
    if (s >= send)
	s = send;
    *offsetp = s - start;
    if (lenp) {
	I32 ulen = *lenp;
	start = s;
	while (s < send && ulen--)
	    s += UTF8SKIP(s);
	if (s >= send)
	    s = send;
	*lenp = s - start;
    }
    return;
}

void
Perl_sv_pos_b2u(pTHX_ register SV *sv, I32* offsetp)
{
    U8 *s;
    U8 *send;
    STRLEN len;

    if (!sv)
	return;

    s = (U8*)SvPV(sv, len);
    if (len < *offsetp)
	Perl_croak(aTHX_ "panic: sv_pos_b2u: bad byte offset");
    send = s + *offsetp;
    len = 0;
    while (s < send) {
	STRLEN n;

	if (utf8_to_uv(s, UTF8SKIP(s), &n, 0)) {
	    s += n;
	    len++;
	}
	else
	    break;
    }
    *offsetp = len;
    return;
}

/*
=for apidoc sv_eq

Returns a boolean indicating whether the strings in the two SVs are
identical.

=cut
*/

I32
Perl_sv_eq(pTHX_ register SV *sv1, register SV *sv2)
{
    char *pv1;
    STRLEN cur1;
    char *pv2;
    STRLEN cur2;
    I32  eq     = 0;
    bool pv1tmp = FALSE;
    bool pv2tmp = FALSE;

    if (!sv1) {
	pv1 = "";
	cur1 = 0;
    }
    else
	pv1 = SvPV(sv1, cur1);

    if (!sv2){
	pv2 = "";
	cur2 = 0;
    }
    else
	pv2 = SvPV(sv2, cur2);

    /* do not utf8ize the comparands as a side-effect */
    if (cur1 && cur2 && SvUTF8(sv1) != SvUTF8(sv2) && !IN_BYTE) {
	bool is_utf8 = TRUE;

	if (SvUTF8(sv1)) {
	    char *pv = (char*)bytes_from_utf8((U8*)pv1, &cur1, &is_utf8);

	    if ((pv1tmp = (pv != pv1)))
		pv1 = pv;
	}
	else {
	    char *pv = (char *)bytes_from_utf8((U8*)pv2, &cur2, &is_utf8);

	    if ((pv2tmp = (pv != pv2)))
		pv2 = pv;
	}
    }

    if (cur1 == cur2)
	eq = memEQ(pv1, pv2, cur1);
	
    if (pv1tmp)
	Safefree(pv1);
    if (pv2tmp)
	Safefree(pv2);

    return eq;
}

/*
=for apidoc sv_cmp

Compares the strings in two SVs.  Returns -1, 0, or 1 indicating whether the
string in C<sv1> is less than, equal to, or greater than the string in
C<sv2>.

=cut
*/

I32
Perl_sv_cmp(pTHX_ register SV *sv1, register SV *sv2)
{
    STRLEN cur1, cur2;
    char *pv1, *pv2;
    I32  cmp; 
    bool pv1tmp = FALSE;
    bool pv2tmp = FALSE;

    if (!sv1) {
	pv1 = "";
	cur1 = 0;
    }
    else
	pv1 = SvPV(sv1, cur1);

    if (!sv2){
	pv2 = "";
	cur2 = 0;
    }
    else
	pv2 = SvPV(sv2, cur2);

    /* do not utf8ize the comparands as a side-effect */
    if (cur1 && cur2 && SvUTF8(sv1) != SvUTF8(sv2) && !IN_BYTE) {
	if (SvUTF8(sv1)) {
	    pv2 = (char*)bytes_to_utf8((U8*)pv2, &cur2);
	    pv2tmp = TRUE;
	}
	else {
	    pv1 = (char*)bytes_to_utf8((U8*)pv1, &cur1);
	    pv1tmp = TRUE;
	}
    }

    if (!cur1) {
	cmp = cur2 ? -1 : 0;
    } else if (!cur2) {
	cmp = 1;
    } else {
	I32 retval = memcmp((void*)pv1, (void*)pv2, cur1 < cur2 ? cur1 : cur2);

	if (retval) {
	    cmp = retval < 0 ? -1 : 1;
	} else if (cur1 == cur2) {
	    cmp = 0;
        } else {
	    cmp = cur1 < cur2 ? -1 : 1;
	}
    }

    if (pv1tmp)
	Safefree(pv1);
    if (pv2tmp)
	Safefree(pv2);

    return cmp;
}

/*
=for apidoc sv_cmp_locale

Compares the strings in two SVs in a locale-aware manner. See
L</sv_cmp_locale>

=cut
*/

I32
Perl_sv_cmp_locale(pTHX_ register SV *sv1, register SV *sv2)
{
#ifdef USE_LOCALE_COLLATE

    char *pv1, *pv2;
    STRLEN len1, len2;
    I32 retval;

    if (PL_collation_standard)
	goto raw_compare;

    len1 = 0;
    pv1 = sv1 ? sv_collxfrm(sv1, &len1) : (char *) NULL;
    len2 = 0;
    pv2 = sv2 ? sv_collxfrm(sv2, &len2) : (char *) NULL;

    if (!pv1 || !len1) {
	if (pv2 && len2)
	    return -1;
	else
	    goto raw_compare;
    }
    else {
	if (!pv2 || !len2)
	    return 1;
    }

    retval = memcmp((void*)pv1, (void*)pv2, len1 < len2 ? len1 : len2);

    if (retval)
	return retval < 0 ? -1 : 1;

    /*
     * When the result of collation is equality, that doesn't mean
     * that there are no differences -- some locales exclude some
     * characters from consideration.  So to avoid false equalities,
     * we use the raw string as a tiebreaker.
     */

  raw_compare:
    /* FALL THROUGH */

#endif /* USE_LOCALE_COLLATE */

    return sv_cmp(sv1, sv2);
}

#ifdef USE_LOCALE_COLLATE
/*
 * Any scalar variable may carry an 'o' magic that contains the
 * scalar data of the variable transformed to such a format that
 * a normal memory comparison can be used to compare the data
 * according to the locale settings.
 */
char *
Perl_sv_collxfrm(pTHX_ SV *sv, STRLEN *nxp)
{
    MAGIC *mg;

    mg = SvMAGICAL(sv) ? mg_find(sv, 'o') : (MAGIC *) NULL;
    if (!mg || !mg->mg_ptr || *(U32*)mg->mg_ptr != PL_collation_ix) {
	char *s, *xf;
	STRLEN len, xlen;

	if (mg)
	    Safefree(mg->mg_ptr);
	s = SvPV(sv, len);
	if ((xf = mem_collxfrm(s, len, &xlen))) {
	    if (SvREADONLY(sv)) {
		SAVEFREEPV(xf);
		*nxp = xlen;
		return xf + sizeof(PL_collation_ix);
	    }
	    if (! mg) {
		sv_magic(sv, 0, 'o', 0, 0);
		mg = mg_find(sv, 'o');
		assert(mg);
	    }
	    mg->mg_ptr = xf;
	    mg->mg_len = xlen;
	}
	else {
	    if (mg) {
		mg->mg_ptr = NULL;
		mg->mg_len = -1;
	    }
	}
    }
    if (mg && mg->mg_ptr) {
	*nxp = mg->mg_len;
	return mg->mg_ptr + sizeof(PL_collation_ix);
    }
    else {
	*nxp = 0;
	return NULL;
    }
}

#endif /* USE_LOCALE_COLLATE */

/*
=for apidoc sv_gets

Get a line from the filehandle and store it into the SV, optionally
appending to the currently-stored string.

=cut
*/

char *
Perl_sv_gets(pTHX_ register SV *sv, register PerlIO *fp, I32 append)
{
    char *rsptr;
    STRLEN rslen;
    register STDCHAR rslast;
    register STDCHAR *bp;
    register I32 cnt;
    I32 i;

    SV_CHECK_THINKFIRST(sv);
    (void)SvUPGRADE(sv, SVt_PV);

    SvSCREAM_off(sv);

    if (RsSNARF(PL_rs)) {
	rsptr = NULL;
	rslen = 0;
    }
    else if (RsRECORD(PL_rs)) {
      I32 recsize, bytesread;
      char *buffer;

      /* Grab the size of the record we're getting */
      recsize = SvIV(SvRV(PL_rs));
      (void)SvPOK_only(sv);    /* Validate pointer */
      buffer = SvGROW(sv, recsize + 1);
      /* Go yank in */
#ifdef VMS
      /* VMS wants read instead of fread, because fread doesn't respect */
      /* RMS record boundaries. This is not necessarily a good thing to be */
      /* doing, but we've got no other real choice */
      bytesread = PerlLIO_read(PerlIO_fileno(fp), buffer, recsize);
#else
      bytesread = PerlIO_read(fp, buffer, recsize);
#endif
      SvCUR_set(sv, bytesread);
      buffer[bytesread] = '\0';
      SvUTF8_off(sv);
      return(SvCUR(sv) ? SvPVX(sv) : Nullch);
    }
    else if (RsPARA(PL_rs)) {
	rsptr = "\n\n";
	rslen = 2;
    }
    else {
	/* Get $/ i.e. PL_rs into same encoding as stream wants */
	if (SvUTF8(PL_rs)) {
	    if (!sv_utf8_downgrade(PL_rs, TRUE)) {
		Perl_croak(aTHX_ "Wide character in $/");
	    }
	}
	rsptr = SvPV(PL_rs, rslen);
    }

    rslast = rslen ? rsptr[rslen - 1] : '\0';

    if (RsPARA(PL_rs)) {		/* have to do this both before and after */
	do {			/* to make sure file boundaries work right */
	    if (PerlIO_eof(fp))
		return 0;
	    i = PerlIO_getc(fp);
	    if (i != '\n') {
		if (i == -1)
		    return 0;
		PerlIO_ungetc(fp,i);
		break;
	    }
	} while (i != EOF);
    }

    /* See if we know enough about I/O mechanism to cheat it ! */

    /* This used to be #ifdef test - it is made run-time test for ease
       of abstracting out stdio interface. One call should be cheap 
       enough here - and may even be a macro allowing compile
       time optimization.
     */

    if (PerlIO_fast_gets(fp)) {

    /*
     * We're going to steal some values from the stdio struct
     * and put EVERYTHING in the innermost loop into registers.
     */
    register STDCHAR *ptr;
    STRLEN bpx;
    I32 shortbuffered;

#if defined(VMS) && defined(PERLIO_IS_STDIO)
    /* An ungetc()d char is handled separately from the regular
     * buffer, so we getc() it back out and stuff it in the buffer.
     */
    i = PerlIO_getc(fp);
    if (i == EOF) return 0;
    *(--((*fp)->_ptr)) = (unsigned char) i;
    (*fp)->_cnt++;
#endif

    /* Here is some breathtakingly efficient cheating */

    cnt = PerlIO_get_cnt(fp);			/* get count into register */
    (void)SvPOK_only(sv);		/* validate pointer */
    if (SvLEN(sv) - append <= cnt + 1) { /* make sure we have the room */
	if (cnt > 80 && SvLEN(sv) > append) {
	    shortbuffered = cnt - SvLEN(sv) + append + 1;
	    cnt -= shortbuffered;
	}
	else {
	    shortbuffered = 0;
	    /* remember that cnt can be negative */
	    SvGROW(sv, append + (cnt <= 0 ? 2 : (cnt + 1)));
	}
    }
    else
	shortbuffered = 0;
    bp = (STDCHAR*)SvPVX(sv) + append;  /* move these two too to registers */
    ptr = (STDCHAR*)PerlIO_get_ptr(fp);
    DEBUG_P(PerlIO_printf(Perl_debug_log,
	"Screamer: entering, ptr=%"UVuf", cnt=%ld\n",PTR2UV(ptr),(long)cnt));
    DEBUG_P(PerlIO_printf(Perl_debug_log,
	"Screamer: entering: FILE * thinks ptr=%"UVuf", cnt=%ld, base=%"UVuf"\n",
	       PTR2UV(PerlIO_get_ptr(fp)), (long)PerlIO_get_cnt(fp), 
	       PTR2UV(PerlIO_has_base(fp) ? PerlIO_get_base(fp) : 0)));
    for (;;) {
      screamer:
	if (cnt > 0) {
	    if (rslen) {
		while (cnt > 0) {		     /* this     |  eat */
		    cnt--;
		    if ((*bp++ = *ptr++) == rslast)  /* really   |  dust */
			goto thats_all_folks;	     /* screams  |  sed :-) */
		}
	    }
	    else {
	        Copy(ptr, bp, cnt, char);	     /* this     |  eat */    
		bp += cnt;			     /* screams  |  dust */   
		ptr += cnt;			     /* louder   |  sed :-) */
		cnt = 0;
	    }
	}
	
	if (shortbuffered) {		/* oh well, must extend */
	    cnt = shortbuffered;
	    shortbuffered = 0;
	    bpx = bp - (STDCHAR*)SvPVX(sv); /* box up before relocation */
	    SvCUR_set(sv, bpx);
	    SvGROW(sv, SvLEN(sv) + append + cnt + 2);
	    bp = (STDCHAR*)SvPVX(sv) + bpx; /* unbox after relocation */
	    continue;
	}

	DEBUG_P(PerlIO_printf(Perl_debug_log,
			      "Screamer: going to getc, ptr=%"UVuf", cnt=%ld\n",
			      PTR2UV(ptr),(long)cnt));
	PerlIO_set_ptrcnt(fp, ptr, cnt); /* deregisterize cnt and ptr */
	DEBUG_P(PerlIO_printf(Perl_debug_log,
	    "Screamer: pre: FILE * thinks ptr=%"UVuf", cnt=%ld, base=%"UVuf"\n",
	    PTR2UV(PerlIO_get_ptr(fp)), (long)PerlIO_get_cnt(fp), 
	    PTR2UV(PerlIO_has_base (fp) ? PerlIO_get_base(fp) : 0)));
	/* This used to call 'filbuf' in stdio form, but as that behaves like 
	   getc when cnt <= 0 we use PerlIO_getc here to avoid introducing
	   another abstraction.  */
	i   = PerlIO_getc(fp);		/* get more characters */
	DEBUG_P(PerlIO_printf(Perl_debug_log,
	    "Screamer: post: FILE * thinks ptr=%"UVuf", cnt=%ld, base=%"UVuf"\n",
	    PTR2UV(PerlIO_get_ptr(fp)), (long)PerlIO_get_cnt(fp), 
	    PTR2UV(PerlIO_has_base (fp) ? PerlIO_get_base(fp) : 0)));
	cnt = PerlIO_get_cnt(fp);
	ptr = (STDCHAR*)PerlIO_get_ptr(fp);	/* reregisterize cnt and ptr */
	DEBUG_P(PerlIO_printf(Perl_debug_log,
	    "Screamer: after getc, ptr=%"UVuf", cnt=%ld\n",PTR2UV(ptr),(long)cnt));

	if (i == EOF)			/* all done for ever? */
	    goto thats_really_all_folks;

	bpx = bp - (STDCHAR*)SvPVX(sv);	/* box up before relocation */
	SvCUR_set(sv, bpx);
	SvGROW(sv, bpx + cnt + 2);
	bp = (STDCHAR*)SvPVX(sv) + bpx;	/* unbox after relocation */

	*bp++ = i;			/* store character from PerlIO_getc */

	if (rslen && (STDCHAR)i == rslast)  /* all done for now? */
	    goto thats_all_folks;
    }

thats_all_folks:
    if ((rslen > 1 && (bp - (STDCHAR*)SvPVX(sv) < rslen)) ||
	  memNE((char*)bp - rslen, rsptr, rslen))
	goto screamer;				/* go back to the fray */
thats_really_all_folks:
    if (shortbuffered)
	cnt += shortbuffered;
	DEBUG_P(PerlIO_printf(Perl_debug_log,
	    "Screamer: quitting, ptr=%"UVuf", cnt=%ld\n",PTR2UV(ptr),(long)cnt));
    PerlIO_set_ptrcnt(fp, ptr, cnt);	/* put these back or we're in trouble */
    DEBUG_P(PerlIO_printf(Perl_debug_log,
	"Screamer: end: FILE * thinks ptr=%"UVuf", cnt=%ld, base=%"UVuf"\n",
	PTR2UV(PerlIO_get_ptr(fp)), (long)PerlIO_get_cnt(fp), 
	PTR2UV(PerlIO_has_base (fp) ? PerlIO_get_base(fp) : 0)));
    *bp = '\0';
    SvCUR_set(sv, bp - (STDCHAR*)SvPVX(sv));	/* set length */
    DEBUG_P(PerlIO_printf(Perl_debug_log,
	"Screamer: done, len=%ld, string=|%.*s|\n",
	(long)SvCUR(sv),(int)SvCUR(sv),SvPVX(sv)));
    }
   else
    {
#ifndef EPOC
       /*The big, slow, and stupid way */
	STDCHAR buf[8192];
#else
	/* Need to work around EPOC SDK features          */
	/* On WINS: MS VC5 generates calls to _chkstk,    */
	/* if a `large' stack frame is allocated          */
	/* gcc on MARM does not generate calls like these */
	STDCHAR buf[1024];
#endif

screamer2:
	if (rslen) {
	    register STDCHAR *bpe = buf + sizeof(buf);
	    bp = buf;
	    while ((i = PerlIO_getc(fp)) != EOF && (*bp++ = i) != rslast && bp < bpe)
		; /* keep reading */
	    cnt = bp - buf;
	}
	else {
	    cnt = PerlIO_read(fp,(char*)buf, sizeof(buf));
	    /* Accomodate broken VAXC compiler, which applies U8 cast to
	     * both args of ?: operator, causing EOF to change into 255
	     */
	    if (cnt) { i = (U8)buf[cnt - 1]; } else { i = EOF; }
	}

	if (append)
	    sv_catpvn(sv, (char *) buf, cnt);
	else
	    sv_setpvn(sv, (char *) buf, cnt);

	if (i != EOF &&			/* joy */
	    (!rslen ||
	     SvCUR(sv) < rslen ||
	     memNE(SvPVX(sv) + SvCUR(sv) - rslen, rsptr, rslen)))
	{
	    append = -1;
	    /*
	     * If we're reading from a TTY and we get a short read,
	     * indicating that the user hit his EOF character, we need
	     * to notice it now, because if we try to read from the TTY
	     * again, the EOF condition will disappear.
	     *
	     * The comparison of cnt to sizeof(buf) is an optimization
	     * that prevents unnecessary calls to feof().
	     *
	     * - jik 9/25/96
	     */
	    if (!(cnt < sizeof(buf) && PerlIO_eof(fp)))
		goto screamer2;
	}
    }

    if (RsPARA(PL_rs)) {		/* have to do this both before and after */  
        while (i != EOF) {	/* to make sure file boundaries work right */
	    i = PerlIO_getc(fp);
	    if (i != '\n') {
		PerlIO_ungetc(fp,i);
		break;
	    }
	}
    }

    SvUTF8_off(sv);

    return (SvCUR(sv) - append) ? SvPVX(sv) : Nullch;
}


/*
=for apidoc sv_inc

Auto-increment of the value in the SV.

=cut
*/

void
Perl_sv_inc(pTHX_ register SV *sv)
{
    register char *d;
    int flags;

    if (!sv)
	return;
    if (SvGMAGICAL(sv))
	mg_get(sv);
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv)) {
	    if (PL_curcop != &PL_compiling)
		Perl_croak(aTHX_ PL_no_modify);
	}
	if (SvROK(sv)) {
	    IV i;
	    if (SvAMAGIC(sv) && AMG_CALLun(sv,inc))
		return;
	    i = PTR2IV(SvRV(sv));
	    sv_unref(sv);
	    sv_setiv(sv, i);
	}
    }
    flags = SvFLAGS(sv);
    if (flags & SVp_NOK) {
	(void)SvNOK_only(sv);
	SvNVX(sv) += 1.0;
	return;
    }
    if (flags & SVp_IOK) {
	if (SvIsUV(sv)) {
	    if (SvUVX(sv) == UV_MAX)
		sv_setnv(sv, (NV)UV_MAX + 1.0);
	    else
		(void)SvIOK_only_UV(sv);
		++SvUVX(sv);
	} else {
	    if (SvIVX(sv) == IV_MAX)
		sv_setnv(sv, (NV)IV_MAX + 1.0);
	    else {
		(void)SvIOK_only(sv);
		++SvIVX(sv);
	    }	    
	}
	return;
    }
    if (!(flags & SVp_POK) || !*SvPVX(sv)) {
	if ((flags & SVTYPEMASK) < SVt_PVNV)
	    sv_upgrade(sv, SVt_NV);
	SvNVX(sv) = 1.0;
	(void)SvNOK_only(sv);
	return;
    }
    d = SvPVX(sv);
    while (isALPHA(*d)) d++;
    while (isDIGIT(*d)) d++;
    if (*d) {
	sv_setnv(sv,Atof(SvPVX(sv)) + 1.0);  /* punt */
	return;
    }
    d--;
    while (d >= SvPVX(sv)) {
	if (isDIGIT(*d)) {
	    if (++*d <= '9')
		return;
	    *(d--) = '0';
	}
	else {
#ifdef EBCDIC
	    /* MKS: The original code here died if letters weren't consecutive.
	     * at least it didn't have to worry about non-C locales.  The
	     * new code assumes that ('z'-'a')==('Z'-'A'), letters are
	     * arranged in order (although not consecutively) and that only 
	     * [A-Za-z] are accepted by isALPHA in the C locale.
	     */
	    if (*d != 'z' && *d != 'Z') {
		do { ++*d; } while (!isALPHA(*d));
		return;
	    }
	    *(d--) -= 'z' - 'a';
#else
	    ++*d;
	    if (isALPHA(*d))
		return;
	    *(d--) -= 'z' - 'a' + 1;
#endif
	}
    }
    /* oh,oh, the number grew */
    SvGROW(sv, SvCUR(sv) + 2);
    SvCUR(sv)++;
    for (d = SvPVX(sv) + SvCUR(sv); d > SvPVX(sv); d--)
	*d = d[-1];
    if (isDIGIT(d[1]))
	*d = '1';
    else
	*d = d[1];
}

/*
=for apidoc sv_dec

Auto-decrement of the value in the SV.

=cut
*/

void
Perl_sv_dec(pTHX_ register SV *sv)
{
    int flags;

    if (!sv)
	return;
    if (SvGMAGICAL(sv))
	mg_get(sv);
    if (SvTHINKFIRST(sv)) {
	if (SvREADONLY(sv)) {
	    if (PL_curcop != &PL_compiling)
		Perl_croak(aTHX_ PL_no_modify);
	}
	if (SvROK(sv)) {
	    IV i;
	    if (SvAMAGIC(sv) && AMG_CALLun(sv,dec))
		return;
	    i = PTR2IV(SvRV(sv));
	    sv_unref(sv);
	    sv_setiv(sv, i);
	}
    }
    flags = SvFLAGS(sv);
    if (flags & SVp_NOK) {
	SvNVX(sv) -= 1.0;
	(void)SvNOK_only(sv);
	return;
    }
    if (flags & SVp_IOK) {
	if (SvIsUV(sv)) {
	    if (SvUVX(sv) == 0) {
		(void)SvIOK_only(sv);
		SvIVX(sv) = -1;
	    }
	    else {
		(void)SvIOK_only_UV(sv);
		--SvUVX(sv);
	    }	    
	} else {
	    if (SvIVX(sv) == IV_MIN)
		sv_setnv(sv, (NV)IV_MIN - 1.0);
	    else {
		(void)SvIOK_only(sv);
		--SvIVX(sv);
	    }	    
	}
	return;
    }
    if (!(flags & SVp_POK)) {
	if ((flags & SVTYPEMASK) < SVt_PVNV)
	    sv_upgrade(sv, SVt_NV);
	SvNVX(sv) = -1.0;
	(void)SvNOK_only(sv);
	return;
    }
    sv_setnv(sv,Atof(SvPVX(sv)) - 1.0);	/* punt */
}

/*
=for apidoc sv_mortalcopy

Creates a new SV which is a copy of the original SV.  The new SV is marked
as mortal.

=cut
*/

/* Make a string that will exist for the duration of the expression
 * evaluation.  Actually, it may have to last longer than that, but
 * hopefully we won't free it until it has been assigned to a
 * permanent location. */

SV *
Perl_sv_mortalcopy(pTHX_ SV *oldstr)
{
    register SV *sv;

    new_SV(sv);
    sv_setsv(sv,oldstr);
    EXTEND_MORTAL(1);
    PL_tmps_stack[++PL_tmps_ix] = sv;
    SvTEMP_on(sv);
    return sv;
}

/*
=for apidoc sv_newmortal

Creates a new SV which is mortal.  The reference count of the SV is set to 1.

=cut
*/

SV *
Perl_sv_newmortal(pTHX)
{
    register SV *sv;

    new_SV(sv);
    SvFLAGS(sv) = SVs_TEMP;
    EXTEND_MORTAL(1);
    PL_tmps_stack[++PL_tmps_ix] = sv;
    return sv;
}

/*
=for apidoc sv_2mortal

Marks an SV as mortal.  The SV will be destroyed when the current context
ends.

=cut
*/

/* same thing without the copying */

SV *
Perl_sv_2mortal(pTHX_ register SV *sv)
{
    if (!sv)
	return sv;
    if (SvREADONLY(sv) && SvIMMORTAL(sv))
	return sv;
    EXTEND_MORTAL(1);
    PL_tmps_stack[++PL_tmps_ix] = sv;
    SvTEMP_on(sv);
    return sv;
}

/*
=for apidoc newSVpv

Creates a new SV and copies a string into it.  The reference count for the
SV is set to 1.  If C<len> is zero, Perl will compute the length using
strlen().  For efficiency, consider using C<newSVpvn> instead.

=cut
*/

SV *
Perl_newSVpv(pTHX_ const char *s, STRLEN len)
{
    register SV *sv;

    new_SV(sv);
    if (!len)
	len = strlen(s);
    sv_setpvn(sv,s,len);
    return sv;
}

/*
=for apidoc newSVpvn

Creates a new SV and copies a string into it.  The reference count for the
SV is set to 1.  Note that if C<len> is zero, Perl will create a zero length 
string.  You are responsible for ensuring that the source string is at least
C<len> bytes long.

=cut
*/

SV *
Perl_newSVpvn(pTHX_ const char *s, STRLEN len)
{
    register SV *sv;

    new_SV(sv);
    sv_setpvn(sv,s,len);
    return sv;
}

#if defined(PERL_IMPLICIT_CONTEXT)
SV *
Perl_newSVpvf_nocontext(const char* pat, ...)
{
    dTHX;
    register SV *sv;
    va_list args;
    va_start(args, pat);
    sv = vnewSVpvf(pat, &args);
    va_end(args);
    return sv;
}
#endif

/*
=for apidoc newSVpvf

Creates a new SV an initialize it with the string formatted like
C<sprintf>.

=cut
*/

SV *
Perl_newSVpvf(pTHX_ const char* pat, ...)
{
    register SV *sv;
    va_list args;
    va_start(args, pat);
    sv = vnewSVpvf(pat, &args);
    va_end(args);
    return sv;
}

SV *
Perl_vnewSVpvf(pTHX_ const char* pat, va_list* args)
{
    register SV *sv;
    new_SV(sv);
    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    return sv;
}

/*
=for apidoc newSVnv

Creates a new SV and copies a floating point value into it.
The reference count for the SV is set to 1.

=cut
*/

SV *
Perl_newSVnv(pTHX_ NV n)
{
    register SV *sv;

    new_SV(sv);
    sv_setnv(sv,n);
    return sv;
}

/*
=for apidoc newSViv

Creates a new SV and copies an integer into it.  The reference count for the
SV is set to 1.

=cut
*/

SV *
Perl_newSViv(pTHX_ IV i)
{
    register SV *sv;

    new_SV(sv);
    sv_setiv(sv,i);
    return sv;
}

/*
=for apidoc newSVuv

Creates a new SV and copies an unsigned integer into it.
The reference count for the SV is set to 1.

=cut
*/

SV *
Perl_newSVuv(pTHX_ UV u)
{
    register SV *sv;

    new_SV(sv);
    sv_setuv(sv,u);
    return sv;
}

/*
=for apidoc newRV_noinc

Creates an RV wrapper for an SV.  The reference count for the original
SV is B<not> incremented.

=cut
*/

SV *
Perl_newRV_noinc(pTHX_ SV *tmpRef)
{
    register SV *sv;

    new_SV(sv);
    sv_upgrade(sv, SVt_RV);
    SvTEMP_off(tmpRef);
    SvRV(sv) = tmpRef;
    SvROK_on(sv);
    return sv;
}

/* newRV_inc is #defined to newRV in sv.h */
SV *
Perl_newRV(pTHX_ SV *tmpRef)
{
    return newRV_noinc(SvREFCNT_inc(tmpRef));
}

/*
=for apidoc newSVsv

Creates a new SV which is an exact duplicate of the original SV.

=cut
*/

/* make an exact duplicate of old */

SV *
Perl_newSVsv(pTHX_ register SV *old)
{
    register SV *sv;

    if (!old)
	return Nullsv;
    if (SvTYPE(old) == SVTYPEMASK) {
        if (ckWARN_d(WARN_INTERNAL))
	    Perl_warner(aTHX_ WARN_INTERNAL, "semi-panic: attempt to dup freed string");
	return Nullsv;
    }
    new_SV(sv);
    if (SvTEMP(old)) {
	SvTEMP_off(old);
	sv_setsv(sv,old);
	SvTEMP_on(old);
    }
    else
	sv_setsv(sv,old);
    return sv;
}

void
Perl_sv_reset(pTHX_ register char *s, HV *stash)
{
    register HE *entry;
    register GV *gv;
    register SV *sv;
    register I32 i;
    register PMOP *pm;
    register I32 max;
    char todo[PERL_UCHAR_MAX+1];

    if (!stash)
	return;

    if (!*s) {		/* reset ?? searches */
	for (pm = HvPMROOT(stash); pm; pm = pm->op_pmnext) {
	    pm->op_pmdynflags &= ~PMdf_USED;
	}
	return;
    }

    /* reset variables */

    if (!HvARRAY(stash))
	return;

    Zero(todo, 256, char);
    while (*s) {
	i = (unsigned char)*s;
	if (s[1] == '-') {
	    s += 2;
	}
	max = (unsigned char)*s++;
	for ( ; i <= max; i++) {
	    todo[i] = 1;
	}
	for (i = 0; i <= (I32) HvMAX(stash); i++) {
	    for (entry = HvARRAY(stash)[i];
		 entry;
		 entry = HeNEXT(entry))
	    {
		if (!todo[(U8)*HeKEY(entry)])
		    continue;
		gv = (GV*)HeVAL(entry);
		sv = GvSV(gv);
		if (SvTHINKFIRST(sv)) {
		    if (!SvREADONLY(sv) && SvROK(sv))
			sv_unref(sv);
		    continue;
		}
		(void)SvOK_off(sv);
		if (SvTYPE(sv) >= SVt_PV) {
		    SvCUR_set(sv, 0);
		    if (SvPVX(sv) != Nullch)
			*SvPVX(sv) = '\0';
		    SvTAINT(sv);
		}
		if (GvAV(gv)) {
		    av_clear(GvAV(gv));
		}
		if (GvHV(gv) && !HvNAME(GvHV(gv))) {
		    hv_clear(GvHV(gv));
#ifdef USE_ENVIRON_ARRAY
		    if (gv == PL_envgv)
			environ[0] = Nullch;
#endif
		}
	    }
	}
    }
}

IO*
Perl_sv_2io(pTHX_ SV *sv)
{
    IO* io;
    GV* gv;
    STRLEN n_a;

    switch (SvTYPE(sv)) {
    case SVt_PVIO:
	io = (IO*)sv;
	break;
    case SVt_PVGV:
	gv = (GV*)sv;
	io = GvIO(gv);
	if (!io)
	    Perl_croak(aTHX_ "Bad filehandle: %s", GvNAME(gv));
	break;
    default:
	if (!SvOK(sv))
	    Perl_croak(aTHX_ PL_no_usym, "filehandle");
	if (SvROK(sv))
	    return sv_2io(SvRV(sv));
	gv = gv_fetchpv(SvPV(sv,n_a), FALSE, SVt_PVIO);
	if (gv)
	    io = GvIO(gv);
	else
	    io = 0;
	if (!io)
	    Perl_croak(aTHX_ "Bad filehandle: %s", SvPV(sv,n_a));
	break;
    }
    return io;
}

CV *
Perl_sv_2cv(pTHX_ SV *sv, HV **st, GV **gvp, I32 lref)
{
    GV *gv;
    CV *cv;
    STRLEN n_a;

    if (!sv)
	return *gvp = Nullgv, Nullcv;
    switch (SvTYPE(sv)) {
    case SVt_PVCV:
	*st = CvSTASH(sv);
	*gvp = Nullgv;
	return (CV*)sv;
    case SVt_PVHV:
    case SVt_PVAV:
	*gvp = Nullgv;
	return Nullcv;
    case SVt_PVGV:
	gv = (GV*)sv;
	*gvp = gv;
	*st = GvESTASH(gv);
	goto fix_gv;

    default:
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvROK(sv)) {
	    SV **sp = &sv;		/* Used in tryAMAGICunDEREF macro. */
	    tryAMAGICunDEREF(to_cv);

	    sv = SvRV(sv);
	    if (SvTYPE(sv) == SVt_PVCV) {
		cv = (CV*)sv;
		*gvp = Nullgv;
		*st = CvSTASH(cv);
		return cv;
	    }
	    else if(isGV(sv))
		gv = (GV*)sv;
	    else
		Perl_croak(aTHX_ "Not a subroutine reference");
	}
	else if (isGV(sv))
	    gv = (GV*)sv;
	else
	    gv = gv_fetchpv(SvPV(sv, n_a), lref, SVt_PVCV);
	*gvp = gv;
	if (!gv)
	    return Nullcv;
	*st = GvESTASH(gv);
    fix_gv:
	if (lref && !GvCVu(gv)) {
	    SV *tmpsv;
	    ENTER;
	    tmpsv = NEWSV(704,0);
	    gv_efullname3(tmpsv, gv, Nullch);
	    /* XXX this is probably not what they think they're getting.
	     * It has the same effect as "sub name;", i.e. just a forward
	     * declaration! */
	    newSUB(start_subparse(FALSE, 0),
		   newSVOP(OP_CONST, 0, tmpsv),
		   Nullop,
		   Nullop);
	    LEAVE;
	    if (!GvCVu(gv))
		Perl_croak(aTHX_ "Unable to create sub named \"%s\"", SvPV(sv,n_a));
	}
	return GvCVu(gv);
    }
}

/*
=for apidoc sv_true

Returns true if the SV has a true value by Perl's rules.

=cut
*/

I32
Perl_sv_true(pTHX_ register SV *sv)
{
    if (!sv)
	return 0;
    if (SvPOK(sv)) {
	register XPV* tXpv;
	if ((tXpv = (XPV*)SvANY(sv)) &&
		(tXpv->xpv_cur > 1 ||
		(tXpv->xpv_cur && *tXpv->xpv_pv != '0')))
	    return 1;
	else
	    return 0;
    }
    else {
	if (SvIOK(sv))
	    return SvIVX(sv) != 0;
	else {
	    if (SvNOK(sv))
		return SvNVX(sv) != 0.0;
	    else
		return sv_2bool(sv);
	}
    }
}

IV
Perl_sv_iv(pTHX_ register SV *sv)
{
    if (SvIOK(sv)) {
	if (SvIsUV(sv))
	    return (IV)SvUVX(sv);
	return SvIVX(sv);
    }
    return sv_2iv(sv);
}

UV
Perl_sv_uv(pTHX_ register SV *sv)
{
    if (SvIOK(sv)) {
	if (SvIsUV(sv))
	    return SvUVX(sv);
	return (UV)SvIVX(sv);
    }
    return sv_2uv(sv);
}

NV
Perl_sv_nv(pTHX_ register SV *sv)
{
    if (SvNOK(sv))
	return SvNVX(sv);
    return sv_2nv(sv);
}

char *
Perl_sv_pv(pTHX_ SV *sv)
{
    STRLEN n_a;

    if (SvPOK(sv))
	return SvPVX(sv);

    return sv_2pv(sv, &n_a);
}

char *
Perl_sv_pvn(pTHX_ SV *sv, STRLEN *lp)
{
    if (SvPOK(sv)) {
	*lp = SvCUR(sv);
	return SvPVX(sv);
    }
    return sv_2pv(sv, lp);
}

/*
=for apidoc sv_pvn_force

Get a sensible string out of the SV somehow.

=cut
*/

char *
Perl_sv_pvn_force(pTHX_ SV *sv, STRLEN *lp)
{
    char *s;

    if (SvTHINKFIRST(sv) && !SvROK(sv))
	sv_force_normal(sv);
    
    if (SvPOK(sv)) {
	*lp = SvCUR(sv);
    }
    else {
	if (SvTYPE(sv) > SVt_PVLV && SvTYPE(sv) != SVt_PVFM) {
	    Perl_croak(aTHX_ "Can't coerce %s to string in %s", sv_reftype(sv,0),
		PL_op_name[PL_op->op_type]);
	}
	else
	    s = sv_2pv(sv, lp);
	if (s != SvPVX(sv)) {	/* Almost, but not quite, sv_setpvn() */
	    STRLEN len = *lp;
	    
	    if (SvROK(sv))
		sv_unref(sv);
	    (void)SvUPGRADE(sv, SVt_PV);		/* Never FALSE */
	    SvGROW(sv, len + 1);
	    Move(s,SvPVX(sv),len,char);
	    SvCUR_set(sv, len);
	    *SvEND(sv) = '\0';
	}
	if (!SvPOK(sv)) {
	    SvPOK_on(sv);		/* validate pointer */
	    SvTAINT(sv);
	    DEBUG_c(PerlIO_printf(Perl_debug_log, "0x%"UVxf" 2pv(%s)\n",
				  PTR2UV(sv),SvPVX(sv)));
	}
    }
    return SvPVX(sv);
}

char *
Perl_sv_pvbyte(pTHX_ SV *sv)
{
    return sv_pv(sv);
}

char *
Perl_sv_pvbyten(pTHX_ SV *sv, STRLEN *lp)
{
    return sv_pvn(sv,lp);
}

char *
Perl_sv_pvbyten_force(pTHX_ SV *sv, STRLEN *lp)
{
    return sv_pvn_force(sv,lp);
}

char *
Perl_sv_pvutf8(pTHX_ SV *sv)
{
    sv_utf8_upgrade(sv);
    return sv_pv(sv);
}

char *
Perl_sv_pvutf8n(pTHX_ SV *sv, STRLEN *lp)
{
    sv_utf8_upgrade(sv);
    return sv_pvn(sv,lp);
}

/*
=for apidoc sv_pvutf8n_force

Get a sensible UTF8-encoded string out of the SV somehow. See
L</sv_pvn_force>.

=cut
*/

char *
Perl_sv_pvutf8n_force(pTHX_ SV *sv, STRLEN *lp)
{
    sv_utf8_upgrade(sv);
    return sv_pvn_force(sv,lp);
}

/*
=for apidoc sv_reftype

Returns a string describing what the SV is a reference to.

=cut
*/

char *
Perl_sv_reftype(pTHX_ SV *sv, int ob)
{
    if (ob && SvOBJECT(sv))
	return HvNAME(SvSTASH(sv));
    else {
	switch (SvTYPE(sv)) {
	case SVt_NULL:
	case SVt_IV:
	case SVt_NV:
	case SVt_RV:
	case SVt_PV:
	case SVt_PVIV:
	case SVt_PVNV:
	case SVt_PVMG:
	case SVt_PVBM:
				if (SvROK(sv))
				    return "REF";
				else
				    return "SCALAR";
	case SVt_PVLV:		return "LVALUE";
	case SVt_PVAV:		return "ARRAY";
	case SVt_PVHV:		return "HASH";
	case SVt_PVCV:		return "CODE";
	case SVt_PVGV:		return "GLOB";
	case SVt_PVFM:		return "FORMAT";
	case SVt_PVIO:		return "IO";
	default:		return "UNKNOWN";
	}
    }
}

/*
=for apidoc sv_isobject

Returns a boolean indicating whether the SV is an RV pointing to a blessed
object.  If the SV is not an RV, or if the object is not blessed, then this
will return false.

=cut
*/

int
Perl_sv_isobject(pTHX_ SV *sv)
{
    if (!sv)
	return 0;
    if (SvGMAGICAL(sv))
	mg_get(sv);
    if (!SvROK(sv))
	return 0;
    sv = (SV*)SvRV(sv);
    if (!SvOBJECT(sv))
	return 0;
    return 1;
}

/*
=for apidoc sv_isa

Returns a boolean indicating whether the SV is blessed into the specified
class.  This does not check for subtypes; use C<sv_derived_from> to verify
an inheritance relationship.

=cut
*/

int
Perl_sv_isa(pTHX_ SV *sv, const char *name)
{
    if (!sv)
	return 0;
    if (SvGMAGICAL(sv))
	mg_get(sv);
    if (!SvROK(sv))
	return 0;
    sv = (SV*)SvRV(sv);
    if (!SvOBJECT(sv))
	return 0;

    return strEQ(HvNAME(SvSTASH(sv)), name);
}

/*
=for apidoc newSVrv

Creates a new SV for the RV, C<rv>, to point to.  If C<rv> is not an RV then
it will be upgraded to one.  If C<classname> is non-null then the new SV will
be blessed in the specified package.  The new SV is returned and its
reference count is 1.

=cut
*/

SV*
Perl_newSVrv(pTHX_ SV *rv, const char *classname)
{
    SV *sv;

    new_SV(sv);

    SV_CHECK_THINKFIRST(rv);
    SvAMAGIC_off(rv);

    if (SvTYPE(rv) >= SVt_PVMG) {
	U32 refcnt = SvREFCNT(rv);
	SvREFCNT(rv) = 0;
	sv_clear(rv);
	SvFLAGS(rv) = 0;
	SvREFCNT(rv) = refcnt;
    }

    if (SvTYPE(rv) < SVt_RV)
	sv_upgrade(rv, SVt_RV);
    else if (SvTYPE(rv) > SVt_RV) {
	(void)SvOOK_off(rv);
	if (SvPVX(rv) && SvLEN(rv))
	    Safefree(SvPVX(rv));
	SvCUR_set(rv, 0);
	SvLEN_set(rv, 0);
    }

    (void)SvOK_off(rv);
    SvRV(rv) = sv;
    SvROK_on(rv);

    if (classname) {
	HV* stash = gv_stashpv(classname, TRUE);
	(void)sv_bless(rv, stash);
    }
    return sv;
}

/*
=for apidoc sv_setref_pv

Copies a pointer into a new SV, optionally blessing the SV.  The C<rv>
argument will be upgraded to an RV.  That RV will be modified to point to
the new SV.  If the C<pv> argument is NULL then C<PL_sv_undef> will be placed
into the SV.  The C<classname> argument indicates the package for the
blessing.  Set C<classname> to C<Nullch> to avoid the blessing.  The new SV
will be returned and will have a reference count of 1.

Do not use with other Perl types such as HV, AV, SV, CV, because those
objects will become corrupted by the pointer copy process.

Note that C<sv_setref_pvn> copies the string while this copies the pointer.

=cut
*/

SV*
Perl_sv_setref_pv(pTHX_ SV *rv, const char *classname, void *pv)
{
    if (!pv) {
	sv_setsv(rv, &PL_sv_undef);
	SvSETMAGIC(rv);
    }
    else
	sv_setiv(newSVrv(rv,classname), PTR2IV(pv));
    return rv;
}

/*
=for apidoc sv_setref_iv

Copies an integer into a new SV, optionally blessing the SV.  The C<rv>
argument will be upgraded to an RV.  That RV will be modified to point to
the new SV.  The C<classname> argument indicates the package for the
blessing.  Set C<classname> to C<Nullch> to avoid the blessing.  The new SV
will be returned and will have a reference count of 1.

=cut
*/

SV*
Perl_sv_setref_iv(pTHX_ SV *rv, const char *classname, IV iv)
{
    sv_setiv(newSVrv(rv,classname), iv);
    return rv;
}

/*
=for apidoc sv_setref_nv

Copies a double into a new SV, optionally blessing the SV.  The C<rv>
argument will be upgraded to an RV.  That RV will be modified to point to
the new SV.  The C<classname> argument indicates the package for the
blessing.  Set C<classname> to C<Nullch> to avoid the blessing.  The new SV
will be returned and will have a reference count of 1.

=cut
*/

SV*
Perl_sv_setref_nv(pTHX_ SV *rv, const char *classname, NV nv)
{
    sv_setnv(newSVrv(rv,classname), nv);
    return rv;
}

/*
=for apidoc sv_setref_pvn

Copies a string into a new SV, optionally blessing the SV.  The length of the
string must be specified with C<n>.  The C<rv> argument will be upgraded to
an RV.  That RV will be modified to point to the new SV.  The C<classname>
argument indicates the package for the blessing.  Set C<classname> to
C<Nullch> to avoid the blessing.  The new SV will be returned and will have
a reference count of 1.

Note that C<sv_setref_pv> copies the pointer while this copies the string.

=cut
*/

SV*
Perl_sv_setref_pvn(pTHX_ SV *rv, const char *classname, char *pv, STRLEN n)
{
    sv_setpvn(newSVrv(rv,classname), pv, n);
    return rv;
}

/*
=for apidoc sv_bless

Blesses an SV into a specified package.  The SV must be an RV.  The package
must be designated by its stash (see C<gv_stashpv()>).  The reference count
of the SV is unaffected.

=cut
*/

SV*
Perl_sv_bless(pTHX_ SV *sv, HV *stash)
{
    SV *tmpRef;
    if (!SvROK(sv))
        Perl_croak(aTHX_ "Can't bless non-reference value");
    tmpRef = SvRV(sv);
    if (SvFLAGS(tmpRef) & (SVs_OBJECT|SVf_READONLY)) {
	if (SvREADONLY(tmpRef))
	    Perl_croak(aTHX_ PL_no_modify);
	if (SvOBJECT(tmpRef)) {
	    if (SvTYPE(tmpRef) != SVt_PVIO)
		--PL_sv_objcount;
	    SvREFCNT_dec(SvSTASH(tmpRef));
	}
    }
    SvOBJECT_on(tmpRef);
    if (SvTYPE(tmpRef) != SVt_PVIO)
	++PL_sv_objcount;
    (void)SvUPGRADE(tmpRef, SVt_PVMG);
    SvSTASH(tmpRef) = (HV*)SvREFCNT_inc(stash);

    if (Gv_AMG(stash))
	SvAMAGIC_on(sv);
    else
	SvAMAGIC_off(sv);

    return sv;
}

STATIC void
S_sv_unglob(pTHX_ SV *sv)
{
    void *xpvmg;

    assert(SvTYPE(sv) == SVt_PVGV);
    SvFAKE_off(sv);
    if (GvGP(sv))
	gp_free((GV*)sv);
    if (GvSTASH(sv)) {
	SvREFCNT_dec(GvSTASH(sv));
	GvSTASH(sv) = Nullhv;
    }
    sv_unmagic(sv, '*');
    Safefree(GvNAME(sv));
    GvMULTI_off(sv);

    /* need to keep SvANY(sv) in the right arena */
    xpvmg = new_XPVMG();
    StructCopy(SvANY(sv), xpvmg, XPVMG);
    del_XPVGV(SvANY(sv));
    SvANY(sv) = xpvmg;

    SvFLAGS(sv) &= ~SVTYPEMASK;
    SvFLAGS(sv) |= SVt_PVMG;
}

/*
=for apidoc sv_unref

Unsets the RV status of the SV, and decrements the reference count of
whatever was being referenced by the RV.  This can almost be thought of
as a reversal of C<newSVrv>.  See C<SvROK_off>.

=cut
*/

void
Perl_sv_unref(pTHX_ SV *sv)
{
    SV* rv = SvRV(sv);

    if (SvWEAKREF(sv)) {
    	sv_del_backref(sv);
	SvWEAKREF_off(sv);
	SvRV(sv) = 0;
	return;
    }
    SvRV(sv) = 0;
    SvROK_off(sv);
    if (SvREFCNT(rv) != 1 || SvREADONLY(rv))
	SvREFCNT_dec(rv);
    else
	sv_2mortal(rv);		/* Schedule for freeing later */
}

void
Perl_sv_taint(pTHX_ SV *sv)
{
    sv_magic((sv), Nullsv, 't', Nullch, 0);
}

void
Perl_sv_untaint(pTHX_ SV *sv)
{
    if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv)) {
	MAGIC *mg = mg_find(sv, 't');
	if (mg)
	    mg->mg_len &= ~1;
    }
}

bool
Perl_sv_tainted(pTHX_ SV *sv)
{
    if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv)) {
	MAGIC *mg = mg_find(sv, 't');
	if (mg && ((mg->mg_len & 1) || ((mg->mg_len & 2) && mg->mg_obj == sv)))
	    return TRUE;
    }
    return FALSE;
}

/*
=for apidoc sv_setpviv

Copies an integer into the given SV, also updating its string value.
Does not handle 'set' magic.  See C<sv_setpviv_mg>.

=cut
*/

void
Perl_sv_setpviv(pTHX_ SV *sv, IV iv)
{
    char buf[TYPE_CHARS(UV)];
    char *ebuf;
    char *ptr = uiv_2buf(buf, iv, 0, 0, &ebuf);

    sv_setpvn(sv, ptr, ebuf - ptr);
}


/*
=for apidoc sv_setpviv_mg

Like C<sv_setpviv>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_setpviv_mg(pTHX_ SV *sv, IV iv)
{
    char buf[TYPE_CHARS(UV)];
    char *ebuf;
    char *ptr = uiv_2buf(buf, iv, 0, 0, &ebuf);

    sv_setpvn(sv, ptr, ebuf - ptr);
    SvSETMAGIC(sv);
}

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_sv_setpvf_nocontext(SV *sv, const char* pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    sv_vsetpvf(sv, pat, &args);
    va_end(args);
}


void
Perl_sv_setpvf_mg_nocontext(SV *sv, const char* pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    sv_vsetpvf_mg(sv, pat, &args);
    va_end(args);
}
#endif

/*
=for apidoc sv_setpvf

Processes its arguments like C<sprintf> and sets an SV to the formatted
output.  Does not handle 'set' magic.  See C<sv_setpvf_mg>.

=cut
*/

void
Perl_sv_setpvf(pTHX_ SV *sv, const char* pat, ...)
{
    va_list args;
    va_start(args, pat);
    sv_vsetpvf(sv, pat, &args);
    va_end(args);
}

void
Perl_sv_vsetpvf(pTHX_ SV *sv, const char* pat, va_list* args)
{
    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
}

/*
=for apidoc sv_setpvf_mg

Like C<sv_setpvf>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_setpvf_mg(pTHX_ SV *sv, const char* pat, ...)
{
    va_list args;
    va_start(args, pat);
    sv_vsetpvf_mg(sv, pat, &args);
    va_end(args);
}

void
Perl_sv_vsetpvf_mg(pTHX_ SV *sv, const char* pat, va_list* args)
{
    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    SvSETMAGIC(sv);
}

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_sv_catpvf_nocontext(SV *sv, const char* pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    sv_vcatpvf(sv, pat, &args);
    va_end(args);
}

void
Perl_sv_catpvf_mg_nocontext(SV *sv, const char* pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    sv_vcatpvf_mg(sv, pat, &args);
    va_end(args);
}
#endif

/*
=for apidoc sv_catpvf

Processes its arguments like C<sprintf> and appends the formatted output
to an SV.  Handles 'get' magic, but not 'set' magic.  C<SvSETMAGIC()> must
typically be called after calling this function to handle 'set' magic.

=cut
*/

void
Perl_sv_catpvf(pTHX_ SV *sv, const char* pat, ...)
{
    va_list args;
    va_start(args, pat);
    sv_vcatpvf(sv, pat, &args);
    va_end(args);
}

void
Perl_sv_vcatpvf(pTHX_ SV *sv, const char* pat, va_list* args)
{
    sv_vcatpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
}

/*
=for apidoc sv_catpvf_mg

Like C<sv_catpvf>, but also handles 'set' magic.

=cut
*/

void
Perl_sv_catpvf_mg(pTHX_ SV *sv, const char* pat, ...)
{
    va_list args;
    va_start(args, pat);
    sv_vcatpvf_mg(sv, pat, &args);
    va_end(args);
}

void
Perl_sv_vcatpvf_mg(pTHX_ SV *sv, const char* pat, va_list* args)
{
    sv_vcatpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    SvSETMAGIC(sv);
}

/*
=for apidoc sv_vsetpvfn

Works like C<vcatpvfn> but copies the text into the SV instead of
appending it.

=cut
*/

void
Perl_sv_vsetpvfn(pTHX_ SV *sv, const char *pat, STRLEN patlen, va_list *args, SV **svargs, I32 svmax, bool *maybe_tainted)
{
    sv_setpvn(sv, "", 0);
    sv_vcatpvfn(sv, pat, patlen, args, svargs, svmax, maybe_tainted);
}

/*
=for apidoc sv_vcatpvfn

Processes its arguments like C<vsprintf> and appends the formatted output
to an SV.  Uses an array of SVs if the C style variable argument list is
missing (NULL).  When running with taint checks enabled, indicates via
C<maybe_tainted> if results are untrustworthy (often due to the use of
locales).

=cut
*/

void
Perl_sv_vcatpvfn(pTHX_ SV *sv, const char *pat, STRLEN patlen, va_list *args, SV **svargs, I32 svmax, bool *maybe_tainted)
{
    char *p;
    char *q;
    char *patend;
    STRLEN origlen;
    I32 svix = 0;
    static char nullstr[] = "(null)";
    SV *argsv;

    /* no matter what, this is a string now */
    (void)SvPV_force(sv, origlen);

    /* special-case "", "%s", and "%_" */
    if (patlen == 0)
	return;
    if (patlen == 2 && pat[0] == '%') {
	switch (pat[1]) {
	case 's':
	    if (args) {
		char *s = va_arg(*args, char*);
		sv_catpv(sv, s ? s : nullstr);
	    }
	    else if (svix < svmax) {
		sv_catsv(sv, *svargs);
		if (DO_UTF8(*svargs))
		    SvUTF8_on(sv);
	    }
	    return;
	case '_':
	    if (args) {
		argsv = va_arg(*args, SV*);
		sv_catsv(sv, argsv);
		if (DO_UTF8(argsv))
		    SvUTF8_on(sv);
		return;
	    }
	    /* See comment on '_' below */
	    break;
	}
    }

    patend = (char*)pat + patlen;
    for (p = (char*)pat; p < patend; p = q) {
	bool alt = FALSE;
	bool left = FALSE;
	bool vectorize = FALSE;
	bool utf = FALSE;
	char fill = ' ';
	char plus = 0;
	char intsize = 0;
	STRLEN width = 0;
	STRLEN zeros = 0;
	bool has_precis = FALSE;
	STRLEN precis = 0;
	bool is_utf = FALSE;

	char esignbuf[4];
	U8 utf8buf[UTF8_MAXLEN+1];
	STRLEN esignlen = 0;

	char *eptr = Nullch;
	STRLEN elen = 0;
	/* Times 4: a decimal digit takes more than 3 binary digits.
	 * NV_DIG: mantissa takes than many decimal digits.
	 * Plus 32: Playing safe. */
	char ebuf[IV_DIG * 4 + NV_DIG + 32];
        /* large enough for "%#.#f" --chip */
	/* what about long double NVs? --jhi */

	SV *vecsv;
	U8 *vecstr = Null(U8*);
	STRLEN veclen = 0;
	char c;
	int i;
	unsigned base;
	IV iv;
	UV uv;
	NV nv;
	STRLEN have;
	STRLEN need;
	STRLEN gap;
	char *dotstr = ".";
	STRLEN dotstrlen = 1;

	for (q = p; q < patend && *q != '%'; ++q) ;
	if (q > p) {
	    sv_catpvn(sv, p, q - p);
	    p = q;
	}
	if (q++ >= patend)
	    break;

	/* FLAGS */

	while (*q) {
	    switch (*q) {
	    case ' ':
	    case '+':
		plus = *q++;
		continue;

	    case '-':
		left = TRUE;
		q++;
		continue;

	    case '0':
		fill = *q++;
		continue;

	    case '#':
		alt = TRUE;
		q++;
		continue;

	    case '*':			/* printf("%*vX",":",$ipv6addr) */
		if (q[1] != 'v')
		    break;
		q++;
		if (args)
		    vecsv = va_arg(*args, SV*);
		else if (svix < svmax)
		    vecsv = svargs[svix++];
		else
		    continue;
		dotstr = SvPVx(vecsv,dotstrlen);
		if (DO_UTF8(vecsv))
		    is_utf = TRUE;
		/* FALL THROUGH */

	    case 'v':
		vectorize = TRUE;
		q++;
		continue;

	    default:
		break;
	    }
	    break;
	}

	/* WIDTH */

	switch (*q) {
	case '1': case '2': case '3':
	case '4': case '5': case '6':
	case '7': case '8': case '9':
	    width = 0;
	    while (isDIGIT(*q))
		width = width * 10 + (*q++ - '0');
	    break;

	case '*':
	    if (args)
		i = va_arg(*args, int);
	    else
		i = (svix < svmax) ? SvIVx(svargs[svix++]) : 0;
	    left |= (i < 0);
	    width = (i < 0) ? -i : i;
	    q++;
	    break;
	}

	/* PRECISION */

	if (*q == '.') {
	    q++;
	    if (*q == '*') {
		if (args)
		    i = va_arg(*args, int);
		else
		    i = (svix < svmax) ? SvIVx(svargs[svix++]) : 0;
		precis = (i < 0) ? 0 : i;
		q++;
	    }
	    else {
		precis = 0;
		while (isDIGIT(*q))
		    precis = precis * 10 + (*q++ - '0');
	    }
	    has_precis = TRUE;
	}

	if (vectorize) {
	    if (args) {
		vecsv = va_arg(*args, SV*);
		vecstr = (U8*)SvPVx(vecsv,veclen);
		utf = DO_UTF8(vecsv);
	    }
	    else if (svix < svmax) {
		vecsv = svargs[svix++];
		vecstr = (U8*)SvPVx(vecsv,veclen);
		utf = DO_UTF8(vecsv);
	    }
	    else {
		vecstr = (U8*)"";
		veclen = 0;
	    }
	}

	/* SIZE */

	switch (*q) {
#if defined(HAS_QUAD) || (defined(HAS_LONG_DOUBLE) && defined(USE_LONG_DOUBLE))
	case 'L':			/* Ld */
	    /* FALL THROUGH */
#endif
#ifdef HAS_QUAD
	case 'q':			/* qd */
	    intsize = 'q';
	    q++;
	    break;
#endif
	case 'l':
#if defined(HAS_QUAD) || (defined(HAS_LONG_DOUBLE) && defined(USE_LONG_DOUBLE))
             if (*(q + 1) == 'l') {	/* lld, llf */
		intsize = 'q';
		q += 2;
		break;
	     }
#endif
	    /* FALL THROUGH */
	case 'h':
	    /* FALL THROUGH */
	case 'V':
	    intsize = *q++;
	    break;
	}

	/* CONVERSION */

	switch (c = *q++) {

	    /* STRINGS */

	case '%':
	    eptr = q - 1;
	    elen = 1;
	    goto string;

	case 'c':
	    if (args)
		uv = va_arg(*args, int);
	    else
		uv = (svix < svmax) ? SvIVx(svargs[svix++]) : 0;
	    if ((uv > 255 || (uv > 127 && SvUTF8(sv))) && !IN_BYTE) {
		eptr = (char*)utf8buf;
		elen = uv_to_utf8((U8*)eptr, uv) - utf8buf;
		is_utf = TRUE;
	    }
	    else {
		c = (char)uv;
		eptr = &c;
		elen = 1;
	    }
	    goto string;

	case 's':
	    if (args) {
		eptr = va_arg(*args, char*);
		if (eptr)
#ifdef MACOS_TRADITIONAL
		  /* On MacOS, %#s format is used for Pascal strings */
		  if (alt)
		    elen = *eptr++;
		  else
#endif
		    elen = strlen(eptr);
		else {
		    eptr = nullstr;
		    elen = sizeof nullstr - 1;
		}
	    }
	    else if (svix < svmax) {
		argsv = svargs[svix++];
		eptr = SvPVx(argsv, elen);
		if (DO_UTF8(argsv)) {
		    if (has_precis && precis < elen) {
			I32 p = precis;
			sv_pos_u2b(argsv, &p, 0); /* sticks at end */
			precis = p;
		    }
		    if (width) { /* fudge width (can't fudge elen) */
			width += elen - sv_len_utf8(argsv);
		    }
		    is_utf = TRUE;
		}
	    }
	    goto string;

	case '_':
	    /*
	     * The "%_" hack might have to be changed someday,
	     * if ISO or ANSI decide to use '_' for something.
	     * So we keep it hidden from users' code.
	     */
	    if (!args)
		goto unknown;
	    argsv = va_arg(*args,SV*);
	    eptr = SvPVx(argsv, elen);
	    if (DO_UTF8(argsv))
		is_utf = TRUE;

	string:
	    vectorize = FALSE;
	    if (has_precis && elen > precis)
		elen = precis;
	    break;

	    /* INTEGERS */

	case 'p':
	    if (alt)
		goto unknown;
	    if (args)
		uv = PTR2UV(va_arg(*args, void*));
	    else
		uv = (svix < svmax) ? PTR2UV(svargs[svix++]) : 0;
	    base = 16;
	    goto integer;

	case 'D':
#ifdef IV_IS_QUAD
	    intsize = 'q';
#else
	    intsize = 'l';
#endif
	    /* FALL THROUGH */
	case 'd':
	case 'i':
	    if (vectorize) {
		STRLEN ulen;
		if (!veclen) {
		    vectorize = FALSE;
		    break;
		}
		if (utf)
		    iv = (IV)utf8_to_uv(vecstr, veclen, &ulen, 0);
		else {
		    iv = *vecstr;
		    ulen = 1;
		}
		vecstr += ulen;
		veclen -= ulen;
	    }
	    else if (args) {
		switch (intsize) {
		case 'h':	iv = (short)va_arg(*args, int); break;
		default:	iv = va_arg(*args, int); break;
		case 'l':	iv = va_arg(*args, long); break;
		case 'V':	iv = va_arg(*args, IV); break;
#ifdef HAS_QUAD
		case 'q':	iv = va_arg(*args, Quad_t); break;
#endif
		}
	    }
	    else {
		iv = (svix < svmax) ? SvIVx(svargs[svix++]) : 0;
		switch (intsize) {
		case 'h':	iv = (short)iv; break;
		default:	break;
		case 'l':	iv = (long)iv; break;
		case 'V':	break;
#ifdef HAS_QUAD
		case 'q':	iv = (Quad_t)iv; break;
#endif
		}
	    }
	    if (iv >= 0) {
		uv = iv;
		if (plus)
		    esignbuf[esignlen++] = plus;
	    }
	    else {
		uv = -iv;
		esignbuf[esignlen++] = '-';
	    }
	    base = 10;
	    goto integer;

	case 'U':
#ifdef IV_IS_QUAD
	    intsize = 'q';
#else
	    intsize = 'l';
#endif
	    /* FALL THROUGH */
	case 'u':
	    base = 10;
	    goto uns_integer;

	case 'b':
	    base = 2;
	    goto uns_integer;

	case 'O':
#ifdef IV_IS_QUAD
	    intsize = 'q';
#else
	    intsize = 'l';
#endif
	    /* FALL THROUGH */
	case 'o':
	    base = 8;
	    goto uns_integer;

	case 'X':
	case 'x':
	    base = 16;

	uns_integer:
	    if (vectorize) {
		STRLEN ulen;
	vector:
		if (!veclen) {
		    vectorize = FALSE;
		    break;
		}
		if (utf)
		    uv = utf8_to_uv(vecstr, veclen, &ulen, 0);
		else {
		    uv = *vecstr;
		    ulen = 1;
		}
		vecstr += ulen;
		veclen -= ulen;
	    }
	    else if (args) {
		switch (intsize) {
		case 'h':  uv = (unsigned short)va_arg(*args, unsigned); break;
		default:   uv = va_arg(*args, unsigned); break;
		case 'l':  uv = va_arg(*args, unsigned long); break;
		case 'V':  uv = va_arg(*args, UV); break;
#ifdef HAS_QUAD
		case 'q':  uv = va_arg(*args, Quad_t); break;
#endif
		}
	    }
	    else {
		uv = (svix < svmax) ? SvUVx(svargs[svix++]) : 0;
		switch (intsize) {
		case 'h':	uv = (unsigned short)uv; break;
		default:	break;
		case 'l':	uv = (unsigned long)uv; break;
		case 'V':	break;
#ifdef HAS_QUAD
		case 'q':	uv = (Quad_t)uv; break;
#endif
		}
	    }

	integer:
	    eptr = ebuf + sizeof ebuf;
	    switch (base) {
		unsigned dig;
	    case 16:
		if (!uv)
		    alt = FALSE;
		p = (char*)((c == 'X')
			    ? "0123456789ABCDEF" : "0123456789abcdef");
		do {
		    dig = uv & 15;
		    *--eptr = p[dig];
		} while (uv >>= 4);
		if (alt) {
		    esignbuf[esignlen++] = '0';
		    esignbuf[esignlen++] = c;  /* 'x' or 'X' */
		}
		break;
	    case 8:
		do {
		    dig = uv & 7;
		    *--eptr = '0' + dig;
		} while (uv >>= 3);
		if (alt && *eptr != '0')
		    *--eptr = '0';
		break;
	    case 2:
		do {
		    dig = uv & 1;
		    *--eptr = '0' + dig;
		} while (uv >>= 1);
		if (alt) {
		    esignbuf[esignlen++] = '0';
		    esignbuf[esignlen++] = 'b';
		}
		break;
	    default:		/* it had better be ten or less */
#if defined(PERL_Y2KWARN)
		if (ckWARN(WARN_Y2K)) {
		    STRLEN n;
		    char *s = SvPV(sv,n);
		    if (n >= 2 && s[n-2] == '1' && s[n-1] == '9'
			&& (n == 2 || !isDIGIT(s[n-3])))
		    {
			Perl_warner(aTHX_ WARN_Y2K,
				    "Possible Y2K bug: %%%c %s",
				    c, "format string following '19'");
		    }
		}
#endif
		do {
		    dig = uv % base;
		    *--eptr = '0' + dig;
		} while (uv /= base);
		break;
	    }
	    elen = (ebuf + sizeof ebuf) - eptr;
	    if (has_precis) {
		if (precis > elen)
		    zeros = precis - elen;
		else if (precis == 0 && elen == 1 && *eptr == '0')
		    elen = 0;
	    }
	    break;

	    /* FLOATING POINT */

	case 'F':
	    c = 'f';		/* maybe %F isn't supported here */
	    /* FALL THROUGH */
	case 'e': case 'E':
	case 'f':
	case 'g': case 'G':

	    /* This is evil, but floating point is even more evil */

	    vectorize = FALSE;
	    if (args)
		nv = va_arg(*args, NV);
	    else
		nv = (svix < svmax) ? SvNVx(svargs[svix++]) : 0.0;

	    need = 0;
	    if (c != 'e' && c != 'E') {
		i = PERL_INT_MIN;
		(void)Perl_frexp(nv, &i);
		if (i == PERL_INT_MIN)
		    Perl_die(aTHX_ "panic: frexp");
		if (i > 0)
		    need = BIT_DIGITS(i);
	    }
	    need += has_precis ? precis : 6; /* known default */
	    if (need < width)
		need = width;

	    need += 20; /* fudge factor */
	    if (PL_efloatsize < need) {
		Safefree(PL_efloatbuf);
		PL_efloatsize = need + 20; /* more fudge */
		New(906, PL_efloatbuf, PL_efloatsize, char);
		PL_efloatbuf[0] = '\0';
	    }

	    eptr = ebuf + sizeof ebuf;
	    *--eptr = '\0';
	    *--eptr = c;
#if defined(USE_LONG_DOUBLE) && defined(PERL_PRIfldbl)
	    {
		/* Copy the one or more characters in a long double
		 * format before the 'base' ([efgEFG]) character to
		 * the format string. */
		static char const prifldbl[] = PERL_PRIfldbl;
		char const *p = prifldbl + sizeof(prifldbl) - 3;
		while (p >= prifldbl) { *--eptr = *p--; }
	    }
#endif
	    if (has_precis) {
		base = precis;
		do { *--eptr = '0' + (base % 10); } while (base /= 10);
		*--eptr = '.';
	    }
	    if (width) {
		base = width;
		do { *--eptr = '0' + (base % 10); } while (base /= 10);
	    }
	    if (fill == '0')
		*--eptr = fill;
	    if (left)
		*--eptr = '-';
	    if (plus)
		*--eptr = plus;
	    if (alt)
		*--eptr = '#';
	    *--eptr = '%';

	    /* No taint.  Otherwise we are in the strange situation
	     * where printf() taints but print($float) doesn't.
	     * --jhi */
	    (void)sprintf(PL_efloatbuf, eptr, nv);

	    eptr = PL_efloatbuf;
	    elen = strlen(PL_efloatbuf);
	    break;

	    /* SPECIAL */

	case 'n':
	    vectorize = FALSE;
	    i = SvCUR(sv) - origlen;
	    if (args) {
		switch (intsize) {
		case 'h':	*(va_arg(*args, short*)) = i; break;
		default:	*(va_arg(*args, int*)) = i; break;
		case 'l':	*(va_arg(*args, long*)) = i; break;
		case 'V':	*(va_arg(*args, IV*)) = i; break;
#ifdef HAS_QUAD
		case 'q':	*(va_arg(*args, Quad_t*)) = i; break;
#endif
		}
	    }
	    else if (svix < svmax)
		sv_setuv_mg(svargs[svix++], (UV)i);
	    continue;	/* not "break" */

	    /* UNKNOWN */

	default:
      unknown:
	    vectorize = FALSE;
	    if (!args && ckWARN(WARN_PRINTF) &&
		  (PL_op->op_type == OP_PRTF || PL_op->op_type == OP_SPRINTF)) {
		SV *msg = sv_newmortal();
		Perl_sv_setpvf(aTHX_ msg, "Invalid conversion in %s: ",
			  (PL_op->op_type == OP_PRTF) ? "printf" : "sprintf");
		if (c) {
		    if (isPRINT(c))
			Perl_sv_catpvf(aTHX_ msg, 
				       "\"%%%c\"", c & 0xFF);
		    else
			Perl_sv_catpvf(aTHX_ msg,
				       "\"%%\\%03"UVof"\"",
				       (UV)c & 0xFF);
		} else
		    sv_catpv(msg, "end of string");
		Perl_warner(aTHX_ WARN_PRINTF, "%"SVf, msg); /* yes, this is reentrant */
	    }

	    /* output mangled stuff ... */
	    if (c == '\0')
		--q;
	    eptr = p;
	    elen = q - p;

	    /* ... right here, because formatting flags should not apply */
	    SvGROW(sv, SvCUR(sv) + elen + 1);
	    p = SvEND(sv);
	    memcpy(p, eptr, elen);
	    p += elen;
	    *p = '\0';
	    SvCUR(sv) = p - SvPVX(sv);
	    continue;	/* not "break" */
	}

	have = esignlen + zeros + elen;
	need = (have > width ? have : width);
	gap = need - have;

	SvGROW(sv, SvCUR(sv) + need + dotstrlen + 1);
	p = SvEND(sv);
	if (esignlen && fill == '0') {
	    for (i = 0; i < esignlen; i++)
		*p++ = esignbuf[i];
	}
	if (gap && !left) {
	    memset(p, fill, gap);
	    p += gap;
	}
	if (esignlen && fill != '0') {
	    for (i = 0; i < esignlen; i++)
		*p++ = esignbuf[i];
	}
	if (zeros) {
	    for (i = zeros; i; i--)
		*p++ = '0';
	}
	if (elen) {
	    memcpy(p, eptr, elen);
	    p += elen;
	}
	if (gap && left) {
	    memset(p, ' ', gap);
	    p += gap;
	}
	if (vectorize) {
	    if (veclen) {
		memcpy(p, dotstr, dotstrlen);
		p += dotstrlen;
	    }
	    else
		vectorize = FALSE;		/* done iterating over vecstr */
	}
	if (is_utf)
	    SvUTF8_on(sv);
	*p = '\0';
	SvCUR(sv) = p - SvPVX(sv);
	if (vectorize) {
	    esignlen = 0;
	    goto vector;
	}
    }
}

#if defined(USE_ITHREADS)

#if defined(USE_THREADS)
#  include "error: USE_THREADS and USE_ITHREADS are incompatible"
#endif

#ifndef GpREFCNT_inc
#  define GpREFCNT_inc(gp)	((gp) ? (++(gp)->gp_refcnt, (gp)) : (GP*)NULL)
#endif


#define sv_dup_inc(s)	SvREFCNT_inc(sv_dup(s))
#define av_dup(s)	(AV*)sv_dup((SV*)s)
#define av_dup_inc(s)	(AV*)SvREFCNT_inc(sv_dup((SV*)s))
#define hv_dup(s)	(HV*)sv_dup((SV*)s)
#define hv_dup_inc(s)	(HV*)SvREFCNT_inc(sv_dup((SV*)s))
#define cv_dup(s)	(CV*)sv_dup((SV*)s)
#define cv_dup_inc(s)	(CV*)SvREFCNT_inc(sv_dup((SV*)s))
#define io_dup(s)	(IO*)sv_dup((SV*)s)
#define io_dup_inc(s)	(IO*)SvREFCNT_inc(sv_dup((SV*)s))
#define gv_dup(s)	(GV*)sv_dup((SV*)s)
#define gv_dup_inc(s)	(GV*)SvREFCNT_inc(sv_dup((SV*)s))
#define SAVEPV(p)	(p ? savepv(p) : Nullch)
#define SAVEPVN(p,n)	(p ? savepvn(p,n) : Nullch)

REGEXP *
Perl_re_dup(pTHX_ REGEXP *r)
{
    /* XXX fix when pmop->op_pmregexp becomes shared */
    return ReREFCNT_inc(r);
}

PerlIO *
Perl_fp_dup(pTHX_ PerlIO *fp, char type)
{
    PerlIO *ret;
    if (!fp)
	return (PerlIO*)NULL;

    /* look for it in the table first */
    ret = (PerlIO*)ptr_table_fetch(PL_ptr_table, fp);
    if (ret)
	return ret;

    /* create anew and remember what it is */
    ret = PerlIO_fdupopen(fp);
    ptr_table_store(PL_ptr_table, fp, ret);
    return ret;
}

DIR *
Perl_dirp_dup(pTHX_ DIR *dp)
{
    if (!dp)
	return (DIR*)NULL;
    /* XXX TODO */
    return dp;
}

GP *
Perl_gp_dup(pTHX_ GP *gp)
{
    GP *ret;
    if (!gp)
	return (GP*)NULL;
    /* look for it in the table first */
    ret = (GP*)ptr_table_fetch(PL_ptr_table, gp);
    if (ret)
	return ret;

    /* create anew and remember what it is */
    Newz(0, ret, 1, GP);
    ptr_table_store(PL_ptr_table, gp, ret);

    /* clone */
    ret->gp_refcnt	= 0;			/* must be before any other dups! */
    ret->gp_sv		= sv_dup_inc(gp->gp_sv);
    ret->gp_io		= io_dup_inc(gp->gp_io);
    ret->gp_form	= cv_dup_inc(gp->gp_form);
    ret->gp_av		= av_dup_inc(gp->gp_av);
    ret->gp_hv		= hv_dup_inc(gp->gp_hv);
    ret->gp_egv		= gv_dup(gp->gp_egv);	/* GvEGV is not refcounted */
    ret->gp_cv		= cv_dup_inc(gp->gp_cv);
    ret->gp_cvgen	= gp->gp_cvgen;
    ret->gp_flags	= gp->gp_flags;
    ret->gp_line	= gp->gp_line;
    ret->gp_file	= gp->gp_file;		/* points to COP.cop_file */
    return ret;
}

MAGIC *
Perl_mg_dup(pTHX_ MAGIC *mg)
{
    MAGIC *mgprev = (MAGIC*)NULL;
    MAGIC *mgret;
    if (!mg)
	return (MAGIC*)NULL;
    /* look for it in the table first */
    mgret = (MAGIC*)ptr_table_fetch(PL_ptr_table, mg);
    if (mgret)
	return mgret;

    for (; mg; mg = mg->mg_moremagic) {
	MAGIC *nmg;
	Newz(0, nmg, 1, MAGIC);
	if (mgprev)
	    mgprev->mg_moremagic = nmg;
	else
	    mgret = nmg;
	nmg->mg_virtual	= mg->mg_virtual;	/* XXX copy dynamic vtable? */
	nmg->mg_private	= mg->mg_private;
	nmg->mg_type	= mg->mg_type;
	nmg->mg_flags	= mg->mg_flags;
	if (mg->mg_type == 'r') {
	    nmg->mg_obj	= (SV*)re_dup((REGEXP*)mg->mg_obj);
	}
	else {
	    nmg->mg_obj	= (mg->mg_flags & MGf_REFCOUNTED)
			      ? sv_dup_inc(mg->mg_obj)
			      : sv_dup(mg->mg_obj);
	}
	nmg->mg_len	= mg->mg_len;
	nmg->mg_ptr	= mg->mg_ptr;	/* XXX random ptr? */
	if (mg->mg_ptr && mg->mg_type != 'g') {
	    if (mg->mg_len >= 0) {
		nmg->mg_ptr	= SAVEPVN(mg->mg_ptr, mg->mg_len);
		if (mg->mg_type == 'c' && AMT_AMAGIC((AMT*)mg->mg_ptr)) {
		    AMT *amtp = (AMT*)mg->mg_ptr;
		    AMT *namtp = (AMT*)nmg->mg_ptr;
		    I32 i;
		    for (i = 1; i < NofAMmeth; i++) {
			namtp->table[i] = cv_dup_inc(amtp->table[i]);
		    }
		}
	    }
	    else if (mg->mg_len == HEf_SVKEY)
		nmg->mg_ptr	= (char*)sv_dup_inc((SV*)mg->mg_ptr);
	}
	mgprev = nmg;
    }
    return mgret;
}

PTR_TBL_t *
Perl_ptr_table_new(pTHX)
{
    PTR_TBL_t *tbl;
    Newz(0, tbl, 1, PTR_TBL_t);
    tbl->tbl_max	= 511;
    tbl->tbl_items	= 0;
    Newz(0, tbl->tbl_ary, tbl->tbl_max + 1, PTR_TBL_ENT_t*);
    return tbl;
}

void *
Perl_ptr_table_fetch(pTHX_ PTR_TBL_t *tbl, void *sv)
{
    PTR_TBL_ENT_t *tblent;
    UV hash = PTR2UV(sv);
    assert(tbl);
    tblent = tbl->tbl_ary[hash & tbl->tbl_max];
    for (; tblent; tblent = tblent->next) {
	if (tblent->oldval == sv)
	    return tblent->newval;
    }
    return (void*)NULL;
}

void
Perl_ptr_table_store(pTHX_ PTR_TBL_t *tbl, void *oldv, void *newv)
{
    PTR_TBL_ENT_t *tblent, **otblent;
    /* XXX this may be pessimal on platforms where pointers aren't good
     * hash values e.g. if they grow faster in the most significant
     * bits */
    UV hash = PTR2UV(oldv);
    bool i = 1;

    assert(tbl);
    otblent = &tbl->tbl_ary[hash & tbl->tbl_max];
    for (tblent = *otblent; tblent; i=0, tblent = tblent->next) {
	if (tblent->oldval == oldv) {
	    tblent->newval = newv;
	    tbl->tbl_items++;
	    return;
	}
    }
    Newz(0, tblent, 1, PTR_TBL_ENT_t);
    tblent->oldval = oldv;
    tblent->newval = newv;
    tblent->next = *otblent;
    *otblent = tblent;
    tbl->tbl_items++;
    if (i && tbl->tbl_items > tbl->tbl_max)
	ptr_table_split(tbl);
}

void
Perl_ptr_table_split(pTHX_ PTR_TBL_t *tbl)
{
    PTR_TBL_ENT_t **ary = tbl->tbl_ary;
    UV oldsize = tbl->tbl_max + 1;
    UV newsize = oldsize * 2;
    UV i;

    Renew(ary, newsize, PTR_TBL_ENT_t*);
    Zero(&ary[oldsize], newsize-oldsize, PTR_TBL_ENT_t*);
    tbl->tbl_max = --newsize;
    tbl->tbl_ary = ary;
    for (i=0; i < oldsize; i++, ary++) {
	PTR_TBL_ENT_t **curentp, **entp, *ent;
	if (!*ary)
	    continue;
	curentp = ary + oldsize;
	for (entp = ary, ent = *ary; ent; ent = *entp) {
	    if ((newsize & PTR2UV(ent->oldval)) != i) {
		*entp = ent->next;
		ent->next = *curentp;
		*curentp = ent;
		continue;
	    }
	    else
		entp = &ent->next;
	}
    }
}

void
Perl_ptr_table_clear(pTHX_ PTR_TBL_t *tbl)
{
    register PTR_TBL_ENT_t **array;
    register PTR_TBL_ENT_t *entry;
    register PTR_TBL_ENT_t *oentry = Null(PTR_TBL_ENT_t*);
    UV riter = 0;
    UV max;

    if (!tbl || !tbl->tbl_items) {
        return;
    }

    array = tbl->tbl_ary;
    entry = array[0];
    max = tbl->tbl_max;

    for (;;) {
        if (entry) {
            oentry = entry;
            entry = entry->next;
            Safefree(oentry);
        }
        if (!entry) {
            if (++riter > max) {
                break;
            }
            entry = array[riter];
        }
    }

    tbl->tbl_items = 0;
}

void
Perl_ptr_table_free(pTHX_ PTR_TBL_t *tbl)
{
    if (!tbl) {
        return;
    }
    ptr_table_clear(tbl);
    Safefree(tbl->tbl_ary);
    Safefree(tbl);
}

#ifdef DEBUGGING
char *PL_watch_pvx;
#endif

SV *
Perl_sv_dup(pTHX_ SV *sstr)
{
    SV *dstr;

    if (!sstr || SvTYPE(sstr) == SVTYPEMASK)
	return Nullsv;
    /* look for it in the table first */
    dstr = (SV*)ptr_table_fetch(PL_ptr_table, sstr);
    if (dstr)
	return dstr;

    /* create anew and remember what it is */
    new_SV(dstr);
    ptr_table_store(PL_ptr_table, sstr, dstr);

    /* clone */
    SvFLAGS(dstr)	= SvFLAGS(sstr);
    SvFLAGS(dstr)	&= ~SVf_OOK;		/* don't propagate OOK hack */
    SvREFCNT(dstr)	= 0;			/* must be before any other dups! */

#ifdef DEBUGGING
    if (SvANY(sstr) && PL_watch_pvx && SvPVX(sstr) == PL_watch_pvx)
	PerlIO_printf(Perl_debug_log, "watch at %p hit, found string \"%s\"\n",
		      PL_watch_pvx, SvPVX(sstr));
#endif

    switch (SvTYPE(sstr)) {
    case SVt_NULL:
	SvANY(dstr)	= NULL;
	break;
    case SVt_IV:
	SvANY(dstr)	= new_XIV();
	SvIVX(dstr)	= SvIVX(sstr);
	break;
    case SVt_NV:
	SvANY(dstr)	= new_XNV();
	SvNVX(dstr)	= SvNVX(sstr);
	break;
    case SVt_RV:
	SvANY(dstr)	= new_XRV();
	SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	break;
    case SVt_PV:
	SvANY(dstr)	= new_XPV();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	if (SvROK(sstr))
	    SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	else if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	break;
    case SVt_PVIV:
	SvANY(dstr)	= new_XPVIV();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	if (SvROK(sstr))
	    SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	else if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	break;
    case SVt_PVNV:
	SvANY(dstr)	= new_XPVNV();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	if (SvROK(sstr))
	    SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	else if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	break;
    case SVt_PVMG:
	SvANY(dstr)	= new_XPVMG();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	SvMAGIC(dstr)	= mg_dup(SvMAGIC(sstr));
	SvSTASH(dstr)	= hv_dup_inc(SvSTASH(sstr));
	if (SvROK(sstr))
	    SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	else if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	break;
    case SVt_PVBM:
	SvANY(dstr)	= new_XPVBM();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	SvMAGIC(dstr)	= mg_dup(SvMAGIC(sstr));
	SvSTASH(dstr)	= hv_dup_inc(SvSTASH(sstr));
	if (SvROK(sstr))
	    SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	else if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	BmRARE(dstr)	= BmRARE(sstr);
	BmUSEFUL(dstr)	= BmUSEFUL(sstr);
	BmPREVIOUS(dstr)= BmPREVIOUS(sstr);
	break;
    case SVt_PVLV:
	SvANY(dstr)	= new_XPVLV();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	SvMAGIC(dstr)	= mg_dup(SvMAGIC(sstr));
	SvSTASH(dstr)	= hv_dup_inc(SvSTASH(sstr));
	if (SvROK(sstr))
	    SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	else if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	LvTARGOFF(dstr)	= LvTARGOFF(sstr);	/* XXX sometimes holds PMOP* when DEBUGGING */
	LvTARGLEN(dstr)	= LvTARGLEN(sstr);
	LvTARG(dstr)	= sv_dup_inc(LvTARG(sstr));
	LvTYPE(dstr)	= LvTYPE(sstr);
	break;
    case SVt_PVGV:
	SvANY(dstr)	= new_XPVGV();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	SvMAGIC(dstr)	= mg_dup(SvMAGIC(sstr));
	SvSTASH(dstr)	= hv_dup_inc(SvSTASH(sstr));
	if (SvROK(sstr))
	    SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	else if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	GvNAMELEN(dstr)	= GvNAMELEN(sstr);
	GvNAME(dstr)	= SAVEPVN(GvNAME(sstr), GvNAMELEN(sstr));
	GvSTASH(dstr)	= hv_dup_inc(GvSTASH(sstr));
	GvFLAGS(dstr)	= GvFLAGS(sstr);
	GvGP(dstr)	= gp_dup(GvGP(sstr));
	(void)GpREFCNT_inc(GvGP(dstr));
	break;
    case SVt_PVIO:
	SvANY(dstr)	= new_XPVIO();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	SvMAGIC(dstr)	= mg_dup(SvMAGIC(sstr));
	SvSTASH(dstr)	= hv_dup_inc(SvSTASH(sstr));
	if (SvROK(sstr))
	    SvRV(dstr)	= sv_dup_inc(SvRV(sstr));
	else if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	IoIFP(dstr)	= fp_dup(IoIFP(sstr), IoTYPE(sstr));
	if (IoOFP(sstr) == IoIFP(sstr))
	    IoOFP(dstr) = IoIFP(dstr);
	else
	    IoOFP(dstr)	= fp_dup(IoOFP(sstr), IoTYPE(sstr));
	/* PL_rsfp_filters entries have fake IoDIRP() */
	if (IoDIRP(sstr) && !(IoFLAGS(sstr) & IOf_FAKE_DIRP))
	    IoDIRP(dstr)	= dirp_dup(IoDIRP(sstr));
	else
	    IoDIRP(dstr)	= IoDIRP(sstr);
	IoLINES(dstr)		= IoLINES(sstr);
	IoPAGE(dstr)		= IoPAGE(sstr);
	IoPAGE_LEN(dstr)	= IoPAGE_LEN(sstr);
	IoLINES_LEFT(dstr)	= IoLINES_LEFT(sstr);
	IoTOP_NAME(dstr)	= SAVEPV(IoTOP_NAME(sstr));
	IoTOP_GV(dstr)		= gv_dup(IoTOP_GV(sstr));
	IoFMT_NAME(dstr)	= SAVEPV(IoFMT_NAME(sstr));
	IoFMT_GV(dstr)		= gv_dup(IoFMT_GV(sstr));
	IoBOTTOM_NAME(dstr)	= SAVEPV(IoBOTTOM_NAME(sstr));
	IoBOTTOM_GV(dstr)	= gv_dup(IoBOTTOM_GV(sstr));
	IoSUBPROCESS(dstr)	= IoSUBPROCESS(sstr);
	IoTYPE(dstr)		= IoTYPE(sstr);
	IoFLAGS(dstr)		= IoFLAGS(sstr);
	break;
    case SVt_PVAV:
	SvANY(dstr)	= new_XPVAV();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	SvMAGIC(dstr)	= mg_dup(SvMAGIC(sstr));
	SvSTASH(dstr)	= hv_dup_inc(SvSTASH(sstr));
	AvARYLEN((AV*)dstr) = sv_dup_inc(AvARYLEN((AV*)sstr));
	AvFLAGS((AV*)dstr) = AvFLAGS((AV*)sstr);
	if (AvARRAY((AV*)sstr)) {
	    SV **dst_ary, **src_ary;
	    SSize_t items = AvFILLp((AV*)sstr) + 1;

	    src_ary = AvARRAY((AV*)sstr);
	    Newz(0, dst_ary, AvMAX((AV*)sstr)+1, SV*);
	    ptr_table_store(PL_ptr_table, src_ary, dst_ary);
	    SvPVX(dstr)	= (char*)dst_ary;
	    AvALLOC((AV*)dstr) = dst_ary;
	    if (AvREAL((AV*)sstr)) {
		while (items-- > 0)
		    *dst_ary++ = sv_dup_inc(*src_ary++);
	    }
	    else {
		while (items-- > 0)
		    *dst_ary++ = sv_dup(*src_ary++);
	    }
	    items = AvMAX((AV*)sstr) - AvFILLp((AV*)sstr);
	    while (items-- > 0) {
		*dst_ary++ = &PL_sv_undef;
	    }
	}
	else {
	    SvPVX(dstr)		= Nullch;
	    AvALLOC((AV*)dstr)	= (SV**)NULL;
	}
	break;
    case SVt_PVHV:
	SvANY(dstr)	= new_XPVHV();
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	SvMAGIC(dstr)	= mg_dup(SvMAGIC(sstr));
	SvSTASH(dstr)	= hv_dup_inc(SvSTASH(sstr));
	HvRITER((HV*)dstr)	= HvRITER((HV*)sstr);
	if (HvARRAY((HV*)sstr)) {
	    STRLEN i = 0;
	    XPVHV *dxhv = (XPVHV*)SvANY(dstr);
	    XPVHV *sxhv = (XPVHV*)SvANY(sstr);
	    Newz(0, dxhv->xhv_array,
		 PERL_HV_ARRAY_ALLOC_BYTES(dxhv->xhv_max+1), char);
	    while (i <= sxhv->xhv_max) {
		((HE**)dxhv->xhv_array)[i] = he_dup(((HE**)sxhv->xhv_array)[i],
						    !!HvSHAREKEYS(sstr));
		++i;
	    }
	    dxhv->xhv_eiter = he_dup(sxhv->xhv_eiter, !!HvSHAREKEYS(sstr));
	}
	else {
	    SvPVX(dstr)		= Nullch;
	    HvEITER((HV*)dstr)	= (HE*)NULL;
	}
	HvPMROOT((HV*)dstr)	= HvPMROOT((HV*)sstr);		/* XXX */
	HvNAME((HV*)dstr)	= SAVEPV(HvNAME((HV*)sstr));
	break;
    case SVt_PVFM:
	SvANY(dstr)	= new_XPVFM();
	FmLINES(dstr)	= FmLINES(sstr);
	goto dup_pvcv;
	/* NOTREACHED */
    case SVt_PVCV:
	SvANY(dstr)	= new_XPVCV();
dup_pvcv:
	SvCUR(dstr)	= SvCUR(sstr);
	SvLEN(dstr)	= SvLEN(sstr);
	SvIVX(dstr)	= SvIVX(sstr);
	SvNVX(dstr)	= SvNVX(sstr);
	SvMAGIC(dstr)	= mg_dup(SvMAGIC(sstr));
	SvSTASH(dstr)	= hv_dup_inc(SvSTASH(sstr));
	if (SvPVX(sstr) && SvLEN(sstr))
	    SvPVX(dstr)	= SAVEPVN(SvPVX(sstr), SvLEN(sstr)-1);
	else
	    SvPVX(dstr)	= SvPVX(sstr);		/* XXX shared string/random ptr? */
	CvSTASH(dstr)	= hv_dup(CvSTASH(sstr));/* NOTE: not refcounted */
	CvSTART(dstr)	= CvSTART(sstr);
	CvROOT(dstr)	= OpREFCNT_inc(CvROOT(sstr));
	CvXSUB(dstr)	= CvXSUB(sstr);
	CvXSUBANY(dstr)	= CvXSUBANY(sstr);
	CvGV(dstr)	= gv_dup(CvGV(sstr));
	CvDEPTH(dstr)	= CvDEPTH(sstr);
	if (CvPADLIST(sstr) && !AvREAL(CvPADLIST(sstr))) {
	    /* XXX padlists are real, but pretend to be not */
	    AvREAL_on(CvPADLIST(sstr));
	    CvPADLIST(dstr)	= av_dup_inc(CvPADLIST(sstr));
	    AvREAL_off(CvPADLIST(sstr));
	    AvREAL_off(CvPADLIST(dstr));
	}
	else
	    CvPADLIST(dstr)	= av_dup_inc(CvPADLIST(sstr));
	if (!CvANON(sstr) || CvCLONED(sstr))
	    CvOUTSIDE(dstr)	= cv_dup_inc(CvOUTSIDE(sstr));
	else
	    CvOUTSIDE(dstr)	= cv_dup(CvOUTSIDE(sstr));
	CvFLAGS(dstr)	= CvFLAGS(sstr);
	break;
    default:
	Perl_croak(aTHX_ "Bizarre SvTYPE [%d]", SvTYPE(sstr));
	break;
    }

    if (SvOBJECT(dstr) && SvTYPE(dstr) != SVt_PVIO)
	++PL_sv_objcount;

    return dstr;
}

PERL_CONTEXT *
Perl_cx_dup(pTHX_ PERL_CONTEXT *cxs, I32 ix, I32 max)
{
    PERL_CONTEXT *ncxs;

    if (!cxs)
	return (PERL_CONTEXT*)NULL;

    /* look for it in the table first */
    ncxs = (PERL_CONTEXT*)ptr_table_fetch(PL_ptr_table, cxs);
    if (ncxs)
	return ncxs;

    /* create anew and remember what it is */
    Newz(56, ncxs, max + 1, PERL_CONTEXT);
    ptr_table_store(PL_ptr_table, cxs, ncxs);

    while (ix >= 0) {
	PERL_CONTEXT *cx = &cxs[ix];
	PERL_CONTEXT *ncx = &ncxs[ix];
	ncx->cx_type	= cx->cx_type;
	if (CxTYPE(cx) == CXt_SUBST) {
	    Perl_croak(aTHX_ "Cloning substitution context is unimplemented");
	}
	else {
	    ncx->blk_oldsp	= cx->blk_oldsp;
	    ncx->blk_oldcop	= cx->blk_oldcop;
	    ncx->blk_oldretsp	= cx->blk_oldretsp;
	    ncx->blk_oldmarksp	= cx->blk_oldmarksp;
	    ncx->blk_oldscopesp	= cx->blk_oldscopesp;
	    ncx->blk_oldpm	= cx->blk_oldpm;
	    ncx->blk_gimme	= cx->blk_gimme;
	    switch (CxTYPE(cx)) {
	    case CXt_SUB:
		ncx->blk_sub.cv		= (cx->blk_sub.olddepth == 0
					   ? cv_dup_inc(cx->blk_sub.cv)
					   : cv_dup(cx->blk_sub.cv));
		ncx->blk_sub.argarray	= (cx->blk_sub.hasargs
					   ? av_dup_inc(cx->blk_sub.argarray)
					   : Nullav);
		ncx->blk_sub.savearray	= av_dup_inc(cx->blk_sub.savearray);
		ncx->blk_sub.olddepth	= cx->blk_sub.olddepth;
		ncx->blk_sub.hasargs	= cx->blk_sub.hasargs;
		ncx->blk_sub.lval	= cx->blk_sub.lval;
		break;
	    case CXt_EVAL:
		ncx->blk_eval.old_in_eval = cx->blk_eval.old_in_eval;
		ncx->blk_eval.old_op_type = cx->blk_eval.old_op_type;
		ncx->blk_eval.old_namesv = sv_dup_inc(cx->blk_eval.old_namesv);
		ncx->blk_eval.old_eval_root = cx->blk_eval.old_eval_root;
		ncx->blk_eval.cur_text	= sv_dup(cx->blk_eval.cur_text);
		break;
	    case CXt_LOOP:
		ncx->blk_loop.label	= cx->blk_loop.label;
		ncx->blk_loop.resetsp	= cx->blk_loop.resetsp;
		ncx->blk_loop.redo_op	= cx->blk_loop.redo_op;
		ncx->blk_loop.next_op	= cx->blk_loop.next_op;
		ncx->blk_loop.last_op	= cx->blk_loop.last_op;
		ncx->blk_loop.iterdata	= (CxPADLOOP(cx)
					   ? cx->blk_loop.iterdata
					   : gv_dup((GV*)cx->blk_loop.iterdata));
		ncx->blk_loop.oldcurpad
		    = (SV**)ptr_table_fetch(PL_ptr_table,
					    cx->blk_loop.oldcurpad);
		ncx->blk_loop.itersave	= sv_dup_inc(cx->blk_loop.itersave);
		ncx->blk_loop.iterlval	= sv_dup_inc(cx->blk_loop.iterlval);
		ncx->blk_loop.iterary	= av_dup_inc(cx->blk_loop.iterary);
		ncx->blk_loop.iterix	= cx->blk_loop.iterix;
		ncx->blk_loop.itermax	= cx->blk_loop.itermax;
		break;
	    case CXt_FORMAT:
		ncx->blk_sub.cv		= cv_dup(cx->blk_sub.cv);
		ncx->blk_sub.gv		= gv_dup(cx->blk_sub.gv);
		ncx->blk_sub.dfoutgv	= gv_dup_inc(cx->blk_sub.dfoutgv);
		ncx->blk_sub.hasargs	= cx->blk_sub.hasargs;
		break;
	    case CXt_BLOCK:
	    case CXt_NULL:
		break;
	    }
	}
	--ix;
    }
    return ncxs;
}

PERL_SI *
Perl_si_dup(pTHX_ PERL_SI *si)
{
    PERL_SI *nsi;

    if (!si)
	return (PERL_SI*)NULL;

    /* look for it in the table first */
    nsi = (PERL_SI*)ptr_table_fetch(PL_ptr_table, si);
    if (nsi)
	return nsi;

    /* create anew and remember what it is */
    Newz(56, nsi, 1, PERL_SI);
    ptr_table_store(PL_ptr_table, si, nsi);

    nsi->si_stack	= av_dup_inc(si->si_stack);
    nsi->si_cxix	= si->si_cxix;
    nsi->si_cxmax	= si->si_cxmax;
    nsi->si_cxstack	= cx_dup(si->si_cxstack, si->si_cxix, si->si_cxmax);
    nsi->si_type	= si->si_type;
    nsi->si_prev	= si_dup(si->si_prev);
    nsi->si_next	= si_dup(si->si_next);
    nsi->si_markoff	= si->si_markoff;

    return nsi;
}

#define POPINT(ss,ix)	((ss)[--(ix)].any_i32)
#define TOPINT(ss,ix)	((ss)[ix].any_i32)
#define POPLONG(ss,ix)	((ss)[--(ix)].any_long)
#define TOPLONG(ss,ix)	((ss)[ix].any_long)
#define POPIV(ss,ix)	((ss)[--(ix)].any_iv)
#define TOPIV(ss,ix)	((ss)[ix].any_iv)
#define POPPTR(ss,ix)	((ss)[--(ix)].any_ptr)
#define TOPPTR(ss,ix)	((ss)[ix].any_ptr)
#define POPDPTR(ss,ix)	((ss)[--(ix)].any_dptr)
#define TOPDPTR(ss,ix)	((ss)[ix].any_dptr)
#define POPDXPTR(ss,ix)	((ss)[--(ix)].any_dxptr)
#define TOPDXPTR(ss,ix)	((ss)[ix].any_dxptr)

/* XXXXX todo */
#define pv_dup_inc(p)	SAVEPV(p)
#define pv_dup(p)	SAVEPV(p)
#define svp_dup_inc(p,pp)	any_dup(p,pp)

void *
Perl_any_dup(pTHX_ void *v, PerlInterpreter *proto_perl)
{
    void *ret;

    if (!v)
	return (void*)NULL;

    /* look for it in the table first */
    ret = ptr_table_fetch(PL_ptr_table, v);
    if (ret)
	return ret;

    /* see if it is part of the interpreter structure */
    if (v >= (void*)proto_perl && v < (void*)(proto_perl+1))
	ret = (void*)(((char*)aTHXo) + (((char*)v) - (char*)proto_perl));
    else
	ret = v;

    return ret;
}

ANY *
Perl_ss_dup(pTHX_ PerlInterpreter *proto_perl)
{
    ANY *ss	= proto_perl->Tsavestack;
    I32 ix	= proto_perl->Tsavestack_ix;
    I32 max	= proto_perl->Tsavestack_max;
    ANY *nss;
    SV *sv;
    GV *gv;
    AV *av;
    HV *hv;
    void* ptr;
    int intval;
    long longval;
    GP *gp;
    IV iv;
    I32 i;
    char *c;
    void (*dptr) (void*);
    void (*dxptr) (pTHXo_ void*);
    OP *o;

    Newz(54, nss, max, ANY);

    while (ix > 0) {
	i = POPINT(ss,ix);
	TOPINT(nss,ix) = i;
	switch (i) {
	case SAVEt_ITEM:			/* normal string */
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup_inc(sv);
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup_inc(sv);
	    break;
        case SAVEt_SV:				/* scalar reference */
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup_inc(sv);
	    gv = (GV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = gv_dup_inc(gv);
	    break;
	case SAVEt_GENERIC_PVREF:		/* generic char* */
	    c = (char*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = pv_dup(c);
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    break;
        case SAVEt_GENERIC_SVREF:		/* generic sv */
        case SAVEt_SVREF:			/* scalar reference */
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup_inc(sv);
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = svp_dup_inc((SV**)ptr, proto_perl);/* XXXXX */
	    break;
        case SAVEt_AV:				/* array reference */
	    av = (AV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = av_dup_inc(av);
	    gv = (GV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = gv_dup(gv);
	    break;
        case SAVEt_HV:				/* hash reference */
	    hv = (HV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = hv_dup_inc(hv);
	    gv = (GV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = gv_dup(gv);
	    break;
	case SAVEt_INT:				/* int reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    intval = (int)POPINT(ss,ix);
	    TOPINT(nss,ix) = intval;
	    break;
	case SAVEt_LONG:			/* long reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    longval = (long)POPLONG(ss,ix);
	    TOPLONG(nss,ix) = longval;
	    break;
	case SAVEt_I32:				/* I32 reference */
	case SAVEt_I16:				/* I16 reference */
	case SAVEt_I8:				/* I8 reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    i = POPINT(ss,ix);
	    TOPINT(nss,ix) = i;
	    break;
	case SAVEt_IV:				/* IV reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    iv = POPIV(ss,ix);
	    TOPIV(nss,ix) = iv;
	    break;
	case SAVEt_SPTR:			/* SV* reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup(sv);
	    break;
	case SAVEt_VPTR:			/* random* reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    break;
	case SAVEt_PPTR:			/* char* reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    c = (char*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = pv_dup(c);
	    break;
	case SAVEt_HPTR:			/* HV* reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    hv = (HV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = hv_dup(hv);
	    break;
	case SAVEt_APTR:			/* AV* reference */
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    av = (AV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = av_dup(av);
	    break;
	case SAVEt_NSTAB:
	    gv = (GV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = gv_dup(gv);
	    break;
	case SAVEt_GP:				/* scalar reference */
	    gp = (GP*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = gp = gp_dup(gp);
	    (void)GpREFCNT_inc(gp);
	    gv = (GV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = gv_dup_inc(c);
            c = (char*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = pv_dup(c);
	    iv = POPIV(ss,ix);
	    TOPIV(nss,ix) = iv;
	    iv = POPIV(ss,ix);
	    TOPIV(nss,ix) = iv;
            break;
	case SAVEt_FREESV:
	case SAVEt_MORTALIZESV:
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup_inc(sv);
	    break;
	case SAVEt_FREEOP:
	    ptr = POPPTR(ss,ix);
	    if (ptr && (((OP*)ptr)->op_private & OPpREFCOUNTED)) {
		/* these are assumed to be refcounted properly */
		switch (((OP*)ptr)->op_type) {
		case OP_LEAVESUB:
		case OP_LEAVESUBLV:
		case OP_LEAVEEVAL:
		case OP_LEAVE:
		case OP_SCOPE:
		case OP_LEAVEWRITE:
		    TOPPTR(nss,ix) = ptr;
		    o = (OP*)ptr;
		    OpREFCNT_inc(o);
		    break;
		default:
		    TOPPTR(nss,ix) = Nullop;
		    break;
		}
	    }
	    else
		TOPPTR(nss,ix) = Nullop;
	    break;
	case SAVEt_FREEPV:
	    c = (char*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = pv_dup_inc(c);
	    break;
	case SAVEt_CLEARSV:
	    longval = POPLONG(ss,ix);
	    TOPLONG(nss,ix) = longval;
	    break;
	case SAVEt_DELETE:
	    hv = (HV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = hv_dup_inc(hv);
	    c = (char*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = pv_dup_inc(c);
	    i = POPINT(ss,ix);
	    TOPINT(nss,ix) = i;
	    break;
	case SAVEt_DESTRUCTOR:
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);	/* XXX quite arbitrary */
	    dptr = POPDPTR(ss,ix);
	    TOPDPTR(nss,ix) = (void (*)(void*))any_dup((void *)dptr, proto_perl);
	    break;
	case SAVEt_DESTRUCTOR_X:
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);	/* XXX quite arbitrary */
	    dxptr = POPDXPTR(ss,ix);
	    TOPDXPTR(nss,ix) = (void (*)(pTHXo_ void*))any_dup((void *)dxptr, proto_perl);
	    break;
	case SAVEt_REGCONTEXT:
	case SAVEt_ALLOC:
	    i = POPINT(ss,ix);
	    TOPINT(nss,ix) = i;
	    ix -= i;
	    break;
	case SAVEt_STACK_POS:		/* Position on Perl stack */
	    i = POPINT(ss,ix);
	    TOPINT(nss,ix) = i;
	    break;
	case SAVEt_AELEM:		/* array element */
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup_inc(sv);
	    i = POPINT(ss,ix);
	    TOPINT(nss,ix) = i;
	    av = (AV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = av_dup_inc(av);
	    break;
	case SAVEt_HELEM:		/* hash element */
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup_inc(sv);
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup_inc(sv);
	    hv = (HV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = hv_dup_inc(hv);
	    break;
	case SAVEt_OP:
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = ptr;
	    break;
	case SAVEt_HINTS:
	    i = POPINT(ss,ix);
	    TOPINT(nss,ix) = i;
	    break;
	case SAVEt_COMPPAD:
	    av = (AV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = av_dup(av);
	    break;
	case SAVEt_PADSV:
	    longval = (long)POPLONG(ss,ix);
	    TOPLONG(nss,ix) = longval;
	    ptr = POPPTR(ss,ix);
	    TOPPTR(nss,ix) = any_dup(ptr, proto_perl);
	    sv = (SV*)POPPTR(ss,ix);
	    TOPPTR(nss,ix) = sv_dup(sv);
	    break;
	default:
	    Perl_croak(aTHX_ "panic: ss_dup inconsistency");
	}
    }

    return nss;
}

#ifdef PERL_OBJECT
#include "XSUB.h"
#endif

PerlInterpreter *
perl_clone(PerlInterpreter *proto_perl, UV flags)
{
#ifdef PERL_OBJECT
    CPerlObj *pPerl = (CPerlObj*)proto_perl;
#endif

#ifdef PERL_IMPLICIT_SYS
    return perl_clone_using(proto_perl, flags,
			    proto_perl->IMem,
			    proto_perl->IMemShared,
			    proto_perl->IMemParse,
			    proto_perl->IEnv,
			    proto_perl->IStdIO,
			    proto_perl->ILIO,
			    proto_perl->IDir,
			    proto_perl->ISock,
			    proto_perl->IProc);
}

PerlInterpreter *
perl_clone_using(PerlInterpreter *proto_perl, UV flags,
		 struct IPerlMem* ipM, struct IPerlMem* ipMS,
		 struct IPerlMem* ipMP, struct IPerlEnv* ipE,
		 struct IPerlStdIO* ipStd, struct IPerlLIO* ipLIO,
		 struct IPerlDir* ipD, struct IPerlSock* ipS,
		 struct IPerlProc* ipP)
{
    /* XXX many of the string copies here can be optimized if they're
     * constants; they need to be allocated as common memory and just
     * their pointers copied. */

    IV i;
#  ifdef PERL_OBJECT
    CPerlObj *pPerl = new(ipM) CPerlObj(ipM, ipMS, ipMP, ipE, ipStd, ipLIO,
					ipD, ipS, ipP);
    PERL_SET_THX(pPerl);
#  else		/* !PERL_OBJECT */
    PerlInterpreter *my_perl = (PerlInterpreter*)(*ipM->pMalloc)(ipM, sizeof(PerlInterpreter));
    PERL_SET_THX(my_perl);

#    ifdef DEBUGGING
    memset(my_perl, 0xab, sizeof(PerlInterpreter));
    PL_markstack = 0;
    PL_scopestack = 0;
    PL_savestack = 0;
    PL_retstack = 0;
#    else	/* !DEBUGGING */
    Zero(my_perl, 1, PerlInterpreter);
#    endif	/* DEBUGGING */

    /* host pointers */
    PL_Mem		= ipM;
    PL_MemShared	= ipMS;
    PL_MemParse		= ipMP;
    PL_Env		= ipE;
    PL_StdIO		= ipStd;
    PL_LIO		= ipLIO;
    PL_Dir		= ipD;
    PL_Sock		= ipS;
    PL_Proc		= ipP;
#  endif	/* PERL_OBJECT */
#else		/* !PERL_IMPLICIT_SYS */
    IV i;
    PerlInterpreter *my_perl = (PerlInterpreter*)PerlMem_malloc(sizeof(PerlInterpreter));
    PERL_SET_THX(my_perl);

#    ifdef DEBUGGING
    memset(my_perl, 0xab, sizeof(PerlInterpreter));
    PL_markstack = 0;
    PL_scopestack = 0;
    PL_savestack = 0;
    PL_retstack = 0;
#    else	/* !DEBUGGING */
    Zero(my_perl, 1, PerlInterpreter);
#    endif	/* DEBUGGING */
#endif		/* PERL_IMPLICIT_SYS */

    /* arena roots */
    PL_xiv_arenaroot	= NULL;
    PL_xiv_root		= NULL;
    PL_xnv_arenaroot	= NULL;
    PL_xnv_root		= NULL;
    PL_xrv_arenaroot	= NULL;
    PL_xrv_root		= NULL;
    PL_xpv_arenaroot	= NULL;
    PL_xpv_root		= NULL;
    PL_xpviv_arenaroot	= NULL;
    PL_xpviv_root	= NULL;
    PL_xpvnv_arenaroot	= NULL;
    PL_xpvnv_root	= NULL;
    PL_xpvcv_arenaroot	= NULL;
    PL_xpvcv_root	= NULL;
    PL_xpvav_arenaroot	= NULL;
    PL_xpvav_root	= NULL;
    PL_xpvhv_arenaroot	= NULL;
    PL_xpvhv_root	= NULL;
    PL_xpvmg_arenaroot	= NULL;
    PL_xpvmg_root	= NULL;
    PL_xpvlv_arenaroot	= NULL;
    PL_xpvlv_root	= NULL;
    PL_xpvbm_arenaroot	= NULL;
    PL_xpvbm_root	= NULL;
    PL_he_arenaroot	= NULL;
    PL_he_root		= NULL;
    PL_nice_chunk	= NULL;
    PL_nice_chunk_size	= 0;
    PL_sv_count		= 0;
    PL_sv_objcount	= 0;
    PL_sv_root		= Nullsv;
    PL_sv_arenaroot	= Nullsv;

    PL_debug		= proto_perl->Idebug;

    /* create SV map for pointer relocation */
    PL_ptr_table = ptr_table_new();

    /* initialize these special pointers as early as possible */
    SvANY(&PL_sv_undef)		= NULL;
    SvREFCNT(&PL_sv_undef)	= (~(U32)0)/2;
    SvFLAGS(&PL_sv_undef)	= SVf_READONLY|SVt_NULL;
    ptr_table_store(PL_ptr_table, &proto_perl->Isv_undef, &PL_sv_undef);

#ifdef PERL_OBJECT
    SvUPGRADE(&PL_sv_no, SVt_PVNV);
#else
    SvANY(&PL_sv_no)		= new_XPVNV();
#endif
    SvREFCNT(&PL_sv_no)		= (~(U32)0)/2;
    SvFLAGS(&PL_sv_no)		= SVp_NOK|SVf_NOK|SVp_POK|SVf_POK|SVf_READONLY|SVt_PVNV;
    SvPVX(&PL_sv_no)		= SAVEPVN(PL_No, 0);
    SvCUR(&PL_sv_no)		= 0;
    SvLEN(&PL_sv_no)		= 1;
    SvNVX(&PL_sv_no)		= 0;
    ptr_table_store(PL_ptr_table, &proto_perl->Isv_no, &PL_sv_no);

#ifdef PERL_OBJECT
    SvUPGRADE(&PL_sv_yes, SVt_PVNV);
#else
    SvANY(&PL_sv_yes)		= new_XPVNV();
#endif
    SvREFCNT(&PL_sv_yes)	= (~(U32)0)/2;
    SvFLAGS(&PL_sv_yes)		= SVp_NOK|SVf_NOK|SVp_POK|SVf_POK|SVf_READONLY|SVt_PVNV;
    SvPVX(&PL_sv_yes)		= SAVEPVN(PL_Yes, 1);
    SvCUR(&PL_sv_yes)		= 1;
    SvLEN(&PL_sv_yes)		= 2;
    SvNVX(&PL_sv_yes)		= 1;
    ptr_table_store(PL_ptr_table, &proto_perl->Isv_yes, &PL_sv_yes);

    /* create shared string table */
    PL_strtab		= newHV();
    HvSHAREKEYS_off(PL_strtab);
    hv_ksplit(PL_strtab, 512);
    ptr_table_store(PL_ptr_table, proto_perl->Istrtab, PL_strtab);

    PL_compiling		= proto_perl->Icompiling;
    PL_compiling.cop_stashpv	= SAVEPV(PL_compiling.cop_stashpv);
    PL_compiling.cop_file	= SAVEPV(PL_compiling.cop_file);
    ptr_table_store(PL_ptr_table, &proto_perl->Icompiling, &PL_compiling);
    if (!specialWARN(PL_compiling.cop_warnings))
	PL_compiling.cop_warnings = sv_dup_inc(PL_compiling.cop_warnings);
    PL_curcop		= (COP*)any_dup(proto_perl->Tcurcop, proto_perl);

    /* pseudo environmental stuff */
    PL_origargc		= proto_perl->Iorigargc;
    i = PL_origargc;
    New(0, PL_origargv, i+1, char*);
    PL_origargv[i] = '\0';
    while (i-- > 0) {
	PL_origargv[i]	= SAVEPV(proto_perl->Iorigargv[i]);
    }
    PL_envgv		= gv_dup(proto_perl->Ienvgv);
    PL_incgv		= gv_dup(proto_perl->Iincgv);
    PL_hintgv		= gv_dup(proto_perl->Ihintgv);
    PL_origfilename	= SAVEPV(proto_perl->Iorigfilename);
    PL_diehook		= sv_dup_inc(proto_perl->Idiehook);
    PL_warnhook		= sv_dup_inc(proto_perl->Iwarnhook);

    /* switches */
    PL_minus_c		= proto_perl->Iminus_c;
    PL_patchlevel	= sv_dup_inc(proto_perl->Ipatchlevel);
    PL_localpatches	= proto_perl->Ilocalpatches;
    PL_splitstr		= proto_perl->Isplitstr;
    PL_preprocess	= proto_perl->Ipreprocess;
    PL_minus_n		= proto_perl->Iminus_n;
    PL_minus_p		= proto_perl->Iminus_p;
    PL_minus_l		= proto_perl->Iminus_l;
    PL_minus_a		= proto_perl->Iminus_a;
    PL_minus_F		= proto_perl->Iminus_F;
    PL_doswitches	= proto_perl->Idoswitches;
    PL_dowarn		= proto_perl->Idowarn;
    PL_doextract	= proto_perl->Idoextract;
    PL_sawampersand	= proto_perl->Isawampersand;
    PL_unsafe		= proto_perl->Iunsafe;
    PL_inplace		= SAVEPV(proto_perl->Iinplace);
    PL_e_script		= sv_dup_inc(proto_perl->Ie_script);
    PL_perldb		= proto_perl->Iperldb;
    PL_perl_destruct_level = proto_perl->Iperl_destruct_level;

    /* magical thingies */
    /* XXX time(&PL_basetime) when asked for? */
    PL_basetime		= proto_perl->Ibasetime;
    PL_formfeed		= sv_dup(proto_perl->Iformfeed);

    PL_maxsysfd		= proto_perl->Imaxsysfd;
    PL_multiline	= proto_perl->Imultiline;
    PL_statusvalue	= proto_perl->Istatusvalue;
#ifdef VMS
    PL_statusvalue_vms	= proto_perl->Istatusvalue_vms;
#endif

    /* shortcuts to various I/O objects */
    PL_stdingv		= gv_dup(proto_perl->Istdingv);
    PL_stderrgv		= gv_dup(proto_perl->Istderrgv);
    PL_defgv		= gv_dup(proto_perl->Idefgv);
    PL_argvgv		= gv_dup(proto_perl->Iargvgv);
    PL_argvoutgv	= gv_dup(proto_perl->Iargvoutgv);
    PL_argvout_stack	= av_dup_inc(proto_perl->Iargvout_stack);

    /* shortcuts to regexp stuff */
    PL_replgv		= gv_dup(proto_perl->Ireplgv);

    /* shortcuts to misc objects */
    PL_errgv		= gv_dup(proto_perl->Ierrgv);

    /* shortcuts to debugging objects */
    PL_DBgv		= gv_dup(proto_perl->IDBgv);
    PL_DBline		= gv_dup(proto_perl->IDBline);
    PL_DBsub		= gv_dup(proto_perl->IDBsub);
    PL_DBsingle		= sv_dup(proto_perl->IDBsingle);
    PL_DBtrace		= sv_dup(proto_perl->IDBtrace);
    PL_DBsignal		= sv_dup(proto_perl->IDBsignal);
    PL_lineary		= av_dup(proto_perl->Ilineary);
    PL_dbargs		= av_dup(proto_perl->Idbargs);

    /* symbol tables */
    PL_defstash		= hv_dup_inc(proto_perl->Tdefstash);
    PL_curstash		= hv_dup(proto_perl->Tcurstash);
    PL_debstash		= hv_dup(proto_perl->Idebstash);
    PL_globalstash	= hv_dup(proto_perl->Iglobalstash);
    PL_curstname	= sv_dup_inc(proto_perl->Icurstname);

    PL_beginav		= av_dup_inc(proto_perl->Ibeginav);
    PL_endav		= av_dup_inc(proto_perl->Iendav);
    PL_checkav		= av_dup_inc(proto_perl->Icheckav);
    PL_initav		= av_dup_inc(proto_perl->Iinitav);

    PL_sub_generation	= proto_perl->Isub_generation;

    /* funky return mechanisms */
    PL_forkprocess	= proto_perl->Iforkprocess;

    /* subprocess state */
    PL_fdpid		= av_dup_inc(proto_perl->Ifdpid);

    /* internal state */
    PL_tainting		= proto_perl->Itainting;
    PL_maxo		= proto_perl->Imaxo;
    if (proto_perl->Iop_mask)
	PL_op_mask	= SAVEPVN(proto_perl->Iop_mask, PL_maxo);
    else
	PL_op_mask 	= Nullch;

    /* current interpreter roots */
    PL_main_cv		= cv_dup_inc(proto_perl->Imain_cv);
    PL_main_root	= OpREFCNT_inc(proto_perl->Imain_root);
    PL_main_start	= proto_perl->Imain_start;
    PL_eval_root	= proto_perl->Ieval_root;
    PL_eval_start	= proto_perl->Ieval_start;

    /* runtime control stuff */
    PL_curcopdb		= (COP*)any_dup(proto_perl->Icurcopdb, proto_perl);
    PL_copline		= proto_perl->Icopline;

    PL_filemode		= proto_perl->Ifilemode;
    PL_lastfd		= proto_perl->Ilastfd;
    PL_oldname		= proto_perl->Ioldname;		/* XXX not quite right */
    PL_Argv		= NULL;
    PL_Cmd		= Nullch;
    PL_gensym		= proto_perl->Igensym;
    PL_preambled	= proto_perl->Ipreambled;
    PL_preambleav	= av_dup_inc(proto_perl->Ipreambleav);
    PL_laststatval	= proto_perl->Ilaststatval;
    PL_laststype	= proto_perl->Ilaststype;
    PL_mess_sv		= Nullsv;

    PL_orslen		= proto_perl->Iorslen;
    PL_ors		= SAVEPVN(proto_perl->Iors, PL_orslen);
    PL_ofmt		= SAVEPV(proto_perl->Iofmt);

    /* interpreter atexit processing */
    PL_exitlistlen	= proto_perl->Iexitlistlen;
    if (PL_exitlistlen) {
	New(0, PL_exitlist, PL_exitlistlen, PerlExitListEntry);
	Copy(proto_perl->Iexitlist, PL_exitlist, PL_exitlistlen, PerlExitListEntry);
    }
    else
	PL_exitlist	= (PerlExitListEntry*)NULL;
    PL_modglobal	= hv_dup_inc(proto_perl->Imodglobal);

    PL_profiledata	= NULL;
    PL_rsfp		= fp_dup(proto_perl->Irsfp, '<');
    /* PL_rsfp_filters entries have fake IoDIRP() */
    PL_rsfp_filters	= av_dup_inc(proto_perl->Irsfp_filters);

    PL_compcv			= cv_dup(proto_perl->Icompcv);
    PL_comppad			= av_dup(proto_perl->Icomppad);
    PL_comppad_name		= av_dup(proto_perl->Icomppad_name);
    PL_comppad_name_fill	= proto_perl->Icomppad_name_fill;
    PL_comppad_name_floor	= proto_perl->Icomppad_name_floor;
    PL_curpad			= (SV**)ptr_table_fetch(PL_ptr_table,
							proto_perl->Tcurpad);

#ifdef HAVE_INTERP_INTERN
    sys_intern_dup(&proto_perl->Isys_intern, &PL_sys_intern);
#endif

    /* more statics moved here */
    PL_generation	= proto_perl->Igeneration;
    PL_DBcv		= cv_dup(proto_perl->IDBcv);

    PL_in_clean_objs	= proto_perl->Iin_clean_objs;
    PL_in_clean_all	= proto_perl->Iin_clean_all;

    PL_uid		= proto_perl->Iuid;
    PL_euid		= proto_perl->Ieuid;
    PL_gid		= proto_perl->Igid;
    PL_egid		= proto_perl->Iegid;
    PL_nomemok		= proto_perl->Inomemok;
    PL_an		= proto_perl->Ian;
    PL_cop_seqmax	= proto_perl->Icop_seqmax;
    PL_op_seqmax	= proto_perl->Iop_seqmax;
    PL_evalseq		= proto_perl->Ievalseq;
    PL_origenviron	= proto_perl->Iorigenviron;	/* XXX not quite right */
    PL_origalen		= proto_perl->Iorigalen;
    PL_pidstatus	= newHV();			/* XXX flag for cloning? */
    PL_osname		= SAVEPV(proto_perl->Iosname);
    PL_sh_path		= SAVEPV(proto_perl->Ish_path);
    PL_sighandlerp	= proto_perl->Isighandlerp;


    PL_runops		= proto_perl->Irunops;

    Copy(proto_perl->Itokenbuf, PL_tokenbuf, 256, char);

#ifdef CSH
    PL_cshlen		= proto_perl->Icshlen;
    PL_cshname		= SAVEPVN(proto_perl->Icshname, PL_cshlen);
#endif

    PL_lex_state	= proto_perl->Ilex_state;
    PL_lex_defer	= proto_perl->Ilex_defer;
    PL_lex_expect	= proto_perl->Ilex_expect;
    PL_lex_formbrack	= proto_perl->Ilex_formbrack;
    PL_lex_dojoin	= proto_perl->Ilex_dojoin;
    PL_lex_starts	= proto_perl->Ilex_starts;
    PL_lex_stuff	= sv_dup_inc(proto_perl->Ilex_stuff);
    PL_lex_repl		= sv_dup_inc(proto_perl->Ilex_repl);
    PL_lex_op		= proto_perl->Ilex_op;
    PL_lex_inpat	= proto_perl->Ilex_inpat;
    PL_lex_inwhat	= proto_perl->Ilex_inwhat;
    PL_lex_brackets	= proto_perl->Ilex_brackets;
    i = (PL_lex_brackets < 120 ? 120 : PL_lex_brackets);
    PL_lex_brackstack	= SAVEPVN(proto_perl->Ilex_brackstack,i);
    PL_lex_casemods	= proto_perl->Ilex_casemods;
    i = (PL_lex_casemods < 12 ? 12 : PL_lex_casemods);
    PL_lex_casestack	= SAVEPVN(proto_perl->Ilex_casestack,i);

    Copy(proto_perl->Inextval, PL_nextval, 5, YYSTYPE);
    Copy(proto_perl->Inexttype, PL_nexttype, 5,	I32);
    PL_nexttoke		= proto_perl->Inexttoke;

    PL_linestr		= sv_dup_inc(proto_perl->Ilinestr);
    i = proto_perl->Ibufptr - SvPVX(proto_perl->Ilinestr);
    PL_bufptr		= SvPVX(PL_linestr) + (i < 0 ? 0 : i);
    i = proto_perl->Ioldbufptr - SvPVX(proto_perl->Ilinestr);
    PL_oldbufptr	= SvPVX(PL_linestr) + (i < 0 ? 0 : i);
    i = proto_perl->Ioldoldbufptr - SvPVX(proto_perl->Ilinestr);
    PL_oldoldbufptr	= SvPVX(PL_linestr) + (i < 0 ? 0 : i);
    PL_bufend		= SvPVX(PL_linestr) + SvCUR(PL_linestr);
    i = proto_perl->Ilinestart - SvPVX(proto_perl->Ilinestr);
    PL_linestart	= SvPVX(PL_linestr) + (i < 0 ? 0 : i);
    PL_pending_ident	= proto_perl->Ipending_ident;
    PL_sublex_info	= proto_perl->Isublex_info;	/* XXX not quite right */

    PL_expect		= proto_perl->Iexpect;

    PL_multi_start	= proto_perl->Imulti_start;
    PL_multi_end	= proto_perl->Imulti_end;
    PL_multi_open	= proto_perl->Imulti_open;
    PL_multi_close	= proto_perl->Imulti_close;

    PL_error_count	= proto_perl->Ierror_count;
    PL_subline		= proto_perl->Isubline;
    PL_subname		= sv_dup_inc(proto_perl->Isubname);

    PL_min_intro_pending	= proto_perl->Imin_intro_pending;
    PL_max_intro_pending	= proto_perl->Imax_intro_pending;
    PL_padix			= proto_perl->Ipadix;
    PL_padix_floor		= proto_perl->Ipadix_floor;
    PL_pad_reset_pending	= proto_perl->Ipad_reset_pending;

    i = proto_perl->Ilast_uni - SvPVX(proto_perl->Ilinestr);
    PL_last_uni		= SvPVX(PL_linestr) + (i < 0 ? 0 : i);
    i = proto_perl->Ilast_lop - SvPVX(proto_perl->Ilinestr);
    PL_last_lop		= SvPVX(PL_linestr) + (i < 0 ? 0 : i);
    PL_last_lop_op	= proto_perl->Ilast_lop_op;
    PL_in_my		= proto_perl->Iin_my;
    PL_in_my_stash	= hv_dup(proto_perl->Iin_my_stash);
#ifdef FCRYPT
    PL_cryptseen	= proto_perl->Icryptseen;
#endif

    PL_hints		= proto_perl->Ihints;

    PL_amagic_generation	= proto_perl->Iamagic_generation;

#ifdef USE_LOCALE_COLLATE
    PL_collation_ix	= proto_perl->Icollation_ix;
    PL_collation_name	= SAVEPV(proto_perl->Icollation_name);
    PL_collation_standard	= proto_perl->Icollation_standard;
    PL_collxfrm_base	= proto_perl->Icollxfrm_base;
    PL_collxfrm_mult	= proto_perl->Icollxfrm_mult;
#endif /* USE_LOCALE_COLLATE */

#ifdef USE_LOCALE_NUMERIC
    PL_numeric_name	= SAVEPV(proto_perl->Inumeric_name);
    PL_numeric_standard	= proto_perl->Inumeric_standard;
    PL_numeric_local	= proto_perl->Inumeric_local;
    PL_numeric_radix_sv	= sv_dup_inc(proto_perl->Inumeric_radix_sv);
#endif /* !USE_LOCALE_NUMERIC */

    /* utf8 character classes */
    PL_utf8_alnum	= sv_dup_inc(proto_perl->Iutf8_alnum);
    PL_utf8_alnumc	= sv_dup_inc(proto_perl->Iutf8_alnumc);
    PL_utf8_ascii	= sv_dup_inc(proto_perl->Iutf8_ascii);
    PL_utf8_alpha	= sv_dup_inc(proto_perl->Iutf8_alpha);
    PL_utf8_space	= sv_dup_inc(proto_perl->Iutf8_space);
    PL_utf8_cntrl	= sv_dup_inc(proto_perl->Iutf8_cntrl);
    PL_utf8_graph	= sv_dup_inc(proto_perl->Iutf8_graph);
    PL_utf8_digit	= sv_dup_inc(proto_perl->Iutf8_digit);
    PL_utf8_upper	= sv_dup_inc(proto_perl->Iutf8_upper);
    PL_utf8_lower	= sv_dup_inc(proto_perl->Iutf8_lower);
    PL_utf8_print	= sv_dup_inc(proto_perl->Iutf8_print);
    PL_utf8_punct	= sv_dup_inc(proto_perl->Iutf8_punct);
    PL_utf8_xdigit	= sv_dup_inc(proto_perl->Iutf8_xdigit);
    PL_utf8_mark	= sv_dup_inc(proto_perl->Iutf8_mark);
    PL_utf8_toupper	= sv_dup_inc(proto_perl->Iutf8_toupper);
    PL_utf8_totitle	= sv_dup_inc(proto_perl->Iutf8_totitle);
    PL_utf8_tolower	= sv_dup_inc(proto_perl->Iutf8_tolower);

    /* swatch cache */
    PL_last_swash_hv	= Nullhv;	/* reinits on demand */
    PL_last_swash_klen	= 0;
    PL_last_swash_key[0]= '\0';
    PL_last_swash_tmps	= (U8*)NULL;
    PL_last_swash_slen	= 0;

    /* perly.c globals */
    PL_yydebug		= proto_perl->Iyydebug;
    PL_yynerrs		= proto_perl->Iyynerrs;
    PL_yyerrflag	= proto_perl->Iyyerrflag;
    PL_yychar		= proto_perl->Iyychar;
    PL_yyval		= proto_perl->Iyyval;
    PL_yylval		= proto_perl->Iyylval;

    PL_glob_index	= proto_perl->Iglob_index;
    PL_srand_called	= proto_perl->Isrand_called;
    PL_uudmap['M']	= 0;		/* reinits on demand */
    PL_bitcount		= Nullch;	/* reinits on demand */

    if (proto_perl->Ipsig_ptr) {
	int sig_num[] = { SIG_NUM };
	Newz(0, PL_psig_ptr, sizeof(sig_num)/sizeof(*sig_num), SV*);
	Newz(0, PL_psig_name, sizeof(sig_num)/sizeof(*sig_num), SV*);
	for (i = 1; PL_sig_name[i]; i++) {
	    PL_psig_ptr[i] = sv_dup_inc(proto_perl->Ipsig_ptr[i]);
	    PL_psig_name[i] = sv_dup_inc(proto_perl->Ipsig_name[i]);
	}
    }
    else {
	PL_psig_ptr	= (SV**)NULL;
	PL_psig_name	= (SV**)NULL;
    }

    /* thrdvar.h stuff */

    if (flags & CLONEf_COPY_STACKS) {
	/* next allocation will be PL_tmps_stack[PL_tmps_ix+1] */
	PL_tmps_ix		= proto_perl->Ttmps_ix;
	PL_tmps_max		= proto_perl->Ttmps_max;
	PL_tmps_floor		= proto_perl->Ttmps_floor;
	Newz(50, PL_tmps_stack, PL_tmps_max, SV*);
	i = 0;
	while (i <= PL_tmps_ix) {
	    PL_tmps_stack[i]	= sv_dup_inc(proto_perl->Ttmps_stack[i]);
	    ++i;
	}

	/* next PUSHMARK() sets *(PL_markstack_ptr+1) */
	i = proto_perl->Tmarkstack_max - proto_perl->Tmarkstack;
	Newz(54, PL_markstack, i, I32);
	PL_markstack_max	= PL_markstack + (proto_perl->Tmarkstack_max
						  - proto_perl->Tmarkstack);
	PL_markstack_ptr	= PL_markstack + (proto_perl->Tmarkstack_ptr
						  - proto_perl->Tmarkstack);
	Copy(proto_perl->Tmarkstack, PL_markstack,
	     PL_markstack_ptr - PL_markstack + 1, I32);

	/* next push_scope()/ENTER sets PL_scopestack[PL_scopestack_ix]
	 * NOTE: unlike the others! */
	PL_scopestack_ix	= proto_perl->Tscopestack_ix;
	PL_scopestack_max	= proto_perl->Tscopestack_max;
	Newz(54, PL_scopestack, PL_scopestack_max, I32);
	Copy(proto_perl->Tscopestack, PL_scopestack, PL_scopestack_ix, I32);

	/* next push_return() sets PL_retstack[PL_retstack_ix]
	 * NOTE: unlike the others! */
	PL_retstack_ix		= proto_perl->Tretstack_ix;
	PL_retstack_max		= proto_perl->Tretstack_max;
	Newz(54, PL_retstack, PL_retstack_max, OP*);
	Copy(proto_perl->Tretstack, PL_retstack, PL_retstack_ix, I32);

	/* NOTE: si_dup() looks at PL_markstack */
	PL_curstackinfo		= si_dup(proto_perl->Tcurstackinfo);

	/* PL_curstack		= PL_curstackinfo->si_stack; */
	PL_curstack		= av_dup(proto_perl->Tcurstack);
	PL_mainstack		= av_dup(proto_perl->Tmainstack);

	/* next PUSHs() etc. set *(PL_stack_sp+1) */
	PL_stack_base		= AvARRAY(PL_curstack);
	PL_stack_sp		= PL_stack_base + (proto_perl->Tstack_sp
						   - proto_perl->Tstack_base);
	PL_stack_max		= PL_stack_base + AvMAX(PL_curstack);

	/* next SSPUSHFOO() sets PL_savestack[PL_savestack_ix]
	 * NOTE: unlike the others! */
	PL_savestack_ix		= proto_perl->Tsavestack_ix;
	PL_savestack_max	= proto_perl->Tsavestack_max;
	/*Newz(54, PL_savestack, PL_savestack_max, ANY);*/
	PL_savestack		= ss_dup(proto_perl);
    }
    else {
	init_stacks();
	ENTER;			/* perl_destruct() wants to LEAVE; */
    }

    PL_start_env	= proto_perl->Tstart_env;	/* XXXXXX */
    PL_top_env		= &PL_start_env;

    PL_op		= proto_perl->Top;

    PL_Sv		= Nullsv;
    PL_Xpv		= (XPV*)NULL;
    PL_na		= proto_perl->Tna;

    PL_statbuf		= proto_perl->Tstatbuf;
    PL_statcache	= proto_perl->Tstatcache;
    PL_statgv		= gv_dup(proto_perl->Tstatgv);
    PL_statname		= sv_dup_inc(proto_perl->Tstatname);
#ifdef HAS_TIMES
    PL_timesbuf		= proto_perl->Ttimesbuf;
#endif

    PL_tainted		= proto_perl->Ttainted;
    PL_curpm		= proto_perl->Tcurpm;	/* XXX No PMOP ref count */
    PL_nrs		= sv_dup_inc(proto_perl->Tnrs);
    PL_rs		= sv_dup_inc(proto_perl->Trs);
    PL_last_in_gv	= gv_dup(proto_perl->Tlast_in_gv);
    PL_ofslen		= proto_perl->Tofslen;
    PL_ofs		= SAVEPVN(proto_perl->Tofs, PL_ofslen);
    PL_defoutgv		= gv_dup_inc(proto_perl->Tdefoutgv);
    PL_chopset		= proto_perl->Tchopset;	/* XXX never deallocated */
    PL_toptarget	= sv_dup_inc(proto_perl->Ttoptarget);
    PL_bodytarget	= sv_dup_inc(proto_perl->Tbodytarget);
    PL_formtarget	= sv_dup(proto_perl->Tformtarget);

    PL_restartop	= proto_perl->Trestartop;
    PL_in_eval		= proto_perl->Tin_eval;
    PL_delaymagic	= proto_perl->Tdelaymagic;
    PL_dirty		= proto_perl->Tdirty;
    PL_localizing	= proto_perl->Tlocalizing;

#ifdef PERL_FLEXIBLE_EXCEPTIONS
    PL_protect		= proto_perl->Tprotect;
#endif
    PL_errors		= sv_dup_inc(proto_perl->Terrors);
    PL_av_fetch_sv	= Nullsv;
    PL_hv_fetch_sv	= Nullsv;
    Zero(&PL_hv_fetch_ent_mh, 1, HE);			/* XXX */
    PL_modcount		= proto_perl->Tmodcount;
    PL_lastgotoprobe	= Nullop;
    PL_dumpindent	= proto_perl->Tdumpindent;

    PL_sortcop		= (OP*)any_dup(proto_perl->Tsortcop, proto_perl);
    PL_sortstash	= hv_dup(proto_perl->Tsortstash);
    PL_firstgv		= gv_dup(proto_perl->Tfirstgv);
    PL_secondgv		= gv_dup(proto_perl->Tsecondgv);
    PL_sortcxix		= proto_perl->Tsortcxix;
    PL_efloatbuf	= Nullch;		/* reinits on demand */
    PL_efloatsize	= 0;			/* reinits on demand */

    /* regex stuff */

    PL_screamfirst	= NULL;
    PL_screamnext	= NULL;
    PL_maxscream	= -1;			/* reinits on demand */
    PL_lastscream	= Nullsv;

    PL_watchaddr	= NULL;
    PL_watchok		= Nullch;

    PL_regdummy		= proto_perl->Tregdummy;
    PL_regcomp_parse	= Nullch;
    PL_regxend		= Nullch;
    PL_regcode		= (regnode*)NULL;
    PL_regnaughty	= 0;
    PL_regsawback	= 0;
    PL_regprecomp	= Nullch;
    PL_regnpar		= 0;
    PL_regsize		= 0;
    PL_regflags		= 0;
    PL_regseen		= 0;
    PL_seen_zerolen	= 0;
    PL_seen_evals	= 0;
    PL_regcomp_rx	= (regexp*)NULL;
    PL_extralen		= 0;
    PL_colorset		= 0;		/* reinits PL_colors[] */
    /*PL_colors[6]	= {0,0,0,0,0,0};*/
    PL_reg_whilem_seen	= 0;
    PL_reginput		= Nullch;
    PL_regbol		= Nullch;
    PL_regeol		= Nullch;
    PL_regstartp	= (I32*)NULL;
    PL_regendp		= (I32*)NULL;
    PL_reglastparen	= (U32*)NULL;
    PL_regtill		= Nullch;
    PL_regprev		= '\n';
    PL_reg_start_tmp	= (char**)NULL;
    PL_reg_start_tmpl	= 0;
    PL_regdata		= (struct reg_data*)NULL;
    PL_bostr		= Nullch;
    PL_reg_flags	= 0;
    PL_reg_eval_set	= 0;
    PL_regnarrate	= 0;
    PL_regprogram	= (regnode*)NULL;
    PL_regindent	= 0;
    PL_regcc		= (CURCUR*)NULL;
    PL_reg_call_cc	= (struct re_cc_state*)NULL;
    PL_reg_re		= (regexp*)NULL;
    PL_reg_ganch	= Nullch;
    PL_reg_sv		= Nullsv;
    PL_reg_magic	= (MAGIC*)NULL;
    PL_reg_oldpos	= 0;
    PL_reg_oldcurpm	= (PMOP*)NULL;
    PL_reg_curpm	= (PMOP*)NULL;
    PL_reg_oldsaved	= Nullch;
    PL_reg_oldsavedlen	= 0;
    PL_reg_maxiter	= 0;
    PL_reg_leftiter	= 0;
    PL_reg_poscache	= Nullch;
    PL_reg_poscache_size= 0;

    /* RE engine - function pointers */
    PL_regcompp		= proto_perl->Tregcompp;
    PL_regexecp		= proto_perl->Tregexecp;
    PL_regint_start	= proto_perl->Tregint_start;
    PL_regint_string	= proto_perl->Tregint_string;
    PL_regfree		= proto_perl->Tregfree;

    PL_reginterp_cnt	= 0;
    PL_reg_starttry	= 0;

    if (!(flags & CLONEf_KEEP_PTR_TABLE)) {
        ptr_table_free(PL_ptr_table);
        PL_ptr_table = NULL;
    }

#ifdef PERL_OBJECT
    return (PerlInterpreter*)pPerl;
#else
    return my_perl;
#endif
}

#else	/* !USE_ITHREADS */

#ifdef PERL_OBJECT
#include "XSUB.h"
#endif

#endif /* USE_ITHREADS */

static void
do_report_used(pTHXo_ SV *sv)
{
    if (SvTYPE(sv) != SVTYPEMASK) {
	PerlIO_printf(Perl_debug_log, "****\n");
	sv_dump(sv);
    }
}

static void
do_clean_objs(pTHXo_ SV *sv)
{
    SV* rv;

    if (SvROK(sv) && SvOBJECT(rv = SvRV(sv))) {
	DEBUG_D((PerlIO_printf(Perl_debug_log, "Cleaning object ref:\n "), sv_dump(sv));)
	if (SvWEAKREF(sv)) {
	    sv_del_backref(sv);
	    SvWEAKREF_off(sv);
	    SvRV(sv) = 0;
	} else {
	    SvROK_off(sv);
	    SvRV(sv) = 0;
	    SvREFCNT_dec(rv);
	}
    }

    /* XXX Might want to check arrays, etc. */
}

#ifndef DISABLE_DESTRUCTOR_KLUDGE
static void
do_clean_named_objs(pTHXo_ SV *sv)
{
    if (SvTYPE(sv) == SVt_PVGV && GvGP(sv)) {
	if ( SvOBJECT(GvSV(sv)) ||
	     (GvAV(sv) && SvOBJECT(GvAV(sv))) ||
	     (GvHV(sv) && SvOBJECT(GvHV(sv))) ||
	     (GvIO(sv) && SvOBJECT(GvIO(sv))) ||
	     (GvCV(sv) && SvOBJECT(GvCV(sv))) )
	{
	    DEBUG_D((PerlIO_printf(Perl_debug_log, "Cleaning named glob object:\n "), sv_dump(sv));)
	    SvREFCNT_dec(sv);
	}
    }
}
#endif

static void
do_clean_all(pTHXo_ SV *sv)
{
    DEBUG_D((PerlIO_printf(Perl_debug_log, "Cleaning loops: SV at 0x%"UVxf"\n", PTR2UV(sv)) );)
    SvFLAGS(sv) |= SVf_BREAK;
    SvREFCNT_dec(sv);
}

