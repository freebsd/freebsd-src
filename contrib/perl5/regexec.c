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
#  ifndef DEBUGGING
#    define DEBUGGING
#  endif
#endif

#ifdef PERL_IN_XSUB_RE
/* We *really* need to overwrite these symbols: */
#  define Perl_regexec_flags my_regexec
#  define Perl_regdump my_regdump
#  define Perl_regprop my_regprop
/* *These* symbols are masked to allow static link. */
#  define Perl_pregexec my_pregexec
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
 ****    Copyright (c) 1991-1999, Larry Wall
 ****
 ****    You may distribute under the terms of either the GNU General Public
 ****    License or the Artistic License, as specified in the README file.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */
#include "EXTERN.h"
#include "perl.h"
#include "regcomp.h"

#define RF_tainted	1		/* tainted information used? */
#define RF_warned	2		/* warned about big count? */
#define RF_evaled	4		/* Did an EVAL with setting? */

#define RS_init		1		/* eval environment created */
#define RS_set		2		/* replsv value is set */

#ifndef STATIC
#define	STATIC	static
#endif

#ifndef PERL_OBJECT
typedef I32 CHECKPOINT;

/*
 * Forwards.
 */

static I32 regmatch _((regnode *prog));
static I32 regrepeat _((regnode *p, I32 max));
static I32 regrepeat_hard _((regnode *p, I32 max, I32 *lp));
static I32 regtry _((regexp *prog, char *startpos));

static bool reginclass _((char *p, I32 c));
static CHECKPOINT regcppush _((I32 parenfloor));
static char * regcppop _((void));
#endif
#define REGINCLASS(p,c)  (*(p) ? reginclass(p,c) : ANYOF_TEST(p,c))

STATIC CHECKPOINT
regcppush(I32 parenfloor)
{
    dTHR;
    int retval = PL_savestack_ix;
    int i = (PL_regsize - parenfloor) * 4;
    int p;

    SSCHECK(i + 5);
    for (p = PL_regsize; p > parenfloor; p--) {
	SSPUSHPTR(PL_regendp[p]);
	SSPUSHPTR(PL_regstartp[p]);
	SSPUSHPTR(PL_reg_start_tmp[p]);
	SSPUSHINT(p);
    }
    SSPUSHINT(PL_regsize);
    SSPUSHINT(*PL_reglastparen);
    SSPUSHPTR(PL_reginput);
    SSPUSHINT(i + 3);
    SSPUSHINT(SAVEt_REGCONTEXT);
    return retval;
}

/* These are needed since we do not localize EVAL nodes: */
#  define REGCP_SET  DEBUG_r(PerlIO_printf(Perl_debug_log,		\
			     "  Setting an EVAL scope, savestack=%i\n",	\
			     PL_savestack_ix)); lastcp = PL_savestack_ix

#  define REGCP_UNWIND  DEBUG_r(lastcp != PL_savestack_ix ?		\
				PerlIO_printf(Perl_debug_log,		\
				"  Clearing an EVAL scope, savestack=%i..%i\n", \
				lastcp, PL_savestack_ix) : 0); regcpblow(lastcp)

STATIC char *
regcppop(void)
{
    dTHR;
    I32 i = SSPOPINT;
    U32 paren = 0;
    char *input;
    char *tmps;
    assert(i == SAVEt_REGCONTEXT);
    i = SSPOPINT;
    input = (char *) SSPOPPTR;
    *PL_reglastparen = SSPOPINT;
    PL_regsize = SSPOPINT;
    for (i -= 3; i > 0; i -= 4) {
	paren = (U32)SSPOPINT;
	PL_reg_start_tmp[paren] = (char *) SSPOPPTR;
	PL_regstartp[paren] = (char *) SSPOPPTR;
	tmps = (char*)SSPOPPTR;
	if (paren <= *PL_reglastparen)
	    PL_regendp[paren] = tmps;
	DEBUG_r(
	    PerlIO_printf(Perl_debug_log,
			  "     restoring \\%d to %d(%d)..%d%s\n",
			  paren, PL_regstartp[paren] - PL_regbol, 
			  PL_reg_start_tmp[paren] - PL_regbol,
			  PL_regendp[paren] - PL_regbol, 
			  (paren > *PL_reglastparen ? "(no)" : ""));
	);
    }
    DEBUG_r(
	if (*PL_reglastparen + 1 <= PL_regnpar) {
	    PerlIO_printf(Perl_debug_log,
			  "     restoring \\%d..\\%d to undef\n",
			  *PL_reglastparen + 1, PL_regnpar);
	}
    );
    for (paren = *PL_reglastparen + 1; paren <= PL_regnpar; paren++) {
	if (paren > PL_regsize)
	    PL_regstartp[paren] = Nullch;
	PL_regendp[paren] = Nullch;
    }
    return input;
}

#define regcpblow(cp) LEAVE_SCOPE(cp)

/*
 * pregexec and friends
 */

/*
 - pregexec - match a regexp against a string
 */
I32
pregexec(register regexp *prog, char *stringarg, register char *strend,
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
  
/*
 - regexec_flags - match a regexp against a string
 */
I32
regexec_flags(register regexp *prog, char *stringarg, register char *strend,
	      char *strbeg, I32 minend, SV *screamer, void *data, U32 flags)
/* strend: pointer to null at end of string */
/* strbeg: real beginning of string */
/* minend: end of match must be >=minend after stringarg. */
/* data: May be used for some additional optimizations. */
/* nosave: For optimizations. */
{
    dTHR;
    register char *s;
    register regnode *c;
    register char *startpos = stringarg;
    register I32 tmp;
    I32 minlen;		/* must match at least this many chars */
    I32 dontbother = 0;	/* how many characters not to try at end */
    CURCUR cc;
    I32 start_shift = 0;		/* Offset of the start to find
					 constant substr. */
    I32 end_shift = 0;			/* Same for the end. */
    I32 scream_pos = -1;		/* Internal iterator of scream. */
    char *scream_olds;
    SV* oreplsv = GvSV(PL_replgv);

    cc.cur = 0;
    cc.oldcc = 0;
    PL_regcc = &cc;

    PL_regprecomp = prog->precomp;		/* Needed for error messages. */
#ifdef DEBUGGING
    PL_regnarrate = PL_debug & 512;
    PL_regprogram = prog->program;
#endif

    /* Be paranoid... */
    if (prog == NULL || startpos == NULL) {
	croak("NULL regexp parameter");
	return 0;
    }

    minlen = prog->minlen;
    if (strend - startpos < minlen) goto phooey;

    if (startpos == strbeg)	/* is ^ valid at stringarg? */
	PL_regprev = '\n';
    else {
	PL_regprev = stringarg[-1];
	if (!PL_multiline && PL_regprev == '\n')
	    PL_regprev = '\0';		/* force ^ to NOT match */
    }

    /* Check validity of program. */
    if (UCHARAT(prog->program) != MAGIC) {
	FAIL("corrupted regexp program");
    }

    PL_regnpar = prog->nparens;
    PL_reg_flags = 0;
    PL_reg_eval_set = 0;

    /* If there is a "must appear" string, look for it. */
    s = startpos;
    if (!(flags & REXEC_CHECKED) 
	&& prog->check_substr != Nullsv &&
	!(prog->reganch & ROPT_ANCH_GPOS) &&
	(!(prog->reganch & (ROPT_ANCH_BOL | ROPT_ANCH_MBOL))
	 || (PL_multiline && prog->check_substr == prog->anchored_substr)) )
    {
	start_shift = prog->check_offset_min;
	/* Should be nonnegative! */
	end_shift = minlen - start_shift - SvCUR(prog->check_substr);
	if (screamer) {
	    if (PL_screamfirst[BmRARE(prog->check_substr)] >= 0)
		    s = screaminstr(screamer, prog->check_substr, 
				    start_shift + (stringarg - strbeg),
				    end_shift, &scream_pos, 0);
	    else
		    s = Nullch;
	    scream_olds = s;
	}
	else
	    s = fbm_instr((unsigned char*)s + start_shift,
			  (unsigned char*)strend - end_shift,
		prog->check_substr, 0);
	if (!s) {
	    ++BmUSEFUL(prog->check_substr);	/* hooray */
	    goto phooey;	/* not present */
	} else if ((s - stringarg) > prog->check_offset_max) {
	    ++BmUSEFUL(prog->check_substr);	/* hooray/2 */
	    s -= prog->check_offset_max;
	} else if (!prog->naughty 
		   && --BmUSEFUL(prog->check_substr) < 0
		   && prog->check_substr == prog->float_substr) { /* boo */
	    SvREFCNT_dec(prog->check_substr);
	    prog->check_substr = Nullsv;	/* disable */
	    prog->float_substr = Nullsv;	/* clear */
	    s = startpos;
	} else s = startpos;
    }

    /* Mark beginning of line for ^ and lookbehind. */
    PL_regbol = startpos;
    PL_bostr  = strbeg;

    /* Mark end of line for $ (and such) */
    PL_regeol = strend;

    /* see how far we have to get to not match where we matched before */
    PL_regtill = startpos+minend;

    DEBUG_r(
	PerlIO_printf(Perl_debug_log, 
		      "Matching `%.60s%s' against `%.*s%s'\n",
		      prog->precomp, 
		      (strlen(prog->precomp) > 60 ? "..." : ""),
		      (strend - startpos > 60 ? 60 : strend - startpos),
		      startpos, 
		      (strend - startpos > 60 ? "..." : ""))
	);

    /* Simplest case:  anchored match need be tried only once. */
    /*  [unless only anchor is BOL and multiline is set] */
    if (prog->reganch & ROPT_ANCH) {
	if (regtry(prog, startpos))
	    goto got_it;
	else if (!(prog->reganch & ROPT_ANCH_GPOS) &&
		 (PL_multiline || (prog->reganch & ROPT_IMPLICIT)
		  || (prog->reganch & ROPT_ANCH_MBOL)))
	{
	    if (minlen)
		dontbother = minlen - 1;
	    strend -= dontbother;
	    /* for multiline we only have to try after newlines */
	    if (s > startpos)
		s--;
	    while (s < strend) {
		if (*s++ == '\n') {
		    if (s < strend && regtry(prog, s))
			goto got_it;
		}
	    }
	}
	goto phooey;
    }

    /* Messy cases:  unanchored match. */
    if (prog->anchored_substr && prog->reganch & ROPT_SKIP) { 
	/* we have /x+whatever/ */
	/* it must be a one character string */
	char ch = SvPVX(prog->anchored_substr)[0];
	while (s < strend) {
	    if (*s == ch) {
		if (regtry(prog, s)) goto got_it;
		s++;
		while (s < strend && *s == ch)
		    s++;
	    }
	    s++;
	}
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
	I32 delta = back_max - back_min;
	char *last = strend - SvCUR(must) - back_min; /* Cannot start after this */
	char *last1 = s - 1;		/* Last position checked before */

	/* XXXX check_substr already used to find `s', can optimize if
	   check_substr==must. */
	scream_pos = -1;
	dontbother = end_shift;
	strend -= dontbother;
	while ( (s <= last) &&
		(screamer 
		 ? (s = screaminstr(screamer, must, s + back_min - strbeg,
				    end_shift, &scream_pos, 0))
		 : (s = fbm_instr((unsigned char*)s + back_min,
				  (unsigned char*)strend, must, 0))) ) {
	    if (s - back_max > last1) {
		last1 = s - back_min;
		s = s - back_max;
	    } else {
		char *t = last1 + 1;		

		last1 = s - back_min;
		s = t;		
	    }
	    while (s <= last1) {
		if (regtry(prog, s))
		    goto got_it;
		s++;
	    }
	}
	goto phooey;
    } else if (c = prog->regstclass) {
	I32 doevery = (prog->reganch & ROPT_SKIP) == 0;
	char *Class;

	if (minlen)
	    dontbother = minlen - 1;
	strend -= dontbother;	/* don't bother with what can't match */
	tmp = 1;
	/* We know what class it must start with. */
	switch (OP(c)) {
	case ANYOF:
	    Class = (char *) OPERAND(c);
	    while (s < strend) {
		if (REGINCLASS(Class, *s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case BOUNDL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case BOUND:
	    if (minlen)
		dontbother++,strend--;
	    tmp = (s != startpos) ? UCHARAT(s - 1) : PL_regprev;
	    tmp = ((OP(c) == BOUND ? isALNUM(tmp) : isALNUM_LC(tmp)) != 0);
	    while (s < strend) {
		if (tmp == !(OP(c) == BOUND ? isALNUM(*s) : isALNUM_LC(*s))) {
		    tmp = !tmp;
		    if (regtry(prog, s))
			goto got_it;
		}
		s++;
	    }
	    if ((minlen || tmp) && regtry(prog,s))
		goto got_it;
	    break;
	case NBOUNDL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NBOUND:
	    if (minlen)
		dontbother++,strend--;
	    tmp = (s != startpos) ? UCHARAT(s - 1) : PL_regprev;
	    tmp = ((OP(c) == NBOUND ? isALNUM(tmp) : isALNUM_LC(tmp)) != 0);
	    while (s < strend) {
		if (tmp == !(OP(c) == NBOUND ? isALNUM(*s) : isALNUM_LC(*s)))
		    tmp = !tmp;
		else if (regtry(prog, s))
		    goto got_it;
		s++;
	    }
	    if ((minlen || !tmp) && regtry(prog,s))
		goto got_it;
	    break;
	case ALNUM:
	    while (s < strend) {
		if (isALNUM(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case ALNUML:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (isALNUM_LC(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NALNUM:
	    while (s < strend) {
		if (!isALNUM(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NALNUML:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (!isALNUM_LC(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case SPACE:
	    while (s < strend) {
		if (isSPACE(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case SPACEL:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (isSPACE_LC(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NSPACE:
	    while (s < strend) {
		if (!isSPACE(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NSPACEL:
	    PL_reg_flags |= RF_tainted;
	    while (s < strend) {
		if (!isSPACE_LC(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case DIGIT:
	    while (s < strend) {
		if (isDIGIT(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NDIGIT:
	    while (s < strend) {
		if (!isDIGIT(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	}
    }
    else {
	dontbother = 0;
	if (prog->float_substr != Nullsv) {	/* Trim the end. */
	    char *last;
	    I32 oldpos = scream_pos;

	    if (screamer) {
		last = screaminstr(screamer, prog->float_substr, s - strbeg,
				   end_shift, &scream_pos, 1); /* last one */
		if (!last) {
		    last = scream_olds; /* Only one occurence. */
		}
	    } else {
		STRLEN len;
		char *little = SvPV(prog->float_substr, len);
		if (len) 
		    last = rninstr(s, strend, little, little + len);
		else
		    last = strend;	/* matching `$' */
	    }
	    if (last == NULL) goto phooey; /* Should not happen! */
	    dontbother = strend - last + prog->float_min_offset;
	}
	if (minlen && (dontbother < minlen))
	    dontbother = minlen - 1;
	strend -= dontbother;
	/* We don't know much -- general case. */
	do {
	    if (regtry(prog, s))
		goto got_it;
	} while (s++ < strend);
    }

    /* Failure. */
    goto phooey;

got_it:
    prog->subbeg = strbeg;
    prog->subend = PL_regeol;	/* strend may have been modified */
    RX_MATCH_TAINTED_set(prog, PL_reg_flags & RF_tainted);

    /* make sure $`, $&, $', and $digit will work later */
    if (strbeg != prog->subbase) {	/* second+ //g match.  */
	if (!(flags & REXEC_COPY_STR)) {
	    if (prog->subbase) {
		Safefree(prog->subbase);
		prog->subbase = Nullch;
	    }
	}
	else {
	    I32 i = PL_regeol - startpos + (stringarg - strbeg);
	    s = savepvn(strbeg, i);
	    Safefree(prog->subbase);
	    prog->subbase = s;
	    prog->subbeg = prog->subbase;
	    prog->subend = prog->subbase + i;
	    s = prog->subbase + (stringarg - strbeg);
	    for (i = 0; i <= prog->nparens; i++) {
		if (prog->endp[i]) {
		    prog->startp[i] = s + (prog->startp[i] - startpos);
		    prog->endp[i] = s + (prog->endp[i] - startpos);
		}
	    }
	}
    }
    /* Preserve the current value of $^R */
    if (oreplsv != GvSV(PL_replgv)) {
	sv_setsv(oreplsv, GvSV(PL_replgv));/* So that when GvSV(replgv) is
					   restored, the value remains
					   the same. */
    }
    return 1;

phooey:
    return 0;
}

/*
 - regtry - try match at specific point
 */
STATIC I32			/* 0 failure, 1 success */
regtry(regexp *prog, char *startpos)
{
    dTHR;
    register I32 i;
    register char **sp;
    register char **ep;
    CHECKPOINT lastcp;

    if ((prog->reganch & ROPT_EVAL_SEEN) && !PL_reg_eval_set) {
	PL_reg_eval_set = RS_init;
	DEBUG_r(DEBUG_s(
	    PerlIO_printf(Perl_debug_log, "  setting stack tmpbase at %i\n",
			  PL_stack_sp - PL_stack_base);
	    ));
	SAVEINT(cxstack[cxstack_ix].blk_oldsp);
	cxstack[cxstack_ix].blk_oldsp = PL_stack_sp - PL_stack_base;
	/* Otherwise OP_NEXTSTATE will free whatever on stack now.  */
	SAVETMPS;
	/* Apparently this is not needed, judging by wantarray. */
	/* SAVEINT(cxstack[cxstack_ix].blk_gimme);
	   cxstack[cxstack_ix].blk_gimme = G_SCALAR; */
    }
    PL_reginput = startpos;
    PL_regstartp = prog->startp;
    PL_regendp = prog->endp;
    PL_reglastparen = &prog->lastparen;
    prog->lastparen = 0;
    PL_regsize = 0;
    if (PL_reg_start_tmpl <= prog->nparens) {
	PL_reg_start_tmpl = prog->nparens*3/2 + 3;
        if(PL_reg_start_tmp)
            Renew(PL_reg_start_tmp, PL_reg_start_tmpl, char*);
        else
            New(22,PL_reg_start_tmp, PL_reg_start_tmpl, char*);
    }

    sp = prog->startp;
    ep = prog->endp;
    PL_regdata = prog->data;
    if (prog->nparens) {
	for (i = prog->nparens; i >= 0; i--) {
	    *sp++ = NULL;
	    *ep++ = NULL;
	}
    }
    REGCP_SET;
    if (regmatch(prog->program + 1)) {
	prog->startp[0] = startpos;
	prog->endp[0] = PL_reginput;
	return 1;
    }
    REGCP_UNWIND;
    return 0;
}

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
regmatch(regnode *prog)
{
    dTHR;
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
#ifdef DEBUGGING
    PL_regindent++;
#endif

    nextchr = UCHARAT(locinput);
    scan = prog;
    while (scan != NULL) {
#define sayNO_L (logical ? (logical = 0, sw = 0, goto cont) : sayNO)
#ifdef DEBUGGING
#  define sayYES goto yes
#  define sayNO goto no
#  define saySAME(x) if (x) goto yes; else goto no
#  define REPORT_CODE_OFF 24
#else
#  define sayYES return 1
#  define sayNO return 0
#  define saySAME(x) return x
#endif
	DEBUG_r( {
	    SV *prop = sv_newmortal();
	    int docolor = *PL_colors[0];
	    int taill = (docolor ? 10 : 7); /* 3 chars for "> <" */
	    int l = (PL_regeol - locinput > taill ? taill : PL_regeol - locinput);
	    int pref_len = (locinput - PL_bostr > (5 + taill) - l 
			    ? (5 + taill) - l : locinput - PL_bostr);

	    if (l + pref_len < (5 + taill) && l < PL_regeol - locinput)
		l = ( PL_regeol - locinput > (5 + taill) - pref_len 
		      ? (5 + taill) - pref_len : PL_regeol - locinput);
	    regprop(prop, scan);
	    PerlIO_printf(Perl_debug_log, 
			  "%4i <%s%.*s%s%s%s%.*s%s>%*s|%3d:%*s%s\n",
			  locinput - PL_bostr, 
			  PL_colors[2], pref_len, locinput - pref_len, PL_colors[3],
			  (docolor ? "" : "> <"),
			  PL_colors[0], l, locinput, PL_colors[1],
			  15 - l - pref_len + 1,
			  "",
			  scan - PL_regprogram, PL_regindent*2, "",
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
		/* PL_regtill = PL_regbol; */
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
	    if (locinput == PL_regbol && PL_regprev == '\n')
		break;
	    sayNO;
	case GPOS:
	    if (locinput == PL_regbol)
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
	case SANY:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case ANY:
	    if (!nextchr && locinput >= PL_regeol || nextchr == '\n')
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case EXACT:
	    s = (char *) OPERAND(scan);
	    ln = UCHARAT(s++);
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
	    s = (char *) OPERAND(scan);
	    ln = UCHARAT(s++);
	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchr &&
		UCHARAT(s) != ((OP(scan) == EXACTF)
			       ? fold : fold_locale)[nextchr])
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
	case ANYOF:
	    s = (char *) OPERAND(scan);
	    if (nextchr < 0)
		nextchr = UCHARAT(locinput);
	    if (!REGINCLASS(s, nextchr))
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
	case SPACEL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case SPACE:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    if (!(OP(scan) == SPACE
		  ? isSPACE(nextchr) : isSPACE_LC(nextchr)))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case NSPACEL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
	case NSPACE:
	    if (!nextchr)
		sayNO;
	    if (OP(scan) == SPACE
		? isSPACE(nextchr) : isSPACE_LC(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case DIGIT:
	    if (!isDIGIT(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case NDIGIT:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    if (isDIGIT(nextchr))
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case REFFL:
	    PL_reg_flags |= RF_tainted;
	    /* FALL THROUGH */
        case REF:
	case REFF:
	    n = ARG(scan);  /* which paren pair */
	    s = PL_regstartp[n];
	    if (*PL_reglastparen < n || !s)
		sayNO;			/* Do not match unless seen CLOSEn. */
	    if (s == PL_regendp[n])
		break;
	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchr &&
		(OP(scan) == REF ||
		 (UCHARAT(s) != ((OP(scan) == REFF
				  ? fold : fold_locale)[nextchr]))))
		sayNO;
	    ln = PL_regendp[n] - s;
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
	    DEBUG_r( PerlIO_printf(Perl_debug_log, "  re_eval 0x%x\n", PL_op) );
	    PL_curpad = AvARRAY((AV*)PL_regdata->data[n + 1]);

	    CALLRUNOPS();			/* Scalar context. */
	    SPAGAIN;
	    ret = POPs;
	    PUTBACK;
	    
	    if (logical) {
		logical = 0;
		sw = SvTRUE(ret);
	    } else
		sv_setsv(save_scalar(PL_replgv), ret);
	    PL_op = oop;
	    PL_curpad = ocurpad;
	    PL_curcop = ocurcop;
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
	    PL_regstartp[n] = PL_reg_start_tmp[n];
	    PL_regendp[n] = locinput;
	    if (n > *PL_reglastparen)
		*PL_reglastparen = n;
	    break;
	case GROUPP:
	    n = ARG(scan);  /* which paren pair */
	    sw = (*PL_reglastparen >= n && PL_regendp[n] != NULL);
	    break;
	case IFTHEN:
	    if (sw)
		next = NEXTOPER(NEXTOPER(scan));
	    else {
		next = scan + ARG(scan);
		if (OP(next) == IFTHEN) /* Fake one. */
		    next = NEXTOPER(NEXTOPER(next));
	    }
	    break;
	case LOGICAL:
	    logical = 1;
	    break;
	case CURLYX: {
		CURCUR cc;
		CHECKPOINT cp = PL_savestack_ix;

		if (OP(PREVOPER(next)) == NOTHING) /* LONGJMP */
		    next += ARG(next);
		cc.oldcc = PL_regcc;
		PL_regcc = &cc;
		cc.parenfloor = *PL_reglastparen;
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
		 * the RE is going to match for sure, and to do that we have
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
		    ln = PL_regcc->cur;
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
			   "%*s  empty match detected, try continuation...\n",
			   REPORT_CODE_OFF+PL_regindent*2, "")
			);
		    if (regmatch(cc->next))
			sayYES;
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
				      "%*s  failed...\n",
				      REPORT_CODE_OFF+PL_regindent*2, "")
			);
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
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
				      "%*s  failed...\n",
				      REPORT_CODE_OFF+PL_regindent*2, "")
			);
		    sayNO;
		}

		/* Prefer next over scan for minimal matching. */

		if (cc->minmod) {
		    PL_regcc = cc->oldcc;
		    ln = PL_regcc->cur;
		    cp = regcppush(cc->parenfloor);
		    REGCP_SET;
		    if (regmatch(cc->next)) {
			regcpblow(cp);
			sayYES;	/* All done. */
		    }
		    REGCP_UNWIND;
		    regcppop();
		    PL_regcc->cur = ln;
		    PL_regcc = cc;

		    if (n >= cc->max) {	/* Maximum greed exceeded? */
			if (PL_dowarn && n >= REG_INFTY 
			    && !(PL_reg_flags & RF_warned)) {
			    PL_reg_flags |= RF_warned;
			    warn("%s limit (%d) exceeded",
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
		    REGCP_SET;
		    if (regmatch(cc->scan)) {
			regcpblow(cp);
			sayYES;
		    }
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
				      "%*s  failed...\n",
				      REPORT_CODE_OFF+PL_regindent*2, "")
			);
		    REGCP_UNWIND;
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
		    REGCP_SET;
		    if (regmatch(cc->scan)) {
			regcpblow(cp);
			sayYES;
		    }
		    REGCP_UNWIND;
		    regcppop();		/* Restore some previous $<digit>s? */
		    PL_reginput = locinput;
		    DEBUG_r(
			PerlIO_printf(Perl_debug_log,
				      "%*s  failed, try continuation...\n",
				      REPORT_CODE_OFF+PL_regindent*2, "")
			);
		}
		if (PL_dowarn && n >= REG_INFTY && !(PL_reg_flags & RF_warned)) {
		    PL_reg_flags |= RF_warned;
		    warn("%s limit (%d) exceeded",
			 "Complex regular subexpression recursion",
			 REG_INFTY - 1);
		}

		/* Failed deeper matches of scan, so see if this one works. */
		PL_regcc = cc->oldcc;
		ln = PL_regcc->cur;
		if (regmatch(cc->next))
		    sayYES;
		DEBUG_r(
		    PerlIO_printf(Perl_debug_log, "%*s  failed...\n",
				  REPORT_CODE_OFF+PL_regindent*2, "")
		    );
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
		    int lastparen = *PL_reglastparen;

		    REGCP_SET;
		    do {
			PL_reginput = locinput;
			if (regmatch(inner))
			    sayYES;
			REGCP_UNWIND;
			for (n = *PL_reglastparen; n > lastparen; n--)
			    PL_regendp[n] = 0;
			*PL_reglastparen = n;
			scan = next;
			/*SUPPRESS 560*/
			if (n = (c1 == BRANCH ? NEXT_OFF(next) : ARG(next)))
			    next += n;
			else
			    next = NULL;
			inner = NEXTOPER(scan);
			if (c1 == BRANCHJ) {
			    inner = NEXTOPER(inner);
			}
		    } while (scan != NULL && OP(scan) == c1);
		    sayNO;
		    /* NOTREACHED */
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
		if (regkind[(U8)OP(next)] == EXACT) {
		    c1 = UCHARAT(OPERAND(next) + 1);
		    if (OP(next) == EXACTF)
			c2 = fold[c1];
		    else if (OP(next) == EXACTFL)
			c2 = fold_locale[c1];
		    else
			c2 = c1;
		} else
		    c1 = c2 = -1000;
		REGCP_SET;
		/* This may be improved if l == 0.  */
		while (n >= ln || (n == REG_INFTY && ln > 0 && l)) { /* ln overflow ? */
		    /* If it could work, try it. */
		    if (c1 == -1000 ||
			UCHARAT(PL_reginput) == c1 ||
			UCHARAT(PL_reginput) == c2)
		    {
			if (paren) {
			    if (n) {
				PL_regstartp[paren] = PL_reginput - l;
				PL_regendp[paren] = PL_reginput;
			    } else
				PL_regendp[paren] = NULL;
			}
			if (regmatch(next))
			    sayYES;
			REGCP_UNWIND;
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
	    } else {
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
				  "%*s  matched %ld times, len=%ld...\n",
				  REPORT_CODE_OFF+PL_regindent*2, "", n, l)
		    );
		if (n >= ln) {
		    if (regkind[(U8)OP(next)] == EXACT) {
			c1 = UCHARAT(OPERAND(next) + 1);
			if (OP(next) == EXACTF)
			    c2 = fold[c1];
			else if (OP(next) == EXACTFL)
			    c2 = fold_locale[c1];
			else
			    c2 = c1;
		    } else
			c1 = c2 = -1000;
		}
		REGCP_SET;
		while (n >= ln) {
		    /* If it could work, try it. */
		    if (c1 == -1000 ||
			UCHARAT(PL_reginput) == c1 ||
			UCHARAT(PL_reginput) == c2)
			{
			    DEBUG_r(
				PerlIO_printf(Perl_debug_log,
					      "%*s  trying tail with n=%ld...\n",
					      REPORT_CODE_OFF+PL_regindent*2, "", n)
				);
			    if (paren) {
				if (n) {
				    PL_regstartp[paren] = PL_reginput - l;
				    PL_regendp[paren] = PL_reginput;
				} else
				    PL_regendp[paren] = NULL;
			    }
			    if (regmatch(next))
				sayYES;
			    REGCP_UNWIND;
			}
		    /* Couldn't or didn't -- back up. */
		    n--;
		    locinput -= l;
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
	    if (regkind[(U8)OP(next)] == EXACT) {
		c1 = UCHARAT(OPERAND(next) + 1);
		if (OP(next) == EXACTF)
		    c2 = fold[c1];
		else if (OP(next) == EXACTFL)
		    c2 = fold_locale[c1];
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
		REGCP_SET;
		while (n >= ln || (n == REG_INFTY && ln > 0)) { /* ln overflow ? */
		    /* If it could work, try it. */
		    if (c1 == -1000 ||
			UCHARAT(PL_reginput) == c1 ||
			UCHARAT(PL_reginput) == c2)
		    {
			if (paren) {
			    if (n) {
				PL_regstartp[paren] = PL_reginput - 1;
				PL_regendp[paren] = PL_reginput;
			    } else
				PL_regendp[paren] = NULL;
			}
			if (regmatch(next))
			    sayYES;
			REGCP_UNWIND;
		    }
		    /* Couldn't or didn't -- move forward. */
		    PL_reginput = locinput + ln;
		    if (regrepeat(scan, 1)) {
			ln++;
			PL_reginput = locinput + ln;
		    } else
			sayNO;
		}
	    }
	    else {
		CHECKPOINT lastcp;
		n = regrepeat(scan, n);
		if (ln < n && regkind[(U8)OP(next)] == EOL &&
		    (!PL_multiline  || OP(next) == SEOL))
		    ln = n;			/* why back off? */
		REGCP_SET;
		if (paren) {
		    while (n >= ln) {
			/* If it could work, try it. */
			if (c1 == -1000 ||
			    UCHARAT(PL_reginput) == c1 ||
			    UCHARAT(PL_reginput) == c2)
			    {
				if (paren && n) {
				    if (n) {
					PL_regstartp[paren] = PL_reginput - 1;
					PL_regendp[paren] = PL_reginput;
				    } else
					PL_regendp[paren] = NULL;
				}
				if (regmatch(next))
				    sayYES;
				REGCP_UNWIND;
			    }
			/* Couldn't or didn't -- back up. */
			n--;
			PL_reginput = locinput + n;
		    }
		} else {
		    while (n >= ln) {
			/* If it could work, try it. */
			if (c1 == -1000 ||
			    UCHARAT(PL_reginput) == c1 ||
			    UCHARAT(PL_reginput) == c2)
			    {
				if (regmatch(next))
				    sayYES;
				REGCP_UNWIND;
			    }
			/* Couldn't or didn't -- back up. */
			n--;
			PL_reginput = locinput + n;
		    }
		}
	    }
	    sayNO;
	    break;
	case END:
	    if (locinput < PL_regtill)
		sayNO;			/* Cannot match: too short. */
	    /* Fall through */
	case SUCCEED:
	    PL_reginput = locinput;	/* put where regtry can find it */
	    sayYES;			/* Success! */
	case SUSPEND:
	    n = 1;
	    PL_reginput = locinput;
	    goto do_ifmatch;	    
	case UNLESSM:
	    n = 0;
	    if (locinput < PL_bostr + scan->flags) 
		goto say_yes;
	    goto do_ifmatch;
	case IFMATCH:
	    n = 1;
	    if (locinput < PL_bostr + scan->flags) 
		goto say_no;
	  do_ifmatch:
	    PL_reginput = locinput - scan->flags;
	    inner = NEXTOPER(NEXTOPER(scan));
	    if (regmatch(inner) != n) {
	      say_no:
		if (logical) {
		    logical = 0;
		    sw = 0;
		    goto do_longjump;
		} else
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
	    PerlIO_printf(PerlIO_stderr(), "%lx %d\n",
			  (unsigned long)scan, OP(scan));
	    FAIL("regexp memory corruption");
	}
	scan = next;
    }

    /*
    * We get here only if there's trouble -- normally "case END" is
    * the terminating point.
    */
    FAIL("corrupted regexp pointers");
    /*NOTREACHED*/
    sayNO;

yes:
#ifdef DEBUGGING
    PL_regindent--;
#endif
    return 1;

no:
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
 * rather than incrementing count on every character.]
 */
STATIC I32
regrepeat(regnode *p, I32 max)
{
    dTHR;
    register char *scan;
    register char *opnd;
    register I32 c;
    register char *loceol = PL_regeol;

    scan = PL_reginput;
    if (max != REG_INFTY && max < loceol - scan)
      loceol = scan + max;
    opnd = (char *) OPERAND(p);
    switch (OP(p)) {
    case ANY:
	while (scan < loceol && *scan != '\n')
	    scan++;
	break;
    case SANY:
	scan = loceol;
	break;
    case EXACT:		/* length of string is 1 */
	c = UCHARAT(++opnd);
	while (scan < loceol && UCHARAT(scan) == c)
	    scan++;
	break;
    case EXACTF:	/* length of string is 1 */
	c = UCHARAT(++opnd);
	while (scan < loceol &&
	       (UCHARAT(scan) == c || UCHARAT(scan) == fold[c]))
	    scan++;
	break;
    case EXACTFL:	/* length of string is 1 */
	PL_reg_flags |= RF_tainted;
	c = UCHARAT(++opnd);
	while (scan < loceol &&
	       (UCHARAT(scan) == c || UCHARAT(scan) == fold_locale[c]))
	    scan++;
	break;
    case ANYOF:
	while (scan < loceol && REGINCLASS(opnd, *scan))
	    scan++;
	break;
    case ALNUM:
	while (scan < loceol && isALNUM(*scan))
	    scan++;
	break;
    case ALNUML:
	PL_reg_flags |= RF_tainted;
	while (scan < loceol && isALNUM_LC(*scan))
	    scan++;
	break;
    case NALNUM:
	while (scan < loceol && !isALNUM(*scan))
	    scan++;
	break;
    case NALNUML:
	PL_reg_flags |= RF_tainted;
	while (scan < loceol && !isALNUM_LC(*scan))
	    scan++;
	break;
    case SPACE:
	while (scan < loceol && isSPACE(*scan))
	    scan++;
	break;
    case SPACEL:
	PL_reg_flags |= RF_tainted;
	while (scan < loceol && isSPACE_LC(*scan))
	    scan++;
	break;
    case NSPACE:
	while (scan < loceol && !isSPACE(*scan))
	    scan++;
	break;
    case NSPACEL:
	PL_reg_flags |= RF_tainted;
	while (scan < loceol && !isSPACE_LC(*scan))
	    scan++;
	break;
    case DIGIT:
	while (scan < loceol && isDIGIT(*scan))
	    scan++;
	break;
    case NDIGIT:
	while (scan < loceol && !isDIGIT(*scan))
	    scan++;
	break;
    default:		/* Called on something of 0 width. */
	break;		/* So match right here or not at all. */
    }

    c = scan - PL_reginput;
    PL_reginput = scan;

    DEBUG_r( 
	{
		SV *prop = sv_newmortal();

		regprop(prop, p);
		PerlIO_printf(Perl_debug_log, 
			      "%*s  %s can match %ld times out of %ld...\n", 
			      REPORT_CODE_OFF+1, "", SvPVX(prop),c,max);
	});
    
    return(c);
}

/*
 - regrepeat_hard - repeatedly match something, report total lenth and length
 * 
 * The repeater is supposed to have constant length.
 */

STATIC I32
regrepeat_hard(regnode *p, I32 max, I32 *lp)
{
    dTHR;
    register char *scan;
    register char *start;
    register char *loceol = PL_regeol;
    I32 l = 0;
    I32 count = 0, res = 1;

    if (!max)
	return 0;

    start = PL_reginput;
    while (PL_reginput < loceol && (scan = PL_reginput, res = regmatch(p))) {
	if (!count++) {
	    *lp = l = PL_reginput - start;
	    if (max != REG_INFTY && l*max < loceol - scan)
		loceol = scan + l*max;
	    if (l == 0)
		return max;
	}
    }
    if (!res)
	PL_reginput = scan;
    
    return count;
}

/*
 - regclass - determine if a character falls into a character class
 */

STATIC bool
reginclass(register char *p, register I32 c)
{
    dTHR;
    char flags = *p;
    bool match = FALSE;

    c &= 0xFF;
    if (ANYOF_TEST(p, c))
	match = TRUE;
    else if (flags & ANYOF_FOLD) {
	I32 cf;
	if (flags & ANYOF_LOCALE) {
	    PL_reg_flags |= RF_tainted;
	    cf = fold_locale[c];
	}
	else
	    cf = fold[c];
	if (ANYOF_TEST(p, cf))
	    match = TRUE;
    }

    if (!match && (flags & ANYOF_ISA)) {
	PL_reg_flags |= RF_tainted;

	if (((flags & ANYOF_ALNUML)  && isALNUM_LC(c))  ||
	    ((flags & ANYOF_NALNUML) && !isALNUM_LC(c)) ||
	    ((flags & ANYOF_SPACEL)  && isSPACE_LC(c))  ||
	    ((flags & ANYOF_NSPACEL) && !isSPACE_LC(c)))
	{
	    match = TRUE;
	}
    }

    return (flags & ANYOF_INVERT) ? !match : match;
}



