/* We need access to debugger hooks */
#ifndef DEBUGGING
#  define DEBUGGING
#endif

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

extern regexp*	my_regcomp (pTHX_ char* exp, char* xend, PMOP* pm);
extern I32	my_regexec (pTHX_ regexp* prog, char* stringarg, char* strend,
			    char* strbeg, I32 minend, SV* screamer,
			    void* data, U32 flags);
extern void	my_regfree (pTHX_ struct regexp* r);
extern char*	my_re_intuit_start (pTHX_ regexp *prog, SV *sv, char *strpos,
				    char *strend, U32 flags,
				    struct re_scream_pos_data_s *data);
extern SV*	my_re_intuit_string (pTHX_ regexp *prog);

static int oldfl;

#define R_DB 512

static void
deinstall(pTHX)
{
    PL_regexecp = Perl_regexec_flags;
    PL_regcompp = Perl_pregcomp;
    PL_regint_start = Perl_re_intuit_start;
    PL_regint_string = Perl_re_intuit_string;
    PL_regfree = Perl_pregfree;

    if (!oldfl)
	PL_debug &= ~R_DB;
}

static void
install(pTHX)
{
    PL_colorset = 0;			/* Allow reinspection of ENV. */
    PL_regexecp = &my_regexec;
    PL_regcompp = &my_regcomp;
    PL_regint_start = &my_re_intuit_start;
    PL_regint_string = &my_re_intuit_string;
    PL_regfree = &my_regfree;
    oldfl = PL_debug & R_DB;
    PL_debug |= R_DB;
}

MODULE = re	PACKAGE = re

void
install()
  CODE:
    install(aTHX);

void
deinstall()
  CODE:
    deinstall(aTHX);
