/*    cv.h
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* This structure much match the beginning of XPVFM */

struct xpvcv {
    char *	xpv_pv;		/* pointer to malloced string */
    STRLEN	xpv_cur;	/* length of xp_pv as a C string */
    STRLEN	xpv_len;	/* allocated size */
    IV		xof_off;	/* integer value */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* magic for scalar array */
    HV*		xmg_stash;	/* class package */

    HV *	xcv_stash;
    OP *	xcv_start;
    OP *	xcv_root;
    void      (*xcv_xsub) _((CV* _CPERLproto));
    ANY		xcv_xsubany;
    GV *	xcv_gv;
    GV *	xcv_filegv;
    long	xcv_depth;		/* >= 2 indicates recursive call */
    AV *	xcv_padlist;
    CV *	xcv_outside;
#ifdef USE_THREADS
    perl_mutex *xcv_mutexp;
    struct perl_thread *xcv_owner;	/* current owner thread */
#endif /* USE_THREADS */
    cv_flags_t	xcv_flags;
};

#define Nullcv Null(CV*)

#define CvSTASH(sv)	((XPVCV*)SvANY(sv))->xcv_stash
#define CvSTART(sv)	((XPVCV*)SvANY(sv))->xcv_start
#define CvROOT(sv)	((XPVCV*)SvANY(sv))->xcv_root
#define CvXSUB(sv)	((XPVCV*)SvANY(sv))->xcv_xsub
#define CvXSUBANY(sv)	((XPVCV*)SvANY(sv))->xcv_xsubany
#define CvGV(sv)	((XPVCV*)SvANY(sv))->xcv_gv
#define CvFILEGV(sv)	((XPVCV*)SvANY(sv))->xcv_filegv
#define CvDEPTH(sv)	((XPVCV*)SvANY(sv))->xcv_depth
#define CvPADLIST(sv)	((XPVCV*)SvANY(sv))->xcv_padlist
#define CvOUTSIDE(sv)	((XPVCV*)SvANY(sv))->xcv_outside
#ifdef USE_THREADS
#define CvMUTEXP(sv)	((XPVCV*)SvANY(sv))->xcv_mutexp
#define CvOWNER(sv)	((XPVCV*)SvANY(sv))->xcv_owner
#endif /* USE_THREADS */
#define CvFLAGS(sv)	((XPVCV*)SvANY(sv))->xcv_flags

#define CVf_CLONE	0x0001	/* anon CV uses external lexicals */
#define CVf_CLONED	0x0002	/* a clone of one of those */
#define CVf_ANON	0x0004	/* CvGV() can't be trusted */
#define CVf_OLDSTYLE	0x0008
#define CVf_UNIQUE	0x0010	/* can't be cloned */
#define CVf_NODEBUG	0x0020	/* no DB::sub indirection for this CV
				   (esp. useful for special XSUBs) */
#define CVf_METHOD	0x0040	/* CV is explicitly marked as a method */
#define CVf_LOCKED	0x0080	/* CV locks itself or first arg on entry */

#define CvCLONE(cv)		(CvFLAGS(cv) & CVf_CLONE)
#define CvCLONE_on(cv)		(CvFLAGS(cv) |= CVf_CLONE)
#define CvCLONE_off(cv)		(CvFLAGS(cv) &= ~CVf_CLONE)

#define CvCLONED(cv)		(CvFLAGS(cv) & CVf_CLONED)
#define CvCLONED_on(cv)		(CvFLAGS(cv) |= CVf_CLONED)
#define CvCLONED_off(cv)	(CvFLAGS(cv) &= ~CVf_CLONED)

#define CvANON(cv)		(CvFLAGS(cv) & CVf_ANON)
#define CvANON_on(cv)		(CvFLAGS(cv) |= CVf_ANON)
#define CvANON_off(cv)		(CvFLAGS(cv) &= ~CVf_ANON)

#define CvOLDSTYLE(cv)		(CvFLAGS(cv) & CVf_OLDSTYLE)
#define CvOLDSTYLE_on(cv)	(CvFLAGS(cv) |= CVf_OLDSTYLE)
#define CvOLDSTYLE_off(cv)	(CvFLAGS(cv) &= ~CVf_OLDSTYLE)

#define CvUNIQUE(cv)		(CvFLAGS(cv) & CVf_UNIQUE)
#define CvUNIQUE_on(cv)		(CvFLAGS(cv) |= CVf_UNIQUE)
#define CvUNIQUE_off(cv)	(CvFLAGS(cv) &= ~CVf_UNIQUE)

#define CvNODEBUG(cv)		(CvFLAGS(cv) & CVf_NODEBUG)
#define CvNODEBUG_on(cv)	(CvFLAGS(cv) |= CVf_NODEBUG)
#define CvNODEBUG_off(cv)	(CvFLAGS(cv) &= ~CVf_NODEBUG)

#define CvMETHOD(cv)		(CvFLAGS(cv) & CVf_METHOD)
#define CvMETHOD_on(cv)		(CvFLAGS(cv) |= CVf_METHOD)
#define CvMETHOD_off(cv)	(CvFLAGS(cv) &= ~CVf_METHOD)

#define CvLOCKED(cv)		(CvFLAGS(cv) & CVf_LOCKED)
#define CvLOCKED_on(cv)		(CvFLAGS(cv) |= CVf_LOCKED)
#define CvLOCKED_off(cv)	(CvFLAGS(cv) &= ~CVf_LOCKED)

#define CvEVAL(cv)		(CvUNIQUE(cv) && !SvFAKE(cv))
#define CvEVAL_on(cv)		(CvUNIQUE_on(cv),SvFAKE_off(cv))
#define CvEVAL_off(cv)		CvUNIQUE_off(cv)

/* BEGIN|INIT|END */
#define CvSPECIAL(cv)		(CvUNIQUE(cv) && SvFAKE(cv))
#define CvSPECIAL_on(cv)	(CvUNIQUE_on(cv),SvFAKE_on(cv))
#define CvSPECIAL_off(cv)	(CvUNIQUE_off(cv),SvFAKE_off(cv))
