#include "EXTERN.h"
#define PERL_IN_XSUTILS_C
#include "perl.h"

/*
 * Contributed by Spider Boardman (spider.boardman@orb.nashua.nh.us).
 */

/* package attributes; */
void XS_attributes__warn_reserved(pTHXo_ CV *cv);
void XS_attributes_reftype(pTHXo_ CV *cv);
void XS_attributes__modify_attrs(pTHXo_ CV *cv);
void XS_attributes__guess_stash(pTHXo_ CV *cv);
void XS_attributes__fetch_attrs(pTHXo_ CV *cv);
void XS_attributes_bootstrap(pTHXo_ CV *cv);


/*
 * Note that only ${pkg}::bootstrap definitions should go here.
 * This helps keep down the start-up time, which is especially
 * relevant for users who don't invoke any features which are
 * (partially) implemented here.
 *
 * The various bootstrap definitions can take care of doing
 * package-specific newXS() calls.  Since the layout of the
 * bundled *.pm files is in a version-specific directory,
 * version checks in these bootstrap calls are optional.
 */

void
Perl_boot_core_xsutils(pTHX)
{
    char *file = __FILE__;

    newXS("attributes::bootstrap",	XS_attributes_bootstrap,	file);
}

#include "XSUB.h"

static int
modify_SV_attributes(pTHXo_ SV *sv, SV **retlist, SV **attrlist, int numattrs)
{
    SV *attr;
    char *name;
    STRLEN len;
    bool negated;
    int nret;

    for (nret = 0 ; numattrs && (attr = *attrlist++); numattrs--) {
	name = SvPV(attr, len);
	if ((negated = (*name == '-'))) {
	    name++;
	    len--;
	}
	switch (SvTYPE(sv)) {
	case SVt_PVCV:
	    switch ((int)len) {
	    case 6:
		switch (*name) {
		case 'l':
#ifdef CVf_LVALUE
		    if (strEQ(name, "lvalue")) {
			if (negated)
			    CvFLAGS((CV*)sv) &= ~CVf_LVALUE;
			else
			    CvFLAGS((CV*)sv) |= CVf_LVALUE;
			continue;
		    }
#endif /* defined CVf_LVALUE */
		    if (strEQ(name, "locked")) {
			if (negated)
			    CvFLAGS((CV*)sv) &= ~CVf_LOCKED;
			else
			    CvFLAGS((CV*)sv) |= CVf_LOCKED;
			continue;
		    }
		    break;
		case 'm':
		    if (strEQ(name, "method")) {
			if (negated)
			    CvFLAGS((CV*)sv) &= ~CVf_METHOD;
			else
			    CvFLAGS((CV*)sv) |= CVf_METHOD;
			continue;
		    }
		    break;
		}
		break;
	    }
	    break;
	default:
	    /* nothing, yet */
	    break;
	}
	/* anything recognized had a 'continue' above */
	*retlist++ = attr;
	nret++;
    }

    return nret;
}



/* package attributes; */

XS(XS_attributes_bootstrap)
{
    dXSARGS;
    char *file = __FILE__;

    newXSproto("attributes::_warn_reserved", XS_attributes__warn_reserved, file, "");
    newXS("attributes::_modify_attrs",	XS_attributes__modify_attrs,	file);
    newXSproto("attributes::_guess_stash", XS_attributes__guess_stash, file, "$");
    newXSproto("attributes::_fetch_attrs", XS_attributes__fetch_attrs, file, "$");
    newXSproto("attributes::reftype",	XS_attributes_reftype,	file, "$");

    XSRETURN(0);
}

XS(XS_attributes__modify_attrs)
{
    dXSARGS;
    SV *rv, *sv;

    if (items < 1) {
usage:
	Perl_croak(aTHX_
		   "Usage: attributes::_modify_attrs $reference, @attributes");
    }

    rv = ST(0);
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);
    if (items > 1)
	XSRETURN(modify_SV_attributes(aTHXo_ sv, &ST(0), &ST(1), items-1));

    XSRETURN(0);
}

XS(XS_attributes__fetch_attrs)
{
    dXSARGS;
    SV *rv, *sv;
    cv_flags_t cvflags;

    if (items != 1) {
usage:
	Perl_croak(aTHX_
		   "Usage: attributes::_fetch_attrs $reference");
    }

    rv = ST(0);
    SP -= items;
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);

    switch (SvTYPE(sv)) {
    case SVt_PVCV:
	cvflags = CvFLAGS((CV*)sv);
	if (cvflags & CVf_LOCKED)
	    XPUSHs(sv_2mortal(newSVpvn("locked", 6)));
#ifdef CVf_LVALUE
	if (cvflags & CVf_LVALUE)
	    XPUSHs(sv_2mortal(newSVpvn("lvalue", 6)));
#endif
	if (cvflags & CVf_METHOD)
	    XPUSHs(sv_2mortal(newSVpvn("method", 6)));
	break;
    default:
	break;
    }

    PUTBACK;
}

XS(XS_attributes__guess_stash)
{
    dXSARGS;
    SV *rv, *sv;
#ifdef dXSTARGET
    dXSTARGET;
#else
    SV * TARG = sv_newmortal();
#endif

    if (items != 1) {
usage:
	Perl_croak(aTHX_
		   "Usage: attributes::_guess_stash $reference");
    }

    rv = ST(0);
    ST(0) = TARG;
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);

    if (SvOBJECT(sv))
	sv_setpv(TARG, HvNAME(SvSTASH(sv)));
#if 0	/* this was probably a bad idea */
    else if (SvPADMY(sv))
	sv_setsv(TARG, &PL_sv_no);	/* unblessed lexical */
#endif
    else {
	HV *stash = Nullhv;
	switch (SvTYPE(sv)) {
	case SVt_PVCV:
	    if (CvGV(sv) && isGV(CvGV(sv)) && GvSTASH(CvGV(sv)) &&
			    HvNAME(GvSTASH(CvGV(sv))))
		stash = GvSTASH(CvGV(sv));
	    else if (/* !CvANON(sv) && */ CvSTASH(sv) && HvNAME(CvSTASH(sv)))
		stash = CvSTASH(sv);
	    break;
	case SVt_PVMG:
	    if (!(SvFAKE(sv) && SvTIED_mg(sv, '*')))
		break;
	    /*FALLTHROUGH*/
	case SVt_PVGV:
	    if (GvGP(sv) && GvESTASH((GV*)sv) && HvNAME(GvESTASH((GV*)sv)))
		stash = GvESTASH((GV*)sv);
	    break;
	default:
	    break;
	}
	if (stash)
	    sv_setpv(TARG, HvNAME(stash));
    }

#ifdef dXSTARGET
    SvSETMAGIC(TARG);
#endif
    XSRETURN(1);
}

XS(XS_attributes_reftype)
{
    dXSARGS;
    SV *rv, *sv;
#ifdef dXSTARGET
    dXSTARGET;
#else
    SV * TARG = sv_newmortal();
#endif

    if (items != 1) {
usage:
	Perl_croak(aTHX_
		   "Usage: attributes::reftype $reference");
    }

    rv = ST(0);
    ST(0) = TARG;
    if (SvGMAGICAL(rv))
	mg_get(rv);
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);
    sv_setpv(TARG, sv_reftype(sv, 0));
#ifdef dXSTARGET
    SvSETMAGIC(TARG);
#endif

    XSRETURN(1);
}

XS(XS_attributes__warn_reserved)
{
    dXSARGS;
#ifdef dXSTARGET
    dXSTARGET;
#else
    SV * TARG = sv_newmortal();
#endif

    if (items != 0) {
	Perl_croak(aTHX_
		   "Usage: attributes::_warn_reserved ()");
    }

    EXTEND(SP,1);
    ST(0) = TARG;
    sv_setiv(TARG, ckWARN(WARN_RESERVED) != 0);
#ifdef dXSTARGET
    SvSETMAGIC(TARG);
#endif

    XSRETURN(1);
}

