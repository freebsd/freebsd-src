/*    regcomp.c
 */

/*
 * "A fair jaw-cracker dwarf-language must be."  --Samwise Gamgee
 */

/* NOTE: this is derived from Henry Spencer's regexp code, and should not
 * confused with the original package (see point 3 below).  Thanks, Henry!
 */

/* Additional note: this code is very heavily munged from Henry's version
 * in places.  In some spots I've traded clarity for efficiency, so don't
 * blame Henry for some of the lack of readability.
 */

/* The names of the functions have been changed from regcomp and
 * regexec to  pregcomp and pregexec in order to avoid conflicts
 * with the POSIX routines of the same names.
*/

#ifdef PERL_EXT_RE_BUILD
/* need to replace pregcomp et al, so enable that */
#  ifndef PERL_IN_XSUB_RE
#    define PERL_IN_XSUB_RE
#  endif
/* need access to debugger hooks */
#  if defined(PERL_EXT_RE_DEBUG) && !defined(DEBUGGING)
#    define DEBUGGING
#  endif
#endif

#ifdef PERL_IN_XSUB_RE
/* We *really* need to overwrite these symbols: */
#  define Perl_pregcomp my_regcomp
#  define Perl_regdump my_regdump
#  define Perl_regprop my_regprop
#  define Perl_pregfree my_regfree
#  define Perl_re_intuit_string my_re_intuit_string
/* *These* symbols are masked to allow static link. */
#  define Perl_regnext my_regnext
#  define Perl_save_re_context my_save_re_context
#  define Perl_reginitcolors my_reginitcolors 

#  define PERL_NO_GET_CONTEXT
#endif 

/*SUPPRESS 112*/
/*
 * pregcomp and pregexec -- regsub and regerror are not used in perl
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 *
 ****    Alterations to Henry's code are...
 ****
 ****    Copyright (c) 1991-2001, Larry Wall
 ****
 ****    You may distribute under the terms of either the GNU General Public
 ****    License or the Artistic License, as specified in the README file.

 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */
#include "EXTERN.h"
#define PERL_IN_REGCOMP_C
#include "perl.h"

#ifdef PERL_IN_XSUB_RE
#  if defined(PERL_CAPI) || defined(PERL_OBJECT)
#    include "XSUB.h"
#  endif
#else
#  include "INTERN.h"
#endif

#define REG_COMP_C
#include "regcomp.h"

#ifdef op
#undef op
#endif /* op */

#ifdef MSDOS
# if defined(BUGGY_MSC6)
 /* MSC 6.00A breaks on op/regexp.t test 85 unless we turn this off */
 # pragma optimize("a",off)
 /* But MSC 6.00A is happy with 'w', for aliases only across function calls*/
 # pragma optimize("w",on )
# endif /* BUGGY_MSC6 */
#endif /* MSDOS */

#ifndef STATIC
#define	STATIC	static
#endif

#define	ISMULT1(c)	((c) == '*' || (c) == '+' || (c) == '?')
#define	ISMULT2(s)	((*s) == '*' || (*s) == '+' || (*s) == '?' || \
	((*s) == '{' && regcurly(s)))

#ifdef SPSTART
#undef SPSTART		/* dratted cpp namespace... */
#endif
/*
 * Flags to be passed up and down.
 */
#define	WORST		0	/* Worst case. */
#define	HASWIDTH	0x1	/* Known to match non-null strings. */
#define	SIMPLE		0x2	/* Simple enough to be STAR/PLUS operand. */
#define	SPSTART		0x4	/* Starts with * or +. */
#define TRYAGAIN	0x8	/* Weeded out a declaration. */

/* Length of a variant. */

typedef struct scan_data_t {
    I32 len_min;
    I32 len_delta;
    I32 pos_min;
    I32 pos_delta;
    SV *last_found;
    I32 last_end;			/* min value, <0 unless valid. */
    I32 last_start_min;
    I32 last_start_max;
    SV **longest;			/* Either &l_fixed, or &l_float. */
    SV *longest_fixed;
    I32 offset_fixed;
    SV *longest_float;
    I32 offset_float_min;
    I32 offset_float_max;
    I32 flags;
    I32 whilem_c;
    I32 *last_closep;
    struct regnode_charclass_class *start_class;
} scan_data_t;

/*
 * Forward declarations for pregcomp()'s friends.
 */

static scan_data_t zero_scan_data = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
				      0, 0, 0, 0, 0, 0};

#define SF_BEFORE_EOL		(SF_BEFORE_SEOL|SF_BEFORE_MEOL)
#define SF_BEFORE_SEOL		0x1
#define SF_BEFORE_MEOL		0x2
#define SF_FIX_BEFORE_EOL	(SF_FIX_BEFORE_SEOL|SF_FIX_BEFORE_MEOL)
#define SF_FL_BEFORE_EOL	(SF_FL_BEFORE_SEOL|SF_FL_BEFORE_MEOL)

#ifdef NO_UNARY_PLUS
#  define SF_FIX_SHIFT_EOL	(0+2)
#  define SF_FL_SHIFT_EOL		(0+4)
#else
#  define SF_FIX_SHIFT_EOL	(+2)
#  define SF_FL_SHIFT_EOL		(+4)
#endif

#define SF_FIX_BEFORE_SEOL	(SF_BEFORE_SEOL << SF_FIX_SHIFT_EOL)
#define SF_FIX_BEFORE_MEOL	(SF_BEFORE_MEOL << SF_FIX_SHIFT_EOL)

#define SF_FL_BEFORE_SEOL	(SF_BEFORE_SEOL << SF_FL_SHIFT_EOL)
#define SF_FL_BEFORE_MEOL	(SF_BEFORE_MEOL << SF_FL_SHIFT_EOL) /* 0x20 */
#define SF_IS_INF		0x40
#define SF_HAS_PAR		0x80
#define SF_IN_PAR		0x100
#define SF_HAS_EVAL		0x200
#define SCF_DO_SUBSTR		0x400
#define SCF_DO_STCLASS_AND	0x0800
#define SCF_DO_STCLASS_OR	0x1000
#define SCF_DO_STCLASS		(SCF_DO_STCLASS_AND|SCF_DO_STCLASS_OR)
#define SCF_WHILEM_VISITED_POS	0x2000

#define RF_utf8		8
#define UTF (PL_reg_flags & RF_utf8)
#define LOC (PL_regflags & PMf_LOCALE)
#define FOLD (PL_regflags & PMf_FOLD)

#define OOB_CHAR8		1234
#define OOB_UTF8		123456
#define OOB_NAMEDCLASS		-1

#define CHR_SVLEN(sv) (UTF ? sv_len_utf8(sv) : SvCUR(sv))
#define CHR_DIST(a,b) (UTF ? utf8_distance(a,b) : a - b)


/* length of regex to show in messages that don't mark a position within */
#define RegexLengthToShowInErrorMessages 127

/*
 * If MARKER[12] are adjusted, be sure to adjust the constants at the top
 * of t/op/regmesg.t, the tests in t/op/re_tests, and those in
 * op/pragma/warn/regcomp.
 */
#define MARKER1 "HERE"      /* marker as it appears in the description */
#define MARKER2 " << HERE "  /* marker as it appears within the regex */
   
#define REPORT_LOCATION " before " MARKER1 " mark in regex m/%.*s" MARKER2 "%s/"

/*
 * Calls SAVEDESTRUCTOR_X if needed, then calls Perl_croak with the given
 * arg. Show regex, up to a maximum length. If it's too long, chop and add
 * "...".
 */
#define	FAIL(msg)                                                             \
    STMT_START {                                                             \
        char *ellipses = "";                                                 \
        unsigned len = strlen(PL_regprecomp);                                \
                                                                             \
	if (!SIZE_ONLY)                                                      \
	    SAVEDESTRUCTOR_X(clear_re,(void*)PL_regcomp_rx);                 \
                                                                             \
	if (len > RegexLengthToShowInErrorMessages) {                        \
            /* chop 10 shorter than the max, to ensure meaning of "..." */   \
	    len = RegexLengthToShowInErrorMessages - 10;                     \
	    ellipses = "...";                                                \
	}                                                                    \
	Perl_croak(aTHX_ "%s in regex m/%.*s%s/",                            \
		   msg, (int)len, PL_regprecomp, ellipses);                  \
    } STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then calls Perl_croak with the given
 * args. Show regex, up to a maximum length. If it's too long, chop and add
 * "...".
 */
#define	FAIL2(pat,msg)                                                        \
    STMT_START {                                                             \
        char *ellipses = "";                                                 \
        unsigned len = strlen(PL_regprecomp);                                \
                                                                             \
	if (!SIZE_ONLY)                                                      \
	    SAVEDESTRUCTOR_X(clear_re,(void*)PL_regcomp_rx);                 \
                                                                             \
	if (len > RegexLengthToShowInErrorMessages) {                        \
            /* chop 10 shorter than the max, to ensure meaning of "..." */   \
	    len = RegexLengthToShowInErrorMessages - 10;                     \
	    ellipses = "...";                                                \
	}                                                                    \
	S_re_croak2(aTHX_ pat, " in regex m/%.*s%s/",                        \
		    msg, (int)len, PL_regprecomp, ellipses);                \
    } STMT_END


/*
 * Simple_vFAIL -- like FAIL, but marks the current location in the scan
 */
#define	Simple_vFAIL(m)                                                      \
    STMT_START {                                                             \
      unsigned offset = strlen(PL_regprecomp)-(PL_regxend-PL_regcomp_parse); \
                                                                             \
      Perl_croak(aTHX_ "%s" REPORT_LOCATION,               \
		 m, (int)offset, PL_regprecomp, PL_regprecomp + offset);     \
    } STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL()
 */
#define	vFAIL(m)                                                             \
    STMT_START {                                                             \
      if (!SIZE_ONLY)                                                        \
	    SAVEDESTRUCTOR_X(clear_re,(void*)PL_regcomp_rx);                 \
      Simple_vFAIL(m);                                                       \
    } STMT_END

/*
 * Like Simple_vFAIL(), but accepts two arguments.
 */
#define	Simple_vFAIL2(m,a1)                                                  \
    STMT_START {                                                             \
      unsigned offset = strlen(PL_regprecomp)-(PL_regxend-PL_regcomp_parse); \
                                                                             \
      S_re_croak2(aTHX_ m, REPORT_LOCATION, a1,       \
		  (int)offset, PL_regprecomp, PL_regprecomp + offset);       \
    } STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL2().
 */
#define	vFAIL2(m,a1)                                                         \
    STMT_START {                                                             \
      if (!SIZE_ONLY)                                                        \
	    SAVEDESTRUCTOR_X(clear_re,(void*)PL_regcomp_rx);                 \
      Simple_vFAIL2(m, a1);                                                  \
    } STMT_END


/*
 * Like Simple_vFAIL(), but accepts three arguments.
 */
#define	Simple_vFAIL3(m, a1, a2)                                             \
    STMT_START {                                                             \
      unsigned offset = strlen(PL_regprecomp)-(PL_regxend-PL_regcomp_parse); \
                                                                             \
      S_re_croak2(aTHX_ m, REPORT_LOCATION, a1, a2,   \
		  (int)offset, PL_regprecomp, PL_regprecomp + offset);       \
    } STMT_END

/*
 * Calls SAVEDESTRUCTOR_X if needed, then Simple_vFAIL3().
 */
#define	vFAIL3(m,a1,a2)                                                      \
    STMT_START {                                                             \
      if (!SIZE_ONLY)                                                        \
	    SAVEDESTRUCTOR_X(clear_re,(void*)PL_regcomp_rx);                 \
      Simple_vFAIL3(m, a1, a2);                                              \
    } STMT_END

/*
 * Like Simple_vFAIL(), but accepts four arguments.
 */
#define	Simple_vFAIL4(m, a1, a2, a3)                                         \
    STMT_START {                                                             \
      unsigned offset = strlen(PL_regprecomp)-(PL_regxend-PL_regcomp_parse); \
                                                                             \
      S_re_croak2(aTHX_ m, REPORT_LOCATION, a1, a2, a3,\
		  (int)offset, PL_regprecomp, PL_regprecomp + offset);       \
    } STMT_END

/*
 * Like Simple_vFAIL(), but accepts five arguments.
 */
#define	Simple_vFAIL5(m, a1, a2, a3, a4)                                     \
    STMT_START {                                                             \
      unsigned offset = strlen(PL_regprecomp)-(PL_regxend-PL_regcomp_parse); \
      S_re_croak2(aTHX_ m, REPORT_LOCATION, a1, a2, a3, a4,\
		  (int)offset, PL_regprecomp, PL_regprecomp + offset);       \
    } STMT_END


#define	vWARN(loc,m)                                                         \
    STMT_START {                                                             \
        unsigned offset = strlen(PL_regprecomp)-(PL_regxend-(loc));          \
	Perl_warner(aTHX_ WARN_REGEXP, "%s" REPORT_LOCATION,\
		 m, (int)offset, PL_regprecomp, PL_regprecomp + offset);          \
    } STMT_END                                                               \


#define	vWARN2(loc, m, a1)                                                   \
    STMT_START {                                                             \
        unsigned offset = strlen(PL_regprecomp)-(PL_regxend-(loc));          \
	Perl_warner(aTHX_ WARN_REGEXP, m REPORT_LOCATION,\
                 a1,                                                         \
		 (int)offset, PL_regprecomp, PL_regprecomp + offset);        \
    } STMT_END

#define	vWARN3(loc, m, a1, a2)                                               \
    STMT_START {                                                             \
      unsigned offset = strlen(PL_regprecomp) - (PL_regxend - (loc));        \
	Perl_warner(aTHX_ WARN_REGEXP, m REPORT_LOCATION,                    \
                 a1, a2,                                                     \
		 (int)offset, PL_regprecomp, PL_regprecomp + offset);        \
    } STMT_END

#define	vWARN4(loc, m, a1, a2, a3)                                           \
    STMT_START {                                                             \
      unsigned offset = strlen(PL_regprecomp)-(PL_regxend-(loc));            \
	Perl_warner(aTHX_ WARN_REGEXP, m REPORT_LOCATION,\
                 a1, a2, a3,                                                 \
		 (int)offset, PL_regprecomp, PL_regprecomp + offset);        \
    } STMT_END



/* Allow for side effects in s */
#define REGC(c,s) STMT_START { if (!SIZE_ONLY) *(s) = (c); else (s);} STMT_END

static void clear_re(pTHXo_ void *r);

/* Mark that we cannot extend a found fixed substring at this point.
   Updata the longest found anchored substring and the longest found
   floating substrings if needed. */

STATIC void
S_scan_commit(pTHX_ scan_data_t *data)
{
    STRLEN l = CHR_SVLEN(data->last_found);
    STRLEN old_l = CHR_SVLEN(*data->longest);
    
    if ((l >= old_l) && ((l > old_l) || (data->flags & SF_BEFORE_EOL))) {
	sv_setsv(*data->longest, data->last_found);
	if (*data->longest == data->longest_fixed) {
	    data->offset_fixed = l ? data->last_start_min : data->pos_min;
	    if (data->flags & SF_BEFORE_EOL)
		data->flags 
		    |= ((data->flags & SF_BEFORE_EOL) << SF_FIX_SHIFT_EOL);
	    else
		data->flags &= ~SF_FIX_BEFORE_EOL;
	}
	else {
	    data->offset_float_min = l ? data->last_start_min : data->pos_min;
	    data->offset_float_max = (l 
				      ? data->last_start_max 
				      : data->pos_min + data->pos_delta);
	    if (data->flags & SF_BEFORE_EOL)
		data->flags 
		    |= ((data->flags & SF_BEFORE_EOL) << SF_FL_SHIFT_EOL);
	    else
		data->flags &= ~SF_FL_BEFORE_EOL;
	}
    }
    SvCUR_set(data->last_found, 0);
    data->last_end = -1;
    data->flags &= ~SF_BEFORE_EOL;
}

/* Can match anything (initialization) */
STATIC void
S_cl_anything(pTHX_ struct regnode_charclass_class *cl)
{
    int value;

    ANYOF_CLASS_ZERO(cl);
    for (value = 0; value < 256; ++value)
	ANYOF_BITMAP_SET(cl, value);
    cl->flags = ANYOF_EOS;
    if (LOC)
	cl->flags |= ANYOF_LOCALE;
}

/* Can match anything (initialization) */
STATIC int
S_cl_is_anything(pTHX_ struct regnode_charclass_class *cl)
{
    int value;

    for (value = 0; value <= ANYOF_MAX; value += 2)
	if (ANYOF_CLASS_TEST(cl, value) && ANYOF_CLASS_TEST(cl, value + 1))
	    return 1;
    for (value = 0; value < 256; ++value)
	if (!ANYOF_BITMAP_TEST(cl, value))
	    return 0;
    return 1;
}

/* Can match anything (initialization) */
STATIC void
S_cl_init(pTHX_ struct regnode_charclass_class *cl)
{
    Zero(cl, 1, struct regnode_charclass_class);
    cl->type = ANYOF;
    cl_anything(cl);
}

STATIC void
S_cl_init_zero(pTHX_ struct regnode_charclass_class *cl)
{
    Zero(cl, 1, struct regnode_charclass_class);
    cl->type = ANYOF;
    cl_anything(cl);
    if (LOC)
	cl->flags |= ANYOF_LOCALE;
}

/* 'And' a given class with another one.  Can create false positives */
/* We assume that cl is not inverted */
STATIC void
S_cl_and(pTHX_ struct regnode_charclass_class *cl,
	 struct regnode_charclass_class *and_with)
{
    if (!(and_with->flags & ANYOF_CLASS)
	&& !(cl->flags & ANYOF_CLASS)
	&& (and_with->flags & ANYOF_LOCALE) == (cl->flags & ANYOF_LOCALE)
	&& !(and_with->flags & ANYOF_FOLD)
	&& !(cl->flags & ANYOF_FOLD)) {
	int i;

	if (and_with->flags & ANYOF_INVERT)
	    for (i = 0; i < ANYOF_BITMAP_SIZE; i++)
		cl->bitmap[i] &= ~and_with->bitmap[i];
	else
	    for (i = 0; i < ANYOF_BITMAP_SIZE; i++)
		cl->bitmap[i] &= and_with->bitmap[i];
    } /* XXXX: logic is complicated otherwise, leave it along for a moment. */
    if (!(and_with->flags & ANYOF_EOS))
	cl->flags &= ~ANYOF_EOS;
}

/* 'OR' a given class with another one.  Can create false positives */
/* We assume that cl is not inverted */
STATIC void
S_cl_or(pTHX_ struct regnode_charclass_class *cl, struct regnode_charclass_class *or_with)
{
    if (or_with->flags & ANYOF_INVERT) {
	/* We do not use
	 * (B1 | CL1) | (!B2 & !CL2) = (B1 | !B2 & !CL2) | (CL1 | (!B2 & !CL2))
	 *   <= (B1 | !B2) | (CL1 | !CL2)
	 * which is wasteful if CL2 is small, but we ignore CL2:
	 *   (B1 | CL1) | (!B2 & !CL2) <= (B1 | CL1) | !B2 = (B1 | !B2) | CL1
	 * XXXX Can we handle case-fold?  Unclear:
	 *   (OK1(i) | OK1(i')) | !(OK1(i) | OK1(i')) =
	 *   (OK1(i) | OK1(i')) | (!OK1(i) & !OK1(i'))
	 */
	if ( (or_with->flags & ANYOF_LOCALE) == (cl->flags & ANYOF_LOCALE)
	     && !(or_with->flags & ANYOF_FOLD)
	     && !(cl->flags & ANYOF_FOLD) ) {
	    int i;

	    for (i = 0; i < ANYOF_BITMAP_SIZE; i++)
		cl->bitmap[i] |= ~or_with->bitmap[i];
	} /* XXXX: logic is complicated otherwise */
	else {
	    cl_anything(cl);
	}
    } else {
	/* (B1 | CL1) | (B2 | CL2) = (B1 | B2) | (CL1 | CL2)) */
	if ( (or_with->flags & ANYOF_LOCALE) == (cl->flags & ANYOF_LOCALE)
	     && (!(or_with->flags & ANYOF_FOLD) 
		 || (cl->flags & ANYOF_FOLD)) ) {
	    int i;

	    /* OR char bitmap and class bitmap separately */
	    for (i = 0; i < ANYOF_BITMAP_SIZE; i++)
		cl->bitmap[i] |= or_with->bitmap[i];
	    if (or_with->flags & ANYOF_CLASS) {
		for (i = 0; i < ANYOF_CLASSBITMAP_SIZE; i++)
		    cl->classflags[i] |= or_with->classflags[i];
		cl->flags |= ANYOF_CLASS;
	    }
	}
	else { /* XXXX: logic is complicated, leave it along for a moment. */
	    cl_anything(cl);
	}
    }
    if (or_with->flags & ANYOF_EOS)
	cl->flags |= ANYOF_EOS;
}

/* REx optimizer.  Converts nodes into quickier variants "in place".
   Finds fixed substrings.  */

/* Stops at toplevel WHILEM as well as at `last'. At end *scanp is set
   to the position after last scanned or to NULL. */

STATIC I32
S_study_chunk(pTHX_ regnode **scanp, I32 *deltap, regnode *last, scan_data_t *data, U32 flags)
			/* scanp: Start here (read-write). */
			/* deltap: Write maxlen-minlen here. */
			/* last: Stop before this one. */
{
    I32 min = 0, pars = 0, code;
    regnode *scan = *scanp, *next;
    I32 delta = 0;
    int is_inf = (flags & SCF_DO_SUBSTR) && (data->flags & SF_IS_INF);
    int is_inf_internal = 0;		/* The studied chunk is infinite */
    I32 is_par = OP(scan) == OPEN ? ARG(scan) : 0;
    scan_data_t data_fake;
    struct regnode_charclass_class and_with; /* Valid if flags & SCF_DO_STCLASS_OR */
    
    while (scan && OP(scan) != END && scan < last) {
	/* Peephole optimizer: */

	if (PL_regkind[(U8)OP(scan)] == EXACT) {
	    /* Merge several consecutive EXACTish nodes into one. */
	    regnode *n = regnext(scan);
	    U32 stringok = 1;
#ifdef DEBUGGING
	    regnode *stop = scan;
#endif 

	    next = scan + NODE_SZ_STR(scan);
	    /* Skip NOTHING, merge EXACT*. */
	    while (n &&
		   ( PL_regkind[(U8)OP(n)] == NOTHING || 
		     (stringok && (OP(n) == OP(scan))))
		   && NEXT_OFF(n)
		   && NEXT_OFF(scan) + NEXT_OFF(n) < I16_MAX) {
		if (OP(n) == TAIL || n > next)
		    stringok = 0;
		if (PL_regkind[(U8)OP(n)] == NOTHING) {
		    NEXT_OFF(scan) += NEXT_OFF(n);
		    next = n + NODE_STEP_REGNODE;
#ifdef DEBUGGING
		    if (stringok)
			stop = n;
#endif 
		    n = regnext(n);
		}
		else if (stringok) {
		    int oldl = STR_LEN(scan);
		    regnode *nnext = regnext(n);

		    if (oldl + STR_LEN(n) > U8_MAX) 
			break;
		    NEXT_OFF(scan) += NEXT_OFF(n);
		    STR_LEN(scan) += STR_LEN(n);
		    next = n + NODE_SZ_STR(n);
		    /* Now we can overwrite *n : */
		    Move(STRING(n), STRING(scan) + oldl, STR_LEN(n), char);
#ifdef DEBUGGING
		    stop = next - 1;
#endif 
		    n = nnext;
		}
	    }
#ifdef DEBUGGING
	    /* Allow dumping */
	    n = scan + NODE_SZ_STR(scan);
	    while (n <= stop) {
		if (PL_regkind[(U8)OP(n)] != NOTHING || OP(n) == NOTHING) {
		    OP(n) = OPTIMIZED;
		    NEXT_OFF(n) = 0;
		}
		n++;
	    }
#endif
	}
	/* Follow the next-chain of the current node and optimize
	   away all the NOTHINGs from it.  */
	if (OP(scan) != CURLYX) {
	    int max = (reg_off_by_arg[OP(scan)]
		       ? I32_MAX
		       /* I32 may be smaller than U16 on CRAYs! */
		       : (I32_MAX < U16_MAX ? I32_MAX : U16_MAX));
	    int off = (reg_off_by_arg[OP(scan)] ? ARG(scan) : NEXT_OFF(scan));
	    int noff;
	    regnode *n = scan;
	    
	    /* Skip NOTHING and LONGJMP. */
	    while ((n = regnext(n))
		   && ((PL_regkind[(U8)OP(n)] == NOTHING && (noff = NEXT_OFF(n)))
		       || ((OP(n) == LONGJMP) && (noff = ARG(n))))
		   && off + noff < max)
		off += noff;
	    if (reg_off_by_arg[OP(scan)])
		ARG(scan) = off;
	    else 
		NEXT_OFF(scan) = off;
	}
	/* The principal pseudo-switch.  Cannot be a switch, since we
	   look into several different things.  */
	if (OP(scan) == BRANCH || OP(scan) == BRANCHJ 
		   || OP(scan) == IFTHEN || OP(scan) == SUSPEND) {
	    next = regnext(scan);
	    code = OP(scan);
	    
	    if (OP(next) == code || code == IFTHEN || code == SUSPEND) { 
		I32 max1 = 0, min1 = I32_MAX, num = 0;
		struct regnode_charclass_class accum;
		
		if (flags & SCF_DO_SUBSTR) /* XXXX Add !SUSPEND? */
		    scan_commit(data);	/* Cannot merge strings after this. */
		if (flags & SCF_DO_STCLASS)
		    cl_init_zero(&accum);
		while (OP(scan) == code) {
		    I32 deltanext, minnext, f = 0, fake = 0;
		    struct regnode_charclass_class this_class;

		    num++;
		    data_fake.flags = 0;
		    if (data) {		    
			data_fake.whilem_c = data->whilem_c;
			data_fake.last_closep = data->last_closep;
		    }
		    else
			data_fake.last_closep = &fake;
		    next = regnext(scan);
		    scan = NEXTOPER(scan);
		    if (code != BRANCH)
			scan = NEXTOPER(scan);
		    if (flags & SCF_DO_STCLASS) {
			cl_init(&this_class);
			data_fake.start_class = &this_class;
			f = SCF_DO_STCLASS_AND;
		    }		    
		    if (flags & SCF_WHILEM_VISITED_POS)
			f |= SCF_WHILEM_VISITED_POS;
		    /* we suppose the run is continuous, last=next...*/
		    minnext = study_chunk(&scan, &deltanext, next,
					  &data_fake, f);
		    if (min1 > minnext) 
			min1 = minnext;
		    if (max1 < minnext + deltanext)
			max1 = minnext + deltanext;
		    if (deltanext == I32_MAX)
			is_inf = is_inf_internal = 1;
		    scan = next;
		    if (data_fake.flags & (SF_HAS_PAR|SF_IN_PAR))
			pars++;
		    if (data && (data_fake.flags & SF_HAS_EVAL))
			data->flags |= SF_HAS_EVAL;
		    if (data)
			data->whilem_c = data_fake.whilem_c;
		    if (flags & SCF_DO_STCLASS)
			cl_or(&accum, &this_class);
		    if (code == SUSPEND) 
			break;
		}
		if (code == IFTHEN && num < 2) /* Empty ELSE branch */
		    min1 = 0;
		if (flags & SCF_DO_SUBSTR) {
		    data->pos_min += min1;
		    data->pos_delta += max1 - min1;
		    if (max1 != min1 || is_inf)
			data->longest = &(data->longest_float);
		}
		min += min1;
		delta += max1 - min1;
		if (flags & SCF_DO_STCLASS_OR) {
		    cl_or(data->start_class, &accum);
		    if (min1) {
			cl_and(data->start_class, &and_with);
			flags &= ~SCF_DO_STCLASS;
		    }
		}
		else if (flags & SCF_DO_STCLASS_AND) {
		    if (min1) {
			cl_and(data->start_class, &accum);
			flags &= ~SCF_DO_STCLASS;
		    }
		    else {
			/* Switch to OR mode: cache the old value of 
			 * data->start_class */
			StructCopy(data->start_class, &and_with,
				   struct regnode_charclass_class);
			flags &= ~SCF_DO_STCLASS_AND;
			StructCopy(&accum, data->start_class,
				   struct regnode_charclass_class);
			flags |= SCF_DO_STCLASS_OR;
			data->start_class->flags |= ANYOF_EOS;
		    }
		}
	    }
	    else if (code == BRANCHJ)	/* single branch is optimized. */
		scan = NEXTOPER(NEXTOPER(scan));
	    else			/* single branch is optimized. */
		scan = NEXTOPER(scan);
	    continue;
	}
	else if (OP(scan) == EXACT) {
	    I32 l = STR_LEN(scan);
	    if (UTF) {
		unsigned char *s = (unsigned char *)STRING(scan);
		unsigned char *e = s + l;
		I32 newl = 0;
		while (s < e) {
		    newl++;
		    s += UTF8SKIP(s);
		}
		l = newl;
	    }
	    min += l;
	    if (flags & SCF_DO_SUBSTR) { /* Update longest substr. */
		/* The code below prefers earlier match for fixed
		   offset, later match for variable offset.  */
		if (data->last_end == -1) { /* Update the start info. */
		    data->last_start_min = data->pos_min;
 		    data->last_start_max = is_inf
 			? I32_MAX : data->pos_min + data->pos_delta; 
		}
		sv_catpvn(data->last_found, STRING(scan), STR_LEN(scan));
		data->last_end = data->pos_min + l;
		data->pos_min += l; /* As in the first entry. */
		data->flags &= ~SF_BEFORE_EOL;
	    }
	    if (flags & SCF_DO_STCLASS_AND) {
		/* Check whether it is compatible with what we know already! */
		int compat = 1;

		if (!(data->start_class->flags & (ANYOF_CLASS | ANYOF_LOCALE)) 
		    && !ANYOF_BITMAP_TEST(data->start_class, *STRING(scan))
		    && (!(data->start_class->flags & ANYOF_FOLD)
			|| !ANYOF_BITMAP_TEST(data->start_class,
					      PL_fold[*(U8*)STRING(scan)])))
		    compat = 0;
		ANYOF_CLASS_ZERO(data->start_class);
		ANYOF_BITMAP_ZERO(data->start_class);
		if (compat)
		    ANYOF_BITMAP_SET(data->start_class, *STRING(scan));
		data->start_class->flags &= ~ANYOF_EOS;
	    }
	    else if (flags & SCF_DO_STCLASS_OR) {
		/* false positive possible if the class is case-folded */
		ANYOF_BITMAP_SET(data->start_class, *STRING(scan));	
		data->start_class->flags &= ~ANYOF_EOS;
		cl_and(data->start_class, &and_with);
	    }
	    flags &= ~SCF_DO_STCLASS;
	}
	else if (PL_regkind[(U8)OP(scan)] == EXACT) { /* But OP != EXACT! */
	    I32 l = STR_LEN(scan);

	    /* Search for fixed substrings supports EXACT only. */
	    if (flags & SCF_DO_SUBSTR) 
		scan_commit(data);
	    if (UTF) {
		unsigned char *s = (unsigned char *)STRING(scan);
		unsigned char *e = s + l;
		I32 newl = 0;
		while (s < e) {
		    newl++;
		    s += UTF8SKIP(s);
		}
		l = newl;
	    }
	    min += l;
	    if (data && (flags & SCF_DO_SUBSTR))
		data->pos_min += l;
	    if (flags & SCF_DO_STCLASS_AND) {
		/* Check whether it is compatible with what we know already! */
		int compat = 1;

		if (!(data->start_class->flags & (ANYOF_CLASS | ANYOF_LOCALE)) 
		    && !ANYOF_BITMAP_TEST(data->start_class, *STRING(scan))
		    && !ANYOF_BITMAP_TEST(data->start_class, 
					  PL_fold[*(U8*)STRING(scan)]))
		    compat = 0;
		ANYOF_CLASS_ZERO(data->start_class);
		ANYOF_BITMAP_ZERO(data->start_class);
		if (compat) {
		    ANYOF_BITMAP_SET(data->start_class, *STRING(scan));
		    data->start_class->flags &= ~ANYOF_EOS;
		    data->start_class->flags |= ANYOF_FOLD;
		    if (OP(scan) == EXACTFL)
			data->start_class->flags |= ANYOF_LOCALE;
		}
	    }
	    else if (flags & SCF_DO_STCLASS_OR) {
		if (data->start_class->flags & ANYOF_FOLD) {
		    /* false positive possible if the class is case-folded.
		       Assume that the locale settings are the same... */
		    ANYOF_BITMAP_SET(data->start_class, *STRING(scan));	
		    data->start_class->flags &= ~ANYOF_EOS;
		}
		cl_and(data->start_class, &and_with);
	    }
	    flags &= ~SCF_DO_STCLASS;
	}
	else if (strchr((char*)PL_varies,OP(scan))) {
	    I32 mincount, maxcount, minnext, deltanext, fl;
	    I32 f = flags, pos_before = 0;
	    regnode *oscan = scan;
	    struct regnode_charclass_class this_class;
	    struct regnode_charclass_class *oclass = NULL;

	    switch (PL_regkind[(U8)OP(scan)]) {
	    case WHILEM:		/* End of (?:...)* . */
		scan = NEXTOPER(scan);
		goto finish;
	    case PLUS:
		if (flags & (SCF_DO_SUBSTR | SCF_DO_STCLASS)) {
		    next = NEXTOPER(scan);
		    if (OP(next) == EXACT || (flags & SCF_DO_STCLASS)) {
			mincount = 1; 
			maxcount = REG_INFTY; 
			next = regnext(scan);
			scan = NEXTOPER(scan);
			goto do_curly;
		    }
		}
		if (flags & SCF_DO_SUBSTR)
		    data->pos_min++;
		min++;
		/* Fall through. */
	    case STAR:
		if (flags & SCF_DO_STCLASS) {
		    mincount = 0;
		    maxcount = REG_INFTY; 
		    next = regnext(scan);
		    scan = NEXTOPER(scan);
		    goto do_curly;
		}
		is_inf = is_inf_internal = 1; 
		scan = regnext(scan);
		if (flags & SCF_DO_SUBSTR) {
		    scan_commit(data);	/* Cannot extend fixed substrings */
		    data->longest = &(data->longest_float);
		}
		goto optimize_curly_tail;
	    case CURLY:
		mincount = ARG1(scan); 
		maxcount = ARG2(scan);
		next = regnext(scan);
		if (OP(scan) == CURLYX) {
		    I32 lp = (data ? *(data->last_closep) : 0);

		    scan->flags = ((lp <= U8_MAX) ? lp : U8_MAX);
		}
		scan = NEXTOPER(scan) + EXTRA_STEP_2ARGS;
	      do_curly:
		if (flags & SCF_DO_SUBSTR) {
		    if (mincount == 0) scan_commit(data); /* Cannot extend fixed substrings */
		    pos_before = data->pos_min;
		}
		if (data) {
		    fl = data->flags;
		    data->flags &= ~(SF_HAS_PAR|SF_IN_PAR|SF_HAS_EVAL);
		    if (is_inf)
			data->flags |= SF_IS_INF;
		}
		if (flags & SCF_DO_STCLASS) {
		    cl_init(&this_class);
		    oclass = data->start_class;
		    data->start_class = &this_class;
		    f |= SCF_DO_STCLASS_AND;
		    f &= ~SCF_DO_STCLASS_OR;
		}
		/* These are the cases when once a subexpression
		   fails at a particular position, it cannot succeed
		   even after backtracking at the enclosing scope.
		   
		   XXXX what if minimal match and we are at the
		        initial run of {n,m}? */
		if ((mincount != maxcount - 1) && (maxcount != REG_INFTY))
		    f &= ~SCF_WHILEM_VISITED_POS;

		/* This will finish on WHILEM, setting scan, or on NULL: */
		minnext = study_chunk(&scan, &deltanext, last, data, 
				      mincount == 0 
					? (f & ~SCF_DO_SUBSTR) : f);

		if (flags & SCF_DO_STCLASS)
		    data->start_class = oclass;
		if (mincount == 0 || minnext == 0) {
		    if (flags & SCF_DO_STCLASS_OR) {
			cl_or(data->start_class, &this_class);
		    }
		    else if (flags & SCF_DO_STCLASS_AND) {
			/* Switch to OR mode: cache the old value of 
			 * data->start_class */
			StructCopy(data->start_class, &and_with,
				   struct regnode_charclass_class);
			flags &= ~SCF_DO_STCLASS_AND;
			StructCopy(&this_class, data->start_class,
				   struct regnode_charclass_class);
			flags |= SCF_DO_STCLASS_OR;
			data->start_class->flags |= ANYOF_EOS;
		    }
		} else {		/* Non-zero len */
		    if (flags & SCF_DO_STCLASS_OR) {
			cl_or(data->start_class, &this_class);
			cl_and(data->start_class, &and_with);
		    }
		    else if (flags & SCF_DO_STCLASS_AND)
			cl_and(data->start_class, &this_class);
		    flags &= ~SCF_DO_STCLASS;
		}
		if (!scan) 		/* It was not CURLYX, but CURLY. */
		    scan = next;
		if (ckWARN(WARN_REGEXP) && (minnext + deltanext == 0) 
		    && !(data->flags & (SF_HAS_PAR|SF_IN_PAR))
		    && maxcount <= REG_INFTY/3) /* Complement check for big count */
		{
		    vWARN(PL_regcomp_parse,
			  "Quantifier unexpected on zero-length expression");
		}

		min += minnext * mincount;
		is_inf_internal |= ((maxcount == REG_INFTY 
				     && (minnext + deltanext) > 0)
				    || deltanext == I32_MAX);
		is_inf |= is_inf_internal;
		delta += (minnext + deltanext) * maxcount - minnext * mincount;

		/* Try powerful optimization CURLYX => CURLYN. */
		if (  OP(oscan) == CURLYX && data 
		      && data->flags & SF_IN_PAR
		      && !(data->flags & SF_HAS_EVAL)
		      && !deltanext && minnext == 1 ) {
		    /* Try to optimize to CURLYN.  */
		    regnode *nxt = NEXTOPER(oscan) + EXTRA_STEP_2ARGS;
		    regnode *nxt1 = nxt, *nxt2;

		    /* Skip open. */
		    nxt = regnext(nxt);
		    if (!strchr((char*)PL_simple,OP(nxt))
			&& !(PL_regkind[(U8)OP(nxt)] == EXACT
			     && STR_LEN(nxt) == 1)) 
			goto nogo;
		    nxt2 = nxt;
		    nxt = regnext(nxt);
		    if (OP(nxt) != CLOSE) 
			goto nogo;
		    /* Now we know that nxt2 is the only contents: */
		    oscan->flags = ARG(nxt);
		    OP(oscan) = CURLYN;
		    OP(nxt1) = NOTHING;	/* was OPEN. */
#ifdef DEBUGGING
		    OP(nxt1 + 1) = OPTIMIZED; /* was count. */
		    NEXT_OFF(nxt1+ 1) = 0; /* just for consistancy. */
		    NEXT_OFF(nxt2) = 0;	/* just for consistancy with CURLY. */
		    OP(nxt) = OPTIMIZED;	/* was CLOSE. */
		    OP(nxt + 1) = OPTIMIZED; /* was count. */
		    NEXT_OFF(nxt+ 1) = 0; /* just for consistancy. */
#endif 
		}
	      nogo:

		/* Try optimization CURLYX => CURLYM. */
		if (  OP(oscan) == CURLYX && data 
		      && !(data->flags & SF_HAS_PAR)
		      && !(data->flags & SF_HAS_EVAL)
		      && !deltanext  ) {
		    /* XXXX How to optimize if data == 0? */
		    /* Optimize to a simpler form.  */
		    regnode *nxt = NEXTOPER(oscan) + EXTRA_STEP_2ARGS; /* OPEN */
		    regnode *nxt2;

		    OP(oscan) = CURLYM;
		    while ( (nxt2 = regnext(nxt)) /* skip over embedded stuff*/
			    && (OP(nxt2) != WHILEM)) 
			nxt = nxt2;
		    OP(nxt2)  = SUCCEED; /* Whas WHILEM */
		    /* Need to optimize away parenths. */
		    if (data->flags & SF_IN_PAR) {
			/* Set the parenth number.  */
			regnode *nxt1 = NEXTOPER(oscan) + EXTRA_STEP_2ARGS; /* OPEN*/

			if (OP(nxt) != CLOSE) 
			    FAIL("Panic opt close");
			oscan->flags = ARG(nxt);
			OP(nxt1) = OPTIMIZED;	/* was OPEN. */
			OP(nxt) = OPTIMIZED;	/* was CLOSE. */
#ifdef DEBUGGING
			OP(nxt1 + 1) = OPTIMIZED; /* was count. */
			OP(nxt + 1) = OPTIMIZED; /* was count. */
			NEXT_OFF(nxt1 + 1) = 0; /* just for consistancy. */
			NEXT_OFF(nxt + 1) = 0; /* just for consistancy. */
#endif 
#if 0
			while ( nxt1 && (OP(nxt1) != WHILEM)) {
			    regnode *nnxt = regnext(nxt1);
			    
			    if (nnxt == nxt) {
				if (reg_off_by_arg[OP(nxt1)])
				    ARG_SET(nxt1, nxt2 - nxt1);
				else if (nxt2 - nxt1 < U16_MAX)
				    NEXT_OFF(nxt1) = nxt2 - nxt1;
				else
				    OP(nxt) = NOTHING;	/* Cannot beautify */
			    }
			    nxt1 = nnxt;
			}
#endif
			/* Optimize again: */
			study_chunk(&nxt1, &deltanext, nxt, NULL, 0);
		    }
		    else
			oscan->flags = 0;
		}
		else if ((OP(oscan) == CURLYX)
			 && (flags & SCF_WHILEM_VISITED_POS)
			 /* See the comment on a similar expression above.
			    However, this time it not a subexpression
			    we care about, but the expression itself. */
			 && (maxcount == REG_INFTY)
			 && data && ++data->whilem_c < 16) {
		    /* This stays as CURLYX, we can put the count/of pair. */
		    /* Find WHILEM (as in regexec.c) */
		    regnode *nxt = oscan + NEXT_OFF(oscan);

		    if (OP(PREVOPER(nxt)) == NOTHING) /* LONGJMP */
			nxt += ARG(nxt);
		    PREVOPER(nxt)->flags = data->whilem_c
			| (PL_reg_whilem_seen << 4); /* On WHILEM */
		}
		if (data && fl & (SF_HAS_PAR|SF_IN_PAR)) 
		    pars++;
		if (flags & SCF_DO_SUBSTR) {
		    SV *last_str = Nullsv;
		    int counted = mincount != 0;

		    if (data->last_end > 0 && mincount != 0) { /* Ends with a string. */
			I32 b = pos_before >= data->last_start_min 
			    ? pos_before : data->last_start_min;
			STRLEN l;
			char *s = SvPV(data->last_found, l);
			I32 old = b - data->last_start_min;

			if (UTF)
			    old = utf8_hop((U8*)s, old) - (U8*)s;
			
			l -= old;
			/* Get the added string: */
			last_str = newSVpvn(s  + old, l);
			if (deltanext == 0 && pos_before == b) {
			    /* What was added is a constant string */
			    if (mincount > 1) {
				SvGROW(last_str, (mincount * l) + 1);
				repeatcpy(SvPVX(last_str) + l, 
					  SvPVX(last_str), l, mincount - 1);
				SvCUR(last_str) *= mincount;
				/* Add additional parts. */
				SvCUR_set(data->last_found, 
					  SvCUR(data->last_found) - l);
				sv_catsv(data->last_found, last_str);
				data->last_end += l * (mincount - 1);
			    }
			} else {
			    /* start offset must point into the last copy */
			    data->last_start_min += minnext * (mincount - 1);
			    data->last_start_max += is_inf ? 0 : (maxcount - 1)
				* (minnext + data->pos_delta);
			}
		    }
		    /* It is counted once already... */
		    data->pos_min += minnext * (mincount - counted);
		    data->pos_delta += - counted * deltanext +
			(minnext + deltanext) * maxcount - minnext * mincount;
		    if (mincount != maxcount) {
			 /* Cannot extend fixed substrings found inside
			    the group.  */
			scan_commit(data);
			if (mincount && last_str) {
			    sv_setsv(data->last_found, last_str);
			    data->last_end = data->pos_min;
			    data->last_start_min = 
				data->pos_min - CHR_SVLEN(last_str);
			    data->last_start_max = is_inf 
				? I32_MAX 
				: data->pos_min + data->pos_delta
				- CHR_SVLEN(last_str);
			}
			data->longest = &(data->longest_float);
		    }
		    SvREFCNT_dec(last_str);
		}
		if (data && (fl & SF_HAS_EVAL))
		    data->flags |= SF_HAS_EVAL;
	      optimize_curly_tail:
		if (OP(oscan) != CURLYX) {
		    while (PL_regkind[(U8)OP(next = regnext(oscan))] == NOTHING
			   && NEXT_OFF(next))
			NEXT_OFF(oscan) += NEXT_OFF(next);
		}
		continue;
	    default:			/* REF and CLUMP only? */
		if (flags & SCF_DO_SUBSTR) {
		    scan_commit(data);	/* Cannot expect anything... */
		    data->longest = &(data->longest_float);
		}
		is_inf = is_inf_internal = 1;
		if (flags & SCF_DO_STCLASS_OR)
		    cl_anything(data->start_class);
		flags &= ~SCF_DO_STCLASS;
		break;
	    }
	}
	else if (strchr((char*)PL_simple,OP(scan)) || PL_regkind[(U8)OP(scan)] == ANYUTF8) {
	    int value;

	    if (flags & SCF_DO_SUBSTR) {
		scan_commit(data);
		data->pos_min++;
	    }
	    min++;
	    if (flags & SCF_DO_STCLASS) {
		data->start_class->flags &= ~ANYOF_EOS;	/* No match on empty */

		/* Some of the logic below assumes that switching
		   locale on will only add false positives. */
		switch (PL_regkind[(U8)OP(scan)]) {
		case ANYUTF8:
		case SANY:
		case SANYUTF8:
		case ALNUMUTF8:
		case ANYOFUTF8:
		case ALNUMLUTF8:
		case NALNUMUTF8:
		case NALNUMLUTF8:
		case SPACEUTF8:
		case NSPACEUTF8:
		case SPACELUTF8:
		case NSPACELUTF8:
		case DIGITUTF8:
		case NDIGITUTF8:
		default:
		  do_default:
		    /* Perl_croak(aTHX_ "panic: unexpected simple REx opcode %d", OP(scan)); */
		    if (flags & SCF_DO_STCLASS_OR) /* Allow everything */
			cl_anything(data->start_class);
		    break;
		case REG_ANY:
		    if (OP(scan) == SANY)
			goto do_default;
		    if (flags & SCF_DO_STCLASS_OR) { /* Everything but \n */
			value = (ANYOF_BITMAP_TEST(data->start_class,'\n')
				 || (data->start_class->flags & ANYOF_CLASS));
			cl_anything(data->start_class);
		    }
		    if (flags & SCF_DO_STCLASS_AND || !value)
			ANYOF_BITMAP_CLEAR(data->start_class,'\n');
		    break;
		case ANYOF:
		    if (flags & SCF_DO_STCLASS_AND)
			cl_and(data->start_class,
			       (struct regnode_charclass_class*)scan);
		    else
			cl_or(data->start_class,
			      (struct regnode_charclass_class*)scan);
		    break;
		case ALNUM:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (!(data->start_class->flags & ANYOF_LOCALE)) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NALNUM);
			    for (value = 0; value < 256; value++)
				if (!isALNUM(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_ALNUM);
			else {
			    for (value = 0; value < 256; value++)
				if (isALNUM(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			    
			}
		    }
		    break;
		case ALNUML:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NALNUM);
		    }
		    else {
			ANYOF_CLASS_SET(data->start_class,ANYOF_ALNUM);
			data->start_class->flags |= ANYOF_LOCALE;
		    }
		    break;
		case NALNUM:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (!(data->start_class->flags & ANYOF_LOCALE)) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_ALNUM);
			    for (value = 0; value < 256; value++)
				if (isALNUM(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_NALNUM);
			else {
			    for (value = 0; value < 256; value++)
				if (!isALNUM(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			    
			}
		    }
		    break;
		case NALNUML:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_ALNUM);
		    }
		    else {
			data->start_class->flags |= ANYOF_LOCALE;
			ANYOF_CLASS_SET(data->start_class,ANYOF_NALNUM);
		    }
		    break;
		case SPACE:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (!(data->start_class->flags & ANYOF_LOCALE)) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NSPACE);
			    for (value = 0; value < 256; value++)
				if (!isSPACE(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_SPACE);
			else {
			    for (value = 0; value < 256; value++)
				if (isSPACE(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			    
			}
		    }
		    break;
		case SPACEL:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NSPACE);
		    }
		    else {
			data->start_class->flags |= ANYOF_LOCALE;
			ANYOF_CLASS_SET(data->start_class,ANYOF_SPACE);
		    }
		    break;
		case NSPACE:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (!(data->start_class->flags & ANYOF_LOCALE)) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_SPACE);
			    for (value = 0; value < 256; value++)
				if (isSPACE(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_NSPACE);
			else {
			    for (value = 0; value < 256; value++)
				if (!isSPACE(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			    
			}
		    }
		    break;
		case NSPACEL:
		    if (flags & SCF_DO_STCLASS_AND) {
			if (data->start_class->flags & ANYOF_LOCALE) {
			    ANYOF_CLASS_CLEAR(data->start_class,ANYOF_SPACE);
			    for (value = 0; value < 256; value++)
				if (!isSPACE(value))
				    ANYOF_BITMAP_CLEAR(data->start_class, value);
			}
		    }
		    else {
			data->start_class->flags |= ANYOF_LOCALE;
			ANYOF_CLASS_SET(data->start_class,ANYOF_NSPACE);
		    }
		    break;
		case DIGIT:
		    if (flags & SCF_DO_STCLASS_AND) {
			ANYOF_CLASS_CLEAR(data->start_class,ANYOF_NDIGIT);
			for (value = 0; value < 256; value++)
			    if (!isDIGIT(value))
				ANYOF_BITMAP_CLEAR(data->start_class, value);
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_DIGIT);
			else {
			    for (value = 0; value < 256; value++)
				if (isDIGIT(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			    
			}
		    }
		    break;
		case NDIGIT:
		    if (flags & SCF_DO_STCLASS_AND) {
			ANYOF_CLASS_CLEAR(data->start_class,ANYOF_DIGIT);
			for (value = 0; value < 256; value++)
			    if (isDIGIT(value))
				ANYOF_BITMAP_CLEAR(data->start_class, value);
		    }
		    else {
			if (data->start_class->flags & ANYOF_LOCALE)
			    ANYOF_CLASS_SET(data->start_class,ANYOF_NDIGIT);
			else {
			    for (value = 0; value < 256; value++)
				if (!isDIGIT(value))
				    ANYOF_BITMAP_SET(data->start_class, value);			    
			}
		    }
		    break;
		}
		if (flags & SCF_DO_STCLASS_OR)
		    cl_and(data->start_class, &and_with);
		flags &= ~SCF_DO_STCLASS;
	    }
	}
	else if (PL_regkind[(U8)OP(scan)] == EOL && flags & SCF_DO_SUBSTR) {
	    data->flags |= (OP(scan) == MEOL
			    ? SF_BEFORE_MEOL
			    : SF_BEFORE_SEOL);
	}
	else if (  PL_regkind[(U8)OP(scan)] == BRANCHJ
		 /* Lookbehind, or need to calculate parens/evals/stclass: */
		   && (scan->flags || data || (flags & SCF_DO_STCLASS))
		   && (OP(scan) == IFMATCH || OP(scan) == UNLESSM)) {
	    /* Lookahead/lookbehind */
	    I32 deltanext, minnext, fake = 0;
	    regnode *nscan;
	    struct regnode_charclass_class intrnl;
	    int f = 0;

	    data_fake.flags = 0;
	    if (data) {		    
		data_fake.whilem_c = data->whilem_c;
		data_fake.last_closep = data->last_closep;
	    }
	    else
		data_fake.last_closep = &fake;
	    if ( flags & SCF_DO_STCLASS && !scan->flags
		 && OP(scan) == IFMATCH ) { /* Lookahead */
		cl_init(&intrnl);
		data_fake.start_class = &intrnl;
		f |= SCF_DO_STCLASS_AND;
	    }
	    if (flags & SCF_WHILEM_VISITED_POS)
		f |= SCF_WHILEM_VISITED_POS;
	    next = regnext(scan);
	    nscan = NEXTOPER(NEXTOPER(scan));
	    minnext = study_chunk(&nscan, &deltanext, last, &data_fake, f);
	    if (scan->flags) {
		if (deltanext) {
		    vFAIL("Variable length lookbehind not implemented");
		}
		else if (minnext > U8_MAX) {
		    vFAIL2("Lookbehind longer than %"UVuf" not implemented", (UV)U8_MAX);
		}
		scan->flags = minnext;
	    }
	    if (data && data_fake.flags & (SF_HAS_PAR|SF_IN_PAR))
		pars++;
	    if (data && (data_fake.flags & SF_HAS_EVAL))
		data->flags |= SF_HAS_EVAL;
	    if (data)
		data->whilem_c = data_fake.whilem_c;
	    if (f & SCF_DO_STCLASS_AND) {
		int was = (data->start_class->flags & ANYOF_EOS);

		cl_and(data->start_class, &intrnl);
		if (was)
		    data->start_class->flags |= ANYOF_EOS;
	    }
	}
	else if (OP(scan) == OPEN) {
	    pars++;
	}
	else if (OP(scan) == CLOSE) {
	    if (ARG(scan) == is_par) {
		next = regnext(scan);

		if ( next && (OP(next) != WHILEM) && next < last)
		    is_par = 0;		/* Disable optimization */
	    }
	    if (data)
		*(data->last_closep) = ARG(scan);
	}
	else if (OP(scan) == EVAL) {
		if (data)
		    data->flags |= SF_HAS_EVAL;
	}
	else if (OP(scan) == LOGICAL && scan->flags == 2) { /* Embedded follows */
		if (flags & SCF_DO_SUBSTR) {
		    scan_commit(data);
		    data->longest = &(data->longest_float);
		}
		is_inf = is_inf_internal = 1;
		if (flags & SCF_DO_STCLASS_OR) /* Allow everything */
		    cl_anything(data->start_class);
		flags &= ~SCF_DO_STCLASS;
	}
	/* Else: zero-length, ignore. */
	scan = regnext(scan);
    }

  finish:
    *scanp = scan;
    *deltap = is_inf_internal ? I32_MAX : delta;
    if (flags & SCF_DO_SUBSTR && is_inf) 
	data->pos_delta = I32_MAX - data->pos_min;
    if (is_par > U8_MAX)
	is_par = 0;
    if (is_par && pars==1 && data) {
	data->flags |= SF_IN_PAR;
	data->flags &= ~SF_HAS_PAR;
    }
    else if (pars && data) {
	data->flags |= SF_HAS_PAR;
	data->flags &= ~SF_IN_PAR;
    }
    if (flags & SCF_DO_STCLASS_OR)
	cl_and(data->start_class, &and_with);
    return min;
}

STATIC I32
S_add_data(pTHX_ I32 n, char *s)
{
    if (PL_regcomp_rx->data) {
	Renewc(PL_regcomp_rx->data, 
	       sizeof(*PL_regcomp_rx->data) + sizeof(void*) * (PL_regcomp_rx->data->count + n - 1), 
	       char, struct reg_data);
	Renew(PL_regcomp_rx->data->what, PL_regcomp_rx->data->count + n, U8);
	PL_regcomp_rx->data->count += n;
    }
    else {
	Newc(1207, PL_regcomp_rx->data, sizeof(*PL_regcomp_rx->data) + sizeof(void*) * (n - 1),
	     char, struct reg_data);
	New(1208, PL_regcomp_rx->data->what, n, U8);
	PL_regcomp_rx->data->count = n;
    }
    Copy(s, PL_regcomp_rx->data->what + PL_regcomp_rx->data->count - n, n, U8);
    return PL_regcomp_rx->data->count - n;
}

void
Perl_reginitcolors(pTHX)
{
    int i = 0;
    char *s = PerlEnv_getenv("PERL_RE_COLORS");
	    
    if (s) {
	PL_colors[0] = s = savepv(s);
	while (++i < 6) {
	    s = strchr(s, '\t');
	    if (s) {
		*s = '\0';
		PL_colors[i] = ++s;
	    }
	    else
		PL_colors[i] = s = "";
	}
    } else {
	while (i < 6) 
	    PL_colors[i++] = "";
    }
    PL_colorset = 1;
}


/*
 - pregcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.) [NB: not true in perl]
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.  [I'll say.]
 */
regexp *
Perl_pregcomp(pTHX_ char *exp, char *xend, PMOP *pm)
{
    register regexp *r;
    regnode *scan;
    regnode *first;
    I32 flags;
    I32 minlen = 0;
    I32 sawplus = 0;
    I32 sawopen = 0;
    scan_data_t data;

    if (exp == NULL)
	FAIL("NULL regexp argument");

    if (pm->op_pmdynflags & PMdf_UTF8) {
	PL_reg_flags |= RF_utf8;
    }
    else
	PL_reg_flags = 0;

    PL_regprecomp = exp;
    DEBUG_r(if (!PL_colorset) reginitcolors());
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%sCompiling REx%s `%s%*s%s'\n",
		      PL_colors[4],PL_colors[5],PL_colors[0],
		      (int)(xend - exp), PL_regprecomp, PL_colors[1]));
    PL_regflags = pm->op_pmflags;
    PL_regsawback = 0;

    PL_regseen = 0;
    PL_seen_zerolen = *exp == '^' ? -1 : 0;
    PL_seen_evals = 0;
    PL_extralen = 0;

    /* First pass: determine size, legality. */
    PL_regcomp_parse = exp;
    PL_regxend = xend;
    PL_regnaughty = 0;
    PL_regnpar = 1;
    PL_regsize = 0L;
    PL_regcode = &PL_regdummy;
    PL_reg_whilem_seen = 0;
#if 0 /* REGC() is (currently) a NOP at the first pass.
       * Clever compilers notice this and complain. --jhi */
    REGC((U8)REG_MAGIC, (char*)PL_regcode);
#endif
    if (reg(0, &flags) == NULL) {
	PL_regprecomp = Nullch;
	return(NULL);
    }
    DEBUG_r(PerlIO_printf(Perl_debug_log, "size %"IVdf" ", (IV)PL_regsize));

    /* Small enough for pointer-storage convention?
       If extralen==0, this means that we will not need long jumps. */
    if (PL_regsize >= 0x10000L && PL_extralen)
        PL_regsize += PL_extralen;
    else
	PL_extralen = 0;
    if (PL_reg_whilem_seen > 15)
	PL_reg_whilem_seen = 15;

    /* Allocate space and initialize. */
    Newc(1001, r, sizeof(regexp) + (unsigned)PL_regsize * sizeof(regnode),
	 char, regexp);
    if (r == NULL)
	FAIL("Regexp out of space");

#ifdef DEBUGGING
    /* avoid reading uninitialized memory in DEBUGGING code in study_chunk() */
    Zero(r, sizeof(regexp) + (unsigned)PL_regsize * sizeof(regnode), char);
#endif
    r->refcnt = 1;
    r->prelen = xend - exp;
    r->precomp = savepvn(PL_regprecomp, r->prelen);
    r->subbeg = NULL;
    r->reganch = pm->op_pmflags & PMf_COMPILETIME;
    r->nparens = PL_regnpar - 1;	/* set early to validate backrefs */

    r->substrs = 0;			/* Useful during FAIL. */
    r->startp = 0;			/* Useful during FAIL. */
    r->endp = 0;			/* Useful during FAIL. */

    PL_regcomp_rx = r;

    /* Second pass: emit code. */
    PL_regcomp_parse = exp;
    PL_regxend = xend;
    PL_regnaughty = 0;
    PL_regnpar = 1;
    PL_regcode = r->program;
    /* Store the count of eval-groups for security checks: */
    PL_regcode->next_off = ((PL_seen_evals > U16_MAX) ? U16_MAX : PL_seen_evals);
    REGC((U8)REG_MAGIC, (char*) PL_regcode++);
    r->data = 0;
    if (reg(0, &flags) == NULL)
	return(NULL);

    /* Dig out information for optimizations. */
    r->reganch = pm->op_pmflags & PMf_COMPILETIME; /* Again? */
    pm->op_pmflags = PL_regflags;
    if (UTF)
	r->reganch |= ROPT_UTF8;
    r->regstclass = NULL;
    if (PL_regnaughty >= 10)	/* Probably an expensive pattern. */
	r->reganch |= ROPT_NAUGHTY;
    scan = r->program + 1;		/* First BRANCH. */

    /* XXXX To minimize changes to RE engine we always allocate
       3-units-long substrs field. */
    Newz(1004, r->substrs, 1, struct reg_substr_data);

    StructCopy(&zero_scan_data, &data, scan_data_t);
    /* XXXX Should not we check for something else?  Usually it is OPEN1... */
    if (OP(scan) != BRANCH) {	/* Only one top-level choice. */
	I32 fake;
	STRLEN longest_float_length, longest_fixed_length;
	struct regnode_charclass_class ch_class;
	int stclass_flag;
	I32 last_close = 0;

	first = scan;
	/* Skip introductions and multiplicators >= 1. */
	while ((OP(first) == OPEN && (sawopen = 1)) ||
	       /* An OR of *one* alternative - should not happen now. */
	    (OP(first) == BRANCH && OP(regnext(first)) != BRANCH) ||
	    (OP(first) == PLUS) ||
	    (OP(first) == MINMOD) ||
	       /* An {n,m} with n>0 */
	    (PL_regkind[(U8)OP(first)] == CURLY && ARG1(first) > 0) ) {
		if (OP(first) == PLUS)
		    sawplus = 1;
		else
		    first += regarglen[(U8)OP(first)];
		first = NEXTOPER(first);
	}

	/* Starting-point info. */
      again:
	if (PL_regkind[(U8)OP(first)] == EXACT) {
	    if (OP(first) == EXACT);	/* Empty, get anchored substr later. */
	    else if ((OP(first) == EXACTF || OP(first) == EXACTFL)
		     && !UTF)
		r->regstclass = first;
	}
	else if (strchr((char*)PL_simple,OP(first)))
	    r->regstclass = first;
	else if (PL_regkind[(U8)OP(first)] == BOUND ||
		 PL_regkind[(U8)OP(first)] == NBOUND)
	    r->regstclass = first;
	else if (PL_regkind[(U8)OP(first)] == BOL) {
	    r->reganch |= (OP(first) == MBOL
			   ? ROPT_ANCH_MBOL
			   : (OP(first) == SBOL
			      ? ROPT_ANCH_SBOL
			      : ROPT_ANCH_BOL));
	    first = NEXTOPER(first);
	    goto again;
	}
	else if (OP(first) == GPOS) {
	    r->reganch |= ROPT_ANCH_GPOS;
	    first = NEXTOPER(first);
	    goto again;
	}
	else if ((OP(first) == STAR &&
	    PL_regkind[(U8)OP(NEXTOPER(first))] == REG_ANY) &&
	    !(r->reganch & ROPT_ANCH) )
	{
	    /* turn .* into ^.* with an implied $*=1 */
	    int type = OP(NEXTOPER(first));

	    if (type == REG_ANY || type == ANYUTF8)
		type = ROPT_ANCH_MBOL;
	    else
		type = ROPT_ANCH_SBOL;

	    r->reganch |= type | ROPT_IMPLICIT;
	    first = NEXTOPER(first);
	    goto again;
	}
	if (sawplus && (!sawopen || !PL_regsawback) 
	    && !(PL_regseen & REG_SEEN_EVAL)) /* May examine pos and $& */
	    /* x+ must match at the 1st pos of run of x's */
	    r->reganch |= ROPT_SKIP;

	/* Scan is after the zeroth branch, first is atomic matcher. */
	DEBUG_r(PerlIO_printf(Perl_debug_log, "first at %"IVdf"\n", 
			      (IV)(first - scan + 1)));
	/*
	* If there's something expensive in the r.e., find the
	* longest literal string that must appear and make it the
	* regmust.  Resolve ties in favor of later strings, since
	* the regstart check works with the beginning of the r.e.
	* and avoiding duplication strengthens checking.  Not a
	* strong reason, but sufficient in the absence of others.
	* [Now we resolve ties in favor of the earlier string if
	* it happens that c_offset_min has been invalidated, since the
	* earlier string may buy us something the later one won't.]
	*/
	minlen = 0;

	data.longest_fixed = newSVpvn("",0);
	data.longest_float = newSVpvn("",0);
	data.last_found = newSVpvn("",0);
	data.longest = &(data.longest_fixed);
	first = scan;
	if (!r->regstclass) {
	    cl_init(&ch_class);
	    data.start_class = &ch_class;
	    stclass_flag = SCF_DO_STCLASS_AND;
	} else				/* XXXX Check for BOUND? */
	    stclass_flag = 0;
	data.last_closep = &last_close;

	minlen = study_chunk(&first, &fake, scan + PL_regsize, /* Up to end */
			     &data, SCF_DO_SUBSTR | SCF_WHILEM_VISITED_POS | stclass_flag);
	if ( PL_regnpar == 1 && data.longest == &(data.longest_fixed)
	     && data.last_start_min == 0 && data.last_end > 0 
	     && !PL_seen_zerolen
	     && (!(PL_regseen & REG_SEEN_GPOS) || (r->reganch & ROPT_ANCH_GPOS)))
	    r->reganch |= ROPT_CHECK_ALL;
	scan_commit(&data);
	SvREFCNT_dec(data.last_found);

	longest_float_length = CHR_SVLEN(data.longest_float);
	if (longest_float_length
	    || (data.flags & SF_FL_BEFORE_EOL
		&& (!(data.flags & SF_FL_BEFORE_MEOL)
		    || (PL_regflags & PMf_MULTILINE)))) {
	    int t;

	    if (SvCUR(data.longest_fixed) 			/* ok to leave SvCUR */
		&& data.offset_fixed == data.offset_float_min
		&& SvCUR(data.longest_fixed) == SvCUR(data.longest_float))
		    goto remove_float;		/* As in (a)+. */

	    r->float_substr = data.longest_float;
	    r->float_min_offset = data.offset_float_min;
	    r->float_max_offset = data.offset_float_max;
	    t = (data.flags & SF_FL_BEFORE_EOL /* Can't have SEOL and MULTI */
		       && (!(data.flags & SF_FL_BEFORE_MEOL)
			   || (PL_regflags & PMf_MULTILINE)));
	    fbm_compile(r->float_substr, t ? FBMcf_TAIL : 0);
	}
	else {
	  remove_float:
	    r->float_substr = Nullsv;
	    SvREFCNT_dec(data.longest_float);
	    longest_float_length = 0;
	}

	longest_fixed_length = CHR_SVLEN(data.longest_fixed);
	if (longest_fixed_length
	    || (data.flags & SF_FIX_BEFORE_EOL /* Cannot have SEOL and MULTI */
		&& (!(data.flags & SF_FIX_BEFORE_MEOL)
		    || (PL_regflags & PMf_MULTILINE)))) {
	    int t;

	    r->anchored_substr = data.longest_fixed;
	    r->anchored_offset = data.offset_fixed;
	    t = (data.flags & SF_FIX_BEFORE_EOL /* Can't have SEOL and MULTI */
		 && (!(data.flags & SF_FIX_BEFORE_MEOL)
		     || (PL_regflags & PMf_MULTILINE)));
	    fbm_compile(r->anchored_substr, t ? FBMcf_TAIL : 0);
	}
	else {
	    r->anchored_substr = Nullsv;
	    SvREFCNT_dec(data.longest_fixed);
	    longest_fixed_length = 0;
	}
	if (r->regstclass 
	    && (OP(r->regstclass) == REG_ANY || OP(r->regstclass) == ANYUTF8
		|| OP(r->regstclass) == SANYUTF8 || OP(r->regstclass) == SANY))
	    r->regstclass = NULL;
	if ((!r->anchored_substr || r->anchored_offset) && stclass_flag
	    && !(data.start_class->flags & ANYOF_EOS)
	    && !cl_is_anything(data.start_class)) {
	    SV *sv;
	    I32 n = add_data(1, "f");

	    New(1006, PL_regcomp_rx->data->data[n], 1, 
		struct regnode_charclass_class);
	    StructCopy(data.start_class,
		       (struct regnode_charclass_class*)PL_regcomp_rx->data->data[n],
		       struct regnode_charclass_class);
	    r->regstclass = (regnode*)PL_regcomp_rx->data->data[n];
	    r->reganch &= ~ROPT_SKIP;	/* Used in find_byclass(). */
	    DEBUG_r((sv = sv_newmortal(),
		     regprop(sv, (regnode*)data.start_class),
		     PerlIO_printf(Perl_debug_log, "synthetic stclass `%s'.\n",
				   SvPVX(sv))));
	}

	/* A temporary algorithm prefers floated substr to fixed one to dig more info. */
	if (longest_fixed_length > longest_float_length) {
	    r->check_substr = r->anchored_substr;
	    r->check_offset_min = r->check_offset_max = r->anchored_offset;
	    if (r->reganch & ROPT_ANCH_SINGLE)
		r->reganch |= ROPT_NOSCAN;
	}
	else {
	    r->check_substr = r->float_substr;
	    r->check_offset_min = data.offset_float_min;
	    r->check_offset_max = data.offset_float_max;
	}
	/* XXXX Currently intuiting is not compatible with ANCH_GPOS.
	   This should be changed ASAP!  */
	if (r->check_substr && !(r->reganch & ROPT_ANCH_GPOS)) {
	    r->reganch |= RE_USE_INTUIT;
	    if (SvTAIL(r->check_substr))
		r->reganch |= RE_INTUIT_TAIL;
	}
    }
    else {
	/* Several toplevels. Best we can is to set minlen. */
	I32 fake;
	struct regnode_charclass_class ch_class;
	I32 last_close = 0;
	
	DEBUG_r(PerlIO_printf(Perl_debug_log, "\n"));
	scan = r->program + 1;
	cl_init(&ch_class);
	data.start_class = &ch_class;
	data.last_closep = &last_close;
	minlen = study_chunk(&scan, &fake, scan + PL_regsize, &data, SCF_DO_STCLASS_AND|SCF_WHILEM_VISITED_POS);
	r->check_substr = r->anchored_substr = r->float_substr = Nullsv;
	if (!(data.start_class->flags & ANYOF_EOS)
	    && !cl_is_anything(data.start_class)) {
	    SV *sv;
	    I32 n = add_data(1, "f");

	    New(1006, PL_regcomp_rx->data->data[n], 1, 
		struct regnode_charclass_class);
	    StructCopy(data.start_class,
		       (struct regnode_charclass_class*)PL_regcomp_rx->data->data[n],
		       struct regnode_charclass_class);
	    r->regstclass = (regnode*)PL_regcomp_rx->data->data[n];
	    r->reganch &= ~ROPT_SKIP;	/* Used in find_byclass(). */
	    DEBUG_r((sv = sv_newmortal(),
		     regprop(sv, (regnode*)data.start_class),
		     PerlIO_printf(Perl_debug_log, "synthetic stclass `%s'.\n",
				   SvPVX(sv))));
	}
    }

    r->minlen = minlen;
    if (PL_regseen & REG_SEEN_GPOS) 
	r->reganch |= ROPT_GPOS_SEEN;
    if (PL_regseen & REG_SEEN_LOOKBEHIND)
	r->reganch |= ROPT_LOOKBEHIND_SEEN;
    if (PL_regseen & REG_SEEN_EVAL)
	r->reganch |= ROPT_EVAL_SEEN;
    Newz(1002, r->startp, PL_regnpar, I32);
    Newz(1002, r->endp, PL_regnpar, I32);
    PL_regdata = r->data; /* for regprop() ANYOFUTF8 */
    DEBUG_r(regdump(r));
    return(r);
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
STATIC regnode *
S_reg(pTHX_ I32 paren, I32 *flagp)
    /* paren: Parenthesized? 0=top, 1=(, inside: changed to letter. */
{
    register regnode *ret;		/* Will be the head of the group. */
    register regnode *br;
    register regnode *lastbr;
    register regnode *ender = 0;
    register I32 parno = 0;
    I32 flags, oregflags = PL_regflags, have_branch = 0, open = 0;
    char *oregcomp_parse = PL_regcomp_parse;
    char c;

    *flagp = 0;				/* Tentatively. */

    /* Make an OPEN node, if parenthesized. */
    if (paren) {
	if (*PL_regcomp_parse == '?') {
	    U16 posflags = 0, negflags = 0;
	    U16 *flagsp = &posflags;
	    int logical = 0;
	    char *seqstart = PL_regcomp_parse;

	    PL_regcomp_parse++;
	    paren = *PL_regcomp_parse++;
	    ret = NULL;			/* For look-ahead/behind. */
	    switch (paren) {
	    case '<':
		PL_regseen |= REG_SEEN_LOOKBEHIND;
		if (*PL_regcomp_parse == '!') 
		    paren = ',';
		if (*PL_regcomp_parse != '=' && *PL_regcomp_parse != '!') 
		    goto unknown;
		PL_regcomp_parse++;
	    case '=':
	    case '!':
		PL_seen_zerolen++;
	    case ':':
	    case '>':
		break;
	    case '$':
	    case '@':
		vFAIL2("Sequence (?%c...) not implemented", (int)paren);
		break;
	    case '#':
		while (*PL_regcomp_parse && *PL_regcomp_parse != ')')
		    PL_regcomp_parse++;
		if (*PL_regcomp_parse != ')')
		    FAIL("Sequence (?#... not terminated");
		nextchar();
		*flagp = TRYAGAIN;
		return NULL;
	    case 'p':
		if (SIZE_ONLY)
		    vWARN(PL_regcomp_parse, "(?p{}) is deprecated - use (??{})");
		/* FALL THROUGH*/
	    case '?':
		logical = 1;
		paren = *PL_regcomp_parse++;
		/* FALL THROUGH */
	    case '{':
	    {
		I32 count = 1, n = 0;
		char c;
		char *s = PL_regcomp_parse;
		SV *sv;
		OP_4tree *sop, *rop;

		PL_seen_zerolen++;
		PL_regseen |= REG_SEEN_EVAL;
		while (count && (c = *PL_regcomp_parse)) {
		    if (c == '\\' && PL_regcomp_parse[1])
			PL_regcomp_parse++;
		    else if (c == '{') 
			count++;
		    else if (c == '}') 
			count--;
		    PL_regcomp_parse++;
		}
		if (*PL_regcomp_parse != ')')
		{
		    PL_regcomp_parse = s;		    
		    vFAIL("Sequence (?{...}) not terminated or not {}-balanced");
		}
		if (!SIZE_ONLY) {
		    AV *av;
		    
		    if (PL_regcomp_parse - 1 - s) 
			sv = newSVpvn(s, PL_regcomp_parse - 1 - s);
		    else
			sv = newSVpvn("", 0);

		    ENTER;
		    Perl_save_re_context(aTHX);
		    rop = sv_compile_2op(sv, &sop, "re", &av);
		    LEAVE;

		    n = add_data(3, "nop");
		    PL_regcomp_rx->data->data[n] = (void*)rop;
		    PL_regcomp_rx->data->data[n+1] = (void*)sop;
		    PL_regcomp_rx->data->data[n+2] = (void*)av;
		    SvREFCNT_dec(sv);
		}
		else {						/* First pass */
		    if (PL_reginterp_cnt < ++PL_seen_evals
			&& PL_curcop != &PL_compiling)
			/* No compiled RE interpolated, has runtime
			   components ===> unsafe.  */
			FAIL("Eval-group not allowed at runtime, use re 'eval'");
		    if (PL_tainted)
			FAIL("Eval-group in insecure regular expression");
		}
		
		nextchar();
		if (logical) {
		    ret = reg_node(LOGICAL);
		    if (!SIZE_ONLY)
			ret->flags = 2;
		    regtail(ret, reganode(EVAL, n));
		    return ret;
		}
		return reganode(EVAL, n);
	    }
	    case '(':
	    {
		if (PL_regcomp_parse[0] == '?') {
		    if (PL_regcomp_parse[1] == '=' || PL_regcomp_parse[1] == '!' 
			|| PL_regcomp_parse[1] == '<' 
			|| PL_regcomp_parse[1] == '{') { /* Lookahead or eval. */
			I32 flag;
			
			ret = reg_node(LOGICAL);
			if (!SIZE_ONLY)
			    ret->flags = 1;
			regtail(ret, reg(1, &flag));
			goto insert_if;
		    } 
		}
		else if (PL_regcomp_parse[0] >= '1' && PL_regcomp_parse[0] <= '9' ) {
		    parno = atoi(PL_regcomp_parse++);

		    while (isDIGIT(*PL_regcomp_parse))
			PL_regcomp_parse++;
		    ret = reganode(GROUPP, parno);
		    if ((c = *nextchar()) != ')')
			vFAIL("Switch condition not recognized");
		  insert_if:
		    regtail(ret, reganode(IFTHEN, 0));
		    br = regbranch(&flags, 1);
		    if (br == NULL)
			br = reganode(LONGJMP, 0);
		    else
			regtail(br, reganode(LONGJMP, 0));
		    c = *nextchar();
		    if (flags&HASWIDTH)
			*flagp |= HASWIDTH;
		    if (c == '|') {
			lastbr = reganode(IFTHEN, 0); /* Fake one for optimizer. */
			regbranch(&flags, 1);
			regtail(ret, lastbr);
		 	if (flags&HASWIDTH)
			    *flagp |= HASWIDTH;
			c = *nextchar();
		    }
		    else
			lastbr = NULL;
		    if (c != ')')
			vFAIL("Switch (?(condition)... contains too many branches");
		    ender = reg_node(TAIL);
		    regtail(br, ender);
		    if (lastbr) {
			regtail(lastbr, ender);
			regtail(NEXTOPER(NEXTOPER(lastbr)), ender);
		    }
		    else
			regtail(ret, ender);
		    return ret;
		}
		else {
		    vFAIL2("Unknown switch condition (?(%.2s", PL_regcomp_parse);
		}
	    }
            case 0:
		PL_regcomp_parse--; /* for vFAIL to print correctly */
                vFAIL("Sequence (? incomplete");
                break;
	    default:
		--PL_regcomp_parse;
	      parse_flags:
		while (*PL_regcomp_parse && strchr("iogcmsx", *PL_regcomp_parse)) {
		    if (*PL_regcomp_parse != 'o')
			pmflag(flagsp, *PL_regcomp_parse);
		    ++PL_regcomp_parse;
		}
		if (*PL_regcomp_parse == '-') {
		    flagsp = &negflags;
		    ++PL_regcomp_parse;
		    goto parse_flags;
		}
		PL_regflags |= posflags;
		PL_regflags &= ~negflags;
		if (*PL_regcomp_parse == ':') {
		    PL_regcomp_parse++;
		    paren = ':';
		    break;
		}		
	      unknown:
		if (*PL_regcomp_parse != ')') {
		    PL_regcomp_parse++;
		    vFAIL3("Sequence (%.*s...) not recognized", PL_regcomp_parse-seqstart, seqstart);
		}
		nextchar();
		*flagp = TRYAGAIN;
		return NULL;
	    }
	}
	else {
	    parno = PL_regnpar;
	    PL_regnpar++;
	    ret = reganode(OPEN, parno);
	    open = 1;
	}
    }
    else
	ret = NULL;

    /* Pick up the branches, linking them together. */
    br = regbranch(&flags, 1);
    if (br == NULL)
	return(NULL);
    if (*PL_regcomp_parse == '|') {
	if (!SIZE_ONLY && PL_extralen) {
	    reginsert(BRANCHJ, br);
	}
	else
	    reginsert(BRANCH, br);
	have_branch = 1;
	if (SIZE_ONLY)
	    PL_extralen += 1;		/* For BRANCHJ-BRANCH. */
    }
    else if (paren == ':') {
	*flagp |= flags&SIMPLE;
    }
    if (open) {				/* Starts with OPEN. */
	regtail(ret, br);		/* OPEN -> first. */
    }
    else if (paren != '?')		/* Not Conditional */
	ret = br;
    if (flags&HASWIDTH)
	*flagp |= HASWIDTH;
    *flagp |= flags&SPSTART;
    lastbr = br;
    while (*PL_regcomp_parse == '|') {
	if (!SIZE_ONLY && PL_extralen) {
	    ender = reganode(LONGJMP,0);
	    regtail(NEXTOPER(NEXTOPER(lastbr)), ender); /* Append to the previous. */
	}
	if (SIZE_ONLY)
	    PL_extralen += 2;		/* Account for LONGJMP. */
	nextchar();
	br = regbranch(&flags, 0);
	if (br == NULL)
	    return(NULL);
	regtail(lastbr, br);		/* BRANCH -> BRANCH. */
	lastbr = br;
	if (flags&HASWIDTH)
	    *flagp |= HASWIDTH;
	*flagp |= flags&SPSTART;
    }

    if (have_branch || paren != ':') {
	/* Make a closing node, and hook it on the end. */
	switch (paren) {
	case ':':
	    ender = reg_node(TAIL);
	    break;
	case 1:
	    ender = reganode(CLOSE, parno);
	    break;
	case '<':
	case ',':
	case '=':
	case '!':
	    *flagp &= ~HASWIDTH;
	    /* FALL THROUGH */
	case '>':
	    ender = reg_node(SUCCEED);
	    break;
	case 0:
	    ender = reg_node(END);
	    break;
	}
	regtail(lastbr, ender);

	if (have_branch) {
	    /* Hook the tails of the branches to the closing node. */
	    for (br = ret; br != NULL; br = regnext(br)) {
		regoptail(br, ender);
	    }
	}
    }

    {
	char *p;
	static char parens[] = "=!<,>";

	if (paren && (p = strchr(parens, paren))) {
	    int node = ((p - parens) % 2) ? UNLESSM : IFMATCH;
	    int flag = (p - parens) > 1;

	    if (paren == '>')
		node = SUSPEND, flag = 0;
	    reginsert(node,ret);
	    ret->flags = flag;
	    regtail(ret, reg_node(TAIL));
	}
    }

    /* Check for proper termination. */
    if (paren) {
	PL_regflags = oregflags;
	if (PL_regcomp_parse >= PL_regxend || *nextchar() != ')') {
	    PL_regcomp_parse = oregcomp_parse;
	    vFAIL("Unmatched (");
	}
    }
    else if (!paren && PL_regcomp_parse < PL_regxend) {
	if (*PL_regcomp_parse == ')') {
	    PL_regcomp_parse++;
	    vFAIL("Unmatched )");
	}
	else
	    FAIL("Junk on end of regexp");	/* "Can't happen". */
	/* NOTREACHED */
    }

    return(ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
STATIC regnode *
S_regbranch(pTHX_ I32 *flagp, I32 first)
{
    register regnode *ret;
    register regnode *chain = NULL;
    register regnode *latest;
    I32 flags = 0, c = 0;

    if (first) 
	ret = NULL;
    else {
	if (!SIZE_ONLY && PL_extralen) 
	    ret = reganode(BRANCHJ,0);
	else
	    ret = reg_node(BRANCH);
    }
	
    if (!first && SIZE_ONLY) 
	PL_extralen += 1;			/* BRANCHJ */
    
    *flagp = WORST;			/* Tentatively. */

    PL_regcomp_parse--;
    nextchar();
    while (PL_regcomp_parse < PL_regxend && *PL_regcomp_parse != '|' && *PL_regcomp_parse != ')') {
	flags &= ~TRYAGAIN;
	latest = regpiece(&flags);
	if (latest == NULL) {
	    if (flags & TRYAGAIN)
		continue;
	    return(NULL);
	}
	else if (ret == NULL)
	    ret = latest;
	*flagp |= flags&HASWIDTH;
	if (chain == NULL) 	/* First piece. */
	    *flagp |= flags&SPSTART;
	else {
	    PL_regnaughty++;
	    regtail(chain, latest);
	}
	chain = latest;
	c++;
    }
    if (chain == NULL) {	/* Loop ran zero times. */
	chain = reg_node(NOTHING);
	if (ret == NULL)
	    ret = chain;
    }
    if (c == 1) {
	*flagp |= flags&SIMPLE;
    }

    return(ret);
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
STATIC regnode *
S_regpiece(pTHX_ I32 *flagp)
{
    register regnode *ret;
    register char op;
    register char *next;
    I32 flags;
    char *origparse = PL_regcomp_parse;
    char *maxpos;
    I32 min;
    I32 max = REG_INFTY;

    ret = regatom(&flags);
    if (ret == NULL) {
	if (flags & TRYAGAIN)
	    *flagp |= TRYAGAIN;
	return(NULL);
    }

    op = *PL_regcomp_parse;

    if (op == '{' && regcurly(PL_regcomp_parse)) {
	next = PL_regcomp_parse + 1;
	maxpos = Nullch;
	while (isDIGIT(*next) || *next == ',') {
	    if (*next == ',') {
		if (maxpos)
		    break;
		else
		    maxpos = next;
	    }
	    next++;
	}
	if (*next == '}') {		/* got one */
	    if (!maxpos)
		maxpos = next;
	    PL_regcomp_parse++;
	    min = atoi(PL_regcomp_parse);
	    if (*maxpos == ',')
		maxpos++;
	    else
		maxpos = PL_regcomp_parse;
	    max = atoi(maxpos);
	    if (!max && *maxpos != '0')
		max = REG_INFTY;		/* meaning "infinity" */
	    else if (max >= REG_INFTY)
		vFAIL2("Quantifier in {,} bigger than %d", REG_INFTY - 1);
	    PL_regcomp_parse = next;
	    nextchar();

	do_curly:
	    if ((flags&SIMPLE)) {
		PL_regnaughty += 2 + PL_regnaughty / 2;
		reginsert(CURLY, ret);
	    }
	    else {
		regnode *w = reg_node(WHILEM);

		w->flags = 0;
		regtail(ret, w);
		if (!SIZE_ONLY && PL_extralen) {
		    reginsert(LONGJMP,ret);
		    reginsert(NOTHING,ret);
		    NEXT_OFF(ret) = 3;	/* Go over LONGJMP. */
		}
		reginsert(CURLYX,ret);
		if (!SIZE_ONLY && PL_extralen)
		    NEXT_OFF(ret) = 3;	/* Go over NOTHING to LONGJMP. */
		regtail(ret, reg_node(NOTHING));
		if (SIZE_ONLY)
		    PL_reg_whilem_seen++, PL_extralen += 3;
		PL_regnaughty += 4 + PL_regnaughty;	/* compound interest */
	    }
	    ret->flags = 0;

	    if (min > 0)
		*flagp = WORST;
	    if (max > 0)
		*flagp |= HASWIDTH;
	    if (max && max < min)
		vFAIL("Can't do {n,m} with n > m");
	    if (!SIZE_ONLY) {
		ARG1_SET(ret, min);
		ARG2_SET(ret, max);
	    }

	    goto nest_check;
	}
    }

    if (!ISMULT1(op)) {
	*flagp = flags;
	return(ret);
    }

#if 0				/* Now runtime fix should be reliable. */

    /* if this is reinstated, don't forget to put this back into perldiag:

	    =item Regexp *+ operand could be empty at {#} in regex m/%s/

	   (F) The part of the regexp subject to either the * or + quantifier
           could match an empty string. The {#} shows in the regular
           expression about where the problem was discovered.

    */

    if (!(flags&HASWIDTH) && op != '?')
      vFAIL("Regexp *+ operand could be empty");
#endif 

    nextchar();

    *flagp = (op != '+') ? (WORST|SPSTART|HASWIDTH) : (WORST|HASWIDTH);

    if (op == '*' && (flags&SIMPLE)) {
	reginsert(STAR, ret);
	ret->flags = 0;
	PL_regnaughty += 4;
    }
    else if (op == '*') {
	min = 0;
	goto do_curly;
    }
    else if (op == '+' && (flags&SIMPLE)) {
	reginsert(PLUS, ret);
	ret->flags = 0;
	PL_regnaughty += 3;
    }
    else if (op == '+') {
	min = 1;
	goto do_curly;
    }
    else if (op == '?') {
	min = 0; max = 1;
	goto do_curly;
    }
  nest_check:
    if (ckWARN(WARN_REGEXP) && !SIZE_ONLY && !(flags&HASWIDTH) && max > REG_INFTY/3) {
	vWARN3(PL_regcomp_parse,
	       "%.*s matches null string many times",
	       PL_regcomp_parse - origparse,
	       origparse);
    }

    if (*PL_regcomp_parse == '?') {
	nextchar();
	reginsert(MINMOD, ret);
	regtail(ret, ret + NODE_STEP_REGNODE);
    }
    if (ISMULT2(PL_regcomp_parse)) {
	PL_regcomp_parse++;
	vFAIL("Nested quantifiers");
    }

    return(ret);
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 *
 * [Yes, it is worth fixing, some scripts can run twice the speed.] */
STATIC regnode *
S_regatom(pTHX_ I32 *flagp)
{
    register regnode *ret = 0;
    I32 flags;

    *flagp = WORST;		/* Tentatively. */

tryagain:
    switch (*PL_regcomp_parse) {
    case '^':
	PL_seen_zerolen++;
	nextchar();
	if (PL_regflags & PMf_MULTILINE)
	    ret = reg_node(MBOL);
	else if (PL_regflags & PMf_SINGLELINE)
	    ret = reg_node(SBOL);
	else
	    ret = reg_node(BOL);
	break;
    case '$':
	nextchar();
	if (*PL_regcomp_parse) 
	    PL_seen_zerolen++;
	if (PL_regflags & PMf_MULTILINE)
	    ret = reg_node(MEOL);
	else if (PL_regflags & PMf_SINGLELINE)
	    ret = reg_node(SEOL);
	else
	    ret = reg_node(EOL);
	break;
    case '.':
	nextchar();
	if (UTF) {
	    if (PL_regflags & PMf_SINGLELINE)
		ret = reg_node(SANYUTF8);
	    else
		ret = reg_node(ANYUTF8);
	    *flagp |= HASWIDTH;
	}
	else {
	    if (PL_regflags & PMf_SINGLELINE)
		ret = reg_node(SANY);
	    else
		ret = reg_node(REG_ANY);
	    *flagp |= HASWIDTH|SIMPLE;
	}
	PL_regnaughty++;
	break;
    case '[':
    {
	char *oregcomp_parse = ++PL_regcomp_parse;
	ret = (UTF ? regclassutf8() : regclass());
	if (*PL_regcomp_parse != ']') {
	    PL_regcomp_parse = oregcomp_parse;
	    vFAIL("Unmatched [");
	}
	nextchar();
	*flagp |= HASWIDTH|SIMPLE;
	break;
    }
    case '(':
	nextchar();
	ret = reg(1, &flags);
	if (ret == NULL) {
		if (flags & TRYAGAIN) {
		    if (PL_regcomp_parse == PL_regxend) {
			 /* Make parent create an empty node if needed. */
			*flagp |= TRYAGAIN;
			return(NULL);
		    }
		    goto tryagain;
		}
		return(NULL);
	}
	*flagp |= flags&(HASWIDTH|SPSTART|SIMPLE);
	break;
    case '|':
    case ')':
	if (flags & TRYAGAIN) {
	    *flagp |= TRYAGAIN;
	    return NULL;
	}
	vFAIL("Internal urp");
				/* Supposed to be caught earlier. */
	break;
    case '{':
	if (!regcurly(PL_regcomp_parse)) {
	    PL_regcomp_parse++;
	    goto defchar;
	}
	/* FALL THROUGH */
    case '?':
    case '+':
    case '*':
	PL_regcomp_parse++;
	vFAIL("Quantifier follows nothing");
	break;
    case '\\':
	switch (*++PL_regcomp_parse) {
	case 'A':
	    PL_seen_zerolen++;
	    ret = reg_node(SBOL);
	    *flagp |= SIMPLE;
	    nextchar();
	    break;
	case 'G':
	    ret = reg_node(GPOS);
	    PL_regseen |= REG_SEEN_GPOS;
	    *flagp |= SIMPLE;
	    nextchar();
	    break;
	case 'Z':
	    ret = reg_node(SEOL);
	    *flagp |= SIMPLE;
	    nextchar();
	    break;
	case 'z':
	    ret = reg_node(EOS);
	    *flagp |= SIMPLE;
	    PL_seen_zerolen++;		/* Do not optimize RE away */
	    nextchar();
	    break;
	case 'C':
	    ret = reg_node(SANY);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    break;
	case 'X':
	    ret = reg_node(CLUMP);
	    *flagp |= HASWIDTH;
	    nextchar();
	    if (UTF && !PL_utf8_mark)
		is_utf8_mark((U8*)"~");		/* preload table */
	    break;
	case 'w':
	    ret = reg_node(
		UTF
		    ? (LOC ? ALNUMLUTF8 : ALNUMUTF8)
		    : (LOC ? ALNUML     : ALNUM));
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    if (UTF && !PL_utf8_alnum)
		is_utf8_alnum((U8*)"a");	/* preload table */
	    break;
	case 'W':
	    ret = reg_node(
		UTF
		    ? (LOC ? NALNUMLUTF8 : NALNUMUTF8)
		    : (LOC ? NALNUML     : NALNUM));
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    if (UTF && !PL_utf8_alnum)
		is_utf8_alnum((U8*)"a");	/* preload table */
	    break;
	case 'b':
	    PL_seen_zerolen++;
	    PL_regseen |= REG_SEEN_LOOKBEHIND;
	    ret = reg_node(
		UTF
		    ? (LOC ? BOUNDLUTF8 : BOUNDUTF8)
		    : (LOC ? BOUNDL     : BOUND));
	    *flagp |= SIMPLE;
	    nextchar();
	    if (UTF && !PL_utf8_alnum)
		is_utf8_alnum((U8*)"a");	/* preload table */
	    break;
	case 'B':
	    PL_seen_zerolen++;
	    PL_regseen |= REG_SEEN_LOOKBEHIND;
	    ret = reg_node(
		UTF
		    ? (LOC ? NBOUNDLUTF8 : NBOUNDUTF8)
		    : (LOC ? NBOUNDL     : NBOUND));
	    *flagp |= SIMPLE;
	    nextchar();
	    if (UTF && !PL_utf8_alnum)
		is_utf8_alnum((U8*)"a");	/* preload table */
	    break;
	case 's':
	    ret = reg_node(
		UTF
		    ? (LOC ? SPACELUTF8 : SPACEUTF8)
		    : (LOC ? SPACEL     : SPACE));
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    if (UTF && !PL_utf8_space)
		is_utf8_space((U8*)" ");	/* preload table */
	    break;
	case 'S':
	    ret = reg_node(
		UTF
		    ? (LOC ? NSPACELUTF8 : NSPACEUTF8)
		    : (LOC ? NSPACEL     : NSPACE));
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    if (UTF && !PL_utf8_space)
		is_utf8_space((U8*)" ");	/* preload table */
	    break;
	case 'd':
	    ret = reg_node(UTF ? DIGITUTF8 : DIGIT);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    if (UTF && !PL_utf8_digit)
		is_utf8_digit((U8*)"1");	/* preload table */
	    break;
	case 'D':
	    ret = reg_node(UTF ? NDIGITUTF8 : NDIGIT);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    if (UTF && !PL_utf8_digit)
		is_utf8_digit((U8*)"1");	/* preload table */
	    break;
	case 'p':
	case 'P':
	    {	/* a lovely hack--pretend we saw [\pX] instead */
		char* oldregxend = PL_regxend;

		if (PL_regcomp_parse[1] == '{') {
		    PL_regxend = strchr(PL_regcomp_parse, '}');
		    if (!PL_regxend) {
			PL_regcomp_parse += 2;
			PL_regxend = oldregxend;
			vFAIL("Missing right brace on \\p{}");
		    }
		    PL_regxend++;
		}
		else
		    PL_regxend = PL_regcomp_parse + 2;
		PL_regcomp_parse--;

		ret = regclassutf8();

		PL_regxend = oldregxend;
		PL_regcomp_parse--;
		nextchar();
		*flagp |= HASWIDTH|SIMPLE;
	    }
	    break;
	case 'n':
	case 'r':
	case 't':
	case 'f':
	case 'e':
	case 'a':
	case 'x':
	case 'c':
	case '0':
	    goto defchar;
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    {
		I32 num = atoi(PL_regcomp_parse);

		if (num > 9 && num >= PL_regnpar)
		    goto defchar;
		else {
		    while (isDIGIT(*PL_regcomp_parse))
			PL_regcomp_parse++;

		    if (!SIZE_ONLY && num > PL_regcomp_rx->nparens)
			vFAIL("Reference to nonexistent group");
		    PL_regsawback = 1;
		    ret = reganode(FOLD
				   ? (LOC ? REFFL : REFF)
				   : REF, num);
		    *flagp |= HASWIDTH;
		    PL_regcomp_parse--;
		    nextchar();
		}
	    }
	    break;
	case '\0':
	    if (PL_regcomp_parse >= PL_regxend)
		FAIL("Trailing \\");
	    /* FALL THROUGH */
	default:
	    /* Do not generate `unrecognized' warnings here, we fall
	       back into the quick-grab loop below */
	    goto defchar;
	}
	break;

    case '#':
	if (PL_regflags & PMf_EXTENDED) {
	    while (PL_regcomp_parse < PL_regxend && *PL_regcomp_parse != '\n') PL_regcomp_parse++;
	    if (PL_regcomp_parse < PL_regxend)
		goto tryagain;
	}
	/* FALL THROUGH */

    default: {
	    register STRLEN len;
	    register UV ender;
	    register char *p;
	    char *oldp, *s;
	    STRLEN numlen;

	    PL_regcomp_parse++;

	defchar:
	    ret = reg_node(FOLD
			  ? (LOC ? EXACTFL : EXACTF)
			  : EXACT);
	    s = STRING(ret);
	    for (len = 0, p = PL_regcomp_parse - 1;
	      len < 127 && p < PL_regxend;
	      len++)
	    {
		oldp = p;

		if (PL_regflags & PMf_EXTENDED)
		    p = regwhite(p, PL_regxend);
		switch (*p) {
		case '^':
		case '$':
		case '.':
		case '[':
		case '(':
		case ')':
		case '|':
		    goto loopdone;
		case '\\':
		    switch (*++p) {
		    case 'A':
		    case 'G':
		    case 'Z':
		    case 'z':
		    case 'w':
		    case 'W':
		    case 'b':
		    case 'B':
		    case 's':
		    case 'S':
		    case 'd':
		    case 'D':
		    case 'p':
		    case 'P':
			--p;
			goto loopdone;
		    case 'n':
			ender = '\n';
			p++;
			break;
		    case 'r':
			ender = '\r';
			p++;
			break;
		    case 't':
			ender = '\t';
			p++;
			break;
		    case 'f':
			ender = '\f';
			p++;
			break;
		    case 'e':
#ifdef ASCIIish
			  ender = '\033';
#else
			  ender = '\047';
#endif
			p++;
			break;
		    case 'a':
#ifdef ASCIIish
			  ender = '\007';
#else
			  ender = '\057';
#endif
			p++;
			break;
		    case 'x':
			if (*++p == '{') {
			    char* e = strchr(p, '}');
	 
			    if (!e) {
				PL_regcomp_parse = p + 1;
				vFAIL("Missing right brace on \\x{}");
			    }
			    else {
				numlen = 1;	/* allow underscores */
				ender = (UV)scan_hex(p + 1, e - p - 1, &numlen);
				/* numlen is generous */
				if (numlen + len >= 127) {
				    p--;
				    goto loopdone;
				}
				p = e + 1;
			    }
			}
			else {
			    numlen = 0;		/* disallow underscores */
			    ender = (UV)scan_hex(p, 2, &numlen);
			    p += numlen;
			}
			break;
		    case 'c':
			p++;
			ender = UCHARAT(p++);
			ender = toCTRL(ender);
			break;
		    case '0': case '1': case '2': case '3':case '4':
		    case '5': case '6': case '7': case '8':case '9':
			if (*p == '0' ||
			  (isDIGIT(p[1]) && atoi(p) >= PL_regnpar) ) {
			    numlen = 0;		/* disallow underscores */
			    ender = (UV)scan_oct(p, 3, &numlen);
			    p += numlen;
			}
			else {
			    --p;
			    goto loopdone;
			}
			break;
		    case '\0':
			if (p >= PL_regxend)
			    FAIL("Trailing \\");
			/* FALL THROUGH */
		    default:
			if (!SIZE_ONLY && ckWARN(WARN_REGEXP) && isALPHA(*p))
			    vWARN2(p +1, "Unrecognized escape \\%c passed through", *p);
			goto normal_default;
		    }
		    break;
		default:
		  normal_default:
		    if (UTF8_IS_START(*p) && UTF) {
			ender = utf8_to_uv((U8*)p, PL_regxend - p,
					   &numlen, 0);
			p += numlen;
		    }
		    else
			ender = *p++;
		    break;
		}
		if (PL_regflags & PMf_EXTENDED)
		    p = regwhite(p, PL_regxend);
		if (UTF && FOLD) {
		    if (LOC)
			ender = toLOWER_LC_uni(ender);
		    else
			ender = toLOWER_uni(ender);
		}
		if (ISMULT2(p)) { /* Back off on ?+*. */
		    if (len)
			p = oldp;
		    /* ender is a Unicode value so it can be > 0xff --
		     * in other words, do not use UTF8_IS_CONTINUED(). */
		    else if (ender >= 0x80 && UTF) {
			reguni(ender, s, &numlen);
			s += numlen;
			len += numlen;
		    }
		    else {
			len++;
			REGC(ender, s++);
		    }
		    break;
		}
		/* ender is a Unicode value so it can be > 0xff --
		 * in other words, do not use UTF8_IS_CONTINUED(). */
		if (ender >= 0x80 && UTF) {
		    reguni(ender, s, &numlen);
		    s += numlen;
		    len += numlen - 1;
		}
		else
		    REGC(ender, s++);
	    }
	loopdone:
	    PL_regcomp_parse = p - 1;
	    nextchar();
	    {
		/* len is STRLEN which is unsigned, need to copy to signed */
		IV iv = len;
		if (iv < 0)
		    vFAIL("Internal disaster");
	    }
	    if (len > 0)
		*flagp |= HASWIDTH;
	    if (len == 1)
		*flagp |= SIMPLE;
	    if (!SIZE_ONLY)
		STR_LEN(ret) = len;
	    if (SIZE_ONLY)
		PL_regsize += STR_SZ(len);
	    else
		PL_regcode += STR_SZ(len);
	}
	break;
    }

    return(ret);
}

STATIC char *
S_regwhite(pTHX_ char *p, char *e)
{
    while (p < e) {
	if (isSPACE(*p))
	    ++p;
	else if (*p == '#') {
	    do {
		p++;
	    } while (p < e && *p != '\n');
	}
	else
	    break;
    }
    return p;
}

/* Parse POSIX character classes: [[:foo:]], [[=foo=]], [[.foo.]].
   Character classes ([:foo:]) can also be negated ([:^foo:]).
   Returns a named class id (ANYOF_XXX) if successful, -1 otherwise.
   Equivalence classes ([=foo=]) and composites ([.foo.]) are parsed,
   but trigger warnings because they are currently unimplemented. */
STATIC I32
S_regpposixcc(pTHX_ I32 value)
{
    char *posixcc = 0;
    I32 namedclass = OOB_NAMEDCLASS;

    if (value == '[' && PL_regcomp_parse + 1 < PL_regxend &&
	/* I smell either [: or [= or [. -- POSIX has been here, right? */
	(*PL_regcomp_parse == ':' ||
	 *PL_regcomp_parse == '=' ||
	 *PL_regcomp_parse == '.')) {
	char  c = *PL_regcomp_parse;
	char* s = PL_regcomp_parse++;
	    
	while (PL_regcomp_parse < PL_regxend && *PL_regcomp_parse != c)
	    PL_regcomp_parse++;
	if (PL_regcomp_parse == PL_regxend)
	    /* Grandfather lone [:, [=, [. */
	    PL_regcomp_parse = s;
	else {
	    char* t = PL_regcomp_parse++; /* skip over the c */

  	    if (*PL_regcomp_parse == ']') {
  		PL_regcomp_parse++; /* skip over the ending ] */
  		posixcc = s + 1;
		if (*s == ':') {
		    I32 complement = *posixcc == '^' ? *posixcc++ : 0;
		    I32 skip = 5; /* the most common skip */

		    switch (*posixcc) {
		    case 'a':
			if (strnEQ(posixcc, "alnum", 5))
			    namedclass =
				complement ? ANYOF_NALNUMC : ANYOF_ALNUMC;
			else if (strnEQ(posixcc, "alpha", 5))
			    namedclass =
				complement ? ANYOF_NALPHA : ANYOF_ALPHA;
			else if (strnEQ(posixcc, "ascii", 5))
			    namedclass =
				complement ? ANYOF_NASCII : ANYOF_ASCII;
			break;
		    case 'b':
			if (strnEQ(posixcc, "blank", 5))
			    namedclass =
				complement ? ANYOF_NBLANK : ANYOF_BLANK;
			break;
		    case 'c':
			if (strnEQ(posixcc, "cntrl", 5))
			    namedclass =
				complement ? ANYOF_NCNTRL : ANYOF_CNTRL;
			break;
		    case 'd':
			if (strnEQ(posixcc, "digit", 5))
			    namedclass =
				complement ? ANYOF_NDIGIT : ANYOF_DIGIT;
			break;
		    case 'g':
			if (strnEQ(posixcc, "graph", 5))
			    namedclass =
				complement ? ANYOF_NGRAPH : ANYOF_GRAPH;
			break;
		    case 'l':
			if (strnEQ(posixcc, "lower", 5))
			    namedclass =
				complement ? ANYOF_NLOWER : ANYOF_LOWER;
			break;
		    case 'p':
			if (strnEQ(posixcc, "print", 5))
			    namedclass =
				complement ? ANYOF_NPRINT : ANYOF_PRINT;
			else if (strnEQ(posixcc, "punct", 5))
			    namedclass =
				complement ? ANYOF_NPUNCT : ANYOF_PUNCT;
			break;
		    case 's':
			if (strnEQ(posixcc, "space", 5))
			    namedclass =
				complement ? ANYOF_NPSXSPC : ANYOF_PSXSPC;
			break;
		    case 'u':
			if (strnEQ(posixcc, "upper", 5))
			    namedclass =
				complement ? ANYOF_NUPPER : ANYOF_UPPER;
 			break;
		    case 'w': /* this is not POSIX, this is the Perl \w */
			if (strnEQ(posixcc, "word", 4)) {
			    namedclass =
				complement ? ANYOF_NALNUM : ANYOF_ALNUM;
			    skip = 4;
			}
			break;
		    case 'x':
			if (strnEQ(posixcc, "xdigit", 6)) {
			    namedclass =
				complement ? ANYOF_NXDIGIT : ANYOF_XDIGIT;
			    skip = 6;
			}
			break;
		    }
		    if (namedclass == OOB_NAMEDCLASS ||
			posixcc[skip] != ':' ||
			posixcc[skip+1] != ']')
		    {
			Simple_vFAIL3("POSIX class [:%.*s:] unknown",
				      t - s - 1, s + 1);
		    }
		} else if (!SIZE_ONLY) {
		    /* [[=foo=]] and [[.foo.]] are still future. */

		    /* adjust PL_regcomp_parse so the warning shows after
		       the class closes */
		    while (*PL_regcomp_parse && *PL_regcomp_parse != ']')
			PL_regcomp_parse++;
		    Simple_vFAIL3("POSIX syntax [%c %c] is reserved for future extensions", c, c);
		}
	    } else {
		/* Maternal grandfather:
		 * "[:" ending in ":" but not in ":]" */
		PL_regcomp_parse = s;
	    }
	}
    }

    return namedclass;
}

STATIC void
S_checkposixcc(pTHX)
{
    if (!SIZE_ONLY && ckWARN(WARN_REGEXP) &&
	(*PL_regcomp_parse == ':' ||
	 *PL_regcomp_parse == '=' ||
	 *PL_regcomp_parse == '.')) {
	char *s = PL_regcomp_parse;
 	char  c = *s++;

	while(*s && isALNUM(*s))
	    s++;
	if (*s && c == *s && s[1] == ']') {
	    vWARN3(s+2, "POSIX syntax [%c %c] belongs inside character classes", c, c);

	    /* [[=foo=]] and [[.foo.]] are still future. */
	    if (c == '=' || c == '.')
	    {
		/* adjust PL_regcomp_parse so the error shows after
		   the class closes */
		while (*PL_regcomp_parse && *PL_regcomp_parse++ != ']')
		    ;
		Simple_vFAIL3("POSIX syntax [%c %c] is reserved for future extensions", c, c);
	    }
	}
    }
}

STATIC regnode *
S_regclass(pTHX)
{
    register U32 value;
    register I32 lastvalue = OOB_CHAR8;
    register I32 range = 0;
    register regnode *ret;
    STRLEN numlen;
    I32 namedclass;
    char *rangebegin;
    bool need_class = 0;

    ret = reg_node(ANYOF);
    if (SIZE_ONLY)
	PL_regsize += ANYOF_SKIP;
    else {
	ret->flags = 0;
	ANYOF_BITMAP_ZERO(ret);
 	PL_regcode += ANYOF_SKIP;
	if (FOLD)
	    ANYOF_FLAGS(ret) |= ANYOF_FOLD;
	if (LOC)
	    ANYOF_FLAGS(ret) |= ANYOF_LOCALE;
    }
    if (*PL_regcomp_parse == '^') {	/* Complement of range. */
	PL_regnaughty++;
	PL_regcomp_parse++;
	if (!SIZE_ONLY)
	    ANYOF_FLAGS(ret) |= ANYOF_INVERT;
    }

    if (!SIZE_ONLY && ckWARN(WARN_REGEXP))
	checkposixcc();

    if (*PL_regcomp_parse == ']' || *PL_regcomp_parse == '-')
	goto skipcond;		/* allow 1st char to be ] or - */
    while (PL_regcomp_parse < PL_regxend && *PL_regcomp_parse != ']') {
       skipcond:
	namedclass = OOB_NAMEDCLASS;
	if (!range)
	    rangebegin = PL_regcomp_parse;
	value = UCHARAT(PL_regcomp_parse++);
	if (value == '[')
	    namedclass = regpposixcc(value);
	else if (value == '\\') {
	    value = UCHARAT(PL_regcomp_parse++);
	    /* Some compilers cannot handle switching on 64-bit integer
	     * values, therefore the 'value' cannot be an UV. --jhi */
	    switch (value) {
	    case 'w':	namedclass = ANYOF_ALNUM;	break;
	    case 'W':	namedclass = ANYOF_NALNUM;	break;
	    case 's':	namedclass = ANYOF_SPACE;	break;
	    case 'S':	namedclass = ANYOF_NSPACE;	break;
	    case 'd':	namedclass = ANYOF_DIGIT;	break;
	    case 'D':	namedclass = ANYOF_NDIGIT;	break;
	    case 'n':	value = '\n';			break;
	    case 'r':	value = '\r';			break;
	    case 't':	value = '\t';			break;
	    case 'f':	value = '\f';			break;
	    case 'b':	value = '\b';			break;
#ifdef ASCIIish
	    case 'e':	value = '\033';			break;
	    case 'a':	value = '\007';			break;
#else
	    case 'e':	value = '\047';			break;
	    case 'a':	value = '\057';			break;
#endif
	    case 'x':
		numlen = 0;		/* disallow underscores */
		value = (UV)scan_hex(PL_regcomp_parse, 2, &numlen);
		PL_regcomp_parse += numlen;
		break;
	    case 'c':
		value = UCHARAT(PL_regcomp_parse++);
		value = toCTRL(value);
		break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
		numlen = 0;		/* disallow underscores */
		value = (UV)scan_oct(--PL_regcomp_parse, 3, &numlen);
		PL_regcomp_parse += numlen;
		break;
	    default:
		if (!SIZE_ONLY && ckWARN(WARN_REGEXP) && isALPHA(value))

		    vWARN2(PL_regcomp_parse, "Unrecognized escape \\%c in character class passed through", (int)value);
		break;
	    }
	}
	if (namedclass > OOB_NAMEDCLASS) {
	    if (!need_class && !SIZE_ONLY)
		ANYOF_CLASS_ZERO(ret);
	    need_class = 1;
	    if (range) { /* a-\d, a-[:digit:] */
		if (!SIZE_ONLY) {
		    if (ckWARN(WARN_REGEXP))
			vWARN4(PL_regcomp_parse,
			       "False [] range \"%*.*s\"",
			       PL_regcomp_parse - rangebegin,
			       PL_regcomp_parse - rangebegin,
			       rangebegin);
		    ANYOF_BITMAP_SET(ret, lastvalue);
		    ANYOF_BITMAP_SET(ret, '-');
		}
		range = 0; /* this is not a true range */
	    }
	    if (!SIZE_ONLY) {
		switch (namedclass) {
		case ANYOF_ALNUM:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_ALNUM);
		    else {
			for (value = 0; value < 256; value++)
			    if (isALNUM(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NALNUM:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NALNUM);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isALNUM(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_SPACE:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_SPACE);
		    else {
			for (value = 0; value < 256; value++)
			    if (isSPACE(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NSPACE:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NSPACE);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isSPACE(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_DIGIT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_DIGIT);
		    else {
			for (value = '0'; value <= '9'; value++)
			    ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NDIGIT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NDIGIT);
		    else {
			for (value = 0; value < '0'; value++)
			    ANYOF_BITMAP_SET(ret, value);
			for (value = '9' + 1; value < 256; value++)
			    ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NALNUMC:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NALNUMC);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isALNUMC(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_ALNUMC:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_ALNUMC);
		    else {
			for (value = 0; value < 256; value++)
			    if (isALNUMC(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_ALPHA:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_ALPHA);
		    else {
			for (value = 0; value < 256; value++)
			    if (isALPHA(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NALPHA:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NALPHA);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isALPHA(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_ASCII:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_ASCII);
		    else {
#ifdef ASCIIish
			for (value = 0; value < 128; value++)
			    ANYOF_BITMAP_SET(ret, value);
#else  /* EBCDIC */
			for (value = 0; value < 256; value++)
			    if (isASCII(value))
				ANYOF_BITMAP_SET(ret, value);
#endif /* EBCDIC */
		    }
		    break;
		case ANYOF_NASCII:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NASCII);
		    else {
#ifdef ASCIIish
			for (value = 128; value < 256; value++)
			    ANYOF_BITMAP_SET(ret, value);
#else  /* EBCDIC */
			for (value = 0; value < 256; value++)
			    if (!isASCII(value))
				ANYOF_BITMAP_SET(ret, value);
#endif /* EBCDIC */
		    }
		    break;
		case ANYOF_BLANK:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_BLANK);
		    else {
			for (value = 0; value < 256; value++)
			    if (isBLANK(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NBLANK:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NBLANK);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isBLANK(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_CNTRL:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_CNTRL);
		    else {
			for (value = 0; value < 256; value++)
			    if (isCNTRL(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    lastvalue = OOB_CHAR8;
		    break;
		case ANYOF_NCNTRL:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NCNTRL);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isCNTRL(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_GRAPH:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_GRAPH);
		    else {
			for (value = 0; value < 256; value++)
			    if (isGRAPH(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NGRAPH:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NGRAPH);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isGRAPH(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_LOWER:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_LOWER);
		    else {
			for (value = 0; value < 256; value++)
			    if (isLOWER(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NLOWER:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NLOWER);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isLOWER(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_PRINT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_PRINT);
		    else {
			for (value = 0; value < 256; value++)
			    if (isPRINT(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NPRINT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NPRINT);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isPRINT(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_PSXSPC:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_PSXSPC);
		    else {
			for (value = 0; value < 256; value++)
			    if (isPSXSPC(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NPSXSPC:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NPSXSPC);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isPSXSPC(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_PUNCT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_PUNCT);
		    else {
			for (value = 0; value < 256; value++)
			    if (isPUNCT(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NPUNCT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NPUNCT);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isPUNCT(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_UPPER:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_UPPER);
		    else {
			for (value = 0; value < 256; value++)
			    if (isUPPER(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NUPPER:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NUPPER);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isUPPER(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_XDIGIT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_XDIGIT);
		    else {
			for (value = 0; value < 256; value++)
			    if (isXDIGIT(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		case ANYOF_NXDIGIT:
		    if (LOC)
			ANYOF_CLASS_SET(ret, ANYOF_NXDIGIT);
		    else {
			for (value = 0; value < 256; value++)
			    if (!isXDIGIT(value))
				ANYOF_BITMAP_SET(ret, value);
		    }
		    break;
		default:
		    vFAIL("Invalid [::] class");
		    break;
		}
		if (LOC)
		    ANYOF_FLAGS(ret) |= ANYOF_CLASS;
		continue;
	    }
	}
	if (range) {
	    if (lastvalue > value) /* b-a */ {
		Simple_vFAIL4("Invalid [] range \"%*.*s\"",
			      PL_regcomp_parse - rangebegin,
			      PL_regcomp_parse - rangebegin,
			      rangebegin);
	    }
	    range = 0;
	}
	else {
	    lastvalue = value;
	    if (*PL_regcomp_parse == '-' && PL_regcomp_parse+1 < PL_regxend &&
		PL_regcomp_parse[1] != ']') {
		PL_regcomp_parse++;
		if (namedclass > OOB_NAMEDCLASS) { /* \w-, [:word:]- */
		    if (ckWARN(WARN_REGEXP))
			vWARN4(PL_regcomp_parse,
			       "False [] range \"%*.*s\"",
			       PL_regcomp_parse - rangebegin,
			       PL_regcomp_parse - rangebegin,
			       rangebegin);
		    if (!SIZE_ONLY)
			ANYOF_BITMAP_SET(ret, '-');
		} else
		    range = 1;
		continue;	/* do it next time */
	    }
	}
	/* now is the next time */
	if (!SIZE_ONLY) {
#ifndef ASCIIish /* EBCDIC, for example. */
	    if ((isLOWER(lastvalue) && isLOWER(value)) ||
		(isUPPER(lastvalue) && isUPPER(value)))
	    {
		I32 i;
 		if (isLOWER(lastvalue)) {
 		    for (i = lastvalue; i <= value; i++)
			if (isLOWER(i))
			    ANYOF_BITMAP_SET(ret, i);
 		} else {
 		    for (i = lastvalue; i <= value; i++)
			if (isUPPER(i))
			    ANYOF_BITMAP_SET(ret, i);
		}
	    }
	    else
#endif
		for ( ; lastvalue <= value; lastvalue++)
		    ANYOF_BITMAP_SET(ret, lastvalue);
        }
	range = 0;
    }
    if (need_class) {
	if (SIZE_ONLY)
	    PL_regsize += ANYOF_CLASS_ADD_SKIP;
	else
	    PL_regcode += ANYOF_CLASS_ADD_SKIP;
    }
    /* optimize case-insensitive simple patterns (e.g. /[a-z]/i) */
    if (!SIZE_ONLY &&
	(ANYOF_FLAGS(ret) & (ANYOF_FLAGS_ALL ^ ANYOF_INVERT)) == ANYOF_FOLD) {
	for (value = 0; value < 256; ++value) {
	    if (ANYOF_BITMAP_TEST(ret, value)) {
		I32 cf = PL_fold[value];
		ANYOF_BITMAP_SET(ret, cf);
	    }
	}
	ANYOF_FLAGS(ret) &= ~ANYOF_FOLD;
    }
    /* optimize inverted simple patterns (e.g. [^a-z]) */
    if (!SIZE_ONLY && (ANYOF_FLAGS(ret) & ANYOF_FLAGS_ALL) == ANYOF_INVERT) {
	for (value = 0; value < ANYOF_BITMAP_SIZE; ++value)
	    ANYOF_BITMAP(ret)[value] ^= ANYOF_FLAGS_ALL;
	ANYOF_FLAGS(ret) = 0;
    }
    return ret;
}

STATIC regnode *
S_regclassutf8(pTHX)
{
    register char *e;
    register U32 value;
    register U32 lastvalue = OOB_UTF8;
    register I32 range = 0;
    register regnode *ret;
    STRLEN numlen;
    I32 n;
    SV *listsv;
    U8 flags = 0;
    I32 namedclass;
    char *rangebegin;

    if (*PL_regcomp_parse == '^') {	/* Complement of range. */
	PL_regnaughty++;
	PL_regcomp_parse++;
	if (!SIZE_ONLY)
	    flags |= ANYOF_INVERT;
    }
    if (!SIZE_ONLY) {
	if (FOLD)
	    flags |= ANYOF_FOLD;
	if (LOC)
	    flags |= ANYOF_LOCALE;
	listsv = newSVpvn("# comment\n", 10);
    }

    if (!SIZE_ONLY && ckWARN(WARN_REGEXP))
	checkposixcc();

    if (*PL_regcomp_parse == ']' || *PL_regcomp_parse == '-')
	goto skipcond;		/* allow 1st char to be ] or - */

    while (PL_regcomp_parse < PL_regxend && *PL_regcomp_parse != ']') {
       skipcond:
	namedclass = OOB_NAMEDCLASS;
	if (!range)
	    rangebegin = PL_regcomp_parse;
	value = utf8_to_uv((U8*)PL_regcomp_parse,
			       PL_regxend - PL_regcomp_parse,
			       &numlen, 0);
	PL_regcomp_parse += numlen;
	if (value == '[')
	    namedclass = regpposixcc(value);
	else if (value == '\\') {
	    value = (U32)utf8_to_uv((U8*)PL_regcomp_parse,
					PL_regxend - PL_regcomp_parse,
					&numlen, 0);
	    PL_regcomp_parse += numlen;
	    /* Some compilers cannot handle switching on 64-bit integer
	     * values, therefore value cannot be an UV.  Yes, this will
	     * be a problem later if we want switch on Unicode.  --jhi */
	    switch (value) {
	    case 'w':		namedclass = ANYOF_ALNUM;		break;
	    case 'W':		namedclass = ANYOF_NALNUM;		break;
	    case 's':		namedclass = ANYOF_SPACE;		break;
	    case 'S':		namedclass = ANYOF_NSPACE;		break;
	    case 'd':		namedclass = ANYOF_DIGIT;		break;
	    case 'D':		namedclass = ANYOF_NDIGIT;		break;
	    case 'p':
	    case 'P':
		if (*PL_regcomp_parse == '{') {
		    e = strchr(PL_regcomp_parse++, '}');
                    if (!e)
                        vFAIL("Missing right brace on \\p{}");
		    n = e - PL_regcomp_parse;
		}
		else {
		    e = PL_regcomp_parse;
		    n = 1;
		}
		if (!SIZE_ONLY) {
		    if (value == 'p')
			Perl_sv_catpvf(aTHX_ listsv,
				       "+utf8::%.*s\n", (int)n, PL_regcomp_parse);
		    else
			Perl_sv_catpvf(aTHX_ listsv,
				       "!utf8::%.*s\n", (int)n, PL_regcomp_parse);
		}
		PL_regcomp_parse = e + 1;
		lastvalue = OOB_UTF8;
		continue;
	    case 'n':		value = '\n';		break;
	    case 'r':		value = '\r';		break;
	    case 't':		value = '\t';		break;
	    case 'f':		value = '\f';		break;
	    case 'b':		value = '\b';		break;
#ifdef ASCIIish
	    case 'e':		value = '\033';		break;
	    case 'a':		value = '\007';		break;
#else
	    case 'e':		value = '\047';		break;
	    case 'a':		value = '\057';		break;
#endif
	    case 'x':
		if (*PL_regcomp_parse == '{') {
		    e = strchr(PL_regcomp_parse++, '}');
                    if (!e) 
                        vFAIL("Missing right brace on \\x{}");
		    numlen = 1;		/* allow underscores */
		    value = (UV)scan_hex(PL_regcomp_parse,
				     e - PL_regcomp_parse,
				     &numlen);
		    PL_regcomp_parse = e + 1;
		}
		else {
		    numlen = 0;		/* disallow underscores */
		    value = (UV)scan_hex(PL_regcomp_parse, 2, &numlen);
		    PL_regcomp_parse += numlen;
		}
		break;
	    case 'c':
		value = UCHARAT(PL_regcomp_parse++);
		value = toCTRL(value);
		break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
		numlen = 0;		/* disallow underscores */
		value = (UV)scan_oct(--PL_regcomp_parse, 3, &numlen);
		PL_regcomp_parse += numlen;
		break;
	    default:
		if (!SIZE_ONLY && ckWARN(WARN_REGEXP) && isALPHA(value))
		    vWARN2(PL_regcomp_parse,
			   "Unrecognized escape \\%c in character class passed through",
			   (int)value);
		break;
	    }
	}
	if (namedclass > OOB_NAMEDCLASS) {
	    if (range) { /* a-\d, a-[:digit:] */
		if (!SIZE_ONLY) {
		    if (ckWARN(WARN_REGEXP))
			vWARN4(PL_regcomp_parse,
			       "False [] range \"%*.*s\"",
			       PL_regcomp_parse - rangebegin,
			       PL_regcomp_parse - rangebegin,
			       rangebegin);
		    Perl_sv_catpvf(aTHX_ listsv,
				   /* 0x002D is Unicode for '-' */
				   "%04"UVxf"\n002D\n", (UV)lastvalue);
		}
		range = 0;
	    }
	    if (!SIZE_ONLY) {
		switch (namedclass) {
		case ANYOF_ALNUM:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsWord\n");	break;
		case ANYOF_NALNUM:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsWord\n");	break;
		case ANYOF_ALNUMC:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsAlnum\n");	break;
		case ANYOF_NALNUMC:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsAlnum\n");	break;
		case ANYOF_ALPHA:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsAlpha\n");	break;
		case ANYOF_NALPHA:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsAlpha\n");	break;
		case ANYOF_ASCII:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsASCII\n");	break;
		case ANYOF_NASCII:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsASCII\n");	break;
		case ANYOF_CNTRL:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsCntrl\n");	break;
		case ANYOF_NCNTRL:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsCntrl\n");	break;
		case ANYOF_GRAPH:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsGraph\n");	break;
		case ANYOF_NGRAPH:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsGraph\n");	break;
		case ANYOF_DIGIT:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsDigit\n");	break;
		case ANYOF_NDIGIT:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsDigit\n");	break;
		case ANYOF_LOWER:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsLower\n");	break;
		case ANYOF_NLOWER:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsLower\n");	break;
		case ANYOF_PRINT:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsPrint\n");	break;
		case ANYOF_NPRINT:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsPrint\n");	break;
		case ANYOF_PUNCT:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsPunct\n");	break;
		case ANYOF_NPUNCT:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsPunct\n");	break;
		case ANYOF_SPACE:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsSpacePerl\n");break;
		case ANYOF_NSPACE:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsSpacePerl\n");break;
		case ANYOF_BLANK:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsBlank\n");	break;
		case ANYOF_NBLANK:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsBlank\n");	break;
		case ANYOF_PSXSPC:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsSpace\n");	break;
		case ANYOF_NPSXSPC:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsSpace\n");	break;
		case ANYOF_UPPER:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsUpper\n");	break;
		case ANYOF_NUPPER:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsUpper\n");	break;
		case ANYOF_XDIGIT:
		    Perl_sv_catpvf(aTHX_ listsv, "+utf8::IsXDigit\n");	break;
		case ANYOF_NXDIGIT:
		    Perl_sv_catpvf(aTHX_ listsv, "!utf8::IsXDigit\n");	break;
		}
		continue;
	    }
	}
        if (range) {
	    if (lastvalue > value) { /* b-a */
		Simple_vFAIL4("Invalid [] range \"%*.*s\"",
			      PL_regcomp_parse - rangebegin,
			      PL_regcomp_parse - rangebegin,
			      rangebegin);
	    }
	    range = 0;
	}
	else {
	    lastvalue = value;
	    if (*PL_regcomp_parse == '-' && PL_regcomp_parse+1 < PL_regxend &&
		PL_regcomp_parse[1] != ']') {
		PL_regcomp_parse++;
		if (namedclass > OOB_NAMEDCLASS) { /* \w-, [:word:]- */
		    if (ckWARN(WARN_REGEXP))
			vWARN4(PL_regcomp_parse,
			       "False [] range \"%*.*s\"",
			       PL_regcomp_parse - rangebegin,
			       PL_regcomp_parse - rangebegin,
			       rangebegin);
		    if (!SIZE_ONLY)
			Perl_sv_catpvf(aTHX_ listsv,
				       /* 0x002D is Unicode for '-' */
				       "002D\n");
		} else
		    range = 1;
		continue;	/* do it next time */
	    }
	}
	/* now is the next time */
	if (!SIZE_ONLY)
	    Perl_sv_catpvf(aTHX_ listsv, "%04"UVxf"\t%04"UVxf"\n",
			   (UV)lastvalue, (UV)value);
	range = 0;
    }

    ret = reganode(ANYOFUTF8, 0);

    if (!SIZE_ONLY) {
	SV *rv = swash_init("utf8", "", listsv, 1, 0);
#ifdef DEBUGGING
	AV *av = newAV();
	av_push(av, rv);
	av_push(av, listsv);
	rv = newRV_noinc((SV*)av);
#else
	SvREFCNT_dec(listsv);
#endif
	n = add_data(1,"s");
	PL_regcomp_rx->data->data[n] = (void*)rv;
	ARG1_SET(ret, flags);
	ARG2_SET(ret, n);
    }

    return ret;
}

STATIC char*
S_nextchar(pTHX)
{
    char* retval = PL_regcomp_parse++;

    for (;;) {
	if (*PL_regcomp_parse == '(' && PL_regcomp_parse[1] == '?' &&
		PL_regcomp_parse[2] == '#') {
	    while (*PL_regcomp_parse && *PL_regcomp_parse != ')')
		PL_regcomp_parse++;
	    PL_regcomp_parse++;
	    continue;
	}
	if (PL_regflags & PMf_EXTENDED) {
	    if (isSPACE(*PL_regcomp_parse)) {
		PL_regcomp_parse++;
		continue;
	    }
	    else if (*PL_regcomp_parse == '#') {
		while (*PL_regcomp_parse && *PL_regcomp_parse != '\n')
		    PL_regcomp_parse++;
		PL_regcomp_parse++;
		continue;
	    }
	}
	return retval;
    }
}

/*
- reg_node - emit a node
*/
STATIC regnode *			/* Location. */
S_reg_node(pTHX_ U8 op)
{
    register regnode *ret;
    register regnode *ptr;

    ret = PL_regcode;
    if (SIZE_ONLY) {
	SIZE_ALIGN(PL_regsize);
	PL_regsize += 1;
	return(ret);
    }

    NODE_ALIGN_FILL(ret);
    ptr = ret;
    FILL_ADVANCE_NODE(ptr, op);
    PL_regcode = ptr;

    return(ret);
}

/*
- reganode - emit a node with an argument
*/
STATIC regnode *			/* Location. */
S_reganode(pTHX_ U8 op, U32 arg)
{
    register regnode *ret;
    register regnode *ptr;

    ret = PL_regcode;
    if (SIZE_ONLY) {
	SIZE_ALIGN(PL_regsize);
	PL_regsize += 2;
	return(ret);
    }

    NODE_ALIGN_FILL(ret);
    ptr = ret;
    FILL_ADVANCE_NODE_ARG(ptr, op, arg);
    PL_regcode = ptr;

    return(ret);
}

/*
- reguni - emit (if appropriate) a Unicode character
*/
STATIC void
S_reguni(pTHX_ UV uv, char* s, STRLEN* lenp)
{
    *lenp = SIZE_ONLY ? UNISKIP(uv) : (uv_to_utf8((U8*)s, uv) - (U8*)s);
}

/*
- reginsert - insert an operator in front of already-emitted operand
*
* Means relocating the operand.
*/
STATIC void
S_reginsert(pTHX_ U8 op, regnode *opnd)
{
    register regnode *src;
    register regnode *dst;
    register regnode *place;
    register int offset = regarglen[(U8)op];
    
/* (PL_regkind[(U8)op] == CURLY ? EXTRA_STEP_2ARGS : 0); */

    if (SIZE_ONLY) {
	PL_regsize += NODE_STEP_REGNODE + offset;
	return;
    }

    src = PL_regcode;
    PL_regcode += NODE_STEP_REGNODE + offset;
    dst = PL_regcode;
    while (src > opnd)
	StructCopy(--src, --dst, regnode);

    place = opnd;		/* Op node, where operand used to be. */
    src = NEXTOPER(place);
    FILL_ADVANCE_NODE(place, op);
    Zero(src, offset, regnode);
}

/*
- regtail - set the next-pointer at the end of a node chain of p to val.
*/
STATIC void
S_regtail(pTHX_ regnode *p, regnode *val)
{
    register regnode *scan;
    register regnode *temp;

    if (SIZE_ONLY)
	return;

    /* Find last node. */
    scan = p;
    for (;;) {
	temp = regnext(scan);
	if (temp == NULL)
	    break;
	scan = temp;
    }

    if (reg_off_by_arg[OP(scan)]) {
	ARG_SET(scan, val - scan);
    }
    else {
	NEXT_OFF(scan) = val - scan;
    }
}

/*
- regoptail - regtail on operand of first argument; nop if operandless
*/
STATIC void
S_regoptail(pTHX_ regnode *p, regnode *val)
{
    /* "Operandless" and "op != BRANCH" are synonymous in practice. */
    if (p == NULL || SIZE_ONLY)
	return;
    if (PL_regkind[(U8)OP(p)] == BRANCH) {
	regtail(NEXTOPER(p), val);
    }
    else if ( PL_regkind[(U8)OP(p)] == BRANCHJ) {
	regtail(NEXTOPER(NEXTOPER(p)), val);
    }
    else
	return;
}

/*
 - regcurly - a little FSA that accepts {\d+,?\d*}
 */
STATIC I32
S_regcurly(pTHX_ register char *s)
{
    if (*s++ != '{')
	return FALSE;
    if (!isDIGIT(*s))
	return FALSE;
    while (isDIGIT(*s))
	s++;
    if (*s == ',')
	s++;
    while (isDIGIT(*s))
	s++;
    if (*s != '}')
	return FALSE;
    return TRUE;
}


STATIC regnode *
S_dumpuntil(pTHX_ regnode *start, regnode *node, regnode *last, SV* sv, I32 l)
{
#ifdef DEBUGGING
    register U8 op = EXACT;	/* Arbitrary non-END op. */
    register regnode *next;

    while (op != END && (!last || node < last)) {
	/* While that wasn't END last time... */

	NODE_ALIGN(node);
	op = OP(node);
	if (op == CLOSE)
	    l--;	
	next = regnext(node);
	/* Where, what. */
	if (OP(node) == OPTIMIZED)
	    goto after_print;
	regprop(sv, node);
	PerlIO_printf(Perl_debug_log, "%4"IVdf":%*s%s", (IV)(node - start),
		      (int)(2*l + 1), "", SvPVX(sv));
	if (next == NULL)		/* Next ptr. */
	    PerlIO_printf(Perl_debug_log, "(0)");
	else 
	    PerlIO_printf(Perl_debug_log, "(%"IVdf")", (IV)(next - start));
	(void)PerlIO_putc(Perl_debug_log, '\n');
      after_print:
	if (PL_regkind[(U8)op] == BRANCHJ) {
	    register regnode *nnode = (OP(next) == LONGJMP 
				       ? regnext(next) 
				       : next);
	    if (last && nnode > last)
		nnode = last;
	    node = dumpuntil(start, NEXTOPER(NEXTOPER(node)), nnode, sv, l + 1);
	}
	else if (PL_regkind[(U8)op] == BRANCH) {
	    node = dumpuntil(start, NEXTOPER(node), next, sv, l + 1);
	}
	else if ( op == CURLY) {   /* `next' might be very big: optimizer */
	    node = dumpuntil(start, NEXTOPER(node) + EXTRA_STEP_2ARGS,
			     NEXTOPER(node) + EXTRA_STEP_2ARGS + 1, sv, l + 1);
	}
	else if (PL_regkind[(U8)op] == CURLY && op != CURLYX) {
	    node = dumpuntil(start, NEXTOPER(node) + EXTRA_STEP_2ARGS,
			     next, sv, l + 1);
	}
	else if ( op == PLUS || op == STAR) {
	    node = dumpuntil(start, NEXTOPER(node), NEXTOPER(node) + 1, sv, l + 1);
	}
	else if (op == ANYOF) {
	    node = NEXTOPER(node);
	    node += ANYOF_SKIP;
	}
	else if (PL_regkind[(U8)op] == EXACT) {
            /* Literal string, where present. */
	    node += NODE_SZ_STR(node) - 1;
	    node = NEXTOPER(node);
	}
	else {
	    node = NEXTOPER(node);
	    node += regarglen[(U8)op];
	}
	if (op == CURLYX || op == OPEN)
	    l++;
	else if (op == WHILEM)
	    l--;
    }
#endif	/* DEBUGGING */
    return node;
}

/*
 - regdump - dump a regexp onto Perl_debug_log in vaguely comprehensible form
 */
void
Perl_regdump(pTHX_ regexp *r)
{
#ifdef DEBUGGING
    SV *sv = sv_newmortal();

    (void)dumpuntil(r->program, r->program + 1, NULL, sv, 0);

    /* Header fields of interest. */
    if (r->anchored_substr)
	PerlIO_printf(Perl_debug_log,
		      "anchored `%s%.*s%s'%s at %"IVdf" ", 
		      PL_colors[0],
		      (int)(SvCUR(r->anchored_substr) - (SvTAIL(r->anchored_substr)!=0)),
		      SvPVX(r->anchored_substr), 
		      PL_colors[1],
		      SvTAIL(r->anchored_substr) ? "$" : "",
		      (IV)r->anchored_offset);
    if (r->float_substr)
	PerlIO_printf(Perl_debug_log,
		      "floating `%s%.*s%s'%s at %"IVdf"..%"UVuf" ", 
		      PL_colors[0],
		      (int)(SvCUR(r->float_substr) - (SvTAIL(r->float_substr)!=0)), 
		      SvPVX(r->float_substr),
		      PL_colors[1],
		      SvTAIL(r->float_substr) ? "$" : "",
		      (IV)r->float_min_offset, (UV)r->float_max_offset);
    if (r->check_substr)
	PerlIO_printf(Perl_debug_log, 
		      r->check_substr == r->float_substr 
		      ? "(checking floating" : "(checking anchored");
    if (r->reganch & ROPT_NOSCAN)
	PerlIO_printf(Perl_debug_log, " noscan");
    if (r->reganch & ROPT_CHECK_ALL)
	PerlIO_printf(Perl_debug_log, " isall");
    if (r->check_substr)
	PerlIO_printf(Perl_debug_log, ") ");

    if (r->regstclass) {
	regprop(sv, r->regstclass);
	PerlIO_printf(Perl_debug_log, "stclass `%s' ", SvPVX(sv));
    }
    if (r->reganch & ROPT_ANCH) {
	PerlIO_printf(Perl_debug_log, "anchored");
	if (r->reganch & ROPT_ANCH_BOL)
	    PerlIO_printf(Perl_debug_log, "(BOL)");
	if (r->reganch & ROPT_ANCH_MBOL)
	    PerlIO_printf(Perl_debug_log, "(MBOL)");
	if (r->reganch & ROPT_ANCH_SBOL)
	    PerlIO_printf(Perl_debug_log, "(SBOL)");
	if (r->reganch & ROPT_ANCH_GPOS)
	    PerlIO_printf(Perl_debug_log, "(GPOS)");
	PerlIO_putc(Perl_debug_log, ' ');
    }
    if (r->reganch & ROPT_GPOS_SEEN)
	PerlIO_printf(Perl_debug_log, "GPOS ");
    if (r->reganch & ROPT_SKIP)
	PerlIO_printf(Perl_debug_log, "plus ");
    if (r->reganch & ROPT_IMPLICIT)
	PerlIO_printf(Perl_debug_log, "implicit ");
    PerlIO_printf(Perl_debug_log, "minlen %ld ", (long) r->minlen);
    if (r->reganch & ROPT_EVAL_SEEN)
	PerlIO_printf(Perl_debug_log, "with eval ");
    PerlIO_printf(Perl_debug_log, "\n");
#endif	/* DEBUGGING */
}

STATIC void
S_put_byte(pTHX_ SV *sv, int c)
{
    if (isCNTRL(c) || c == 127 || c == 255)
	Perl_sv_catpvf(aTHX_ sv, "\\%o", c);
    else if (c == '-' || c == ']' || c == '\\' || c == '^')
	Perl_sv_catpvf(aTHX_ sv, "\\%c", c);
    else
	Perl_sv_catpvf(aTHX_ sv, "%c", c);
}

/*
- regprop - printable representation of opcode
*/
void
Perl_regprop(pTHX_ SV *sv, regnode *o)
{
#ifdef DEBUGGING
    register int k;

    sv_setpvn(sv, "", 0);
    if (OP(o) >= reg_num)		/* regnode.type is unsigned */
	FAIL("Corrupted regexp opcode");
    sv_catpv(sv, (char*)reg_name[OP(o)]); /* Take off const! */

    k = PL_regkind[(U8)OP(o)];

    if (k == EXACT)
	Perl_sv_catpvf(aTHX_ sv, " <%s%.*s%s>", PL_colors[0],
		       STR_LEN(o), STRING(o), PL_colors[1]);
    else if (k == CURLY) {
	if (OP(o) == CURLYM || OP(o) == CURLYN || OP(o) == CURLYX)
	    Perl_sv_catpvf(aTHX_ sv, "[%d]", o->flags); /* Parenth number */
	Perl_sv_catpvf(aTHX_ sv, " {%d,%d}", ARG1(o), ARG2(o));
    }
    else if (k == WHILEM && o->flags)			/* Ordinal/of */
	Perl_sv_catpvf(aTHX_ sv, "[%d/%d]", o->flags & 0xf, o->flags>>4);
    else if (k == REF || k == OPEN || k == CLOSE || k == GROUPP )
	Perl_sv_catpvf(aTHX_ sv, "%d", (int)ARG(o));	/* Parenth number */
    else if (k == LOGICAL)
	Perl_sv_catpvf(aTHX_ sv, "[%d]", o->flags);	/* 2: embedded, otherwise 1 */
    else if (k == ANYOF) {
	int i, rangestart = -1;
	bool anyofutf8 = OP(o) == ANYOFUTF8;
	U8 flags = anyofutf8 ? ARG1(o) : o->flags;
	const char * const anyofs[] = {	/* Should be syncronized with
					 * ANYOF_ #xdefines in regcomp.h */
	    "\\w",
	    "\\W",
	    "\\s",
	    "\\S",
	    "\\d",
	    "\\D",
	    "[:alnum:]",
	    "[:^alnum:]",
	    "[:alpha:]",
	    "[:^alpha:]",
	    "[:ascii:]",
	    "[:^ascii:]",
	    "[:ctrl:]",
	    "[:^ctrl:]",
	    "[:graph:]",
	    "[:^graph:]",
	    "[:lower:]",
	    "[:^lower:]",
	    "[:print:]",
	    "[:^print:]",
	    "[:punct:]",
	    "[:^punct:]",
	    "[:upper:]",
	    "[:^upper:]",
	    "[:xdigit:]",
	    "[:^xdigit:]",
	    "[:space:]",
	    "[:^space:]",
	    "[:blank:]",
	    "[:^blank:]"
	};

	if (flags & ANYOF_LOCALE)
	    sv_catpv(sv, "{loc}");
	if (flags & ANYOF_FOLD)
	    sv_catpv(sv, "{i}");
	Perl_sv_catpvf(aTHX_ sv, "[%s", PL_colors[0]);
	if (flags & ANYOF_INVERT)
	    sv_catpv(sv, "^");
	if (OP(o) == ANYOF) {
	    for (i = 0; i <= 256; i++) {
		if (i < 256 && ANYOF_BITMAP_TEST(o,i)) {
		    if (rangestart == -1)
			rangestart = i;
		} else if (rangestart != -1) {
		    if (i <= rangestart + 3)
			for (; rangestart < i; rangestart++)
			    put_byte(sv, rangestart);
		    else {
			put_byte(sv, rangestart);
			sv_catpv(sv, "-");
			put_byte(sv, i - 1);
		    }
		    rangestart = -1;
		}
	    }
	    if (o->flags & ANYOF_CLASS)
		for (i = 0; i < sizeof(anyofs)/sizeof(char*); i++)
		    if (ANYOF_CLASS_TEST(o,i))
			sv_catpv(sv, anyofs[i]);
	}
	else {
	    SV *rv = (SV*)PL_regdata->data[ARG2(o)];
	    AV *av = (AV*)SvRV((SV*)rv);
	    SV *sw = *av_fetch(av, 0, FALSE);
	    SV *lv = *av_fetch(av, 1, FALSE);
	    UV i;
	    U8 s[UTF8_MAXLEN+1];
	    for (i = 0; i <= 256; i++) { /* just the first 256 */
		U8 *e = uv_to_utf8(s, i);
		if (i < 256 && swash_fetch(sw, s)) {
		    if (rangestart == -1)
			rangestart = i;
		} else if (rangestart != -1) {
		    U8 *p;

		    if (i <= rangestart + 3)
			for (; rangestart < i; rangestart++) {
			    for(e = uv_to_utf8(s, rangestart), p = s; p < e; p++)
				put_byte(sv, *p);
			}
		    else {
			for (e = uv_to_utf8(s, rangestart), p = s; p < e; p++)
			    put_byte(sv, *p);
			sv_catpv(sv, "-");
			for (e = uv_to_utf8(s, i - 1), p = s; p < e; p++)
			    put_byte(sv, *p);
		    }
		    rangestart = -1;
		}
	    }
	    sv_catpv(sv, "...");
	    {
		char *s = savepv(SvPVX(lv));

		while(*s && *s != '\n') s++;
		if (*s == '\n') {
		    char *t = ++s;

		    while (*s) {
			if (*s == '\n')
			    *s = ' ';
			s++;
		    }
		    if (s[-1] == ' ')
			s[-1] = 0;

		    sv_catpv(sv, t);
		}
	    }
	}
	Perl_sv_catpvf(aTHX_ sv, "%s]", PL_colors[1]);
    }
    else if (k == BRANCHJ && (OP(o) == UNLESSM || OP(o) == IFMATCH))
	Perl_sv_catpvf(aTHX_ sv, "[-%d]", o->flags);
#endif	/* DEBUGGING */
}

SV *
Perl_re_intuit_string(pTHX_ regexp *prog)
{				/* Assume that RE_INTUIT is set */
    DEBUG_r(
	{   STRLEN n_a;
	    char *s = SvPV(prog->check_substr,n_a);

	    if (!PL_colorset) reginitcolors();
	    PerlIO_printf(Perl_debug_log,
		      "%sUsing REx substr:%s `%s%.60s%s%s'\n",
		      PL_colors[4],PL_colors[5],PL_colors[0],
		      s,
		      PL_colors[1],
		      (strlen(s) > 60 ? "..." : ""));
	} );

    return prog->check_substr;
}

void
Perl_pregfree(pTHX_ struct regexp *r)
{
    DEBUG_r(if (!PL_colorset) reginitcolors());

    if (!r || (--r->refcnt > 0))
	return;
    DEBUG_r(PerlIO_printf(Perl_debug_log,
		      "%sFreeing REx:%s `%s%.60s%s%s'\n",
		      PL_colors[4],PL_colors[5],PL_colors[0],
		      r->precomp,
		      PL_colors[1],
		      (strlen(r->precomp) > 60 ? "..." : "")));

    if (r->precomp)
	Safefree(r->precomp);
    if (RX_MATCH_COPIED(r))
	Safefree(r->subbeg);
    if (r->substrs) {
	if (r->anchored_substr)
	    SvREFCNT_dec(r->anchored_substr);
	if (r->float_substr)
	    SvREFCNT_dec(r->float_substr);
	Safefree(r->substrs);
    }
    if (r->data) {
	int n = r->data->count;
	AV* new_comppad = NULL;
	AV* old_comppad;
	SV** old_curpad;

	while (--n >= 0) {
	    switch (r->data->what[n]) {
	    case 's':
		SvREFCNT_dec((SV*)r->data->data[n]);
		break;
	    case 'f':
		Safefree(r->data->data[n]);
		break;
	    case 'p':
		new_comppad = (AV*)r->data->data[n];
		break;
	    case 'o':
		if (new_comppad == NULL)
		    Perl_croak(aTHX_ "panic: pregfree comppad");
		old_comppad = PL_comppad;
		old_curpad = PL_curpad;
		/* Watch out for global destruction's random ordering. */
		if (SvTYPE(new_comppad) == SVt_PVAV) {
		    PL_comppad = new_comppad;
		    PL_curpad = AvARRAY(new_comppad);
		}
		else
		    PL_curpad = NULL;
		op_free((OP_4tree*)r->data->data[n]);
		PL_comppad = old_comppad;
		PL_curpad = old_curpad;
		SvREFCNT_dec((SV*)new_comppad);
		new_comppad = NULL;
		break;
	    case 'n':
		break;
	    default:
		FAIL2("panic: regfree data code '%c'", r->data->what[n]);
	    }
	}
	Safefree(r->data->what);
	Safefree(r->data);
    }
    Safefree(r->startp);
    Safefree(r->endp);
    Safefree(r);
}

/*
 - regnext - dig the "next" pointer out of a node
 *
 * [Note, when REGALIGN is defined there are two places in regmatch()
 * that bypass this code for speed.]
 */
regnode *
Perl_regnext(pTHX_ register regnode *p)
{
    register I32 offset;

    if (p == &PL_regdummy)
	return(NULL);

    offset = (reg_off_by_arg[OP(p)] ? ARG(p) : NEXT_OFF(p));
    if (offset == 0)
	return(NULL);

    return(p+offset);
}

STATIC void	
S_re_croak2(pTHX_ const char* pat1,const char* pat2,...)
{
    va_list args;
    STRLEN l1 = strlen(pat1);
    STRLEN l2 = strlen(pat2);
    char buf[512];
    SV *msv;
    char *message;

    if (l1 > 510)
	l1 = 510;
    if (l1 + l2 > 510)
	l2 = 510 - l1;
    Copy(pat1, buf, l1 , char);
    Copy(pat2, buf + l1, l2 , char);
    buf[l1 + l2] = '\n';
    buf[l1 + l2 + 1] = '\0';
#ifdef I_STDARG
    /* ANSI variant takes additional second argument */
    va_start(args, pat2);
#else
    va_start(args);
#endif
    msv = vmess(buf, &args);
    va_end(args);
    message = SvPV(msv,l1);
    if (l1 > 512)
	l1 = 512;
    Copy(message, buf, l1 , char);
    buf[l1] = '\0';			/* Overwrite \n */
    Perl_croak(aTHX_ "%s", buf);
}

/* XXX Here's a total kludge.  But we need to re-enter for swash routines. */

void
Perl_save_re_context(pTHX)
{                   
    SAVEPPTR(PL_bostr);
    SAVEPPTR(PL_regprecomp);		/* uncompiled string. */
    SAVEI32(PL_regnpar);		/* () count. */
    SAVEI32(PL_regsize);		/* Code size. */
    SAVEI16(PL_regflags);		/* are we folding, multilining? */
    SAVEPPTR(PL_reginput);		/* String-input pointer. */
    SAVEPPTR(PL_regbol);		/* Beginning of input, for ^ check. */
    SAVEPPTR(PL_regeol);		/* End of input, for $ check. */
    SAVEVPTR(PL_regstartp);		/* Pointer to startp array. */
    SAVEVPTR(PL_regendp);		/* Ditto for endp. */
    SAVEVPTR(PL_reglastparen);		/* Similarly for lastparen. */
    SAVEPPTR(PL_regtill);		/* How far we are required to go. */
    SAVEI8(PL_regprev);			/* char before regbol, \n if none */
    SAVEGENERICPV(PL_reg_start_tmp);	/* from regexec.c */
    PL_reg_start_tmp = 0;
    SAVEI32(PL_reg_start_tmpl);		/* from regexec.c */
    PL_reg_start_tmpl = 0;
    SAVEVPTR(PL_regdata);
    SAVEI32(PL_reg_flags);		/* from regexec.c */
    SAVEI32(PL_reg_eval_set);		/* from regexec.c */
    SAVEI32(PL_regnarrate);		/* from regexec.c */
    SAVEVPTR(PL_regprogram);		/* from regexec.c */
    SAVEINT(PL_regindent);		/* from regexec.c */
    SAVEVPTR(PL_regcc);			/* from regexec.c */
    SAVEVPTR(PL_curcop);
    SAVEVPTR(PL_regcomp_rx);		/* from regcomp.c */
    SAVEI32(PL_regseen);		/* from regcomp.c */
    SAVEI32(PL_regsawback);		/* Did we see \1, ...? */
    SAVEI32(PL_regnaughty);		/* How bad is this pattern? */
    SAVEVPTR(PL_regcode);		/* Code-emit pointer; &regdummy = don't */
    SAVEPPTR(PL_regxend);		/* End of input for compile */
    SAVEPPTR(PL_regcomp_parse);		/* Input-scan pointer. */
    SAVEVPTR(PL_reg_call_cc);		/* from regexec.c */
    SAVEVPTR(PL_reg_re);		/* from regexec.c */
    SAVEPPTR(PL_reg_ganch);		/* from regexec.c */
    SAVESPTR(PL_reg_sv);		/* from regexec.c */
    SAVEVPTR(PL_reg_magic);		/* from regexec.c */
    SAVEI32(PL_reg_oldpos);			/* from regexec.c */
    SAVEVPTR(PL_reg_oldcurpm);		/* from regexec.c */
    SAVEVPTR(PL_reg_curpm);		/* from regexec.c */
    SAVEI32(PL_regnpar);		/* () count. */
#ifdef DEBUGGING
    SAVEPPTR(PL_reg_starttry);		/* from regexec.c */    
#endif
}

#ifdef PERL_OBJECT
#include "XSUB.h"
#undef this
#define this pPerl
#endif

static void
clear_re(pTHXo_ void *r)
{
    ReREFCNT_dec((regexp *)r);
}

