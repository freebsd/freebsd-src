/* We need access to debugger hooks */
#ifndef DEBUGGING
#  define DEBUGGING
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

extern regexp*	my_regcomp _((char* exp, char* xend, PMOP* pm));
extern I32	my_regexec _((regexp* prog, char* stringarg, char* strend,
			      char* strbeg, I32 minend, SV* screamer,
			      void* data, U32 flags));

static int oldfl;

#define R_DB 512

static void
deinstall(void)
{
    dTHR;
    PL_regexecp = &regexec_flags;
    PL_regcompp = &pregcomp;
    if (!oldfl)
	PL_debug &= ~R_DB;
}

static void
install(void)
{
    dTHR;
    PL_colorset = 0;			/* Allow reinspection of ENV. */
    PL_regexecp = &my_regexec;
    PL_regcompp = &my_regcomp;
    oldfl = PL_debug & R_DB;
    PL_debug |= R_DB;
}

MODULE = re	PACKAGE = re

void
install()

void
deinstall()
