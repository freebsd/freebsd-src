/* NOTE: this is derived from Henry Spencer's regexp code, and should not
 * confused with the original package (see point 3 below).  Thanks, Henry!
 */

/* Additional note: this code is very heavily munged from Henry's version
 * in places.  In some spots I've traded clarity for efficiency, so don't
 * blame Henry for some of the lack of readability.
 */

/* $RCSfile: regexec.c,v $$Revision: 1.2 $$Date: 1995/05/30 05:03:16 $
 *
 * $Log: regexec.c,v $
 * Revision 1.2  1995/05/30 05:03:16  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1994/09/10  06:27:33  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:39  nate
 * PERL!
 *
 * Revision 4.0.1.4  92/06/08  15:25:50  lwall
 * patch20: pattern modifiers i and g didn't interact right
 * patch20: in some cases $` and $' didn't get set by match
 * patch20: /x{0}/ was wrongly interpreted as /x{0,}/
 *
 * Revision 4.0.1.3  91/11/05  18:23:55  lwall
 * patch11: prepared for ctype implementations that don't define isascii()
 * patch11: initial .* in pattern had dependency on value of $*
 *
 * Revision 4.0.1.2  91/06/07  11:50:33  lwall
 * patch4: new copyright notice
 * patch4: // wouldn't use previous pattern if it started with a null character
 *
 * Revision 4.0.1.1  91/04/12  09:07:39  lwall
 * patch1: regexec only allocated space for 9 subexpresssions
 *
 * Revision 4.0  91/03/20  01:39:16  lwall
 * 4.0 baseline.
 *
 */
/*SUPPRESS 112*/
/*
 * regcomp and regexec -- regsub and regerror are not used in perl
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
 ****    Copyright (c) 1991, Larry Wall
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

#ifndef STATIC
#define	STATIC	static
#endif

#ifdef DEBUGGING
int regnarrate = 0;
#endif

/*
 * regexec and friends
 */

/*
 * Global work variables for regexec().
 */
static char *regprecomp;
static char *reginput;		/* String-input pointer. */
static char regprev;		/* char before regbol, \n if none */
static char *regbol;		/* Beginning of input, for ^ check. */
static char *regeol;		/* End of input, for $ check. */
static char **regstartp;	/* Pointer to startp array. */
static char **regendp;		/* Ditto for endp. */
static char *reglastparen;	/* Similarly for lastparen. */
static char *regtill;

static int regmyp_size = 0;
static char **regmystartp = Null(char**);
static char **regmyendp   = Null(char**);

/*
 * Forwards.
 */
STATIC int regtry();
STATIC int regmatch();
STATIC int regrepeat();

extern int multiline;

/*
 - regexec - match a regexp against a string
 */
int
regexec(prog, stringarg, strend, strbeg, minend, screamer, safebase)
register regexp *prog;
char *stringarg;
register char *strend;	/* pointer to null at end of string */
char *strbeg;	/* real beginning of string */
int minend;	/* end of match must be at least minend after stringarg */
STR *screamer;
int safebase;	/* no need to remember string in subbase */
{
	register char *s;
	register int i;
	register char *c;
	register char *string = stringarg;
	register int tmp;
	int minlen = 0;		/* must match at least this many chars */
	int dontbother = 0;	/* how many characters not to try at end */

	/* Be paranoid... */
	if (prog == NULL || string == NULL) {
		fatal("NULL regexp parameter");
		return(0);
	}

	if (string == strbeg)	/* is ^ valid at stringarg? */
	    regprev = '\n';
	else {
	    regprev = stringarg[-1];
	    if (!multiline && regprev == '\n')
		regprev = '\0';		/* force ^ to NOT match */
	}
	regprecomp = prog->precomp;
	/* Check validity of program. */
	if (UCHARAT(prog->program) != MAGIC) {
		FAIL("corrupted regexp program");
	}

	if (prog->do_folding) {
		i = strend - string;
		New(1101,c,i+1,char);
		Copy(string, c, i+1, char);
		string = c;
		strend = string + i;
		for (s = string; s < strend; s++)
			if (isUPPER(*s))
				*s = tolower(*s);
	}

	/* If there is a "must appear" string, look for it. */
	s = string;
	if (prog->regmust != Nullstr &&
	    (!(prog->reganch & ROPT_ANCH)
	     || (multiline && prog->regback >= 0)) ) {
		if (stringarg == strbeg && screamer) {
			if (screamfirst[prog->regmust->str_rare] >= 0)
				s = screaminstr(screamer,prog->regmust);
			else
				s = Nullch;
		}
#ifndef lint
		else
			s = fbminstr((unsigned char*)s, (unsigned char*)strend,
			    prog->regmust);
#endif
		if (!s) {
			++prog->regmust->str_u.str_useful;	/* hooray */
			goto phooey;	/* not present */
		}
		else if (prog->regback >= 0) {
			s -= prog->regback;
			if (s < string)
			    s = string;
			minlen = prog->regback + prog->regmust->str_cur;
		}
		else if (--prog->regmust->str_u.str_useful < 0) { /* boo */
			str_free(prog->regmust);
			prog->regmust = Nullstr;	/* disable regmust */
			s = string;
		}
		else {
			s = string;
			minlen = prog->regmust->str_cur;
		}
	}

	/* Mark beginning of line for ^ . */
	regbol = string;

	/* Mark end of line for $ (and such) */
	regeol = strend;

	/* see how far we have to get to not match where we matched before */
	regtill = string+minend;

	/* Allocate our backreference arrays */
	if ( regmyp_size < prog->nparens + 1 ) {
	    /* Allocate or enlarge the arrays */
	    regmyp_size = prog->nparens + 1;
	    if ( regmyp_size < 10 ) regmyp_size = 10;	/* minimum */
	    if ( regmystartp ) {
		/* reallocate larger */
		Renew(regmystartp,regmyp_size,char*);
		Renew(regmyendp,  regmyp_size,char*);
	    }
	    else {
		/* Initial allocation */
		New(1102,regmystartp,regmyp_size,char*);
		New(1102,regmyendp,  regmyp_size,char*);
	    }

	}

	/* Simplest case:  anchored match need be tried only once. */
	/*  [unless multiline is set] */
	if (prog->reganch & ROPT_ANCH) {
		if (regtry(prog, string))
			goto got_it;
		else if (multiline || (prog->reganch & ROPT_IMPLICIT)) {
			if (minlen)
			    dontbother = minlen - 1;
			strend -= dontbother;
			/* for multiline we only have to try after newlines */
			if (s > string)
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
	if (prog->regstart) {
		if (prog->reganch & ROPT_SKIP) {  /* we have /x+whatever/ */
		    /* it must be a one character string */
		    i = prog->regstart->str_ptr[0];
		    while (s < strend) {
			    if (*s == i) {
				    if (regtry(prog, s))
					    goto got_it;
				    s++;
				    while (s < strend && *s == i)
					s++;
			    }
			    s++;
		    }
		}
		else if (prog->regstart->str_pok == 3) {
		    /* We know what string it must start with. */
#ifndef lint
		    while ((s = fbminstr((unsigned char*)s,
		      (unsigned char*)strend, prog->regstart)) != NULL)
#else
		    while (s = Nullch)
#endif
		    {
			    if (regtry(prog, s))
				    goto got_it;
			    s++;
		    }
		}
		else {
		    c = prog->regstart->str_ptr;
		    while ((s = ninstr(s, strend,
		      c, c + prog->regstart->str_cur )) != NULL) {
			    if (regtry(prog, s))
				    goto got_it;
			    s++;
		    }
		}
		goto phooey;
	}
	/*SUPPRESS 560*/
	if (c = prog->regstclass) {
		int doevery = (prog->reganch & ROPT_SKIP) == 0;

		if (minlen)
		    dontbother = minlen - 1;
		strend -= dontbother;	/* don't bother with what can't match */
		tmp = 1;
		/* We know what class it must start with. */
		switch (OP(c)) {
		case ANYOF:
		    c = OPERAND(c);
		    while (s < strend) {
			    i = UCHARAT(s);
			    if (!(c[i >> 3] & (1 << (i&7)))) {
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
		case BOUND:
		    if (minlen)
			dontbother++,strend--;
		    if (s != string) {
			i = s[-1];
			tmp = isALNUM(i);
		    }
		    else
			tmp = isALNUM(regprev);	/* assume not alphanumeric */
		    while (s < strend) {
			    i = *s;
			    if (tmp != isALNUM(i)) {
				    tmp = !tmp;
				    if (regtry(prog, s))
					    goto got_it;
			    }
			    s++;
		    }
		    if ((minlen || tmp) && regtry(prog,s))
			    goto got_it;
		    break;
		case NBOUND:
		    if (minlen)
			dontbother++,strend--;
		    if (s != string) {
			i = s[-1];
			tmp = isALNUM(i);
		    }
		    else
			tmp = isALNUM(regprev);	/* assume not alphanumeric */
		    while (s < strend) {
			    i = *s;
			    if (tmp != isALNUM(i))
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
			    i = *s;
			    if (isALNUM(i)) {
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
			    i = *s;
			    if (!isALNUM(i)) {
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
		if (minlen)
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
	prog->subend = strend;
	if ((!safebase && (prog->nparens || sawampersand)) || prog->do_folding){
		strend += dontbother;	/* uncheat */
		if (safebase)			/* no need for $digit later */
		    s = strbeg;
		else if (strbeg != prog->subbase) {
		    i = strend - string + (stringarg - strbeg);
		    s = nsavestr(strbeg,i);	/* so $digit will work later */
		    if (prog->subbase)
			    Safefree(prog->subbase);
		    prog->subbeg = prog->subbase = s;
		    prog->subend = s+i;
		}
		else {
		    i = strend - string + (stringarg - strbeg);
		    prog->subbeg = s = prog->subbase;
		    prog->subend = s+i;
		}
		s += (stringarg - strbeg);
		for (i = 0; i <= prog->nparens; i++) {
			if (prog->endp[i]) {
			    prog->startp[i] = s + (prog->startp[i] - string);
			    prog->endp[i] = s + (prog->endp[i] - string);
			}
		}
		if (prog->do_folding)
			Safefree(string);
	}
	return(1);

    phooey:
	if (prog->do_folding)
		Safefree(string);
	return(0);
}

/*
 - regtry - try match at specific point
 */
static int			/* 0 failure, 1 success */
regtry(prog, string)
regexp *prog;
char *string;
{
	register int i;
	register char **sp;
	register char **ep;

	reginput = string;
	regstartp = prog->startp;
	regendp = prog->endp;
	reglastparen = &prog->lastparen;
	prog->lastparen = 0;

	sp = prog->startp;
	ep = prog->endp;
	if (prog->nparens) {
		for (i = prog->nparens; i >= 0; i--) {
			*sp++ = NULL;
			*ep++ = NULL;
		}
	}
	if (regmatch(prog->program + 1) && reginput >= regtill) {
		prog->startp[0] = string;
		prog->endp[0] = reginput;
		return(1);
	} else
		return(0);
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
static int			/* 0 failure, 1 success */
regmatch(prog)
char *prog;
{
	register char *scan;	/* Current node. */
	char *next;		/* Next node. */
	register int nextchar;
	register int n;		/* no or next */
	register int ln;        /* len or last */
	register char *s;	/* operand or save */
	register char *locinput = reginput;

	nextchar = *locinput;
	scan = prog;
#ifdef DEBUGGING
	if (scan != NULL && regnarrate)
		fprintf(stderr, "%s(\n", regprop(scan));
#endif
	while (scan != NULL) {
#ifdef DEBUGGING
		if (regnarrate)
			fprintf(stderr, "%s...\n", regprop(scan));
#endif

#ifdef REGALIGN
		next = scan + NEXT(scan);
		if (next == scan)
		    next = NULL;
#else
		next = regnext(scan);
#endif

		switch (OP(scan)) {
		case BOL:
			if (locinput == regbol ? regprev == '\n' :
			    ((nextchar || locinput < regeol) &&
			      locinput[-1] == '\n') )
			{
				/* regtill = regbol; */
				break;
			}
			return(0);
		case EOL:
			if ((nextchar || locinput < regeol) && nextchar != '\n')
				return(0);
			if (!multiline && regeol - locinput > 1)
				return 0;
			/* regtill = regbol; */
			break;
		case ANY:
			if ((nextchar == '\0' && locinput >= regeol) ||
			  nextchar == '\n')
				return(0);
			nextchar = *++locinput;
			break;
		case EXACTLY:
			s = OPERAND(scan);
			ln = *s++;
			/* Inline the first character, for speed. */
			if (*s != nextchar)
				return(0);
			if (regeol - locinput < ln)
				return 0;
			if (ln > 1 && bcmp(s, locinput, ln) != 0)
				return(0);
			locinput += ln;
			nextchar = *locinput;
			break;
		case ANYOF:
			s = OPERAND(scan);
			if (nextchar < 0)
				nextchar = UCHARAT(locinput);
			if (s[nextchar >> 3] & (1 << (nextchar&7)))
				return(0);
			if (!nextchar && locinput >= regeol)
				return 0;
			nextchar = *++locinput;
			break;
		case ALNUM:
			if (!nextchar)
				return(0);
			if (!isALNUM(nextchar))
				return(0);
			nextchar = *++locinput;
			break;
		case NALNUM:
			if (!nextchar && locinput >= regeol)
				return(0);
			if (isALNUM(nextchar))
				return(0);
			nextchar = *++locinput;
			break;
		case NBOUND:
		case BOUND:
			if (locinput == regbol)	/* was last char in word? */
				ln = isALNUM(regprev);
			else
				ln = isALNUM(locinput[-1]);
			n = isALNUM(nextchar); /* is next char in word? */
			if ((ln == n) == (OP(scan) == BOUND))
				return(0);
			break;
		case SPACE:
			if (!nextchar && locinput >= regeol)
				return(0);
			if (!isSPACE(nextchar))
				return(0);
			nextchar = *++locinput;
			break;
		case NSPACE:
			if (!nextchar)
				return(0);
			if (isSPACE(nextchar))
				return(0);
			nextchar = *++locinput;
			break;
		case DIGIT:
			if (!isDIGIT(nextchar))
				return(0);
			nextchar = *++locinput;
			break;
		case NDIGIT:
			if (!nextchar && locinput >= regeol)
				return(0);
			if (isDIGIT(nextchar))
				return(0);
			nextchar = *++locinput;
			break;
		case REF:
			n = ARG1(scan);  /* which paren pair */
			s = regmystartp[n];
			if (!s)
			    return(0);
			if (!regmyendp[n])
			    return(0);
			if (s == regmyendp[n])
			    break;
			/* Inline the first character, for speed. */
			if (*s != nextchar)
				return(0);
			ln = regmyendp[n] - s;
			if (locinput + ln > regeol)
				return 0;
			if (ln > 1 && bcmp(s, locinput, ln) != 0)
				return(0);
			locinput += ln;
			nextchar = *locinput;
			break;

		case NOTHING:
			break;
		case BACK:
			break;
		case OPEN:
			n = ARG1(scan);  /* which paren pair */
			reginput = locinput;

			regmystartp[n] = locinput;	/* for REF */
			if (regmatch(next)) {
				/*
				 * Don't set startp if some later
				 * invocation of the same parentheses
				 * already has.
				 */
				if (regstartp[n] == NULL)
					regstartp[n] = locinput;
				return(1);
			} else
				return(0);
			/* NOTREACHED */
		case CLOSE: {
				n = ARG1(scan);  /* which paren pair */
				reginput = locinput;

				regmyendp[n] = locinput;	/* for REF */
				if (regmatch(next)) {
					/*
					 * Don't set endp if some later
					 * invocation of the same parentheses
					 * already has.
					 */
					if (regendp[n] == NULL) {
						regendp[n] = locinput;
						if (n > *reglastparen)
						    *reglastparen = n;
					}
					return(1);
				} else
					return(0);
			}
			/*NOTREACHED*/
		case BRANCH: {
				if (OP(next) != BRANCH)		/* No choice. */
					next = NEXTOPER(scan);	/* Avoid recursion. */
				else {
					do {
						reginput = locinput;
						if (regmatch(NEXTOPER(scan)))
							return(1);
#ifdef REGALIGN
						/*SUPPRESS 560*/
						if (n = NEXT(scan))
						    scan += n;
						else
						    scan = NULL;
#else
						scan = regnext(scan);
#endif
					} while (scan != NULL && OP(scan) == BRANCH);
					return(0);
					/* NOTREACHED */
				}
			}
			break;
		case CURLY:
			ln = ARG1(scan);  /* min to match */
			n  = ARG2(scan);  /* max to match */
			scan = NEXTOPER(scan) + 4;
			goto repeat;
		case STAR:
			ln = 0;
			n = 32767;
			scan = NEXTOPER(scan);
			goto repeat;
		case PLUS:
			/*
			 * Lookahead to avoid useless match attempts
			 * when we know what character comes next.
			 */
			ln = 1;
			n = 32767;
			scan = NEXTOPER(scan);
		    repeat:
			if (OP(next) == EXACTLY)
				nextchar = *(OPERAND(next)+1);
			else
				nextchar = -1000;
			reginput = locinput;
			n = regrepeat(scan, n);
			if (!multiline && OP(next) == EOL && ln < n)
			    ln = n;			/* why back off? */
			while (n >= ln) {
				/* If it could work, try it. */
				if (nextchar == -1000 || *reginput == nextchar)
					if (regmatch(next))
						return(1);
				/* Couldn't or didn't -- back up. */
				n--;
				reginput = locinput + n;
			}
			return(0);
		case END:
			reginput = locinput; /* put where regtry can find it */
			return(1);	/* Success! */
		default:
			printf("%p %d\n",scan,scan[1]);
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
#ifdef lint
	return 0;
#endif
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
/*
 * [This routine now assumes that it will only match on things of length 1.
 * That was true before, but now we assume scan - reginput is the count,
 * rather than incrementing count on every character.]
 */
static int
regrepeat(p, max)
char *p;
int max;
{
	register char *scan;
	register char *opnd;
	register int c;
	register char *loceol = regeol;

	scan = reginput;
	if (max != 32767 && max < loceol - scan)
	    loceol = scan + max;
	opnd = OPERAND(p);
	switch (OP(p)) {
	case ANY:
		while (scan < loceol && *scan != '\n')
			scan++;
		break;
	case EXACTLY:		/* length of string is 1 */
		opnd++;
		while (scan < loceol && *opnd == *scan)
			scan++;
		break;
	case ANYOF:
		c = UCHARAT(scan);
		while (scan < loceol && !(opnd[c >> 3] & (1 << (c & 7)))) {
			scan++;
			c = UCHARAT(scan);
		}
		break;
	case ALNUM:
		while (scan < loceol && isALNUM(*scan))
			scan++;
		break;
	case NALNUM:
		while (scan < loceol && !isALNUM(*scan))
			scan++;
		break;
	case SPACE:
		while (scan < loceol && isSPACE(*scan))
			scan++;
		break;
	case NSPACE:
		while (scan < loceol && !isSPACE(*scan))
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
	default:		/* Oh dear.  Called inappropriately. */
		FAIL("internal regexp foulup");
		/* NOTREACHED */
	}

	c = scan - reginput;
	reginput = scan;

	return(c);
}

/*
 - regnext - dig the "next" pointer out of a node
 *
 * [Note, when REGALIGN is defined there are two places in regmatch()
 * that bypass this code for speed.]
 */
char *
regnext(p)
register char *p;
{
	register int offset;

	if (p == &regdummy)
		return(NULL);

	offset = NEXT(p);
	if (offset == 0)
		return(NULL);

#ifdef REGALIGN
	return(p+offset);
#else
	if (OP(p) == BACK)
		return(p-offset);
	else
		return(p+offset);
#endif
}
