#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "bsd_glob.h"

/* XXX: need some thread awareness */
static int GLOB_ERROR = 0;

static double
constant(char *name, int arg)
{
    errno = 0;
    if (strlen(name) <= 5)
        goto not_there;
    switch (*(name+5)) {
    case 'A':
	if (strEQ(name, "GLOB_ABEND"))
#ifdef GLOB_ABEND
	    return GLOB_ABEND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GLOB_ALPHASORT"))
#ifdef GLOB_ALPHASORT
	    return GLOB_ALPHASORT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GLOB_ALTDIRFUNC"))
#ifdef GLOB_ALTDIRFUNC
	    return GLOB_ALTDIRFUNC;
#else
	    goto not_there;
#endif
	break;
    case 'B':
	if (strEQ(name, "GLOB_BRACE"))
#ifdef GLOB_BRACE
	    return GLOB_BRACE;
#else
	    goto not_there;
#endif
	break;
    case 'C':
	break;
    case 'D':
	break;
    case 'E':
	if (strEQ(name, "GLOB_ERR"))
#ifdef GLOB_ERR
	    return GLOB_ERR;
#else
	    goto not_there;
#endif
        if (strEQ(name, "GLOB_ERROR"))
            return GLOB_ERROR;
        break;
    case 'F':
	break;
    case 'G':
        break;
    case 'H':
	break;
    case 'I':
	break;
    case 'J':
	break;
    case 'K':
	break;
    case 'L':
	break;
    case 'M':
	if (strEQ(name, "GLOB_MARK"))
#ifdef GLOB_MARK
	    return GLOB_MARK;
#else
	    goto not_there;
#endif
	break;
    case 'N':
	if (strEQ(name, "GLOB_NOCASE"))
#ifdef GLOB_NOCASE
	    return GLOB_NOCASE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GLOB_NOCHECK"))
#ifdef GLOB_NOCHECK
	    return GLOB_NOCHECK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GLOB_NOMAGIC"))
#ifdef GLOB_NOMAGIC
	    return GLOB_NOMAGIC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GLOB_NOSORT"))
#ifdef GLOB_NOSORT
	    return GLOB_NOSORT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "GLOB_NOSPACE"))
#ifdef GLOB_NOSPACE
	    return GLOB_NOSPACE;
#else
	    goto not_there;
#endif
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	if (strEQ(name, "GLOB_QUOTE"))
#ifdef GLOB_QUOTE
	    return GLOB_QUOTE;
#else
	    goto not_there;
#endif
	break;
    case 'R':
	break;
    case 'S':
	break;
    case 'T':
	if (strEQ(name, "GLOB_TILDE"))
#ifdef GLOB_TILDE
	    return GLOB_TILDE;
#else
	    goto not_there;
#endif
	break;
    case 'U':
	break;
    case 'V':
	break;
    case 'W':
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

#ifdef WIN32
#define errfunc		NULL
#else
int
errfunc(const char *foo, int bar) {
  return !(bar == ENOENT || bar == ENOTDIR);
}
#endif

MODULE = File::Glob		PACKAGE = File::Glob

void
doglob(pattern,...)
    char *pattern
PROTOTYPE: $;$
PREINIT:
    glob_t pglob;
    int i;
    int retval;
    int flags = 0;
    SV *tmp;
PPCODE:
    {
	/* allow for optional flags argument */
	if (items > 1) {
	    flags = (int) SvIV(ST(1));
	}

	/* call glob */
	retval = bsd_glob(pattern, flags, errfunc, &pglob);
	GLOB_ERROR = retval;

	/* return any matches found */
	EXTEND(sp, pglob.gl_pathc);
	for (i = 0; i < pglob.gl_pathc; i++) {
	    /* printf("# bsd_glob: %s\n", pglob.gl_pathv[i]); */
	    tmp = sv_2mortal(newSVpvn(pglob.gl_pathv[i],
				      strlen(pglob.gl_pathv[i])));
	    TAINT;
	    SvTAINT(tmp);
	    PUSHs(tmp);
	}

	bsd_globfree(&pglob);
    }

double
constant(name,arg)
    char *name
    int   arg
PROTOTYPE: $$
