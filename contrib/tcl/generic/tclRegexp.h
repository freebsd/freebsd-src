/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 *
 * SCCS: @(#) tclRegexp.h 1.6 96/04/02 18:43:57
 */

#ifndef _REGEXP
#define _REGEXP 1

#ifndef _TCL
#include "tcl.h"
#endif

/*
 * NSUBEXP must be at least 10, and no greater than 117 or the parser
 * will not work properly.
 */

#define NSUBEXP  20

typedef struct regexp {
	char *startp[NSUBEXP];
	char *endp[NSUBEXP];
	char regstart;		/* Internal use only. */
	char reganch;		/* Internal use only. */
	char *regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	char program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

EXTERN regexp *TclRegComp _ANSI_ARGS_((char *exp));
EXTERN int TclRegExec _ANSI_ARGS_((regexp *prog, char *string, char *start));
EXTERN void TclRegSub _ANSI_ARGS_((regexp *prog, char *source, char *dest));
EXTERN void TclRegError _ANSI_ARGS_((char *msg));
EXTERN char *TclGetRegError _ANSI_ARGS_((void));

#endif /* REGEXP */
