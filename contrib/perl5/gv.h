/*    gv.h
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

struct gp {
    SV *	gp_sv;		/* scalar value */
    U32		gp_refcnt;	/* how many globs point to this? */
    struct io *	gp_io;		/* filehandle value */
    CV *	gp_form;	/* format value */
    AV *	gp_av;		/* array value */
    HV *	gp_hv;		/* hash value */
    GV *	gp_egv;		/* effective gv, if *glob */
    CV *	gp_cv;		/* subroutine value */
    U32		gp_cvgen;	/* generational validity of cached gv_cv */
    U32		gp_flags;	/* XXX unused */
    line_t	gp_line;	/* line first declared at (for -w) */
    char *	gp_file;	/* file first declared in (for -w) */
};

#if defined(CRIPPLED_CC) && (defined(iAPX286) || defined(M_I286) || defined(I80286))
#define MICROPORT
#endif

#define GvXPVGV(gv)	((XPVGV*)SvANY(gv))

#define GvGP(gv)	(GvXPVGV(gv)->xgv_gp)
#define GvNAME(gv)	(GvXPVGV(gv)->xgv_name)
#define GvNAMELEN(gv)	(GvXPVGV(gv)->xgv_namelen)
#define GvSTASH(gv)	(GvXPVGV(gv)->xgv_stash)
#define GvFLAGS(gv)	(GvXPVGV(gv)->xgv_flags)

/*
=for apidoc Am|SV*|GvSV|GV* gv

Return the SV from the GV.

=cut
*/

#define GvSV(gv)	(GvGP(gv)->gp_sv)
#define GvREFCNT(gv)	(GvGP(gv)->gp_refcnt)
#define GvIO(gv)	((gv) && SvTYPE((SV*)gv) == SVt_PVGV ? GvIOp(gv) : 0)
#define GvIOp(gv)	(GvGP(gv)->gp_io)
#define GvIOn(gv)	(GvIO(gv) ? GvIOp(gv) : GvIOp(gv_IOadd(gv)))

#define GvFORM(gv)	(GvGP(gv)->gp_form)
#define GvAV(gv)	(GvGP(gv)->gp_av)

/* This macro is deprecated.  Do not use! */
#define GvREFCNT_inc(gv) ((GV*)SvREFCNT_inc(gv))	/* DO NOT USE */

#ifdef	MICROPORT	/* Microport 2.4 hack */
AV *GvAVn();
#else
#define GvAVn(gv)	(GvGP(gv)->gp_av ? \
			 GvGP(gv)->gp_av : \
			 GvGP(gv_AVadd(gv))->gp_av)
#endif
#define GvHV(gv)	((GvGP(gv))->gp_hv)

#ifdef	MICROPORT	/* Microport 2.4 hack */
HV *GvHVn();
#else
#define GvHVn(gv)	(GvGP(gv)->gp_hv ? \
			 GvGP(gv)->gp_hv : \
			 GvGP(gv_HVadd(gv))->gp_hv)
#endif			/* Microport 2.4 hack */

#define GvCV(gv)	(GvGP(gv)->gp_cv)
#define GvCVGEN(gv)	(GvGP(gv)->gp_cvgen)
#define GvCVu(gv)	(GvGP(gv)->gp_cvgen ? Nullcv : GvGP(gv)->gp_cv)

#define GvGPFLAGS(gv)	(GvGP(gv)->gp_flags)

#define GvLINE(gv)	(GvGP(gv)->gp_line)
#define GvFILE(gv)	(GvGP(gv)->gp_file)
#define GvFILEGV(gv)	(gv_fetchfile(GvFILE(gv)))

#define GvEGV(gv)	(GvGP(gv)->gp_egv)
#define GvENAME(gv)	GvNAME(GvEGV(gv) ? GvEGV(gv) : gv)
#define GvESTASH(gv)	GvSTASH(GvEGV(gv) ? GvEGV(gv) : gv)

#define GVf_INTRO	0x01
#define GVf_MULTI	0x02
#define GVf_ASSUMECV	0x04
#define GVf_IN_PAD	0x08
#define GVf_IMPORTED	0xF0
#define GVf_IMPORTED_SV	  0x10
#define GVf_IMPORTED_AV	  0x20
#define GVf_IMPORTED_HV	  0x40
#define GVf_IMPORTED_CV	  0x80

#define GvINTRO(gv)		(GvFLAGS(gv) & GVf_INTRO)
#define GvINTRO_on(gv)		(GvFLAGS(gv) |= GVf_INTRO)
#define GvINTRO_off(gv)		(GvFLAGS(gv) &= ~GVf_INTRO)

#define GvMULTI(gv)		(GvFLAGS(gv) & GVf_MULTI)
#define GvMULTI_on(gv)		(GvFLAGS(gv) |= GVf_MULTI)
#define GvMULTI_off(gv)		(GvFLAGS(gv) &= ~GVf_MULTI)

#define GvASSUMECV(gv)		(GvFLAGS(gv) & GVf_ASSUMECV)
#define GvASSUMECV_on(gv)	(GvFLAGS(gv) |= GVf_ASSUMECV)
#define GvASSUMECV_off(gv)	(GvFLAGS(gv) &= ~GVf_ASSUMECV)

#define GvIMPORTED(gv)		(GvFLAGS(gv) & GVf_IMPORTED)
#define GvIMPORTED_on(gv)	(GvFLAGS(gv) |= GVf_IMPORTED)
#define GvIMPORTED_off(gv)	(GvFLAGS(gv) &= ~GVf_IMPORTED)

#define GvIMPORTED_SV(gv)	(GvFLAGS(gv) & GVf_IMPORTED_SV)
#define GvIMPORTED_SV_on(gv)	(GvFLAGS(gv) |= GVf_IMPORTED_SV)
#define GvIMPORTED_SV_off(gv)	(GvFLAGS(gv) &= ~GVf_IMPORTED_SV)

#define GvIMPORTED_AV(gv)	(GvFLAGS(gv) & GVf_IMPORTED_AV)
#define GvIMPORTED_AV_on(gv)	(GvFLAGS(gv) |= GVf_IMPORTED_AV)
#define GvIMPORTED_AV_off(gv)	(GvFLAGS(gv) &= ~GVf_IMPORTED_AV)

#define GvIMPORTED_HV(gv)	(GvFLAGS(gv) & GVf_IMPORTED_HV)
#define GvIMPORTED_HV_on(gv)	(GvFLAGS(gv) |= GVf_IMPORTED_HV)
#define GvIMPORTED_HV_off(gv)	(GvFLAGS(gv) &= ~GVf_IMPORTED_HV)

#define GvIMPORTED_CV(gv)	(GvFLAGS(gv) & GVf_IMPORTED_CV)
#define GvIMPORTED_CV_on(gv)	(GvFLAGS(gv) |= GVf_IMPORTED_CV)
#define GvIMPORTED_CV_off(gv)	(GvFLAGS(gv) &= ~GVf_IMPORTED_CV)

#define GvIN_PAD(gv)		(GvFLAGS(gv) & GVf_IN_PAD)
#define GvIN_PAD_on(gv)		(GvFLAGS(gv) |= GVf_IN_PAD)
#define GvIN_PAD_off(gv)	(GvFLAGS(gv) &= ~GVf_IN_PAD)

#define Nullgv Null(GV*)

#define DM_UID   0x003
#define DM_RUID   0x001
#define DM_EUID   0x002
#define DM_GID   0x030
#define DM_RGID   0x010
#define DM_EGID   0x020
#define DM_DELAY 0x100

/*
 * symbol creation flags, for use in gv_fetchpv() and get_*v()
 */
#define GV_ADD		0x01	/* add, if symbol not already there */
#define GV_ADDMULTI	0x02	/* add, pretending it has been added already */
#define GV_ADDWARN	0x04	/* add, but warn if symbol wasn't already there */
#define GV_ADDINEVAL	0x08	/* add, as though we're doing so within an eval */
#define GV_NOINIT	0x10	/* add, but don't init symbol, if type != PVGV */
