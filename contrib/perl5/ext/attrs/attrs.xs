#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static cv_flags_t
get_flag(char *attr)
{
    if (strnEQ(attr, "method", 6))
	return CVf_METHOD;
    else if (strnEQ(attr, "locked", 6))
	return CVf_LOCKED;
    else
	return 0;
}

MODULE = attrs		PACKAGE = attrs

void
import(Class, ...)
char *	Class
    ALIAS:
	unimport = 1
    PREINIT:
	int i;
	CV *cv;
    PPCODE:
	if (!PL_compcv || !(cv = CvOUTSIDE(PL_compcv)))
	    croak("can't set attributes outside a subroutine scope");
	for (i = 1; i < items; i++) {
	    STRLEN n_a;
	    char *attr = SvPV(ST(i), n_a);
	    cv_flags_t flag = get_flag(attr);
	    if (!flag)
		croak("invalid attribute name %s", attr);
	    if (ix)
    		CvFLAGS(cv) &= ~flag;
	    else
		CvFLAGS(cv) |= flag;
	}

void
get(sub)
SV *	sub
    PPCODE:
	if (SvROK(sub)) {
	    sub = SvRV(sub);
	    if (SvTYPE(sub) != SVt_PVCV)
		sub = Nullsv;
	}
	else {
	    STRLEN n_a;
	    char *name = SvPV(sub, n_a);
	    sub = (SV*)perl_get_cv(name, FALSE);
	}
	if (!sub)
	    croak("invalid subroutine reference or name");
	if (CvFLAGS(sub) & CVf_METHOD)
	    XPUSHs(sv_2mortal(newSVpv("method", 0)));
	if (CvFLAGS(sub) & CVf_LOCKED)
	    XPUSHs(sv_2mortal(newSVpv("locked", 0)));

