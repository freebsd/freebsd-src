#include "EXTERN.h"
#define PERL_IN_UNIVERSAL_C
#include "perl.h"

/*
 * Contributed by Graham Barr  <Graham.Barr@tiuk.ti.com>
 * The main guts of traverse_isa was actually copied from gv_fetchmeth
 */

STATIC SV *
S_isa_lookup(pTHX_ HV *stash, const char *name, int len, int level)
{
    AV* av;
    GV* gv;
    GV** gvp;
    HV* hv = Nullhv;
    SV* subgen = Nullsv;

    if (!stash)
	return &PL_sv_undef;

    if (strEQ(HvNAME(stash), name))
	return &PL_sv_yes;

    if (level > 100)
	Perl_croak(aTHX_ "Recursive inheritance detected in package '%s'",
		   HvNAME(stash));

    gvp = (GV**)hv_fetch(stash, "::ISA::CACHE::", 14, FALSE);

    if (gvp && (gv = *gvp) != (GV*)&PL_sv_undef && (subgen = GvSV(gv))
	&& (hv = GvHV(gv)))
    {
	if (SvIV(subgen) == PL_sub_generation) {
	    SV* sv;
	    SV** svp = (SV**)hv_fetch(hv, name, len, FALSE);
	    if (svp && (sv = *svp) != (SV*)&PL_sv_undef) {
	        DEBUG_o( Perl_deb(aTHX_ "Using cached ISA %s for package %s\n",
				  name, HvNAME(stash)) );
		return sv;
	    }
	}
	else {
	    DEBUG_o( Perl_deb(aTHX_ "ISA Cache in package %s is stale\n",
			      HvNAME(stash)) );
	    hv_clear(hv);
	    sv_setiv(subgen, PL_sub_generation);
	}
    }

    gvp = (GV**)hv_fetch(stash,"ISA",3,FALSE);

    if (gvp && (gv = *gvp) != (GV*)&PL_sv_undef && (av = GvAV(gv))) {
	if (!hv || !subgen) {
	    gvp = (GV**)hv_fetch(stash, "::ISA::CACHE::", 14, TRUE);

	    gv = *gvp;

	    if (SvTYPE(gv) != SVt_PVGV)
		gv_init(gv, stash, "::ISA::CACHE::", 14, TRUE);

	    if (!hv)
		hv = GvHVn(gv);
	    if (!subgen) {
		subgen = newSViv(PL_sub_generation);
		GvSV(gv) = subgen;
	    }
	}
	if (hv) {
	    SV** svp = AvARRAY(av);
	    /* NOTE: No support for tied ISA */
	    I32 items = AvFILLp(av) + 1;
	    while (items--) {
		SV* sv = *svp++;
		HV* basestash = gv_stashsv(sv, FALSE);
		if (!basestash) {
		    if (ckWARN(WARN_MISC))
			Perl_warner(aTHX_ WARN_SYNTAX,
		             "Can't locate package %s for @%s::ISA",
			    SvPVX(sv), HvNAME(stash));
		    continue;
		}
		if (&PL_sv_yes == isa_lookup(basestash, name, len, level + 1)) {
		    (void)hv_store(hv,name,len,&PL_sv_yes,0);
		    return &PL_sv_yes;
		}
	    }
	    (void)hv_store(hv,name,len,&PL_sv_no,0);
	}
    }

    return boolSV(strEQ(name, "UNIVERSAL"));
}

/*
=for apidoc sv_derived_from

Returns a boolean indicating whether the SV is derived from the specified
class.  This is the function that implements C<UNIVERSAL::isa>.  It works
for class names as well as for objects.

=cut
*/

bool
Perl_sv_derived_from(pTHX_ SV *sv, const char *name)
{
    char *type;
    HV *stash;

    stash = Nullhv;
    type = Nullch;

    if (SvGMAGICAL(sv))
        mg_get(sv) ;

    if (SvROK(sv)) {
        sv = SvRV(sv);
        type = sv_reftype(sv,0);
        if (SvOBJECT(sv))
            stash = SvSTASH(sv);
    }
    else {
        stash = gv_stashsv(sv, FALSE);
    }

    return (type && strEQ(type,name)) ||
            (stash && isa_lookup(stash, name, strlen(name), 0) == &PL_sv_yes)
        ? TRUE
        : FALSE ;
}

void XS_UNIVERSAL_isa(pTHXo_ CV *cv);
void XS_UNIVERSAL_can(pTHXo_ CV *cv);
void XS_UNIVERSAL_VERSION(pTHXo_ CV *cv);

void
Perl_boot_core_UNIVERSAL(pTHX)
{
    char *file = __FILE__;

    newXS("UNIVERSAL::isa",             XS_UNIVERSAL_isa,         file);
    newXS("UNIVERSAL::can",             XS_UNIVERSAL_can,         file);
    newXS("UNIVERSAL::VERSION", 	XS_UNIVERSAL_VERSION, 	  file);
}

#include "XSUB.h"

XS(XS_UNIVERSAL_isa)
{
    dXSARGS;
    SV *sv;
    char *name;
    STRLEN n_a;

    if (items != 2)
	Perl_croak(aTHX_ "Usage: UNIVERSAL::isa(reference, kind)");

    sv = ST(0);

    if (SvGMAGICAL(sv))
	mg_get(sv);

    if (!SvOK(sv) || !(SvROK(sv) || (SvPOK(sv) && SvCUR(sv))))
	XSRETURN_UNDEF;

    name = (char *)SvPV(ST(1),n_a);

    ST(0) = boolSV(sv_derived_from(sv, name));
    XSRETURN(1);
}

XS(XS_UNIVERSAL_can)
{
    dXSARGS;
    SV   *sv;
    char *name;
    SV   *rv;
    HV   *pkg = NULL;
    STRLEN n_a;

    if (items != 2)
	Perl_croak(aTHX_ "Usage: UNIVERSAL::can(object-ref, method)");

    sv = ST(0);

    if (SvGMAGICAL(sv))
	mg_get(sv);

    if (!SvOK(sv) || !(SvROK(sv) || (SvPOK(sv) && SvCUR(sv))))
	XSRETURN_UNDEF;

    name = (char *)SvPV(ST(1),n_a);
    rv = &PL_sv_undef;

    if (SvROK(sv)) {
        sv = (SV*)SvRV(sv);
        if (SvOBJECT(sv))
            pkg = SvSTASH(sv);
    }
    else {
        pkg = gv_stashsv(sv, FALSE);
    }

    if (pkg) {
        GV *gv = gv_fetchmethod_autoload(pkg, name, FALSE);
        if (gv && isGV(gv))
	    rv = sv_2mortal(newRV((SV*)GvCV(gv)));
    }

    ST(0) = rv;
    XSRETURN(1);
}

XS(XS_UNIVERSAL_VERSION)
{
    dXSARGS;
    HV *pkg;
    GV **gvp;
    GV *gv;
    SV *sv;
    char *undef;

    if (SvROK(ST(0))) {
        sv = (SV*)SvRV(ST(0));
        if (!SvOBJECT(sv))
            Perl_croak(aTHX_ "Cannot find version of an unblessed reference");
        pkg = SvSTASH(sv);
    }
    else {
        pkg = gv_stashsv(ST(0), FALSE);
    }

    gvp = pkg ? (GV**)hv_fetch(pkg,"VERSION",7,FALSE) : Null(GV**);

    if (gvp && isGV(gv = *gvp) && SvOK(sv = GvSV(gv))) {
        SV *nsv = sv_newmortal();
        sv_setsv(nsv, sv);
        sv = nsv;
        undef = Nullch;
    }
    else {
        sv = (SV*)&PL_sv_undef;
        undef = "(undef)";
    }

    if (items > 1) {
	STRLEN len;
	SV *req = ST(1);

	if (undef)
	    Perl_croak(aTHX_ "%s does not define $%s::VERSION--version check failed",
		       HvNAME(pkg), HvNAME(pkg));

	if (!SvNIOK(sv) && SvPOK(sv)) {
	    char *str = SvPVx(sv,len);
	    while (len) {
		--len;
		/* XXX could DWIM "1.2.3" here */
		if (!isDIGIT(str[len]) && str[len] != '.' && str[len] != '_')
		    break;
	    }
	    if (len) {
		if (SvNOK(req) && SvPOK(req)) {
		    /* they said C<use Foo v1.2.3> and $Foo::VERSION
		     * doesn't look like a float: do string compare */
		    if (sv_cmp(req,sv) == 1) {
			Perl_croak(aTHX_ "%s v%"VDf" required--"
				   "this is only v%"VDf,
				   HvNAME(pkg), req, sv);
		    }
		    goto finish;
		}
		/* they said C<use Foo 1.002_003> and $Foo::VERSION
		 * doesn't look like a float: force numeric compare */
		(void)SvUPGRADE(sv, SVt_PVNV);
		SvNVX(sv) = str_to_version(sv);
		SvPOK_off(sv);
		SvNOK_on(sv);
	    }
	}
	/* if we get here, we're looking for a numeric comparison,
	 * so force the required version into a float, even if they
	 * said C<use Foo v1.2.3> */
	if (SvNOK(req) && SvPOK(req)) {
	    NV n = SvNV(req);
	    req = sv_newmortal();
	    sv_setnv(req, n);
	}

	if (SvNV(req) > SvNV(sv))
	    Perl_croak(aTHX_ "%s version %s required--this is only version %s",
		  HvNAME(pkg), SvPV(req,len), SvPV(sv,len));
    }

finish:
    ST(0) = sv;

    XSRETURN(1);
}

