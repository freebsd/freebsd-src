/* $RCSfile: consarg.c,v $$Revision: 1.4 $$Date: 1997/08/08 20:53:58 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: consarg.c,v $
 * Revision 1.4  1997/08/08 20:53:58  joerg
 * Fix a buffer overflow condition (that causes a security hole in suidperl).
 *
 * Closes: CERT Advisory CA-97.17 - Vulnerability in suidperl
 * Obtained from: (partly) the fix in CA-97.17
 *
 * Revision 1.3  1995/05/30 05:02:57  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.2  1994/09/11  03:17:29  gclarkii
 * Changed AF_LOCAL to AF_LOCAL_XX so as not to conflict with 4.4 socket.h
 * Added casts to shutup warnings in doio.c
 *
 * Revision 1.1.1.1  1994/09/10  06:27:32  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:35  nate
 * PERL!
 *
 * Revision 4.0.1.4  92/06/08  12:26:27  lwall
 * patch20: new warning for use of x with non-numeric right operand
 * patch20: modulus with highest bit in left operand set didn't always work
 * patch20: illegal lvalue message could be followed by core dump
 * patch20: deleted some minor memory leaks
 *
 * Revision 4.0.1.3  91/11/05  16:21:16  lwall
 * patch11: random cleanup
 * patch11: added eval {}
 * patch11: added sort {} LIST
 * patch11: "foo" x -1 dumped core
 * patch11: substr() and vec() weren't allowed in an lvalue list
 *
 * Revision 4.0.1.2  91/06/07  10:33:12  lwall
 * patch4: new copyright notice
 * patch4: length($`), length($&), length($') now optimized to avoid string copy
 *
 * Revision 4.0.1.1  91/04/11  17:38:34  lwall
 * patch1: fixed "Bad free" error
 *
 * Revision 4.0  91/03/20  01:06:15  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"
static int nothing_in_common();
static int arg_common();
static int spat_common();

ARG *
make_split(stab,arg,limarg)
register STAB *stab;
register ARG *arg;
ARG *limarg;
{
    register SPAT *spat;

    if (arg->arg_type != O_MATCH) {
	Newz(201,spat,1,SPAT);
	spat->spat_next = curstash->tbl_spatroot; /* link into spat list */
	curstash->tbl_spatroot = spat;

	spat->spat_runtime = arg;
	arg = make_match(O_MATCH,stab2arg(A_STAB,defstab),spat);
    }
    Renew(arg,4,ARG);
    arg->arg_len = 3;
    if (limarg) {
	if (limarg->arg_type == O_ITEM) {
	    Copy(limarg+1,arg+3,1,ARG);
	    limarg[1].arg_type = A_NULL;
	    arg_free(limarg);
	}
	else {
	    arg[3].arg_flags = 0;
	    arg[3].arg_len = 0;
	    arg[3].arg_type = A_EXPR;
	    arg[3].arg_ptr.arg_arg = limarg;
	}
    }
    else {
	arg[3].arg_flags = 0;
	arg[3].arg_len = 0;
	arg[3].arg_type = A_NULL;
	arg[3].arg_ptr.arg_arg = Nullarg;
    }
    arg->arg_type = O_SPLIT;
    spat = arg[2].arg_ptr.arg_spat;
    spat->spat_repl = stab2arg(A_STAB,aadd(stab));
    if (spat->spat_short) {	/* exact match can bypass regexec() */
	if (!((spat->spat_flags & SPAT_SCANFIRST) &&
	    (spat->spat_flags & SPAT_ALL) )) {
	    str_free(spat->spat_short);
	    spat->spat_short = Nullstr;
	}
    }
    return arg;
}

ARG *
mod_match(type,left,pat)
register ARG *left;
register ARG *pat;
{

    register SPAT *spat;
    register ARG *newarg;

    if (!pat)
	return Nullarg;

    if ((pat->arg_type == O_MATCH ||
	 pat->arg_type == O_SUBST ||
	 pat->arg_type == O_TRANS ||
	 pat->arg_type == O_SPLIT
	) &&
	pat[1].arg_ptr.arg_stab == defstab ) {
	switch (pat->arg_type) {
	case O_MATCH:
	    newarg = make_op(type == O_MATCH ? O_MATCH : O_NMATCH,
		pat->arg_len,
		left,Nullarg,Nullarg);
	    break;
	case O_SUBST:
	    newarg = l(make_op(type == O_MATCH ? O_SUBST : O_NSUBST,
		pat->arg_len,
		left,Nullarg,Nullarg));
	    break;
	case O_TRANS:
	    newarg = l(make_op(type == O_MATCH ? O_TRANS : O_NTRANS,
		pat->arg_len,
		left,Nullarg,Nullarg));
	    break;
	case O_SPLIT:
	    newarg = make_op(type == O_MATCH ? O_SPLIT : O_SPLIT,
		pat->arg_len,
		left,Nullarg,Nullarg);
	    break;
	}
	if (pat->arg_len >= 2) {
	    newarg[2].arg_type = pat[2].arg_type;
	    newarg[2].arg_ptr = pat[2].arg_ptr;
	    newarg[2].arg_len = pat[2].arg_len;
	    newarg[2].arg_flags = pat[2].arg_flags;
	    if (pat->arg_len >= 3) {
		newarg[3].arg_type = pat[3].arg_type;
		newarg[3].arg_ptr = pat[3].arg_ptr;
		newarg[3].arg_len = pat[3].arg_len;
		newarg[3].arg_flags = pat[3].arg_flags;
	    }
	}
	free_arg(pat);
    }
    else {
	Newz(202,spat,1,SPAT);
	spat->spat_next = curstash->tbl_spatroot; /* link into spat list */
	curstash->tbl_spatroot = spat;

	spat->spat_runtime = pat;
	newarg = make_op(type,2,left,Nullarg,Nullarg);
	newarg[2].arg_type = A_SPAT | A_DONT;
	newarg[2].arg_ptr.arg_spat = spat;
    }

    return newarg;
}

ARG *
make_op(type,newlen,arg1,arg2,arg3)
int type;
int newlen;
ARG *arg1;
ARG *arg2;
ARG *arg3;
{
    register ARG *arg;
    register ARG *chld;
    register unsigned doarg;
    register int i;
    extern ARG *arg4;	/* should be normal arguments, really */
    extern ARG *arg5;

    arg = op_new(newlen);
    arg->arg_type = type;
    /*SUPPRESS 560*/
    if (chld = arg1) {
	if (chld->arg_type == O_ITEM &&
	    (hoistable[ i = (chld[1].arg_type&A_MASK)] || i == A_LVAL ||
	     (i == A_LEXPR &&
	      (chld[1].arg_ptr.arg_arg->arg_type == O_LIST ||
	       chld[1].arg_ptr.arg_arg->arg_type == O_ARRAY ||
	       chld[1].arg_ptr.arg_arg->arg_type == O_HASH ))))
	{
	    arg[1].arg_type = chld[1].arg_type;
	    arg[1].arg_ptr = chld[1].arg_ptr;
	    arg[1].arg_flags |= chld[1].arg_flags;
	    arg[1].arg_len = chld[1].arg_len;
	    free_arg(chld);
	}
	else {
	    arg[1].arg_type = A_EXPR;
	    arg[1].arg_ptr.arg_arg = chld;
	}
    }
    /*SUPPRESS 560*/
    if (chld = arg2) {
	if (chld->arg_type == O_ITEM &&
	    (hoistable[chld[1].arg_type&A_MASK] ||
	     (type == O_ASSIGN &&
	      ((chld[1].arg_type == A_READ && !(arg[1].arg_type & A_DONT))
		||
	       (chld[1].arg_type == A_INDREAD && !(arg[1].arg_type & A_DONT))
		||
	       (chld[1].arg_type == A_GLOB && !(arg[1].arg_type & A_DONT))
	      ) ) ) ) {
	    arg[2].arg_type = chld[1].arg_type;
	    arg[2].arg_ptr = chld[1].arg_ptr;
	    arg[2].arg_len = chld[1].arg_len;
	    free_arg(chld);
	}
	else {
	    arg[2].arg_type = A_EXPR;
	    arg[2].arg_ptr.arg_arg = chld;
	}
    }
    /*SUPPRESS 560*/
    if (chld = arg3) {
	if (chld->arg_type == O_ITEM && hoistable[chld[1].arg_type&A_MASK]) {
	    arg[3].arg_type = chld[1].arg_type;
	    arg[3].arg_ptr = chld[1].arg_ptr;
	    arg[3].arg_len = chld[1].arg_len;
	    free_arg(chld);
	}
	else {
	    arg[3].arg_type = A_EXPR;
	    arg[3].arg_ptr.arg_arg = chld;
	}
    }
    if (newlen >= 4 && (chld = arg4)) {
	if (chld->arg_type == O_ITEM && hoistable[chld[1].arg_type&A_MASK]) {
	    arg[4].arg_type = chld[1].arg_type;
	    arg[4].arg_ptr = chld[1].arg_ptr;
	    arg[4].arg_len = chld[1].arg_len;
	    free_arg(chld);
	}
	else {
	    arg[4].arg_type = A_EXPR;
	    arg[4].arg_ptr.arg_arg = chld;
	}
    }
    if (newlen >= 5 && (chld = arg5)) {
	if (chld->arg_type == O_ITEM && hoistable[chld[1].arg_type&A_MASK]) {
	    arg[5].arg_type = chld[1].arg_type;
	    arg[5].arg_ptr = chld[1].arg_ptr;
	    arg[5].arg_len = chld[1].arg_len;
	    free_arg(chld);
	}
	else {
	    arg[5].arg_type = A_EXPR;
	    arg[5].arg_ptr.arg_arg = chld;
	}
    }
    doarg = opargs[type];
    for (i = 1; i <= newlen; ++i) {
	if (!(doarg & 1))
	    arg[i].arg_type |= A_DONT;
	if (doarg & 2)
	    arg[i].arg_flags |= AF_ARYOK;
	doarg >>= 2;
    }
#ifdef DEBUGGING
    if (debug & 16) {
	fprintf(stderr,"%p <= make_op(%s",arg,opname[arg->arg_type]);
	if (arg1)
	    fprintf(stderr,",%s=%p",
		argname[arg[1].arg_type&A_MASK],arg[1].arg_ptr.arg_arg);
	if (arg2)
	    fprintf(stderr,",%s=%p",
		argname[arg[2].arg_type&A_MASK],arg[2].arg_ptr.arg_arg);
	if (arg3)
	    fprintf(stderr,",%s=%p",
		argname[arg[3].arg_type&A_MASK],arg[3].arg_ptr.arg_arg);
	if (newlen >= 4)
	    fprintf(stderr,",%s=%p",
		argname[arg[4].arg_type&A_MASK],arg[4].arg_ptr.arg_arg);
	if (newlen >= 5)
	    fprintf(stderr,",%s=%p",
		argname[arg[5].arg_type&A_MASK],arg[5].arg_ptr.arg_arg);
	fprintf(stderr,")\n");
    }
#endif
    arg = evalstatic(arg);	/* see if we can consolidate anything */
    return arg;
}

ARG *
evalstatic(arg)
register ARG *arg;
{
    static STR *str = Nullstr;
    register STR *s1;
    register STR *s2;
    double value;		/* must not be register */
    register char *tmps;
    int i;
    unsigned long tmplong;
    long tmp2;
    double exp(), log(), sqrt(), modf();
    char *crypt();
    double sin(), cos(), atan2(), pow();

    if (!arg || !arg->arg_len)
	return arg;

    if (!str)
	str = Str_new(20,0);

    if (arg[1].arg_type == A_SINGLE)
	s1 = arg[1].arg_ptr.arg_str;
    else
	s1 = Nullstr;
    if (arg->arg_len >= 2 && arg[2].arg_type == A_SINGLE)
	s2 = arg[2].arg_ptr.arg_str;
    else
	s2 = Nullstr;

#define CHECK1 if (!s1) return arg
#define CHECK2 if (!s2) return arg
#define CHECK12 if (!s1 || !s2) return arg

    switch (arg->arg_type) {
    default:
	return arg;
    case O_SORT:
	if (arg[1].arg_type == A_CMD)
	    arg[1].arg_type |= A_DONT;
	return arg;
    case O_EVAL:
	if (arg[1].arg_type == A_CMD) {
	    arg->arg_type = O_TRY;
	    arg[1].arg_type |= A_DONT;
	    return arg;
	}
	CHECK1;
	arg->arg_type = O_EVALONCE;
	return arg;
    case O_AELEM:
	CHECK2;
	i = (int)str_gnum(s2);
	if (i < 32767 && i >= 0) {
	    arg->arg_type = O_ITEM;
	    arg->arg_len = 1;
	    arg[1].arg_type = A_ARYSTAB;	/* $abc[123] is hoistable now */
	    arg[1].arg_len = i;
	    str_free(s2);
	    Renew(arg, 2, ARG);
	}
	return arg;
    case O_CONCAT:
	CHECK12;
	str_sset(str,s1);
	str_scat(str,s2);
	break;
    case O_REPEAT:
	CHECK2;
	if (dowarn && !s2->str_nok && !looks_like_number(s2))
	    warn("Right operand of x is not numeric");
	CHECK1;
	i = (int)str_gnum(s2);
	tmps = str_get(s1);
	str_nset(str,"",0);
	if (i > 0) {
	    STR_GROW(str, i * s1->str_cur + 1);
	    repeatcpy(str->str_ptr, tmps, s1->str_cur, i);
	    str->str_cur = i * s1->str_cur;
	    str->str_ptr[str->str_cur] = '\0';
	}
	break;
    case O_MULTIPLY:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,value * str_gnum(s2));
	break;
    case O_DIVIDE:
	CHECK12;
	value = str_gnum(s2);
	if (value == 0.0)
	    yyerror("Illegal division by constant zero");
	else
#ifdef SLOPPYDIVIDE
	/* insure that 20./5. == 4. */
	{
	    double x;
	    int    k;
	    x =  str_gnum(s1);
	    if ((double)(int)x     == x &&
		(double)(int)value == value &&
		(k = (int)x/(int)value)*(int)value == (int)x) {
		value = k;
	    } else {
		value = x/value;
	    }
	    str_numset(str,value);
	}
#else
	str_numset(str,str_gnum(s1) / value);
#endif
	break;
    case O_MODULO:
	CHECK12;
	tmplong = (unsigned long)str_gnum(s2);
	if (tmplong == 0L) {
	    yyerror("Illegal modulus of constant zero");
	    return arg;
	}
	value = str_gnum(s1);
#ifndef lint
	if (value >= 0.0)
	    str_numset(str,(double)(((unsigned long)value) % tmplong));
	else {
	    tmp2 = (long)value;
	    str_numset(str,(double)((tmplong-((-tmp2 - 1) % tmplong)) - 1));
	}
#else
	tmp2 = tmp2;
#endif
	break;
    case O_ADD:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,value + str_gnum(s2));
	break;
    case O_SUBTRACT:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,value - str_gnum(s2));
	break;
    case O_LEFT_SHIFT:
	CHECK12;
	value = str_gnum(s1);
	i = (int)str_gnum(s2);
#ifndef lint
	str_numset(str,(double)(((long)value) << i));
#endif
	break;
    case O_RIGHT_SHIFT:
	CHECK12;
	value = str_gnum(s1);
	i = (int)str_gnum(s2);
#ifndef lint
	str_numset(str,(double)(((long)value) >> i));
#endif
	break;
    case O_LT:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,(value < str_gnum(s2)) ? 1.0 : 0.0);
	break;
    case O_GT:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,(value > str_gnum(s2)) ? 1.0 : 0.0);
	break;
    case O_LE:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,(value <= str_gnum(s2)) ? 1.0 : 0.0);
	break;
    case O_GE:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,(value >= str_gnum(s2)) ? 1.0 : 0.0);
	break;
    case O_EQ:
	CHECK12;
	if (dowarn) {
	    if ((!s1->str_nok && !looks_like_number(s1)) ||
		(!s2->str_nok && !looks_like_number(s2)) )
		warn("Possible use of == on string value");
	}
	value = str_gnum(s1);
	str_numset(str,(value == str_gnum(s2)) ? 1.0 : 0.0);
	break;
    case O_NE:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,(value != str_gnum(s2)) ? 1.0 : 0.0);
	break;
    case O_NCMP:
	CHECK12;
	value = str_gnum(s1);
	value -= str_gnum(s2);
	if (value > 0.0)
	    value = 1.0;
	else if (value < 0.0)
	    value = -1.0;
	str_numset(str,value);
	break;
    case O_BIT_AND:
	CHECK12;
	value = str_gnum(s1);
#ifndef lint
	str_numset(str,(double)(U_L(value) & U_L(str_gnum(s2))));
#endif
	break;
    case O_XOR:
	CHECK12;
	value = str_gnum(s1);
#ifndef lint
	str_numset(str,(double)(U_L(value) ^ U_L(str_gnum(s2))));
#endif
	break;
    case O_BIT_OR:
	CHECK12;
	value = str_gnum(s1);
#ifndef lint
	str_numset(str,(double)(U_L(value) | U_L(str_gnum(s2))));
#endif
	break;
    case O_AND:
	CHECK12;
	if (str_true(s1))
	    str_sset(str,s2);
	else
	    str_sset(str,s1);
	break;
    case O_OR:
	CHECK12;
	if (str_true(s1))
	    str_sset(str,s1);
	else
	    str_sset(str,s2);
	break;
    case O_COND_EXPR:
	CHECK12;
	if ((arg[3].arg_type & A_MASK) != A_SINGLE)
	    return arg;
	if (str_true(s1))
	    str_sset(str,s2);
	else
	    str_sset(str,arg[3].arg_ptr.arg_str);
	str_free(arg[3].arg_ptr.arg_str);
	Renew(arg, 3, ARG);
	break;
    case O_NEGATE:
	CHECK1;
	str_numset(str,(double)(-str_gnum(s1)));
	break;
    case O_NOT:
	CHECK1;
#ifdef NOTNOT
	{ char xxx = str_true(s1); str_numset(str,(double)!xxx); }
#else
	str_numset(str,(double)(!str_true(s1)));
#endif
	break;
    case O_COMPLEMENT:
	CHECK1;
#ifndef lint
	str_numset(str,(double)(~U_L(str_gnum(s1))));
#endif
	break;
    case O_SIN:
	CHECK1;
	str_numset(str,sin(str_gnum(s1)));
	break;
    case O_COS:
	CHECK1;
	str_numset(str,cos(str_gnum(s1)));
	break;
    case O_ATAN2:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,atan2(value, str_gnum(s2)));
	break;
    case O_POW:
	CHECK12;
	value = str_gnum(s1);
	str_numset(str,pow(value, str_gnum(s2)));
	break;
    case O_LENGTH:
	if (arg[1].arg_type == A_STAB) {
	    arg->arg_type = O_ITEM;
	    arg[1].arg_type = A_LENSTAB;
	    return arg;
	}
	CHECK1;
	str_numset(str, (double)str_len(s1));
	break;
    case O_SLT:
	CHECK12;
	str_numset(str,(double)(str_cmp(s1,s2) < 0));
	break;
    case O_SGT:
	CHECK12;
	str_numset(str,(double)(str_cmp(s1,s2) > 0));
	break;
    case O_SLE:
	CHECK12;
	str_numset(str,(double)(str_cmp(s1,s2) <= 0));
	break;
    case O_SGE:
	CHECK12;
	str_numset(str,(double)(str_cmp(s1,s2) >= 0));
	break;
    case O_SEQ:
	CHECK12;
	str_numset(str,(double)(str_eq(s1,s2)));
	break;
    case O_SNE:
	CHECK12;
	str_numset(str,(double)(!str_eq(s1,s2)));
	break;
    case O_SCMP:
	CHECK12;
	str_numset(str,(double)(str_cmp(s1,s2)));
	break;
    case O_CRYPT:
	CHECK12;
#ifdef HAS_CRYPT
	tmps = str_get(s1);
	str_set(str,crypt(tmps,str_get(s2)));
#else
	yyerror(
	"The crypt() function is unimplemented due to excessive paranoia.");
#endif
	break;
    case O_EXP:
	CHECK1;
	str_numset(str,exp(str_gnum(s1)));
	break;
    case O_LOG:
	CHECK1;
	str_numset(str,log(str_gnum(s1)));
	break;
    case O_SQRT:
	CHECK1;
	str_numset(str,sqrt(str_gnum(s1)));
	break;
    case O_INT:
	CHECK1;
	value = str_gnum(s1);
	if (value >= 0.0)
	    (void)modf(value,&value);
	else {
	    (void)modf(-value,&value);
	    value = -value;
	}
	str_numset(str,value);
	break;
    case O_ORD:
	CHECK1;
#ifndef I286
	str_numset(str,(double)(*str_get(s1)));
#else
	{
	    int  zapc;
	    char *zaps;

	    zaps = str_get(s1);
	    zapc = (int) *zaps;
	    str_numset(str,(double)(zapc));
	}
#endif
	break;
    }
    arg->arg_type = O_ITEM;	/* note arg1 type is already SINGLE */
    str_free(s1);
    arg[1].arg_ptr.arg_str = str;
    if (s2) {
	str_free(s2);
	arg[2].arg_ptr.arg_str = Nullstr;
	arg[2].arg_type = A_NULL;
    }
    str = Nullstr;

    return arg;
}

ARG *
l(arg)
register ARG *arg;
{
    register int i;
    register ARG *arg1;
    register ARG *arg2;
    SPAT *spat;
    int arghog = 0;

    i = arg[1].arg_type & A_MASK;

    arg->arg_flags |= AF_COMMON;	/* assume something in common */
					/* which forces us to copy things */

    if (i == A_ARYLEN) {
	arg[1].arg_type = A_LARYLEN;
	return arg;
    }
    if (i == A_ARYSTAB) {
	arg[1].arg_type = A_LARYSTAB;
	return arg;
    }

    /* see if it's an array reference */

    if (i == A_EXPR || i == A_LEXPR) {
	arg1 = arg[1].arg_ptr.arg_arg;

	if (arg1->arg_type == O_LIST || arg1->arg_type == O_ITEM) {
						/* assign to list */
	    if (arg->arg_len > 1) {
		dehoist(arg,2);
		arg2 = arg[2].arg_ptr.arg_arg;
		if (nothing_in_common(arg1,arg2))
		    arg->arg_flags &= ~AF_COMMON;
		if (arg->arg_type == O_ASSIGN) {
		    if (arg1->arg_flags & AF_LOCAL_XX)
			arg->arg_flags |= AF_LOCAL_XX;
		    arg[1].arg_flags |= AF_ARYOK;
		    arg[2].arg_flags |= AF_ARYOK;
		}
	    }
	    else if (arg->arg_type != O_CHOP)
		arg->arg_type = O_ASSIGN;	/* possible local(); */
	    for (i = arg1->arg_len; i >= 1; i--) {
		switch (arg1[i].arg_type) {
		case A_STAR: case A_LSTAR:
		    arg1[i].arg_type = A_LSTAR;
		    break;
		case A_STAB: case A_LVAL:
		    arg1[i].arg_type = A_LVAL;
		    break;
		case A_ARYLEN: case A_LARYLEN:
		    arg1[i].arg_type = A_LARYLEN;
		    break;
		case A_ARYSTAB: case A_LARYSTAB:
		    arg1[i].arg_type = A_LARYSTAB;
		    break;
		case A_EXPR: case A_LEXPR:
		    arg1[i].arg_type = A_LEXPR;
		    switch(arg1[i].arg_ptr.arg_arg->arg_type) {
		    case O_ARRAY: case O_LARRAY:
			arg1[i].arg_ptr.arg_arg->arg_type = O_LARRAY;
			arghog = 1;
			break;
		    case O_AELEM: case O_LAELEM:
			arg1[i].arg_ptr.arg_arg->arg_type = O_LAELEM;
			break;
		    case O_HASH: case O_LHASH:
			arg1[i].arg_ptr.arg_arg->arg_type = O_LHASH;
			arghog = 1;
			break;
		    case O_HELEM: case O_LHELEM:
			arg1[i].arg_ptr.arg_arg->arg_type = O_LHELEM;
			break;
		    case O_ASLICE: case O_LASLICE:
			arg1[i].arg_ptr.arg_arg->arg_type = O_LASLICE;
			break;
		    case O_HSLICE: case O_LHSLICE:
			arg1[i].arg_ptr.arg_arg->arg_type = O_LHSLICE;
			break;
		    case O_SUBSTR: case O_VEC:
			(void)l(arg1[i].arg_ptr.arg_arg);
			Renewc(arg1[i].arg_ptr.arg_arg->arg_ptr.arg_str, 1,
			  struct lstring, STR);
			    /* grow string struct to hold an lstring struct */
			break;
		    default:
			goto ill_item;
		    }
		    break;
		default:
		  ill_item:
		    (void)sprintf(tokenbuf, "Illegal item (%s) as lvalue",
		      argname[arg1[i].arg_type&A_MASK]);
		    yyerror(tokenbuf);
		}
	    }
	    if (arg->arg_len > 1) {
		if (arg2->arg_type == O_SPLIT && !arg2[3].arg_type && !arghog) {
		    arg2[3].arg_type = A_SINGLE;
		    arg2[3].arg_ptr.arg_str =
		      str_nmake((double)arg1->arg_len + 1); /* limit split len*/
		}
	    }
	}
	else if (arg1->arg_type == O_AELEM || arg1->arg_type == O_LAELEM)
	    if (arg->arg_type == O_DEFINED)
		arg1->arg_type = O_AELEM;
	    else
		arg1->arg_type = O_LAELEM;
	else if (arg1->arg_type == O_ARRAY || arg1->arg_type == O_LARRAY) {
	    arg1->arg_type = O_LARRAY;
	    if (arg->arg_len > 1) {
		dehoist(arg,2);
		arg2 = arg[2].arg_ptr.arg_arg;
		if (arg2->arg_type == O_SPLIT) { /* use split's builtin =?*/
		    spat = arg2[2].arg_ptr.arg_spat;
		    if (!(spat->spat_flags & SPAT_ONCE) &&
		      nothing_in_common(arg1,spat->spat_repl)) {
			spat->spat_repl[1].arg_ptr.arg_stab =
			    arg1[1].arg_ptr.arg_stab;
			arg1[1].arg_ptr.arg_stab = Nullstab;
			spat->spat_flags |= SPAT_ONCE;
			arg_free(arg1);	/* recursive */
			arg[1].arg_ptr.arg_arg = Nullarg;
			free_arg(arg);	/* non-recursive */
			return arg2;	/* split has builtin assign */
		    }
		}
		else if (nothing_in_common(arg1,arg2))
		    arg->arg_flags &= ~AF_COMMON;
		if (arg->arg_type == O_ASSIGN) {
		    arg[1].arg_flags |= AF_ARYOK;
		    arg[2].arg_flags |= AF_ARYOK;
		}
	    }
	    else if (arg->arg_type == O_ASSIGN)
		arg[1].arg_flags |= AF_ARYOK;
	}
	else if (arg1->arg_type == O_HELEM || arg1->arg_type == O_LHELEM)
	    if (arg->arg_type == O_DEFINED)
		arg1->arg_type = O_HELEM;	/* avoid creating one */
	    else
		arg1->arg_type = O_LHELEM;
	else if (arg1->arg_type == O_HASH || arg1->arg_type == O_LHASH) {
	    arg1->arg_type = O_LHASH;
	    if (arg->arg_len > 1) {
		dehoist(arg,2);
		arg2 = arg[2].arg_ptr.arg_arg;
		if (nothing_in_common(arg1,arg2))
		    arg->arg_flags &= ~AF_COMMON;
		if (arg->arg_type == O_ASSIGN) {
		    arg[1].arg_flags |= AF_ARYOK;
		    arg[2].arg_flags |= AF_ARYOK;
		}
	    }
	    else if (arg->arg_type == O_ASSIGN)
		arg[1].arg_flags |= AF_ARYOK;
	}
	else if (arg1->arg_type == O_ASLICE) {
	    arg1->arg_type = O_LASLICE;
	    if (arg->arg_type == O_ASSIGN) {
		dehoist(arg,2);
		arg[1].arg_flags |= AF_ARYOK;
		arg[2].arg_flags |= AF_ARYOK;
	    }
	}
	else if (arg1->arg_type == O_HSLICE) {
	    arg1->arg_type = O_LHSLICE;
	    if (arg->arg_type == O_ASSIGN) {
		dehoist(arg,2);
		arg[1].arg_flags |= AF_ARYOK;
		arg[2].arg_flags |= AF_ARYOK;
	    }
	}
	else if ((arg->arg_type == O_DEFINED || arg->arg_type == O_UNDEF) &&
	  (arg1->arg_type == (perldb ? O_DBSUBR : O_SUBR)) ) {
	    arg[1].arg_type |= A_DONT;
	}
	else if (arg1->arg_type == O_SUBSTR || arg1->arg_type == O_VEC) {
	    (void)l(arg1);
	    Renewc(arg1->arg_ptr.arg_str, 1, struct lstring, STR);
			/* grow string struct to hold an lstring struct */
	}
	else if (arg1->arg_type == O_ASSIGN)
	    /*SUPPRESS 530*/
	    ;
	else {
	    (void)sprintf(tokenbuf,
	      "Illegal expression (%s) as lvalue",opname[arg1->arg_type]);
	    yyerror(tokenbuf);
	    return arg;
	}
	arg[1].arg_type = A_LEXPR | (arg[1].arg_type & A_DONT);
	if (arg->arg_type == O_ASSIGN && (arg1[1].arg_flags & AF_ARYOK)) {
	    arg[1].arg_flags |= AF_ARYOK;
	    if (arg->arg_len > 1)
		arg[2].arg_flags |= AF_ARYOK;
	}
#ifdef DEBUGGING
	if (debug & 16)
	    fprintf(stderr,"lval LEXPR\n");
#endif
	return arg;
    }
    if (i == A_STAR || i == A_LSTAR) {
	arg[1].arg_type = A_LSTAR | (arg[1].arg_type & A_DONT);
	return arg;
    }

    /* not an array reference, should be a register name */

    if (i != A_STAB && i != A_LVAL) {
	(void)sprintf(tokenbuf,
	  "Illegal item (%s) as lvalue",argname[arg[1].arg_type&A_MASK]);
	yyerror(tokenbuf);
	return arg;
    }
    arg[1].arg_type = A_LVAL | (arg[1].arg_type & A_DONT);
#ifdef DEBUGGING
    if (debug & 16)
	fprintf(stderr,"lval LVAL\n");
#endif
    return arg;
}

ARG *
fixl(type,arg)
int type;
ARG *arg;
{
    if (type == O_DEFINED || type == O_UNDEF) {
	if (arg->arg_type != O_ITEM)
	    arg = hide_ary(arg);
	if (arg->arg_type == O_ITEM) {
	    type = arg[1].arg_type & A_MASK;
	    if (type == A_EXPR || type == A_LEXPR)
		arg[1].arg_type = A_LEXPR|A_DONT;
	}
    }
    return arg;
}

void
dehoist(arg,i)
ARG *arg;
{
    ARG *tmparg;

    if (arg[i].arg_type != A_EXPR) {	/* dehoist */
	tmparg = make_op(O_ITEM,1,Nullarg,Nullarg,Nullarg);
	tmparg[1] = arg[i];
	arg[i].arg_ptr.arg_arg = tmparg;
	arg[i].arg_type = A_EXPR;
    }
}

ARG *
addflags(i,flags,arg)
register ARG *arg;
{
    arg[i].arg_flags |= flags;
    return arg;
}

ARG *
hide_ary(arg)
ARG *arg;
{
    if (arg->arg_type == O_ARRAY || arg->arg_type == O_HASH)
	return make_op(O_ITEM,1,arg,Nullarg,Nullarg);
    return arg;
}

/* maybe do a join on multiple array dimensions */

ARG *
jmaybe(arg)
register ARG *arg;
{
    if (arg && arg->arg_type == O_COMMA) {
	arg = listish(arg);
	arg = make_op(O_JOIN, 2,
	    stab2arg(A_STAB,stabent(";",TRUE)),
	    make_list(arg),
	    Nullarg);
    }
    return arg;
}

ARG *
make_list(arg)
register ARG *arg;
{
    register int i;
    register ARG *node;
    register ARG *nxtnode;
    register int j;
    STR *tmpstr;

    if (!arg) {
	arg = op_new(0);
	arg->arg_type = O_LIST;
    }
    if (arg->arg_type != O_COMMA) {
	if (arg->arg_type != O_ARRAY)
	    arg->arg_flags |= AF_LISTISH;	/* see listish() below */
	    arg->arg_flags |= AF_LISTISH;	/* see listish() below */
	return arg;
    }
    for (i = 2, node = arg; ; i++) {
	if (node->arg_len < 2)
	    break;
        if (node[1].arg_type != A_EXPR)
	    break;
	node = node[1].arg_ptr.arg_arg;
	if (node->arg_type != O_COMMA)
	    break;
    }
    if (i > 2) {
	node = arg;
	arg = op_new(i);
	tmpstr = arg->arg_ptr.arg_str;
	StructCopy(node, arg, ARG);	/* copy everything except the STR */
	arg->arg_ptr.arg_str = tmpstr;
	for (j = i; ; ) {
	    StructCopy(node+2, arg+j, ARG);
	    arg[j].arg_flags |= AF_ARYOK;
	    --j;		/* Bug in Xenix compiler */
	    if (j < 2) {
		StructCopy(node+1, arg+1, ARG);
		free_arg(node);
		break;
	    }
	    nxtnode = node[1].arg_ptr.arg_arg;
	    free_arg(node);
	    node = nxtnode;
	}
    }
    arg[1].arg_flags |= AF_ARYOK;
    arg[2].arg_flags |= AF_ARYOK;
    arg->arg_type = O_LIST;
    arg->arg_len = i;
    str_free(arg->arg_ptr.arg_str);
    arg->arg_ptr.arg_str = Nullstr;
    return arg;
}

/* turn a single item into a list */

ARG *
listish(arg)
ARG *arg;
{
    if (arg && arg->arg_flags & AF_LISTISH)
	arg = make_op(O_LIST,1,arg,Nullarg,Nullarg);
    return arg;
}

ARG *
maybelistish(optype, arg)
int optype;
ARG *arg;
{
    ARG *tmparg = arg;

    if (optype == O_RETURN && arg->arg_type == O_ITEM &&
      arg[1].arg_type == A_EXPR && (tmparg = arg[1].arg_ptr.arg_arg) &&
      ((tmparg->arg_flags & AF_LISTISH) || (tmparg->arg_type == O_ARRAY) )) {
	tmparg = listish(tmparg);
	free_arg(arg);
	arg = tmparg;
    }
    else if (optype == O_PRTF ||
      (arg->arg_type == O_ASLICE || arg->arg_type == O_HSLICE ||
       arg->arg_type == O_F_OR_R) )
	arg = listish(arg);
    return arg;
}

/* mark list of local variables */

ARG *
localize(arg)
ARG *arg;
{
    arg->arg_flags |= AF_LOCAL_XX;
    return arg;
}

ARG *
rcatmaybe(arg)
ARG *arg;
{
    ARG *arg2;

    if (arg->arg_type == O_CONCAT && arg[2].arg_type == A_EXPR) {
	arg2 = arg[2].arg_ptr.arg_arg;
	if (arg2->arg_type == O_ITEM && arg2[1].arg_type == A_READ) {
	    arg->arg_type = O_RCAT;
	    arg[2].arg_type = arg2[1].arg_type;
	    arg[2].arg_ptr = arg2[1].arg_ptr;
	    free_arg(arg2);
	}
    }
    return arg;
}

ARG *
stab2arg(atype,stab)
int atype;
register STAB *stab;
{
    register ARG *arg;

    arg = op_new(1);
    arg->arg_type = O_ITEM;
    arg[1].arg_type = atype;
    arg[1].arg_ptr.arg_stab = stab;
    return arg;
}

ARG *
cval_to_arg(cval)
register char *cval;
{
    register ARG *arg;

    arg = op_new(1);
    arg->arg_type = O_ITEM;
    arg[1].arg_type = A_SINGLE;
    arg[1].arg_ptr.arg_str = str_make(cval,0);
    Safefree(cval);
    return arg;
}

ARG *
op_new(numargs)
int numargs;
{
    register ARG *arg;

    Newz(203,arg, numargs + 1, ARG);
    arg->arg_ptr.arg_str = Str_new(21,0);
    arg->arg_len = numargs;
    return arg;
}

void
free_arg(arg)
ARG *arg;
{
    str_free(arg->arg_ptr.arg_str);
    Safefree(arg);
}

ARG *
make_match(type,expr,spat)
int type;
ARG *expr;
SPAT *spat;
{
    register ARG *arg;

    arg = make_op(type,2,expr,Nullarg,Nullarg);

    arg[2].arg_type = A_SPAT|A_DONT;
    arg[2].arg_ptr.arg_spat = spat;
#ifdef DEBUGGING
    if (debug & 16)
	fprintf(stderr,"make_match SPAT=%lx\n",(long)spat);
#endif

    if (type == O_SUBST || type == O_NSUBST) {
	if (arg[1].arg_type != A_STAB) {
	    yyerror("Illegal lvalue");
	}
	arg[1].arg_type = A_LVAL;
    }
    return arg;
}

ARG *
cmd_to_arg(cmd)
CMD *cmd;
{
    register ARG *arg;

    arg = op_new(1);
    arg->arg_type = O_ITEM;
    arg[1].arg_type = A_CMD;
    arg[1].arg_ptr.arg_cmd = cmd;
    return arg;
}

/* Check two expressions to see if there is any identifier in common */

static int
nothing_in_common(arg1,arg2)
ARG *arg1;
ARG *arg2;
{
    static int thisexpr = 0;	/* I don't care if this wraps */

    thisexpr++;
    if (arg_common(arg1,thisexpr,1))
	return 0;	/* hit eval or do {} */
    stab_lastexpr(defstab) = thisexpr;		/* pretend to hit @_ */
    if (arg_common(arg2,thisexpr,0))
	return 0;	/* hit identifier again */
    return 1;
}

/* Recursively descend an expression and mark any identifier or check
 * it to see if it was marked already.
 */

static int
arg_common(arg,exprnum,marking)
register ARG *arg;
int exprnum;
int marking;
{
    register int i;

    if (!arg)
	return 0;
    for (i = arg->arg_len; i >= 1; i--) {
	switch (arg[i].arg_type & A_MASK) {
	case A_NULL:
	    break;
	case A_LEXPR:
	case A_EXPR:
	    if (arg_common(arg[i].arg_ptr.arg_arg,exprnum,marking))
		return 1;
	    break;
	case A_CMD:
	    return 1;		/* assume hanky panky */
	case A_STAR:
	case A_LSTAR:
	case A_STAB:
	case A_LVAL:
	case A_ARYLEN:
	case A_LARYLEN:
	    if (marking)
		stab_lastexpr(arg[i].arg_ptr.arg_stab) = exprnum;
	    else if (stab_lastexpr(arg[i].arg_ptr.arg_stab) == exprnum)
		return 1;
	    break;
	case A_DOUBLE:
	case A_BACKTICK:
	    {
		register char *s = arg[i].arg_ptr.arg_str->str_ptr;
		register char *send = s + arg[i].arg_ptr.arg_str->str_cur;
		register STAB *stab;

		while (*s) {
		    if (*s == '$' && s[1]) {
			s = scanident(s,send,tokenbuf,sizeof tokenbuf);
			stab = stabent(tokenbuf,TRUE);
			if (marking)
			    stab_lastexpr(stab) = exprnum;
			else if (stab_lastexpr(stab) == exprnum)
			    return 1;
			continue;
		    }
		    else if (*s == '\\' && s[1])
			s++;
		    s++;
		}
	    }
	    break;
	case A_SPAT:
	    if (spat_common(arg[i].arg_ptr.arg_spat,exprnum,marking))
		return 1;
	    break;
	case A_READ:
	case A_INDREAD:
	case A_GLOB:
	case A_WORD:
	case A_SINGLE:
	    break;
	}
    }
    switch (arg->arg_type) {
    case O_ARRAY:
    case O_LARRAY:
	if ((arg[1].arg_type & A_MASK) == A_STAB)
	    (void)aadd(arg[1].arg_ptr.arg_stab);
	break;
    case O_HASH:
    case O_LHASH:
	if ((arg[1].arg_type & A_MASK) == A_STAB)
	    (void)hadd(arg[1].arg_ptr.arg_stab);
	break;
    case O_EVAL:
    case O_SUBR:
    case O_DBSUBR:
	return 1;
    }
    return 0;
}

static int
spat_common(spat,exprnum,marking)
register SPAT *spat;
int exprnum;
int marking;
{
    if (spat->spat_runtime)
	if (arg_common(spat->spat_runtime,exprnum,marking))
	    return 1;
    if (spat->spat_repl) {
	if (arg_common(spat->spat_repl,exprnum,marking))
	    return 1;
    }
    return 0;
}
