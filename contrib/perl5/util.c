/*    util.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
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
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

#ifndef SIG_ERR
# define SIG_ERR ((Sighandler_t) -1)
#endif

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#  include <unistd.h>
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

#ifdef I_FCNTL
#  include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#  include <sys/file.h>
#endif

#ifdef I_SYS_WAIT
#  include <sys/wait.h>
#endif

#define FLUSH

#ifdef LEAKTEST

static void xstat _((int));
long xcount[MAXXCOUNT];
long lastxcount[MAXXCOUNT];
long xycount[MAXXCOUNT][MAXYCOUNT];
long lastxycount[MAXXCOUNT][MAXYCOUNT];

#endif

#ifndef MYMALLOC

/* paranoid version of malloc */

/* NOTE:  Do not call the next three routines directly.  Use the macros
 * in handy.h, so that we can easily redefine everything to do tracking of
 * allocated hunks back to the original New to track down any memory leaks.
 * XXX This advice seems to be widely ignored :-(   --AD  August 1996.
 */

Malloc_t
safemalloc(MEM_SIZE size)
{
    Malloc_t ptr;
#ifdef HAS_64K_LIMIT
	if (size > 0xffff) {
		PerlIO_printf(PerlIO_stderr(), "Allocation too large: %lx\n", size) FLUSH;
		my_exit(1);
	}
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
    if ((long)size < 0)
	croak("panic: malloc");
#endif
    ptr = PerlMem_malloc(size?size:1);	/* malloc(0) is NASTY on our system */
#if !(defined(I286) || defined(atarist))
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%x: (%05d) malloc %ld bytes\n",ptr,PL_an++,(long)size));
#else
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) malloc %ld bytes\n",ptr,PL_an++,(long)size));
#endif
    if (ptr != Nullch)
	return ptr;
    else if (PL_nomemok)
	return Nullch;
    else {
	PerlIO_puts(PerlIO_stderr(),no_mem) FLUSH;
	my_exit(1);
        return Nullch;
    }
    /*NOTREACHED*/
}

/* paranoid version of realloc */

Malloc_t
saferealloc(Malloc_t where,MEM_SIZE size)
{
    Malloc_t ptr;
#if !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE)
    Malloc_t PerlMem_realloc();
#endif /* !defined(STANDARD_C) && !defined(HAS_REALLOC_PROTOTYPE) */

#ifdef HAS_64K_LIMIT 
    if (size > 0xffff) {
	PerlIO_printf(PerlIO_stderr(),
		      "Reallocation too large: %lx\n", size) FLUSH;
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
    if (!size) {
	safefree(where);
	return NULL;
    }

    if (!where)
	return safemalloc(size);
#ifdef DEBUGGING
    if ((long)size < 0)
	croak("panic: realloc");
#endif
    ptr = PerlMem_realloc(where,size);

#if !(defined(I286) || defined(atarist))
    DEBUG_m( {
	PerlIO_printf(Perl_debug_log, "0x%x: (%05d) rfree\n",where,PL_an++);
	PerlIO_printf(Perl_debug_log, "0x%x: (%05d) realloc %ld bytes\n",ptr,PL_an++,(long)size);
    } )
#else
    DEBUG_m( {
	PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) rfree\n",where,PL_an++);
	PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) realloc %ld bytes\n",ptr,PL_an++,(long)size);
    } )
#endif

    if (ptr != Nullch)
	return ptr;
    else if (PL_nomemok)
	return Nullch;
    else {
	PerlIO_puts(PerlIO_stderr(),no_mem) FLUSH;
	my_exit(1);
	return Nullch;
    }
    /*NOTREACHED*/
}

/* safe version of free */

Free_t
safefree(Malloc_t where)
{
#if !(defined(I286) || defined(atarist))
    DEBUG_m( PerlIO_printf(Perl_debug_log, "0x%x: (%05d) free\n",(char *) where,PL_an++));
#else
    DEBUG_m( PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) free\n",(char *) where,PL_an++));
#endif
    if (where) {
	/*SUPPRESS 701*/
	PerlMem_free(where);
    }
}

/* safe version of calloc */

Malloc_t
safecalloc(MEM_SIZE count, MEM_SIZE size)
{
    Malloc_t ptr;

#ifdef HAS_64K_LIMIT
    if (size * count > 0xffff) {
	PerlIO_printf(PerlIO_stderr(),
		      "Allocation too large: %lx\n", size * count) FLUSH;
	my_exit(1);
    }
#endif /* HAS_64K_LIMIT */
#ifdef DEBUGGING
    if ((long)size < 0 || (long)count < 0)
	croak("panic: calloc");
#endif
    size *= count;
    ptr = PerlMem_malloc(size?size:1);	/* malloc(0) is NASTY on our system */
#if !(defined(I286) || defined(atarist))
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%x: (%05d) calloc %ld  x %ld bytes\n",ptr,PL_an++,(long)count,(long)size));
#else
    DEBUG_m(PerlIO_printf(Perl_debug_log, "0x%lx: (%05d) calloc %ld x %ld bytes\n",ptr,PL_an++,(long)count,(long)size));
#endif
    if (ptr != Nullch) {
	memset((void*)ptr, 0, size);
	return ptr;
    }
    else if (PL_nomemok)
	return Nullch;
    else {
	PerlIO_puts(PerlIO_stderr(),no_mem) FLUSH;
	my_exit(1);
	return Nullch;
    }
    /*NOTREACHED*/
}

#endif /* !MYMALLOC */

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
safexmalloc(I32 x, MEM_SIZE size)
{
    register char* where = (char*)safemalloc(size + ALIGN);

    xcount[x] += size;
    xycount[x][SIZE_TO_Y(size)]++;
    set_typeof_chunk(where, x);
    sizeof_chunk(where) = size;
    return (Malloc_t)(where + ALIGN);
}

Malloc_t
safexrealloc(Malloc_t wh, MEM_SIZE size)
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
safexfree(Malloc_t wh)
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
safexcalloc(I32 x,MEM_SIZE count, MEM_SIZE size)
{
    register char * where = (char*)safexmalloc(x, size * count + ALIGN);
    xcount[x] += size;
    xycount[x][SIZE_TO_Y(size)]++;
    memset((void*)(where + ALIGN), 0, size * count);
    set_typeof_chunk(where, x);
    sizeof_chunk(where) = size;
    return (Malloc_t)(where + ALIGN);
}

static void
xstat(int flag)
{
    register I32 i, j, total = 0;
    I32 subtot[MAXYCOUNT];

    for (j = 0; j < MAXYCOUNT; j++) {
	subtot[j] = 0;
    }
    
    PerlIO_printf(PerlIO_stderr(), "   Id  subtot   4   8  12  16  20  24  28  32  36  40  48  56  64  72  80 80+\n", total);
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
	    PerlIO_printf(PerlIO_stderr(),"%2d %02d %7ld ", i / 100, i % 100, 
			  flag == 2 ? xcount[i] - lastxcount[i] : xcount[i]);
	    lastxcount[i] = xcount[i];
	    for (j = 0; j < MAXYCOUNT; j++) {
		if ( flag == 0 
		     ? xycount[i][j]	/* Have something */
		     : (flag == 2 
			? xycount[i][j] != lastxycount[i][j] /* Changed */
			: xycount[i][j] > lastxycount[i][j])) {	/* Growed */
		    PerlIO_printf(PerlIO_stderr(),"%3ld ", 
				  flag == 2 
				  ? xycount[i][j] - lastxycount[i][j] 
				  : xycount[i][j]);
		    lastxycount[i][j] = xycount[i][j];
		} else {
		    PerlIO_printf(PerlIO_stderr(), "  . ", xycount[i][j]);
		}
	    }
	    PerlIO_printf(PerlIO_stderr(), "\n");
	}
    }
    if (flag != 2) {
	PerlIO_printf(PerlIO_stderr(), "Total %7ld ", total);
	for (j = 0; j < MAXYCOUNT; j++) {
	    if (subtot[j]) {
		PerlIO_printf(PerlIO_stderr(), "%3ld ", subtot[j]);
	    } else {
		PerlIO_printf(PerlIO_stderr(), "  . ");
	    }
	}
	PerlIO_printf(PerlIO_stderr(), "\n");	
    }
}

#endif /* LEAKTEST */

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
delimcpy(register char *to, register char *toend, register char *from, register char *fromend, register int delim, I32 *retlen)
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
instr(register char *big, register char *little)
{
    register char *s, *x;
    register I32 first;

    if (!little)
	return big;
    first = *little++;
    if (!first)
	return big;
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
	    return big-1;
    }
    return Nullch;
}

/* same as instr but allow embedded nulls */

char *
ninstr(register char *big, register char *bigend, char *little, char *lend)
{
    register char *s, *x;
    register I32 first = *little;
    register char *littleend = lend;

    if (!first && little >= littleend)
	return big;
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
	    return big-1;
    }
    return Nullch;
}

/* reverse of the above--find last substring */

char *
rninstr(register char *big, char *bigend, char *little, char *lend)
{
    register char *bigbeg;
    register char *s, *x;
    register I32 first = *little;
    register char *littleend = lend;

    if (!first && little >= littleend)
	return bigend;
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
	    return big+1;
    }
    return Nullch;
}

/*
 * Set up for a new ctype locale.
 */
void
perl_new_ctype(char *newctype)
{
#ifdef USE_LOCALE_CTYPE

    int i;

    for (i = 0; i < 256; i++) {
	if (isUPPER_LC(i))
	    fold_locale[i] = toLOWER_LC(i);
	else if (isLOWER_LC(i))
	    fold_locale[i] = toUPPER_LC(i);
	else
	    fold_locale[i] = i;
    }

#endif /* USE_LOCALE_CTYPE */
}

/*
 * Set up for a new collation locale.
 */
void
perl_new_collate(char *newcoll)
{
#ifdef USE_LOCALE_COLLATE

    if (! newcoll) {
	if (PL_collation_name) {
	    ++PL_collation_ix;
	    Safefree(PL_collation_name);
	    PL_collation_name = NULL;
	    PL_collation_standard = TRUE;
	    PL_collxfrm_base = 0;
	    PL_collxfrm_mult = 2;
	}
	return;
    }

    if (! PL_collation_name || strNE(PL_collation_name, newcoll)) {
	++PL_collation_ix;
	Safefree(PL_collation_name);
	PL_collation_name = savepv(newcoll);
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
	      croak("strxfrm() gets absurd");
	  PL_collxfrm_base = (fa > mult) ? (fa - mult) : 0;
	  PL_collxfrm_mult = mult;
	}
    }

#endif /* USE_LOCALE_COLLATE */
}

/*
 * Set up for a new numeric locale.
 */
void
perl_new_numeric(char *newnum)
{
#ifdef USE_LOCALE_NUMERIC

    if (! newnum) {
	if (PL_numeric_name) {
	    Safefree(PL_numeric_name);
	    PL_numeric_name = NULL;
	    PL_numeric_standard = TRUE;
	    PL_numeric_local = TRUE;
	}
	return;
    }

    if (! PL_numeric_name || strNE(PL_numeric_name, newnum)) {
	Safefree(PL_numeric_name);
	PL_numeric_name = savepv(newnum);
	PL_numeric_standard = (strEQ(newnum, "C") || strEQ(newnum, "POSIX"));
	PL_numeric_local = TRUE;
    }

#endif /* USE_LOCALE_NUMERIC */
}

void
perl_set_numeric_standard(void)
{
#ifdef USE_LOCALE_NUMERIC

    if (! PL_numeric_standard) {
	setlocale(LC_NUMERIC, "C");
	PL_numeric_standard = TRUE;
	PL_numeric_local = FALSE;
    }

#endif /* USE_LOCALE_NUMERIC */
}

void
perl_set_numeric_local(void)
{
#ifdef USE_LOCALE_NUMERIC

    if (! PL_numeric_local) {
	setlocale(LC_NUMERIC, PL_numeric_name);
	PL_numeric_standard = FALSE;
	PL_numeric_local = TRUE;
    }

#endif /* USE_LOCALE_NUMERIC */
}


/*
 * Initialize locale awareness.
 */
int
perl_init_i18nl10n(int printwarn)
{
    int ok = 1;
    /* returns
     *    1 = set ok or not applicable,
     *    0 = fallback to C locale,
     *   -1 = fallback to C locale failed
     */

#ifdef USE_LOCALE

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
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	if (! (curcoll =
	       setlocale(LC_COLLATE,
			 (!done && (lang || PerlEnv_getenv("LC_COLLATE")))
				   ? "" : Nullch)))
	    setlocale_failure = TRUE;
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	if (! (curnum =
	       setlocale(LC_NUMERIC,
			 (!done && (lang || PerlEnv_getenv("LC_NUMERIC")))
				  ? "" : Nullch)))
	    setlocale_failure = TRUE;
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
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	if (! (curcoll = setlocale(LC_COLLATE, "")))
	    setlocale_failure = TRUE;
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	if (! (curnum = setlocale(LC_NUMERIC, "")))
	    setlocale_failure = TRUE;
#endif /* USE_LOCALE_NUMERIC */
    }

    if (setlocale_failure) {
	char *p;
	bool locwarn = (printwarn > 1 || 
			printwarn &&
			(!(p = PerlEnv_getenv("PERL_BADLANG")) || atoi(p)));

	if (locwarn) {
#ifdef LC_ALL
  
	    PerlIO_printf(PerlIO_stderr(),
	       "perl: warning: Setting locale failed.\n");

#else /* !LC_ALL */
  
	    PerlIO_printf(PerlIO_stderr(),
	       "perl: warning: Setting locale failed for the categories:\n\t");
#ifdef USE_LOCALE_CTYPE
	    if (! curctype)
		PerlIO_printf(PerlIO_stderr(), "LC_CTYPE ");
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	    if (! curcoll)
		PerlIO_printf(PerlIO_stderr(), "LC_COLLATE ");
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	    if (! curnum)
		PerlIO_printf(PerlIO_stderr(), "LC_NUMERIC ");
#endif /* USE_LOCALE_NUMERIC */
	    PerlIO_printf(PerlIO_stderr(), "\n");

#endif /* LC_ALL */

	    PerlIO_printf(PerlIO_stderr(),
		"perl: warning: Please check that your locale settings:\n");

#ifdef __GLIBC__
	    PerlIO_printf(PerlIO_stderr(),
			  "\tLANGUAGE = %c%s%c,\n",
			  language ? '"' : '(',
			  language ? language : "unset",
			  language ? '"' : ')');
#endif

	    PerlIO_printf(PerlIO_stderr(),
			  "\tLC_ALL = %c%s%c,\n",
			  lc_all ? '"' : '(',
			  lc_all ? lc_all : "unset",
			  lc_all ? '"' : ')');

	    {
	      char **e;
	      for (e = environ; *e; e++) {
		  if (strnEQ(*e, "LC_", 3)
			&& strnNE(*e, "LC_ALL=", 7)
			&& (p = strchr(*e, '=')))
		      PerlIO_printf(PerlIO_stderr(), "\t%.*s = \"%s\",\n",
				    (int)(p - *e), *e, p + 1);
	      }
	    }

	    PerlIO_printf(PerlIO_stderr(),
			  "\tLANG = %c%s%c\n",
			  lang ? '"' : '(',
			  lang ? lang : "unset",
			  lang ? '"' : ')');

	    PerlIO_printf(PerlIO_stderr(),
			  "    are supported and installed on your system.\n");
	}

#ifdef LC_ALL

	if (setlocale(LC_ALL, "C")) {
	    if (locwarn)
		PerlIO_printf(PerlIO_stderr(),
      "perl: warning: Falling back to the standard locale (\"C\").\n");
	    ok = 0;
	}
	else {
	    if (locwarn)
		PerlIO_printf(PerlIO_stderr(),
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
		PerlIO_printf(PerlIO_stderr(),
      "perl: warning: Cannot fall back to the standard locale (\"C\").\n");
	    ok = -1;
	}

#endif /* ! LC_ALL */

#ifdef USE_LOCALE_CTYPE
	curctype = setlocale(LC_CTYPE, Nullch);
#endif /* USE_LOCALE_CTYPE */
#ifdef USE_LOCALE_COLLATE
	curcoll = setlocale(LC_COLLATE, Nullch);
#endif /* USE_LOCALE_COLLATE */
#ifdef USE_LOCALE_NUMERIC
	curnum = setlocale(LC_NUMERIC, Nullch);
#endif /* USE_LOCALE_NUMERIC */
    }

#ifdef USE_LOCALE_CTYPE
    perl_new_ctype(curctype);
#endif /* USE_LOCALE_CTYPE */

#ifdef USE_LOCALE_COLLATE
    perl_new_collate(curcoll);
#endif /* USE_LOCALE_COLLATE */

#ifdef USE_LOCALE_NUMERIC
    perl_new_numeric(curnum);
#endif /* USE_LOCALE_NUMERIC */

#endif /* USE_LOCALE */

    return ok;
}

/* Backwards compatibility. */
int
perl_init_i18nl14n(int printwarn)
{
    return perl_init_i18nl10n(printwarn);
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
mem_collxfrm(const char *s, STRLEN len, STRLEN *xlen)
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

void
fbm_compile(SV *sv, U32 flags /* not used yet */)
{
    register U8 *s;
    register U8 *table;
    register U32 i;
    STRLEN len;
    I32 rarest = 0;
    U32 frequency = 256;

    s = (U8*)SvPV_force(sv, len);
    (void)SvUPGRADE(sv, SVt_PVBM);
    if (len > 255 || len == 0)	/* TAIL might be on on a zero-length string. */
	return;			/* can't have offsets that big */
    if (len > 2) {
	Sv_Grow(sv,len + 258);
	table = (unsigned char*)(SvPVX(sv) + len + 1);
	s = table - 2;
	for (i = 0; i < 256; i++) {
	    table[i] = len;
	}
	i = 0;
	while (s >= (unsigned char*)(SvPVX(sv)))
	    {
		if (table[*s] == len)
		    table[*s] = i;
		s--,i++;
	    }
    }
    sv_magic(sv, Nullsv, 'B', Nullch, 0);	/* deep magic */
    SvVALID_on(sv);

    s = (unsigned char*)(SvPVX(sv));		/* deeper magic */
    for (i = 0; i < len; i++) {
	if (freq[s[i]] < frequency) {
	    rarest = i;
	    frequency = freq[s[i]];
	}
    }
    BmRARE(sv) = s[rarest];
    BmPREVIOUS(sv) = rarest;
    DEBUG_r(PerlIO_printf(Perl_debug_log, "rarest char %c at %d\n",BmRARE(sv),BmPREVIOUS(sv)));
}

char *
fbm_instr(unsigned char *big, register unsigned char *bigend, SV *littlestr, U32 flags)
{
    register unsigned char *s;
    register I32 tmp;
    register I32 littlelen;
    register unsigned char *little;
    register unsigned char *table;
    register unsigned char *olds;
    register unsigned char *oldlittle;

    if (SvTYPE(littlestr) != SVt_PVBM || !SvVALID(littlestr)) {
	STRLEN len;
	char *l = SvPV(littlestr,len);
	if (!len) {
	    if (SvTAIL(littlestr)) {	/* Can be only 0-len constant
					   substr => we can ignore SvVALID */
		if (PL_multiline) {
		    char *t = "\n";
		    if ((s = (unsigned char*)ninstr((char*)big, (char*)bigend,
			 			    t, t + len))) {
			return (char*)s;
		    }
		}
		if (bigend > big && bigend[-1] == '\n')
		    return (char *)(bigend - 1);
		else
		    return (char *) bigend;
	    }
	    return (char*)big;
	}
	return ninstr((char*)big,(char*)bigend, l, l + len);
    }

    littlelen = SvCUR(littlestr);
    if (SvTAIL(littlestr) && !PL_multiline) {	/* tail anchored? */
	if (littlelen > bigend - big)
	    return Nullch;
	little = (unsigned char*)SvPVX(littlestr);
	s = bigend - littlelen;
	if (s > big
	    && bigend[-1] == '\n' 
	    && s[-1] == *little && memEQ((char*)s - 1,(char*)little,littlelen))
	    return (char*)s - 1;	/* how sweet it is */
	else if (*s == *little && memEQ((char*)s,(char*)little,littlelen))
	    return (char*)s;		/* how sweet it is */
	return Nullch;
    }
    if (littlelen <= 2) {
	unsigned char c1 = (unsigned char)SvPVX(littlestr)[0];
	unsigned char c2 = (unsigned char)SvPVX(littlestr)[1];
	/* This may do extra comparisons if littlelen == 2, but this
	   should be hidden in the noise since we do less indirection. */
	
	s = big;
	bigend -= littlelen;
	while (s <= bigend) {
	    if (s[0] == c1 
		&& (littlelen == 1 || s[1] == c2)
		&& (!SvTAIL(littlestr)
		    || s == bigend
		    || s[littlelen] == '\n')) /* Automatically multiline */
	    {
		return (char*)s;
	    }
	    s++;
	}
	return Nullch;
    }
    table = (unsigned char*)(SvPVX(littlestr) + littlelen + 1);
    if (--littlelen >= bigend - big)
	return Nullch;
    s = big + littlelen;
    oldlittle = little = table - 2;
    if (s < bigend) {
      top2:
	/*SUPPRESS 560*/
	if (tmp = table[*s]) {
#ifdef POINTERRIGOR
	    if (bigend - s > tmp) {
		s += tmp;
		goto top2;
	    }
#else
	    if ((s += tmp) < bigend)
		goto top2;
#endif
	    return Nullch;
	}
	else {
	    tmp = littlelen;	/* less expensive than calling strncmp() */
	    olds = s;
	    while (tmp--) {
		if (*--s == *--little)
		    continue;
	      differ:
		s = olds + 1;	/* here we pay the price for failure */
		little = oldlittle;
		if (s < bigend)	/* fake up continue to outer loop */
		    goto top2;
		return Nullch;
	    }
	    if (SvTAIL(littlestr)	/* automatically multiline */
		&& olds + 1 != bigend
		&& olds[1] != '\n') 
		goto differ;
	    return (char *)s;
	}
    }
    return Nullch;
}

/* start_shift, end_shift are positive quantities which give offsets
   of ends of some substring of bigstr.
   If `last' we want the last occurence.
   old_posp is the way of communication between consequent calls if
   the next call needs to find the . 
   The initial *old_posp should be -1.
   Note that we do not take into account SvTAIL, so it may give wrong
   positives if _ALL flag is set.
 */

char *
screaminstr(SV *bigstr, SV *littlestr, I32 start_shift, I32 end_shift, I32 *old_posp, I32 last)
{
    dTHR;
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
	: (((pos = *old_posp), pos += PL_screamnext[pos]) == 0))
	return Nullch;
    little = (unsigned char *)(SvPVX(littlestr));
    littleend = little + SvCUR(littlestr);
    first = *little++;
    /* The value of pos we can start at: */
    previous = BmPREVIOUS(littlestr);
    big = (unsigned char *)(SvPVX(bigstr));
    /* The value of pos we can stop at: */
    stop_pos = SvCUR(bigstr) - end_shift - (SvCUR(littlestr) - 1 - previous);
    if (previous + start_shift > stop_pos) return Nullch;
    while (pos < previous + start_shift) {
	if (!(pos += PL_screamnext[pos]))
	    return Nullch;
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
    return (last && found) ? (char *)(big+(*old_posp)) : Nullch;
#endif /* POINTERRIGOR */
}

I32
ibcmp(char *s1, char *s2, register I32 len)
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    while (len--) {
	if (*a != *b && *a != fold[*b])
	    return 1;
	a++,b++;
    }
    return 0;
}

I32
ibcmp_locale(char *s1, char *s2, register I32 len)
{
    register U8 *a = (U8 *)s1;
    register U8 *b = (U8 *)s2;
    while (len--) {
	if (*a != *b && *a != fold_locale[*b])
	    return 1;
	a++,b++;
    }
    return 0;
}

/* copy a string to a safe spot */

char *
savepv(char *sv)
{
    register char *newaddr;

    New(902,newaddr,strlen(sv)+1,char);
    (void)strcpy(newaddr,sv);
    return newaddr;
}

/* same thing but with a known length */

char *
savepvn(char *sv, register I32 len)
{
    register char *newaddr;

    New(903,newaddr,len+1,char);
    Copy(sv,newaddr,len,char);		/* might not be null terminated */
    newaddr[len] = '\0';		/* is now */
    return newaddr;
}

/* the SV for form() and mess() is not kept in an arena */

STATIC SV *
mess_alloc(void)
{
    SV *sv;
    XPVMG *any;

    /* Create as PVMG now, to avoid any upgrading later */
    New(905, sv, 1, SV);
    Newz(905, any, 1, XPVMG);
    SvFLAGS(sv) = SVt_PVMG;
    SvANY(sv) = (void*)any;
    SvREFCNT(sv) = 1 << 30; /* practically infinite */
    return sv;
}

char *
form(const char* pat, ...)
{
    va_list args;
    va_start(args, pat);
    if (!PL_mess_sv)
	PL_mess_sv = mess_alloc();
    sv_vsetpvfn(PL_mess_sv, pat, strlen(pat), &args, Null(SV**), 0, Null(bool*));
    va_end(args);
    return SvPVX(PL_mess_sv);
}

char *
mess(const char *pat, va_list *args)
{
    SV *sv;
    static char dgd[] = " during global destruction.\n";

    if (!PL_mess_sv)
	PL_mess_sv = mess_alloc();
    sv = PL_mess_sv;
    sv_vsetpvfn(sv, pat, strlen(pat), args, Null(SV**), 0, Null(bool*));
    if (!SvCUR(sv) || *(SvEND(sv) - 1) != '\n') {
	dTHR;
	if (PL_dirty)
	    sv_catpv(sv, dgd);
	else {
	    if (PL_curcop->cop_line)
		sv_catpvf(sv, " at %_ line %ld",
			  GvSV(PL_curcop->cop_filegv), (long)PL_curcop->cop_line);
	    if (GvIO(PL_last_in_gv) && IoLINES(GvIOp(PL_last_in_gv))) {
		bool line_mode = (RsSIMPLE(PL_rs) &&
				  SvLEN(PL_rs) == 1 && *SvPVX(PL_rs) == '\n');
		sv_catpvf(sv, ", <%s> %s %ld",
			  PL_last_in_gv == PL_argvgv ? "" : GvNAME(PL_last_in_gv),
			  line_mode ? "line" : "chunk", 
			  (long)IoLINES(GvIOp(PL_last_in_gv)));
	    }
	    sv_catpv(sv, ".\n");
	}
    }
    return SvPVX(sv);
}

OP *
die(const char* pat, ...)
{
    dTHR;
    va_list args;
    char *message;
    int was_in_eval = PL_in_eval;
    HV *stash;
    GV *gv;
    CV *cv;

    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			  "%p: die: curstack = %p, mainstack = %p\n",
			  thr, PL_curstack, PL_mainstack));

    va_start(args, pat);
    message = pat ? mess(pat, &args) : Nullch;
    va_end(args);

    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			  "%p: die: message = %s\ndiehook = %p\n",
			  thr, message, PL_diehook));
    if (PL_diehook) {
	/* sv_2cv might call croak() */
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
	    if(message) {
		msg = newSVpv(message, 0);
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
	    perl_call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	}
    }

    PL_restartop = die_where(message);
    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
	  "%p: die: restartop = %p, was_in_eval = %d, top_env = %p\n",
	  thr, PL_restartop, was_in_eval, PL_top_env));
    if ((!PL_restartop && was_in_eval) || PL_top_env->je_prev)
	JMPENV_JUMP(3);
    return PL_restartop;
}

void
croak(const char* pat, ...)
{
    dTHR;
    va_list args;
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;

    va_start(args, pat);
    message = mess(pat, &args);
    va_end(args);
    DEBUG_S(PerlIO_printf(PerlIO_stderr(), "croak: 0x%lx %s", (unsigned long) thr, message));
    if (PL_diehook) {
	/* sv_2cv might call croak() */
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
	    msg = newSVpv(message, 0);
	    SvREADONLY_on(msg);
	    SAVEFREESV(msg);

	    PUSHSTACKi(PERLSI_DIEHOOK);
	    PUSHMARK(SP);
	    XPUSHs(msg);
	    PUTBACK;
	    perl_call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	}
    }
    if (PL_in_eval) {
	PL_restartop = die_where(message);
	JMPENV_JUMP(3);
    }
    PerlIO_puts(PerlIO_stderr(),message);
    (void)PerlIO_flush(PerlIO_stderr());
    my_failure_exit();
}

void
warn(const char* pat,...)
{
    va_list args;
    char *message;
    HV *stash;
    GV *gv;
    CV *cv;

    va_start(args, pat);
    message = mess(pat, &args);
    va_end(args);

    if (PL_warnhook) {
	/* sv_2cv might call warn() */
	dTHR;
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
	    msg = newSVpv(message, 0);
	    SvREADONLY_on(msg);
	    SAVEFREESV(msg);

	    PUSHSTACKi(PERLSI_WARNHOOK);
	    PUSHMARK(SP);
	    XPUSHs(msg);
	    PUTBACK;
	    perl_call_sv((SV*)cv, G_DISCARD);
	    POPSTACK;
	    LEAVE;
	    return;
	}
    }
    PerlIO_puts(PerlIO_stderr(),message);
#ifdef LEAKTEST
    DEBUG_L(*message == '!' 
	    ? (xstat(message[1]=='!'
		     ? (message[2]=='!' ? 2 : 1)
		     : 0)
	       , 0)
	    : 0);
#endif
    (void)PerlIO_flush(PerlIO_stderr());
}

#ifndef VMS  /* VMS' my_setenv() is in VMS.c */
#ifndef WIN32
void
my_setenv(char *nam, char *val)
{
    register I32 i=setenv_getix(nam);		/* where does it go? */

    if (environ == PL_origenviron) {	/* need we copy environment? */
	I32 j;
	I32 max;
	char **tmpenv;

	/*SUPPRESS 530*/
	for (max = i; environ[max]; max++) ;
	New(901,tmpenv, max+2, char*);
	for (j=0; j<max; j++)		/* copy environment */
	    tmpenv[j] = savepv(environ[j]);
	tmpenv[max] = Nullch;
	environ = tmpenv;		/* tell exec where it is now */
    }
    if (!val) {
	Safefree(environ[i]);
	while (environ[i]) {
	    environ[i] = environ[i+1];
	    i++;
	}
	return;
    }
    if (!environ[i]) {			/* does not exist yet */
	Renew(environ, i+2, char*);	/* just expand it a bit */
	environ[i+1] = Nullch;	/* make sure it's null terminated */
    }
    else
	Safefree(environ[i]);
    New(904, environ[i], strlen(nam) + strlen(val) + 2, char);
#ifndef MSDOS
    (void)sprintf(environ[i],"%s=%s",nam,val);/* all that work just for this */
#else
    /* MS-DOS requires environment variable names to be in uppercase */
    /* [Tom Dinger, 27 August 1990: Well, it doesn't _require_ it, but
     * some utilities and applications may break because they only look
     * for upper case strings. (Fixed strupr() bug here.)]
     */
    strcpy(environ[i],nam); strupr(environ[i]);
    (void)sprintf(environ[i] + strlen(nam),"=%s",val);
#endif /* MSDOS */
}

#else /* if WIN32 */

void
my_setenv(char *nam,char *val)
{

#ifdef USE_WIN32_RTL_ENV

    register char *envstr;
    STRLEN namlen = strlen(nam);
    STRLEN vallen;
    char *oldstr = environ[setenv_getix(nam)];

    /* putenv() has totally broken semantics in both the Borland
     * and Microsoft CRTLs.  They either store the passed pointer in
     * the environment without making a copy, or make a copy and don't
     * free it. And on top of that, they dont free() old entries that
     * are being replaced/deleted.  This means the caller must
     * free any old entries somehow, or we end up with a memory
     * leak every time my_setenv() is called.  One might think
     * one could directly manipulate environ[], like the UNIX code
     * above, but direct changes to environ are not allowed when
     * calling putenv(), since the RTLs maintain an internal
     * *copy* of environ[]. Bad, bad, *bad* stink.
     * GSAR 97-06-07
     */

    if (!val) {
	if (!oldstr)
	    return;
	val = "";
	vallen = 0;
    }
    else
	vallen = strlen(val);
    New(904, envstr, namlen + vallen + 3, char);
    (void)sprintf(envstr,"%s=%s",nam,val);
    (void)PerlEnv_putenv(envstr);
    if (oldstr)
	Safefree(oldstr);
#ifdef _MSC_VER
    Safefree(envstr);		/* MSVCRT leaks without this */
#endif

#else /* !USE_WIN32_RTL_ENV */

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

#endif
}

#endif /* WIN32 */

I32
setenv_getix(char *nam)
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

#endif /* !VMS */

#ifdef UNLINK_ALL_VERSIONS
I32
unlnk(f)	/* unlink all versions of a file */
char *f;
{
    I32 i;

    for (i = 0; PerlLIO_unlink(f) >= 0; i++) ;
    return i ? 0 : -1;
}
#endif

#if !defined(HAS_BCOPY) || !defined(HAS_SAFE_BCOPY)
char *
my_bcopy(register char *from,register char *to,register I32 len)
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

#ifndef HAS_MEMSET
void *
my_memset(loc,ch,len)
register char *loc;
register I32 ch;
register I32 len;
{
    char *retval = loc;

    while (len--)
	*loc++ = ch;
    return retval;
}
#endif

#if !defined(HAS_BZERO) && !defined(HAS_MEMSET)
char *
my_bzero(loc,len)
register char *loc;
register I32 len;
{
    char *retval = loc;

    while (len--)
	*loc++ = 0;
    return retval;
}
#endif

#if !defined(HAS_MEMCMP) || !defined(HAS_SANE_MEMCMP)
I32
my_memcmp(s1,s2,len)
char *s1;
char *s2;
register I32 len;
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
vsprintf(dest, pat, args)
char *dest;
const char *pat;
char *args;
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
my_swap(short s)
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
my_htonl(long l)
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
    croak("Unknown BYTEORDER\n");
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
my_ntohl(long l)
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
    croak("Unknown BYTEORDER\n");
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
	name (n)						\
	register type n;					\
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
	name (n)						\
	register type n;					\
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
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS)
PerlIO *
my_popen(char *cmd, char *mode)
{
    int p[2];
    register I32 This, that;
    register I32 pid;
    SV *sv;
    I32 doexec = strNE(cmd,"-");

#ifdef OS2
    if (doexec) {
	return my_syspopen(cmd,mode);
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
    while ((pid = (doexec?vfork():fork())) < 0) {
	if (errno != EAGAIN) {
	    PerlLIO_close(p[This]);
	    if (!doexec)
		croak("Can't fork");
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
	if (p[THIS] != (*mode == 'r')) {
	    PerlLIO_dup2(p[THIS], *mode == 'r');
	    PerlLIO_close(p[THIS]);
	}
	if (doexec) {
#if !defined(HAS_FCNTL) || !defined(F_SETFD)
	    int fd;

#ifndef NOFILE
#define NOFILE 20
#endif
	    for (fd = PL_maxsysfd + 1; fd < NOFILE; fd++)
		PerlLIO_close(fd);
#endif
	    do_exec(cmd);	/* may or may not use the shell */
	    PerlProc__exit(1);
	}
	/*SUPPRESS 560*/
	if (tmpgv = gv_fetchpv("$",TRUE, SVt_PV))
	    sv_setiv(GvSV(tmpgv), (IV)getpid());
	PL_forkprocess = 0;
	hv_clear(PL_pidstatus);	/* we have no children */
	return Nullfp;
#undef THIS
#undef THAT
    }
    do_execfree();	/* free any memory malloced by child on vfork */
    PerlLIO_close(p[that]);
    if (p[that] < p[This]) {
	PerlLIO_dup2(p[This], p[that]);
	PerlLIO_close(p[This]);
	p[This] = p[that];
    }
    sv = *av_fetch(PL_fdpid,p[This],TRUE);
    (void)SvUPGRADE(sv,SVt_IV);
    SvIVX(sv) = pid;
    PL_forkprocess = pid;
    return PerlIO_fdopen(p[This], mode);
}
#else
#if defined(atarist) || defined(DJGPP)
FILE *popen();
PerlIO *
my_popen(cmd,mode)
char	*cmd;
char	*mode;
{
    /* Needs work for PerlIO ! */
    /* used 0 for 2nd parameter to PerlIO-exportFILE; apparently not used */
    return popen(PerlIO_exportFILE(cmd, 0), mode);
}
#endif

#endif /* !DOSISH */

#ifdef DUMP_FDS
void
dump_fds(char *s)
{
    int fd;
    struct stat tmpstatbuf;

    PerlIO_printf(PerlIO_stderr(),"%s", s);
    for (fd = 0; fd < 32; fd++) {
	if (PerlLIO_fstat(fd,&tmpstatbuf) >= 0)
	    PerlIO_printf(PerlIO_stderr()," %d",fd);
    }
    PerlIO_printf(PerlIO_stderr(),"\n");
}
#endif	/* DUMP_FDS */

#ifndef HAS_DUP2
int
dup2(oldfd,newfd)
int oldfd;
int newfd;
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
rsignal(int signo, Sighandler_t handler)
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
rsignal_state(int signo)
{
    struct sigaction oact;

    if (sigaction(signo, (struct sigaction *)NULL, &oact) == -1)
        return SIG_ERR;
    else
        return oact.sa_handler;
}

int
rsignal_save(int signo, Sighandler_t handler, Sigsave_t *save)
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
rsignal_restore(int signo, Sigsave_t *save)
{
    return sigaction(signo, save, (struct sigaction *)NULL);
}

#else /* !HAS_SIGACTION */

Sighandler_t
rsignal(int signo, Sighandler_t handler)
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
rsignal_state(int signo)
{
    Sighandler_t oldsig;

    sig_trapped = 0;
    oldsig = PerlProc_signal(signo, sig_trap);
    PerlProc_signal(signo, oldsig);
    if (sig_trapped)
        PerlProc_kill(getpid(), signo);
    return oldsig;
}

int
rsignal_save(int signo, Sighandler_t handler, Sigsave_t *save)
{
    *save = PerlProc_signal(signo, handler);
    return (*save == SIG_ERR) ? -1 : 0;
}

int
rsignal_restore(int signo, Sigsave_t *save)
{
    return (PerlProc_signal(signo, *save) == SIG_ERR) ? -1 : 0;
}

#endif /* !HAS_SIGACTION */

    /* VMS' my_pclose() is in VMS.c; same with OS/2 */
#if (!defined(DOSISH) || defined(HAS_FORK) || defined(AMIGAOS)) && !defined(VMS)
I32
my_pclose(PerlIO *ptr)
{
    Sigsave_t hstat, istat, qstat;
    int status;
    SV **svp;
    int pid;
    int pid2;
    bool close_failed;
    int saved_errno;
#ifdef VMS
    int saved_vaxc_errno;
#endif
#ifdef WIN32
    int saved_win32_errno;
#endif

    svp = av_fetch(PL_fdpid,PerlIO_fileno(ptr),TRUE);
    pid = (int)SvIVX(*svp);
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

#if  !defined(DOSISH) || defined(OS2) || defined(WIN32)
I32
wait4pid(int pid, int *statusp, int flags)
{
    SV *sv;
    SV** svp;
    char spid[TYPE_CHARS(int)];

    if (!pid)
	return -1;
    if (pid > 0) {
	sprintf(spid, "%d", pid);
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
	if (entry = hv_iternext(PL_pidstatus)) {
	    pid = atoi(hv_iterkey(entry,(I32*)statusp));
	    sv = hv_iterval(PL_pidstatus,entry);
	    *statusp = SvIVX(sv);
	    sprintf(spid, "%d", pid);
	    (void)hv_delete(PL_pidstatus,spid,strlen(spid),G_DISCARD);
	    return pid;
	}
    }
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
	    croak("Can't do waitpid with flags");
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
pidgone(int pid, int status)
{
    register SV *sv;
    char spid[TYPE_CHARS(int)];

    sprintf(spid, "%d", pid);
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
my_syspclose(ptr)
#else
I32
my_pclose(ptr)
#endif 
PerlIO *ptr;
{
    /* Needs work for PerlIO ! */
    FILE *f = PerlIO_findFILE(ptr);
    I32 result = pclose(f);
    PerlIO_releaseFILE(ptr,f);
    return result;
}
#endif

void
repeatcpy(register char *to, register char *from, I32 len, register I32 count)
{
    register I32 todo;
    register char *frombase = from;

    if (len == 1) {
	register char c = *from;
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

#ifndef CASTNEGFLOAT
U32
cast_ulong(f)
double f;
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
#endif

#ifndef CASTI32

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
cast_i32(f)
double f;
{
    if (f >= I32_MAX)
	return (I32) I32_MAX;
    if (f <= I32_MIN)
	return (I32) I32_MIN;
    return (I32) f;
}

IV
cast_iv(f)
double f;
{
    if (f >= IV_MAX)
	return (IV) IV_MAX;
    if (f <= IV_MIN)
	return (IV) IV_MIN;
    return (IV) f;
}

UV
cast_uv(f)
double f;
{
    if (f >= MY_UV_MAX)
	return (UV) MY_UV_MAX;
    return (UV) f;
}

#endif

#ifndef HAS_RENAME
I32
same_dirent(a,b)
char *a;
char *b;
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

UV
scan_oct(char *start, I32 len, I32 *retlen)
{
    register char *s = start;
    register UV retval = 0;
    bool overflowed = FALSE;

    while (len && *s >= '0' && *s <= '7') {
	register UV n = retval << 3;
	if (!overflowed && (n >> 3) != retval) {
	    warn("Integer overflow in octal number");
	    overflowed = TRUE;
	}
	retval = n | (*s++ - '0');
	len--;
    }
    if (PL_dowarn && len && (*s == '8' || *s == '9'))
	warn("Illegal octal digit ignored");
    *retlen = s - start;
    return retval;
}

UV
scan_hex(char *start, I32 len, I32 *retlen)
{
    register char *s = start;
    register UV retval = 0;
    bool overflowed = FALSE;
    char *tmp = s;
    register UV n;

    while (len-- && *s) {
	tmp = strchr((char *) PL_hexdigit, *s++);
	if (!tmp) {
	    if (*(s-1) == '_' || (*(s-1) == 'x' && retval == 0))
		continue;
	    else {
		--s;
		if (PL_dowarn)
		    warn("Illegal hex digit ignored");
		break;
	    }
	}
	n = retval << 4;
	if (!overflowed && (n >> 4) != retval) {
	    warn("Integer overflow in hex number");
	    overflowed = TRUE;
	}
	retval = n | ((tmp - PL_hexdigit) & 15);
    }
    *retlen = s - start;
    return retval;
}

char*
find_script(char *scriptname, bool dosearch, char **search_ext, I32 flags)
{
    dTHR;
    char *xfound = Nullch;
    char *xfailed = Nullch;
    char tmpbuf[512];
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

    if (dosearch && !strchr(scriptname, '/')
#ifdef DOSISH
		 && !strchr(scriptname, '\\')
#endif
		 && (s = PerlEnv_getenv("PATH"))) {
	bool seen_dot = 0;
	
	PL_bufend = s + strlen(s);
	while (s < PL_bufend) {
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
	    if (s < PL_bufend)
		s++;
	    if (len + 1 + strlen(scriptname) + MAX_EXT_LEN >= sizeof tmpbuf)
		continue;	/* don't search dir with too-long name */
	    if (len
#if defined(atarist) || defined(DOSISH)
		&& tmpbuf[len - 1] != '/'
		&& tmpbuf[len - 1] != '\\'
#endif
	       )
		tmpbuf[len++] = '/';
	    if (len == 2 && tmpbuf[0] == '.')
		seen_dot = 1;
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
#ifndef DOSISH
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
	        croak("Can't %s %s%s%s",
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


#ifdef USE_THREADS
#ifdef FAKE_THREADS
/* Very simplistic scheduler for now */
void
schedule(void)
{
    thr = thr->i.next_run;
}

void
perl_cond_init(cp)
perl_cond *cp;
{
    *cp = 0;
}

void
perl_cond_signal(cp)
perl_cond *cp;
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
perl_cond_broadcast(cp)
perl_cond *cp;
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
perl_cond_wait(cp)
perl_cond *cp;
{
    perl_cond cond;

    if (thr->i.next_run == thr)
	croak("panic: perl_cond_wait called by last runnable thread");
    
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

#ifdef OLD_PTHREADS_API
struct perl_thread *
getTHR _((void))
{
    pthread_addr_t t;

    if (pthread_getspecific(PL_thr_key, &t))
	croak("panic: pthread_getspecific");
    return (struct perl_thread *) t;
}
#endif /* OLD_PTHREADS_API */

MAGIC *
condpair_magic(SV *sv)
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
	LOCK_SV_MUTEX;
	mg = mg_find(sv, 'm');
	if (mg) {
	    /* someone else beat us to initialising it */
	    UNLOCK_SV_MUTEX;
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
	    UNLOCK_SV_MUTEX;
	    DEBUG_S(WITH_THR(PerlIO_printf(PerlIO_stderr(),
					   "%p: condpair_magic %p\n", thr, sv));)
	}
    }
    return mg;
}

/*
 * Make a new perl thread structure using t as a prototype. Some of the
 * fields for the new thread are copied from the prototype thread, t,
 * so t should not be running in perl at the time this function is
 * called. The use by ext/Thread/Thread.xs in core perl (where t is the
 * thread calling new_struct_thread) clearly satisfies this constraint.
 */
struct perl_thread *
new_struct_thread(struct perl_thread *t)
{
    struct perl_thread *thr;
    SV *sv;
    SV **svp;
    I32 i;

    sv = newSVpv("", 0);
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
#else
    Zero(thr, 1, struct perl_thread);
#endif

    thr->oursv = sv;
    init_stacks(ARGS);

    PL_curcop = &PL_compiling;
    thr->cvcache = newHV();
    thr->threadsv = newAV();
    thr->specific = newAV();
    thr->errsv = newSVpv("", 0);
    thr->errhv = newHV();
    thr->flags = THRf_R_JOINABLE;
    MUTEX_INIT(&thr->mutex);


    /* top_env needs to be non-zero. It points to an area
       in which longjmp() stuff is stored, as C callstack
       info there at least is thread specific this has to
       be per-thread. Otherwise a 'die' in a thread gives
       that thread the C stack of last thread to do an eval {}!
       See comments in scope.h    
       Initialize top entry (as in perl.c for main thread)
     */
    PL_start_env.je_prev = NULL;
    PL_start_env.je_ret = -1;
    PL_start_env.je_mustcatch = TRUE;
    PL_top_env  = &PL_start_env;

    PL_in_eval = FALSE;
    PL_restartop = 0;

    PL_statname = NEWSV(66,0);
    PL_maxscream = -1;
    PL_regcompp = FUNC_NAME_TO_PTR(pregcomp);
    PL_regexecp = FUNC_NAME_TO_PTR(regexec_flags);
    PL_regindent = 0;
    PL_reginterp_cnt = 0;
    PL_lastscream = Nullsv;
    PL_screamfirst = 0;
    PL_screamnext = 0;
    PL_reg_start_tmp = 0;
    PL_reg_start_tmpl = 0;

    /* parent thread's data needs to be locked while we make copy */
    MUTEX_LOCK(&t->mutex);

    PL_curcop = t->Tcurcop;       /* XXX As good a guess as any? */
    PL_defstash = t->Tdefstash;   /* XXX maybe these should */
    PL_curstash = t->Tcurstash;   /* always be set to main? */

    PL_tainted = t->Ttainted;
    PL_curpm = t->Tcurpm;         /* XXX No PMOP ref count */
    PL_nrs = newSVsv(t->Tnrs);
    PL_rs = SvREFCNT_inc(PL_nrs);
    PL_last_in_gv = Nullgv;
    PL_ofslen = t->Tofslen;
    PL_ofs = savepvn(t->Tofs, PL_ofslen);
    PL_defoutgv = (GV*)SvREFCNT_inc(t->Tdefoutgv);
    PL_chopset = t->Tchopset;
    PL_formtarget = newSVsv(t->Tformtarget);
    PL_bodytarget = newSVsv(t->Tbodytarget);
    PL_toptarget = newSVsv(t->Ttoptarget);

    /* Initialise all per-thread SVs that the template thread used */
    svp = AvARRAY(t->threadsv);
    for (i = 0; i <= AvFILLp(t->threadsv); i++, svp++) {
	if (*svp && *svp != &PL_sv_undef) {
	    SV *sv = newSVsv(*svp);
	    av_store(thr->threadsv, i, sv);
	    sv_magic(sv, 0, 0, &PL_threadsv_names[i], 1);
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
		"new_struct_thread: copied threadsv %d %p->%p\n",i, t, thr));
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
    init_thread_intern(thr);
#endif /* HAVE_THREAD_INTERN */
    return thr;
}
#endif /* USE_THREADS */

#ifdef HUGE_VAL
/*
 * This hack is to force load of "huge" support from libm.a
 * So it is in perl for (say) POSIX to use. 
 * Needed for SunOS with Sun's 'acc' for example.
 */
double 
Perl_huge(void)
{
 return HUGE_VAL;
}
#endif

#ifdef PERL_GLOBAL_STRUCT
struct perl_vars *
Perl_GetVars(void)
{
 return &PL_Vars;
}
#endif

char **
get_op_names(void)
{
 return op_name;
}

char **
get_op_descs(void)
{
 return op_desc;
}

char *
get_no_modify(void)
{
 return (char*)no_modify;
}

U32 *
get_opargs(void)
{
 return opargs;
}


SV **
get_specialsv_list(void)
{
 return PL_specialsv_list;
}


MGVTBL*
get_vtbl(int vtbl_id)
{
    MGVTBL* result = Null(MGVTBL*);

    switch(vtbl_id) {
    case want_vtbl_sv:
	result = &vtbl_sv;
	break;
    case want_vtbl_env:
	result = &vtbl_env;
	break;
    case want_vtbl_envelem:
	result = &vtbl_envelem;
	break;
    case want_vtbl_sig:
	result = &vtbl_sig;
	break;
    case want_vtbl_sigelem:
	result = &vtbl_sigelem;
	break;
    case want_vtbl_pack:
	result = &vtbl_pack;
	break;
    case want_vtbl_packelem:
	result = &vtbl_packelem;
	break;
    case want_vtbl_dbline:
	result = &vtbl_dbline;
	break;
    case want_vtbl_isa:
	result = &vtbl_isa;
	break;
    case want_vtbl_isaelem:
	result = &vtbl_isaelem;
	break;
    case want_vtbl_arylen:
	result = &vtbl_arylen;
	break;
    case want_vtbl_glob:
	result = &vtbl_glob;
	break;
    case want_vtbl_mglob:
	result = &vtbl_mglob;
	break;
    case want_vtbl_nkeys:
	result = &vtbl_nkeys;
	break;
    case want_vtbl_taint:
	result = &vtbl_taint;
	break;
    case want_vtbl_substr:
	result = &vtbl_substr;
	break;
    case want_vtbl_vec:
	result = &vtbl_vec;
	break;
    case want_vtbl_pos:
	result = &vtbl_pos;
	break;
    case want_vtbl_bm:
	result = &vtbl_bm;
	break;
    case want_vtbl_fm:
	result = &vtbl_fm;
	break;
    case want_vtbl_uvar:
	result = &vtbl_uvar;
	break;
#ifdef USE_THREADS
    case want_vtbl_mutex:
	result = &vtbl_mutex;
	break;
#endif
    case want_vtbl_defelem:
	result = &vtbl_defelem;
	break;
    case want_vtbl_regexp:
	result = &vtbl_regexp;
	break;
#ifdef USE_LOCALE_COLLATE
    case want_vtbl_collxfrm:
	result = &vtbl_collxfrm;
	break;
#endif
    case want_vtbl_amagic:
	result = &vtbl_amagic;
	break;
    case want_vtbl_amagicelem:
	result = &vtbl_amagicelem;
	break;
    }
    return result;
}

