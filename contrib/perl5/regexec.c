/*    regexec.c
 */

/*
 * "One Ring to rule them all, One Ring to find them..."
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
#  define Perl_regexec_flags my_regexec
#  define Perl_regdump my_regdump
#  define Perl_regprop my_regprop
#  define Perl_re_intuit_start my_re_intuit_start
/* *These* symbols are masked to allow static link. */
#  define Perl_pregexec my_pregexec
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
#define PERL_IN_REGEXEC_C
#include "perl.h"

#ifdef PERL_IN_XSUB_RE
#  if defined(PERL_CAPI) || defined(PERL_OBJECT)
#    include "XSUB.h"
#  endif
#endif

#include "regcomp.h"

#define RF_tainted	1		/* tainted information used? */
#define RF_warned	2		/* warned about big count? */
#define RF_evaled	4		/* Did an EVAL with setting? */
#define RF_utf8		8		/* String contains multibyte chars? */

#define UTF (PL_reg_flags & RF_utf8)

#define RS_init		1		/* eval environment created */
#define RS_set		2		/* replsv value is set */

#ifndef STATIC
#define	STATIC	static
#endif

/*
 * Forwards.
 */

#define REGINCLASS(p,c)  (ANYOF_FLAGS(p) ? reginclass(p,c) : ANYOF_BITMAP_TEST(p,c))
#ifdef DEBUGGING
#   define REGINCLASSUTF8(f,p)  (ARG1(f) ? reginclassutf8(f,p) : swash_fetch(*av_fetch((AV*)SvRV((SV*)PL_regdata->data[ARG2(f)]),0,FALSE),p))
#else
#   define REGINCLASSUTF8(f,p)  (ARG1(f) ? reginclassutf8(f,p) : swash_fetch((SV*)PL_regdata->data[ARG2(f)],p))
#endif

#define CHR_SVLEN(sv) (UTF ? sv_len_utf8(sv) : SvCUR(sv))
#define CHR_DIST(a,b) (UTF ? utf8_distance(a,b) : a - b)

#define reghop_c(pos,off) ((char*)reghop((U8*)pos, off))
#define reghopmaybe_c(pos,off) ((char*)reghopmaybe((U8*)pos, off))
#define HOP(pos,off) (UTF ? reghop((U8*)pos, off) : (U8*)(pos + off))
#define HOPMAYBE(pos,off) (UTF ? reghopmaybe((U8*)pos, off) : (U8*)(pos + off))
#define HOPc(pos,off) ((char*)HOP(pos,off))
#define HOPMAYBEc(pos,off) ((char*)HOPMAYBE(pos,off))

static void restore_pos(pTHXo_ void *arg);


STATIC CHECKPOINT
S_regcppush(pTHX_ I32 parenfloor)
{
    int retval = PL_savestack_ix;
#define REGCP_PAREN_ELEMS 4
    int paren_elems_to_push = (PL_regsize - parenfloor) * REGCP_PAREN_ELEMS;
    int p;

#define REGCP_OTHER_ELEMS 5
    SSCHECK(paren_elems_to_push + REGCP_OTHER_ELEMS);
    for (p = PL_regsize; p > parenfloor; p--) {
/* REGCP_PARENS_ELEMS are pushed per pairs of parentheses. */
	SSPUSHINT(PL_regendp[p]);
	SSPUSHINT(PL_regstartp[p]);
	SSPUSHPTR(PL_reg_start_tmp[p]);
	SSPUSHINT(p);
    }
/* REGCP_OTHER_ELEMS are pushed in any case, parentheses or no. */
    SSPUSHINT(PL_regsize);
    SSPUSHINT(*PL_reglastparen);
    SSPUSHPTR(PL_reginput);
#define REGCP_FRAME_ELEMS 2
/* REGCP_FRAME_ELEMS are part of the REGCP_OTHER_ELEMS and
 * are needed for the regexp context stack bookkeeping. */
    SSPUSHINT(paren_elems_to_push + REGCP_OTHER_ELEMS - REGCP_FRAME_ELEMS);
    SSPUSHINT(SAVEt_REGCONTEXT); /* Magic cookie. */

    return retval;
}

/* These are needed since we do not localize EVAL nodes: */
#  define REGCP_SET(cp)  DEBUG_r(PerlIO_printf(Perl_debug_log,		\
			     "  Setting an EVAL scope, savestack=%"IVdf"\n",	\
			     (IV)PL_savestack_ix)); cp = PL_savestack_ix

#  define REGCP_UNWIND(cp)  DEBUG_r(cp != PL_savestack_ix ?		\
				PerlIO_printf(Perl_debug_log,		\
				"  Clearing an EVAL scope, savestack=%"IVdf"..%"IVdf"\n", \
				(IV)(cp), (IV)PL_savestack_ix) : 0); regcpblow(cp)

STATIC char *
S_regcppop(pTHX)
{
    I32 i;
    U32 paren = 0;
    char *input;
    I32 tmps;

    /* Pop REGCP_OTHER_ELEMS before the parentheses loop starts. */
    i = SSPOPINT;
    assert(i == SAVEt_REGCONTEXT); /* Check that the magic cookie is there. */
    i = SSPOPINT; /* Parentheses elements to pop. */
    input = (char *) SSPOPPTR;
    *PL_reglastparen = SSPOPINT;
    PL_regsize = SSPOPINT;

    /* Now restore the parentheses context. */
    for (i -= (REGCP_OTHER_ELEMS - REGCP_FRAME_ELEMS);
	 i > 0; i -= REGCP_PAREN_ELEMS) {
	paren = (U32)SSPOPINT;
	PL_reg_start_tmp[paren] = (char *) SSPOPPTR;
	PL_regstartp[paren] = SSPOPINT;
	tmps = SSPOPINT;
	if (paren <= *PL_reglastparen)
	    PL_regendp[paren] = tmps;
	DEBUG_r(
	    PerlIO_printf(Perl_debug_log,
			  "     restoring \\%"UVuf" to %"IVdf"(%"IVdf")..%"IVdf"%s\n",
			  (UV)paren, (IV)PL_regstartp[paren], 
			  (IV)(PL_reg_start_tmp[paren] - PL_bostr),
			  (IV)PL_regendp[paren], 
			  (paren > *PL_reglastparen ? "(no)" : ""));
	);
    }
    DEBUG_r(
	if (*PL_reglastparen + 1 <= PL_regnpar) {
	    PerlIO_printf(Perl_debug_log,
			  "     restoring \\%"IVdf"..\\%"IVdf" to undef\n",
			  (IV)(*PL_reglastparen + 1), (IV)PL_regnpar);
	}
    );
#if 1
    /* It would seem that the similar code in regtry()
     * already takes care of this, and in fact it is in
     * a better location to since this code can #if 0-ed out
     * but the code in regtry() is needed or otherwise tests
     * requiring null fields (pat.t#187 and split.t#{13,14}
     * (as of patchlevel 7877)  will fail.  Then again,
     * this code seems to be necessary or otherwise
     * building DynaLoader will fail:
     * "Error: '*' not in typemap in DynaLoader.xs, line 164"
     * --jhi */
    for (paren = *PL_reglastparen + 1; paren <= PL_regnpar; paren++) {
	if (paren > PL_regsize)
	    PL_regstartp[paren] = -1;
	PL_regendp[paren] = -1;
    }
#endif
    return input;
}

STATIC char *
S_regcp_set_to(pTHX_ I32 ss)
{
    I32 tmp = PL_savestack_ix;

    PL_savestack_ix = ss;
    regcppop();
    PL_savestack_ix = tmp;
    return Nullch;
}

typedef struct re_cc_state
{
    I32 ss;
    regnode *node;
    struct re_cc_state *prev;
    CURCUR *cc;
    regexp *re;
} re_cc_state;

#define regcpblow(cp) LEAVE_SCOPE(cp)	/* Ignores regcppush()ed data. */

#define TRYPAREN(paren, n, input) {				\
    if (paren) {						\
	if (n) {						\
	    PL_regstartp[paren] = HOPc(input, -1) - PL_bostr;	\
	    PL_regendp[paren] = input - PL_bostr;		\
	}							\
	else							\
	    PL_regendp[paren] = -1;				\
    }								\
    if (regmatch(next))						\
	sayYES;							\
    if (paren && n)						\
	PL_regendp[paren] = -1;					\
}


/*
 * pregexec and friends
 */

/*
 - pregexec - match a regexp against a string
 */
I32
Perl_pregexec(pTHX_ register regexp *prog, char *stringarg, register char *strend,
	 char *strbeg, I32 minend, SV *screamer, U32 nosave)
/* strend: pointer to null at end of string */
/* strbeg: real beginning of string */
/* minend: end of match must be >=minend after stringarg. */
/* nosave: For optimizations. */
{
    return
	regexec_flags(prog, stringarg, strend, strbeg, minend, screamer, NULL, 
		      nosave ? 0 : REXEC_COPY_STR);
}

STATIC void
S_cache_re(pTHX_ regexp *prog)
{
    PL_regprecomp = prog->precomp;		/* Needed for FAIL. */
#ifdef DEBUGGING
    PL_regprogram = prog->program;
#endif
    PL_regnpar = prog->nparens;
    PL_regdata = prog->data;    
    PL_reg_re = prog;    
}

/* 
 * Need to implement the following flags for reg_anch:
 *
 * USE_INTUIT_NOML		- Useful to call re_intuit_start() first
 * USE_INTUIT_ML
 * INTUIT_AUTORITATIVE_NOML	- Can trust a positive answer
 * INTUIT_AUTORITATIVE_ML
 * INTUIT_ONCE_NOML		- Intuit can match in one location only.
 * INTUIT_ONCE_ML
 *
 * Another flag for this function: SECOND_TIME (so that float substrs
 * with giant delta may be not rechecked).
 */

/* Assumptions: if ANCH_GPOS, then strpos is anchored. XXXX Check GPOS logic */

/* If SCREAM, then SvPVX(sv) should be compatible with strpos and strend.
   Otherwise, only SvCUR(sv) is used to get strbeg. */

/* XXXX We assume that strpos is strbeg unless sv. */

/* XXXX Some places assume that there is a fixed substring.
	An update may be needed if optimizer marks as "INTUITable"
	RExen without fixed substrings.  Similarly, it is assumed that
	lengths of all the strings are no more than minlen, thus they
	cannot come from lookahead.
	(Or minlen should take into account lookahead.) */

/* A failure to find a constant substring means that there is no need to make
   an expensive call to REx engine, thus we celebrate a failure.  Similarly,
   finding a substring too deep into the string means that less calls to
   regtry() should be needed.

   REx compiler's optimizer found 4 possible hints:
	a) Anchored substring;
	b) Fixed substring;
	c) Whether we are anchored (beginning-of-line or \G);
	d) First node (of those at offset 0) which may distingush positions;
   We use a)b)d) and multiline-part of c), and try to find a position in the
   string which does not contradict any of them.
 */

/* Most of decisions we do here should have been done at compile time.
   The nodes of the REx which we used for the search should have been
   deleted from the finite automaton. */

char *
Perl_re_intuit_start(pTHX_ regexp *prog, SV *sv, char *strpos,
		     char *strend, U32 flags, re_scream_pos_data *data)
{
    register I32 start_shift;
    /* Should be nonnegative! */
    register I32 end_shift;
    register char *s;
    register SV *check;
    char *strbeg;
    char *t;
    I32 ml_anch;
    char *tmp;
    register char *other_last = Nullch;	/* other substr checked before this */
    char *check_at;			/* check substr found at this pos */
#ifdef DEBUGGING
    char *i_strpos = strpos;
#endif

    DEBUG_r( if (!PL_colorset) reginitcolors() );
    DEBUG_r(PerlIO_printf(Perl_debug_log,
		      "%sGuessing start of match, REx%s `%s%.60s%s%s' against `%s%.*s%s%s'...\n",
		      PL_colors[4],PL_colors[5],PL_colors[0],
		      prog->precomp,
		      PL_colors[1],
		      (strlen(prog->precomp) > 60 ? "..." : ""),
		      PL_colors[0],
		      (int)(strend - strpos > 60 ? 60 : strend - strpos),
		      strpos, PL_colors[1],
		      (strend - strpos > 60 ? "..." : ""))
	);

    if (prog->minlen > strend - strpos) {
	DEBUG_r(PerlIO_printf(Perl_debug_log, "String too short...\n"));
	goto fail;
    }
    strbeg = (sv && SvPOK(sv)) ? strend - SvCUR(sv) : strpos;
    check = prog->check_substr;
    if (prog->reganch & ROPT_ANCH) {	/* Match at beg-of-str or after \n */
	ml_anch = !( (prog->reganch & ROPT_ANCH_SINGLE)
		     || ( (prog->reganch & ROPT_ANCH_BOL)
			  && !PL_multiline ) );	/* Check after \n? */

	if (!ml_anch) {
	  if ( !(prog->reganch & ROPT_ANCH_GPOS) /* Checked by the caller */
	       /* SvCUR is not set on references: SvRV and SvPVX overlap */
	       && sv && !SvROK(sv)
	       && (strpos != strbeg)) {
	      DEBUG_r(PerlIO_printf(Perl_debug_log, "Not at start...\n"));
	      goto fail;
	  }
	  if (prog->check_offset_min == prog->check_offset_max) {
	    /* Substring at constant offset from beg-of-str... */
	    I32 slen;

	    PL_regeol = strend;			/* Used in HOP() */
	    s = HOPc(strpos, prog->check_offset_min);
	    if (SvTAIL(check)) {
		slen = SvCUR(check);	/* >= 1 */

		if ( strend - s > slen || strend - s < slen - 1 
		     || (strend - s == slen && strend[-1] != '\n')) {
		    DEBUG_r(PerlIO_printf(Perl_debug_log, "String too long...\n"));
		    goto fail_finish;
		}
		/* Now should match s[0..slen-2] */
		slen--;
		if (slen && (*SvPVX(check) != *s
			     || (slen > 1
				 && memNE(SvPVX(check), s, slen)))) {
		  report_neq:
		    DEBUG_r(PerlIO_printf(Perl_debug_log, "String not equal...\n"));
		    goto fail_finish;
		}
	    }
	    else if (*SvPVX(check) != *s
		     || ((slen = SvCUR(check)) > 1
			 && memNE(SvPVX(check), s, slen)))
		goto report_neq;
	    goto success_at_start;
	  }
	}
	/* Match is anchored, but substr is not anchored wrt beg-of-str. */
	s = strpos;
	start_shift = prog->check_offset_min; /* okay to underestimate on CC */
	end_shift = prog->minlen - start_shift -
	    CHR_SVLEN(check) + (SvTAIL(check) != 0);
	if (!ml_anch) {
	    I32 end = prog->check_offset_max + CHR_SVLEN(check)
					 - (SvTAIL(check) != 0);
	    I32 eshift = strend - s - end;

	    if (end_shift < eshift)
		end_shift = eshift;
	}
    }
    else {				/* Can match at random position */
	ml_anch = 0;
	s = strpos;
	start_shift = prog->check_offset_min; /* okay to underestimate on CC */
	/* Should be nonnegative! */
	end_shift = prog->minlen - start_shift -
	    CHR_SVLEN(check) + (SvTAIL(check) != 0);
    }

#ifdef DEBUGGING	/* 7/99: reports of failure (with the older version) */
    if (end_shift < 0)
	Perl_croak(aTHX_ "panic: end_shift");
#endif

  restart:
    other_last = Nullch;

    /* Find a possible match in the region s..strend by looking for
       the "check" substring in the region corrected by start/end_shift. */
    if (flags & REXEC_SCREAM) {
	I32 p = -1;			/* Internal iterator of scream. */
	I32 *pp = data ? data->scream_pos : &p;

	if (PL_screamfirst[BmRARE(check)] >= 0
	    || ( BmRARE(check) == '\n'
		 && (BmPREVIOUS(check) == SvCUR(check) - 1)
		 && SvTAIL(check) ))
	    s = screaminstr(sv, check, 
			    start_shift + (s - strbeg), end_shift, pp, 0);
	else
	    goto fail_finish;
	if (data)
	    *data->scream_olds = s;
    }
    else
	s = fbm_instr((unsigned char*)s + start_shift,
		      (unsigned char*)strend - end_shift,
		      check, PL_multiline ? FBMrf_MULTILINE : 0);

    /* Update the count-of-usability, remove useless subpatterns,
	unshift s.  */

    DEBUG_r(PerlIO_printf(Perl_debug_log, "%s %s substr `%s%.*s%s'%s%s",
			  (s ? "Found" : "Did not find"),
			  ((check == prog->anchored_substr) ? "anchored" : "floating"),
			  PL_colors[0],
			  (int)(SvCUR(check) - (SvTAIL(check)!=0)),
			  SvPVX(check),
			  PL_colors[1], (SvTAIL(check) ? "$" : ""),
			  (s ? " at offset " : "...\n") ) );

    if (!s)
	goto fail_finish;

    check_at = s;

    /* Finish the diagnostic message */
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%ld...\n", (long)(s - i_strpos)) );

    /* Got a candidate.  Check MBOL anchoring, and the *other* substr.
       Start with the other substr.
       XXXX no SCREAM optimization yet - and a very coarse implementation
       XXXX /ttx+/ results in anchored=`ttx', floating=`x'.  floating will
		*always* match.  Probably should be marked during compile...
       Probably it is right to do no SCREAM here...
     */

    if (prog->float_substr && prog->anchored_substr) {
	/* Take into account the "other" substring. */
	/* XXXX May be hopelessly wrong for UTF... */
	if (!other_last)
	    other_last = strpos;
	if (check == prog->float_substr) {
	  do_other_anchored:
	    {
		char *last = s - start_shift, *last1, *last2;
		char *s1 = s;

		tmp = PL_bostr;
		t = s - prog->check_offset_max;
		if (s - strpos > prog->check_offset_max  /* signed-corrected t > strpos */
		    && (!(prog->reganch & ROPT_UTF8)
			|| (PL_bostr = strpos, /* Used in regcopmaybe() */
			    (t = reghopmaybe_c(s, -(prog->check_offset_max)))
			    && t > strpos)))
		    /* EMPTY */;
		else
		    t = strpos;
		t += prog->anchored_offset;
		if (t < other_last)	/* These positions already checked */
		    t = other_last;
		PL_bostr = tmp;
		last2 = last1 = strend - prog->minlen;
		if (last < last1)
		    last1 = last;
 /* XXXX It is not documented what units *_offsets are in.  Assume bytes.  */
		/* On end-of-str: see comment below. */
		s = fbm_instr((unsigned char*)t,
			      (unsigned char*)last1 + prog->anchored_offset
				 + SvCUR(prog->anchored_substr)
				 - (SvTAIL(prog->anchored_substr)!=0),
			      prog->anchored_substr, PL_multiline ? FBMrf_MULTILINE : 0);
		DEBUG_r(PerlIO_printf(Perl_debug_log, "%s anchored substr `%s%.*s%s'%s",
			(s ? "Found" : "Contradicts"),
			PL_colors[0],
			  (int)(SvCUR(prog->anchored_substr)
			  - (SvTAIL(prog->anchored_substr)!=0)),
			  SvPVX(prog->anchored_substr),
			  PL_colors[1], (SvTAIL(prog->anchored_substr) ? "$" : "")));
		if (!s) {
		    if (last1 >= last2) {
			DEBUG_r(PerlIO_printf(Perl_debug_log,
						", giving up...\n"));
			goto fail_finish;
		    }
		    DEBUG_r(PerlIO_printf(Perl_debug_log,
			", trying floating at offset %ld...\n",
			(long)(s1 + 1 - i_strpos)));
		    PL_regeol = strend;			/* Used in HOP() */
		    other_last = last1 + prog->anchored_offset + 1;
		    s = HOPc(last, 1);
		    goto restart;
		}
		else {
		    DEBUG_r(PerlIO_printf(Perl_debug_log, " at offset %ld...\n",
			  (long)(s - i_strpos)));
		    t = s - prog->anchored_offset;
		    other_last = s + 1;
		    s = s1;
		    if (t == strpos)
			goto try_at_start;
		    goto try_at_offset;
		}
	    }
	}
	else {		/* Take into account the floating substring. */
		char *last, *last1;
		char *s1 = s;

		t = s - start_shift;
		last1 = last = strend - prog->minlen + prog->float_min_offset;
		if (last - t > prog->float_max_offset)
		    last = t + prog->float_max_offset;
		s = t + prog->float_min_offset;
		if (s < other_last)
		    s = other_last;
 /* XXXX It is not documented what units *_offsets are in.  Assume bytes.  */
		/* fbm_instr() takes into account exact value of end-of-str
		   if the check is SvTAIL(ed).  Since false positives are OK,
		   and end-of-str is not later than strend we are OK. */
		s = fbm_instr((unsigned char*)s,
			      (unsigned char*)last + SvCUR(prog->float_substr)
				  - (SvTAIL(prog->float_substr)!=0),
			      prog->float_substr, PL_multiline ? FBMrf_MULTILINE : 0);
		DEBUG_r(PerlIO_printf(Perl_debug_log, "%s floating substr `%s%.*s%s'%s",
			(s ? "Found" : "Contradicts"),
			PL_colors[0],
			  (int)(SvCUR(prog->float_substr)
			  - (SvTAIL(prog->float_substr)!=0)),
			  SvPVX(prog->float_substr),
			  PL_colors[1], (SvTAIL(prog->float_substr) ? "$" : "")));
		if (!s) {
		    if (last1 == last) {
			DEBUG_r(PerlIO_printf(Perl_debug_log,
						", giving up...\n"));
			goto fail_finish;
		    }
		    DEBUG_r(PerlIO_printf(Perl_debug_log,
			", trying anchored starting at offset %ld...\n",
			(long)(s1 + 1 - i_strpos)));
		    other_last = last;
		    PL_regeol = strend;			/* Used in HOP() */
		    s = HOPc(t, 1);
		    goto restart;
		}
		else {
		    DEBUG_r(PerlIO_printf(Perl_debug_log, " at offset %ld...\n",
			  (long)(s - i_strpos)));
		    other_last = s; /* Fix this later. --Hugo */
		    s = s1;
		    if (t == strpos)
			goto try_at_start;
		    goto try_at_offset;
		}
	}
    }

    t = s - prog->check_offset_max;
    tmp = PL_bostr;
    if (s - strpos > prog->check_offset_max  /* signed-corrected t > strpos */
        && (!(prog->reganch & ROPT_UTF8)
	    || (PL_bostr = strpos, /* Used in regcopmaybe() */
		((t = reghopmaybe_c(s, -(prog->check_offset_max)))
		 && t > strpos)))) {
	PL_bostr = tmp;
	/* Fixed substring is found far enough so that the match
	   cannot start at strpos. */
      try_at_offset:
	if (ml_anch && t[-1] != '\n') {
	    /* Eventually fbm_*() should handle this, but often
	       anchored_offset is not 0, so this check will not be wasted. */
	    /* XXXX In the code below we prefer to look for "^" even in
	       presence of anchored substrings.  And we search even
	       beyond the found float position.  These pessimizations
	       are historical artefacts only.  */
	  find_anchor:
	    while (t < strend - prog->minlen) {
		if (*t == '\n') {
		    if (t < check_at - prog->check_offset_min) {
			if (prog->anchored_substr) {
			    /* Since we moved from the found position,
			       we definitely contradict the found anchored
			       substr.  Due to the above check we do not
			       contradict "check" substr.
			       Thus we can arrive here only if check substr
			       is float.  Redo checking for "other"=="fixed".
			     */
			    strpos = t + 1;			    
			    DEBUG_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m at offset %ld, rescanning for anchored from offset %ld...\n",
				PL_colors[0],PL_colors[1], (long)(strpos - i_strpos), (long)(strpos - i_strpos + prog->anchored_offset)));
			    goto do_other_anchored;
			}
			/* We don't contradict the found floating substring. */
			/* XXXX Why not check for STCLASS? */
			s = t + 1;
			DEBUG_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m at offset %ld...\n",
			    PL_colors[0],PL_colors[1], (long)(s - i_strpos)));
			goto set_useful;
		    }
		    /* Position contradicts check-string */
		    /* XXXX probably better to look for check-string
		       than for "\n", so one should lower the limit for t? */
		    DEBUG_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m, restarting lookup for check-string at offset %ld...\n",
			PL_colors[0],PL_colors[1], (long)(t + 1 - i_strpos)));
		    other_last = strpos = s = t + 1;
		    goto restart;
		}
		t++;
	    }
	    DEBUG_r(PerlIO_printf(Perl_debug_log, "Did not find /%s^%s/m...\n",
			PL_colors[0],PL_colors[1]));
	    goto fail_finish;
	}
	else {
	    DEBUG_r(PerlIO_printf(Perl_debug_log, "Starting position does not contradict /%s^%s/m...\n",
			PL_colors[0],PL_colors[1]));
	}
	s = t;
      set_useful:
	++BmUSEFUL(prog->check_substr);	/* hooray/5 */
    }
    else {
	PL_bostr = tmp;
	/* The found string does not prohibit matching at strpos,
	   - no optimization of calling REx engine can be performed,
	   unless it was an MBOL and we are not after MBOL,
	   or a future STCLASS check will fail this. */
      try_at_start:
	/* Even in this situation we may use MBOL flag if strpos is offset
	   wrt the start of the string. */
	if (ml_anch && sv && !SvROK(sv)	/* See prev comment on SvROK */
	    && (strpos != strbeg) && strpos[-1] != '\n'
	    /* May be due to an implicit anchor of m{.*foo}  */
	    && !(prog->reganch & ROPT_IMPLICIT))
	{
	    t = strpos;
	    goto find_anchor;
	}
	DEBUG_r( if (ml_anch)
	    PerlIO_printf(Perl_debug_log, "Position at offset %ld does not contradict /%s^%s/m...\n",
			(long)(strpos - i_strpos), PL_colors[0],PL_colors[1]);
	);
      success_at_start:
	if (!(prog->reganch & ROPT_NAUGHTY)	/* XXXX If strpos moved? */
	    && prog->check_substr		/* Could be deleted already */
	    && --BmUSEFUL(prog->check_substr) < 0
	    && prog->check_substr == prog->float_substr)
	{
	    /* If flags & SOMETHING - do not do it many times on the same match */
	    DEBUG_r(PerlIO_printf(Perl_debug_log, "... Disabling check substring...\n"));
	    SvREFCNT_dec(prog->check_substr);
	    prog->check_substr = Nullsv;	/* disable */
	    prog->float_substr = Nullsv;	/* clear */
	    check = Nullsv;			/* abort */
	    s = strpos;
	    /* XXXX This is a remnant of the old implementation.  It
	            looks wasteful, since now INTUIT can use many
	            other heuristics. */
	    prog->reganch &= ~RE_USE_INTUIT;
	}
	else
	    s = strpos;
    }

    /* Last resort... */
    /* XXXX BmUSEFUL already changed, maybe multiple change is meaningful... */
    if (prog->regstclass) {
	/* minlen == 0 is possible if regstclass is \b or \B,
	   and the fixed substr is ''$.
	   Since minlen is already taken into account, s+1 is before strend;
	   accidentally, minlen >= 1 guaranties no false positives at s + 1
	   even for \b or \B.  But (minlen? 1 : 0) below assumes that
	   regstclass does not come from lookahead...  */
	/* If regstclass takes bytelength more than 1: If charlength==1, OK.
	   This leaves EXACTF only, which is dealt with in find_byclass().  */
	int cl_l = (PL_regkind[(U8)OP(prog->regstclass)] == EXACT
		    ? STR_LEN(prog->regstclass)
		    : 1);
	char *endpos = (prog->anchored_substr || ml_anch)
		? s + (prog->minlen? cl_l : 0)
		: (prog->float_substr ? check_at - start_shift + cl_l
				      : strend) ;
	char *startpos = strbeg;

	t = s;
	if (prog->reganch & ROPT_UTF8) {	
	    PL_regdata = prog->data;	/* Used by REGINCLASS UTF logic */
	    PL_bostr = startpos;
	}
        s = find_byclass(prog, prog->regstclass, s, endpos, startpos, 1);
	if (!s) {
#ifdef DEBUGGING
	    char *what;
#endif
	    if (endpos == strend) {
		DEBUG_r( PerlIO_printf(Perl_debug_log,
				"Could not match STCLASS...\n") );
		goto fail;
	    }
	    DEBUG_r( PerlIO_printf(Perl_debug_log,
				   "This position contradicts STCLASS...\n") );
	    if ((prog->reganch & ROPT_ANCH) && !ml_anch)
		goto fail;
	    /* Contradict one of substrings */
	    if (prog->anchored_substr) {
		if (prog->anchored_substr == check) {
		    DEBUG_r( what = "anchored" );
		  hop_and_restart:
		    PL_regeol = strend;	/* Used in HOP() */
		    s = HOPc(t, 1);
		    if (s + start_shift + end_shift > strend) {
			/* XXXX Should be taken into account earlier? */
			DEBUG_r( PerlIO_printf(Perl_debug_log,
					       "Could not match STCLASS...\n") );
			goto fail;
		    }
		    if (!check)
			goto giveup;
		    DEBUG_r( PerlIO_printf(Perl_debug_log,
				"Looking for %s substr starting at offset %ld...\n",
				 what, (long)(s + start_shift - i_strpos)) );
		    goto restart;
		}
		/* Have both, check_string is floating */
		if (t + start_shift >= check_at) /* Contradicts floating=check */
		    goto retry_floating_check;
		/* Recheck anchored substring, but not floating... */
		s = check_at; 
		if (!check)
		    goto giveup;
		DEBUG_r( PerlIO_printf(Perl_debug_log,
			  "Looking for anchored substr starting at offset %ld...\n",
			  (long)(other_last - i_strpos)) );
		goto do_other_anchored;
	    }
	    /* Another way we could have checked stclass at the
               current position only: */
	    if (ml_anch) {
		s = t = t + 1;
		if (!check)
		    goto giveup;
		DEBUG_r( PerlIO_printf(Perl_debug_log,
			  "Looking for /%s^%s/m starting at offset %ld...\n",
			  PL_colors[0],PL_colors[1], (long)(t - i_strpos)) );
		goto try_at_offset;
	    }
	    if (!prog->float_substr)	/* Could have been deleted */
		goto fail;
	    /* Check is floating subtring. */
	  retry_floating_check:
	    t = check_at - start_shift;
	    DEBUG_r( what = "floating" );
	    goto hop_and_restart;
	}
	DEBUG_r( if (t != s)
		     PerlIO_printf(Perl_debug_log, 
			"By STCLASS: moving %ld --> %ld\n",
			(long)(t - i_strpos), (long)(s - i_strpos));
		 else
		     PerlIO_printf(Perl_debug_log, 
			"Does not contradict STCLASS...\n") );
    }
  giveup:
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%s%s:%s match at offset %ld\n",
			  PL_colors[4], (check ? "Guessed" : "Giving up"),
			  PL_colors[5], (long)(s - i_strpos)) );
    return s;

  fail_finish:				/* Substring not found */
    if (prog->check_substr)		/* could be removed already */
	BmUSEFUL(prog->check_substr) += 5; /* hooray */
  fail:
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%sMatch rejected by optimizer%s\n",
			  PL_colors[4],PL_colors[5]));
    return Nullch;
}

/* We know what class REx starts with.  Try to find this position... */
STATIC char *
S_find_byclass(pTHX_ regexp * prog, regnode *c, char *s, char *strend, char *startpos, I32 norun)
{
	I32 doevery = (prog->reganch & ROPT_SKIP) == 0;
	char *m;
	STRLEN ln;
	unsigned int c1;
	unsigned int c2;
	char *e;
	register I32 tmp = 1;	/* Scratch variable? */

	/* We know what class it must start with. */
	switch (OP(c)) {
	case ANYOFUTF8:
	    while (s < strend) {
		if (REGINCLASSUTF8(c, (U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case ANYOF:
	    while (s < strend) {
		if (REGINCLASS(c, *(U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case EXACTF:
	    m = STRING(c);
	    ln = STR_LEN(c);
	    c1 = *(U8*)m;
	    c2 = PL_fold[c1];
	    goto do_exactf;
	case EXACTFL:
	    m = STRING(c);
	    ln = STR_LEN(c);
	    c1 = *(U8*)m;
	    c2 = PL_fold_locale[c1];
	  do_exactf:
	    e = strend - ln;

	    if (norun && e < s)
		e = s;			/* Due to minlen logic of intuit() */
	    /* Here it is NOT UTF!  */
	    if (c1 == c2) {
		while (s <= e) {
		    if ( *(U8*)s == c1
			 && (ln == 1 || !(OP(c) == EXACTF
					  ? ibcmp(s, m, ln)
					  : ibcmp_locale(s, m, ln)))
			 && (norun || regtry(prog, s)) )
			goto got_it;
		    s++;
		}
	    } else {
		while (s <= e) {
		    if ( (*(U8*)s == c1 || *(U8*)s == c2)
			 && (ln == 1 || !(OP(c) == EXACTF
					  ? ibcmp(s, m, ln)
					  : ibcmp_locale(s, m, ln)))
			 && (norun || regtry(prog, s)) )
			goto got_it;
		    s++;
		}
	    }
	    break;
	case BOUNDL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case BOUND:
	    tmp = (s != startpos) ? UCHARAT(s - 1) : '\n';
	    tmp = ((OP(c) == BOUND ? isALNUM(tmp) : isALNUM_LC(tmp)) != 0);
	    while (s < strend) {
		if (tmp == !(OP(c) == BOUND ? isALNUM(*s) : isALNUM_LC(*s))) {
		    tmp = !tmp;
		    if ((norun || regtry(prog, s)))
			goto got_it;
		}
		s++;
	    }
	    if ((!prog->minlen && tmp) && (norun || regtry(prog, s)))
		goto got_it;
	    break;
	case BOUNDLUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case BOUNDUTF8:
	    if (s == startpos)
		tmp = '\n';
	    else {
		U8 *r = reghop((U8*)s, -1);

		tmp = (I32)utf8_to_uv(r, s - (char*)r, 0, 0);
	    }
	    tmp = ((OP(c) == BOUNDUTF8 ?
		    isALNUM_uni(tmp) : isALNUM_LC_uni(tmp)) != 0);
	    while (s < strend) {
		if (tmp == !(OP(c) == BOUNDUTF8 ?
			     swash_fetch(PL_utf8_alnum, (U8*)s) :
			     isALNUM_LC_utf8((U8*)s)))
		{
		    tmp = !tmp;
		    if ((norun || regtry(prog, s)))
			goto got_it;
		}
		s += UTF8SKIP(s);
	    }
	    if ((!prog->minlen && tmp) && (norun || regtry(prog, s)))
		goto got_it;
	    break;
	case NBOUNDL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NBOUND:
	    tmp = (s != startpos) ? UCHARAT(s - 1) : '\n';
	    tmp = ((OP(c) == NBOUND ? isALNUM(tmp) : isALNUM_LC(tmp)) != 0);
	    while (s < strend) {
		if (tmp == !(OP(c) == NBOUND ? isALNUM(*s) : isALNUM_LC(*s)))
		    tmp = !tmp;
		else if ((norun || regtry(prog, s)))
		    goto got_it;
		s++;
	    }
	    if ((!prog->minlen && !tmp) && (norun || regtry(prog, s)))
		goto got_it;
	    break;
	case NBOUNDLUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NBOUNDUTF8:
	    if (s == startpos)
		tmp = '\n';
	    else {
		U8 *r = reghop((U8*)s, -1);

		tmp = (I32)utf8_to_uv(r, s - (char*)r, 0, 0);
	    }
	    tmp = ((OP(c) == NBOUNDUTF8 ?
		    isALNUM_uni(tmp) : isALNUM_LC_uni(tmp)) != 0);
	    while (s < strend) {
		if (tmp == !(OP(c) == NBOUNDUTF8 ?
			     swash_fetch(PL_utf8_alnum, (U8*)s) :
			     isALNUM_LC_utf8((U8*)s)))
		    tmp = !tmp;
		else if ((norun || regtry(prog, s)))
		    goto got_it;
		s += UTF8SKIP(s);
	    }
	    if ((!prog->minlen && !tmp) && (norun || regtry(prog, s)))
		goto got_it;
	    break;
	case ALNUM:
	    while (s < strend) {
		if (isALNUM(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case ALNUMUTF8:
	    while (s < strend) {
		if (swash_fetch(PL_utf8_alnum, (U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case ALNUML:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (isALNUM_LC(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case ALNUMLUTF8:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (isALNUM_LC_utf8((U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case NALNUM:
	    while (s < strend) {
		if (!isALNUM(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NALNUMUTF8:
	    while (s < strend) {
		if (!swash_fetch(PL_utf8_alnum, (U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case NALNUML:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (!isALNUM_LC(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NALNUMLUTF8:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (!isALNUM_LC_utf8((U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case SPACE:
	    while (s < strend) {
		if (isSPACE(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case SPACEUTF8:
	    while (s < strend) {
		if (*s == ' ' || swash_fetch(PL_utf8_space,(U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case SPACEL:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (isSPACE_LC(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case SPACELUTF8:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (*s == ' ' || isSPACE_LC_utf8((U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case NSPACE:
	    while (s < strend) {
		if (!isSPACE(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NSPACEUTF8:
	    while (s < strend) {
		if (!(*s == ' ' || swash_fetch(PL_utf8_space,(U8*)s))) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case NSPACEL:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (!isSPACE_LC(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NSPACELUTF8:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (!(*s == ' ' || isSPACE_LC_utf8((U8*)s))) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case DIGIT:
	    while (s < strend) {
		if (isDIGIT(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case DIGITUTF8:
	    while (s < strend) {
		if (swash_fetch(PL_utf8_digit,(U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case DIGITL:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (isDIGIT_LC(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case DIGITLUTF8:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (isDIGIT_LC_utf8((U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case NDIGIT:
	    while (s < strend) {
		if (!isDIGIT(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NDIGITUTF8:
	    while (s < strend) {
		if (!swash_fetch(PL_utf8_digit,(U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	case NDIGITL:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (!isDIGIT_LC(*s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NDIGITLUTF8:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (!isDIGIT_LC_utf8((U8*)s)) {
		    if (tmp && (norun || regtry(prog, s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s += UTF8SKIP(s);
	    }
	    break;
	default:
	    Perl_croak(aTHX_ "panic: unknown regstclass %d", (int)OP(c));
	    break;
	}
	return 0;
      got_it:
	return s;
}

/*
 - regexec_flags - match a regexp against a string
 */
I32
Perl_regexec_flags(pTHX_ register regexp *prog, char *stringarg, register char *strend,
	      char *strbeg, I32 minend, SV *sv, void *data, U32 flags)
/* strend: pointer to null at end of string */
/* strbeg: real beginning of string */
/* minend: end of match must be >=minend after stringarg. */
/* data: May be used for some additional optimizations. */
/* nosave: For optimizations. */
{
    register char *s;
    register regnode *c;
    register char *startpos = stringarg;
    I32 minlen;		/* must match at least this many chars */
    I32 dontbother = 0;	/* how many characters not to try at end */
    /* I32 start_shift = 0; */		/* Offset of the start to find
					 constant substr. */		/* CC */
    I32 end_shift = 0;			/* Same for the end. */		/* CC */
    I32 scream_pos = -1;		/* Internal iterator of scream. */
    char *scream_olds;
    SV* oreplsv = GvSV(PL_replgv);

    PL_regcc = 0;

    cache_re(prog);
#ifdef DEBUGGING
    PL_regnarrate = PL_debug & 512;
#endif

    /* Be paranoid... */
    if (prog == NULL || startpos == NULL) {
	Perl_croak(aTHX_ "NULL regexp parameter");
	return 0;
    }

    minlen = prog->minlen;
    if (strend - startpos < minlen) goto phooey;

    if (startpos == strbeg)	/* is ^ valid at stringarg? */
	PL_regprev = '\n';
    else {
	PL_regprev = (U32)stringarg[-1];
	if (!PL_multiline && PL_regprev == '\n')
	    PL_regprev = '\0';		/* force ^ to NOT match */
    }

    /* Check validity of program. */
    if (UCHARAT(prog->program) != REG_MAGIC) {
	Perl_croak(aTHX_ "corrupted regexp program");
    }

    PL_reg_flags = 0;
    PL_reg_eval_set = 0;
    PL_reg_maxiter = 0;

    if (prog->reganch & ROPT_UTF8)
	PL_reg_flags |= RF_utf8;

    /* Mark beginning of line for ^ and lookbehind. */
    PL_regbol = startpos;
    PL_bostr  = strbeg;
    PL_reg_sv = sv;

    /* Mark end of line for $ (and such) */
    PL_regeol = strend;

    /* see how far we have to get to not match where we matched before */
    PL_regtill = startpos+minend;

    /* We start without call_cc context.  */
    PL_reg_call_cc = 0;

    /* If there is a "must appear" string, look for it. */
    s = startpos;

    if (prog->reganch & ROPT_GPOS_SEEN) { /* Need to have PL_reg_ganch */
	MAGIC *mg;

	if (flags & REXEC_IGNOREPOS)	/* Means: check only at start */
	    PL_reg_ganch = startpos;
	else if (sv && SvTYPE(sv) >= SVt_PVMG
		  && SvMAGIC(sv)
		  && (mg = mg_find(sv, 'g')) && mg->mg_len >= 0) {
	    PL_reg_ganch = strbeg + mg->mg_len;	/* Defined pos() */
	    if (prog->reganch & ROPT_ANCH_GPOS) {
		if (s > PL_reg_ganch)
		    goto phooey;
		s = PL_reg_ganch;
	    }
	}
	else				/* pos() not defined */
	    PL_reg_ganch = strbeg;
    }

    if (!(flags & REXEC_CHECKED) && prog->check_substr != Nullsv) {
	re_scream_pos_data d;

	d.scream_olds = &scream_olds;
	d.scream_pos = &scream_pos;
	s = re_intuit_start(prog, sv, s, strend, flags, &d);
	if (!s)
	    goto phooey;	/* not present */
    }

    DEBUG_r( if (!PL_colorset) reginitcolors() );
    DEBUG_r(PerlIO_printf(Perl_debug_log,
		      "%sMatching REx%s `%s%.60s%s%s' against `%s%.*s%s%s'\n",
		      PL_colors[4],PL_colors[5],PL_colors[0],
		      prog->precomp,
		      PL_colors[1],
		      (strlen(prog->precomp) > 60 ? "..." : ""),
		      PL_colors[0],
		      (int)(strend - startpos > 60 ? 60 : strend - startpos),
		      startpos, PL_colors[1],
		      (strend - startpos > 60 ? "..." : ""))
	);

    /* Simplest case:  anchored match need be tried only once. */
    /*  [unless only anchor is BOL and multiline is set] */
    if (prog->reganch & (ROPT_ANCH & ~ROPT_ANCH_GPOS)) {
	if (s == startpos && regtry(prog, startpos))
	    goto got_it;
	else if (PL_multiline || (prog->reganch & ROPT_IMPLICIT)
		 || (prog->reganch & ROPT_ANCH_MBOL)) /* XXXX SBOL? */
	{
	    char *end;

	    if (minlen)
		dontbother = minlen - 1;
	    end = HOPc(strend, -dontbother) - 1;
	    /* for multiline we only have to try after newlines */
	    if (prog->check_substr) {
		if (s == startpos)
		    goto after_try;
		while (1) {
		    if (regtry(prog, s))
			goto got_it;
		  after_try:
		    if (s >= end)
			goto phooey;
		    if (prog->reganch & RE_USE_INTUIT) {
			s = re_intuit_start(prog, sv, s + 1, strend, flags, NULL);
			if (!s)
			    goto phooey;
		    }
		    else
			s++;
		}		
	    } else {
		if (s > startpos)
		    s--;
		while (s < end) {
		    if (*s++ == '\n') {	/* don't need PL_utf8skip here */
			if (regtry(prog, s))
			    goto got_it;
		    }
		}		
	    }
	}
	goto phooey;
    } else if (prog->reganch & ROPT_ANCH_GPOS) {
	if (regtry(prog, PL_reg_ganch))
	    goto got_it;
	goto phooey;
    }

    /* Messy cases:  unanchored match. */
    if (prog->anchored_substr && prog->reganch & ROPT_SKIP) { 
	/* we have /x+whatever/ */
	/* it must be a one character string (XXXX Except UTF?) */
	char ch = SvPVX(prog->anchored_substr)[0];
#ifdef DEBUGGING
	int did_match = 0;
#endif

	if (UTF) {
	    while (s < strend) {
		if (*s == ch) {
		    DEBUG_r( did_match = 1 );
		    if (regtry(prog, s)) goto got_it;
		    s += UTF8SKIP(s);
		    while (s < strend && *s == ch)
			s += UTF8SKIP(s);
		}
		s += UTF8SKIP(s);
	    }
	}
	else {
	    while (s < strend) {
		if (*s == ch) {
		    DEBUG_r( did_match = 1 );
		    if (regtry(prog, s)) goto got_it;
		    s++;
		    while (s < strend && *s == ch)
			s++;
		}
		s++;
	    }
	}
	DEBUG_r(did_match ||
		PerlIO_printf(Perl_debug_log,
			      "Did not find anchored character...\n"));
    }
    /*SUPPRESS 560*/
    else if (prog->anchored_substr != Nullsv
	     || (prog->float_substr != Nullsv 
		 && prog->float_max_offset < strend - s)) {
	SV *must = prog->anchored_substr 
	    ? prog->anchored_substr : prog->float_substr;
	I32 back_max = 
	    prog->anchored_substr ? prog->anchored_offset : prog->float_max_offset;
	I32 back_min = 
	    prog->anchored_substr ? prog->anchored_offset : prog->float_min_offset;
	char *last = HOPc(strend,	/* Cannot start after this */
			  -(I32)(CHR_SVLEN(must)
				 - (SvTAIL(must) != 0) + back_min));
	char *last1;		/* Last position checked before */
#ifdef DEBUGGING
	int did_match = 0;
#endif

	if (s > PL_bostr)
	    last1 = HOPc(s, -1);
	else
	    last1 = s - 1;	/* bogus */

	/* XXXX check_substr already used to find `s', can optimize if
	   check_substr==must. */
	scream_pos = -1;
	dontbother = end_shift;
	strend = HOPc(strend, -dontbother);
	while ( (s <= last) &&
		((flags & REXEC_SCREAM) 
		 ? (s = screaminstr(sv, must, HOPc(s, back_min) - strbeg,
				    end_shift, &scream_pos, 0))
		 : (s = fbm_instr((unsigned char*)HOP(s, back_min),
				  (unsigned char*)strend, must, 
				  PL_multiline ? FBMrf_MULTILINE : 0))) ) {
	    DEBUG_r( did_match = 1 );
	    if (HOPc(s, -back_max) > last1) {
		last1 = HOPc(s, -back_min);
		s = HOPc(s, -back_max);
	    }
	    else {
		char *t = (last1 >= PL_bostr) ? HOPc(last1, 1) : last1 + 1;

		last1 = HOPc(s, -back_min);
		s = t;		
	    }
	    if (UTF) {
		while (s <= last1) {
		    if (regtry(prog, s))
			goto got_it;
		    s += UTF8SKIP(s);
		}
	    }
	    else {
		while (s <= last1) {
		    if (regtry(prog, s))
			goto got_it;
		    s++;
		}
	    }
	}
	DEBUG_r(did_match ||
		PerlIO_printf(Perl_debug_log, "Did not find %s substr `%s%.*s%s'%s...\n",
			      ((must == prog->anchored_substr)
			       ? "anchored" : "floating"),
			      PL_colors[0],
			      (int)(SvCUR(must) - (SvTAIL(must)!=0)),
			      SvPVX(must),
			      PL_colors[1], (SvTAIL(must) ? "$" : "")));
	goto phooey;
    }
    else if ((c = prog->regstclass)) {
	if (minlen && PL_regkind[(U8)OP(prog->regstclass)] != EXACT)
	    /* don't bother with what can't match */
	    strend = HOPc(strend, -(minlen - 1));
  	if (find_byclass(prog, c, s, strend, startpos, 0))
	    goto got_it;
	DEBUG_r(PerlIO_printf(Perl_debug_log, "Contradicts stclass...\n"));
    }
    else {
	dontbother = 0;
	if (prog->float_substr != Nullsv) {	/* Trim the end. */
	    char *last;

	    if (flags & REXEC_SCREAM) {
		last = screaminstr(sv, prog->float_substr, s - strbeg,
				   end_shift, &scream_pos, 1); /* last one */
		if (!last)
		    last = scream_olds; /* Only one occurence. */
	    }
	    else {
		STRLEN len;
		char *little = SvPV(prog->float_substr, len);

		if (SvTAIL(prog->float_substr)) {
		    if (memEQ(strend - len + 1, little, len - 1))
			last = strend - len + 1;
		    else if (!PL_multiline)
			last = memEQ(strend - len, little, len) 
			    ? strend - len : Nullch;
		    else
			goto find_last;
		} else {
		  find_last:
		    if (len) 
			last = rninstr(s, strend, little, little + len);
		    else
			last = strend;	/* matching `$' */
		}
	    }
	    if (last == NULL) {
		DEBUG_r(PerlIO_printf(Perl_debug_log,
				      "%sCan't trim the tail, match fails (should not happen)%s\n",
				      PL_colors[4],PL_colors[5]));
		goto phooey; /* Should not happen! */
	    }
	    dontbother = strend - last + prog->float_min_offset;
	}
	if (minlen && (dontbother < minlen))
	    dontbother = minlen - 1;
	strend -= dontbother; 		   /* this one's always in bytes! */
	/* We don't know much -- general case. */
	if (UTF) {
	    for (;;) {
		if (regtry(prog, s))
		    goto got_it;
		if (s >= strend)
		    break;
		s += UTF8SKIP(s);
	    };
	}
	else {
	    do {
		if (regtry(prog, s))
		    goto got_it;
	    } while (s++ < strend);
	}
    }

    /* Failure. */
    goto phooey;

got_it:
    RX_MATCH_TAINTED_set(prog, PL_reg_flags & RF_tainted);

    if (PL_reg_eval_set) {
	/* Preserve the current value of $^R */
	if (oreplsv != GvSV(PL_replgv))
	    sv_setsv(oreplsv, GvSV(PL_replgv));/* So that when GvSV(replgv) is
						  restored, the value remains
						  the same. */
	restore_pos(aTHXo_ 0);
    }

    /* make sure $`, $&, $', and $digit will work later */
    if ( !(flags & REXEC_NOT_FIRST) ) {
	if (RX_MATCH_COPIED(prog)) {
	    Safefree(prog->subbeg);
	    RX_MATCH_COPIED_off(prog);
	}
	if (flags & REXEC_COPY_STR) {
	    I32 i = PL_regeol - startpos + (stringarg - strbeg);

	    s = savepvn(strbeg, i);
	    prog->subbeg = s;
	    prog->sublen = i;
	    RX_MATCH_COPIED_on(prog);
	}
	else {
	    prog->subbeg = strbeg;
	    prog->sublen = PL_regeol - strbeg;	/* strend may have been modified */
	}
    }
    
    return 1;

phooey:
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%sMatch failed%s\n",
			  PL_colors[4],PL_colors[5]));
    if (PL_reg_eval_set)
	restore_pos(aTHXo_ 0);
    return 0;
}

/*
 - regtry - try match at specific point
 */
STATIC I32			/* 0 failure, 1 success */
S_regtry(pTHX_ regexp *prog, char *startpos)
{
    register I32 i;
    register I32 *sp;
    register I32 *ep;
    CHECKPOINT lastcp;

#ifdef DEBUGGING
    PL_regindent = 0;	/* XXXX Not good when matches are reenterable... */
#endif
    if ((prog->reganch & ROPT_EVAL_SEEN) && !PL_reg_eval_set) {
	MAGIC *mg;

	PL_reg_eval_set = RS_init;
	DEBUG_r(DEBUG_s(
	    PerlIO_printf(Perl_debug_log, "  setting stack tmpbase at %"IVdf"\n",
			  (IV)(PL_stack_sp - PL_stack_base));
	    ));
	SAVEI32(cxstack[cxstack_ix].blk_oldsp);
	cxstack[cxstack_ix].blk_oldsp = PL_stack_sp - PL_stack_base;
	/* Otherwise OP_NEXTSTATE will free whatever on stack now.  */
	SAVETMPS;
	/* Apparently this is not needed, judging by wantarray. */
	/* SAVEI8(cxstack[cxstack_ix].blk_gimme);
	   cxstack[cxstack_ix].blk_gimme = G_SCALAR; */

	if (PL_reg_sv) {
	    /* Make $_ available to executed code. */
	    if (PL_reg_sv != DEFSV) {
		/* SAVE_DEFSV does *not* suffice here for USE_THREADS */
		SAVESPTR(DEFSV);
		DEFSV = PL_reg_sv;
	    }
	
	    if (!(SvTYPE(PL_reg_sv) >= SVt_PVMG && SvMAGIC(PL_reg_sv) 
		  && (mg = mg_find(PL_reg_sv, 'g')))) {
		/* prepare for quick setting of pos */
		sv_magic(PL_reg_sv, (SV*)0, 'g', Nullch, 0);
		mg = mg_find(PL_reg_sv, 'g');
		mg->mg_len = -1;
	    }
	    PL_reg_magic    = mg;
	    PL_reg_oldpos   = mg->mg_len;
	    SAVEDESTRUCTOR_X(restore_pos, 0);
        }
	if (!PL_reg_curpm)
	    Newz(22,PL_reg_curpm, 1, PMOP);
	PL_reg_curpm->op_pmregexp = prog;
	PL_reg_oldcurpm = PL_curpm;
	PL_curpm = PL_reg_curpm;
	if (RX_MATCH_COPIED(prog)) {
	    /*  Here is a serious problem: we cannot rewrite subbeg,
		since it may be needed if this match fails.  Thus
		$` inside (?{}) could fail... */
	    PL_reg_oldsaved = prog->subbeg;
	    PL_reg_oldsavedlen = prog->sublen;
	    RX_MATCH_COPIED_off(prog);
	}
	else
	    PL_reg_oldsaved = Nullch;
	prog->subbeg = PL_bostr;
	prog->sublen = PL_regeol - PL_bostr; /* strend may have been modified */
    }
    prog->startp[0] = startpos - PL_bostr;
    PL_reginput = startpos;
    PL_regstartp = prog->startp;
    PL_regendp = prog->endp;
    PL_reglastparen = &prog->lastparen;
    prog->lastparen = 0;
    PL_regsize = 0;
    DEBUG_r(PL_reg_starttry = startpos);
    if (PL_reg_start_tmpl <= prog->nparens) {
	PL_reg_start_tmpl = prog->nparens*3/2 + 3;
        if(PL_reg_start_tmp)
            Renew(PL_reg_start_tmp, PL_reg_start_tmpl, char*);
        else
            New(22,PL_reg_start_tmp, PL_reg_start_tmpl, char*);
    }

    /* XXXX What this code is doing here?!!!  There should be no need
       to do this again and again, PL_reglastparen should take care of
       this!  --ilya*/

    /* Tests pat.t#187 and split.t#{13,14} seem to depend on this code.
     * Actually, the code in regcppop() (which Ilya may be meaning by
     * PL_reglastparen), is not needed at all by the test suite
     * (op/regexp, op/pat, op/split), but that code is needed, oddly
     * enough, for building DynaLoader, or otherwise this
     * "Error: '*' not in typemap in DynaLoader.xs, line 164"
     * will happen.  Meanwhile, this code *is* needed for the
     * above-mentioned test suite tests to succeed.  The common theme
     * on those tests seems to be returning null fields from matches.
     * --jhi */
#if 1
    sp = prog->startp;
    ep = prog->endp;
    if (prog->nparens) {
	for (i = prog->nparens; i > *PL_reglastparen; i--) {
	    *++sp = -1;
	    *++ep = -1;
	}
    }
#endif
    REGCP_SET(lastcp);
    if (regmatch(prog->program + 1)) {
	prog->endp[0] = PL_reginput - PL_bostr;
	return 1;
    }
    REGCP_UNWIND(lastcp);
    return 0;
}

#define RE_UNWIND_BRANCH	1
#define RE_UNWIND_BRANCHJ	2

union re_unwind_t;

typedef struct {		/* XX: makes sense to enlarge it... */
    I32 type;
    I32 prev;
    CHECKPOINT lastcp;
} re_unwind_generic_t;

typedef struct {
    I32 type;
    I32 prev;
    CHECKPOINT lastcp;
    I32 lastparen;
    regnode *next;
    char *locinput;
    I32 nextchr;
#ifdef DEBUGGING
    int regindent;
#endif
} re_unwind_branch_t;

typedef union re_unwind_t {
    I32 type;
    re_unwind_generic_t generic;
    re_unwind_branch_t branch;
} re_unwind_t;

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
/* [lwall] I've hoisted the register declarations to the outer block in order to
 * maybe save a little bit of pushing and popping on the stack.  It also takes
 * advantage of machines that use a register save mask on subroutine entry.
 */
STATIC I32			/* 0 failure, 1 success */
S_regmatch(pTHX_ regnode *prog)
{
    register regnode *scan;	/* Current node. */
    regnode *next;		/* Next node. */
    regnode *inner;		/* Next node in internal branch. */
    register I32 nextchr;	/* renamed nextchr - nextchar colides with
				   function of same name */
    register I32 n;		/* no or next */
    register I32 ln;		/* len or last */
    register char *s;		/* operand or save */
    register char *locinput = PL_reginput;
    register I32 c1, c2, paren;	/* case fold search, parenth */
    int minmod = 0, sw = 0, logical = 0;
    I32 unwind = 0;
    I32 firstcp = PL_savestack_ix;

#ifdef DEBUGGING
    PL_regindent++;
#endif

    /* Note that nextchr is a byte even in UTF */
    nextchr = UCHARAT(locinput);
    scan = prog;
    while (scan != NULL) {
#define sayNO_L (logical ? (logical = 0, sw = 0, goto cont) : sayNO)
#if 1
#  define sayYES goto yes
#  define sayNO goto no
#  define sayYES_FINAL goto yes_final
#  define sayYES_LOUD  goto yes_loud
#  define sayNO_FINAL  goto no_final
#  define sayNO_SILENT goto do_no
#  define saySAME(x) if (x) goto yes; else goto no
#  define REPORT_CODE_OFF 24
#else
#  define sayYES return 1
#  define sayNO return 0
#  define sayYES_FINAL return 1
#  define sayYES_LOUD  return 1
#  define sayNO_FINAL  return 0
#  define sayNO_SILENT return 0
#  define saySAME(x) return x
#endif
	DEBUG_r( {
	    SV *prop = sv_newmortal();
	    int docolor = *PL_colors[0];
	    int taill = (docolor ? 10 : 7); /* 3 chars for "> <" */
	    int l = (PL_regeol - locinput > taill ? taill : PL_regeol - locinput);
	    /* The part of the string before starttry has one color
	       (pref0_len chars), between starttry and current
	       position another one (pref_len - pref0_len chars),
	       after the current position the third one.
	       We assume that pref0_len <= pref_len, otherwise we
	       decrease pref0_len.  */
	    int pref_len = (locinput - PL_bostr > (5 + taill) - l 
			    ? (5 + taill) - l : locinput - PL_bostr);
	    int pref0_len = pref_len  - (locinput - PL_reg_starttry);

	    if (l + pref_len < (5 + taill) && l < PL_regeol - locinput)
		l = ( PL_regeol - locinput > (5 + taill) - pref_len 
		      ? (5 + taill) - pref_len : PL_regeol - locinput);
	    if (pref0_len < 0)
		pref0_len = 0;
	    if (pref0_len > pref_len)
		pref0_len = pref_len;
	    regprop(prop, scan);
	    PerlIO_printf(Perl_debug_log, 
			  "%4"IVdf" <%s%.*s%s%s%.*s%s%s%s%.*s%s>%*s|%3"IVdf":%*s%s\n",
			  (IV)(locinput - PL_bostr), 
			  PL_colors[4], pref0_len, 
			  locinput - pref_len, PL_colors[5],
			  PL_colors[2], pref_len - pref0_len, 
			  locinput - pref_len + pref0_len, PL_colors[3],
			  (docolor ? "" : "> <"),
			  PL_colors[0], l, locinput, PL_colors[1],
			  15 - l - pref_len + 1,
			  "",
			  (IV)(scan - PL_regprogram), PL_regindent*2, "",
			  SvPVX(prop));
	} );

	next = scan + NEXT_OFF(scan);
	if (next == scan)
	    next = NULL;

	switch (OP(scan)) {
	case BOL:
	    if (locinput == PL_bostr
		? PL_regprev == '\n'
		: (PL_multiline && 
		   (nextchr || locinput < PL_regeol) && locinput[-1] == '\n') )
	    {
		/* regtill = regbol; */
		break;
	    }
	    sayNO;
	case MBOL:
	    if (locinput == PL_bostr
		? PL_regprev == '\n'
		: ((nextchr || locinput < PL_regeol) && locinput[-1] == '\n') )
	    {
		break;
	    }
	    sayNO;
	case SBOL:
	    if (locinput == PL_bostr)
		break;
	    sayNO;
	case GPOS:
	    if (locinput == PL_reg_ganch)
		break;
	    sayNO;
	case EOL:
	    if (PL_multiline)
		goto meol;
	    else
		goto seol;
	case MEOL:
	  meol:
	    if ((nextchr || locinput < PL_regeol) && nextchr != '\n')
		sayNO;
	    break;
	case SEOL:
	  seol:
	    if ((nextchr || locinput < PL_regeol) && nextchr != '\n')
		sayNO;
	    if (PL_regeol - locinput > 1)
		sayNO;
	    break;
	case EOS:
	    if (PL_regeol != locinput)
		sayNO;
	    break;
	case SANYUTF8:
	    if (nextchr & 0x80) {
		locinput += PL_utf8skip[nextchr];
		if (locinput > PL_regeol)
		    sayNO;
		nextchr = UCHARAT(locinput);
		break;
	    }
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case SANY:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case ANYUTF8:
	    if (nextchr & 0x80) {
		locinput += PL_utf8skip[nextchr];
		if (locinput > PL_regeol)
		    sayNO;
		nextchr = UCHARAT(locinput);
		break;
	    }
	    if ((!nextchr && locinput >= PL_regeol) || nextchr == '\n')
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case REG_ANY:
	    if ((!nextchr && locinput >= PL_regeol) || nextchr == '\n')
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case EXACT:
	    s = STRING(scan);
	    ln = STR_LEN(scan);
	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchr)
		sayNO;
	    if (PL_regeol - locinput < ln)
		sayNO;
	    if (ln > 1 && memNE(s, locinput, ln))
		sayNO;
	    locinput += ln;
	    nextchr = UCHARAT(locinput);
	    break;
	case EXACTFL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case EXACTF:
	    s = STRING(scan);
	    ln = STR_LEN(scan);

	    if (UTF) {
		char *l = locinput;
		char *e = s + ln;
		c1 = OP(scan) == EXACTF;
		while (s < e) {
		    if (l >= PL_regeol)
			sayNO;
		    if (utf8_to_uv((U8*)s, e - s, 0, 0) !=
			(c1 ?
			 toLOWER_utf8((U8*)l) :
			 toLOWER_LC_utf8((U8*)l)))
		    {
			sayNO;
		    }
		    s += UTF8SKIP(s);
		    l += UTF8SKIP(l);
		}
		locinput = l;
		nextchr = UCHARAT(locinput);
		break;
	    }

	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchr &&
		UCHARAT(s) != ((OP(scan) == EXACTF)
			       ? PL_fold : PL_fold_locale)[nextchr])
		sayNO;
	    if (PL_regeol - locinput < ln)
		sayNO;
	    if (ln > 1 && (OP(scan) == EXACTF
			   ? ibcmp(s, locinput, ln)
			   : ibcmp_locale(s, locinput, ln)))
		sayNO;
	    locinput += ln;
	    nextchr = UCHARAT(locinput);
	    break;
	case ANYOFUTF8:
	    if (!REGINCLASSUTF8(scan, (U8*)locinput))
		sayNO;
	    if (locinput >= PL_regeol)
		sayNO;
	    locinput += PL_utf8skip[nextchr];
	    nextchr = UCHARAT(locinput);
	    break;
	case ANYOF:
	    if (nextchr < 0)
		nextchr = UCHARAT(locinput);
	    if (!REGINCLASS(scan, nextchr))
		sayNO;
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case ALNUML:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case ALNUM:
	    if (!nextchr)
		sayNO;
	    if (!(OP(scan) == ALNUM
		  ? isALNUM(nextchr) : isALNUM_LC(nextchr)))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case ALNUMLUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case ALNUMUTF8:
	    if (!nextchr)
		sayNO;
	    if (nextchr & 0x80) {
		if (!(OP(scan) == ALNUMUTF8
		      ? swash_fetch(PL_utf8_alnum, (U8*)locinput)
		      : isALNUM_LC_utf8((U8*)locinput)))
		{
		    sayNO;
		}
		locinput += PL_utf8skip[nextchr];
		nextchr = UCHARAT(locinput);
		break;
	    }
	    if (!(OP(scan) == ALNUMUTF8
		  ? isALNUM(nextchr) : isALNUM_LC(nextchr)))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case NALNUML:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NALNUM:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    if (OP(scan) == NALNUM
		? isALNUM(nextchr) : isALNUM_LC(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case NALNUMLUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NALNUMUTF8:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    if (nextchr & 0x80) {
		if (OP(scan) == NALNUMUTF8
		    ? swash_fetch(PL_utf8_alnum, (U8*)locinput)
		    : isALNUM_LC_utf8((U8*)locinput))
		{
		    sayNO;
		}
		locinput += PL_utf8skip[nextchr];
		nextchr = UCHARAT(locinput);
		break;
	    }
	    if (OP(scan) == NALNUMUTF8
		? isALNUM(nextchr) : isALNUM_LC(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case BOUNDL:
	case NBOUNDL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case BOUND:
	case NBOUND:
	    /* was last char in word? */
	    ln = (locinput != PL_regbol) ? UCHARAT(locinput - 1) : PL_regprev;
	    if (OP(scan) == BOUND || OP(scan) == NBOUND) {
		ln = isALNUM(ln);
		n = isALNUM(nextchr);
	    }
	    else {
		ln = isALNUM_LC(ln);
		n = isALNUM_LC(nextchr);
	    }
	    if (((!ln) == (!n)) == (OP(scan) == BOUND || OP(scan) == BOUNDL))
		sayNO;
	    break;
	case BOUNDLUTF8:
	case NBOUNDLUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case BOUNDUTF8:
	case NBOUNDUTF8:
	    /* was last char in word? */
	    if (locinput == PL_regbol)
		ln = PL_regprev;
	    else {
		U8 *r = reghop((U8*)locinput, -1);

		ln = utf8_to_uv(r, s - (char*)r, 0, 0);
	    }
	    if (OP(scan) == BOUNDUTF8 || OP(scan) == NBOUNDUTF8) {
		ln = isALNUM_uni(ln);
		n = swash_fetch(PL_utf8_alnum, (U8*)locinput);
	    }
	    else {
		ln = isALNUM_LC_uni(ln);
		n = isALNUM_LC_utf8((U8*)locinput);
	    }
	    if (((!ln) == (!n)) == (OP(scan) == BOUNDUTF8 || OP(scan) == BOUNDLUTF8))
		sayNO;
	    break;
	case SPACEL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case SPACE:
	    if (!nextchr)
		sayNO;
	    if (!(OP(scan) == SPACE
		  ? isSPACE(nextchr) : isSPACE_LC(nextchr)))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case SPACELUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case SPACEUTF8:
	    if (!nextchr)
		sayNO;
	    if (nextchr & 0x80) {
		if (!(OP(scan) == SPACEUTF8
		      ? swash_fetch(PL_utf8_space, (U8*)locinput)
		      : isSPACE_LC_utf8((U8*)locinput)))
		{
		    sayNO;
		}
		locinput += PL_utf8skip[nextchr];
		nextchr = UCHARAT(locinput);
		break;
	    }
	    if (!(OP(scan) == SPACEUTF8
		  ? isSPACE(nextchr) : isSPACE_LC(nextchr)))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case NSPACEL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NSPACE:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    if (OP(scan) == NSPACE
		? isSPACE(nextchr) : isSPACE_LC(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case NSPACELUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NSPACEUTF8:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    if (nextchr & 0x80) {
		if (OP(scan) == NSPACEUTF8
		    ? swash_fetch(PL_utf8_space, (U8*)locinput)
		    : isSPACE_LC_utf8((U8*)locinput))
		{
		    sayNO;
		}
		locinput += PL_utf8skip[nextchr];
		nextchr = UCHARAT(locinput);
		break;
	    }
	    if (OP(scan) == NSPACEUTF8
		? isSPACE(nextchr) : isSPACE_LC(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case DIGITL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case DIGIT:
	    if (!nextchr)
		sayNO;
	    if (!(OP(scan) == DIGIT
		  ? isDIGIT(nextchr) : isDIGIT_LC(nextchr)))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case DIGITLUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case DIGITUTF8:
	    if (!nextchr)
		sayNO;
	    if (nextchr & 0x80) {
		if (!(OP(scan) == DIGITUTF8
		      ? swash_fetch(PL_utf8_digit, (U8*)locinput)
		      : isDIGIT_LC_utf8((U8*)locinput)))
		{
		    sayNO;
		}
		locinput += PL_utf8skip[nextchr];
		nextchr = UCHARAT(locinput);
		break;
	    }
	    if (!(OP(scan) == DIGITUTF8
		  ? isDIGIT(nextchr) : isDIGIT_LC(nextchr)))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case NDIGITL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NDIGIT:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    if (OP(scan) == NDIGIT
		? isDIGIT(nextchr) : isDIGIT_LC(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case NDIGITLUTF8:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NDIGITUTF8:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    if (nextchr & 0x80) {
		if (OP(scan) == NDIGITUTF8
		    ? swash_fetch(PL_utf8_digit, (U8*)locinput)
		    : isDIGIT_LC_utf8((U8*)locinput))
		{
		    sayNO;
		}
		locinput += PL_utf8skip[nextchr];
		nextchr = UCHARAT(locinput);
		break;
	    }
	    if (OP(scan) == NDIGITUTF8
		? isDIGIT(nextchr) : isDIGIT_LC(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case CLUMP:
	    if (locinput >= PL_regeol || swash_fetch(PL_utf8_mark,(U8*)locinput))
		sayNO;
	    locinput += PL_utf8skip[nextchr];
	    while (locinput < PL_regeol && swash_fetch(PL_utf8_mark,(U8*)locinput))
		locinput += UTF8SKIP(locinput);
	    if (locinput > PL_regeol)
		sayNO;
	    nextchr = UCHARAT(locinput);
	    break;
	case REFFL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
        case REF:
	case REFF:
	    n = ARG(scan);  /* which paren pair */
	    ln = PL_regstartp[n];
	    PL_reg_leftiter = PL_reg_maxiter;		/* Void cache */
	    if (*PL_reglastparen < n || ln == -1)
		sayNO;			/* Do not match unless seen CLOSEn. */
	    if (ln == PL_regendp[n])
		break;

	    s = PL_bostr + ln;
	    if (UTF && OP(scan) != REF) {	/* REF can do byte comparison */
		char *l = locinput;
		char *e = PL_bostr + PL_regendp[n];
		/*
		 * Note that we can't do the "other character" lookup trick as
		 * in the 8-bit case (no pun intended) because in Unicode we
		 * have to map both upper and title case to lower case.
		 */
		if (OP(scan) == REFF) {
		    while (s < e) {
			if (l >= PL_regeol)
			    sayNO;
			if (toLOWER_utf8((U8*)s) != toLOWER_utf8((U8*)l))
			    sayNO;
			s += UTF8SKIP(s);
			l += UTF8SKIP(l);
		    }
		}
		else {
		    while (s < e) {
			if (l >= PL_regeol)
			    sayNO;
			if (toLOWER_LC_utf8((U8*)s) != toLOWER_LC_utf8((U8*)l))
			    sayNO;
			s += UTF8SKIP(s);
			l += UTF8SKIP(l);
		    }
		}
		locinput = l;
		nextchr = UCHARAT(locinput);
		break;
	    }

	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchr &&
		(OP(scan) == REF ||
		 (UCHARAT(s) != ((OP(scan) == REFF
				  ? PL_fold : PL_fold_locale)[nextchr]))))
		sayNO;
	    ln = PL_regendp[n] - ln;
	    if (locinput + ln > PL_regeol)
		sayNO;
	    if (ln > 1 && (OP(scan) == REF
			   ? memNE(s, locinput, ln)
			   : (OP(scan) == REFF
			      ? ibcmp(s, locinput, ln)
			      : ibcmp_locale(s, locinput, ln))))
		sayNO;
	    locinput += ln;
	    nextchr = UCHARAT(locinput);
	    break;

	case NOTHING:
	case TAIL:
	    break;
	case BACK:
	    break;
	case EVAL:
	{
	    dSP;
	    OP_4tree *oop = PL_op;
	    COP *ocurcop = PL_curcop;
	    SV **ocurpad = PL_curpad;
	    SV *ret;
	    
	    n = ARG(scan);
	    PL_op = (OP_4tree*)PL_regdata->data[n];
	    DEBUG_r( PerlIO_printf(Perl_debug_log, "  re_eval 0x%"UVxf"\n", PTR2UV(PL_op)) );
	    PL_curpad = AvARRAY((AV*)PL_regdata->data[n + 2]);
	    PL_regendp[0] = PL_reg_magic->mg_len = locinput - PL_bostr;

	    CALLRUNOPS(aTHX);			/* Scalar context. */
	    SPAGAIN;
	    ret = POPs;
	    PUTBACK;
	    
	    PL_op = oop;
	    PL_curpad = ocurpad;
	    PL_curcop = ocurcop;
	    if (logical) {
		if (logical == 2) {	/* Postponed subexpression. */
		    regexp *re;
		    MAGIC *mg = Null(MAGIC*);
		    re_cc_state state;
		    CHECKPOINT cp, lastcp;

		    if(SvROK(ret) || SvRMAGICAL(ret)) {
			SV *sv = SvROK(ret) ? SvRV(ret) : ret;

			if(SvMAGICAL(sv))
			    mg = mg_find(sv, 'r');
		    }
		    if (mg) {
			re = (regexp *)mg->mg_obj;
			(void)ReREFCNT_inc(re);
		    }
		    else {
			STRLEN len;
			char *t = SvPV(ret, len);
			PMOP pm;
			char *oprecomp = PL_regprecomp;
			I32 osize = PL_regsize;
			I32 onpar = PL_regnpar;

			pm.op_pmflags = 0;
			pm.op_pmdynflags = (UTF||DO_UTF8(ret) ? PMdf_UTF8 : 0);
			re = CALLREGCOMP(aTHX_ t, t + len, &pm);
			if (!(SvFLAGS(ret) 
			      & (SVs_TEMP | SVs_PADTMP | SVf_READONLY)))
			    sv_magic(ret,(SV*)ReREFCNT_inc(re),'r',0,0);
			PL_regprecomp = oprecomp;
			PL_regsize = osize;
			PL_regnpar = onpar;
		    }
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log, 
				      "Entering embedded `%s%.60s%s%s'\n",
				      PL_colors[0],
				      re->precomp,
				      PL_colors[1],
				      (strlen(re->precomp) > 60 ? "..." : ""))
			);
		    state.node = next;
		    state.prev = PL_reg_call_cc;
		    state.cc = PL_regcc;
		    state.re = PL_reg_re;

		    PL_regcc = 0;
		    
		    cp = regcppush(0);	/* Save *all* the positions. */
		    REGCP_SET(lastcp);
		    cache_re(re);
		    state.ss = PL_savestack_ix;
		    *PL_reglastparen = 0;
		    PL_reg_call_cc = &state;
		    PL_reginput = locinput;

		    /* XXXX This is too dramatic a measure... */
		    PL_reg_maxiter = 0;

		    if (regmatch(re->program + 1)) {
			/* Even though we succeeded, we need to restore
			   global variables, since we may be wrapped inside
			   SUSPEND, thus the match may be not finished yet. */

			/* XXXX Do this only if SUSPENDed? */
			PL_reg_call_cc = state.prev;
			PL_regcc = state.cc;
			PL_reg_re = state.re;
			cache_re(PL_reg_re);

			/* XXXX This is too dramatic a measure... */
			PL_reg_maxiter = 0;

			/* These are needed even if not SUSPEND. */
			ReREFCNT_dec(re);
			regcpblow(cp);
			sayYES;
		    }
		    ReREFCNT_dec(re);
		    REGCP_UNWIND(lastcp);
		    regcppop();
		    PL_reg_call_cc = state.prev;
		    PL_regcc = state.cc;
		    PL_reg_re = state.re;
		    cache_re(PL_reg_re);

		    /* XXXX This is too dramatic a measure... */
		    PL_reg_maxiter = 0;

		    sayNO;
		}
		sw = SvTRUE(ret);
		logical = 0;
	    }
	    else
		sv_setsv(save_scalar(PL_replgv), ret);
	    break;
	}
	case OPEN:
	    n = ARG(scan);  /* which paren pair */
	    PL_reg_start_tmp[n] = locinput;
	    if (n > PL_regsize)
		PL_regsize = n;
	    break;
	case CLOSE:
	    n = ARG(scan);  /* which paren pair */
	    PL_regstartp[n] = PL_reg_start_tmp[n] - PL_bostr;
	    PL_regendp[n] = locinput - PL_bostr;
	    if (n > *PL_reglastparen)
		*PL_reglastparen = n;
	    break;
	case GROUPP:
	    n = ARG(scan);  /* which paren pair */
	    sw = (*PL_reglastparen >= n && PL_regendp[n] != -1);
	    break;
	case IFTHEN:
	    PL_reg_leftiter = PL_reg_maxiter;		/* Void cache */
	    if (sw)
		next = NEXTOPER(NEXTOPER(scan));
	    else {
		next = scan + ARG(scan);
		if (OP(next) == IFTHEN) /* Fake one. */
		    next = NEXTOPER(NEXTOPER(next));
	    }
	    break;
	case LOGICAL:
	    logical = scan->flags;
	    break;
/*******************************************************************
 PL_regcc contains infoblock about the innermost (...)* loop, and
 a pointer to the next outer infoblock.

 Here is how Y(A)*Z is processed (if it is compiled into CURLYX/WHILEM):

   1) After matching X, regnode for CURLYX is processed;

   2) This regnode creates infoblock on the stack, and calls 
      regmatch() recursively with the starting point at WHILEM node;

   3) Each hit of WHILEM node tries to match A and Z (in the order
      depending on the current iteration, min/max of {min,max} and
      greediness).  The information about where are nodes for "A"
      and "Z" is read from the infoblock, as is info on how many times "A"
      was already matched, and greediness.

   4) After A matches, the same WHILEM node is hit again.

   5) Each time WHILEM is hit, PL_regcc is the infoblock created by CURLYX
      of the same pair.  Thus when WHILEM tries to match Z, it temporarily
      resets PL_regcc, since this Y(A)*Z can be a part of some other loop:
      as in (Y(A)*Z)*.  If Z matches, the automaton will hit the WHILEM node
      of the external loop.

 Currently present infoblocks form a tree with a stem formed by PL_curcc
 and whatever it mentions via ->next, and additional attached trees
 corresponding to temporarily unset infoblocks as in "5" above.

 In the following picture infoblocks for outer loop of 
 (Y(A)*?Z)*?T are denoted O, for inner I.  NULL starting block
 is denoted by x.  The matched string is YAAZYAZT.  Temporarily postponed
 infoblocks are drawn below the "reset" infoblock.

 In fact in the picture below we do not show failed matches for Z and T
 by WHILEM blocks.  [We illustrate minimal matches, since for them it is
 more obvious *why* one needs to *temporary* unset infoblocks.]

  Matched	REx position	InfoBlocks	Comment
  		(Y(A)*?Z)*?T	x
  		Y(A)*?Z)*?T	x <- O
  Y		(A)*?Z)*?T	x <- O
  Y		A)*?Z)*?T	x <- O <- I
  YA		)*?Z)*?T	x <- O <- I
  YA		A)*?Z)*?T	x <- O <- I
  YAA		)*?Z)*?T	x <- O <- I
  YAA		Z)*?T		x <- O		# Temporary unset I
				     I

  YAAZ		Y(A)*?Z)*?T	x <- O
				     I

  YAAZY		(A)*?Z)*?T	x <- O
				     I

  YAAZY		A)*?Z)*?T	x <- O <- I
				     I

  YAAZYA	)*?Z)*?T	x <- O <- I	
				     I

  YAAZYA	Z)*?T		x <- O		# Temporary unset I
				     I,I

  YAAZYAZ	)*?T		x <- O
				     I,I

  YAAZYAZ	T		x		# Temporary unset O
				O
				I,I

  YAAZYAZT			x
				O
				I,I
 *******************************************************************/
	case CURLYX: {
		CURCUR cc;
		CHECKPOINT cp = PL_savestack_ix;
		/* No need to save/restore up to this paren */
		I32 parenfloor = scan->flags;

		if (OP(PREVOPER(next)) == NOTHING) /* LONGJMP */
		    next += ARG(next);
		cc.oldcc = PL_regcc;
		PL_regcc = &cc;
		/* XXXX Probably it is better to teach regpush to support
		   parenfloor > PL_regsize... */
		if (parenfloor > *PL_reglastparen)
		    parenfloor = *PL_reglastparen; /* Pessimization... */
		cc.parenfloor = parenfloor;
		cc.cur = -1;
		cc.min = ARG1(scan);
		cc.max  = ARG2(scan);
		cc.scan = NEXTOPER(scan) + EXTRA_STEP_2ARGS;
		cc.next = next;
		cc.minmod = minmod;
		cc.lastloc = 0;
		PL_reginput = locinput;
		n = regmatch(PREVOPER(next));	/* start on the WHILEM */
		regcpblow(cp);
		PL_regcc = cc.oldcc;
		saySAME(n);
	    }
	    /* NOT REACHED */
	case WHILEM: {
		/*
		 * This is really hard to understand, because after we match
		 * what we're trying to match, we must make sure the rest of
		 * the REx is going to match for sure, and to do that we have
		 * to go back UP the parse tree by recursing ever deeper.  And
		 * if it fails, we have to reset our parent's current state
		 * that we can try again after backing off.
		 */

		CHECKPOINT cp, lastcp;
		CURCUR* cc = PL_regcc;
		char *lastloc = cc->lastloc; /* Detection of 0-len. */
		
		n = cc->cur + 1;	/* how many we know we matched */
		PL_reginput = locinput;

		DEBUG_r(
		    PerlIO_printf(Perl_debug_log, 
				  "%*s  %ld out of %ld..%ld  cc=%lx\n", 
				  REPORT_CODE_OFF+PL_regindent*2, "",
				  (long)n, (long)cc->min, 
				  (long)cc->max, (long)cc)
		    );

		/* If degenerate scan matches "", assume scan done. */

		if (locinput == cc->lastloc && n >= cc->min) {
		    PL_regcc = cc->oldcc;
		    if (PL_regcc)
			ln = PL_regcc->cur;
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
			   "%*s  empty match detected, try continuation...\n",
			   REPORT_CODE_OFF+PL_regindent*2, "")
			);
		    if (regmatch(cc->next))
			sayYES;
		    if (PL_regcc)
			PL_regcc->cur = ln;
		    PL_regcc = cc;
		    sayNO;
		}

		/* First just match a string of min scans. */

		if (n < cc->min) {
		    cc->cur = n;
		    cc->lastloc = locinput;
		    if (regmatch(cc->scan))
			sayYES;
		    cc->cur = n - 1;
		    cc->lastloc = lastloc;
		    sayNO;
		}

		if (scan->flags) {
		    /* Check whether we already were at this position.
			Postpone detection until we know the match is not
			*that* much linear. */
		if (!PL_reg_maxiter) {
		    PL_reg_maxiter = (PL_regeol - PL_bostr + 1) * (scan->flags>>4);
		    PL_reg_leftiter = PL_reg_maxiter;
		}
		if (PL_reg_leftiter-- == 0) {
		    I32 size = (PL_reg_maxiter + 7)/8;
		    if (PL_reg_poscache) {
			if (PL_reg_poscache_size < size) {
			    Renew(PL_reg_poscache, size, char);
			    PL_reg_poscache_size = size;
			}
			Zero(PL_reg_poscache, size, char);
		    }
		    else {
			PL_reg_poscache_size = size;
			Newz(29, PL_reg_poscache, size, char);
		    }
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
	      "%sDetected a super-linear match, switching on caching%s...\n",
				      PL_colors[4], PL_colors[5])
			);
		}
		if (PL_reg_leftiter < 0) {
		    I32 o = locinput - PL_bostr, b;

		    o = (scan->flags & 0xf) - 1 + o * (scan->flags>>4);
		    b = o % 8;
		    o /= 8;
		    if (PL_reg_poscache[o] & (1<<b)) {
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
				      "%*s  already tried at this position...\n",
				      REPORT_CODE_OFF+PL_regindent*2, "")
			);
			sayNO_SILENT;
		    }
		    PL_reg_poscache[o] |= (1<<b);
		}
		}

		/* Prefer next over scan for minimal matching. */

		if (cc->minmod) {
		    PL_regcc = cc->oldcc;
		    if (PL_regcc)
			ln = PL_regcc->cur;
		    cp = regcppush(cc->parenfloor);
		    REGCP_SET(lastcp);
		    if (regmatch(cc->next)) {
			regcpblow(cp);
			sayYES;	/* All done. */
		    }
		    REGCP_UNWIND(lastcp);
		    regcppop();
		    if (PL_regcc)
			PL_regcc->cur = ln;
		    PL_regcc = cc;

		    if (n >= cc->max) {	/* Maximum greed exceeded? */
			if (ckWARN(WARN_REGEXP) && n >= REG_INFTY 
			    && !(PL_reg_flags & RF_warned)) {
			    PL_reg_flags |= RF_warned;
			    Perl_warner(aTHX_ WARN_REGEXP, "%s limit (%d) exceeded",
				 "Complex regular subexpression recursion",
				 REG_INFTY - 1);
			}
			sayNO;
		    }

		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
				      "%*s  trying longer...\n",
				      REPORT_CODE_OFF+PL_regindent*2, "")
			);
		    /* Try scanning more and see if it helps. */
		    PL_reginput = locinput;
		    cc->cur = n;
		    cc->lastloc = locinput;
		    cp = regcppush(cc->parenfloor);
		    REGCP_SET(lastcp);
		    if (regmatch(cc->scan)) {
			regcpblow(cp);
			sayYES;
		    }
		    REGCP_UNWIND(lastcp);
		    regcppop();
		    cc->cur = n - 1;
		    cc->lastloc = lastloc;
		    sayNO;
		}

		/* Prefer scan over next for maximal matching. */

		if (n < cc->max) {	/* More greed allowed? */
		    cp = regcppush(cc->parenfloor);
		    cc->cur = n;
		    cc->lastloc = locinput;
		    REGCP_SET(lastcp);
		    if (regmatch(cc->scan)) {
			regcpblow(cp);
			sayYES;
		    }
		    REGCP_UNWIND(lastcp);
		    regcppop();		/* Restore some previous $<digit>s? */
		    PL_reginput = locinput;
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
				      "%*s  failed, try continuation...\n",
				      REPORT_CODE_OFF+PL_regindent*2, "")
			);
		}
		if (ckWARN(WARN_REGEXP) && n >= REG_INFTY 
			&& !(PL_reg_flags & RF_warned)) {
		    PL_reg_flags |= RF_warned;
		    Perl_warner(aTHX_ WARN_REGEXP, "%s limit (%d) exceeded",
			 "Complex regular subexpression recursion",
			 REG_INFTY - 1);
		}

		/* Failed deeper matches of scan, so see if this one works. */
		PL_regcc = cc->oldcc;
		if (PL_regcc)
		    ln = PL_regcc->cur;
		if (regmatch(cc->next))
		    sayYES;
		if (PL_regcc)
		    PL_regcc->cur = ln;
		PL_regcc = cc;
		cc->cur = n - 1;
		cc->lastloc = lastloc;
		sayNO;
	    }
	    /* NOT REACHED */
	case BRANCHJ: 
	    next = scan + ARG(scan);
	    if (next == scan)
		next = NULL;
	    inner = NEXTOPER(NEXTOPER(scan));
	    goto do_branch;
	case BRANCH: 
	    inner = NEXTOPER(scan);
	  do_branch:
	    {
		CHECKPOINT lastcp;
		c1 = OP(scan);
		if (OP(next) != c1)	/* No choice. */
		    next = inner;	/* Avoid recursion. */
		else {
		    I32 lastparen = *PL_reglastparen;
		    I32 unwind1;
		    re_unwind_branch_t *uw;

		    /* Put unwinding data on stack */
		    unwind1 = SSNEWt(1,re_unwind_branch_t);
		    uw = SSPTRt(unwind1,re_unwind_branch_t);
		    uw->prev = unwind;
		    unwind = unwind1;
		    uw->type = ((c1 == BRANCH)
				? RE_UNWIND_BRANCH
				: RE_UNWIND_BRANCHJ);
		    uw->lastparen = lastparen;
		    uw->next = next;
		    uw->locinput = locinput;
		    uw->nextchr = nextchr;
#ifdef DEBUGGING
		    uw->regindent = ++PL_regindent;
#endif

		    REGCP_SET(uw->lastcp);

		    /* Now go into the first branch */
		    next = inner;
		}
	    }
	    break;
	case MINMOD:
	    minmod = 1;
	    break;
	case CURLYM:
	{
	    I32 l = 0;
	    CHECKPOINT lastcp;
	    
	    /* We suppose that the next guy does not need
	       backtracking: in particular, it is of constant length,
	       and has no parenths to influence future backrefs. */
	    ln = ARG1(scan);  /* min to match */
	    n  = ARG2(scan);  /* max to match */
	    paren = scan->flags;
	    if (paren) {
		if (paren > PL_regsize)
		    PL_regsize = paren;
		if (paren > *PL_reglastparen)
		    *PL_reglastparen = paren;
	    }
	    scan = NEXTOPER(scan) + NODE_STEP_REGNODE;
	    if (paren)
		scan += NEXT_OFF(scan); /* Skip former OPEN. */
	    PL_reginput = locinput;
	    if (minmod) {
		minmod = 0;
		if (ln && regrepeat_hard(scan, ln, &l) < ln)
		    sayNO;
		if (ln && l == 0 && n >= ln
		    /* In fact, this is tricky.  If paren, then the
		       fact that we did/didnot match may influence
		       future execution. */
		    && !(paren && ln == 0))
		    ln = n;
		locinput = PL_reginput;
		if (PL_regkind[(U8)OP(next)] == EXACT) {
		    c1 = (U8)*STRING(next);
		    if (OP(next) == EXACTF)
			c2 = PL_fold[c1];
		    else if (OP(next) == EXACTFL)
			c2 = PL_fold_locale[c1];
		    else
			c2 = c1;
		}
		else
		    c1 = c2 = -1000;
		REGCP_SET(lastcp);
		/* This may be improved if l == 0.  */
		while (n >= ln || (n == REG_INFTY && ln > 0 && l)) { /* ln overflow ? */
		    /* If it could work, try it. */
		    if (c1 == -1000 ||
			UCHARAT(PL_reginput) == c1 ||
			UCHARAT(PL_reginput) == c2)
		    {
			if (paren) {
			    if (n) {
				PL_regstartp[paren] =
				    HOPc(PL_reginput, -l) - PL_bostr;
				PL_regendp[paren] = PL_reginput - PL_bostr;
			    }
			    else
				PL_regendp[paren] = -1;
			}
			if (regmatch(next))
			    sayYES;
			REGCP_UNWIND(lastcp);
		    }
		    /* Couldn't or didn't -- move forward. */
		    PL_reginput = locinput;
		    if (regrepeat_hard(scan, 1, &l)) {
			ln++;
			locinput = PL_reginput;
		    }
		    else
			sayNO;
		}
	    }
	    else {
		n = regrepeat_hard(scan, n, &l);
		if (n != 0 && l == 0
		    /* In fact, this is tricky.  If paren, then the
		       fact that we did/didnot match may influence
		       future execution. */
		    && !(paren && ln == 0))
		    ln = n;
		locinput = PL_reginput;
		DEBUG_r(
		    PerlIO_printf(Perl_debug_log,
				  "%*s  matched %"IVdf" times, len=%"IVdf"...\n",
				  (int)(REPORT_CODE_OFF+PL_regindent*2), "",
				  (IV) n, (IV)l)
		    );
		if (n >= ln) {
		    if (PL_regkind[(U8)OP(next)] == EXACT) {
			c1 = (U8)*STRING(next);
			if (OP(next) == EXACTF)
			    c2 = PL_fold[c1];
			else if (OP(next) == EXACTFL)
			    c2 = PL_fold_locale[c1];
			else
			    c2 = c1;
		    }
		    else
			c1 = c2 = -1000;
		}
		REGCP_SET(lastcp);
		while (n >= ln) {
		    /* If it could work, try it. */
		    if (c1 == -1000 ||
			UCHARAT(PL_reginput) == c1 ||
			UCHARAT(PL_reginput) == c2)
		    {
			DEBUG_r(
				PerlIO_printf(Perl_debug_log,
					      "%*s  trying tail with n=%"IVdf"...\n",
					      (int)(REPORT_CODE_OFF+PL_regindent*2), "", (IV)n)
			    );
			if (paren) {
			    if (n) {
				PL_regstartp[paren] = HOPc(PL_reginput, -l) - PL_bostr;
				PL_regendp[paren] = PL_reginput - PL_bostr;
			    }
			    else
				PL_regendp[paren] = -1;
			}
			if (regmatch(next))
			    sayYES;
			REGCP_UNWIND(lastcp);
		    }
		    /* Couldn't or didn't -- back up. */
		    n--;
		    locinput = HOPc(locinput, -l);
		    PL_reginput = locinput;
		}
	    }
	    sayNO;
	    break;
	}
	case CURLYN:
	    paren = scan->flags;	/* Which paren to set */
	    if (paren > PL_regsize)
		PL_regsize = paren;
	    if (paren > *PL_reglastparen)
		*PL_reglastparen = paren;
	    ln = ARG1(scan);  /* min to match */
	    n  = ARG2(scan);  /* max to match */
            scan = regnext(NEXTOPER(scan) + NODE_STEP_REGNODE);
	    goto repeat;
	case CURLY:
	    paren = 0;
	    ln = ARG1(scan);  /* min to match */
	    n  = ARG2(scan);  /* max to match */
	    scan = NEXTOPER(scan) + NODE_STEP_REGNODE;
	    goto repeat;
	case STAR:
	    ln = 0;
	    n = REG_INFTY;
	    scan = NEXTOPER(scan);
	    paren = 0;
	    goto repeat;
	case PLUS:
	    ln = 1;
	    n = REG_INFTY;
	    scan = NEXTOPER(scan);
	    paren = 0;
	  repeat:
	    /*
	    * Lookahead to avoid useless match attempts
	    * when we know what character comes next.
	    */
	    if (PL_regkind[(U8)OP(next)] == EXACT) {
		c1 = (U8)*STRING(next);
		if (OP(next) == EXACTF)
		    c2 = PL_fold[c1];
		else if (OP(next) == EXACTFL)
		    c2 = PL_fold_locale[c1];
		else
		    c2 = c1;
	    }
	    else
		c1 = c2 = -1000;
	    PL_reginput = locinput;
	    if (minmod) {
		CHECKPOINT lastcp;
		minmod = 0;
		if (ln && regrepeat(scan, ln) < ln)
		    sayNO;
		locinput = PL_reginput;
		REGCP_SET(lastcp);
		if (c1 != -1000) {
		    char *e = locinput + n - ln; /* Should not check after this */
		    char *old = locinput;

		    if (e >= PL_regeol || (n == REG_INFTY))
			e = PL_regeol - 1;
		    while (1) {
			/* Find place 'next' could work */
			if (c1 == c2) {
			    while (locinput <= e && *locinput != c1)
				locinput++;
			} else {
			    while (locinput <= e 
				   && *locinput != c1
				   && *locinput != c2)
				locinput++;			    
			}
			if (locinput > e) 
			    sayNO;
			/* PL_reginput == old now */
			if (locinput != old) {
			    ln = 1;	/* Did some */
			    if (regrepeat(scan, locinput - old) <
				 locinput - old)
				sayNO;
			}
			/* PL_reginput == locinput now */
			TRYPAREN(paren, ln, locinput);
			PL_reginput = locinput;	/* Could be reset... */
			REGCP_UNWIND(lastcp);
			/* Couldn't or didn't -- move forward. */
			old = locinput++;
		    }
		}
		else
		while (n >= ln || (n == REG_INFTY && ln > 0)) { /* ln overflow ? */
		    /* If it could work, try it. */
		    if (c1 == -1000 ||
			UCHARAT(PL_reginput) == c1 ||
			UCHARAT(PL_reginput) == c2)
		    {
			TRYPAREN(paren, n, PL_reginput);
			REGCP_UNWIND(lastcp);
		    }
		    /* Couldn't or didn't -- move forward. */
		    PL_reginput = locinput;
		    if (regrepeat(scan, 1)) {
			ln++;
			locinput = PL_reginput;
		    }
		    else
			sayNO;
		}
	    }
	    else {
		CHECKPOINT lastcp;
		n = regrepeat(scan, n);
		locinput = PL_reginput;
		if (ln < n && PL_regkind[(U8)OP(next)] == EOL &&
		    (!PL_multiline  || OP(next) == SEOL || OP(next) == EOS)) {
		    ln = n;			/* why back off? */
		    /* ...because $ and \Z can match before *and* after
		       newline at the end.  Consider "\n\n" =~ /\n+\Z\n/.
		       We should back off by one in this case. */
		    if (UCHARAT(PL_reginput - 1) == '\n' && OP(next) != EOS)
			ln--;
		}
		REGCP_SET(lastcp);
		if (paren) {
		    while (n >= ln) {
			/* If it could work, try it. */
			if (c1 == -1000 ||
			    UCHARAT(PL_reginput) == c1 ||
			    UCHARAT(PL_reginput) == c2)
			    {
				TRYPAREN(paren, n, PL_reginput);
				REGCP_UNWIND(lastcp);
			    }
			/* Couldn't or didn't -- back up. */
			n--;
			PL_reginput = locinput = HOPc(locinput, -1);
		    }
		}
		else {
		    while (n >= ln) {
			/* If it could work, try it. */
			if (c1 == -1000 ||
			    UCHARAT(PL_reginput) == c1 ||
			    UCHARAT(PL_reginput) == c2)
			    {
				TRYPAREN(paren, n, PL_reginput);
				REGCP_UNWIND(lastcp);
			    }
			/* Couldn't or didn't -- back up. */
			n--;
			PL_reginput = locinput = HOPc(locinput, -1);
		    }
		}
	    }
	    sayNO;
	    break;
	case END:
	    if (PL_reg_call_cc) {
		re_cc_state *cur_call_cc = PL_reg_call_cc;
		CURCUR *cctmp = PL_regcc;
		regexp *re = PL_reg_re;
		CHECKPOINT cp, lastcp;
		
		cp = regcppush(0);	/* Save *all* the positions. */
		REGCP_SET(lastcp);
		regcp_set_to(PL_reg_call_cc->ss); /* Restore parens of
						    the caller. */
		PL_reginput = locinput;	/* Make position available to
					   the callcc. */
		cache_re(PL_reg_call_cc->re);
		PL_regcc = PL_reg_call_cc->cc;
		PL_reg_call_cc = PL_reg_call_cc->prev;
		if (regmatch(cur_call_cc->node)) {
		    PL_reg_call_cc = cur_call_cc;
		    regcpblow(cp);
		    sayYES;
		}
		REGCP_UNWIND(lastcp);
		regcppop();
		PL_reg_call_cc = cur_call_cc;
		PL_regcc = cctmp;
		PL_reg_re = re;
		cache_re(re);

		DEBUG_r(
		    PerlIO_printf(Perl_debug_log,
				  "%*s  continuation failed...\n",
				  REPORT_CODE_OFF+PL_regindent*2, "")
		    );
		sayNO_SILENT;
	    }
	    if (locinput < PL_regtill) {
		DEBUG_r(PerlIO_printf(Perl_debug_log,
				      "%sMatch possible, but length=%ld is smaller than requested=%ld, failing!%s\n",
				      PL_colors[4],
				      (long)(locinput - PL_reg_starttry),
				      (long)(PL_regtill - PL_reg_starttry),
				      PL_colors[5]));
		sayNO_FINAL;		/* Cannot match: too short. */
	    }
	    PL_reginput = locinput;	/* put where regtry can find it */
	    sayYES_FINAL;		/* Success! */
	case SUCCEED:
	    PL_reginput = locinput;	/* put where regtry can find it */
	    sayYES_LOUD;		/* Success! */
	case SUSPEND:
	    n = 1;
	    PL_reginput = locinput;
	    goto do_ifmatch;	    
	case UNLESSM:
	    n = 0;
	    if (scan->flags) {
		if (UTF) {		/* XXXX This is absolutely
					   broken, we read before
					   start of string. */
		    s = HOPMAYBEc(locinput, -scan->flags);
		    if (!s)
			goto say_yes;
		    PL_reginput = s;
		}
		else {
		    if (locinput < PL_bostr + scan->flags) 
			goto say_yes;
		    PL_reginput = locinput - scan->flags;
		    goto do_ifmatch;
		}
	    }
	    else
		PL_reginput = locinput;
	    goto do_ifmatch;
	case IFMATCH:
	    n = 1;
	    if (scan->flags) {
		if (UTF) {		/* XXXX This is absolutely
					   broken, we read before
					   start of string. */
		    s = HOPMAYBEc(locinput, -scan->flags);
		    if (!s || s < PL_bostr)
			goto say_no;
		    PL_reginput = s;
		}
		else {
		    if (locinput < PL_bostr + scan->flags) 
			goto say_no;
		    PL_reginput = locinput - scan->flags;
		    goto do_ifmatch;
		}
	    }
	    else
		PL_reginput = locinput;

	  do_ifmatch:
	    inner = NEXTOPER(NEXTOPER(scan));
	    if (regmatch(inner) != n) {
	      say_no:
		if (logical) {
		    logical = 0;
		    sw = 0;
		    goto do_longjump;
		}
		else
		    sayNO;
	    }
	  say_yes:
	    if (logical) {
		logical = 0;
		sw = 1;
	    }
	    if (OP(scan) == SUSPEND) {
		locinput = PL_reginput;
		nextchr = UCHARAT(locinput);
	    }
	    /* FALL THROUGH. */
	case LONGJMP:
	  do_longjump:
	    next = scan + ARG(scan);
	    if (next == scan)
		next = NULL;
	    break;
	default:
	    PerlIO_printf(Perl_error_log, "%"UVxf" %d\n",
			  PTR2UV(scan), OP(scan));
	    Perl_croak(aTHX_ "regexp memory corruption");
	}
      reenter:
	scan = next;
    }

    /*
    * We get here only if there's trouble -- normally "case END" is
    * the terminating point.
    */
    Perl_croak(aTHX_ "corrupted regexp pointers");
    /*NOTREACHED*/
    sayNO;

yes_loud:
    DEBUG_r(
	PerlIO_printf(Perl_debug_log,
		      "%*s  %scould match...%s\n",
		      REPORT_CODE_OFF+PL_regindent*2, "", PL_colors[4],PL_colors[5])
	);
    goto yes;
yes_final:
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%sMatch successful!%s\n",
			  PL_colors[4],PL_colors[5]));
yes:
#ifdef DEBUGGING
    PL_regindent--;
#endif

#if 0					/* Breaks $^R */
    if (unwind)
	regcpblow(firstcp);
#endif
    return 1;

no:
    DEBUG_r(
	PerlIO_printf(Perl_debug_log,
		      "%*s  %sfailed...%s\n",
		      REPORT_CODE_OFF+PL_regindent*2, "",PL_colors[4],PL_colors[5])
	);
    goto do_no;
no_final:
do_no:
    if (unwind) {
	re_unwind_t *uw = SSPTRt(unwind,re_unwind_t);

	switch (uw->type) {
	case RE_UNWIND_BRANCH:
	case RE_UNWIND_BRANCHJ:
	{
	    re_unwind_branch_t *uwb = &(uw->branch);
	    I32 lastparen = uwb->lastparen;
	    
	    REGCP_UNWIND(uwb->lastcp);
	    for (n = *PL_reglastparen; n > lastparen; n--)
		PL_regendp[n] = -1;
	    *PL_reglastparen = n;
	    scan = next = uwb->next;
	    if ( !scan || 
		 OP(scan) != (uwb->type == RE_UNWIND_BRANCH 
			      ? BRANCH : BRANCHJ) ) {		/* Failure */
		unwind = uwb->prev;
#ifdef DEBUGGING
		PL_regindent--;
#endif
		goto do_no;
	    }
	    /* Have more choice yet.  Reuse the same uwb.  */
	    /*SUPPRESS 560*/
	    if ((n = (uwb->type == RE_UNWIND_BRANCH
		      ? NEXT_OFF(next) : ARG(next))))
		next += n;
	    else
		next = NULL;	/* XXXX Needn't unwinding in this case... */
	    uwb->next = next;
	    next = NEXTOPER(scan);
	    if (uwb->type == RE_UNWIND_BRANCHJ)
		next = NEXTOPER(next);
	    locinput = uwb->locinput;
	    nextchr = uwb->nextchr;
#ifdef DEBUGGING
	    PL_regindent = uwb->regindent;
#endif

	    goto reenter;
	}
	/* NOT REACHED */
	default:
	    Perl_croak(aTHX_ "regexp unwind memory corruption");
	}
	/* NOT REACHED */
    }
#ifdef DEBUGGING
    PL_regindent--;
#endif
    return 0;
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
/*
 * [This routine now assumes that it will only match on things of length 1.
 * That was true before, but now we assume scan - reginput is the count,
 * rather than incrementing count on every character.  [Er, except utf8.]]
 */
STATIC I32
S_regrepeat(pTHX_ regnode *p, I32 max)
{
    register char *scan;
    register I32 c;
    register char *loceol = PL_regeol;
    register I32 hardcount = 0;

    scan = PL_reginput;
    if (max != REG_INFTY && max < loceol - scan)
      loceol = scan + max;
    switch (OP(p)) {
    case REG_ANY:
	while (scan < loceol && *scan != '\n')
	    scan++;
	break;
    case SANY:
	scan = loceol;
	break;
    case ANYUTF8:
	loceol = PL_regeol;
	while (scan < loceol && *scan != '\n') {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case SANYUTF8:
	loceol = PL_regeol;
	while (scan < loceol) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case EXACT:		/* length of string is 1 */
	c = (U8)*STRING(p);
	while (scan < loceol && UCHARAT(scan) == c)
	    scan++;
	break;
    case EXACTF:	/* length of string is 1 */
	c = (U8)*STRING(p);
	while (scan < loceol &&
	       (UCHARAT(scan) == c || UCHARAT(scan) == PL_fold[c]))
	    scan++;
	break;
    case EXACTFL:	/* length of string is 1 */
	PL_reg_flags |= RF_tainted;
	c = (U8)*STRING(p);
	while (scan < loceol &&
	       (UCHARAT(scan) == c || UCHARAT(scan) == PL_fold_locale[c]))
	    scan++;
	break;
    case ANYOFUTF8:
	loceol = PL_regeol;
	while (scan < loceol && REGINCLASSUTF8(p, (U8*)scan)) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case ANYOF:
	while (scan < loceol && REGINCLASS(p, *scan))
	    scan++;
	break;
    case ALNUM:
	while (scan < loceol && isALNUM(*scan))
	    scan++;
	break;
    case ALNUMUTF8:
	loceol = PL_regeol;
	while (scan < loceol && swash_fetch(PL_utf8_alnum, (U8*)scan)) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case ALNUML:
	PL_reg_flags |= RF_tainted;
	while (scan < loceol && isALNUM_LC(*scan))
	    scan++;
	break;
    case ALNUMLUTF8:
	PL_reg_flags |= RF_tainted;
	loceol = PL_regeol;
	while (scan < loceol && isALNUM_LC_utf8((U8*)scan)) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
	break;
    case NALNUM:
	while (scan < loceol && !isALNUM(*scan))
	    scan++;
	break;
    case NALNUMUTF8:
	loceol = PL_regeol;
	while (scan < loceol && !swash_fetch(PL_utf8_alnum, (U8*)scan)) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case NALNUML:
	PL_reg_flags |= RF_tainted;
	while (scan < loceol && !isALNUM_LC(*scan))
	    scan++;
	break;
    case NALNUMLUTF8:
	PL_reg_flags |= RF_tainted;
	loceol = PL_regeol;
	while (scan < loceol && !isALNUM_LC_utf8((U8*)scan)) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case SPACE:
	while (scan < loceol && isSPACE(*scan))
	    scan++;
	break;
    case SPACEUTF8:
	loceol = PL_regeol;
	while (scan < loceol && (*scan == ' ' || swash_fetch(PL_utf8_space,(U8*)scan))) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case SPACEL:
	PL_reg_flags |= RF_tainted;
	while (scan < loceol && isSPACE_LC(*scan))
	    scan++;
	break;
    case SPACELUTF8:
	PL_reg_flags |= RF_tainted;
	loceol = PL_regeol;
	while (scan < loceol && (*scan == ' ' || isSPACE_LC_utf8((U8*)scan))) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case NSPACE:
	while (scan < loceol && !isSPACE(*scan))
	    scan++;
	break;
    case NSPACEUTF8:
	loceol = PL_regeol;
	while (scan < loceol && !(*scan == ' ' || swash_fetch(PL_utf8_space,(U8*)scan))) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case NSPACEL:
	PL_reg_flags |= RF_tainted;
	while (scan < loceol && !isSPACE_LC(*scan))
	    scan++;
	break;
    case NSPACELUTF8:
	PL_reg_flags |= RF_tainted;
	loceol = PL_regeol;
	while (scan < loceol && !(*scan == ' ' || isSPACE_LC_utf8((U8*)scan))) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case DIGIT:
	while (scan < loceol && isDIGIT(*scan))
	    scan++;
	break;
    case DIGITUTF8:
	loceol = PL_regeol;
	while (scan < loceol && swash_fetch(PL_utf8_digit,(U8*)scan)) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
	break;
    case NDIGIT:
	while (scan < loceol && !isDIGIT(*scan))
	    scan++;
	break;
    case NDIGITUTF8:
	loceol = PL_regeol;
	while (scan < loceol && !swash_fetch(PL_utf8_digit,(U8*)scan)) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    default:		/* Called on something of 0 width. */
	break;		/* So match right here or not at all. */
    }

    if (hardcount)
	c = hardcount;
    else
	c = scan - PL_reginput;
    PL_reginput = scan;

    DEBUG_r( 
	{
		SV *prop = sv_newmortal();

		regprop(prop, p);
		PerlIO_printf(Perl_debug_log, 
			      "%*s  %s can match %"IVdf" times out of %"IVdf"...\n", 
			      REPORT_CODE_OFF+1, "", SvPVX(prop),(IV)c,(IV)max);
	});
    
    return(c);
}

/*
 - regrepeat_hard - repeatedly match something, report total lenth and length
 * 
 * The repeater is supposed to have constant length.
 */

STATIC I32
S_regrepeat_hard(pTHX_ regnode *p, I32 max, I32 *lp)
{
    register char *scan;
    register char *start;
    register char *loceol = PL_regeol;
    I32 l = 0;
    I32 count = 0, res = 1;

    if (!max)
	return 0;

    start = PL_reginput;
    if (UTF) {
	while (PL_reginput < loceol && (scan = PL_reginput, res = regmatch(p))) {
	    if (!count++) {
		l = 0;
		while (start < PL_reginput) {
		    l++;
		    start += UTF8SKIP(start);
		}
		*lp = l;
		if (l == 0)
		    return max;
	    }
	    if (count == max)
		return count;
	}
    }
    else {
	while (PL_reginput < loceol && (scan = PL_reginput, res = regmatch(p))) {
	    if (!count++) {
		*lp = l = PL_reginput - start;
		if (max != REG_INFTY && l*max < loceol - scan)
		    loceol = scan + l*max;
		if (l == 0)
		    return max;
	    }
	}
    }
    if (!res)
	PL_reginput = scan;
    
    return count;
}

/*
 - reginclass - determine if a character falls into a character class
 */

STATIC bool
S_reginclass(pTHX_ register regnode *p, register I32 c)
{
    char flags = ANYOF_FLAGS(p);
    bool match = FALSE;

    c &= 0xFF;
    if (ANYOF_BITMAP_TEST(p, c))
	match = TRUE;
    else if (flags & ANYOF_FOLD) {
	I32 cf;
	if (flags & ANYOF_LOCALE) {
	    PL_reg_flags |= RF_tainted;
	    cf = PL_fold_locale[c];
	}
	else
	    cf = PL_fold[c];
	if (ANYOF_BITMAP_TEST(p, cf))
	    match = TRUE;
    }

    if (!match && (flags & ANYOF_CLASS)) {
	PL_reg_flags |= RF_tainted;
	if (
	    (ANYOF_CLASS_TEST(p, ANYOF_ALNUM)   &&  isALNUM_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NALNUM)  && !isALNUM_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_SPACE)   &&  isSPACE_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NSPACE)  && !isSPACE_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_DIGIT)   &&  isDIGIT_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NDIGIT)  && !isDIGIT_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_ALNUMC)  &&  isALNUMC_LC(c)) ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NALNUMC) && !isALNUMC_LC(c)) ||
	    (ANYOF_CLASS_TEST(p, ANYOF_ALPHA)   &&  isALPHA_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NALPHA)  && !isALPHA_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_ASCII)   &&  isASCII(c))     ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NASCII)  && !isASCII(c))     ||
	    (ANYOF_CLASS_TEST(p, ANYOF_CNTRL)   &&  isCNTRL_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NCNTRL)  && !isCNTRL_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_GRAPH)   &&  isGRAPH_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NGRAPH)  && !isGRAPH_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_LOWER)   &&  isLOWER_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NLOWER)  && !isLOWER_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_PRINT)   &&  isPRINT_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NPRINT)  && !isPRINT_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_PUNCT)   &&  isPUNCT_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NPUNCT)  && !isPUNCT_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_UPPER)   &&  isUPPER_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NUPPER)  && !isUPPER_LC(c))  ||
	    (ANYOF_CLASS_TEST(p, ANYOF_XDIGIT)  &&  isXDIGIT(c))    ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NXDIGIT) && !isXDIGIT(c))    ||
	    (ANYOF_CLASS_TEST(p, ANYOF_PSXSPC)  &&  isPSXSPC(c))    ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NPSXSPC) && !isPSXSPC(c))    ||
	    (ANYOF_CLASS_TEST(p, ANYOF_BLANK)   &&  isBLANK(c))     ||
	    (ANYOF_CLASS_TEST(p, ANYOF_NBLANK)  && !isBLANK(c))
	    ) /* How's that for a conditional? */
	{
	    match = TRUE;
	}
    }

    return (flags & ANYOF_INVERT) ? !match : match;
}

STATIC bool
S_reginclassutf8(pTHX_ regnode *f, U8 *p)
{                                           
    char flags = ARG1(f);
    bool match = FALSE;
#ifdef DEBUGGING
    SV *rv = (SV*)PL_regdata->data[ARG2(f)];
    AV *av = (AV*)SvRV((SV*)rv);
    SV *sw = *av_fetch(av, 0, FALSE);
    SV *lv = *av_fetch(av, 1, FALSE);
#else
    SV *sw = (SV*)PL_regdata->data[ARG2(f)];
#endif

    if (swash_fetch(sw, p))
	match = TRUE;
    else if (flags & ANYOF_FOLD) {
	U8 tmpbuf[UTF8_MAXLEN+1];
	if (flags & ANYOF_LOCALE) {
	    PL_reg_flags |= RF_tainted;
	    uv_to_utf8(tmpbuf, toLOWER_LC_utf8(p));
	}
	else
	    uv_to_utf8(tmpbuf, toLOWER_utf8(p));
	if (swash_fetch(sw, tmpbuf))
	    match = TRUE;
    }

    /* UTF8 combined with ANYOF_CLASS is ill-defined. */

    return (flags & ANYOF_INVERT) ? !match : match;
}

STATIC U8 *
S_reghop(pTHX_ U8 *s, I32 off)
{                               
    if (off >= 0) {
	while (off-- && s < (U8*)PL_regeol)
	    s += UTF8SKIP(s);
    }
    else {
	while (off++) {
	    if (s > (U8*)PL_bostr) {
		s--;
		if (*s & 0x80) {
		    while (s > (U8*)PL_bostr && (*s & 0xc0) == 0x80)
			s--;
		}		/* XXX could check well-formedness here */
	    }
	}
    }
    return s;
}

STATIC U8 *
S_reghopmaybe(pTHX_ U8* s, I32 off)
{
    if (off >= 0) {
	while (off-- && s < (U8*)PL_regeol)
	    s += UTF8SKIP(s);
	if (off >= 0)
	    return 0;
    }
    else {
	while (off++) {
	    if (s > (U8*)PL_bostr) {
		s--;
		if (*s & 0x80) {
		    while (s > (U8*)PL_bostr && (*s & 0xc0) == 0x80)
			s--;
		}		/* XXX could check well-formedness here */
	    }
	    else
		break;
	}
	if (off <= 0)
	    return 0;
    }
    return s;
}

#ifdef PERL_OBJECT
#include "XSUB.h"
#endif

static void
restore_pos(pTHXo_ void *arg)
{
    if (PL_reg_eval_set) {
	if (PL_reg_oldsaved) {
	    PL_reg_re->subbeg = PL_reg_oldsaved;
	    PL_reg_re->sublen = PL_reg_oldsavedlen;
	    RX_MATCH_COPIED_on(PL_reg_re);
	}
	PL_reg_magic->mg_len = PL_reg_oldpos;
	PL_reg_eval_set = 0;
	PL_curpm = PL_reg_oldcurpm;
    }	
}
