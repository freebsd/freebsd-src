/*    util.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Very useful, no doubt, that was to Saruman; yet it seems that he was
 * not content."  --Gandalf
 */

#include "EXTERN.h"
#define PERL_IN_UTIL_C
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

#ifndef SIG_ERR
# define SIG_ERR ((Sighandler_t) -1)
#endif

#ifdef I_VFORK
#  include <vfork.h>
#endif

/* Put this after #includes because fork and vfork prototypes may
   conflict.
*/
#ifndef HAS_VFORK
#   define vfork fork
#endif

#ifdef I_SYS_WAIT
#  include <sys/wait.h>
#endif

#ifdef I_LOCALE
#  include <locale.h>
#endif

#define FLUSH

#ifdef LEAKTEST

long xcount[MAXXCOUNT];
long lastxcount[MAXXCOUNT];
long xycount[MAXXCOUNT][MAXYCOUNT];
long lastxycount[MAXXCOUNT][MAXYCOUNT];

#endif

#if defined(HAS_FCNTL) && defined(F_SETFD) && !defined(FD_CLOEXEC)
#  define FD_CLOEXEC 1			/* NeXT needs this */
#endif

/* paranoid version of system's malloc() */

/* NOTE:  Do not call the next three routines directly.  Use the macros
 * in handy.h, so that we can easily redefine everything to do tracking of
 * allocated hunks back to the original New to track down any memory leaks.
 * XXX This advice seems to be widely ignored :-(   --AD  August 1996.
 */

Malloc_t
Perl_safesysmalloc(MEM_SIZE size)
{
    dTHX;
    Malloc_t ptr;
#ifdef HAS_64K_LIMIT
	if (size > 0xffff) {
	    PerlIO_printf(Perl_error_log,
			  "Allocation too large: %lx\n", size) FLUSH;
	    my_exit(1);
	}
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
    if ((long)size < 0)
	Perl_croak_nocontext("panic: malloc");
#endif
    ptr = PerlMem_malloc(size?size:1);	/* malloc(0) is NASTY on our system */
    PERL_ALLOC_CHECK(ptr);
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) malloc %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)size));
    if (ptr != Nullch)
	return ptr;
    else if (PL_nomemok)
	return Nullch;
    else {
	PerlIO_puts(Perl_error_log,PL_no_mem) FLUSH;
	my_exit(1);
        return Nullch;
    }
    /*NOTREACHED*/
}

/* paranoid version of system's realloc() */

Malloc_t
Perl_safesysrealloc(Malloc_t where,MEM_SIZE size)
{
    dTHX;
    Malloc_t ptr;
#if !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) && !defined(PERL_MICRO)
    Malloc_t PerlMem_realloc();
#endif /* !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) */

#ifdef HAS_64K_LIMIT 
    if (size > 0xffff) {
	PerlIO_printf(Perl_error_log,
		      "Reallocation too large: %lx\n", size) FLUSH;
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
    if (!size) {
	safesysfree(where);
	return NULL;
    }

    if (!where)
	return safesysmalloc(size);
#ifdef DEBUGGING
    if ((long)size < 0)
	Perl_croak_nocontext("panic: realloc");
#endif
    ptr = PerlMem_realloc(where,size);
    PERL_ALLOC_CHECK(ptr);
 
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) rfree\n",PTR2UV(where),(long)PL_an++));
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) realloc %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)size));

    if (ptr != Nullch)
	return ptr;
    else if (PL_nomemok)
	return Nullch;
    else {
	PerlIO_puts(Perl_error_log,PL_no_mem) FLUSH;
	my_exit(1);
	return Nullch;
    }
    /*NOTREACHED*/
}

/* safe version of system's free() */

Free_t
Perl_safesysfree(Malloc_t where)
{
#ifdef PERL_IMPLICIT_SYS
    dTHX;
#endif
    DEBUG_m( PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) free\n",PTR2UV(where),(long)PL_an++));
    if (where) {
	/*SUPPRESS 701*/
	PerlMem_free(where);
    }
}

/* safe version of system's calloc() */

Malloc_t
Perl_safesyscalloc(MEM_SIZE count, MEM_SIZE size)
{
    dTHX;
    Malloc_t ptr;

#ifdef HAS_64K_LIMIT
    if (size * count > 0xffff) {
	PerlIO_printf(Perl_error_log,
		      "Allocation too large: %lx\n", size * count) FLUSH;
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
    if ((long)size < 0 || (long)count < 0)
	Perl_croak_nocontext("panic: calloc");
#endif
    size *= count;
    ptr = PerlMem_malloc(size?size:1);	/* malloc(0) is NASTY on our system */
    PERL_ALLOC_CHECK(ptr);
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%"UVxf": (%05ld) calloc %ld x %ld bytes\n",PTR2UV(ptr),(long)PL_an++,(long)count,(long)size));
    if (ptr != Nullch) {
	memset((void*)ptr, 0, size);
	return ptr;
    }
    else if (PL_nomemok)
	return Nullch;
    else {
	PerlIO_puts(Perl_error_log,PL_no_mem) FLUSH;
	my_exit(1);
	return Nullch;
    }
    /*NOTREACHED*/
}

#ifdef LEAKTEST

struct mem_test_strut {
    union {
	long type;
	char c[2];
    } u;
    long size;
};

#    define ALIGN sizeof(struct mem_test_strut)

#    define sizeof_chunk(ch) (((struct mem_test_strut*) (ch))->size)
#    define typeof_chunk(ch) \
	(((struct mem_test_strut*) (ch))->u.c[0] + ((struct mem_test_strut*) (ch))->u.c[1]*100)
#    define set_typeof_chunk(ch,t) \
	(((struct mem_test_strut*) (ch))->u.c[0] = t % 100, ((struct mem_test_strut*) (ch))->u.c[1] = t / 100)
#define SIZE_TO_Y(size) ( (size) > MAXY_SIZE				\
			  ? MAXYCOUNT - 1 				\
			  : ( (size) > 40 				\
			      ? ((size) - 1)/8 + 5			\
			      : ((size) - 1)/4))

Malloc_t
Perl_safexmalloc(I32 x, MEM_SIZE size)
{
    register char* where = (char*)safemalloc(size + ALIGN);

    xcount[x] += size;
    xycount[x][SIZE_TO_Y(size)]++;
    set_typeof_chunk(where, x);
    sizeof_chunk(where) = size;
    return (Malloc_t)(where + ALIGN);
}

Malloc_t
Perl_safexrealloc(Malloc_t wh, MEM_SIZE size)
{
    char *where = (char*)wh;

    if (!wh)
	return safexmalloc(0,size);
    
    {
	MEM_SIZE old = sizeof_chunk(where - ALIGN);
	int t = typeof_chunk(where - ALIGN);
	register char* new = (char*)saferealloc(where - ALIGN, size + ALIGN);
    
	xycount[t][SIZE_TO_Y(old)]--;
	xycount[t][SIZE_TO_Y(size)]++;
	xcount[t] += size - old;
	sizeof_chunk(new) = size;
	return (Malloc_t)(new + ALIGN);
    }
}

void
Perl_safexfree(Malloc_t wh)
{
    I32 x;
    char *where = (char*)wh;
    MEM_SIZE size;
    
    if (!where)
	return;
    where -= ALIGN;
    size = sizeof_chunk(where);
    x = where[0] + 100 * where[1];
    xcount[x] -= size;
    xycount[x][SIZE_TO_Y(size)]--;
    safefree(where);
}

Malloc_t
Perl_safexcalloc(I32 x,MEM_SIZE count, MEM_SIZE size)
{
    register char * where = (char*)safexmalloc(x, size * count + ALIGN);
    xcount[x] += size;
    xycount[x][SIZE_TO_Y(size)]++;
    memset((void*)(where + ALIGN), 0, size * count);
    set_typeof_chunk(where, x);
    sizeof_chunk(where) = size;
    return (Malloc_t)(where + ALIGN);
}

STATIC void
S_xstat(pTHX_ int flag)
{
    register I32 i, j, total = 0;
    I32 subtot[MAXYCOUNT];

    for (j = 0; j < MAXYCOUNT; j++) {
	subtot[j] = 0;
    }
    
    PerlIO_printf(Perl_debug_log, "   Id  subtot   4   8  12  16  20  24  28  32  36  40  48  56  64  72  80 80+\n", total);
    for (i = 0; i < MAXXCOUNT; i++) {
	total += xcount[i];
	for (j = 0; j < MAXYCOUNT; j++) {
	    subtot[j] += xycount[i][j];
	}
	if (flag == 0
	    ? xcount[i]			/* Have something */
	    : (flag == 2 
	       ? xcount[i] != lastxcount[i] /* Changed */
	       : xcount[i] > lastxcount[i])) { /* Growed */
	    PerlIO_printf(Perl_debug_log,"%2d %02d %7ld ", i / 100, i % 100, 
			  flag == 2 ? xcount[i] - lastxcount[i] : xcount[i]);
	    lastxcount[i] = xcount[i];
	    for (j = 0; j < MAXYCOUNT; j++) {
		if ( flag == 0 
		     ? xycount[i][j]	/* Have something */
		     : (flag == 2 
			? xycount[i][j] != lastxycount[i][j] /* Changed */
			: xycount[i][j] > lastxycount[i][j])) {	/* Growed */
		    PerlIO_printf(Perl_debug_log,"%3ld ", 
				  flag == 2 
				  ? xycount[i][j] - lastxycount[i][j] 
				  : xycount[i][j]);
		    lastxycount[i][j] = xycount[i][j];
		} else {
		    PerlIO_printf(Perl_debug_log, "  . ", xycount[i][j]);
		}
	    }
	    PerlIO_printf(Perl_debug_log, "\n");
	}
    }
    if (flag != 2) {
	PerlIO_printf(Perl_debug_log, "Total %7ld ", total);
	for (j = 0; j < MAXYCOUNT; j++) {
	    if (subtot[j]) {
		PerlIO_printf(Perl_debug_log, "%3ld ", subtot[j]);
	    } else {
		PerlIO_printf(Perl_debug_log, "  . ");
	    }
	}
	PerlIO_printf(Perl_debug_log, "\n");	
    }
}

#endif /* LEAKTEST */

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
Perl_delimcpy(pTHX_ register char *to, register char *toend, register char *from, register char *fromend, register int delim, I32 *retlen)
{
    register I32 tolen;
    for (tolen = 0; from < fromend; from++, tolen++) {
	if (*from == '\\') {
	    if (from[1] == delim)
		from++;
	    else {
		if (to < toend)
		    *to++ = *from;
		tolen++;
		from++;
	    }
	}
	else if (*from == delim)
	    break;
	if (to < toend)
	    *to++ = *from;
    }
    if (to < toend)
	*to = '\0';
    *retlen = tolen;
    return from;
}

/* return ptr to little string in big string, NULL if not found */
/* This routine was donated by Corey Satten. */

char *
Perl_instr(pTHX_ register const char *big, register const char *little)
{
    register const char *s, *x;
    register I32 first;

    if (!little)
	return (char*)big;
    first = *little++;
    if (!first)
	return (char*)big;
    while (*big) {
	if (*big++ != first)
	    continue;
	for (x=big,s=little; *s; /**/ ) {
	    if (!*x)
		return Nullch;
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (!*s)
	    return (char*)(big-1);
    }
    return Nullch;
}

/* same as instr but allow embedded nulls */

char *
Perl_ninstr(pTHX_ register const char *big, register const char *bigend, const char *little, const char *lend)
{
    register const char *s, *x;
    register I32 first = *little;
    register const char *littleend = lend;

    if (!first && little >= littleend)
	return (char*)big;
    if (bigend - big < littleend - little)
	return Nullch;
    bigend -= littleend - little++;
    while (big <= bigend) {
	if (*big++ != first)
	    continue;
	for (x=big,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s >= littleend)
	    return (char*)(big-1);
    }
    return Nullch;
}

/* reverse of the above--find last substring */

char *
Perl_rninstr(pTHX_ register const char *big, const char *bigend, const char *little, const char *lend)
{
    register const char *bigbeg;
    register const char *s, *x;
    register I32 first = *little;
    register const char *littleend = lend;

    if (!first && little >= littleend)
	return (char*)bigend;
    bigbeg = big;
    big = bigend - (littleend - little++);
    while (big >= bigbeg) {
	if (*big-- != first)
	    continue;
	for (x=big+2,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s >= littleend)
	    return (char*)(big+1);
    }
    return Nullch;
}

/*
 * Set up for a new ctype locale.
 */
void
Perl_new_ctype(pTHX_ char *newctype)
{
#ifdef USE_LOCALE_CTYPE

    int i;

    for (i = 0; i < 256; i++) {
	if (isUPPER_LC(i))
	    PL_fold_locale[i] = toLOWER_LC(i);
	else if (isLOWER_LC(i))
	    PL_fold_locale[i] = toUPPER_LC(i);
	else
	    PL_fold_locale[i] = i;
    }

#endif /* USE_LOCALE_CTYPE */
}

/*
 * Standardize the locale name from a string returned by 'setlocale'.
 *
 * The standard return value of setlocale() is either
 * (1) "xx_YY" if the first argument of setlocale() is not LC_ALL
 * (2) "xa_YY xb_YY ..." if the first argument of setlocale() is LC_ALL
 *     (the space-separated values represent the various sublocales,
 *      in some unspecificed order)
 *
 * In some platforms it has a form like "LC_SOMETHING=Lang_Country.866\n",
 * which is harmful for further use of the string in setlocale().
 *
 */
STATIC char *
S_stdize_locale(pTHX_ char *locs)
{
    char *s;
    bool okay = TRUE;

    if ((s = strchr(locs, '='))) {
	char *t;

	okay = FALSE;
	if ((t = strchr(s, '.'))) {
	    char *u;

	    if ((u = strchr(t, '\n'))) {

		if (u[1] == 0) {
		    STRLEN len = u - s;
		    Move(s + 1, locs, len, char);
		    locs[len] = 0;
		    okay = TRUE;
		}
	    }
	}
    }

    if (!okay)
	Perl_croak(aTHX_ "Can't fix broken locale name \"%s\"", locs);

    return locs;
}

/*
 * Set up for a new collation locale.
 */
void
Perl_new_collate(pTHX_ char *newcoll)
{
#ifdef USE_LOCALE_COLLATE

    if (! newcoll) {
	if (PL_collation_name) {
	    ++PL_collation_ix;
	    Safefree(PL_collation_name);
	    PL_collation_name = NULL;
	}
	PL_collation_standard = TRUE;
	PL_collxfrm_base = 0;
	PL_collxfrm_mult = 2;
	return;
    }

    if (! PL_collation_name || strNE(PL_collation_name, newcoll)) {
	++PL_collation_ix;
	Safefree(PL_collation_name);
	PL_collation_name = stdize_locale(savepv(newcoll));
	PL_collation_standard = (strEQ(newcoll, "C") || strEQ(newcoll, "POSIX"));

	{
	  /*  2: at most so many chars ('a', 'b'). */
	  /* 50: surely no system expands a char more. */
#define XFRMBUFSIZE  (2 * 50)
	  char xbuf[XFRMBUFSIZE];
	  Size_t fa = strxfrm(xbuf, "a",  XFRMBUFSIZE);
	  Size_t fb = strxfrm(xbuf, "ab", XFRMBUFSIZE);
	  SSize_t mult = fb - fa;
	  if (mult < 1)
	      Perl_croak(aTHX_ "strxfrm() gets absurd");
	  PL_collxfrm_base = (fa > mult) ? (fa - mult) : 0;
	  PL_collxfrm_mult = mult;
	}
    }

#endif /* USE_LOCALE_COLLATE */
}

void
Perl_set_numeric_radix(pTHX)
{
#ifdef USE_LOCALE_NUMERIC
# ifdef HAS_LOCALECONV
    struct lconv* lc;

    lc = localeconv();
    if (lc && lc->decimal_point) {
	if (lc->decimal_point[0] == '.' && lc->decimal_point[1] == 0) {
	    SvREFCNT_dec(PL_numeric_radix_sv);
	    PL_numeric_radix_sv = 0;
	}
	else {
	    if (PL_numeric_radix_sv)
		sv_setpv(PL_numeric_radix_sv, lc->decimal_point);
	    else
		PL_numeric_radix_sv = newSVpv(lc->decimal_point, 0);
	}
    }
    else
	PL_numeric_radix_sv = 0;
# endif /* HAS_LOCALECONV */
#endif /* USE_LOCALE_NUMERIC */
}

/*
 * Set up for a new numeric locale.
 */
void
Perl_new_numeric(pTHX_ char *newnum)
{
#ifdef USE_LOCALE_NUMERIC

    if (! newnum) {
	if (PL_numeric_name) {
	    Safefree(PL_numeric_name);
	    PL_numeric_name = NULL;
	}
	PL_numeric_standard = TRUE;
	PL_numeric_local = TRUE;
	return;
    }

    if (! PL_numeric_name || strNE(PL_numeric_name, newnum)) {
	Safefree(PL_numeric_name);
	PL_numeric_name = stdize_locale(savepv(newnum));
	PL_numeric_standard = (strEQ(newnum, "C") || strEQ(newnum, "POSIX"));
	PL_numeric_local = TRUE;
	set_numeric_radix();
    }

#endif /* USE_LOCALE_NUMERIC */
}

void
Perl_set_numeric_standard(pTHX)
{
#ifdef USE_LOCALE_NUMERIC

    if (! PL_numeric_standard) {
	setlocale(LC_NUMERIC, "C");
	PL_numeric_standard = TRUE;
	PL_numeric_local = FALSE;
	set_numeric_radix();
    }

#endif /* USE_LOCALE_NUMERIC */
}

void
Perl_set_numeric_local(pTHX)
{
#ifdef USE_LOCALE_NUMERIC

    if (! PL_numeric_local) {
	setlocale(LC_NUMERIC, PL_numeric_name);
	PL_numeric_standard = FALSE;
	PL_numeric_local = TRUE;
	set_numeric_radix();
    }

#endif /* USE_LOCALE_NUMERIC */
}

/*
 * Initialize locale awareness.
 */
int
Perl_init_i18nl10n(pTHX_ int printwarn)
{
    int ok = 1;
    /* returns
     *    1 = set ok or not applicable,
     *    0 = fallback to C locale,
     *   -1 = fallback to C locale failed
     */

#if defined(USE_LOCALE)

#ifdef USE_LOCALE_CTYPE
    char *curctype   = NULL;
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
    char *curcoll    = NULL;
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
    char *curnum     = NULL;
#endif /* USE_LOCALE_NUMERIC */
#ifdef __GLIBC__
    char *language   = PerlEnv_getenv("LANGUAGE");
#endif
    char *lc_all     = PerlEnv_getenv("LC_ALL");
    char *lang       = PerlEnv_getenv("LANG");
    bool setlocale_failure = FALSE;

#ifdef LOCALE_ENVIRON_REQUIRED

    /*
     * Ultrix setlocale(..., "") fails if there are no environment
     * variables from which to get a locale name.
     */

    bool done = FALSE;

#ifdef LC_ALL
    if (lang) {
	if (setlocale(LC_ALL, ""))
	    done = TRUE;
	else
	    setlocale_failure = TRUE;
    }
    if (!setlocale_failure) {
#ifdef USE_LOCALE_CTYPE
	if (! (curctype =
	       setlocale(LC_CTYPE,
			 (!done && (lang || PerlEnv_getenv("LC_CTYPE")))
				    ? "" : Nullch)))
	    setlocale_failure = TRUE;
	else
	    curctype = savepv(curctype);
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	if (! (curcoll =
	       setlocale(LC_COLLATE,
			 (!done && (lang || PerlEnv_getenv("LC_COLLATE")))
				   ? "" : Nullch)))
	    setlocale_failure = TRUE;
	else
	    curcoll = savepv(curcoll);
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	if (! (curnum =
	       setlocale(LC_NUMERIC,
			 (!done && (lang || PerlEnv_getenv("LC_NUMERIC")))
				  ? "" : Nullch)))
	    setlocale_failure = TRUE;
	else
	    curnum = savepv(curnum);
#endif /* USE_LOCALE_NUMERIC */
    }

#endif /* LC_ALL */

#endif /* !LOCALE_ENVIRON_REQUIRED */

#ifdef LC_ALL
    if (! setlocale(LC_ALL, ""))
	setlocale_failure = TRUE;
#endif /* LC_ALL */

    if (!setlocale_failure) {
#ifdef USE_LOCALE_CTYPE
	if (! (curctype = setlocale(LC_CTYPE, "")))
	    setlocale_failure = TRUE;
	else
	    curctype = savepv(curctype);
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	if (! (curcoll = setlocale(LC_COLLATE, "")))
	    setlocale_failure = TRUE;
	else
	    curcoll = savepv(curcoll);
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	if (! (curnum = setlocale(LC_NUMERIC, "")))
	    setlocale_failure = TRUE;
	else
	    curnum = savepv(curnum);
#endif /* USE_LOCALE_NUMERIC */
    }

    if (setlocale_failure) {
	char *p;
	bool locwarn = (printwarn > 1 || 
			(printwarn &&
			 (!(p = PerlEnv_getenv("PERL_BADLANG")) || atoi(p))));

	if (locwarn) {
#ifdef LC_ALL
  
	    PerlIO_printf(Perl_error_log,
	       "perl: warning: Setting locale failed.\n");

#else /* !LC_ALL */
  
	    PerlIO_printf(Perl_error_log,
	       "perl: warning: Setting locale failed for the categories:\n\t");
#ifdef USE_LOCALE_CTYPE
	    if (! curctype)
		PerlIO_printf(Perl_error_log, "LC_CTYPE ");
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	    if (! curcoll)
		PerlIO_printf(Perl_error_log, "LC_COLLATE ");
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	    if (! curnum)
		PerlIO_printf(Perl_error_log, "LC_NUMERIC ");
#endif /* USE_LOCALE_NUMERIC */
	    PerlIO_printf(Perl_error_log, "\n");

#endif /* LC_ALL */

	    PerlIO_printf(Perl_error_log,
		"perl: warning: Please check that your locale settings:\n");

#ifdef __GLIBC__
	    PerlIO_printf(Perl_error_log,
			  "\tLANGUAGE = %c%s%c,\n",
			  language ? '"' : '(',
			  language ? language : "unset",
			  language ? '"' : ')');
#endif

	    PerlIO_printf(Perl_error_log,
			  "\tLC_ALL = %c%s%c,\n",
			  lc_all ? '"' : '(',
			  lc_all ? lc_all : "unset",
			  lc_all ? '"' : ')');

#if defined(USE_ENVIRON_ARRAY)
	    {
	      char **e;
	      for (e = environ; *e; e++) {
		  if (strnEQ(*e, "LC_", 3)
			&& strnNE(*e, "LC_ALL=", 7)
			&& (p = strchr(*e, '=')))
		      PerlIO_printf(Perl_error_log, "\t%.*s = \"%s\",\n",
				    (int)(p - *e), *e, p + 1);
	      }
	    }
#else
	    PerlIO_printf(Perl_error_log,
			  "\t(possibly more locale environment variables)\n");
#endif

	    PerlIO_printf(Perl_error_log,
			  "\tLANG = %c%s%c\n",
			  lang ? '"' : '(',
			  lang ? lang : "unset",
			  lang ? '"' : ')');

	    PerlIO_printf(Perl_error_log,
			  "    are supported and installed on your system.\n");
	}

#ifdef LC_ALL

	if (setlocale(LC_ALL, "C")) {
	    if (locwarn)
		PerlIO_printf(Perl_error_log,
      "perl: warning: Falling back to the standard locale (\"C\").\n");
	    ok = 0;
	}
	else {
	    if (locwarn)
		PerlIO_printf(Perl_error_log,
      "perl: warning: Failed to fall back to the standard locale (\"C\").\n");
	    ok = -1;
	}

#else /* ! LC_ALL */

	if (0
#ifdef USE_LOCALE_CTYPE
	    || !(curctype || setlocale(LC_CTYPE, "C"))
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	    || !(curcoll || setlocale(LC_COLLATE, "C"))
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	    || !(curnum || setlocale(LC_NUMERIC, "C"))
#endif /* USE_LOCALE_NUMERIC */
	    )
	{
	    if (locwarn)
		PerlIO_printf(Perl_error_log,
      "perl: warning: Cannot fall back to the standard locale (\"C\").\n");
	    ok = -1;
	}

#endif /* ! LC_ALL */

#ifdef USE_LOCALE_CTYPE
	curctype = savepv(setlocale(LC_CTYPE, Nullch));
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	curcoll = savepv(setlocale(LC_COLLATE, Nullch));
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	curnum = savepv(setlocale(LC_NUMERIC, Nullch));
#endif /* USE_LOCALE_NUMERIC */
    }
    else {

#ifdef USE_LOCALE_CTYPE
    new_ctype(curctype);
#endif /* USE_LOCALE_CTYPE */

#ifdef USE_LOCALE_COLLATE
    new_collate(curcoll);
#endif /* USE_LOCALE_COLLATE */

#ifdef USE_LOCALE_NUMERIC
    new_numeric(curnum);
#endif /* USE_LOCALE_NUMERIC */
    }

#endif /* USE_LOCALE */

#ifdef USE_LOCALE_CTYPE
    if (curctype != NULL)
	Safefree(curctype);
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
    if (curcoll != NULL)
	Safefree(curcoll);
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
    if (curnum != NULL)
	Safefree(curnum);
#endif /* USE_LOCALE_NUMERIC */
    return ok;
}

/* Backwards compatibility. */
int
Perl_init_i18nl14n(pTHX_ int printwarn)
{
    return init_i18nl10n(printwarn);
}

#ifdef USE_LOCALE_COLLATE

/*
 * mem_collxfrm() is a bit like strxfrm() but with two important
 * differences. First, it handles embedded NULs. Second, it allocates
 * a bit more memory than needed for the transformed data itself.
 * The real transformed data begins at offset sizeof(collationix).
 * Please see sv_collxfrm() to see how this is used.
 */
char *
Perl_mem_collxfrm(pTHX_ const char *s, STRLEN len, STRLEN *xlen)
{
    char *xbuf;
    STRLEN xAlloc, xin, xout; /* xalloc is a reserved word in VC */

    /* the first sizeof(collationix) bytes are used by sv_collxfrm(). */
    /* the +1 is for the terminating NUL. */

    xAlloc = sizeof(PL_collation_ix) + PL_collxfrm_base + (PL_collxfrm_mult * len) + 1;
    New(171, xbuf, xAlloc, char);
    if (! xbuf)
	goto bad;

    *(U32*)xbuf = PL_collation_ix;
    xout = sizeof(PL_collation_ix);
    for (xin = 0; xin < len; ) {
	SSize_t xused;

	for (;;) {
	    xused = strxfrm(xbuf + xout, s + xin, xAlloc - xout);
	    if (xused == -1)
		goto bad;
	    if (xused < xAlloc - xout)
		break;
	    xAlloc = (2 * xAlloc) + 1;
	    Renew(xbuf, xAlloc, char);
	    if (! xbuf)
		goto bad;
	}

	xin += strlen(s + xin) + 1;
	xout += xused;

	/* Embedded NULs are understood but silently skipped
	 * because they make no sense in locale collation. */
    }

    xbuf[xout] = '\0';
    *xlen = xout - sizeof(PL_collation_ix);
    return xbuf;

  bad:
    Safefree(xbuf);
    *xlen = 0;
    return NULL;
}

#endif /* USE_LOCALE_COLLATE */

#define FBM_TABLE_OFFSET 2	/* Number of bytes between EOS and table*/

/* As a space optimization, we do not compile tables for strings of length
   0 and 1, and for strings of length 2 unless FBMcf_TAIL.  These are
   special-cased in fbm_instr().

   If FBMcf_TAIL, the table is created as if the string has a trailing \n. */

/*
=for apidoc fbm_compile

Analyses the string in order to make fast searches on it using fbm_instr()
-- the Boyer-Moore algorithm.

=cut
*/

void
Perl_fbm_compile(pTHX_ SV *sv, U32 flags)
{
    register U8 *s;
    register U8 *table;
    register U32 i;
    STRLEN len;
    I32 rarest = 0;
    U32 frequency = 256;

    if (flags & FBMcf_TAIL)
	sv_catpvn(sv, "\n", 1);		/* Taken into account in fbm_instr() */
    s = (U8*)SvPV_force(sv, len);
    (void)SvUPGRADE(sv, SVt_PVBM);
    if (len == 0)		/* TAIL might be on on a zero-length string. */
	return;
    if (len > 2) {
	U8 mlen;
	unsigned char *sb;

	if (len > 255)
	    mlen = 255;
	else
	    mlen = (U8)len;
	Sv_Grow(sv, len + 256 + FBM_TABLE_OFFSET);
	table = (unsigned char*)(SvPVX(sv) + len + FBM_TABLE_OFFSET);
	s = table - 1 - FBM_TABLE_OFFSET;	/* last char */
	memset((void*)table, mlen, 256);
	table[-1] = (U8)flags;
	i = 0;
	sb = s - mlen + 1;			/* first char (maybe) */
	while (s >= sb) {
	    if (table[*s] == mlen)
		table[*s] = (U8)i;
	    s--, i++;
	}
    }
    sv_magic(sv, Nullsv, 'B', Nullch, 0);	/* deep magic */
    SvVALID_on(sv);

    s = (unsigned char*)(SvPVX(sv));		/* deeper magic */
    for (i = 0; i < len; i++) {
	if (PL_freq[s[i]] < frequency) {
	    rarest = i;
	    frequency = PL_freq[s[i]];
	}
    }
    BmRARE(sv) = s[rarest];
    BmPREVIOUS(sv) = rarest;
    BmUSEFUL(sv) = 100;			/* Initial value */
    if (flags & FBMcf_TAIL)
	SvTAIL_on(sv);
    DEBUG_r(PerlIO_printf(Perl_debug_log, "rarest char %c at %d\n",
			  BmRARE(sv),BmPREVIOUS(sv)));
}

/* If SvTAIL(littlestr), it has a fake '\n' at end. */
/* If SvTAIL is actually due to \Z or \z, this gives false positives
   if multiline */

/*
=for apidoc fbm_instr

Returns the location of the SV in the string delimited by C<str> and
C<strend>.  It returns C<Nullch> if the string can't be found.  The C<sv>
does not have to be fbm_compiled, but the search will not be as fast
then.

=cut
*/

char *
Perl_fbm_instr(pTHX_ unsigned char *big, register unsigned char *bigend, SV *littlestr, U32 flags)
{
    register unsigned char *s;
    STRLEN l;
    register unsigned char *little = (unsigned char *)SvPV(littlestr,l);
    register STRLEN littlelen = l;
    register I32 multiline = flags & FBMrf_MULTILINE;

    if (bigend - big < littlelen) {
	if ( SvTAIL(littlestr) 
	     && (bigend - big == littlelen - 1)
	     && (littlelen == 1 
		 || (*big == *little && memEQ(big, little, littlelen - 1))))
	    return (char*)big;
	return Nullch;
    }

    if (littlelen <= 2) {		/* Special-cased */

	if (littlelen == 1) {
	    if (SvTAIL(littlestr) && !multiline) { /* Anchor only! */
		/* Know that bigend != big.  */
		if (bigend[-1] == '\n')
		    return (char *)(bigend - 1);
		return (char *) bigend;
	    }
	    s = big;
	    while (s < bigend) {
		if (*s == *little)
		    return (char *)s;
		s++;
	    }
	    if (SvTAIL(littlestr))
		return (char *) bigend;
	    return Nullch;
	}
	if (!littlelen)
	    return (char*)big;		/* Cannot be SvTAIL! */

	/* littlelen is 2 */
	if (SvTAIL(littlestr) && !multiline) {
	    if (bigend[-1] == '\n' && bigend[-2] == *little)
		return (char*)bigend - 2;
	    if (bigend[-1] == *little)
		return (char*)bigend - 1;
	    return Nullch;
	}
	{
	    /* This should be better than FBM if c1 == c2, and almost
	       as good otherwise: maybe better since we do less indirection.
	       And we save a lot of memory by caching no table. */
	    register unsigned char c1 = little[0];
	    register unsigned char c2 = little[1];

	    s = big + 1;
	    bigend--;
	    if (c1 != c2) {
		while (s <= bigend) {
		    if (s[0] == c2) {
			if (s[-1] == c1)
			    return (char*)s - 1;
			s += 2;
			continue;
		    }
		  next_chars:
		    if (s[0] == c1) {
			if (s == bigend)
			    goto check_1char_anchor;
			if (s[1] == c2)
			    return (char*)s;
			else {
			    s++;
			    goto next_chars;
			}
		    }
		    else
			s += 2;
		}
		goto check_1char_anchor;
	    }
	    /* Now c1 == c2 */
	    while (s <= bigend) {
		if (s[0] == c1) {
		    if (s[-1] == c1)
			return (char*)s - 1;
		    if (s == bigend)
			goto check_1char_anchor;
		    if (s[1] == c1)
			return (char*)s;
		    s += 3;
		}
		else
		    s += 2;
	    }
	}
      check_1char_anchor:		/* One char and anchor! */
	if (SvTAIL(littlestr) && (*bigend == *little))
	    return (char *)bigend;	/* bigend is already decremented. */
	return Nullch;
    }
    if (SvTAIL(littlestr) && !multiline) {	/* tail anchored? */
	s = bigend - littlelen;
	if (s >= big && bigend[-1] == '\n' && *s == *little 
	    /* Automatically of length > 2 */
	    && memEQ((char*)s + 1, (char*)little + 1, littlelen - 2))
	{
	    return (char*)s;		/* how sweet it is */
	}
	if (s[1] == *little
	    && memEQ((char*)s + 2, (char*)little + 1, littlelen - 2))
	{
	    return (char*)s + 1;	/* how sweet it is */
	}
	return Nullch;
    }
    if (SvTYPE(littlestr) != SVt_PVBM || !SvVALID(littlestr)) {
	char *b = ninstr((char*)big,(char*)bigend,
			 (char*)little, (char*)little + littlelen);

	if (!b && SvTAIL(littlestr)) {	/* Automatically multiline!  */
	    /* Chop \n from littlestr: */
	    s = bigend - littlelen + 1;
	    if (*s == *little
		&& memEQ((char*)s + 1, (char*)little + 1, littlelen - 2))
	    {
		return (char*)s;
	    }
	    return Nullch;
	}
	return b;
    }
    
    {	/* Do actual FBM.  */
	register unsigned char *table = little + littlelen + FBM_TABLE_OFFSET;
	register unsigned char *oldlittle;

	if (littlelen > bigend - big)
	    return Nullch;
	--littlelen;			/* Last char found by table lookup */

	s = big + littlelen;
	little += littlelen;		/* last char */
	oldlittle = little;
	if (s < bigend) {
	    register I32 tmp;

	  top2:
	    /*SUPPRESS 560*/
	    if ((tmp = table[*s])) {
#ifdef POINTERRIGOR
		if (bigend - s > tmp) {
		    s += tmp;
		    goto top2;
		}
		s += tmp;
#else
		if ((s += tmp) < bigend)
		    goto top2;
#endif
		goto check_end;
	    }
	    else {		/* less expensive than calling strncmp() */
		register unsigned char *olds = s;

		tmp = littlelen;

		while (tmp--) {
		    if (*--s == *--little)
			continue;
		    s = olds + 1;	/* here we pay the price for failure */
		    little = oldlittle;
		    if (s < bigend)	/* fake up continue to outer loop */
			goto top2;
		    goto check_end;
		}
		return (char *)s;
	    }
	}
      check_end:
	if ( s == bigend && (table[-1] & FBMcf_TAIL)
	     && memEQ(bigend - littlelen, oldlittle - littlelen, littlelen) )
	    return (char*)bigend - littlelen;
	return Nullch;
    }
}

/* start_shift, end_shift are positive quantities which give offsets
   of ends of some substring of bigstr.
   If `last' we want the last occurence.
   old_posp is the way of communication between consequent calls if
   the next call needs to find the . 
   The initial *old_posp should be -1.

   Note that we take into account SvTAIL, so one can get extra
   optimizations if _ALL flag is set.
 */

/* If SvTAIL is actually due to \Z or \z, this gives false positives
   if PL_multiline.  In fact if !PL_multiline the autoritative answer
   is not supported yet. */

char *
Perl_screaminstr(pTHX_ SV *bigstr, SV *littlestr, I32 start_shift, I32 end_shift, I32 *old_posp, I32 last)
{
    register unsigned char *s, *x;
    register unsigned char *big;
    register I32 pos;
    register I32 previous;
    register I32 first;
    register unsigned char *little;
    register I32 stop_pos;
    register unsigned char *littleend;
    I32 found = 0;

    if (*old_posp == -1
	? (pos = PL_screamfirst[BmRARE(littlestr)]) < 0
	: (((pos = *old_posp), pos += PL_screamnext[pos]) == 0)) {
      cant_find:
	if ( BmRARE(littlestr) == '\n' 
	     && BmPREVIOUS(littlestr) == SvCUR(littlestr) - 1) {
	    little = (unsigned char *)(SvPVX(littlestr));
	    littleend = little + SvCUR(littlestr);
	    first = *little++;
	    goto check_tail;
	}
	return Nullch;
    }

    little = (unsigned char *)(SvPVX(littlestr));
    littleend = little + SvCUR(littlestr);
    first = *little++;
    /* The value of pos we can start at: */
    previous = BmPREVIOUS(littlestr);
    big = (unsigned char *)(SvPVX(bigstr));
    /* The value of pos we can stop at: */
    stop_pos = SvCUR(bigstr) - end_shift - (SvCUR(littlestr) - 1 - previous);
    if (previous + start_shift > stop_pos) {
	if (previous + start_shift == stop_pos + 1) /* A fake '\n'? */
	    goto check_tail;
	return Nullch;
    }
    while (pos < previous + start_shift) {
	if (!(pos += PL_screamnext[pos]))
	    goto cant_find;
    }
#ifdef POINTERRIGOR
    do {
	if (pos >= stop_pos) break;
	if (big[pos-previous] != first)
	    continue;
	for (x=big+pos+1-previous,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s == littleend) {
	    *old_posp = pos;
	    if (!last) return (char *)(big+pos-previous);
	    found = 1;
	}
    } while ( pos += PL_screamnext[pos] );
    return (last && found) ? (char *)(big+(*old_posp)-previous) : Nullch;
#else /* !POINTERRIGOR */
    big -= previous;
    do {
	if (pos >= stop_pos) break;
	if (big[pos] != first)
	    continue;
	for (x=big+pos+1,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s == littleend) {
	    *old_posp = pos;
	    if (!last) return (char *)(big+pos);
	    found = 1;
	}
    } while ( pos += PL_screamnext[pos] );
    if (last && found) 
	return (char *)(big+(*old_posp));
#endif /* POINTERRIGOR */
  check_tail:
    if (!SvTAIL(littlestr) || (end_shift > 0))
	return Nullch;
    /* Ignore the trailing "\n".  This code is not microoptimized */
    big = (unsigned char *)(SvPVX(bigstr) + SvCUR(bigstr));
    stop_pos = littleend - little;	/* Actual littlestr len */
    if (stop_pos == 0)
	return (char*)big;
    big -= stop_pos;
    if (*big == first
	&& ((stop_pos == 1) || memEQ(big + 1, little, stop_pos - 1)))
	return (char*)big;
    return Nullch;
}

I32
Perl_ibcmp(pTHX_ const char *s1, const char *s2, register I32 len)
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    while (len--) {
	if (*a != *b && *a != PL_fold[*b])
	    return 1;
	a++,b++;
    }
    return 0;
}

I32
Perl_ibcmp_locale(pTHX_ const char *s1, const char *s2, register I32 len)
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    while (len--) {
	if (*a != *b && *a != PL_fold_locale[*b])
	    return 1;
	a++,b++;
    }
    return 0;
}

/* copy a string to a safe spot */

/*
=for apidoc savepv

Copy a string to a safe spot.  This does not use an SV.

=cut
*/

char *
Perl_savepv(pTHX_ const char *sv)
{
    register char *newaddr;

    New(902,newaddr,strlen(sv)+1,char);
    (void)strcpy(newaddr,sv);
    return newaddr;
}

/* same thing but with a known length */

/*
=for apidoc savepvn

Copy a string to a safe spot.  The C<len> indicates number of bytes to
copy.  This does not use an SV.

=cut
*/

char *
Perl_savepvn(pTHX_ const char *sv, register I32 len)
{
    register char *newaddr;

    New(903,newaddr,len+1,char);
    Copy(sv,newaddr,len,char);		/* might not be null terminated */
    newaddr[len] = '\0';		/* is now */
    return newaddr;
}

/* the SV for Perl_form() and mess() is not kept in an arena */

STATIC SV *
S_mess_alloc(pTHX)
{
    SV *sv;
    XPVMG *any;

    if (!PL_dirty)
	return sv_2mortal(newSVpvn("",0));

    if (PL_mess_sv)
	return PL_mess_sv;

    /* Create as PVMG now, to avoid any upgrading later */
    New(905, sv, 1, SV);
    Newz(905, any, 1, XPVMG);
    SvFLAGS(sv) = SVt_PVMG;
    SvANY(sv) = (void*)any;
    SvREFCNT(sv) = 1 << 30; /* practically infinite */
    PL_mess_sv = sv;
    return sv;
}

#if defined(PERL_IMPLICIT_CONTEXT)
char *
Perl_form_nocontext(const char* pat, ...)
{
    dTHX;
    char *retval;
    va_list args;
    va_start(args, pat);
    retval = vform(pat, &args);
    va_end(args);
    return retval;
}
#endif /* PERL_IMPLICIT_CONTEXT */

char *
Perl_form(pTHX_ const char* pat, ...)
{
    char *retval;
    va_list args;
    va_start(args, pat);
    retval = vform(pat, &args);
    va_end(args);
    return retval;
}

char *
Perl_vform(pTHX_ const char *pat, va_list *args)
{
    SV *sv = mess_alloc();
    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    return SvPVX(sv);
}

#if defined(PERL_IMPLICIT_CONTEXT)
SV *
Perl_mess_nocontext(const char *pat, ...)
{
    dTHX;
    SV *retval;
    va_list args;
    va_start(args, pat);
    retval = vmess(pat, &args);
    va_end(args);
    return retval;
}
#endif /* PERL_IMPLICIT_CONTEXT */

SV *
Perl_mess(pTHX_ const char *pat, ...)
{
    SV *retval;
    va_list args;
    va_start(args, pat);
    retval = vmess(pat, &args);
    va_end(args);
    return retval;
}

SV *
Perl_vmess(pTHX_ const char *pat, va_list *args)
{
    SV *sv = mess_alloc();
    static char dgd[] = " during global destruction.\n";

    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    if (!SvCUR(sv) || *(SvEND(sv) - 1) != '\n') {
	if (CopLINE(PL_curcop))
	    Perl_sv_catpvf(aTHX_ sv, " at %s line %"IVdf,
			   CopFILE(PL_curcop), (IV)CopLINE(PL_curcop));
	if (GvIO(PL_last_in_gv) && IoLINES(GvIOp(PL_last_in_gv))) {
	    bool line_mode = (RsSIMPLE(PL_rs) &&
			      SvCUR(PL_rs) == 1 && *SvPVX(PL_rs) == '\n');
	    Perl_sv_catpvf(aTHX_ sv, ", <%s> %s %"IVdf,
		      PL_last_in_gv == PL_argvgv ? "" : GvNAME(PL_last_in_gv),
		      line_mode ? "line" : "chunk", 
		      (IV)IoLINES(GvIOp(PL_last_in_gv)));
	}
#ifdef USE_THREADS
	if (thr->tid)
	    Perl_sv_catpvf(aTHX_ sv, " thread %ld", thr->tid);
#endif
	sv_catpv(sv, PL_dirty ? dgd : ".\n");
    }
    return sv;
}

OP *
Perl_vdie(pTHX_ const char* pat, va_list *args)
{
    char *message;
    int was_in_eval = PL_in_eval;
    HV *stash;
    GV *gv;
    CV *cv;
    SV *msv;
    STRLEN msglen;

    DEBUG_S(PerlIO_printf(Perl_debug_log,
			  "%p: die: curstack = %p, mainstack = %p\n",
			  thr, PL_curstack, PL_mainstack));

    if (pat) {
	msv = vmess(pat, args);
	if (PL_errors && SvCUR(PL_errors)) {
	    sv_catsv(PL_errors, msv);
	    message = SvPV(PL_errors, msglen);
	    SvCUR_set(PL_errors, 0);
	}
	else
	    message = SvPV(msv,msglen);
    }
    else {
	message = Nullch;
	msglen = 0;
    }

    DEBUG_S(PerlIO_printf(Perl_debug_log,
			  "%p: die: message = %s\ndiehook = %p\n",
			  thr, message, PL_diehook));
    if (PL_diehook) {
	/* sv_2cv might call Perl_croak() */
	SV *olddiehook = PL_diehook;
	ENTER;
	SAVESPTR(PL_diehook);
	PL_diehook = Nullsv;
	cv = sv_2cv(olddiehook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    save_re_context();
	    if (message) {
		msg = newSVpvn(message, msglen);
		SvREADONLY_on(msg);
		SAVEFREESV(msg);
	    }
	    else {
		msg = ERRSV;
	    }

	    PUSHSTACKi(PERLSI_DIEHOOK);
	    PUSHMARK(SP);
	    XPUSHs(msg);
	    PUTBACK;
	    call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	}
    }

    PL_restartop = die_where(message, msglen);
    DEBUG_S(PerlIO_printf(Perl_debug_log,
	  "%p: die: restartop = %p, was_in_eval = %d, top_env = %p\n",
	  thr, PL_restartop, was_in_eval, PL_top_env));
    if ((!PL_restartop && was_in_eval) || PL_top_env->je_prev)
	JMPENV_JUMP(3);
    return PL_restartop;
}

#if defined(PERL_IMPLICIT_CONTEXT)
OP *
Perl_die_nocontext(const char* pat, ...)
{
    dTHX;
    OP *o;
    va_list args;
    va_start(args, pat);
    o = vdie(pat, &args);
    va_end(args);
    return o;
}
#endif /* PERL_IMPLICIT_CONTEXT */

OP *
Perl_die(pTHX_ const char* pat, ...)
{
    OP *o;
    va_list args;
    va_start(args, pat);
    o = vdie(pat, &args);
    va_end(args);
    return o;
}

void
Perl_vcroak(pTHX_ const char* pat, va_list *args)
{
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;
    SV *msv;
    STRLEN msglen;

    if (pat) {
	msv = vmess(pat, args);
	if (PL_errors && SvCUR(PL_errors)) {
	    sv_catsv(PL_errors, msv);
	    message = SvPV(PL_errors, msglen);
	    SvCUR_set(PL_errors, 0);
	}
	else
	    message = SvPV(msv,msglen);
    }
    else {
	message = Nullch;
	msglen = 0;
    }

    DEBUG_S(PerlIO_printf(Perl_debug_log, "croak: 0x%"UVxf" %s",
			  PTR2UV(thr), message));

    if (PL_diehook) {
	/* sv_2cv might call Perl_croak() */
	SV *olddiehook = PL_diehook;
	ENTER;
	SAVESPTR(PL_diehook);
	PL_diehook = Nullsv;
	cv = sv_2cv(olddiehook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    save_re_context();
	    if (message) {
		msg = newSVpvn(message, msglen);
		SvREADONLY_on(msg);
		SAVEFREESV(msg);
	    }
	    else {
		msg = ERRSV;
	    }

	    PUSHSTACKi(PERLSI_DIEHOOK);
	    PUSHMARK(SP);
	    XPUSHs(msg);
	    PUTBACK;
	    call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	}
    }
    if (PL_in_eval) {
	PL_restartop = die_where(message, msglen);
	JMPENV_JUMP(3);
    }
    {
#ifdef USE_SFIO
	/* SFIO can really mess with your errno */
	int e = errno;
#endif
	PerlIO *serr = Perl_error_log;

	PerlIO_write(serr, message, msglen);
	(void)PerlIO_flush(serr);
#ifdef USE_SFIO
	errno = e;
#endif
    }
    my_failure_exit();
}

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_croak_nocontext(const char *pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    vcroak(pat, &args);
    /* NOTREACHED */
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

/*
=for apidoc croak

This is the XSUB-writer's interface to Perl's C<die> function.
Normally use this function the same way you use the C C<printf>
function.  See C<warn>.

If you want to throw an exception object, assign the object to
C<$@> and then pass C<Nullch> to croak():

   errsv = get_sv("@", TRUE);
   sv_setsv(errsv, exception_object);
   croak(Nullch);

=cut
*/

void
Perl_croak(pTHX_ const char *pat, ...)
{
    va_list args;
    va_start(args, pat);
    vcroak(pat, &args);
    /* NOTREACHED */
    va_end(args);
}

void
Perl_vwarn(pTHX_ const char* pat, va_list *args)
{
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;
    SV *msv;
    STRLEN msglen;

    msv = vmess(pat, args);
    message = SvPV(msv, msglen);

    if (PL_warnhook) {
	/* sv_2cv might call Perl_warn() */
	SV *oldwarnhook = PL_warnhook;
	ENTER;
	SAVESPTR(PL_warnhook);
	PL_warnhook = Nullsv;
	cv = sv_2cv(oldwarnhook, &stash, &gv, 0);
	LEAVE;
	if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
	    dSP;
	    SV *msg;

	    ENTER;
	    save_re_context();
	    msg = newSVpvn(message, msglen);
	    SvREADONLY_on(msg);
	    SAVEFREESV(msg);

	    PUSHSTACKi(PERLSI_WARNHOOK);
	    PUSHMARK(SP);
	    XPUSHs(msg);
	    PUTBACK;
	    call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	    return;
	}
    }
    {
	PerlIO *serr = Perl_error_log;

	PerlIO_write(serr, message, msglen);
#ifdef LEAKTEST
	DEBUG_L(*message == '!' 
		? (xstat(message[1]=='!'
			 ? (message[2]=='!' ? 2 : 1)
			 : 0)
		   , 0)
		: 0);
#endif
	(void)PerlIO_flush(serr);
    }
}

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_warn_nocontext(const char *pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    vwarn(pat, &args);
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

/*
=for apidoc warn

This is the XSUB-writer's interface to Perl's C<warn> function.  Use this
function the same way you use the C C<printf> function.  See
C<croak>.

=cut
*/

void
Perl_warn(pTHX_ const char *pat, ...)
{
    va_list args;
    va_start(args, pat);
    vwarn(pat, &args);
    va_end(args);
}

#if defined(PERL_IMPLICIT_CONTEXT)
void
Perl_warner_nocontext(U32 err, const char *pat, ...)
{
    dTHX;
    va_list args;
    va_start(args, pat);
    vwarner(err, pat, &args);
    va_end(args);
}
#endif /* PERL_IMPLICIT_CONTEXT */

void
Perl_warner(pTHX_ U32  err, const char* pat,...)
{
    va_list args;
    va_start(args, pat);
    vwarner(err, pat, &args);
    va_end(args);
}

void
Perl_vwarner(pTHX_ U32  err, const char* pat, va_list* args)
{
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;
    SV *msv;
    STRLEN msglen;

    msv = vmess(pat, args);
    message = SvPV(msv, msglen);

    if (ckDEAD(err)) {
#ifdef USE_THREADS
        DEBUG_S(PerlIO_printf(Perl_debug_log, "croak: 0x%"UVxf" %s", PTR2UV(thr), message));
#endif /* USE_THREADS */
        if (PL_diehook) {
            /* sv_2cv might call Perl_croak() */
            SV *olddiehook = PL_diehook;
            ENTER;
            SAVESPTR(PL_diehook);
            PL_diehook = Nullsv;
            cv = sv_2cv(olddiehook, &stash, &gv, 0);
            LEAVE;
            if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
                dSP;
                SV *msg;
 
                ENTER;
		save_re_context();
                msg = newSVpvn(message, msglen);
                SvREADONLY_on(msg);
                SAVEFREESV(msg);
 
		PUSHSTACKi(PERLSI_DIEHOOK);
                PUSHMARK(sp);
                XPUSHs(msg);
                PUTBACK;
                call_sv((SV*)cv, G_DISCARD);
		POPSTACK;
                LEAVE;
            }
        }
        if (PL_in_eval) {
            PL_restartop = die_where(message, msglen);
            JMPENV_JUMP(3);
        }
	{
	    PerlIO *serr = Perl_error_log;
	    PerlIO_write(serr, message, msglen);
	    (void)PerlIO_flush(serr);
	}
        my_failure_exit();

    }
    else {
        if (PL_warnhook) {
            /* sv_2cv might call Perl_warn() */
            SV *oldwarnhook = PL_warnhook;
            ENTER;
            SAVESPTR(PL_warnhook);
            PL_warnhook = Nullsv;
            cv = sv_2cv(oldwarnhook, &stash, &gv, 0);
	    LEAVE;
            if (cv && !CvDEPTH(cv) && (CvROOT(cv) || CvXSUB(cv))) {
                dSP;
                SV *msg;
 
                ENTER;
		save_re_context();
                msg = newSVpvn(message, msglen);
                SvREADONLY_on(msg);
                SAVEFREESV(msg);
 
		PUSHSTACKi(PERLSI_WARNHOOK);
                PUSHMARK(sp);
                XPUSHs(msg);
                PUTBACK;
                call_sv((SV*)cv, G_DISCARD);
		POPSTACK;
                LEAVE;
                return;
            }
        }
	{
	    PerlIO *serr = Perl_error_log;
	    PerlIO_write(serr, message, msglen);
#ifdef LEAKTEST
	    DEBUG_L(*message == '!' 
		? (xstat(message[1]=='!'
			 ? (message[2]=='!' ? 2 : 1)
			 : 0)
		   , 0)
		: 0);
#endif
	    (void)PerlIO_flush(serr);
	}
    }
}

#ifdef USE_ENVIRON_ARRAY
       /* VMS' and EPOC's my_setenv() is in vms.c and epoc.c */
#if !defined(WIN32)
void
Perl_my_setenv(pTHX_ char *nam, char *val)
{
#ifndef PERL_USE_SAFE_PUTENV
    /* most putenv()s leak, so we manipulate environ directly */
    register I32 i=setenv_getix(nam);		/* where does it go? */

    if (environ == PL_origenviron) {	/* need we copy environment? */
	I32 j;
	I32 max;
	char **tmpenv;

	/*SUPPRESS 530*/
	for (max = i; environ[max]; max++) ;
	tmpenv = (char**)safesysmalloc((max+2) * sizeof(char*));
	for (j=0; j<max; j++) {		/* copy environment */
	    tmpenv[j] = (char*)safesysmalloc((strlen(environ[j])+1)*sizeof(char));
	    strcpy(tmpenv[j], environ[j]);
	}
	tmpenv[max] = Nullch;
	environ = tmpenv;		/* tell exec where it is now */
    }
    if (!val) {
	safesysfree(environ[i]);
	while (environ[i]) {
	    environ[i] = environ[i+1];
	    i++;
	}
	return;
    }
    if (!environ[i]) {			/* does not exist yet */
	environ = (char**)safesysrealloc(environ, (i+2) * sizeof(char*));
	environ[i+1] = Nullch;	/* make sure it's null terminated */
    }
    else
	safesysfree(environ[i]);
    environ[i] = (char*)safesysmalloc((strlen(nam)+strlen(val)+2) * sizeof(char));

    (void)sprintf(environ[i],"%s=%s",nam,val);/* all that work just for this */

#else   /* PERL_USE_SAFE_PUTENV */
#   if defined(__CYGWIN__)
    setenv(nam, val, 1);
#   else
    char *new_env;

    new_env = (char*)safesysmalloc((strlen(nam) + strlen(val) + 2) * sizeof(char));
    (void)sprintf(new_env,"%s=%s",nam,val);/* all that work just for this */
    (void)putenv(new_env);
#   endif /* __CYGWIN__ */
#endif  /* PERL_USE_SAFE_PUTENV */
}

#else /* WIN32 */

void
Perl_my_setenv(pTHX_ char *nam,char *val)
{
    register char *envstr;
    STRLEN len = strlen(nam) + 3;
    if (!val) {
	val = "";
    }
    len += strlen(val);
    New(904, envstr, len, char);
    (void)sprintf(envstr,"%s=%s",nam,val);
    (void)PerlEnv_putenv(envstr);
    Safefree(envstr);
}

#endif /* WIN32 */

I32
Perl_setenv_getix(pTHX_ char *nam)
{
    register I32 i, len = strlen(nam);

    for (i = 0; environ[i]; i++) {
	if (
#ifdef WIN32
	    strnicmp(environ[i],nam,len) == 0
#else
	    strnEQ(environ[i],nam,len)
#endif
	    && environ[i][len] == '=')
	    break;			/* strnEQ must come first to avoid */
    }					/* potential SEGV's */
    return i;
}

#endif /* !VMS && !EPOC*/

#ifdef UNLINK_ALL_VERSIONS
I32
Perl_unlnk(pTHX_ char *f)	/* unlink all versions of a file */
{
    I32 i;

    for (i = 0; PerlLIO_unlink(f) >= 0; i++) ;
    return i ? 0 : -1;
}
#endif

/* this is a drop-in replacement for bcopy() */
#if !defined(HAS_BCOPY) || !defined(HAS_SAFE_BCOPY)
char *
Perl_my_bcopy(register const char *from,register char *to,register I32 len)
{
    char *retval = to;

    if (from - to >= 0) {
	while (len--)
	    *to++ = *from++;
    }
    else {
	to += len;
	from += len;
	while (len--)
	    *(--to) = *(--from);
    }
    return retval;
}
#endif

/* this is a drop-in replacement for memset() */
#ifndef HAS_MEMSET
void *
Perl_my_memset(register char *loc, register I32 ch, register I32 len)
{
    char *retval = loc;

    while (len--)
	*loc++ = ch;
    return retval;
}
#endif

/* this is a drop-in replacement for bzero() */
#if !defined(HAS_BZERO) && !defined(HAS_MEMSET)
char *
Perl_my_bzero(register char *loc, register I32 len)
{
    char *retval = loc;

    while (len--)
	*loc++ = 0;
    return retval;
}
#endif

/* this is a drop-in replacement for memcmp() */
#if !defined(HAS_MEMCMP) || !defined(HAS_SANE_MEMCMP)
I32
Perl_my_memcmp(const char *s1, const char *s2, register I32 len)
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    register I32 tmp;

    while (len--) {
	if (tmp = *a++ - *b++)
	    return tmp;
    }
    return 0;
}
#endif /* !HAS_MEMCMP || !HAS_SANE_MEMCMP */

#ifndef HAS_VPRINTF

#ifdef USE_CHAR_VSPRINTF
char *
#else
int
#endif
vsprintf(char *dest, const char *pat, char *args)
{
    FILE fakebuf;

    fakebuf._ptr = dest;
    fakebuf._cnt = 32767;
#ifndef _IOSTRG
#define _IOSTRG 0
#endif
    fakebuf._flag = _IOWRT|_IOSTRG;
    _doprnt(pat, args, &fakebuf);	/* what a kludge */
    (void)putc('\0', &fakebuf);
#ifdef USE_CHAR_VSPRINTF
    return(dest);
#else
    return 0;		/* perl doesn't use return value */
#endif
}

#endif /* HAS_VPRINTF */

#ifdef MYSWAP
#if BYTEORDER != 0x4321
short
Perl_my_swap(pTHX_ short s)
{
#if (BYTEORDER & 1) == 0
    short result;

    result = ((s & 255) << 8) + ((s >> 8) & 255);
    return result;
#else
    return s;
#endif
}

long
Perl_my_htonl(pTHX_ long l)
{
    union {
	long result;
	char c[sizeof(long)];
    } u;

#if BYTEORDER == 0x1234
    u.c[0] = (l >> 24) & 255;
    u.c[1] = (l >> 16) & 255;
    u.c[2] = (l >> 8) & 255;
    u.c[3] = l & 255;
    return u.result;
#else
#if ((BYTEORDER - 0x1111) & 0x444) || !(BYTEORDER & 0xf)
    Perl_croak(aTHX_ "Unknown BYTEORDER\n");
#else
    register I32 o;
    register I32 s;

    for (o = BYTEORDER - 0x1111, s = 0; s < (sizeof(long)*8); o >>= 4, s += 8) {
	u.c[o & 0xf] = (l >> s) & 255;
    }
    return u.result;
#endif
#endif
}

long
Perl_my_ntohl(pTHX_ long l)
{
    union {
	long l;
	char c[sizeof(long)];
    } u;

#if BYTEORDER == 0x1234
    u.c[0] = (l >> 24) & 255;
    u.c[1] = (l >> 16) & 255;
    u.c[2] = (l >> 8) & 255;
    u.c[3] = l & 255;
    return u.l;
#else
#if ((BYTEORDER - 0x1111) & 0x444) || !(BYTEORDER & 0xf)
    Perl_croak(aTHX_ "Unknown BYTEORDER\n");
#else
    register I32 o;
    register I32 s;

    u.l = l;
    l = 0;
    for (o = BYTEORDER - 0x1111, s = 0; s < (sizeof(long)*8); o >>= 4, s += 8) {
	l |= (u.c[o & 0xf] & 255) << s;
    }
    return l;
#endif
#endif
}

#endif /* BYTEORDER != 0x4321 */
#endif /* MYSWAP */

/*
 * Little-endian byte order functions - 'v' for 'VAX', or 'reVerse'.
 * If these functions are defined,
 * the BYTEORDER is neither 0x1234 nor 0x4321.
 * However, this is not assumed.
 * -DWS
 */

#define HTOV(name,type)						\
	type							\
	name (register type n)					\
	{							\
	    union {						\
		type value;					\
		char c[sizeof(type)];				\
	    } u;						\
	    register I32 i;					\
	    register I32 s;					\
	    for (i = 0, s = 0; i < sizeof(u.c); i++, s += 8) {	\
		u.c[i] = (n >> s) & 0xFF;			\
	    }							\
	    return u.value;					\
	}

#define VTOH(name,type)						\
	type							\
	name (register type n)					\
	{							\
	    union {						\
		type value;					\
		char c[sizeof(type)];				\
	    } u;						\
	    register I32 i;					\
	    register I32 s;					\
	    u.value = n;					\
	    n = 0;						\
	    for (i = 0, s = 0; i < sizeof(u.c); i++, s += 8) {	\
		n += (u.c[i] & 0xFF) << s;			\
	    }							\
	    return n;						\
	}

#if defined(HAS_HTOVS) && !defined(htovs)
HTOV(htovs,short)
#endif
#if defined(HAS_HTOVL) && !defined(htovl)
HTOV(htovl,long)
#endif
#if defined(HAS_VTOHS) && !defined(vtohs)
VTOH(vtohs,short)
#endif
#if defined(HAS_VTOHL) && !defined(vtohl)
VTOH(vtohl,long)
#endif

    /* VMS' my_popen() is in VMS.c, same with OS/2. */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS) && !defined(__OPEN_VM) && !defined(EPOC) && !defined(MACOS_TRADITIONAL)
PerlIO *
Perl_my_popen(pTHX_ char *cmd, char *mode)
{
    int p[2];
    register I32 This, that;
    register Pid_t pid;
    SV *sv;
    I32 doexec = strNE(cmd,"-");
    I32 did_pipes = 0;
    int pp[2];

    PERL_FLUSHALL_FOR_CHILD;
#ifdef OS2
    if (doexec) {
	return my_syspopen(aTHX_ cmd,mode);
    }
#endif 
    This = (*mode == 'w');
    that = !This;
    if (doexec && PL_tainting) {
	taint_env();
	taint_proper("Insecure %s%s", "EXEC");
    }
    if (PerlProc_pipe(p) < 0)
	return Nullfp;
    if (doexec && PerlProc_pipe(pp) >= 0)
	did_pipes = 1;
    while ((pid = (doexec?vfork():fork())) < 0) {
	if (errno != EAGAIN) {
	    PerlLIO_close(p[This]);
	    if (did_pipes) {
		PerlLIO_close(pp[0]);
		PerlLIO_close(pp[1]);
	    }
	    if (!doexec)
		Perl_croak(aTHX_ "Can't fork");
	    return Nullfp;
	}
	sleep(5);
    }
    if (pid == 0) {
	GV* tmpgv;

#undef THIS
#undef THAT
#define THIS that
#define THAT This
	PerlLIO_close(p[THAT]);
	if (did_pipes) {
	    PerlLIO_close(pp[0]);
#if defined(HAS_FCNTL) && defined(F_SETFD)
	    fcntl(pp[1], F_SETFD, FD_CLOEXEC);
#endif
	}
	if (p[THIS] != (*mode == 'r')) {
	    PerlLIO_dup2(p[THIS], *mode == 'r');
	    PerlLIO_close(p[THIS]);
	}
#ifndef OS2
	if (doexec) {
#if !defined(HAS_FCNTL) || !defined(F_SETFD)
	    int fd;

#ifndef NOFILE
#define NOFILE 20
#endif
	    for (fd = PL_maxsysfd + 1; fd < NOFILE; fd++)
		if (fd != pp[1])
		    PerlLIO_close(fd);
#endif
	    do_exec3(cmd,pp[1],did_pipes);	/* may or may not use the shell */
	    PerlProc__exit(1);
	}
#endif	/* defined OS2 */
	/*SUPPRESS 560*/
	if ((tmpgv = gv_fetchpv("$",TRUE, SVt_PV)))
	    sv_setiv(GvSV(tmpgv), PerlProc_getpid());
	PL_forkprocess = 0;
	hv_clear(PL_pidstatus);	/* we have no children */
	return Nullfp;
#undef THIS
#undef THAT
    }
    do_execfree();	/* free any memory malloced by child on vfork */
    PerlLIO_close(p[that]);
    if (did_pipes)
	PerlLIO_close(pp[1]);
    if (p[that] < p[This]) {
	PerlLIO_dup2(p[This], p[that]);
	PerlLIO_close(p[This]);
	p[This] = p[that];
    }
    LOCK_FDPID_MUTEX;
    sv = *av_fetch(PL_fdpid,p[This],TRUE);
    UNLOCK_FDPID_MUTEX;
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = pid;
    PL_forkprocess = pid;
    if (did_pipes && pid > 0) {
	int errkid;
	int n = 0, n1;

	while (n < sizeof(int)) {
	    n1 = PerlLIO_read(pp[0],
			      (void*)(((char*)&errkid)+n),
			      (sizeof(int)) - n);
	    if (n1 <= 0)
		break;
	    n += n1;
	}
	PerlLIO_close(pp[0]);
	did_pipes = 0;
	if (n) {			/* Error */
	    if (n != sizeof(int))
		Perl_croak(aTHX_ "panic: kid popen errno read");
	    errno = errkid;		/* Propagate errno from kid */
	    return Nullfp;
	}
    }
    if (did_pipes)
	 PerlLIO_close(pp[0]);
    return PerlIO_fdopen(p[This], mode);
}
#else
#if defined(atarist) || defined(DJGPP)
FILE *popen();
PerlIO *
Perl_my_popen(pTHX_ char *cmd, char *mode)
{
    /* Needs work for PerlIO ! */
    /* used 0 for 2nd parameter to PerlIO-exportFILE; apparently not used */
    PERL_FLUSHALL_FOR_CHILD;
    return popen(PerlIO_exportFILE(cmd, 0), mode);
}
#endif

#endif /* !DOSISH */

#ifdef DUMP_FDS
void
Perl_dump_fds(pTHX_ char *s)
{
    int fd;
    struct stat tmpstatbuf;

    PerlIO_printf(Perl_debug_log,"%s", s);
    for (fd = 0; fd < 32; fd++) {
	if (PerlLIO_fstat(fd,&tmpstatbuf) >= 0)
	    PerlIO_printf(Perl_debug_log," %d",fd);
    }
    PerlIO_printf(Perl_debug_log,"\n");
}
#endif	/* DUMP_FDS */

#ifndef HAS_DUP2
int
dup2(int oldfd, int newfd)
{
#if defined(HAS_FCNTL) && defined(F_DUPFD)
    if (oldfd == newfd)
	return oldfd;
    PerlLIO_close(newfd);
    return fcntl(oldfd, F_DUPFD, newfd);
#else
#define DUP2_MAX_FDS 256
    int fdtmp[DUP2_MAX_FDS];
    I32 fdx = 0;
    int fd;

    if (oldfd == newfd)
	return oldfd;
    PerlLIO_close(newfd);
    /* good enough for low fd's... */
    while ((fd = PerlLIO_dup(oldfd)) != newfd && fd >= 0) {
	if (fdx >= DUP2_MAX_FDS) {
	    PerlLIO_close(fd);
	    fd = -1;
	    break;
	}
	fdtmp[fdx++] = fd;
    }
    while (fdx > 0)
	PerlLIO_close(fdtmp[--fdx]);
    return fd;
#endif
}
#endif


#ifdef HAS_SIGACTION

Sighandler_t
Perl_rsignal(pTHX_ int signo, Sighandler_t handler)
{
    struct sigaction act, oact;

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
#ifdef SA_NOCLDWAIT
    if (signo == SIGCHLD && handler == (Sighandler_t)SIG_IGN)
	act.sa_flags |= SA_NOCLDWAIT;
#endif
    if (sigaction(signo, &act, &oact) == -1)
    	return SIG_ERR;
    else
    	return oact.sa_handler;
}

Sighandler_t
Perl_rsignal_state(pTHX_ int signo)
{
    struct sigaction oact;

    if (sigaction(signo, (struct sigaction *)NULL, &oact) == -1)
        return SIG_ERR;
    else
        return oact.sa_handler;
}

int
Perl_rsignal_save(pTHX_ int signo, Sighandler_t handler, Sigsave_t *save)
{
    struct sigaction act;

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
#ifdef SA_NOCLDWAIT
    if (signo == SIGCHLD && handler == (Sighandler_t)SIG_IGN)
	act.sa_flags |= SA_NOCLDWAIT;
#endif
    return sigaction(signo, &act, save);
}

int
Perl_rsignal_restore(pTHX_ int signo, Sigsave_t *save)
{
    return sigaction(signo, save, (struct sigaction *)NULL);
}

#else /* !HAS_SIGACTION */

Sighandler_t
Perl_rsignal(pTHX_ int signo, Sighandler_t handler)
{
    return PerlProc_signal(signo, handler);
}

static int sig_trapped;

static
Signal_t
sig_trap(int signo)
{
    sig_trapped++;
}

Sighandler_t
Perl_rsignal_state(pTHX_ int signo)
{
    Sighandler_t oldsig;

    sig_trapped = 0;
    oldsig = PerlProc_signal(signo, sig_trap);
    PerlProc_signal(signo, oldsig);
    if (sig_trapped)
        PerlProc_kill(PerlProc_getpid(), signo);
    return oldsig;
}

int
Perl_rsignal_save(pTHX_ int signo, Sighandler_t handler, Sigsave_t *save)
{
    *save = PerlProc_signal(signo, handler);
    return (*save == SIG_ERR) ? -1 : 0;
}

int
Perl_rsignal_restore(pTHX_ int signo, Sigsave_t *save)
{
    return (PerlProc_signal(signo, *save) == SIG_ERR) ? -1 : 0;
}

#endif /* !HAS_SIGACTION */

    /* VMS' my_pclose() is in VMS.c; same with OS/2 */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS) && !defined(__OPEN_VM) && !defined(EPOC) && !defined(MACOS_TRADITIONAL)
I32
Perl_my_pclose(pTHX_ PerlIO *ptr)
{
    Sigsave_t hstat, istat, qstat;
    int status;
    SV **svp;
    Pid_t pid;
    Pid_t pid2;
    bool close_failed;
    int saved_errno;
#ifdef VMS
    int saved_vaxc_errno;
#endif
#ifdef WIN32
    int saved_win32_errno;
#endif

    LOCK_FDPID_MUTEX;
    svp = av_fetch(PL_fdpid,PerlIO_fileno(ptr),TRUE);
    UNLOCK_FDPID_MUTEX;
    pid = SvIVX(*svp);
    SvREFCNT_dec(*svp);
    *svp = &PL_sv_undef;
#ifdef OS2
    if (pid == -1) {			/* Opened by popen. */
	return my_syspclose(ptr);
    }
#endif 
    if ((close_failed = (PerlIO_close(ptr) == EOF))) {
	saved_errno = errno;
#ifdef VMS
	saved_vaxc_errno = vaxc$errno;
#endif
#ifdef WIN32
	saved_win32_errno = GetLastError();
#endif
    }
#ifdef UTS
    if(PerlProc_kill(pid, 0) < 0) { return(pid); }   /* HOM 12/23/91 */
#endif
    rsignal_save(SIGHUP, SIG_IGN, &hstat);
    rsignal_save(SIGINT, SIG_IGN, &istat);
    rsignal_save(SIGQUIT, SIG_IGN, &qstat);
    do {
	pid2 = wait4pid(pid, &status, 0);
    } while (pid2 == -1 && errno == EINTR);
    rsignal_restore(SIGHUP, &hstat);
    rsignal_restore(SIGINT, &istat);
    rsignal_restore(SIGQUIT, &qstat);
    if (close_failed) {
	SETERRNO(saved_errno, saved_vaxc_errno);
	return -1;
    }
    return(pid2 < 0 ? pid2 : status == 0 ? 0 : (errno = 0, status));
}
#endif /* !DOSISH */

#if  (!defined(DOSISH) || defined(OS2) || defined(WIN32)) && !defined(MACOS_TRADITIONAL)
I32
Perl_wait4pid(pTHX_ Pid_t pid, int *statusp, int flags)
{
    SV *sv;
    SV** svp;
    char spid[TYPE_CHARS(int)];

    if (!pid)
	return -1;
#if !defined(HAS_WAITPID) && !defined(HAS_WAIT4) || defined(HAS_WAITPID_RUNTIME)
    if (pid > 0) {
	sprintf(spid, "%"IVdf, (IV)pid);
	svp = hv_fetch(PL_pidstatus,spid,strlen(spid),FALSE);
	if (svp && *svp != &PL_sv_undef) {
	    *statusp = SvIVX(*svp);
	    (void)hv_delete(PL_pidstatus,spid,strlen(spid),G_DISCARD);
	    return pid;
	}
    }
    else {
	HE *entry;

	hv_iterinit(PL_pidstatus);
	if ((entry = hv_iternext(PL_pidstatus))) {
	    pid = atoi(hv_iterkey(entry,(I32*)statusp));
	    sv = hv_iterval(PL_pidstatus,entry);
	    *statusp = SvIVX(sv);
	    sprintf(spid, "%"IVdf, (IV)pid);
	    (void)hv_delete(PL_pidstatus,spid,strlen(spid),G_DISCARD);
	    return pid;
	}
    }
#endif
#ifdef HAS_WAITPID
#  ifdef HAS_WAITPID_RUNTIME
    if (!HAS_WAITPID_RUNTIME)
	goto hard_way;
#  endif
    return PerlProc_waitpid(pid,statusp,flags);
#endif
#if !defined(HAS_WAITPID) && defined(HAS_WAIT4)
    return wait4((pid==-1)?0:pid,statusp,flags,Null(struct rusage *));
#endif
#if !defined(HAS_WAITPID) && !defined(HAS_WAIT4) || defined(HAS_WAITPID_RUNTIME)
  hard_way:
    {
	I32 result;
	if (flags)
	    Perl_croak(aTHX_ "Can't do waitpid with flags");
	else {
	    while ((result = PerlProc_wait(statusp)) != pid && pid > 0 && result >= 0)
		pidgone(result,*statusp);
	    if (result < 0)
		*statusp = -1;
	}
	return result;
    }
#endif
}
#endif /* !DOSISH || OS2 || WIN32 */

void
/*SUPPRESS 590*/
Perl_pidgone(pTHX_ Pid_t pid, int status)
{
    register SV *sv;
    char spid[TYPE_CHARS(int)];

    sprintf(spid, "%"IVdf, (IV)pid);
    sv = *hv_fetch(PL_pidstatus,spid,strlen(spid),TRUE);
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = status;
    return;
}

#if defined(atarist) || defined(OS2) || defined(DJGPP)
int pclose();
#ifdef HAS_FORK
int					/* Cannot prototype with I32
					   in os2ish.h. */
my_syspclose(PerlIO *ptr)
#else
I32
Perl_my_pclose(pTHX_ PerlIO *ptr)
#endif 
{
    /* Needs work for PerlIO ! */
    FILE *f = PerlIO_findFILE(ptr);
    I32 result = pclose(f);
#if defined(DJGPP)
    result = (result << 8) & 0xff00;
#endif
    PerlIO_releaseFILE(ptr,f);
    return result;
}
#endif

void
Perl_repeatcpy(pTHX_ register char *to, register const char *from, I32 len, register I32 count)
{
    register I32 todo;
    register const char *frombase = from;

    if (len == 1) {
	register const char c = *from;
	while (count-- > 0)
	    *to++ = c;
	return;
    }
    while (count-- > 0) {
	for (todo = len; todo > 0; todo--) {
	    *to++ = *from++;
	}
	from = frombase;
    }
}

U32
Perl_cast_ulong(pTHX_ NV f)
{
    long along;

#if CASTFLAGS & 2
#   define BIGDOUBLE 2147483648.0
    if (f >= BIGDOUBLE)
	return (unsigned long)(f-(long)(f/BIGDOUBLE)*BIGDOUBLE)|0x80000000;
#endif
    if (f >= 0.0)
	return (unsigned long)f;
    along = (long)f;
    return (unsigned long)along;
}
# undef BIGDOUBLE

/* Unfortunately, on some systems the cast_uv() function doesn't
   work with the system-supplied definition of ULONG_MAX.  The
   comparison  (f >= ULONG_MAX) always comes out true.  It must be a
   problem with the compiler constant folding.

   In any case, this workaround should be fine on any two's complement
   system.  If it's not, supply a '-DMY_ULONG_MAX=whatever' in your
   ccflags.
	       --Andy Dougherty      <doughera@lafcol.lafayette.edu>
*/

/* Code modified to prefer proper named type ranges, I32, IV, or UV, instead
   of LONG_(MIN/MAX).
                           -- Kenneth Albanowski <kjahds@kjahds.com>
*/                                      

#ifndef MY_UV_MAX
#  define MY_UV_MAX ((UV)IV_MAX * (UV)2 + (UV)1)
#endif

I32
Perl_cast_i32(pTHX_ NV f)
{
    if (f >= I32_MAX)
	return (I32) I32_MAX;
    if (f <= I32_MIN)
	return (I32) I32_MIN;
    return (I32) f;
}

IV
Perl_cast_iv(pTHX_ NV f)
{
    if (f >= IV_MAX) {
	UV uv;
	
	if (f >= (NV)UV_MAX)
	    return (IV) UV_MAX;	
	uv = (UV) f;
	return (IV)uv;
    }
    if (f <= IV_MIN)
	return (IV) IV_MIN;
    return (IV) f;
}

UV
Perl_cast_uv(pTHX_ NV f)
{
    if (f >= MY_UV_MAX)
	return (UV) MY_UV_MAX;
    if (f < 0) {
	IV iv;
	
	if (f < IV_MIN)
	    return (UV)IV_MIN;
	iv = (IV) f;
	return (UV) iv;
    }
    return (UV) f;
}

#ifndef HAS_RENAME
I32
Perl_same_dirent(pTHX_ char *a, char *b)
{
    char *fa = strrchr(a,'/');
    char *fb = strrchr(b,'/');
    struct stat tmpstatbuf1;
    struct stat tmpstatbuf2;
    SV *tmpsv = sv_newmortal();

    if (fa)
	fa++;
    else
	fa = a;
    if (fb)
	fb++;
    else
	fb = b;
    if (strNE(a,b))
	return FALSE;
    if (fa == a)
	sv_setpv(tmpsv, ".");
    else
	sv_setpvn(tmpsv, a, fa - a);
    if (PerlLIO_stat(SvPVX(tmpsv), &tmpstatbuf1) < 0)
	return FALSE;
    if (fb == b)
	sv_setpv(tmpsv, ".");
    else
	sv_setpvn(tmpsv, b, fb - b);
    if (PerlLIO_stat(SvPVX(tmpsv), &tmpstatbuf2) < 0)
	return FALSE;
    return tmpstatbuf1.st_dev == tmpstatbuf2.st_dev &&
	   tmpstatbuf1.st_ino == tmpstatbuf2.st_ino;
}
#endif /* !HAS_RENAME */

NV
Perl_scan_bin(pTHX_ char *start, STRLEN len, STRLEN *retlen)
{
    register char *s = start;
    register NV rnv = 0.0;
    register UV ruv = 0;
    register bool seenb = FALSE;
    register bool overflowed = FALSE;

    for (; len-- && *s; s++) {
	if (!(*s == '0' || *s == '1')) {
	    if (*s == '_' && len && *retlen
		&& (s[1] == '0' || s[1] == '1'))
	    {
		--len;
		++s;
	    }
	    else if (seenb == FALSE && *s == 'b' && ruv == 0) {
		/* Disallow 0bbb0b0bbb... */
		seenb = TRUE;
		continue;
	    }
	    else {
		if (ckWARN(WARN_DIGIT))
		    Perl_warner(aTHX_ WARN_DIGIT,
				"Illegal binary digit '%c' ignored", *s);
		break;
	    }
	}
	if (!overflowed) {
	    register UV xuv = ruv << 1;

	    if ((xuv >> 1) != ruv) {
		overflowed = TRUE;
		rnv = (NV) ruv;
		if (ckWARN_d(WARN_OVERFLOW))
		    Perl_warner(aTHX_ WARN_OVERFLOW,
				"Integer overflow in binary number");
	    }
	    else
		ruv = xuv | (*s - '0');
	}
	if (overflowed) {
	    rnv *= 2;
	    /* If an NV has not enough bits in its mantissa to
	     * represent an UV this summing of small low-order numbers
	     * is a waste of time (because the NV cannot preserve
	     * the low-order bits anyway): we could just remember when
	     * did we overflow and in the end just multiply rnv by the
	     * right amount. */
	    rnv += (*s - '0');
	}
    }
    if (!overflowed)
	rnv = (NV) ruv;
    if (   ( overflowed && rnv > 4294967295.0)
#if UVSIZE > 4
	|| (!overflowed && ruv > 0xffffffff  )
#endif
	) { 
	if (ckWARN(WARN_PORTABLE))
	    Perl_warner(aTHX_ WARN_PORTABLE,
			"Binary number > 0b11111111111111111111111111111111 non-portable");
    }
    *retlen = s - start;
    return rnv;
}

NV
Perl_scan_oct(pTHX_ char *start, STRLEN len, STRLEN *retlen)
{
    register char *s = start;
    register NV rnv = 0.0;
    register UV ruv = 0;
    register bool overflowed = FALSE;

    for (; len-- && *s; s++) {
	if (!(*s >= '0' && *s <= '7')) {
	    if (*s == '_' && len && *retlen
		&& (s[1] >= '0' && s[1] <= '7'))
	    {
		--len;
		++s;
	    }
	    else {
		/* Allow \octal to work the DWIM way (that is, stop scanning
		 * as soon as non-octal characters are seen, complain only iff
		 * someone seems to want to use the digits eight and nine). */
		if (*s == '8' || *s == '9') {
		    if (ckWARN(WARN_DIGIT))
			Perl_warner(aTHX_ WARN_DIGIT,
				    "Illegal octal digit '%c' ignored", *s);
		}
		break;
	    }
	}
	if (!overflowed) {
	    register UV xuv = ruv << 3;

	    if ((xuv >> 3) != ruv) {
		overflowed = TRUE;
		rnv = (NV) ruv;
		if (ckWARN_d(WARN_OVERFLOW))
		    Perl_warner(aTHX_ WARN_OVERFLOW,
				"Integer overflow in octal number");
	    }
	    else
		ruv = xuv | (*s - '0');
	}
	if (overflowed) {
	    rnv *= 8.0;
	    /* If an NV has not enough bits in its mantissa to
	     * represent an UV this summing of small low-order numbers
	     * is a waste of time (because the NV cannot preserve
	     * the low-order bits anyway): we could just remember when
	     * did we overflow and in the end just multiply rnv by the
	     * right amount of 8-tuples. */
	    rnv += (NV)(*s - '0');
	}
    }
    if (!overflowed)
	rnv = (NV) ruv;
    if (   ( overflowed && rnv > 4294967295.0)
#if UVSIZE > 4
	|| (!overflowed && ruv > 0xffffffff  )
#endif
	) {
	if (ckWARN(WARN_PORTABLE))
	    Perl_warner(aTHX_ WARN_PORTABLE,
			"Octal number > 037777777777 non-portable");
    }
    *retlen = s - start;
    return rnv;
}

NV
Perl_scan_hex(pTHX_ char *start, STRLEN len, STRLEN *retlen)
{
    register char *s = start;
    register NV rnv = 0.0;
    register UV ruv = 0;
    register bool seenx = FALSE;
    register bool overflowed = FALSE;
    char *hexdigit;

    for (; len-- && *s; s++) {
	hexdigit = strchr((char *) PL_hexdigit, *s);
	if (!hexdigit) {
	    if (*s == '_' && len && *retlen && s[1]
		&& (hexdigit = strchr((char *) PL_hexdigit, s[1])))
	    {
		--len;
		++s;
	    }
	    else if (seenx == FALSE && *s == 'x' && ruv == 0) {
		/* Disallow 0xxx0x0xxx... */
		seenx = TRUE;
		continue;
	    }
	    else {
		if (ckWARN(WARN_DIGIT))
		    Perl_warner(aTHX_ WARN_DIGIT,
				"Illegal hexadecimal digit '%c' ignored", *s);
		break;
	    }
	}
	if (!overflowed) {
	    register UV xuv = ruv << 4;

	    if ((xuv >> 4) != ruv) {
		overflowed = TRUE;
		rnv = (NV) ruv;
		if (ckWARN_d(WARN_OVERFLOW))
		    Perl_warner(aTHX_ WARN_OVERFLOW,
				"Integer overflow in hexadecimal number");
	    }
	    else
		ruv = xuv | ((hexdigit - PL_hexdigit) & 15);
	}
	if (overflowed) {
	    rnv *= 16.0;
	    /* If an NV has not enough bits in its mantissa to
	     * represent an UV this summing of small low-order numbers
	     * is a waste of time (because the NV cannot preserve
	     * the low-order bits anyway): we could just remember when
	     * did we overflow and in the end just multiply rnv by the
	     * right amount of 16-tuples. */
	    rnv += (NV)((hexdigit - PL_hexdigit) & 15);
	}
    }
    if (!overflowed)
	rnv = (NV) ruv;
    if (   ( overflowed && rnv > 4294967295.0)
#if UVSIZE > 4
	|| (!overflowed && ruv > 0xffffffff  )
#endif
	) { 
	if (ckWARN(WARN_PORTABLE))
	    Perl_warner(aTHX_ WARN_PORTABLE,
			"Hexadecimal number > 0xffffffff non-portable");
    }
    *retlen = s - start;
    return rnv;
}

char*
Perl_find_script(pTHX_ char *scriptname, bool dosearch, char **search_ext, I32 flags)
{
    char *xfound = Nullch;
    char *xfailed = Nullch;
    char tmpbuf[MAXPATHLEN];
    register char *s;
    I32 len;
    int retval;
#if defined(DOSISH) && !defined(OS2) && !defined(atarist)
#  define SEARCH_EXTS ".bat", ".cmd", NULL
#  define MAX_EXT_LEN 4
#endif
#ifdef OS2
#  define SEARCH_EXTS ".cmd", ".btm", ".bat", ".pl", NULL
#  define MAX_EXT_LEN 4
#endif
#ifdef VMS
#  define SEARCH_EXTS ".pl", ".com", NULL
#  define MAX_EXT_LEN 4
#endif
    /* additional extensions to try in each dir if scriptname not found */
#ifdef SEARCH_EXTS
    char *exts[] = { SEARCH_EXTS };
    char **ext = search_ext ? search_ext : exts;
    int extidx = 0, i = 0;
    char *curext = Nullch;
#else
#  define MAX_EXT_LEN 0
#endif

    /*
     * If dosearch is true and if scriptname does not contain path
     * delimiters, search the PATH for scriptname.
     *
     * If SEARCH_EXTS is also defined, will look for each
     * scriptname{SEARCH_EXTS} whenever scriptname is not found
     * while searching the PATH.
     *
     * Assuming SEARCH_EXTS is C<".foo",".bar",NULL>, PATH search
     * proceeds as follows:
     *   If DOSISH or VMSISH:
     *     + look for ./scriptname{,.foo,.bar}
     *     + search the PATH for scriptname{,.foo,.bar}
     *
     *   If !DOSISH:
     *     + look *only* in the PATH for scriptname{,.foo,.bar} (note
     *       this will not look in '.' if it's not in the PATH)
     */
    tmpbuf[0] = '\0';

#ifdef VMS
#  ifdef ALWAYS_DEFTYPES
    len = strlen(scriptname);
    if (!(len == 1 && *scriptname == '-') && scriptname[len-1] != ':') {
	int hasdir, idx = 0, deftypes = 1;
	bool seen_dot = 1;

	hasdir = !dosearch || (strpbrk(scriptname,":[</") != Nullch) ;
#  else
    if (dosearch) {
	int hasdir, idx = 0, deftypes = 1;
	bool seen_dot = 1;

	hasdir = (strpbrk(scriptname,":[</") != Nullch) ;
#  endif
	/* The first time through, just add SEARCH_EXTS to whatever we
	 * already have, so we can check for default file types. */
	while (deftypes ||
	       (!hasdir && my_trnlnm("DCL$PATH",tmpbuf,idx++)) )
	{
	    if (deftypes) {
		deftypes = 0;
		*tmpbuf = '\0';
	    }
	    if ((strlen(tmpbuf) + strlen(scriptname)
		 + MAX_EXT_LEN) >= sizeof tmpbuf)
		continue;	/* don't search dir with too-long name */
	    strcat(tmpbuf, scriptname);
#else  /* !VMS */

#ifdef DOSISH
    if (strEQ(scriptname, "-"))
 	dosearch = 0;
    if (dosearch) {		/* Look in '.' first. */
	char *cur = scriptname;
#ifdef SEARCH_EXTS
	if ((curext = strrchr(scriptname,'.')))	/* possible current ext */
	    while (ext[i])
		if (strEQ(ext[i++],curext)) {
		    extidx = -1;		/* already has an ext */
		    break;
		}
	do {
#endif
	    DEBUG_p(PerlIO_printf(Perl_debug_log,
				  "Looking for %s\n",cur));
	    if (PerlLIO_stat(cur,&PL_statbuf) >= 0
		&& !S_ISDIR(PL_statbuf.st_mode)) {
		dosearch = 0;
		scriptname = cur;
#ifdef SEARCH_EXTS
		break;
#endif
	    }
#ifdef SEARCH_EXTS
	    if (cur == scriptname) {
		len = strlen(scriptname);
		if (len+MAX_EXT_LEN+1 >= sizeof(tmpbuf))
		    break;
		cur = strcpy(tmpbuf, scriptname);
	    }
	} while (extidx >= 0 && ext[extidx]	/* try an extension? */
		 && strcpy(tmpbuf+len, ext[extidx++]));
#endif
    }
#endif

#ifdef MACOS_TRADITIONAL
    if (dosearch && !strchr(scriptname, ':') &&
	(s = PerlEnv_getenv("Commands")))
#else
    if (dosearch && !strchr(scriptname, '/')
#ifdef DOSISH
		 && !strchr(scriptname, '\\')
#endif
		 && (s = PerlEnv_getenv("PATH")))
#endif
    {
	bool seen_dot = 0;
	
	PL_bufend = s + strlen(s);
	while (s < PL_bufend) {
#ifdef MACOS_TRADITIONAL
	    s = delimcpy(tmpbuf, tmpbuf + sizeof tmpbuf, s, PL_bufend,
			',',
			&len);
#else
#if defined(atarist) || defined(DOSISH)
	    for (len = 0; *s
#  ifdef atarist
		    && *s != ','
#  endif
		    && *s != ';'; len++, s++) {
		if (len < sizeof tmpbuf)
		    tmpbuf[len] = *s;
	    }
	    if (len < sizeof tmpbuf)
		tmpbuf[len] = '\0';
#else  /* ! (atarist || DOSISH) */
	    s = delimcpy(tmpbuf, tmpbuf + sizeof tmpbuf, s, PL_bufend,
			':',
			&len);
#endif /* ! (atarist || DOSISH) */
#endif /* MACOS_TRADITIONAL */
	    if (s < PL_bufend)
		s++;
	    if (len + 1 + strlen(scriptname) + MAX_EXT_LEN >= sizeof tmpbuf)
		continue;	/* don't search dir with too-long name */
#ifdef MACOS_TRADITIONAL
	    if (len && tmpbuf[len - 1] != ':')
	    	tmpbuf[len++] = ':';
#else
	    if (len
#if defined(atarist) || defined(__MINT__) || defined(DOSISH)
		&& tmpbuf[len - 1] != '/'
		&& tmpbuf[len - 1] != '\\'
#endif
	       )
		tmpbuf[len++] = '/';
	    if (len == 2 && tmpbuf[0] == '.')
		seen_dot = 1;
#endif
	    (void)strcpy(tmpbuf + len, scriptname);
#endif  /* !VMS */

#ifdef SEARCH_EXTS
	    len = strlen(tmpbuf);
	    if (extidx > 0)	/* reset after previous loop */
		extidx = 0;
	    do {
#endif
	    	DEBUG_p(PerlIO_printf(Perl_debug_log, "Looking for %s\n",tmpbuf));
		retval = PerlLIO_stat(tmpbuf,&PL_statbuf);
		if (S_ISDIR(PL_statbuf.st_mode)) {
		    retval = -1;
		}
#ifdef SEARCH_EXTS
	    } while (  retval < 0		/* not there */
		    && extidx>=0 && ext[extidx]	/* try an extension? */
		    && strcpy(tmpbuf+len, ext[extidx++])
		);
#endif
	    if (retval < 0)
		continue;
	    if (S_ISREG(PL_statbuf.st_mode)
		&& cando(S_IRUSR,TRUE,&PL_statbuf)
#if !defined(DOSISH) && !defined(MACOS_TRADITIONAL)
		&& cando(S_IXUSR,TRUE,&PL_statbuf)
#endif
		)
	    {
		xfound = tmpbuf;              /* bingo! */
		break;
	    }
	    if (!xfailed)
		xfailed = savepv(tmpbuf);
	}
#ifndef DOSISH
	if (!xfound && !seen_dot && !xfailed &&
	    (PerlLIO_stat(scriptname,&PL_statbuf) < 0 
	     || S_ISDIR(PL_statbuf.st_mode)))
#endif
	    seen_dot = 1;			/* Disable message. */
	if (!xfound) {
	    if (flags & 1) {			/* do or die? */
	        Perl_croak(aTHX_ "Can't %s %s%s%s",
		      (xfailed ? "execute" : "find"),
		      (xfailed ? xfailed : scriptname),
		      (xfailed ? "" : " on PATH"),
		      (xfailed || seen_dot) ? "" : ", '.' not in PATH");
	    }
	    scriptname = Nullch;
	}
	if (xfailed)
	    Safefree(xfailed);
	scriptname = xfound;
    }
    return (scriptname ? savepv(scriptname) : Nullch);
}

#ifndef PERL_GET_CONTEXT_DEFINED

void *
Perl_get_context(void)
{
#if defined(USE_THREADS) || defined(USE_ITHREADS)
#  ifdef OLD_PTHREADS_API
    pthread_addr_t t;
    if (pthread_getspecific(PL_thr_key, &t))
	Perl_croak_nocontext("panic: pthread_getspecific");
    return (void*)t;
#  else
#  ifdef I_MACH_CTHREADS
    return (void*)cthread_data(cthread_self());
#  else
    return (void*)pthread_getspecific(PL_thr_key);
#  endif
#  endif
#else
    return (void*)NULL;
#endif
}

void
Perl_set_context(void *t)
{
#if defined(USE_THREADS) || defined(USE_ITHREADS)
#  ifdef I_MACH_CTHREADS
    cthread_set_data(cthread_self(), t);
#  else
    if (pthread_setspecific(PL_thr_key, t))
	Perl_croak_nocontext("panic: pthread_setspecific");
#  endif
#endif
}

#endif /* !PERL_GET_CONTEXT_DEFINED */

#ifdef USE_THREADS

#ifdef FAKE_THREADS
/* Very simplistic scheduler for now */
void
schedule(void)
{
    thr = thr->i.next_run;
}

void
Perl_cond_init(pTHX_ perl_cond *cp)
{
    *cp = 0;
}

void
Perl_cond_signal(pTHX_ perl_cond *cp)
{
    perl_os_thread t;
    perl_cond cond = *cp;
    
    if (!cond)
	return;
    t = cond->thread;
    /* Insert t in the runnable queue just ahead of us */
    t->i.next_run = thr->i.next_run;
    thr->i.next_run->i.prev_run = t;
    t->i.prev_run = thr;
    thr->i.next_run = t;
    thr->i.wait_queue = 0;
    /* Remove from the wait queue */
    *cp = cond->next;
    Safefree(cond);
}

void
Perl_cond_broadcast(pTHX_ perl_cond *cp)
{
    perl_os_thread t;
    perl_cond cond, cond_next;
    
    for (cond = *cp; cond; cond = cond_next) {
	t = cond->thread;
	/* Insert t in the runnable queue just ahead of us */
	t->i.next_run = thr->i.next_run;
	thr->i.next_run->i.prev_run = t;
	t->i.prev_run = thr;
	thr->i.next_run = t;
	thr->i.wait_queue = 0;
	/* Remove from the wait queue */
	cond_next = cond->next;
	Safefree(cond);
    }
    *cp = 0;
}

void
Perl_cond_wait(pTHX_ perl_cond *cp)
{
    perl_cond cond;

    if (thr->i.next_run == thr)
	Perl_croak(aTHX_ "panic: perl_cond_wait called by last runnable thread");
    
    New(666, cond, 1, struct perl_wait_queue);
    cond->thread = thr;
    cond->next = *cp;
    *cp = cond;
    thr->i.wait_queue = cond;
    /* Remove ourselves from runnable queue */
    thr->i.next_run->i.prev_run = thr->i.prev_run;
    thr->i.prev_run->i.next_run = thr->i.next_run;
}
#endif /* FAKE_THREADS */

MAGIC *
Perl_condpair_magic(pTHX_ SV *sv)
{
    MAGIC *mg;
    
    SvUPGRADE(sv, SVt_PVMG);
    mg = mg_find(sv, 'm');
    if (!mg) {
	condpair_t *cp;

	New(53, cp, 1, condpair_t);
	MUTEX_INIT(&cp->mutex);
	COND_INIT(&cp->owner_cond);
	COND_INIT(&cp->cond);
	cp->owner = 0;
	LOCK_CRED_MUTEX;		/* XXX need separate mutex? */
	mg = mg_find(sv, 'm');
	if (mg) {
	    /* someone else beat us to initialising it */
	    UNLOCK_CRED_MUTEX;		/* XXX need separate mutex? */
	    MUTEX_DESTROY(&cp->mutex);
	    COND_DESTROY(&cp->owner_cond);
	    COND_DESTROY(&cp->cond);
	    Safefree(cp);
	}
	else {
	    sv_magic(sv, Nullsv, 'm', 0, 0);
	    mg = SvMAGIC(sv);
	    mg->mg_ptr = (char *)cp;
	    mg->mg_len = sizeof(cp);
	    UNLOCK_CRED_MUTEX;		/* XXX need separate mutex? */
	    DEBUG_S(WITH_THR(PerlIO_printf(Perl_debug_log,
					   "%p: condpair_magic %p\n", thr, sv));)
	}
    }
    return mg;
}

SV *
Perl_sv_lock(pTHX_ SV *osv)
{
    MAGIC *mg;
    SV *sv = osv;

    LOCK_SV_LOCK_MUTEX;
    if (SvROK(sv)) {
	sv = SvRV(sv);
    }

    mg = condpair_magic(sv);
    MUTEX_LOCK(MgMUTEXP(mg));
    if (MgOWNER(mg) == thr)
	MUTEX_UNLOCK(MgMUTEXP(mg));
    else {
	while (MgOWNER(mg))
	    COND_WAIT(MgOWNERCONDP(mg), MgMUTEXP(mg));
	MgOWNER(mg) = thr;
	DEBUG_S(PerlIO_printf(Perl_debug_log,
			      "0x%"UVxf": Perl_lock lock 0x%"UVxf"\n",
			      PTR2UV(thr), PTR2UV(sv));)
	MUTEX_UNLOCK(MgMUTEXP(mg));
	SAVEDESTRUCTOR_X(Perl_unlock_condpair, sv);
    }
    UNLOCK_SV_LOCK_MUTEX;
    return sv;
}

/*
 * Make a new perl thread structure using t as a prototype. Some of the
 * fields for the new thread are copied from the prototype thread, t,
 * so t should not be running in perl at the time this function is
 * called. The use by ext/Thread/Thread.xs in core perl (where t is the
 * thread calling new_struct_thread) clearly satisfies this constraint.
 */
struct perl_thread *
Perl_new_struct_thread(pTHX_ struct perl_thread *t)
{
#if !defined(PERL_IMPLICIT_CONTEXT)
    struct perl_thread *thr;
#endif
    SV *sv;
    SV **svp;
    I32 i;

    sv = newSVpvn("", 0);
    SvGROW(sv, sizeof(struct perl_thread) + 1);
    SvCUR_set(sv, sizeof(struct perl_thread));
    thr = (Thread) SvPVX(sv);
#ifdef DEBUGGING
    memset(thr, 0xab, sizeof(struct perl_thread));
    PL_markstack = 0;
    PL_scopestack = 0;
    PL_savestack = 0;
    PL_retstack = 0;
    PL_dirty = 0;
    PL_localizing = 0;
    Zero(&PL_hv_fetch_ent_mh, 1, HE);
    PL_efloatbuf = (char*)NULL;
    PL_efloatsize = 0;
#else
    Zero(thr, 1, struct perl_thread);
#endif

    thr->oursv = sv;
    init_stacks();

    PL_curcop = &PL_compiling;
    thr->interp = t->interp;
    thr->cvcache = newHV();
    thr->threadsv = newAV();
    thr->specific = newAV();
    thr->errsv = newSVpvn("", 0);
    thr->flags = THRf_R_JOINABLE;
    MUTEX_INIT(&thr->mutex);

    JMPENV_BOOTSTRAP;

    PL_in_eval = EVAL_NULL;	/* ~(EVAL_INEVAL|EVAL_WARNONLY|EVAL_KEEPERR|EVAL_INREQUIRE) */
    PL_restartop = 0;

    PL_statname = NEWSV(66,0);
    PL_errors = newSVpvn("", 0);
    PL_maxscream = -1;
    PL_regcompp = MEMBER_TO_FPTR(Perl_pregcomp);
    PL_regexecp = MEMBER_TO_FPTR(Perl_regexec_flags);
    PL_regint_start = MEMBER_TO_FPTR(Perl_re_intuit_start);
    PL_regint_string = MEMBER_TO_FPTR(Perl_re_intuit_string);
    PL_regfree = MEMBER_TO_FPTR(Perl_pregfree);
    PL_regindent = 0;
    PL_reginterp_cnt = 0;
    PL_lastscream = Nullsv;
    PL_screamfirst = 0;
    PL_screamnext = 0;
    PL_reg_start_tmp = 0;
    PL_reg_start_tmpl = 0;
    PL_reg_poscache = Nullch;

    /* parent thread's data needs to be locked while we make copy */
    MUTEX_LOCK(&t->mutex);

#ifdef PERL_FLEXIBLE_EXCEPTIONS
    PL_protect = t->Tprotect;
#endif

    PL_curcop = t->Tcurcop;       /* XXX As good a guess as any? */
    PL_defstash = t->Tdefstash;   /* XXX maybe these should */
    PL_curstash = t->Tcurstash;   /* always be set to main? */

    PL_tainted = t->Ttainted;
    PL_curpm = t->Tcurpm;         /* XXX No PMOP ref count */
    PL_nrs = newSVsv(t->Tnrs);
    PL_rs = t->Tnrs ? SvREFCNT_inc(PL_nrs) : Nullsv;
    PL_last_in_gv = Nullgv;
    PL_ofslen = t->Tofslen;
    PL_ofs = savepvn(t->Tofs, PL_ofslen);
    PL_defoutgv = (GV*)SvREFCNT_inc(t->Tdefoutgv);
    PL_chopset = t->Tchopset;
    PL_bodytarget = newSVsv(t->Tbodytarget);
    PL_toptarget = newSVsv(t->Ttoptarget);
    if (t->Tformtarget == t->Ttoptarget)
	PL_formtarget = PL_toptarget;
    else
	PL_formtarget = PL_bodytarget;

    /* Initialise all per-thread SVs that the template thread used */
    svp = AvARRAY(t->threadsv);
    for (i = 0; i <= AvFILLp(t->threadsv); i++, svp++) {
	if (*svp && *svp != &PL_sv_undef) {
	    SV *sv = newSVsv(*svp);
	    av_store(thr->threadsv, i, sv);
	    sv_magic(sv, 0, 0, &PL_threadsv_names[i], 1);
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
		"new_struct_thread: copied threadsv %"IVdf" %p->%p\n",
				  (IV)i, t, thr));
	}
    } 
    thr->threadsvp = AvARRAY(thr->threadsv);

    MUTEX_LOCK(&PL_threads_mutex);
    PL_nthreads++;
    thr->tid = ++PL_threadnum;
    thr->next = t->next;
    thr->prev = t;
    t->next = thr;
    thr->next->prev = thr;
    MUTEX_UNLOCK(&PL_threads_mutex);

    /* done copying parent's state */
    MUTEX_UNLOCK(&t->mutex);

#ifdef HAVE_THREAD_INTERN
    Perl_init_thread_intern(thr);
#endif /* HAVE_THREAD_INTERN */
    return thr;
}
#endif /* USE_THREADS */

#if defined(HUGE_VAL) || (defined(USE_LONG_DOUBLE) && defined(HUGE_VALL))
/*
 * This hack is to force load of "huge" support from libm.a
 * So it is in perl for (say) POSIX to use. 
 * Needed for SunOS with Sun's 'acc' for example.
 */
NV 
Perl_huge(void)
{
#   if defined(USE_LONG_DOUBLE) && defined(HUGE_VALL)
    return HUGE_VALL;
#   endif
    return HUGE_VAL;
}
#endif

#ifdef PERL_GLOBAL_STRUCT
struct perl_vars *
Perl_GetVars(pTHX)
{
 return &PL_Vars;
}
#endif

char **
Perl_get_op_names(pTHX)
{
 return PL_op_name;
}

char **
Perl_get_op_descs(pTHX)
{
 return PL_op_desc;
}

char *
Perl_get_no_modify(pTHX)
{
 return (char*)PL_no_modify;
}

U32 *
Perl_get_opargs(pTHX)
{
 return PL_opargs;
}

PPADDR_t*
Perl_get_ppaddr(pTHX)
{
 return &PL_ppaddr;
}

#ifndef HAS_GETENV_LEN
char *
Perl_getenv_len(pTHX_ const char *env_elem, unsigned long *len)
{
    char *env_trans = PerlEnv_getenv(env_elem);
    if (env_trans)
	*len = strlen(env_trans);
    return env_trans;
}
#endif


MGVTBL*
Perl_get_vtbl(pTHX_ int vtbl_id)
{
    MGVTBL* result = Null(MGVTBL*);

    switch(vtbl_id) {
    case want_vtbl_sv:
	result = &PL_vtbl_sv;
	break;
    case want_vtbl_env:
	result = &PL_vtbl_env;
	break;
    case want_vtbl_envelem:
	result = &PL_vtbl_envelem;
	break;
    case want_vtbl_sig:
	result = &PL_vtbl_sig;
	break;
    case want_vtbl_sigelem:
	result = &PL_vtbl_sigelem;
	break;
    case want_vtbl_pack:
	result = &PL_vtbl_pack;
	break;
    case want_vtbl_packelem:
	result = &PL_vtbl_packelem;
	break;
    case want_vtbl_dbline:
	result = &PL_vtbl_dbline;
	break;
    case want_vtbl_isa:
	result = &PL_vtbl_isa;
	break;
    case want_vtbl_isaelem:
	result = &PL_vtbl_isaelem;
	break;
    case want_vtbl_arylen:
	result = &PL_vtbl_arylen;
	break;
    case want_vtbl_glob:
	result = &PL_vtbl_glob;
	break;
    case want_vtbl_mglob:
	result = &PL_vtbl_mglob;
	break;
    case want_vtbl_nkeys:
	result = &PL_vtbl_nkeys;
	break;
    case want_vtbl_taint:
	result = &PL_vtbl_taint;
	break;
    case want_vtbl_substr:
	result = &PL_vtbl_substr;
	break;
    case want_vtbl_vec:
	result = &PL_vtbl_vec;
	break;
    case want_vtbl_pos:
	result = &PL_vtbl_pos;
	break;
    case want_vtbl_bm:
	result = &PL_vtbl_bm;
	break;
    case want_vtbl_fm:
	result = &PL_vtbl_fm;
	break;
    case want_vtbl_uvar:
	result = &PL_vtbl_uvar;
	break;
#ifdef USE_THREADS
    case want_vtbl_mutex:
	result = &PL_vtbl_mutex;
	break;
#endif
    case want_vtbl_defelem:
	result = &PL_vtbl_defelem;
	break;
    case want_vtbl_regexp:
	result = &PL_vtbl_regexp;
	break;
    case want_vtbl_regdata:
	result = &PL_vtbl_regdata;
	break;
    case want_vtbl_regdatum:
	result = &PL_vtbl_regdatum;
	break;
#ifdef USE_LOCALE_COLLATE
    case want_vtbl_collxfrm:
	result = &PL_vtbl_collxfrm;
	break;
#endif
    case want_vtbl_amagic:
	result = &PL_vtbl_amagic;
	break;
    case want_vtbl_amagicelem:
	result = &PL_vtbl_amagicelem;
	break;
    case want_vtbl_backref:
	result = &PL_vtbl_backref;
	break;
    }
    return result;
}

#if !defined(FFLUSH_NULL) && defined(HAS__FWALK)
static int S_fflush(FILE *fp);

static int
S_fflush(FILE *fp)
{
    return fflush(fp);
}
#endif

I32
Perl_my_fflush_all(pTHX)
{
#if defined(FFLUSH_NULL)
    return PerlIO_flush(NULL);
#else
# if defined(HAS__FWALK)
    /* undocumented, unprototyped, but very useful BSDism */
    extern void _fwalk(int (*)(FILE *));
    _fwalk(&S_fflush);
    return 0;
#   else
    long open_max = -1;
#  if defined(FFLUSH_ALL) && defined(HAS_STDIO_STREAM_ARRAY)
#   ifdef PERL_FFLUSH_ALL_FOPEN_MAX
    open_max = PERL_FFLUSH_ALL_FOPEN_MAX;
#   else
#   if defined(HAS_SYSCONF) && defined(_SC_OPEN_MAX)
    open_max = sysconf(_SC_OPEN_MAX);
#   else
#    ifdef FOPEN_MAX
    open_max = FOPEN_MAX;
#    else
#     ifdef OPEN_MAX
    open_max = OPEN_MAX;
#     else
#      ifdef _NFILE
    open_max = _NFILE;
#      endif
#     endif
#    endif
#   endif
#   endif
    if (open_max > 0) {
      long i;
      for (i = 0; i < open_max; i++)
	    if (STDIO_STREAM_ARRAY[i]._file >= 0 &&
		STDIO_STREAM_ARRAY[i]._file < open_max &&
		STDIO_STREAM_ARRAY[i]._flag)
		PerlIO_flush(&STDIO_STREAM_ARRAY[i]);
      return 0;
    }
#  endif
    SETERRNO(EBADF,RMS$_IFI);
    return EOF;
# endif
#endif
}

NV
Perl_my_atof(pTHX_ const char* s)
{
    NV x = 0.0;
#ifdef USE_LOCALE_NUMERIC
    if ((PL_hints & HINT_LOCALE) && PL_numeric_local) {
	NV y;

	Perl_atof2(s, x);
	SET_NUMERIC_STANDARD();
	Perl_atof2(s, y);
	SET_NUMERIC_LOCAL();
	if ((y < 0.0 && y < x) || (y > 0.0 && y > x))
	    return y;
    }
    else
	Perl_atof2(s, x);
#else
    Perl_atof2(s, x);
#endif
    return x;
}

void
Perl_report_evil_fh(pTHX_ GV *gv, IO *io, I32 op)
{
    char *vile;
    I32   warn_type;
    char *func =
	op == OP_READLINE   ? "readline"  :	/* "<HANDLE>" not nice */
	op == OP_LEAVEWRITE ? "write" :		/* "write exit" not nice */
	PL_op_desc[op];
    char *pars = OP_IS_FILETEST(op) ? "" : "()";
    char *type = OP_IS_SOCKET(op) || (io && IoTYPE(io) == IoTYPE_SOCKET) ?
                     "socket" : "filehandle";
    char *name = NULL;

    if (io && IoTYPE(io) == IoTYPE_CLOSED) {
	vile = "closed";
	warn_type = WARN_CLOSED;
    }
    else {
	vile = "unopened";
	warn_type = WARN_UNOPENED;
    }

    if (gv && isGV(gv)) {
	SV *sv = sv_newmortal();
	gv_efullname4(sv, gv, Nullch, FALSE);
	name = SvPVX(sv);
    }

    if (name && *name) {
	Perl_warner(aTHX_ warn_type,
		    "%s%s on %s %s %s", func, pars, vile, type, name);
	if (io && IoDIRP(io) && !(IoFLAGS(io) & IOf_FAKE_DIRP))
	    Perl_warner(aTHX_ warn_type,
			"\t(Are you trying to call %s%s on dirhandle %s?)\n",
			func, pars, name);
    }
    else {
	Perl_warner(aTHX_ warn_type,
		    "%s%s on %s %s", func, pars, vile, type);
	if (io && IoDIRP(io) && !(IoFLAGS(io) & IOf_FAKE_DIRP))
	    Perl_warner(aTHX_ warn_type,
			"\t(Are you trying to call %s%s on dirhandle?)\n",
			func, pars);
    }
}

#ifdef EBCDIC
/* in ASCII order, not that it matters */
static const char controllablechars[] = "?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";

int
Perl_ebcdic_control(pTHX_ int ch)
{
    	if (ch > 'a') {
	        char *ctlp;
 
 	       if (islower(ch))
  	              ch = toupper(ch);
 
 	       if ((ctlp = strchr(controllablechars, ch)) == 0) {
  	              Perl_die(aTHX_ "unrecognised control character '%c'\n", ch);
     	       }
 
        	if (ctlp == controllablechars)
         	       return('\177'); /* DEL */
        	else
         	       return((unsigned char)(ctlp - controllablechars - 1));
	} else { /* Want uncontrol */
        	if (ch == '\177' || ch == -1)
                	return('?');
        	else if (ch == '\157')
                	return('\177');
        	else if (ch == '\174')
                	return('\000');
        	else if (ch == '^')    /* '\137' in 1047, '\260' in 819 */
                	return('\036');
        	else if (ch == '\155')
                	return('\037');
        	else if (0 < ch && ch < (sizeof(controllablechars) - 1))
                	return(controllablechars[ch+1]);
        	else
                	Perl_die(aTHX_ "invalid control request: '\\%03o'\n", ch & 0xFF);
	}
}
#endif
