/*    sv.h
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#ifdef sv_flags
#undef sv_flags		/* Convex has this in <signal.h> for sigvec() */
#endif

typedef enum {
	SVt_NULL,	/* 0 */
	SVt_IV,		/* 1 */
	SVt_NV,		/* 2 */
	SVt_RV,		/* 3 */
	SVt_PV,		/* 4 */
	SVt_PVIV,	/* 5 */
	SVt_PVNV,	/* 6 */
	SVt_PVMG,	/* 7 */
	SVt_PVBM,	/* 8 */
	SVt_PVLV,	/* 9 */
	SVt_PVAV,	/* 10 */
	SVt_PVHV,	/* 11 */
	SVt_PVCV,	/* 12 */
	SVt_PVGV,	/* 13 */
	SVt_PVFM,	/* 14 */
	SVt_PVIO	/* 15 */
} svtype;

/* Using C's structural equivalence to help emulate C++ inheritance here... */

struct sv {
    void*	sv_any;		/* pointer to something */
    U32		sv_refcnt;	/* how many references to us */
    U32		sv_flags;	/* what we are */
};

struct gv {
    XPVGV*	sv_any;		/* pointer to something */
    U32		sv_refcnt;	/* how many references to us */
    U32		sv_flags;	/* what we are */
};

struct cv {
    XPVCV*	sv_any;		/* pointer to something */
    U32		sv_refcnt;	/* how many references to us */
    U32		sv_flags;	/* what we are */
};

struct av {
    XPVAV*	sv_any;		/* pointer to something */
    U32		sv_refcnt;	/* how many references to us */
    U32		sv_flags;	/* what we are */
};

struct hv {
    XPVHV*	sv_any;		/* pointer to something */
    U32		sv_refcnt;	/* how many references to us */
    U32		sv_flags;	/* what we are */
};

struct io {
    XPVIO*	sv_any;		/* pointer to something */
    U32		sv_refcnt;	/* how many references to us */
    U32		sv_flags;	/* what we are */
};

#define SvANY(sv)	(sv)->sv_any
#define SvFLAGS(sv)	(sv)->sv_flags
#define SvREFCNT(sv)	(sv)->sv_refcnt

#ifdef USE_THREADS

#  ifdef EMULATE_ATOMIC_REFCOUNTS
#    define ATOMIC_INC(count) STMT_START {	\
	MUTEX_LOCK(&PL_svref_mutex);		\
	++count;				\
	MUTEX_UNLOCK(&PL_svref_mutex);		\
     } STMT_END
#    define ATOMIC_DEC_AND_TEST(res,count) STMT_START {	\
	MUTEX_LOCK(&PL_svref_mutex);			\
	res = (--count == 0);				\
	MUTEX_UNLOCK(&PL_svref_mutex);			\
     } STMT_END
#  else
#    define ATOMIC_INC(count) atomic_inc(&count)
#    define ATOMIC_DEC_AND_TEST(res,count) (res = atomic_dec_and_test(&count))
#  endif /* EMULATE_ATOMIC_REFCOUNTS */
#else
#  define ATOMIC_INC(count) (++count)
#  define ATOMIC_DEC_AND_TEST(res, count) (res = (--count == 0))
#endif /* USE_THREADS */

#ifdef __GNUC__
#  define SvREFCNT_inc(sv)		\
    ({					\
	SV *nsv = (SV*)(sv);		\
	if (nsv)			\
	     ATOMIC_INC(SvREFCNT(nsv));	\
	nsv;				\
    })
#else
#  if defined(CRIPPLED_CC) || defined(USE_THREADS)
#    define SvREFCNT_inc(sv) sv_newref((SV*)sv)
#  else
#    define SvREFCNT_inc(sv)	\
	((PL_Sv=(SV*)(sv)), (PL_Sv && ATOMIC_INC(SvREFCNT(PL_Sv))), (SV*)PL_Sv)
#  endif
#endif

#define SvREFCNT_dec(sv)	sv_free((SV*)sv)

#define SVTYPEMASK	0xff
#define SvTYPE(sv)	((sv)->sv_flags & SVTYPEMASK)

#define SvUPGRADE(sv, mt) (SvTYPE(sv) >= mt || sv_upgrade(sv, mt))

#define SVs_PADBUSY	0x00000100	/* reserved for tmp or my already */
#define SVs_PADTMP	0x00000200	/* in use as tmp */
#define SVs_PADMY	0x00000400	/* in use a "my" variable */
#define SVs_TEMP	0x00000800	/* string is stealable? */
#define SVs_OBJECT	0x00001000	/* is "blessed" */
#define SVs_GMG		0x00002000	/* has magical get method */
#define SVs_SMG		0x00004000	/* has magical set method */
#define SVs_RMG		0x00008000	/* has random magical methods */

#define SVf_IOK		0x00010000	/* has valid public integer value */
#define SVf_NOK		0x00020000	/* has valid public numeric value */
#define SVf_POK		0x00040000	/* has valid public pointer value */
#define SVf_ROK		0x00080000	/* has a valid reference pointer */

#define SVf_FAKE	0x00100000	/* glob or lexical is just a copy */
#define SVf_OOK		0x00200000	/* has valid offset value */
#define SVf_BREAK	0x00400000	/* refcnt is artificially low */
#define SVf_READONLY	0x00800000	/* may not be modified */

#define SVf_THINKFIRST	(SVf_READONLY|SVf_ROK)

#define SVp_IOK		0x01000000	/* has valid non-public integer value */
#define SVp_NOK		0x02000000	/* has valid non-public numeric value */
#define SVp_POK		0x04000000	/* has valid non-public pointer value */
#define SVp_SCREAM	0x08000000	/* has been studied? */

#define SVf_OK		(SVf_IOK|SVf_NOK|SVf_POK|SVf_ROK| \
			 SVp_IOK|SVp_NOK|SVp_POK)

#ifdef OVERLOAD
#define SVf_AMAGIC    0x10000000      /* has magical overloaded methods */
#else
#define SVf_AMAGIC    0               /* can be or-ed without effect */
#endif /* OVERLOAD */

#define PRIVSHIFT 8

/* Some private flags. */

#define SVpfm_COMPILED	0x80000000

#define SVpbm_VALID	0x80000000
#define SVpbm_TAIL	0x40000000

#define SVphv_SHAREKEYS 0x20000000	/* keys live on shared string table */
#define SVphv_LAZYDEL	0x40000000	/* entry in xhv_eiter must be deleted */

struct xrv {
    SV *	xrv_rv;		/* pointer to another SV */
};

struct xpv {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
};

struct xpviv {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xiv_iv;		/* integer value or pv offset */
};

struct xpvuv {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    UV		xuv_uv;		/* unsigned value or pv offset */
};

struct xpvnv {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xiv_iv;		/* integer value or pv offset */
    double	xnv_nv;		/* numeric value, if any */
};

/* These structure must match the beginning of struct xpvhv in hv.h. */
struct xpvmg {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xiv_iv;		/* integer value or pv offset */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* linked list of magicalness */
    HV*		xmg_stash;	/* class package */
};

struct xpvlv {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xiv_iv;		/* integer value or pv offset */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* linked list of magicalness */
    HV*		xmg_stash;	/* class package */

    STRLEN	xlv_targoff;
    STRLEN	xlv_targlen;
    SV*		xlv_targ;
    char	xlv_type;
};

struct xpvgv {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xiv_iv;		/* integer value or pv offset */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* linked list of magicalness */
    HV*		xmg_stash;	/* class package */

    GP*		xgv_gp;
    char*	xgv_name;
    STRLEN	xgv_namelen;
    HV*		xgv_stash;
    U8		xgv_flags;
};

struct xpvbm {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xiv_iv;		/* integer value or pv offset */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* linked list of magicalness */
    HV*		xmg_stash;	/* class package */

    I32		xbm_useful;	/* is this constant pattern being useful? */
    U16		xbm_previous;	/* how many characters in string before rare? */
    U8		xbm_rare;	/* rarest character in string */
};

/* This structure much match XPVCV */

typedef U16 cv_flags_t;

struct xpvfm {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xiv_iv;		/* integer value or pv offset */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* linked list of magicalness */
    HV*		xmg_stash;	/* class package */

    HV *	xcv_stash;
    OP *	xcv_start;
    OP *	xcv_root;
    void      (*xcv_xsub)_((CV* _CPERLproto));
    ANY		xcv_xsubany;
    GV *	xcv_gv;
    GV *	xcv_filegv;
    long	xcv_depth;		/* >= 2 indicates recursive call */
    AV *	xcv_padlist;
    CV *	xcv_outside;
#ifdef USE_THREADS
    perl_mutex *xcv_mutexp;	/* protects xcv_owner */
    struct perl_thread *xcv_owner;	/* current owner thread */
#endif /* USE_THREADS */
    cv_flags_t	xcv_flags;

    I32		xfm_lines;
};

struct xpvio {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xpv_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xiv_iv;		/* integer value or pv offset */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* linked list of magicalness */
    HV*		xmg_stash;	/* class package */

    PerlIO *	xio_ifp;	/* ifp and ofp are normally the same */
    PerlIO *	xio_ofp;	/* but sockets need separate streams */
    DIR *	xio_dirp;	/* for opendir, readdir, etc */
    long	xio_lines;	/* $. */
    long	xio_page;	/* $% */
    long	xio_page_len;	/* $= */
    long	xio_lines_left;	/* $- */
    char *	xio_top_name;	/* $^ */
    GV *	xio_top_gv;	/* $^ */
    char *	xio_fmt_name;	/* $~ */
    GV *	xio_fmt_gv;	/* $~ */
    char *	xio_bottom_name;/* $^B */
    GV *	xio_bottom_gv;	/* $^B */
    short	xio_subprocess;	/* -| or |- */
    char	xio_type;
    char	xio_flags;
};

#define IOf_ARGV 1	/* this fp iterates over ARGV */
#define IOf_START 2	/* check for null ARGV and substitute '-' */
#define IOf_FLUSH 4	/* this fp wants a flush after write op */
#define IOf_DIDTOP 8	/* just did top of form */
#define IOf_UNTAINT 16  /* consider this fp (and its data) "safe" */
#define IOf_NOLINE  32	/* slurped a pseudo-line from empty file */

/* The following macros define implementation-independent predicates on SVs. */

#define SvNIOK(sv)		(SvFLAGS(sv) & (SVf_IOK|SVf_NOK))
#define SvNIOKp(sv)		(SvFLAGS(sv) & (SVp_IOK|SVp_NOK))
#define SvNIOK_off(sv)		(SvFLAGS(sv) &= ~(SVf_IOK|SVf_NOK| \
						  SVp_IOK|SVp_NOK))

#define SvOK(sv)		(SvFLAGS(sv) & SVf_OK)
#define SvOK_off(sv)		(SvFLAGS(sv) &=	~(SVf_OK|SVf_AMAGIC),	\
							SvOOK_off(sv))

#define SvOKp(sv)		(SvFLAGS(sv) & (SVp_IOK|SVp_NOK|SVp_POK))
#define SvIOKp(sv)		(SvFLAGS(sv) & SVp_IOK)
#define SvIOKp_on(sv)		(SvOOK_off(sv), SvFLAGS(sv) |= SVp_IOK)
#define SvNOKp(sv)		(SvFLAGS(sv) & SVp_NOK)
#define SvNOKp_on(sv)		(SvFLAGS(sv) |= SVp_NOK)
#define SvPOKp(sv)		(SvFLAGS(sv) & SVp_POK)
#define SvPOKp_on(sv)		(SvFLAGS(sv) |= SVp_POK)

#define SvIOK(sv)		(SvFLAGS(sv) & SVf_IOK)
#define SvIOK_on(sv)		(SvOOK_off(sv), \
				    SvFLAGS(sv) |= (SVf_IOK|SVp_IOK))
#define SvIOK_off(sv)		(SvFLAGS(sv) &= ~(SVf_IOK|SVp_IOK))
#define SvIOK_only(sv)		(SvOK_off(sv), \
				    SvFLAGS(sv) |= (SVf_IOK|SVp_IOK))

#define SvNOK(sv)		(SvFLAGS(sv) & SVf_NOK)
#define SvNOK_on(sv)		(SvFLAGS(sv) |= (SVf_NOK|SVp_NOK))
#define SvNOK_off(sv)		(SvFLAGS(sv) &= ~(SVf_NOK|SVp_NOK))
#define SvNOK_only(sv)		(SvOK_off(sv), \
				    SvFLAGS(sv) |= (SVf_NOK|SVp_NOK))

#define SvPOK(sv)		(SvFLAGS(sv) & SVf_POK)
#define SvPOK_on(sv)		(SvFLAGS(sv) |= (SVf_POK|SVp_POK))
#define SvPOK_off(sv)		(SvFLAGS(sv) &= ~(SVf_POK|SVp_POK))
#define SvPOK_only(sv)		(SvFLAGS(sv) &= ~(SVf_OK|SVf_AMAGIC),	\
				    SvFLAGS(sv) |= (SVf_POK|SVp_POK))

#define SvOOK(sv)		(SvFLAGS(sv) & SVf_OOK)
#define SvOOK_on(sv)		(SvIOK_off(sv), SvFLAGS(sv) |= SVf_OOK)
#define SvOOK_off(sv)		(SvOOK(sv) && sv_backoff(sv))

#define SvFAKE(sv)		(SvFLAGS(sv) & SVf_FAKE)
#define SvFAKE_on(sv)		(SvFLAGS(sv) |= SVf_FAKE)
#define SvFAKE_off(sv)		(SvFLAGS(sv) &= ~SVf_FAKE)

#define SvROK(sv)		(SvFLAGS(sv) & SVf_ROK)
#define SvROK_on(sv)		(SvFLAGS(sv) |= SVf_ROK)
#define SvROK_off(sv)		(SvFLAGS(sv) &= ~(SVf_ROK|SVf_AMAGIC))

#define SvMAGICAL(sv)		(SvFLAGS(sv) & (SVs_GMG|SVs_SMG|SVs_RMG))
#define SvMAGICAL_on(sv)	(SvFLAGS(sv) |= (SVs_GMG|SVs_SMG|SVs_RMG))
#define SvMAGICAL_off(sv)	(SvFLAGS(sv) &= ~(SVs_GMG|SVs_SMG|SVs_RMG))

#define SvGMAGICAL(sv)		(SvFLAGS(sv) & SVs_GMG)
#define SvGMAGICAL_on(sv)	(SvFLAGS(sv) |= SVs_GMG)
#define SvGMAGICAL_off(sv)	(SvFLAGS(sv) &= ~SVs_GMG)

#define SvSMAGICAL(sv)		(SvFLAGS(sv) & SVs_SMG)
#define SvSMAGICAL_on(sv)	(SvFLAGS(sv) |= SVs_SMG)
#define SvSMAGICAL_off(sv)	(SvFLAGS(sv) &= ~SVs_SMG)

#define SvRMAGICAL(sv)		(SvFLAGS(sv) & SVs_RMG)
#define SvRMAGICAL_on(sv)	(SvFLAGS(sv) |= SVs_RMG)
#define SvRMAGICAL_off(sv)	(SvFLAGS(sv) &= ~SVs_RMG)

#ifdef OVERLOAD
#define SvAMAGIC(sv)         (SvFLAGS(sv) & SVf_AMAGIC)
#define SvAMAGIC_on(sv)      (SvFLAGS(sv) |= SVf_AMAGIC)
#define SvAMAGIC_off(sv)     (SvFLAGS(sv) &= ~SVf_AMAGIC)

/*
#define Gv_AMG(stash) \
        (HV_AMAGICmb(stash) && \
         ((!HV_AMAGICbad(stash) && HV_AMAGIC(stash)) || Gv_AMupdate(stash)))
*/
#define Gv_AMG(stash)           (PL_amagic_generation && Gv_AMupdate(stash))
#endif /* OVERLOAD */

#define SvTHINKFIRST(sv)	(SvFLAGS(sv) & SVf_THINKFIRST)

#define SvPADBUSY(sv)		(SvFLAGS(sv) & SVs_PADBUSY)

#define SvPADTMP(sv)		(SvFLAGS(sv) & SVs_PADTMP)
#define SvPADTMP_on(sv)		(SvFLAGS(sv) |= SVs_PADTMP|SVs_PADBUSY)
#define SvPADTMP_off(sv)	(SvFLAGS(sv) &= ~SVs_PADTMP)

#define SvPADMY(sv)		(SvFLAGS(sv) & SVs_PADMY)
#define SvPADMY_on(sv)		(SvFLAGS(sv) |= SVs_PADMY|SVs_PADBUSY)

#define SvTEMP(sv)		(SvFLAGS(sv) & SVs_TEMP)
#define SvTEMP_on(sv)		(SvFLAGS(sv) |= SVs_TEMP)
#define SvTEMP_off(sv)		(SvFLAGS(sv) &= ~SVs_TEMP)

#define SvOBJECT(sv)		(SvFLAGS(sv) & SVs_OBJECT)
#define SvOBJECT_on(sv)		(SvFLAGS(sv) |= SVs_OBJECT)
#define SvOBJECT_off(sv)	(SvFLAGS(sv) &= ~SVs_OBJECT)

#define SvREADONLY(sv)		(SvFLAGS(sv) & SVf_READONLY)
#define SvREADONLY_on(sv)	(SvFLAGS(sv) |= SVf_READONLY)
#define SvREADONLY_off(sv)	(SvFLAGS(sv) &= ~SVf_READONLY)

#define SvSCREAM(sv)		(SvFLAGS(sv) & SVp_SCREAM)
#define SvSCREAM_on(sv)		(SvFLAGS(sv) |= SVp_SCREAM)
#define SvSCREAM_off(sv)	(SvFLAGS(sv) &= ~SVp_SCREAM)

#define SvCOMPILED(sv)		(SvFLAGS(sv) & SVpfm_COMPILED)
#define SvCOMPILED_on(sv)	(SvFLAGS(sv) |= SVpfm_COMPILED)
#define SvCOMPILED_off(sv)	(SvFLAGS(sv) &= ~SVpfm_COMPILED)

#define SvTAIL(sv)		(SvFLAGS(sv) & SVpbm_TAIL)
#define SvTAIL_on(sv)		(SvFLAGS(sv) |= SVpbm_TAIL)
#define SvTAIL_off(sv)		(SvFLAGS(sv) &= ~SVpbm_TAIL)

#define SvVALID(sv)		(SvFLAGS(sv) & SVpbm_VALID)
#define SvVALID_on(sv)		(SvFLAGS(sv) |= SVpbm_VALID)
#define SvVALID_off(sv)		(SvFLAGS(sv) &= ~SVpbm_VALID)

#define SvRV(sv) ((XRV*)  SvANY(sv))->xrv_rv
#define SvRVx(sv) SvRV(sv)

#define SvIVX(sv) ((XPVIV*)  SvANY(sv))->xiv_iv
#define SvIVXx(sv) SvIVX(sv)
#define SvUVX(sv) ((XPVUV*)  SvANY(sv))->xuv_uv
#define SvUVXx(sv) SvUVX(sv)
#define SvNVX(sv)  ((XPVNV*)SvANY(sv))->xnv_nv
#define SvNVXx(sv) SvNVX(sv)
#define SvPVX(sv)  ((XPV*)  SvANY(sv))->xpv_pv
#define SvPVXx(sv) SvPVX(sv)
#define SvCUR(sv) ((XPV*)  SvANY(sv))->xpv_cur
#define SvLEN(sv) ((XPV*)  SvANY(sv))->xpv_len
#define SvLENx(sv) SvLEN(sv)
#define SvEND(sv)(((XPV*)  SvANY(sv))->xpv_pv + ((XPV*)SvANY(sv))->xpv_cur)
#define SvENDx(sv) ((PL_Sv = (sv)), SvEND(PL_Sv))
#define SvMAGIC(sv)	((XPVMG*)  SvANY(sv))->xmg_magic
#define SvSTASH(sv)	((XPVMG*)  SvANY(sv))->xmg_stash

#define SvIV_set(sv, val) \
	STMT_START { assert(SvTYPE(sv) == SVt_IV || SvTYPE(sv) >= SVt_PVIV); \
		(((XPVIV*)  SvANY(sv))->xiv_iv = val); } STMT_END
#define SvNV_set(sv, val) \
	STMT_START { assert(SvTYPE(sv) == SVt_NV || SvTYPE(sv) >= SVt_PVNV); \
		(((XPVNV*)  SvANY(sv))->xnv_nv = val); } STMT_END
#define SvPV_set(sv, val) \
	STMT_START { assert(SvTYPE(sv) >= SVt_PV); \
		(((XPV*)  SvANY(sv))->xpv_pv = val); } STMT_END
#define SvCUR_set(sv, val) \
	STMT_START { assert(SvTYPE(sv) >= SVt_PV); \
		(((XPV*)  SvANY(sv))->xpv_cur = val); } STMT_END
#define SvLEN_set(sv, val) \
	STMT_START { assert(SvTYPE(sv) >= SVt_PV); \
		(((XPV*)  SvANY(sv))->xpv_len = val); } STMT_END
#define SvEND_set(sv, val) \
	STMT_START { assert(SvTYPE(sv) >= SVt_PV); \
		(((XPV*)  SvANY(sv))->xpv_cur = val - SvPVX(sv)); } STMT_END

#define BmRARE(sv)	((XPVBM*)  SvANY(sv))->xbm_rare
#define BmUSEFUL(sv)	((XPVBM*)  SvANY(sv))->xbm_useful
#define BmPREVIOUS(sv)	((XPVBM*)  SvANY(sv))->xbm_previous

#define FmLINES(sv)	((XPVFM*)  SvANY(sv))->xfm_lines

#define LvTYPE(sv)	((XPVLV*)  SvANY(sv))->xlv_type
#define LvTARG(sv)	((XPVLV*)  SvANY(sv))->xlv_targ
#define LvTARGOFF(sv)	((XPVLV*)  SvANY(sv))->xlv_targoff
#define LvTARGLEN(sv)	((XPVLV*)  SvANY(sv))->xlv_targlen

#define IoIFP(sv)	((XPVIO*)  SvANY(sv))->xio_ifp
#define IoOFP(sv)	((XPVIO*)  SvANY(sv))->xio_ofp
#define IoDIRP(sv)	((XPVIO*)  SvANY(sv))->xio_dirp
#define IoLINES(sv)	((XPVIO*)  SvANY(sv))->xio_lines
#define IoPAGE(sv)	((XPVIO*)  SvANY(sv))->xio_page
#define IoPAGE_LEN(sv)	((XPVIO*)  SvANY(sv))->xio_page_len
#define IoLINES_LEFT(sv)((XPVIO*)  SvANY(sv))->xio_lines_left
#define IoTOP_NAME(sv)	((XPVIO*)  SvANY(sv))->xio_top_name
#define IoTOP_GV(sv)	((XPVIO*)  SvANY(sv))->xio_top_gv
#define IoFMT_NAME(sv)	((XPVIO*)  SvANY(sv))->xio_fmt_name
#define IoFMT_GV(sv)	((XPVIO*)  SvANY(sv))->xio_fmt_gv
#define IoBOTTOM_NAME(sv)((XPVIO*) SvANY(sv))->xio_bottom_name
#define IoBOTTOM_GV(sv)	((XPVIO*)  SvANY(sv))->xio_bottom_gv
#define IoSUBPROCESS(sv)((XPVIO*)  SvANY(sv))->xio_subprocess
#define IoTYPE(sv)	((XPVIO*)  SvANY(sv))->xio_type
#define IoFLAGS(sv)	((XPVIO*)  SvANY(sv))->xio_flags

#define SvTAINTED(sv)	  (SvMAGICAL(sv) && sv_tainted(sv))
#define SvTAINTED_on(sv)  STMT_START{ if(PL_tainting){sv_taint(sv);}   }STMT_END
#define SvTAINTED_off(sv) STMT_START{ if(PL_tainting){sv_untaint(sv);} }STMT_END

#define SvTAINT(sv)			\
    STMT_START {			\
	if (PL_tainting) {		\
	    dTHR;			\
	    if (PL_tainted)		\
		SvTAINTED_on(sv);	\
	}				\
    } STMT_END

#define SvPV_force(sv, lp) sv_pvn_force(sv, &lp)
#define SvPV(sv, lp) sv_pvn(sv, &lp)
#define SvIVx(sv) sv_iv(sv)
#define SvUVx(sv) sv_uv(sv)
#define SvNVx(sv) sv_nv(sv)
#define SvPVx(sv, lp) sv_pvn(sv, &lp)
#define SvPVx_force(sv, lp) sv_pvn_force(sv, &lp)
#define SvTRUEx(sv) sv_true(sv)

#define SvIV(sv) SvIVx(sv)
#define SvNV(sv) SvNVx(sv)
#define SvUV(sv) SvIVx(sv)
#define SvTRUE(sv) SvTRUEx(sv)

#ifndef CRIPPLED_CC
/* redefine some things to more efficient inlined versions */

#undef SvIV
#define SvIV(sv) (SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv))

#undef SvUV
#define SvUV(sv) (SvIOK(sv) ? SvUVX(sv) : sv_2uv(sv))

#undef SvNV
#define SvNV(sv) (SvNOK(sv) ? SvNVX(sv) : sv_2nv(sv))

#undef SvPV
#define SvPV(sv, lp) \
    (SvPOK(sv) ? ((lp = SvCUR(sv)), SvPVX(sv)) : sv_2pv(sv, &lp))

#undef SvPV_force
#define SvPV_force(sv, lp) \
    ((SvFLAGS(sv) & (SVf_POK|SVf_THINKFIRST)) == SVf_POK \
     ? ((lp = SvCUR(sv)), SvPVX(sv)) : sv_pvn_force(sv, &lp))

#ifdef __GNUC__
#  undef SvIVx
#  undef SvUVx
#  undef SvNVx
#  undef SvPVx
#  undef SvTRUE
#  undef SvTRUEx
#  define SvIVx(sv) ({SV *nsv = (SV*)(sv); SvIV(nsv); })
#  define SvUVx(sv) ({SV *nsv = (SV*)(sv); SvUV(nsv); })
#  define SvNVx(sv) ({SV *nsv = (SV*)(sv); SvNV(nsv); })
#  define SvPVx(sv, lp) ({SV *nsv = (sv); SvPV(nsv, lp); })
#  define SvTRUE(sv) (						\
    !sv								\
    ? 0								\
    :    SvPOK(sv)						\
	?   (({XPV *nxpv = (XPV*)SvANY(sv);			\
	     nxpv &&						\
	     (*nxpv->xpv_pv > '0' ||				\
	      nxpv->xpv_cur > 1 ||				\
	      (nxpv->xpv_cur && *nxpv->xpv_pv != '0')); })	\
	     ? 1						\
	     : 0)						\
	:							\
	    SvIOK(sv)						\
	    ? SvIVX(sv) != 0					\
	    :   SvNOK(sv)					\
		? SvNVX(sv) != 0.0				\
		: sv_2bool(sv) )
#  define SvTRUEx(sv) ({SV *nsv = (sv); SvTRUE(nsv); })
#else /* __GNUC__ */
#ifndef USE_THREADS
/* These inlined macros use globals, which will require a thread
 * declaration in user code, so we avoid them under threads */

#  undef SvIVx
#  undef SvUVx
#  undef SvNVx
#  undef SvPVx
#  undef SvTRUE
#  undef SvTRUEx
#  define SvIVx(sv) ((PL_Sv = (sv)), SvIV(PL_Sv))
#  define SvUVx(sv) ((PL_Sv = (sv)), SvUV(PL_Sv))
#  define SvNVx(sv) ((PL_Sv = (sv)), SvNV(PL_Sv))
#  define SvPVx(sv, lp) ((PL_Sv = (sv)), SvPV(PL_Sv, lp))
#  define SvTRUE(sv) (						\
    !sv								\
    ? 0								\
    :    SvPOK(sv)						\
	?   ((PL_Xpv = (XPV*)SvANY(sv)) &&			\
	     (*PL_Xpv->xpv_pv > '0' ||				\
	      PL_Xpv->xpv_cur > 1 ||				\
	      (PL_Xpv->xpv_cur && *PL_Xpv->xpv_pv != '0'))	\
	     ? 1						\
	     : 0)						\
	:							\
	    SvIOK(sv)						\
	    ? SvIVX(sv) != 0					\
	    :   SvNOK(sv)					\
		? SvNVX(sv) != 0.0				\
		: sv_2bool(sv) )
#  define SvTRUEx(sv) ((PL_Sv = (sv)), SvTRUE(PL_Sv))
#endif /* !USE_THREADS */
#endif /* !__GNU__ */
#endif /* !CRIPPLED_CC */

#define newRV_inc(sv)	newRV(sv)

/* the following macros update any magic values this sv is associated with */

#define SvGETMAGIC(x) STMT_START { if (SvGMAGICAL(x)) mg_get(x); } STMT_END
#define SvSETMAGIC(x) STMT_START { if (SvSMAGICAL(x)) mg_set(x); } STMT_END

#define SvSetSV_and(dst,src,finally) \
	STMT_START {					\
	    if ((dst) != (src)) {			\
		sv_setsv(dst, src);			\
		finally;				\
	    }						\
	} STMT_END
#define SvSetSV_nosteal_and(dst,src,finally) \
	STMT_START {					\
	    if ((dst) != (src)) {			\
		U32 tMpF = SvFLAGS(src) & SVs_TEMP;	\
		SvTEMP_off(src);			\
		sv_setsv(dst, src);			\
		SvFLAGS(src) |= tMpF;			\
		finally;				\
	    }						\
	} STMT_END

#define SvSetSV(dst,src) \
		SvSetSV_and(dst,src,/*nothing*/;)
#define SvSetSV_nosteal(dst,src) \
		SvSetSV_nosteal_and(dst,src,/*nothing*/;)

#define SvSetMagicSV(dst,src) \
		SvSetSV_and(dst,src,SvSETMAGIC(dst))
#define SvSetMagicSV_nosteal(dst,src) \
		SvSetSV_nosteal_and(dst,src,SvSETMAGIC(dst))

#define SvPEEK(sv) sv_peek(sv)

#define SvIMMORTAL(sv) ((sv)==&PL_sv_undef || (sv)==&PL_sv_yes || (sv)==&PL_sv_no)

#define boolSV(b) ((b) ? &PL_sv_yes : &PL_sv_no)

#define isGV(sv) (SvTYPE(sv) == SVt_PVGV)

#ifndef DOSISH
#  define SvGROW(sv,len) (SvLEN(sv) < (len) ? sv_grow(sv,len) : SvPVX(sv))
#  define Sv_Grow sv_grow
#else
    /* extra parentheses intentionally NOT placed around "len"! */
#  define SvGROW(sv,len) ((SvLEN(sv) < (unsigned long)len) \
		? sv_grow(sv,(unsigned long)len) : SvPVX(sv))
#  define Sv_Grow(sv,len) sv_grow(sv,(unsigned long)(len))
#endif /* DOSISH */
