#include "EXTERN.h"
#include "perl.h"

/*
 * Contributed by Graham Barr  <Graham.Barr@tiuk.ti.com>
 * The main guts of traverse_isa was actually copied from gv_fetchmeth
 */

STATIC SV *
isa_lookup(HV *stash, char *name, int len, int level)
{
    AV* av;
    GV* gv;
    GV** gvp;
    HV* hv = Nullhv;

    if (!stash)
	return &PL_sv_undef;

    if(strEQ(HvNAME(stash), name))
	return &PL_sv_yes;

    if (level > 100)
	croak("Recursive inheritance detected in package '%s'", HvNAME(stash));

    gvp = (GV**)hv_fetch(stash, "::ISA::CACHE::", 14, FALSE);

    if (gvp && (gv = *gvp) != (GV*)&PL_sv_undef && (hv = GvHV(gv))) {
	SV* sv;
	SV** svp = (SV**)hv_fetch(hv, name, len, FALSE);
	if (svp && (sv = *svp) != (SV*)&PL_sv_undef)
	    return sv;
    }

    gvp = (GV**)hv_fetch(stash,"ISA",3,FALSE);
    
    if (gvp && (gv = *gvp) != (GV*)&PL_sv_undef && (av = GvAV(gv))) {
	if(!hv) {
	    gvp = (GV**)hv_fetch(stash, "::ISA::CACHE::", 14, TRUE);

	    gv = *gvp;

	    if (SvTYPE(gv) != SVt_PVGV)
		gv_init(gv, stash, "::ISA::CACHE::", 14, TRUE);

	    hv = GvHVn(gv);
	}
	if(hv) {
	    SV** svp = AvARRAY(av);
	    /* NOTE: No support for tied ISA */
	    I32 items = AvFILLp(av) + 1;
	    while (items--) {
		SV* sv = *svp++;
		HV* basestash = gv_stashsv(sv, FALSE);
		if (!basestash) {
		    if (PL_dowarn)
			warn("Can't locate package %s for @%s::ISA",
			    SvPVX(sv), HvNAME(stash));
		    continue;
		}
		if(&PL_sv_yes == isa_lookup(basestash, name, len, level + 1)) {
		    (void)hv_store(hv,name,len,&PL_sv_yes,0);
		    return &PL_sv_yes;
		}
	    }
	    (void)hv_store(hv,name,len,&PL_sv_no,0);
	}
    }

    return boolSV(strEQ(name, "UNIVERSAL"));
}

bool
sv_derived_from(SV *sv, char *name)
{
    SV *rv;
    char *type;
    HV *stash;
  
    stash = Nullhv;
    type = Nullch;
 
    if (SvGMAGICAL(sv))
        mg_get(sv) ;

    if (SvROK(sv)) {
        sv = SvRV(sv);
        type = sv_reftype(sv,0);
        if(SvOBJECT(sv))
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

#ifdef PERL_OBJECT
#define NO_XSLOCKS
#endif  /* PERL_OBJECT */

#include "XSUB.h"

XS(XS_UNIVERSAL_isa)
{
    dXSARGS;
    SV *sv;
    char *name;
    STRLEN n_a;

    if (items != 2)
	croak("Usage: UNIVERSAL::isa(reference, kind)");

    sv = ST(0);
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
	croak("Usage: UNIVERSAL::can(object-ref, method)");

    sv = ST(0);
    name = (char *)SvPV(ST(1),n_a);
    rv = &PL_sv_undef;

    if(SvROK(sv)) {
        sv = (SV*)SvRV(sv);
        if(SvOBJECT(sv))
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
    double req;

    if(SvROK(ST(0))) {
        sv = (SV*)SvRV(ST(0));
        if(!SvOBJECT(sv))
            croak("Cannot find version of an unblessed reference");
        pkg = SvSTASH(sv);
    }
    else {
        pkg = gv_stashsv(ST(0), FALSE);
    }

    gvp = pkg ? (GV**)hv_fetch(pkg,"VERSION",7,FALSE) : Null(GV**);

    if (gvp && (gv = *gvp) != (GV*)&PL_sv_undef && (sv = GvSV(gv))) {
        SV *nsv = sv_newmortal();
        sv_setsv(nsv, sv);
        sv = nsv;
        undef = Nullch;
    }
    else {
        sv = (SV*)&PL_sv_undef;
        undef = "(undef)";
    }

    if (items > 1 && (undef || (req = SvNV(ST(1)), req > SvNV(sv)))) {
	STRLEN n_a;
	croak("%s version %s required--this is only version %s",
	      HvNAME(pkg), SvPV(ST(1),n_a), undef ? undef : SvPV(sv,n_a));
    }

    ST(0) = sv;

    XSRETURN(1);
}

#ifdef PERL_OBJECT
#undef  boot_core_UNIVERSAL
#define boot_core_UNIVERSAL CPerlObj::Perl_boot_core_UNIVERSAL
#define pPerl this
#endif

void
boot_core_UNIVERSAL(void)
{
    char *file = __FILE__;

    newXS("UNIVERSAL::isa",             XS_UNIVERSAL_isa,         file);
    newXS("UNIVERSAL::can",             XS_UNIVERSAL_can,         file);
    newXS("UNIVERSAL::VERSION", 	XS_UNIVERSAL_VERSION, 	  file);
}
