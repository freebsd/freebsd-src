/*
 * "...we will have peace, when you and all your works have perished--and
 * the works of your dark master to whom you would deliver us.  You are a
 * liar, Saruman, and a corrupter of men's hearts."  --Theoden
 */

#include "EXTERN.h"
#include "perl.h"

void
taint_proper(const char *f, char *s)
{
    dTHR;	/* just for taint */
    char *ug;

    DEBUG_u(PerlIO_printf(Perl_debug_log,
            "%s %d %d %d\n", s, PL_tainted, PL_uid, PL_euid));

    if (PL_tainted) {
	if (!f)
	    f = no_security;
	if (PL_euid != PL_uid)
	    ug = " while running setuid";
	else if (PL_egid != PL_gid)
	    ug = " while running setgid";
	else
	    ug = " while running with -T switch";
	if (!PL_unsafe)
	    croak(f, s, ug);
	else if (PL_dowarn)
	    warn(f, s, ug);
    }
}

void
taint_env(void)
{
    SV** svp;
    MAGIC* mg;
    char** e;
    static char* misc_env[] = {
	"IFS",		/* most shells' inter-field separators */
	"CDPATH",	/* ksh dain bramage #1 */
	"ENV",		/* ksh dain bramage #2 */
	"BASH_ENV",	/* bash dain bramage -- I guess it's contagious */
	NULL
    };

    if(!PL_envgv)
	return;

#ifdef VMS
    {
    int i = 0;
    char name[10 + TYPE_DIGITS(int)] = "DCL$PATH";

    while (1) {
	if (i)
	    (void)sprintf(name,"DCL$PATH;%d", i);
	svp = hv_fetch(GvHVn(PL_envgv), name, strlen(name), FALSE);
	if (!svp || *svp == &PL_sv_undef)
	    break;
	if (SvTAINTED(*svp)) {
	    dTHR;
	    TAINT;
	    taint_proper("Insecure %s%s", "$ENV{DCL$PATH}");
	}
	if ((mg = mg_find(*svp, 'e')) && MgTAINTEDDIR(mg)) {
	    dTHR;
	    TAINT;
	    taint_proper("Insecure directory in %s%s", "$ENV{DCL$PATH}");
	}
	i++;
    }
  }
#endif /* VMS */

    svp = hv_fetch(GvHVn(PL_envgv),"PATH",4,FALSE);
    if (svp && *svp) {
	if (SvTAINTED(*svp)) {
	    dTHR;
	    TAINT;
	    taint_proper("Insecure %s%s", "$ENV{PATH}");
	}
	if ((mg = mg_find(*svp, 'e')) && MgTAINTEDDIR(mg)) {
	    dTHR;
	    TAINT;
	    taint_proper("Insecure directory in %s%s", "$ENV{PATH}");
	}
    }

#ifndef VMS
    /* tainted $TERM is okay if it contains no metachars */
    svp = hv_fetch(GvHVn(PL_envgv),"TERM",4,FALSE);
    if (svp && *svp && SvTAINTED(*svp)) {
    	dTHR;	/* just for taint */
	STRLEN n_a;
	bool was_tainted = PL_tainted;
	char *t = SvPV(*svp, n_a);
	char *e = t + n_a;
	PL_tainted = was_tainted;
	if (t < e && isALNUM(*t))
	    t++;
	while (t < e && (isALNUM(*t) || *t == '-' || *t == ':'))
	    t++;
	if (t < e) {
	    TAINT;
	    taint_proper("Insecure $ENV{%s}%s", "TERM");
	}
    }
#endif /* !VMS */

    for (e = misc_env; *e; e++) {
	svp = hv_fetch(GvHVn(PL_envgv), *e, strlen(*e), FALSE);
	if (svp && *svp != &PL_sv_undef && SvTAINTED(*svp)) {
	    dTHR;	/* just for taint */
	    TAINT;
	    taint_proper("Insecure $ENV{%s}%s", *e);
	}
    }
}
