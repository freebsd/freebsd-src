/*    toke.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *   "It all comes from here, the stench and the peril."  --Frodo
 */

/*
 * This file is the lexer for Perl.  It's closely linked to the
 * parser, perly.y.
 *
 * The main routine is yylex(), which returns the next token.
 */

#include "EXTERN.h"
#define PERL_IN_TOKE_C
#include "perl.h"

#define yychar	PL_yychar
#define yylval	PL_yylval

static char ident_too_long[] = "Identifier too long";

static void restore_rsfp(pTHXo_ void *f);
#ifndef PERL_NO_UTF16_FILTER
static I32 utf16_textfilter(pTHXo_ int idx, SV *sv, int maxlen);
static I32 utf16rev_textfilter(pTHXo_ int idx, SV *sv, int maxlen);
#endif

#define XFAKEBRACK 128
#define XENUMMASK 127

/*#define UTF (SvUTF8(PL_linestr) && !(PL_hints & HINT_BYTE))*/
#define UTF (PL_hints & HINT_UTF8)

/* In variables name $^X, these are the legal values for X.
 * 1999-02-27 mjd-perl-patch@plover.com */
#define isCONTROLVAR(x) (isUPPER(x) || strchr("[\\]^_?", (x)))

/* On MacOS, respect nonbreaking spaces */
#ifdef MACOS_TRADITIONAL
#define SPACE_OR_TAB(c) ((c)==' '||(c)=='\312'||(c)=='\t')
#else
#define SPACE_OR_TAB(c) ((c)==' '||(c)=='\t')
#endif

/* LEX_* are values for PL_lex_state, the state of the lexer.
 * They are arranged oddly so that the guard on the switch statement
 * can get by with a single comparison (if the compiler is smart enough).
 */

/* #define LEX_NOTPARSING		11 is done in perl.h. */

#define LEX_NORMAL		10
#define LEX_INTERPNORMAL	 9
#define LEX_INTERPCASEMOD	 8
#define LEX_INTERPPUSH		 7
#define LEX_INTERPSTART		 6
#define LEX_INTERPEND		 5
#define LEX_INTERPENDMAYBE	 4
#define LEX_INTERPCONCAT	 3
#define LEX_INTERPCONST		 2
#define LEX_FORMLINE		 1
#define LEX_KNOWNEXT		 0

#ifdef ff_next
#undef ff_next
#endif

#ifdef USE_PURE_BISON
#  ifndef YYMAXLEVEL
#    define YYMAXLEVEL 100
#  endif
YYSTYPE* yylval_pointer[YYMAXLEVEL];
int* yychar_pointer[YYMAXLEVEL];
int yyactlevel = -1;
#  undef yylval
#  undef yychar
#  define yylval (*yylval_pointer[yyactlevel])
#  define yychar (*yychar_pointer[yyactlevel])
#  define PERL_YYLEX_PARAM yylval_pointer[yyactlevel],yychar_pointer[yyactlevel]
#  undef yylex
#  define yylex()      Perl_yylex_r(aTHX_ yylval_pointer[yyactlevel],yychar_pointer[yyactlevel])
#endif

#include "keywords.h"

/* CLINE is a macro that ensures PL_copline has a sane value */

#ifdef CLINE
#undef CLINE
#endif
#define CLINE (PL_copline = (CopLINE(PL_curcop) < PL_copline ? CopLINE(PL_curcop) : PL_copline))

/*
 * Convenience functions to return different tokens and prime the
 * lexer for the next token.  They all take an argument.
 *
 * TOKEN        : generic token (used for '(', DOLSHARP, etc)
 * OPERATOR     : generic operator
 * AOPERATOR    : assignment operator
 * PREBLOCK     : beginning the block after an if, while, foreach, ...
 * PRETERMBLOCK : beginning a non-code-defining {} block (eg, hash ref)
 * PREREF       : *EXPR where EXPR is not a simple identifier
 * TERM         : expression term
 * LOOPX        : loop exiting command (goto, last, dump, etc)
 * FTST         : file test operator
 * FUN0         : zero-argument function
 * FUN1         : not used, except for not, which isn't a UNIOP
 * BOop         : bitwise or or xor
 * BAop         : bitwise and
 * SHop         : shift operator
 * PWop         : power operator
 * PMop         : pattern-matching operator
 * Aop          : addition-level operator
 * Mop          : multiplication-level operator
 * Eop          : equality-testing operator
 * Rop          : relational operator <= != gt
 *
 * Also see LOP and lop() below.
 */

#define TOKEN(retval) return (PL_bufptr = s,(int)retval)
#define OPERATOR(retval) return (PL_expect = XTERM,PL_bufptr = s,(int)retval)
#define AOPERATOR(retval) return ao((PL_expect = XTERM,PL_bufptr = s,(int)retval))
#define PREBLOCK(retval) return (PL_expect = XBLOCK,PL_bufptr = s,(int)retval)
#define PRETERMBLOCK(retval) return (PL_expect = XTERMBLOCK,PL_bufptr = s,(int)retval)
#define PREREF(retval) return (PL_expect = XREF,PL_bufptr = s,(int)retval)
#define TERM(retval) return (CLINE, PL_expect = XOPERATOR,PL_bufptr = s,(int)retval)
#define LOOPX(f) return(yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)LOOPEX)
#define FTST(f) return(yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)UNIOP)
#define FUN0(f) return(yylval.ival = f,PL_expect = XOPERATOR,PL_bufptr = s,(int)FUNC0)
#define FUN1(f) return(yylval.ival = f,PL_expect = XOPERATOR,PL_bufptr = s,(int)FUNC1)
#define BOop(f) return ao((yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)BITOROP))
#define BAop(f) return ao((yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)BITANDOP))
#define SHop(f) return ao((yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)SHIFTOP))
#define PWop(f) return ao((yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)POWOP))
#define PMop(f) return(yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)MATCHOP)
#define Aop(f) return ao((yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)ADDOP))
#define Mop(f) return ao((yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)MULOP))
#define Eop(f) return(yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)EQOP)
#define Rop(f) return(yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)RELOP)

/* This bit of chicanery makes a unary function followed by
 * a parenthesis into a function with one argument, highest precedence.
 */
#define UNI(f) return(yylval.ival = f, \
	PL_expect = XTERM, \
	PL_bufptr = s, \
	PL_last_uni = PL_oldbufptr, \
	PL_last_lop_op = f, \
	(*s == '(' || (s = skipspace(s), *s == '(') ? (int)FUNC1 : (int)UNIOP) )

#define UNIBRACK(f) return(yylval.ival = f, \
	PL_bufptr = s, \
	PL_last_uni = PL_oldbufptr, \
	(*s == '(' || (s = skipspace(s), *s == '(') ? (int)FUNC1 : (int)UNIOP) )

/* grandfather return to old style */
#define OLDLOP(f) return(yylval.ival=f,PL_expect = XTERM,PL_bufptr = s,(int)LSTOP)

/*
 * S_ao
 *
 * This subroutine detects &&= and ||= and turns an ANDAND or OROR
 * into an OP_ANDASSIGN or OP_ORASSIGN
 */

STATIC int
S_ao(pTHX_ int toketype)
{
    if (*PL_bufptr == '=') {
	PL_bufptr++;
	if (toketype == ANDAND)
	    yylval.ival = OP_ANDASSIGN;
	else if (toketype == OROR)
	    yylval.ival = OP_ORASSIGN;
	toketype = ASSIGNOP;
    }
    return toketype;
}

/*
 * S_no_op
 * When Perl expects an operator and finds something else, no_op
 * prints the warning.  It always prints "<something> found where
 * operator expected.  It prints "Missing semicolon on previous line?"
 * if the surprise occurs at the start of the line.  "do you need to
 * predeclare ..." is printed out for code like "sub bar; foo bar $x"
 * where the compiler doesn't know if foo is a method call or a function.
 * It prints "Missing operator before end of line" if there's nothing
 * after the missing operator, or "... before <...>" if there is something
 * after the missing operator.
 */

STATIC void
S_no_op(pTHX_ char *what, char *s)
{
    char *oldbp = PL_bufptr;
    bool is_first = (PL_oldbufptr == PL_linestart);

    if (!s)
	s = oldbp;
    else
	PL_bufptr = s;
    yywarn(Perl_form(aTHX_ "%s found where operator expected", what));
    if (is_first)
	Perl_warn(aTHX_ "\t(Missing semicolon on previous line?)\n");
    else if (PL_oldoldbufptr && isIDFIRST_lazy_if(PL_oldoldbufptr,UTF)) {
	char *t;
	for (t = PL_oldoldbufptr; *t && (isALNUM_lazy_if(t,UTF) || *t == ':'); t++) ;
	if (t < PL_bufptr && isSPACE(*t))
	    Perl_warn(aTHX_ "\t(Do you need to predeclare %.*s?)\n",
		t - PL_oldoldbufptr, PL_oldoldbufptr);
    }
    else {
	assert(s >= oldbp);
	Perl_warn(aTHX_ "\t(Missing operator before %.*s?)\n", s - oldbp, oldbp);
    }
    PL_bufptr = oldbp;
}

/*
 * S_missingterm
 * Complain about missing quote/regexp/heredoc terminator.
 * If it's called with (char *)NULL then it cauterizes the line buffer.
 * If we're in a delimited string and the delimiter is a control
 * character, it's reformatted into a two-char sequence like ^C.
 * This is fatal.
 */

STATIC void
S_missingterm(pTHX_ char *s)
{
    char tmpbuf[3];
    char q;
    if (s) {
	char *nl = strrchr(s,'\n');
	if (nl)
	    *nl = '\0';
    }
    else if (
#ifdef EBCDIC
	iscntrl(PL_multi_close)
#else
	PL_multi_close < 32 || PL_multi_close == 127
#endif
	) {
	*tmpbuf = '^';
	tmpbuf[1] = toCTRL(PL_multi_close);
	s = "\\n";
	tmpbuf[2] = '\0';
	s = tmpbuf;
    }
    else {
	*tmpbuf = PL_multi_close;
	tmpbuf[1] = '\0';
	s = tmpbuf;
    }
    q = strchr(s,'"') ? '\'' : '"';
    Perl_croak(aTHX_ "Can't find string terminator %c%s%c anywhere before EOF",q,s,q);
}

/*
 * Perl_deprecate
 */

void
Perl_deprecate(pTHX_ char *s)
{
    if (ckWARN(WARN_DEPRECATED))
	Perl_warner(aTHX_ WARN_DEPRECATED, "Use of %s is deprecated", s);
}

/*
 * depcom
 * Deprecate a comma-less variable list.
 */

STATIC void
S_depcom(pTHX)
{
    deprecate("comma-less variable list");
}

/*
 * experimental text filters for win32 carriage-returns, utf16-to-utf8 and
 * utf16-to-utf8-reversed.
 */

#ifdef PERL_CR_FILTER
static void
strip_return(SV *sv)
{
    register char *s = SvPVX(sv);
    register char *e = s + SvCUR(sv);
    /* outer loop optimized to do nothing if there are no CR-LFs */
    while (s < e) {
	if (*s++ == '\r' && *s == '\n') {
	    /* hit a CR-LF, need to copy the rest */
	    register char *d = s - 1;
	    *d++ = *s++;
	    while (s < e) {
		if (*s == '\r' && s[1] == '\n')
		    s++;
		*d++ = *s++;
	    }
	    SvCUR(sv) -= s - d;
	    return;
	}
    }
}

STATIC I32
S_cr_textfilter(pTHX_ int idx, SV *sv, int maxlen)
{
    I32 count = FILTER_READ(idx+1, sv, maxlen);
    if (count > 0 && !maxlen)
	strip_return(sv);
    return count;
}
#endif

/*
 * Perl_lex_start
 * Initialize variables.  Uses the Perl save_stack to save its state (for
 * recursive calls to the parser).
 */

void
Perl_lex_start(pTHX_ SV *line)
{
    char *s;
    STRLEN len;

    SAVEI32(PL_lex_dojoin);
    SAVEI32(PL_lex_brackets);
    SAVEI32(PL_lex_casemods);
    SAVEI32(PL_lex_starts);
    SAVEI32(PL_lex_state);
    SAVEVPTR(PL_lex_inpat);
    SAVEI32(PL_lex_inwhat);
    if (PL_lex_state == LEX_KNOWNEXT) {
	I32 toke = PL_nexttoke;
	while (--toke >= 0) {
	    SAVEI32(PL_nexttype[toke]);
	    SAVEVPTR(PL_nextval[toke]);
	}
	SAVEI32(PL_nexttoke);
    }
    SAVECOPLINE(PL_curcop);
    SAVEPPTR(PL_bufptr);
    SAVEPPTR(PL_bufend);
    SAVEPPTR(PL_oldbufptr);
    SAVEPPTR(PL_oldoldbufptr);
    SAVEPPTR(PL_last_lop);
    SAVEPPTR(PL_last_uni);
    SAVEPPTR(PL_linestart);
    SAVESPTR(PL_linestr);
    SAVEPPTR(PL_lex_brackstack);
    SAVEPPTR(PL_lex_casestack);
    SAVEDESTRUCTOR_X(restore_rsfp, PL_rsfp);
    SAVESPTR(PL_lex_stuff);
    SAVEI32(PL_lex_defer);
    SAVEI32(PL_sublex_info.sub_inwhat);
    SAVESPTR(PL_lex_repl);
    SAVEINT(PL_expect);
    SAVEINT(PL_lex_expect);

    PL_lex_state = LEX_NORMAL;
    PL_lex_defer = 0;
    PL_expect = XSTATE;
    PL_lex_brackets = 0;
    New(899, PL_lex_brackstack, 120, char);
    New(899, PL_lex_casestack, 12, char);
    SAVEFREEPV(PL_lex_brackstack);
    SAVEFREEPV(PL_lex_casestack);
    PL_lex_casemods = 0;
    *PL_lex_casestack = '\0';
    PL_lex_dojoin = 0;
    PL_lex_starts = 0;
    PL_lex_stuff = Nullsv;
    PL_lex_repl = Nullsv;
    PL_lex_inpat = 0;
    PL_nexttoke = 0;
    PL_lex_inwhat = 0;
    PL_sublex_info.sub_inwhat = 0;
    PL_linestr = line;
    if (SvREADONLY(PL_linestr))
	PL_linestr = sv_2mortal(newSVsv(PL_linestr));
    s = SvPV(PL_linestr, len);
    if (len && s[len-1] != ';') {
	if (!(SvFLAGS(PL_linestr) & SVs_TEMP))
	    PL_linestr = sv_2mortal(newSVsv(PL_linestr));
	sv_catpvn(PL_linestr, "\n;", 2);
    }
    SvTEMP_off(PL_linestr);
    PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = PL_linestart = SvPVX(PL_linestr);
    PL_bufend = PL_bufptr + SvCUR(PL_linestr);
    PL_last_lop = PL_last_uni = Nullch;
    SvREFCNT_dec(PL_rs);
    PL_rs = newSVpvn("\n", 1);
    PL_rsfp = 0;
}

/*
 * Perl_lex_end
 * Finalizer for lexing operations.  Must be called when the parser is
 * done with the lexer.
 */

void
Perl_lex_end(pTHX)
{
    PL_doextract = FALSE;
}

/*
 * S_incline
 * This subroutine has nothing to do with tilting, whether at windmills
 * or pinball tables.  Its name is short for "increment line".  It
 * increments the current line number in CopLINE(PL_curcop) and checks
 * to see whether the line starts with a comment of the form
 *    # line 500 "foo.pm"
 * If so, it sets the current line number and file to the values in the comment.
 */

STATIC void
S_incline(pTHX_ char *s)
{
    char *t;
    char *n;
    char *e;
    char ch;

    CopLINE_inc(PL_curcop);
    if (*s++ != '#')
	return;
    while (SPACE_OR_TAB(*s)) s++;
    if (strnEQ(s, "line", 4))
	s += 4;
    else
	return;
    if (SPACE_OR_TAB(*s))
	s++;
    else
	return;
    while (SPACE_OR_TAB(*s)) s++;
    if (!isDIGIT(*s))
	return;
    n = s;
    while (isDIGIT(*s))
	s++;
    while (SPACE_OR_TAB(*s))
	s++;
    if (*s == '"' && (t = strchr(s+1, '"'))) {
	s++;
	e = t + 1;
    }
    else {
	for (t = s; !isSPACE(*t); t++) ;
	e = t;
    }
    while (SPACE_OR_TAB(*e) || *e == '\r' || *e == '\f')
	e++;
    if (*e != '\n' && *e != '\0')
	return;		/* false alarm */

    ch = *t;
    *t = '\0';
    if (t - s > 0) {
#ifdef USE_ITHREADS
	Safefree(CopFILE(PL_curcop));
#else
	SvREFCNT_dec(CopFILEGV(PL_curcop));
#endif
	CopFILE_set(PL_curcop, s);
    }
    *t = ch;
    CopLINE_set(PL_curcop, atoi(n)-1);
}

/*
 * S_skipspace
 * Called to gobble the appropriate amount and type of whitespace.
 * Skips comments as well.
 */

STATIC char *
S_skipspace(pTHX_ register char *s)
{
    if (PL_lex_formbrack && PL_lex_brackets <= PL_lex_formbrack) {
	while (s < PL_bufend && SPACE_OR_TAB(*s))
	    s++;
	return s;
    }
    for (;;) {
	STRLEN prevlen;
	SSize_t oldprevlen, oldoldprevlen;
	SSize_t oldloplen, oldunilen;
	while (s < PL_bufend && isSPACE(*s)) {
	    if (*s++ == '\n' && PL_in_eval && !PL_rsfp)
		incline(s);
	}

	/* comment */
	if (s < PL_bufend && *s == '#') {
	    while (s < PL_bufend && *s != '\n')
		s++;
	    if (s < PL_bufend) {
		s++;
		if (PL_in_eval && !PL_rsfp) {
		    incline(s);
		    continue;
		}
	    }
	}

	/* only continue to recharge the buffer if we're at the end
	 * of the buffer, we're not reading from a source filter, and
	 * we're in normal lexing mode
	 */
	if (s < PL_bufend || !PL_rsfp || PL_sublex_info.sub_inwhat ||
		PL_lex_state == LEX_FORMLINE)
	    return s;

	/* try to recharge the buffer */
	if ((s = filter_gets(PL_linestr, PL_rsfp,
			     (prevlen = SvCUR(PL_linestr)))) == Nullch)
	{
	    /* end of file.  Add on the -p or -n magic */
	    if (PL_minus_n || PL_minus_p) {
		sv_setpv(PL_linestr,PL_minus_p ?
			 ";}continue{print or die qq(-p destination: $!\\n)" :
			 "");
		sv_catpv(PL_linestr,";}");
		PL_minus_n = PL_minus_p = 0;
	    }
	    else
		sv_setpv(PL_linestr,";");

	    /* reset variables for next time we lex */
	    PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = s = PL_linestart
		= SvPVX(PL_linestr);
	    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	    PL_last_lop = PL_last_uni = Nullch;

	    /* Close the filehandle.  Could be from -P preprocessor,
	     * STDIN, or a regular file.  If we were reading code from
	     * STDIN (because the commandline held no -e or filename)
	     * then we don't close it, we reset it so the code can
	     * read from STDIN too.
	     */

	    if (PL_preprocess && !PL_in_eval)
		(void)PerlProc_pclose(PL_rsfp);
	    else if ((PerlIO*)PL_rsfp == PerlIO_stdin())
		PerlIO_clearerr(PL_rsfp);
	    else
		(void)PerlIO_close(PL_rsfp);
	    PL_rsfp = Nullfp;
	    return s;
	}

	/* not at end of file, so we only read another line */
	/* make corresponding updates to old pointers, for yyerror() */
	oldprevlen = PL_oldbufptr - PL_bufend;
	oldoldprevlen = PL_oldoldbufptr - PL_bufend;
	if (PL_last_uni)
	    oldunilen = PL_last_uni - PL_bufend;
	if (PL_last_lop)
	    oldloplen = PL_last_lop - PL_bufend;
	PL_linestart = PL_bufptr = s + prevlen;
	PL_bufend = s + SvCUR(PL_linestr);
	s = PL_bufptr;
	PL_oldbufptr = s + oldprevlen;
	PL_oldoldbufptr = s + oldoldprevlen;
	if (PL_last_uni)
	    PL_last_uni = s + oldunilen;
	if (PL_last_lop)
	    PL_last_lop = s + oldloplen;
	incline(s);

	/* debugger active and we're not compiling the debugger code,
	 * so store the line into the debugger's array of lines
	 */
	if (PERLDB_LINE && PL_curstash != PL_debstash) {
	    SV *sv = NEWSV(85,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setpvn(sv,PL_bufptr,PL_bufend-PL_bufptr);
	    av_store(CopFILEAV(PL_curcop),(I32)CopLINE(PL_curcop),sv);
	}
    }
}

/*
 * S_check_uni
 * Check the unary operators to ensure there's no ambiguity in how they're
 * used.  An ambiguous piece of code would be:
 *     rand + 5
 * This doesn't mean rand() + 5.  Because rand() is a unary operator,
 * the +5 is its argument.
 */

STATIC void
S_check_uni(pTHX)
{
    char *s;
    char *t;

    if (PL_oldoldbufptr != PL_last_uni)
	return;
    while (isSPACE(*PL_last_uni))
	PL_last_uni++;
    for (s = PL_last_uni; isALNUM_lazy_if(s,UTF) || *s == '-'; s++) ;
    if ((t = strchr(s, '(')) && t < PL_bufptr)
	return;
    if (ckWARN_d(WARN_AMBIGUOUS)){
        char ch = *s;
        *s = '\0';
        Perl_warner(aTHX_ WARN_AMBIGUOUS,
		   "Warning: Use of \"%s\" without parens is ambiguous",
		   PL_last_uni);
        *s = ch;
    }
}

/* workaround to replace the UNI() macro with a function.  Only the
 * hints/uts.sh file mentions this.  Other comments elsewhere in the
 * source indicate Microport Unix might need it too.
 */

#ifdef CRIPPLED_CC

#undef UNI
#define UNI(f) return uni(f,s)

STATIC int
S_uni(pTHX_ I32 f, char *s)
{
    yylval.ival = f;
    PL_expect = XTERM;
    PL_bufptr = s;
    PL_last_uni = PL_oldbufptr;
    PL_last_lop_op = f;
    if (*s == '(')
	return FUNC1;
    s = skipspace(s);
    if (*s == '(')
	return FUNC1;
    else
	return UNIOP;
}

#endif /* CRIPPLED_CC */

/*
 * LOP : macro to build a list operator.  Its behaviour has been replaced
 * with a subroutine, S_lop() for which LOP is just another name.
 */

#define LOP(f,x) return lop(f,x,s)

/*
 * S_lop
 * Build a list operator (or something that might be one).  The rules:
 *  - if we have a next token, then it's a list operator [why?]
 *  - if the next thing is an opening paren, then it's a function
 *  - else it's a list operator
 */

STATIC I32
S_lop(pTHX_ I32 f, int x, char *s)
{
    yylval.ival = f;
    CLINE;
    PL_expect = x;
    PL_bufptr = s;
    PL_last_lop = PL_oldbufptr;
    PL_last_lop_op = f;
    if (PL_nexttoke)
	return LSTOP;
    if (*s == '(')
	return FUNC;
    s = skipspace(s);
    if (*s == '(')
	return FUNC;
    else
	return LSTOP;
}

/*
 * S_force_next
 * When the lexer realizes it knows the next token (for instance,
 * it is reordering tokens for the parser) then it can call S_force_next
 * to know what token to return the next time the lexer is called.  Caller
 * will need to set PL_nextval[], and possibly PL_expect to ensure the lexer
 * handles the token correctly.
 */

STATIC void
S_force_next(pTHX_ I32 type)
{
    PL_nexttype[PL_nexttoke] = type;
    PL_nexttoke++;
    if (PL_lex_state != LEX_KNOWNEXT) {
	PL_lex_defer = PL_lex_state;
	PL_lex_expect = PL_expect;
	PL_lex_state = LEX_KNOWNEXT;
    }
}

/*
 * S_force_word
 * When the lexer knows the next thing is a word (for instance, it has
 * just seen -> and it knows that the next char is a word char, then
 * it calls S_force_word to stick the next word into the PL_next lookahead.
 *
 * Arguments:
 *   char *start : buffer position (must be within PL_linestr)
 *   int token   : PL_next will be this type of bare word (e.g., METHOD,WORD)
 *   int check_keyword : if true, Perl checks to make sure the word isn't
 *       a keyword (do this if the word is a label, e.g. goto FOO)
 *   int allow_pack : if true, : characters will also be allowed (require,
 *       use, etc. do this)
 *   int allow_initial_tick : used by the "sub" lexer only.
 */

STATIC char *
S_force_word(pTHX_ register char *start, int token, int check_keyword, int allow_pack, int allow_initial_tick)
{
    register char *s;
    STRLEN len;

    start = skipspace(start);
    s = start;
    if (isIDFIRST_lazy_if(s,UTF) ||
	(allow_pack && *s == ':') ||
	(allow_initial_tick && *s == '\'') )
    {
	s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, allow_pack, &len);
	if (check_keyword && keyword(PL_tokenbuf, len))
	    return start;
	if (token == METHOD) {
	    s = skipspace(s);
	    if (*s == '(')
		PL_expect = XTERM;
	    else {
		PL_expect = XOPERATOR;
	    }
	}
	PL_nextval[PL_nexttoke].opval = (OP*)newSVOP(OP_CONST,0, newSVpv(PL_tokenbuf,0));
	PL_nextval[PL_nexttoke].opval->op_private |= OPpCONST_BARE;
	force_next(token);
    }
    return s;
}

/*
 * S_force_ident
 * Called when the lexer wants $foo *foo &foo etc, but the program
 * text only contains the "foo" portion.  The first argument is a pointer
 * to the "foo", and the second argument is the type symbol to prefix.
 * Forces the next token to be a "WORD".
 * Creates the symbol if it didn't already exist (via gv_fetchpv()).
 */

STATIC void
S_force_ident(pTHX_ register char *s, int kind)
{
    if (s && *s) {
	OP* o = (OP*)newSVOP(OP_CONST, 0, newSVpv(s,0));
	PL_nextval[PL_nexttoke].opval = o;
	force_next(WORD);
	if (kind) {
	    o->op_private = OPpCONST_ENTERED;
	    /* XXX see note in pp_entereval() for why we forgo typo
	       warnings if the symbol must be introduced in an eval.
	       GSAR 96-10-12 */
	    gv_fetchpv(s, PL_in_eval ? (GV_ADDMULTI | GV_ADDINEVAL) : TRUE,
		kind == '$' ? SVt_PV :
		kind == '@' ? SVt_PVAV :
		kind == '%' ? SVt_PVHV :
			      SVt_PVGV
		);
	}
    }
}

NV
Perl_str_to_version(pTHX_ SV *sv)
{
    NV retval = 0.0;
    NV nshift = 1.0;
    STRLEN len;
    char *start = SvPVx(sv,len);
    bool utf = SvUTF8(sv) ? TRUE : FALSE;
    char *end = start + len;
    while (start < end) {
	STRLEN skip;
	UV n;
	if (utf)
	    n = utf8_to_uv((U8*)start, len, &skip, 0);
	else {
	    n = *(U8*)start;
	    skip = 1;
	}
	retval += ((NV)n)/nshift;
	start += skip;
	nshift *= 1000;
    }
    return retval;
}

/*
 * S_force_version
 * Forces the next token to be a version number.
 */

STATIC char *
S_force_version(pTHX_ char *s)
{
    OP *version = Nullop;
    char *d;

    s = skipspace(s);

    d = s;
    if (*d == 'v')
	d++;
    if (isDIGIT(*d)) {
        for (; isDIGIT(*d) || *d == '_' || *d == '.'; d++);
        if (*d == ';' || isSPACE(*d) || *d == '}' || !*d) {
	    SV *ver;
            s = scan_num(s, &yylval);
            version = yylval.opval;
	    ver = cSVOPx(version)->op_sv;
	    if (SvPOK(ver) && !SvNIOK(ver)) {
		(void)SvUPGRADE(ver, SVt_PVNV);
		SvNVX(ver) = str_to_version(ver);
		SvNOK_on(ver);		/* hint that it is a version */
	    }
        }
    }

    /* NOTE: The parser sees the package name and the VERSION swapped */
    PL_nextval[PL_nexttoke].opval = version;
    force_next(WORD);

    return (s);
}

/*
 * S_tokeq
 * Tokenize a quoted string passed in as an SV.  It finds the next
 * chunk, up to end of string or a backslash.  It may make a new
 * SV containing that chunk (if HINT_NEW_STRING is on).  It also
 * turns \\ into \.
 */

STATIC SV *
S_tokeq(pTHX_ SV *sv)
{
    register char *s;
    register char *send;
    register char *d;
    STRLEN len = 0;
    SV *pv = sv;

    if (!SvLEN(sv))
	goto finish;

    s = SvPV_force(sv, len);
    if (SvTYPE(sv) >= SVt_PVIV && SvIVX(sv) == -1)
	goto finish;
    send = s + len;
    while (s < send && *s != '\\')
	s++;
    if (s == send)
	goto finish;
    d = s;
    if ( PL_hints & HINT_NEW_STRING )
	pv = sv_2mortal(newSVpvn(SvPVX(pv), len));
    while (s < send) {
	if (*s == '\\') {
	    if (s + 1 < send && (s[1] == '\\'))
		s++;		/* all that, just for this */
	}
	*d++ = *s++;
    }
    *d = '\0';
    SvCUR_set(sv, d - SvPVX(sv));
  finish:
    if ( PL_hints & HINT_NEW_STRING )
       return new_constant(NULL, 0, "q", sv, pv, "q");
    return sv;
}

/*
 * Now come three functions related to double-quote context,
 * S_sublex_start, S_sublex_push, and S_sublex_done.  They're used when
 * converting things like "\u\Lgnat" into ucfirst(lc("gnat")).  They
 * interact with PL_lex_state, and create fake ( ... ) argument lists
 * to handle functions and concatenation.
 * They assume that whoever calls them will be setting up a fake
 * join call, because each subthing puts a ',' after it.  This lets
 *   "lower \luPpEr"
 * become
 *  join($, , 'lower ', lcfirst( 'uPpEr', ) ,)
 *
 * (I'm not sure whether the spurious commas at the end of lcfirst's
 * arguments and join's arguments are created or not).
 */

/*
 * S_sublex_start
 * Assumes that yylval.ival is the op we're creating (e.g. OP_LCFIRST).
 *
 * Pattern matching will set PL_lex_op to the pattern-matching op to
 * make (we return THING if yylval.ival is OP_NULL, PMFUNC otherwise).
 *
 * OP_CONST and OP_READLINE are easy--just make the new op and return.
 *
 * Everything else becomes a FUNC.
 *
 * Sets PL_lex_state to LEX_INTERPPUSH unless (ival was OP_NULL or we
 * had an OP_CONST or OP_READLINE).  This just sets us up for a
 * call to S_sublex_push().
 */

STATIC I32
S_sublex_start(pTHX)
{
    register I32 op_type = yylval.ival;

    if (op_type == OP_NULL) {
	yylval.opval = PL_lex_op;
	PL_lex_op = Nullop;
	return THING;
    }
    if (op_type == OP_CONST || op_type == OP_READLINE) {
	SV *sv = tokeq(PL_lex_stuff);

	if (SvTYPE(sv) == SVt_PVIV) {
	    /* Overloaded constants, nothing fancy: Convert to SVt_PV: */
	    STRLEN len;
	    char *p;
	    SV *nsv;

	    p = SvPV(sv, len);
	    nsv = newSVpvn(p, len);
	    if (SvUTF8(sv))
		SvUTF8_on(nsv);
	    SvREFCNT_dec(sv);
	    sv = nsv;
	}
	yylval.opval = (OP*)newSVOP(op_type, 0, sv);
	PL_lex_stuff = Nullsv;
	return THING;
    }

    PL_sublex_info.super_state = PL_lex_state;
    PL_sublex_info.sub_inwhat = op_type;
    PL_sublex_info.sub_op = PL_lex_op;
    PL_lex_state = LEX_INTERPPUSH;

    PL_expect = XTERM;
    if (PL_lex_op) {
	yylval.opval = PL_lex_op;
	PL_lex_op = Nullop;
	return PMFUNC;
    }
    else
	return FUNC;
}

/*
 * S_sublex_push
 * Create a new scope to save the lexing state.  The scope will be
 * ended in S_sublex_done.  Returns a '(', starting the function arguments
 * to the uc, lc, etc. found before.
 * Sets PL_lex_state to LEX_INTERPCONCAT.
 */

STATIC I32
S_sublex_push(pTHX)
{
    ENTER;

    PL_lex_state = PL_sublex_info.super_state;
    SAVEI32(PL_lex_dojoin);
    SAVEI32(PL_lex_brackets);
    SAVEI32(PL_lex_casemods);
    SAVEI32(PL_lex_starts);
    SAVEI32(PL_lex_state);
    SAVEVPTR(PL_lex_inpat);
    SAVEI32(PL_lex_inwhat);
    SAVECOPLINE(PL_curcop);
    SAVEPPTR(PL_bufptr);
    SAVEPPTR(PL_oldbufptr);
    SAVEPPTR(PL_oldoldbufptr);
    SAVEPPTR(PL_last_lop);
    SAVEPPTR(PL_last_uni);
    SAVEPPTR(PL_linestart);
    SAVESPTR(PL_linestr);
    SAVEPPTR(PL_lex_brackstack);
    SAVEPPTR(PL_lex_casestack);

    PL_linestr = PL_lex_stuff;
    PL_lex_stuff = Nullsv;

    PL_bufend = PL_bufptr = PL_oldbufptr = PL_oldoldbufptr = PL_linestart
	= SvPVX(PL_linestr);
    PL_bufend += SvCUR(PL_linestr);
    PL_last_lop = PL_last_uni = Nullch;
    SAVEFREESV(PL_linestr);

    PL_lex_dojoin = FALSE;
    PL_lex_brackets = 0;
    New(899, PL_lex_brackstack, 120, char);
    New(899, PL_lex_casestack, 12, char);
    SAVEFREEPV(PL_lex_brackstack);
    SAVEFREEPV(PL_lex_casestack);
    PL_lex_casemods = 0;
    *PL_lex_casestack = '\0';
    PL_lex_starts = 0;
    PL_lex_state = LEX_INTERPCONCAT;
    CopLINE_set(PL_curcop, PL_multi_start);

    PL_lex_inwhat = PL_sublex_info.sub_inwhat;
    if (PL_lex_inwhat == OP_MATCH || PL_lex_inwhat == OP_QR || PL_lex_inwhat == OP_SUBST)
	PL_lex_inpat = PL_sublex_info.sub_op;
    else
	PL_lex_inpat = Nullop;

    return '(';
}

/*
 * S_sublex_done
 * Restores lexer state after a S_sublex_push.
 */

STATIC I32
S_sublex_done(pTHX)
{
    if (!PL_lex_starts++) {
	SV *sv = newSVpvn("",0);
	if (SvUTF8(PL_linestr))
	    SvUTF8_on(sv);
	PL_expect = XOPERATOR;
	yylval.opval = (OP*)newSVOP(OP_CONST, 0, sv);
	return THING;
    }

    if (PL_lex_casemods) {		/* oops, we've got some unbalanced parens */
	PL_lex_state = LEX_INTERPCASEMOD;
	return yylex();
    }

    /* Is there a right-hand side to take care of? (s//RHS/ or tr//RHS/) */
    if (PL_lex_repl && (PL_lex_inwhat == OP_SUBST || PL_lex_inwhat == OP_TRANS)) {
	PL_linestr = PL_lex_repl;
	PL_lex_inpat = 0;
	PL_bufend = PL_bufptr = PL_oldbufptr = PL_oldoldbufptr = PL_linestart = SvPVX(PL_linestr);
	PL_bufend += SvCUR(PL_linestr);
	PL_last_lop = PL_last_uni = Nullch;
	SAVEFREESV(PL_linestr);
	PL_lex_dojoin = FALSE;
	PL_lex_brackets = 0;
	PL_lex_casemods = 0;
	*PL_lex_casestack = '\0';
	PL_lex_starts = 0;
	if (SvEVALED(PL_lex_repl)) {
	    PL_lex_state = LEX_INTERPNORMAL;
	    PL_lex_starts++;
	    /*	we don't clear PL_lex_repl here, so that we can check later
		whether this is an evalled subst; that means we rely on the
		logic to ensure sublex_done() is called again only via the
		branch (in yylex()) that clears PL_lex_repl, else we'll loop */
	}
	else {
	    PL_lex_state = LEX_INTERPCONCAT;
	    PL_lex_repl = Nullsv;
	}
	return ',';
    }
    else {
	LEAVE;
	PL_bufend = SvPVX(PL_linestr);
	PL_bufend += SvCUR(PL_linestr);
	PL_expect = XOPERATOR;
	PL_sublex_info.sub_inwhat = 0;
	return ')';
    }
}

/*
  scan_const

  Extracts a pattern, double-quoted string, or transliteration.  This
  is terrifying code.

  It looks at lex_inwhat and PL_lex_inpat to find out whether it's
  processing a pattern (PL_lex_inpat is true), a transliteration
  (lex_inwhat & OP_TRANS is true), or a double-quoted string.

  Returns a pointer to the character scanned up to. Iff this is
  advanced from the start pointer supplied (ie if anything was
  successfully parsed), will leave an OP for the substring scanned
  in yylval. Caller must intuit reason for not parsing further
  by looking at the next characters herself.

  In patterns:
    backslashes:
      double-quoted style: \r and \n
      regexp special ones: \D \s
      constants: \x3
      backrefs: \1 (deprecated in substitution replacements)
      case and quoting: \U \Q \E
    stops on @ and $, but not for $ as tail anchor

  In transliterations:
    characters are VERY literal, except for - not at the start or end
    of the string, which indicates a range.  scan_const expands the
    range to the full set of intermediate characters.

  In double-quoted strings:
    backslashes:
      double-quoted style: \r and \n
      constants: \x3
      backrefs: \1 (deprecated)
      case and quoting: \U \Q \E
    stops on @ and $

  scan_const does *not* construct ops to handle interpolated strings.
  It stops processing as soon as it finds an embedded $ or @ variable
  and leaves it to the caller to work out what's going on.

  @ in pattern could be: @foo, @{foo}, @$foo, @'foo, @:foo.

  $ in pattern could be $foo or could be tail anchor.  Assumption:
  it's a tail anchor if $ is the last thing in the string, or if it's
  followed by one of ")| \n\t"

  \1 (backreferences) are turned into $1

  The structure of the code is
      while (there's a character to process) {
          handle transliteration ranges
	  skip regexp comments
	  skip # initiated comments in //x patterns
	  check for embedded @foo
	  check for embedded scalars
	  if (backslash) {
	      leave intact backslashes from leave (below)
	      deprecate \1 in strings and sub replacements
	      handle string-changing backslashes \l \U \Q \E, etc.
	      switch (what was escaped) {
	          handle - in a transliteration (becomes a literal -)
		  handle \132 octal characters
		  handle 0x15 hex characters
		  handle \cV (control V)
		  handle printf backslashes (\f, \r, \n, etc)
	      } (end switch)
	  } (end if backslash)
    } (end while character to read)
		
*/

STATIC char *
S_scan_const(pTHX_ char *start)
{
    register char *send = PL_bufend;		/* end of the constant */
    SV *sv = NEWSV(93, send - start);		/* sv for the constant */
    register char *s = start;			/* start of the constant */
    register char *d = SvPVX(sv);		/* destination for copies */
    bool dorange = FALSE;			/* are we in a translit range? */
    bool has_utf8 = FALSE;			/* embedded \x{} */
    UV uv;

    I32 utf = (PL_lex_inwhat == OP_TRANS && PL_sublex_info.sub_op)
	? (PL_sublex_info.sub_op->op_private & (OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF))
	: UTF;
    I32 this_utf8 = (PL_lex_inwhat == OP_TRANS && PL_sublex_info.sub_op)
	? (PL_sublex_info.sub_op->op_private & (PL_lex_repl ?
						OPpTRANS_FROM_UTF : OPpTRANS_TO_UTF))
	: UTF;
    const char *leaveit =	/* set of acceptably-backslashed characters */
	PL_lex_inpat
	    ? "\\.^$@AGZdDwWsSbBpPXC+*?|()-nrtfeaxcz0123456789[{]} \t\n\r\f\v#"
	    : "";

    while (s < send || dorange) {
        /* get transliterations out of the way (they're most literal) */
	if (PL_lex_inwhat == OP_TRANS) {
	    /* expand a range A-Z to the full set of characters.  AIE! */
	    if (dorange) {
		I32 i;				/* current expanded character */
		I32 min;			/* first character in range */
		I32 max;			/* last character in range */

		i = d - SvPVX(sv);		/* remember current offset */
		SvGROW(sv, SvLEN(sv) + 256);	/* never more than 256 chars in a range */
		d = SvPVX(sv) + i;		/* refresh d after realloc */
		d -= 2;				/* eat the first char and the - */

		min = (U8)*d;			/* first char in range */
		max = (U8)d[1];			/* last char in range  */

#ifndef ASCIIish
		if ((isLOWER(min) && isLOWER(max)) ||
		    (isUPPER(min) && isUPPER(max))) {
		    if (isLOWER(min)) {
			for (i = min; i <= max; i++)
			    if (isLOWER(i))
				*d++ = i;
		    } else {
			for (i = min; i <= max; i++)
			    if (isUPPER(i))
				*d++ = i;
		    }
		}
		else
#endif
		    for (i = min; i <= max; i++)
			*d++ = i;

		/* mark the range as done, and continue */
		dorange = FALSE;
		continue;
	    }

	    /* range begins (ignore - as first or last char) */
	    else if (*s == '-' && s+1 < send  && s != start) {
		if (utf) {
		    *d++ = (char)0xff;	/* use illegal utf8 byte--see pmtrans */
		    s++;
		    continue;
		}
		dorange = TRUE;
		s++;
	    }
	}

	/* if we get here, we're not doing a transliteration */

	/* skip for regexp comments /(?#comment)/ and code /(?{code})/,
	   except for the last char, which will be done separately. */
	else if (*s == '(' && PL_lex_inpat && s[1] == '?') {
	    if (s[2] == '#') {
		while (s < send && *s != ')')
		    *d++ = *s++;
	    }
	    else if (s[2] == '{' /* This should match regcomp.c */
		     || ((s[2] == 'p' || s[2] == '?') && s[3] == '{'))
	    {
		I32 count = 1;
		char *regparse = s + (s[2] == '{' ? 3 : 4);
		char c;

		while (count && (c = *regparse)) {
		    if (c == '\\' && regparse[1])
			regparse++;
		    else if (c == '{')
			count++;
		    else if (c == '}')
			count--;
		    regparse++;
		}
		if (*regparse != ')') {
		    regparse--;		/* Leave one char for continuation. */
		    yyerror("Sequence (?{...}) not terminated or not {}-balanced");
		}
		while (s < regparse)
		    *d++ = *s++;
	    }
	}

	/* likewise skip #-initiated comments in //x patterns */
	else if (*s == '#' && PL_lex_inpat &&
	  ((PMOP*)PL_lex_inpat)->op_pmflags & PMf_EXTENDED) {
	    while (s+1 < send && *s != '\n')
		*d++ = *s++;
	}

	/* check for embedded arrays (@foo, @:foo, @'foo, @{foo}, @$foo) */
	else if (*s == '@' && s[1]
		 && (isALNUM_lazy_if(s+1,UTF) || strchr(":'{$", s[1])))
	    break;

	/* check for embedded scalars.  only stop if we're sure it's a
	   variable.
        */
	else if (*s == '$') {
	    if (!PL_lex_inpat)	/* not a regexp, so $ must be var */
		break;
	    if (s + 1 < send && !strchr("()| \n\t", s[1]))
		break;		/* in regexp, $ might be tail anchor */
	}

	/* backslashes */
	if (*s == '\\' && s+1 < send) {
	    bool to_be_utf8 = FALSE;

	    s++;

	    /* some backslashes we leave behind */
	    if (*leaveit && *s && strchr(leaveit, *s)) {
		*d++ = '\\';
		*d++ = *s++;
		continue;
	    }

	    /* deprecate \1 in strings and substitution replacements */
	    if (PL_lex_inwhat == OP_SUBST && !PL_lex_inpat &&
		isDIGIT(*s) && *s != '0' && !isDIGIT(s[1]))
	    {
		if (ckWARN(WARN_SYNTAX))
		    Perl_warner(aTHX_ WARN_SYNTAX, "\\%c better written as $%c", *s, *s);
		*--s = '$';
		break;
	    }

	    /* string-change backslash escapes */
	    if (PL_lex_inwhat != OP_TRANS && *s && strchr("lLuUEQ", *s)) {
		--s;
		break;
	    }

	    /* if we get here, it's either a quoted -, or a digit */
	    switch (*s) {

	    /* quoted - in transliterations */
	    case '-':
		if (PL_lex_inwhat == OP_TRANS) {
		    *d++ = *s++;
		    continue;
		}
		/* FALL THROUGH */
	    default:
	        {
		    if (ckWARN(WARN_MISC) && isALPHA(*s))
			Perl_warner(aTHX_ WARN_MISC, 
			       "Unrecognized escape \\%c passed through",
			       *s);
		    /* default action is to copy the quoted character */
		    goto default_action;
		}

	    /* \132 indicates an octal constant */
	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
		{
		    STRLEN len = 0;	/* disallow underscores */
		    uv = (UV)scan_oct(s, 3, &len);
		    s += len;
		}
		goto NUM_ESCAPE_INSERT;

	    /* \x24 indicates a hex constant */
	    case 'x':
		++s;
		if (*s == '{') {
		    char* e = strchr(s, '}');
		    if (!e) {
			yyerror("Missing right brace on \\x{}");
			e = s;
		    }
		    else {
			STRLEN len = 1;		/* allow underscores */
			uv = (UV)scan_hex(s + 1, e - s - 1, &len);
			to_be_utf8 = TRUE;
		    }
		    s = e + 1;
		}
		else {
		    {
			STRLEN len = 0;		/* disallow underscores */
			uv = (UV)scan_hex(s, 2, &len);
			s += len;
		    }
		}

	      NUM_ESCAPE_INSERT:
		/* Insert oct or hex escaped character.
		 * There will always enough room in sv since such
		 * escapes will be longer than any UT-F8 sequence
		 * they can end up as. */

		/* This spot is wrong for EBCDIC.  Characters like
		 * the lowercase letters and digits are >127 in EBCDIC,
		 * so here they would need to be mapped to the Unicode
		 * repertoire.   --jhi */
		
		if (uv > 127) {
		    if (!has_utf8 && (to_be_utf8 || uv > 255)) {
		        /* Might need to recode whatever we have
			 * accumulated so far if it contains any
			 * hibit chars.
			 *
			 * (Can't we keep track of that and avoid
			 *  this rescan? --jhi)
			 */
		        int hicount = 0;
			char *c;

			for (c = SvPVX(sv); c < d; c++) {
			    if (UTF8_IS_CONTINUED(*c))
			        hicount++;
			}
			if (hicount) {
			    char *old_pvx = SvPVX(sv);
			    char *src, *dst;
			  
			    d = SvGROW(sv,
				       SvCUR(sv) + hicount + 1) +
				         (d - old_pvx);

			    src = d - 1;
			    d += hicount;
			    dst = d - 1;

			    while (src < dst) {
			        if (UTF8_IS_CONTINUED(*src)) {
 				    *dst-- = UTF8_EIGHT_BIT_LO(*src);
 				    *dst-- = UTF8_EIGHT_BIT_HI(*src--);
			        }
			        else {
				    *dst-- = *src--;
			        }
			    }
                        }
                    }

		    if (to_be_utf8 || has_utf8 || uv > 255) {
		        d = (char*)uv_to_utf8((U8*)d, uv);
			has_utf8 = TRUE;
			if (PL_lex_inwhat == OP_TRANS &&
			    PL_sublex_info.sub_op) {
			    PL_sublex_info.sub_op->op_private |=
				(PL_lex_repl ? OPpTRANS_FROM_UTF
					     : OPpTRANS_TO_UTF);
			    utf = TRUE;
			}
                    }
		    else {
		        *d++ = (char)uv;
		    }
		}
		else {
		    *d++ = (char)uv;
		}
		continue;

 	    /* \N{latin small letter a} is a named character */
 	    case 'N':
 		++s;
 		if (*s == '{') {
 		    char* e = strchr(s, '}');
 		    SV *res;
 		    STRLEN len;
 		    char *str;

 		    if (!e) {
			yyerror("Missing right brace on \\N{}");
			e = s - 1;
			goto cont_scan;
		    }
		    res = newSVpvn(s + 1, e - s - 1);
		    res = new_constant( Nullch, 0, "charnames",
					res, Nullsv, "\\N{...}" );
		    if (has_utf8)
			sv_utf8_upgrade(res);
		    str = SvPV(res,len);
		    if (!has_utf8 && SvUTF8(res)) {
			char *ostart = SvPVX(sv);
			SvCUR_set(sv, d - ostart);
			SvPOK_on(sv);
			*d = '\0';
			sv_utf8_upgrade(sv);
			/* this just broke our allocation above... */
			SvGROW(sv, send - start);
			d = SvPVX(sv) + SvCUR(sv);
			has_utf8 = TRUE;
		    }
		    if (len > e - s + 4) {
			char *odest = SvPVX(sv);

			SvGROW(sv, (SvCUR(sv) + len - (e - s + 4)));
			d = SvPVX(sv) + (d - odest);
		    }
		    Copy(str, d, len, char);
		    d += len;
		    SvREFCNT_dec(res);
		  cont_scan:
		    s = e + 1;
		}
		else
		    yyerror("Missing braces on \\N{}");
		continue;

	    /* \c is a control character */
	    case 'c':
		s++;
#ifdef EBCDIC
		*d = *s++;
		if (isLOWER(*d))
		   *d = toUPPER(*d);
		*d = toCTRL(*d);
		d++;
#else
		{
		    U8 c = *s++;
		    *d++ = toCTRL(c);
		}
#endif
		continue;

	    /* printf-style backslashes, formfeeds, newlines, etc */
	    case 'b':
		*d++ = '\b';
		break;
	    case 'n':
		*d++ = '\n';
		break;
	    case 'r':
		*d++ = '\r';
		break;
	    case 'f':
		*d++ = '\f';
		break;
	    case 't':
		*d++ = '\t';
		break;
#ifdef EBCDIC
	    case 'e':
		*d++ = '\047';  /* CP 1047 */
		break;
	    case 'a':
		*d++ = '\057';  /* CP 1047 */
		break;
#else
	    case 'e':
		*d++ = '\033';
		break;
	    case 'a':
		*d++ = '\007';
		break;
#endif
	    } /* end switch */

	    s++;
	    continue;
	} /* end if (backslash) */

    default_action:
       if (UTF8_IS_CONTINUED(*s) && (this_utf8 || has_utf8)) {
           STRLEN len = (STRLEN) -1;
           UV uv;
           if (this_utf8) {
               uv = utf8_to_uv((U8*)s, send - s, &len, 0);
           }
           if (len == (STRLEN)-1) {
               /* Illegal UTF8 (a high-bit byte), make it valid. */
               char *old_pvx = SvPVX(sv);
               /* need space for one extra char (NOTE: SvCUR() not set here) */
               d = SvGROW(sv, SvLEN(sv) + 1) + (d - old_pvx);
               d = (char*)uv_to_utf8((U8*)d, (U8)*s++);
           }
           else {
               while (len--)
                   *d++ = *s++;
           }
           has_utf8 = TRUE;
	   if (PL_lex_inwhat == OP_TRANS && PL_sublex_info.sub_op) {
	       PL_sublex_info.sub_op->op_private |=
		   (PL_lex_repl ? OPpTRANS_FROM_UTF : OPpTRANS_TO_UTF);
	       utf = TRUE;
	   }
           continue;
       }

       *d++ = *s++;
    } /* while loop to process each character */

    /* terminate the string and set up the sv */
    *d = '\0';
    SvCUR_set(sv, d - SvPVX(sv));
    SvPOK_on(sv);
    if (has_utf8)
	SvUTF8_on(sv);

    /* shrink the sv if we allocated more than we used */
    if (SvCUR(sv) + 5 < SvLEN(sv)) {
	SvLEN_set(sv, SvCUR(sv) + 1);
	Renew(SvPVX(sv), SvLEN(sv), char);
    }

    /* return the substring (via yylval) only if we parsed anything */
    if (s > PL_bufptr) {
	if ( PL_hints & ( PL_lex_inpat ? HINT_NEW_RE : HINT_NEW_STRING ) )
	    sv = new_constant(start, s - start, (PL_lex_inpat ? "qr" : "q"),
			      sv, Nullsv,
			      ( PL_lex_inwhat == OP_TRANS
				? "tr"
				: ( (PL_lex_inwhat == OP_SUBST && !PL_lex_inpat)
				    ? "s"
				    : "qq")));
	yylval.opval = (OP*)newSVOP(OP_CONST, 0, sv);
    } else
	SvREFCNT_dec(sv);
    return s;
}

/* S_intuit_more
 * Returns TRUE if there's more to the expression (e.g., a subscript),
 * FALSE otherwise.
 *
 * It deals with "$foo[3]" and /$foo[3]/ and /$foo[0123456789$]+/
 *
 * ->[ and ->{ return TRUE
 * { and [ outside a pattern are always subscripts, so return TRUE
 * if we're outside a pattern and it's not { or [, then return FALSE
 * if we're in a pattern and the first char is a {
 *   {4,5} (any digits around the comma) returns FALSE
 * if we're in a pattern and the first char is a [
 *   [] returns FALSE
 *   [SOMETHING] has a funky algorithm to decide whether it's a
 *      character class or not.  It has to deal with things like
 *      /$foo[-3]/ and /$foo[$bar]/ as well as /$foo[$\d]+/
 * anything else returns TRUE
 */

/* This is the one truly awful dwimmer necessary to conflate C and sed. */

STATIC int
S_intuit_more(pTHX_ register char *s)
{
    if (PL_lex_brackets)
	return TRUE;
    if (*s == '-' && s[1] == '>' && (s[2] == '[' || s[2] == '{'))
	return TRUE;
    if (*s != '{' && *s != '[')
	return FALSE;
    if (!PL_lex_inpat)
	return TRUE;

    /* In a pattern, so maybe we have {n,m}. */
    if (*s == '{') {
	s++;
	if (!isDIGIT(*s))
	    return TRUE;
	while (isDIGIT(*s))
	    s++;
	if (*s == ',')
	    s++;
	while (isDIGIT(*s))
	    s++;
	if (*s == '}')
	    return FALSE;
	return TRUE;
	
    }

    /* On the other hand, maybe we have a character class */

    s++;
    if (*s == ']' || *s == '^')
	return FALSE;
    else {
        /* this is terrifying, and it works */
	int weight = 2;		/* let's weigh the evidence */
	char seen[256];
	unsigned char un_char = 255, last_un_char;
	char *send = strchr(s,']');
	char tmpbuf[sizeof PL_tokenbuf * 4];

	if (!send)		/* has to be an expression */
	    return TRUE;

	Zero(seen,256,char);
	if (*s == '$')
	    weight -= 3;
	else if (isDIGIT(*s)) {
	    if (s[1] != ']') {
		if (isDIGIT(s[1]) && s[2] == ']')
		    weight -= 10;
	    }
	    else
		weight -= 100;
	}
	for (; s < send; s++) {
	    last_un_char = un_char;
	    un_char = (unsigned char)*s;
	    switch (*s) {
	    case '@':
	    case '&':
	    case '$':
		weight -= seen[un_char] * 10;
		if (isALNUM_lazy_if(s+1,UTF)) {
		    scan_ident(s, send, tmpbuf, sizeof tmpbuf, FALSE);
		    if ((int)strlen(tmpbuf) > 1 && gv_fetchpv(tmpbuf,FALSE, SVt_PV))
			weight -= 100;
		    else
			weight -= 10;
		}
		else if (*s == '$' && s[1] &&
		  strchr("[#!%*<>()-=",s[1])) {
		    if (/*{*/ strchr("])} =",s[2]))
			weight -= 10;
		    else
			weight -= 1;
		}
		break;
	    case '\\':
		un_char = 254;
		if (s[1]) {
		    if (strchr("wds]",s[1]))
			weight += 100;
		    else if (seen['\''] || seen['"'])
			weight += 1;
		    else if (strchr("rnftbxcav",s[1]))
			weight += 40;
		    else if (isDIGIT(s[1])) {
			weight += 40;
			while (s[1] && isDIGIT(s[1]))
			    s++;
		    }
		}
		else
		    weight += 100;
		break;
	    case '-':
		if (s[1] == '\\')
		    weight += 50;
		if (strchr("aA01! ",last_un_char))
		    weight += 30;
		if (strchr("zZ79~",s[1]))
		    weight += 30;
		if (last_un_char == 255 && (isDIGIT(s[1]) || s[1] == '$'))
		    weight -= 5;	/* cope with negative subscript */
		break;
	    default:
		if (!isALNUM(last_un_char) && !strchr("$@&",last_un_char) &&
			isALPHA(*s) && s[1] && isALPHA(s[1])) {
		    char *d = tmpbuf;
		    while (isALPHA(*s))
			*d++ = *s++;
		    *d = '\0';
		    if (keyword(tmpbuf, d - tmpbuf))
			weight -= 150;
		}
		if (un_char == last_un_char + 1)
		    weight += 5;
		weight -= seen[un_char];
		break;
	    }
	    seen[un_char]++;
	}
	if (weight >= 0)	/* probably a character class */
	    return FALSE;
    }

    return TRUE;
}

/*
 * S_intuit_method
 *
 * Does all the checking to disambiguate
 *   foo bar
 * between foo(bar) and bar->foo.  Returns 0 if not a method, otherwise
 * FUNCMETH (bar->foo(args)) or METHOD (bar->foo args).
 *
 * First argument is the stuff after the first token, e.g. "bar".
 *
 * Not a method if bar is a filehandle.
 * Not a method if foo is a subroutine prototyped to take a filehandle.
 * Not a method if it's really "Foo $bar"
 * Method if it's "foo $bar"
 * Not a method if it's really "print foo $bar"
 * Method if it's really "foo package::" (interpreted as package->foo)
 * Not a method if bar is known to be a subroutne ("sub bar; foo bar")
 * Not a method if bar is a filehandle or package, but is quoted with
 *   =>
 */

STATIC int
S_intuit_method(pTHX_ char *start, GV *gv)
{
    char *s = start + (*start == '$');
    char tmpbuf[sizeof PL_tokenbuf];
    STRLEN len;
    GV* indirgv;

    if (gv) {
	CV *cv;
	if (GvIO(gv))
	    return 0;
	if ((cv = GvCVu(gv))) {
	    char *proto = SvPVX(cv);
	    if (proto) {
		if (*proto == ';')
		    proto++;
		if (*proto == '*')
		    return 0;
	    }
	} else
	    gv = 0;
    }
    s = scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
    /* start is the beginning of the possible filehandle/object,
     * and s is the end of it
     * tmpbuf is a copy of it
     */

    if (*start == '$') {
	if (gv || PL_last_lop_op == OP_PRINT || isUPPER(*PL_tokenbuf))
	    return 0;
	s = skipspace(s);
	PL_bufptr = start;
	PL_expect = XREF;
	return *s == '(' ? FUNCMETH : METHOD;
    }
    if (!keyword(tmpbuf, len)) {
	if (len > 2 && tmpbuf[len - 2] == ':' && tmpbuf[len - 1] == ':') {
	    len -= 2;
	    tmpbuf[len] = '\0';
	    goto bare_package;
	}
	indirgv = gv_fetchpv(tmpbuf, FALSE, SVt_PVCV);
	if (indirgv && GvCVu(indirgv))
	    return 0;
	/* filehandle or package name makes it a method */
	if (!gv || GvIO(indirgv) || gv_stashpvn(tmpbuf, len, FALSE)) {
	    s = skipspace(s);
	    if ((PL_bufend - s) >= 2 && *s == '=' && *(s+1) == '>')
		return 0;	/* no assumptions -- "=>" quotes bearword */
      bare_package:
	    PL_nextval[PL_nexttoke].opval = (OP*)newSVOP(OP_CONST, 0,
						   newSVpvn(tmpbuf,len));
	    PL_nextval[PL_nexttoke].opval->op_private = OPpCONST_BARE;
	    PL_expect = XTERM;
	    force_next(WORD);
	    PL_bufptr = s;
	    return *s == '(' ? FUNCMETH : METHOD;
	}
    }
    return 0;
}

/*
 * S_incl_perldb
 * Return a string of Perl code to load the debugger.  If PERL5DB
 * is set, it will return the contents of that, otherwise a
 * compile-time require of perl5db.pl.
 */

STATIC char*
S_incl_perldb(pTHX)
{
    if (PL_perldb) {
	char *pdb = PerlEnv_getenv("PERL5DB");

	if (pdb)
	    return pdb;
	SETERRNO(0,SS$_NORMAL);
	return "BEGIN { require 'perl5db.pl' }";
    }
    return "";
}


/* Encoded script support. filter_add() effectively inserts a
 * 'pre-processing' function into the current source input stream.
 * Note that the filter function only applies to the current source file
 * (e.g., it will not affect files 'require'd or 'use'd by this one).
 *
 * The datasv parameter (which may be NULL) can be used to pass
 * private data to this instance of the filter. The filter function
 * can recover the SV using the FILTER_DATA macro and use it to
 * store private buffers and state information.
 *
 * The supplied datasv parameter is upgraded to a PVIO type
 * and the IoDIRP/IoANY field is used to store the function pointer,
 * and IOf_FAKE_DIRP is enabled on datasv to mark this as such.
 * Note that IoTOP_NAME, IoFMT_NAME, IoBOTTOM_NAME, if set for
 * private use must be set using malloc'd pointers.
 */

SV *
Perl_filter_add(pTHX_ filter_t funcp, SV *datasv)
{
    if (!funcp)
	return Nullsv;

    if (!PL_rsfp_filters)
	PL_rsfp_filters = newAV();
    if (!datasv)
	datasv = NEWSV(255,0);
    if (!SvUPGRADE(datasv, SVt_PVIO))
        Perl_die(aTHX_ "Can't upgrade filter_add data to SVt_PVIO");
    IoANY(datasv) = (void *)funcp; /* stash funcp into spare field */
    IoFLAGS(datasv) |= IOf_FAKE_DIRP;
    DEBUG_P(PerlIO_printf(Perl_debug_log, "filter_add func %p (%s)\n",
			  funcp, SvPV_nolen(datasv)));
    av_unshift(PL_rsfp_filters, 1);
    av_store(PL_rsfp_filters, 0, datasv) ;
    return(datasv);
}


/* Delete most recently added instance of this filter function.	*/
void
Perl_filter_del(pTHX_ filter_t funcp)
{
    SV *datasv;
    DEBUG_P(PerlIO_printf(Perl_debug_log, "filter_del func %p", funcp));
    if (!PL_rsfp_filters || AvFILLp(PL_rsfp_filters)<0)
	return;
    /* if filter is on top of stack (usual case) just pop it off */
    datasv = FILTER_DATA(AvFILLp(PL_rsfp_filters));
    if (IoANY(datasv) == (void *)funcp) {
	IoFLAGS(datasv) &= ~IOf_FAKE_DIRP;
	IoANY(datasv) = (void *)NULL;
	sv_free(av_pop(PL_rsfp_filters));

        return;
    }
    /* we need to search for the correct entry and clear it	*/
    Perl_die(aTHX_ "filter_del can only delete in reverse order (currently)");
}


/* Invoke the n'th filter function for the current rsfp.	 */
I32
Perl_filter_read(pTHX_ int idx, SV *buf_sv, int maxlen)


               		/* 0 = read one text line */
{
    filter_t funcp;
    SV *datasv = NULL;

    if (!PL_rsfp_filters)
	return -1;
    if (idx > AvFILLp(PL_rsfp_filters)){       /* Any more filters?	*/
	/* Provide a default input filter to make life easy.	*/
	/* Note that we append to the line. This is handy.	*/
	DEBUG_P(PerlIO_printf(Perl_debug_log,
			      "filter_read %d: from rsfp\n", idx));
	if (maxlen) {
 	    /* Want a block */
	    int len ;
	    int old_len = SvCUR(buf_sv) ;

	    /* ensure buf_sv is large enough */
	    SvGROW(buf_sv, old_len + maxlen) ;
	    if ((len = PerlIO_read(PL_rsfp, SvPVX(buf_sv) + old_len, maxlen)) <= 0){
		if (PerlIO_error(PL_rsfp))
	            return -1;		/* error */
	        else
		    return 0 ;		/* end of file */
	    }
	    SvCUR_set(buf_sv, old_len + len) ;
	} else {
	    /* Want a line */
            if (sv_gets(buf_sv, PL_rsfp, SvCUR(buf_sv)) == NULL) {
		if (PerlIO_error(PL_rsfp))
	            return -1;		/* error */
	        else
		    return 0 ;		/* end of file */
	    }
	}
	return SvCUR(buf_sv);
    }
    /* Skip this filter slot if filter has been deleted	*/
    if ( (datasv = FILTER_DATA(idx)) == &PL_sv_undef){
	DEBUG_P(PerlIO_printf(Perl_debug_log,
			      "filter_read %d: skipped (filter deleted)\n",
			      idx));
	return FILTER_READ(idx+1, buf_sv, maxlen); /* recurse */
    }
    /* Get function pointer hidden within datasv	*/
    funcp = (filter_t)IoANY(datasv);
    DEBUG_P(PerlIO_printf(Perl_debug_log,
			  "filter_read %d: via function %p (%s)\n",
			  idx, funcp, SvPV_nolen(datasv)));
    /* Call function. The function is expected to 	*/
    /* call "FILTER_READ(idx+1, buf_sv)" first.		*/
    /* Return: <0:error, =0:eof, >0:not eof 		*/
    return (*funcp)(aTHXo_ idx, buf_sv, maxlen);
}

STATIC char *
S_filter_gets(pTHX_ register SV *sv, register PerlIO *fp, STRLEN append)
{
#ifdef PERL_CR_FILTER
    if (!PL_rsfp_filters) {
	filter_add(S_cr_textfilter,NULL);
    }
#endif
    if (PL_rsfp_filters) {

	if (!append)
            SvCUR_set(sv, 0);	/* start with empty line	*/
        if (FILTER_READ(0, sv, 0) > 0)
            return ( SvPVX(sv) ) ;
        else
	    return Nullch ;
    }
    else
        return (sv_gets(sv, fp, append));
}

STATIC HV *
S_find_in_my_stash(pTHX_ char *pkgname, I32 len)
{
    GV *gv;

    if (len == 11 && *pkgname == '_' && strEQ(pkgname, "__PACKAGE__"))
        return PL_curstash;

    if (len > 2 &&
        (pkgname[len - 2] == ':' && pkgname[len - 1] == ':') &&
        (gv = gv_fetchpv(pkgname, FALSE, SVt_PVHV)))
    {
        return GvHV(gv);			/* Foo:: */
    }

    /* use constant CLASS => 'MyClass' */
    if ((gv = gv_fetchpv(pkgname, FALSE, SVt_PVCV))) {
        SV *sv;
        if (GvCV(gv) && (sv = cv_const_sv(GvCV(gv)))) {
            pkgname = SvPV_nolen(sv);
        }
    }

    return gv_stashpv(pkgname, FALSE);
}

#ifdef DEBUGGING
    static char* exp_name[] =
	{ "OPERATOR", "TERM", "REF", "STATE", "BLOCK", "ATTRBLOCK",
	  "ATTRTERM", "TERMBLOCK"
	};
#endif

/*
  yylex

  Works out what to call the token just pulled out of the input
  stream.  The yacc parser takes care of taking the ops we return and
  stitching them into a tree.

  Returns:
    PRIVATEREF

  Structure:
      if read an identifier
          if we're in a my declaration
	      croak if they tried to say my($foo::bar)
	      build the ops for a my() declaration
	  if it's an access to a my() variable
	      are we in a sort block?
	          croak if my($a); $a <=> $b
	      build ops for access to a my() variable
	  if in a dq string, and they've said @foo and we can't find @foo
	      croak
	  build ops for a bareword
      if we already built the token before, use it.
*/

#ifdef USE_PURE_BISON
int
Perl_yylex_r(pTHX_ YYSTYPE *lvalp, int *lcharp)
{
    int r;

    yyactlevel++;
    yylval_pointer[yyactlevel] = lvalp;
    yychar_pointer[yyactlevel] = lcharp;
    if (yyactlevel >= YYMAXLEVEL)
	Perl_croak(aTHX_ "panic: YYMAXLEVEL");

    r = Perl_yylex(aTHX);

    yyactlevel--;

    return r;
}
#endif

#ifdef __SC__
#pragma segment Perl_yylex
#endif
int
Perl_yylex(pTHX)
{
    register char *s;
    register char *d;
    register I32 tmp;
    STRLEN len;
    GV *gv = Nullgv;
    GV **gvp = 0;
    bool bof = FALSE;

    /* check if there's an identifier for us to look at */
    if (PL_pending_ident) {
        /* pit holds the identifier we read and pending_ident is reset */
	char pit = PL_pending_ident;
	PL_pending_ident = 0;

	DEBUG_T({ PerlIO_printf(Perl_debug_log,
              "### Tokener saw identifier '%s'\n", PL_tokenbuf); })

	/* if we're in a my(), we can't allow dynamics here.
	   $foo'bar has already been turned into $foo::bar, so
	   just check for colons.

	   if it's a legal name, the OP is a PADANY.
	*/
	if (PL_in_my) {
	    if (PL_in_my == KEY_our) {	/* "our" is merely analogous to "my" */
		if (strchr(PL_tokenbuf,':'))
		    yyerror(Perl_form(aTHX_ "No package name allowed for "
				      "variable %s in \"our\"",
				      PL_tokenbuf));
		tmp = pad_allocmy(PL_tokenbuf);
	    }
	    else {
		if (strchr(PL_tokenbuf,':'))
		    yyerror(Perl_form(aTHX_ PL_no_myglob,PL_tokenbuf));

		yylval.opval = newOP(OP_PADANY, 0);
		yylval.opval->op_targ = pad_allocmy(PL_tokenbuf);
		return PRIVATEREF;
	    }
	}

	/*
	   build the ops for accesses to a my() variable.

	   Deny my($a) or my($b) in a sort block, *if* $a or $b is
	   then used in a comparison.  This catches most, but not
	   all cases.  For instance, it catches
	       sort { my($a); $a <=> $b }
	   but not
	       sort { my($a); $a < $b ? -1 : $a == $b ? 0 : 1; }
	   (although why you'd do that is anyone's guess).
	*/

	if (!strchr(PL_tokenbuf,':')) {
#ifdef USE_THREADS
	    /* Check for single character per-thread SVs */
	    if (PL_tokenbuf[0] == '$' && PL_tokenbuf[2] == '\0'
		&& !isALPHA(PL_tokenbuf[1]) /* Rule out obvious non-threadsvs */
		&& (tmp = find_threadsv(&PL_tokenbuf[1])) != NOT_IN_PAD)
	    {
		yylval.opval = newOP(OP_THREADSV, 0);
		yylval.opval->op_targ = tmp;
		return PRIVATEREF;
	    }
#endif /* USE_THREADS */
	    if ((tmp = pad_findmy(PL_tokenbuf)) != NOT_IN_PAD) {
		SV *namesv = AvARRAY(PL_comppad_name)[tmp];
		/* might be an "our" variable" */
		if (SvFLAGS(namesv) & SVpad_OUR) {
		    /* build ops for a bareword */
		    SV *sym = newSVpv(HvNAME(GvSTASH(namesv)),0);
		    sv_catpvn(sym, "::", 2);
		    sv_catpv(sym, PL_tokenbuf+1);
		    yylval.opval = (OP*)newSVOP(OP_CONST, 0, sym);
		    yylval.opval->op_private = OPpCONST_ENTERED;
		    gv_fetchpv(SvPVX(sym),
			(PL_in_eval
			    ? (GV_ADDMULTI | GV_ADDINEVAL)
			    : TRUE
			),
			((PL_tokenbuf[0] == '$') ? SVt_PV
			 : (PL_tokenbuf[0] == '@') ? SVt_PVAV
			 : SVt_PVHV));
		    return WORD;
		}

		/* if it's a sort block and they're naming $a or $b */
		if (PL_last_lop_op == OP_SORT &&
		    PL_tokenbuf[0] == '$' &&
		    (PL_tokenbuf[1] == 'a' || PL_tokenbuf[1] == 'b')
		    && !PL_tokenbuf[2])
		{
		    for (d = PL_in_eval ? PL_oldoldbufptr : PL_linestart;
			 d < PL_bufend && *d != '\n';
			 d++)
		    {
			if (strnEQ(d,"<=>",3) || strnEQ(d,"cmp",3)) {
			    Perl_croak(aTHX_ "Can't use \"my %s\" in sort comparison",
				  PL_tokenbuf);
			}
		    }
		}

		yylval.opval = newOP(OP_PADANY, 0);
		yylval.opval->op_targ = tmp;
		return PRIVATEREF;
	    }
	}

	/*
	   Whine if they've said @foo in a doublequoted string,
	   and @foo isn't a variable we can find in the symbol
	   table.
	*/
	if (pit == '@' && PL_lex_state != LEX_NORMAL && !PL_lex_brackets) {
	    GV *gv = gv_fetchpv(PL_tokenbuf+1, FALSE, SVt_PVAV);
	    if ((!gv || ((PL_tokenbuf[0] == '@') ? !GvAV(gv) : !GvHV(gv)))
		 && ckWARN(WARN_AMBIGUOUS))
	    {
                /* Downgraded from fatal to warning 20000522 mjd */
		Perl_warner(aTHX_ WARN_AMBIGUOUS,
			    "Possible unintended interpolation of %s in string",
			     PL_tokenbuf);
	    }
	}

	/* build ops for a bareword */
	yylval.opval = (OP*)newSVOP(OP_CONST, 0, newSVpv(PL_tokenbuf+1, 0));
	yylval.opval->op_private = OPpCONST_ENTERED;
	gv_fetchpv(PL_tokenbuf+1, PL_in_eval ? (GV_ADDMULTI | GV_ADDINEVAL) : TRUE,
		   ((PL_tokenbuf[0] == '$') ? SVt_PV
		    : (PL_tokenbuf[0] == '@') ? SVt_PVAV
		    : SVt_PVHV));
	return WORD;
    }

    /* no identifier pending identification */

    switch (PL_lex_state) {
#ifdef COMMENTARY
    case LEX_NORMAL:		/* Some compilers will produce faster */
    case LEX_INTERPNORMAL:	/* code if we comment these out. */
	break;
#endif

    /* when we've already built the next token, just pull it out of the queue */
    case LEX_KNOWNEXT:
	PL_nexttoke--;
	yylval = PL_nextval[PL_nexttoke];
	if (!PL_nexttoke) {
	    PL_lex_state = PL_lex_defer;
	    PL_expect = PL_lex_expect;
	    PL_lex_defer = LEX_NORMAL;
	}
	DEBUG_T({ PerlIO_printf(Perl_debug_log,
              "### Next token after '%s' was known, type %"IVdf"\n", PL_bufptr,
              (IV)PL_nexttype[PL_nexttoke]); })

	return(PL_nexttype[PL_nexttoke]);

    /* interpolated case modifiers like \L \U, including \Q and \E.
       when we get here, PL_bufptr is at the \
    */
    case LEX_INTERPCASEMOD:
#ifdef DEBUGGING
	if (PL_bufptr != PL_bufend && *PL_bufptr != '\\')
	    Perl_croak(aTHX_ "panic: INTERPCASEMOD");
#endif
	/* handle \E or end of string */
       	if (PL_bufptr == PL_bufend || PL_bufptr[1] == 'E') {
	    char oldmod;

	    /* if at a \E */
	    if (PL_lex_casemods) {
		oldmod = PL_lex_casestack[--PL_lex_casemods];
		PL_lex_casestack[PL_lex_casemods] = '\0';

		if (PL_bufptr != PL_bufend && strchr("LUQ", oldmod)) {
		    PL_bufptr += 2;
		    PL_lex_state = LEX_INTERPCONCAT;
		}
		return ')';
	    }
	    if (PL_bufptr != PL_bufend)
		PL_bufptr += 2;
	    PL_lex_state = LEX_INTERPCONCAT;
	    return yylex();
	}
	else {
	    DEBUG_T({ PerlIO_printf(Perl_debug_log,
              "### Saw case modifier at '%s'\n", PL_bufptr); })
	    s = PL_bufptr + 1;
	    if (strnEQ(s, "L\\u", 3) || strnEQ(s, "U\\l", 3))
		tmp = *s, *s = s[2], s[2] = tmp;	/* misordered... */
	    if (strchr("LU", *s) &&
		(strchr(PL_lex_casestack, 'L') || strchr(PL_lex_casestack, 'U')))
	    {
		PL_lex_casestack[--PL_lex_casemods] = '\0';
		return ')';
	    }
	    if (PL_lex_casemods > 10) {
		char* newlb = Renew(PL_lex_casestack, PL_lex_casemods + 2, char);
		if (newlb != PL_lex_casestack) {
		    SAVEFREEPV(newlb);
		    PL_lex_casestack = newlb;
		}
	    }
	    PL_lex_casestack[PL_lex_casemods++] = *s;
	    PL_lex_casestack[PL_lex_casemods] = '\0';
	    PL_lex_state = LEX_INTERPCONCAT;
	    PL_nextval[PL_nexttoke].ival = 0;
	    force_next('(');
	    if (*s == 'l')
		PL_nextval[PL_nexttoke].ival = OP_LCFIRST;
	    else if (*s == 'u')
		PL_nextval[PL_nexttoke].ival = OP_UCFIRST;
	    else if (*s == 'L')
		PL_nextval[PL_nexttoke].ival = OP_LC;
	    else if (*s == 'U')
		PL_nextval[PL_nexttoke].ival = OP_UC;
	    else if (*s == 'Q')
		PL_nextval[PL_nexttoke].ival = OP_QUOTEMETA;
	    else
		Perl_croak(aTHX_ "panic: yylex");
	    PL_bufptr = s + 1;
	    force_next(FUNC);
	    if (PL_lex_starts) {
		s = PL_bufptr;
		PL_lex_starts = 0;
		Aop(OP_CONCAT);
	    }
	    else
		return yylex();
	}

    case LEX_INTERPPUSH:
        return sublex_push();

    case LEX_INTERPSTART:
	if (PL_bufptr == PL_bufend)
	    return sublex_done();
	DEBUG_T({ PerlIO_printf(Perl_debug_log,
              "### Interpolated variable at '%s'\n", PL_bufptr); })
	PL_expect = XTERM;
	PL_lex_dojoin = (*PL_bufptr == '@');
	PL_lex_state = LEX_INTERPNORMAL;
	if (PL_lex_dojoin) {
	    PL_nextval[PL_nexttoke].ival = 0;
	    force_next(',');
#ifdef USE_THREADS
	    PL_nextval[PL_nexttoke].opval = newOP(OP_THREADSV, 0);
	    PL_nextval[PL_nexttoke].opval->op_targ = find_threadsv("\"");
	    force_next(PRIVATEREF);
#else
	    force_ident("\"", '$');
#endif /* USE_THREADS */
	    PL_nextval[PL_nexttoke].ival = 0;
	    force_next('$');
	    PL_nextval[PL_nexttoke].ival = 0;
	    force_next('(');
	    PL_nextval[PL_nexttoke].ival = OP_JOIN;	/* emulate join($", ...) */
	    force_next(FUNC);
	}
	if (PL_lex_starts++) {
	    s = PL_bufptr;
	    Aop(OP_CONCAT);
	}
	return yylex();

    case LEX_INTERPENDMAYBE:
	if (intuit_more(PL_bufptr)) {
	    PL_lex_state = LEX_INTERPNORMAL;	/* false alarm, more expr */
	    break;
	}
	/* FALL THROUGH */

    case LEX_INTERPEND:
	if (PL_lex_dojoin) {
	    PL_lex_dojoin = FALSE;
	    PL_lex_state = LEX_INTERPCONCAT;
	    return ')';
	}
	if (PL_lex_inwhat == OP_SUBST && PL_linestr == PL_lex_repl
	    && SvEVALED(PL_lex_repl))
	{
	    if (PL_bufptr != PL_bufend)
		Perl_croak(aTHX_ "Bad evalled substitution pattern");
	    PL_lex_repl = Nullsv;
	}
	/* FALLTHROUGH */
    case LEX_INTERPCONCAT:
#ifdef DEBUGGING
	if (PL_lex_brackets)
	    Perl_croak(aTHX_ "panic: INTERPCONCAT");
#endif
	if (PL_bufptr == PL_bufend)
	    return sublex_done();

	if (SvIVX(PL_linestr) == '\'') {
	    SV *sv = newSVsv(PL_linestr);
	    if (!PL_lex_inpat)
		sv = tokeq(sv);
	    else if ( PL_hints & HINT_NEW_RE )
		sv = new_constant(NULL, 0, "qr", sv, sv, "q");
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0, sv);
	    s = PL_bufend;
	}
	else {
	    s = scan_const(PL_bufptr);
	    if (*s == '\\')
		PL_lex_state = LEX_INTERPCASEMOD;
	    else
		PL_lex_state = LEX_INTERPSTART;
	}

	if (s != PL_bufptr) {
	    PL_nextval[PL_nexttoke] = yylval;
	    PL_expect = XTERM;
	    force_next(THING);
	    if (PL_lex_starts++)
		Aop(OP_CONCAT);
	    else {
		PL_bufptr = s;
		return yylex();
	    }
	}

	return yylex();
    case LEX_FORMLINE:
	PL_lex_state = LEX_NORMAL;
	s = scan_formline(PL_bufptr);
	if (!PL_lex_formbrack)
	    goto rightbracket;
	OPERATOR(';');
    }

    s = PL_bufptr;
    PL_oldoldbufptr = PL_oldbufptr;
    PL_oldbufptr = s;
    DEBUG_T( {
	PerlIO_printf(Perl_debug_log, "### Tokener expecting %s at %s\n",
		      exp_name[PL_expect], s);
    } )

  retry:
    switch (*s) {
    default:
	if (isIDFIRST_lazy_if(s,UTF))
	    goto keylookup;
	Perl_croak(aTHX_ "Unrecognized character \\x%02X", *s & 255);
    case 4:
    case 26:
	goto fake_eof;			/* emulate EOF on ^D or ^Z */
    case 0:
	if (!PL_rsfp) {
	    PL_last_uni = 0;
	    PL_last_lop = 0;
	    if (PL_lex_brackets)
		yyerror("Missing right curly or square bracket");
            DEBUG_T( { PerlIO_printf(Perl_debug_log,
                        "### Tokener got EOF\n");
            } )
	    TOKEN(0);
	}
	if (s++ < PL_bufend)
	    goto retry;			/* ignore stray nulls */
	PL_last_uni = 0;
	PL_last_lop = 0;
	if (!PL_in_eval && !PL_preambled) {
	    PL_preambled = TRUE;
	    sv_setpv(PL_linestr,incl_perldb());
	    if (SvCUR(PL_linestr))
		sv_catpv(PL_linestr,";");
	    if (PL_preambleav){
		while(AvFILLp(PL_preambleav) >= 0) {
		    SV *tmpsv = av_shift(PL_preambleav);
		    sv_catsv(PL_linestr, tmpsv);
		    sv_catpv(PL_linestr, ";");
		    sv_free(tmpsv);
		}
		sv_free((SV*)PL_preambleav);
		PL_preambleav = NULL;
	    }
	    if (PL_minus_n || PL_minus_p) {
		sv_catpv(PL_linestr, "LINE: while (<>) {");
		if (PL_minus_l)
		    sv_catpv(PL_linestr,"chomp;");
		if (PL_minus_a) {
		    GV* gv = gv_fetchpv("::F", TRUE, SVt_PVAV);
		    if (gv)
			GvIMPORTED_AV_on(gv);
		    if (PL_minus_F) {
			if (strchr("/'\"", *PL_splitstr)
			      && strchr(PL_splitstr + 1, *PL_splitstr))
			    Perl_sv_catpvf(aTHX_ PL_linestr, "@F=split(%s);", PL_splitstr);
			else {
			    char delim;
			    s = "'~#\200\1'"; /* surely one char is unused...*/
			    while (s[1] && strchr(PL_splitstr, *s))  s++;
			    delim = *s;
			    Perl_sv_catpvf(aTHX_ PL_linestr, "@F=split(%s%c",
				      "q" + (delim == '\''), delim);
			    for (s = PL_splitstr; *s; s++) {
				if (*s == '\\')
				    sv_catpvn(PL_linestr, "\\", 1);
				sv_catpvn(PL_linestr, s, 1);
			    }
			    Perl_sv_catpvf(aTHX_ PL_linestr, "%c);", delim);
			}
		    }
		    else
		        sv_catpv(PL_linestr,"@F=split(' ');");
		}
	    }
	    sv_catpv(PL_linestr, "\n");
	    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
	    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	    PL_last_lop = PL_last_uni = Nullch;
	    if (PERLDB_LINE && PL_curstash != PL_debstash) {
		SV *sv = NEWSV(85,0);

		sv_upgrade(sv, SVt_PVMG);
		sv_setsv(sv,PL_linestr);
		av_store(CopFILEAV(PL_curcop),(I32)CopLINE(PL_curcop),sv);
	    }
	    goto retry;
	}
	do {
	    bof = PL_rsfp ? TRUE : FALSE;
	    if ((s = filter_gets(PL_linestr, PL_rsfp, 0)) == Nullch) {
	      fake_eof:
		if (PL_rsfp) {
		    if (PL_preprocess && !PL_in_eval)
			(void)PerlProc_pclose(PL_rsfp);
		    else if ((PerlIO *)PL_rsfp == PerlIO_stdin())
			PerlIO_clearerr(PL_rsfp);
		    else
			(void)PerlIO_close(PL_rsfp);
		    PL_rsfp = Nullfp;
		    PL_doextract = FALSE;
		}
		if (!PL_in_eval && (PL_minus_n || PL_minus_p)) {
		    sv_setpv(PL_linestr,PL_minus_p ? ";}continue{print" : "");
		    sv_catpv(PL_linestr,";}");
		    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
		    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
		    PL_last_lop = PL_last_uni = Nullch;
		    PL_minus_n = PL_minus_p = 0;
		    goto retry;
		}
		PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
		PL_last_lop = PL_last_uni = Nullch;
		sv_setpv(PL_linestr,"");
		TOKEN(';');	/* not infinite loop because rsfp is NULL now */
	    }
	    /* if it looks like the start of a BOM, check if it in fact is */
	    else if (bof && (!*s || *(U8*)s == 0xEF || *(U8*)s >= 0xFE)) {
#ifdef PERLIO_IS_STDIO
#  ifdef __GNU_LIBRARY__
#    if __GNU_LIBRARY__ == 1 /* Linux glibc5 */
#      define FTELL_FOR_PIPE_IS_BROKEN
#    endif
#  else
#    ifdef __GLIBC__
#      if __GLIBC__ == 1 /* maybe some glibc5 release had it like this? */
#        define FTELL_FOR_PIPE_IS_BROKEN
#      endif
#    endif
#  endif
#endif
#ifdef FTELL_FOR_PIPE_IS_BROKEN
		/* This loses the possibility to detect the bof
		 * situation on perl -P when the libc5 is being used.
		 * Workaround?  Maybe attach some extra state to PL_rsfp?
		 */
		if (!PL_preprocess)
		    bof = PerlIO_tell(PL_rsfp) == SvCUR(PL_linestr);
#else
		bof = PerlIO_tell(PL_rsfp) == SvCUR(PL_linestr);
#endif
		if (bof) {
		    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
		    s = swallow_bom((U8*)s);
		}
	    }
	    if (PL_doextract) {
		if (*s == '#' && s[1] == '!' && instr(s,"perl"))
		    PL_doextract = FALSE;

		/* Incest with pod. */
		if (*s == '=' && strnEQ(s, "=cut", 4)) {
		    sv_setpv(PL_linestr, "");
		    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
		    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
		    PL_last_lop = PL_last_uni = Nullch;
		    PL_doextract = FALSE;
		}
	    }
	    incline(s);
	} while (PL_doextract);
	PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = PL_linestart = s;
	if (PERLDB_LINE && PL_curstash != PL_debstash) {
	    SV *sv = NEWSV(85,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setsv(sv,PL_linestr);
	    av_store(CopFILEAV(PL_curcop),(I32)CopLINE(PL_curcop),sv);
	}
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	PL_last_lop = PL_last_uni = Nullch;
	if (CopLINE(PL_curcop) == 1) {
	    while (s < PL_bufend && isSPACE(*s))
		s++;
	    if (*s == ':' && s[1] != ':') /* for csh execing sh scripts */
		s++;
	    d = Nullch;
	    if (!PL_in_eval) {
		if (*s == '#' && *(s+1) == '!')
		    d = s + 2;
#ifdef ALTERNATE_SHEBANG
		else {
		    static char as[] = ALTERNATE_SHEBANG;
		    if (*s == as[0] && strnEQ(s, as, sizeof(as) - 1))
			d = s + (sizeof(as) - 1);
		}
#endif /* ALTERNATE_SHEBANG */
	    }
	    if (d) {
		char *ipath;
		char *ipathend;

		while (isSPACE(*d))
		    d++;
		ipath = d;
		while (*d && !isSPACE(*d))
		    d++;
		ipathend = d;

#ifdef ARG_ZERO_IS_SCRIPT
		if (ipathend > ipath) {
		    /*
		     * HP-UX (at least) sets argv[0] to the script name,
		     * which makes $^X incorrect.  And Digital UNIX and Linux,
		     * at least, set argv[0] to the basename of the Perl
		     * interpreter. So, having found "#!", we'll set it right.
		     */
		    SV *x = GvSV(gv_fetchpv("\030", TRUE, SVt_PV));
		    assert(SvPOK(x) || SvGMAGICAL(x));
		    if (sv_eq(x, CopFILESV(PL_curcop))) {
			sv_setpvn(x, ipath, ipathend - ipath);
			SvSETMAGIC(x);
		    }
		    TAINT_NOT;	/* $^X is always tainted, but that's OK */
		}
#endif /* ARG_ZERO_IS_SCRIPT */

		/*
		 * Look for options.
		 */
		d = instr(s,"perl -");
		if (!d) {
		    d = instr(s,"perl");
#if defined(DOSISH)
		    /* avoid getting into infinite loops when shebang
		     * line contains "Perl" rather than "perl" */
		    if (!d) {
			for (d = ipathend-4; d >= ipath; --d) {
			    if ((*d == 'p' || *d == 'P')
				&& !ibcmp(d, "perl", 4))
			    {
				break;
			    }
			}
			if (d < ipath)
			    d = Nullch;
		    }
#endif
		}
#ifdef ALTERNATE_SHEBANG
		/*
		 * If the ALTERNATE_SHEBANG on this system starts with a
		 * character that can be part of a Perl expression, then if
		 * we see it but not "perl", we're probably looking at the
		 * start of Perl code, not a request to hand off to some
		 * other interpreter.  Similarly, if "perl" is there, but
		 * not in the first 'word' of the line, we assume the line
		 * contains the start of the Perl program.
		 */
		if (d && *s != '#') {
		    char *c = ipath;
		    while (*c && !strchr("; \t\r\n\f\v#", *c))
			c++;
		    if (c < d)
			d = Nullch;	/* "perl" not in first word; ignore */
		    else
			*s = '#';	/* Don't try to parse shebang line */
		}
#endif /* ALTERNATE_SHEBANG */
#ifndef MACOS_TRADITIONAL
		if (!d &&
		    *s == '#' &&
		    ipathend > ipath &&
		    !PL_minus_c &&
		    !instr(s,"indir") &&
		    instr(PL_origargv[0],"perl"))
		{
		    char **newargv;

		    *ipathend = '\0';
		    s = ipathend + 1;
		    while (s < PL_bufend && isSPACE(*s))
			s++;
		    if (s < PL_bufend) {
			Newz(899,newargv,PL_origargc+3,char*);
			newargv[1] = s;
			while (s < PL_bufend && !isSPACE(*s))
			    s++;
			*s = '\0';
			Copy(PL_origargv+1, newargv+2, PL_origargc+1, char*);
		    }
		    else
			newargv = PL_origargv;
		    newargv[0] = ipath;
		    PerlProc_execv(ipath, EXEC_ARGV_CAST(newargv));
		    Perl_croak(aTHX_ "Can't exec %s", ipath);
		}
#endif
		if (d) {
		    U32 oldpdb = PL_perldb;
		    bool oldn = PL_minus_n;
		    bool oldp = PL_minus_p;

		    while (*d && !isSPACE(*d)) d++;
		    while (SPACE_OR_TAB(*d)) d++;

		    if (*d++ == '-') {
			do {
			    if (*d == 'M' || *d == 'm') {
				char *m = d;
				while (*d && !isSPACE(*d)) d++;
				Perl_croak(aTHX_ "Too late for \"-%.*s\" option",
				      (int)(d - m), m);
			    }
			    d = moreswitches(d);
			} while (d);
			if ((PERLDB_LINE && !oldpdb) ||
			    ((PL_minus_n || PL_minus_p) && !(oldn || oldp)))
			      /* if we have already added "LINE: while (<>) {",
			         we must not do it again */
			{
			    sv_setpv(PL_linestr, "");
			    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
			    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
			    PL_last_lop = PL_last_uni = Nullch;
			    PL_preambled = FALSE;
			    if (PERLDB_LINE)
				(void)gv_fetchfile(PL_origfilename);
			    goto retry;
			}
		    }
		}
	    }
	}
	if (PL_lex_formbrack && PL_lex_brackets <= PL_lex_formbrack) {
	    PL_bufptr = s;
	    PL_lex_state = LEX_FORMLINE;
	    return yylex();
	}
	goto retry;
    case '\r':
#ifdef PERL_STRICT_CR
	Perl_warn(aTHX_ "Illegal character \\%03o (carriage return)", '\r');
	Perl_croak(aTHX_
      "\t(Maybe you didn't strip carriage returns after a network transfer?)\n");
#endif
    case ' ': case '\t': case '\f': case 013:
#ifdef MACOS_TRADITIONAL
    case '\312':
#endif
	s++;
	goto retry;
    case '#':
    case '\n':
	if (PL_lex_state != LEX_NORMAL || (PL_in_eval && !PL_rsfp)) {
	    if (*s == '#' && s == PL_linestart && PL_in_eval && !PL_rsfp) {
		/* handle eval qq[#line 1 "foo"\n ...] */
		CopLINE_dec(PL_curcop);
		incline(s);
	    }
	    d = PL_bufend;
	    while (s < d && *s != '\n')
		s++;
	    if (s < d)
		s++;
	    incline(s);
	    if (PL_lex_formbrack && PL_lex_brackets <= PL_lex_formbrack) {
		PL_bufptr = s;
		PL_lex_state = LEX_FORMLINE;
		return yylex();
	    }
	}
	else {
	    *s = '\0';
	    PL_bufend = s;
	}
	goto retry;
    case '-':
	if (s[1] && isALPHA(s[1]) && !isALNUM(s[2])) {
	    I32 ftst = 0;

	    s++;
	    PL_bufptr = s;
	    tmp = *s++;

	    while (s < PL_bufend && SPACE_OR_TAB(*s))
		s++;

	    if (strnEQ(s,"=>",2)) {
		s = force_word(PL_bufptr,WORD,FALSE,FALSE,FALSE);
                DEBUG_T( { PerlIO_printf(Perl_debug_log,
                            "### Saw unary minus before =>, forcing word '%s'\n", s);
                } )
		OPERATOR('-');		/* unary minus */
	    }
	    PL_last_uni = PL_oldbufptr;
	    switch (tmp) {
	    case 'r': ftst = OP_FTEREAD;	break;
	    case 'w': ftst = OP_FTEWRITE;	break;
	    case 'x': ftst = OP_FTEEXEC;	break;
	    case 'o': ftst = OP_FTEOWNED;	break;
	    case 'R': ftst = OP_FTRREAD;	break;
	    case 'W': ftst = OP_FTRWRITE;	break;
	    case 'X': ftst = OP_FTREXEC;	break;
	    case 'O': ftst = OP_FTROWNED;	break;
	    case 'e': ftst = OP_FTIS;		break;
	    case 'z': ftst = OP_FTZERO;		break;
	    case 's': ftst = OP_FTSIZE;		break;
	    case 'f': ftst = OP_FTFILE;		break;
	    case 'd': ftst = OP_FTDIR;		break;
	    case 'l': ftst = OP_FTLINK;		break;
	    case 'p': ftst = OP_FTPIPE;		break;
	    case 'S': ftst = OP_FTSOCK;		break;
	    case 'u': ftst = OP_FTSUID;		break;
	    case 'g': ftst = OP_FTSGID;		break;
	    case 'k': ftst = OP_FTSVTX;		break;
	    case 'b': ftst = OP_FTBLK;		break;
	    case 'c': ftst = OP_FTCHR;		break;
	    case 't': ftst = OP_FTTTY;		break;
	    case 'T': ftst = OP_FTTEXT;		break;
	    case 'B': ftst = OP_FTBINARY;	break;
	    case 'M': case 'A': case 'C':
		gv_fetchpv("\024",TRUE, SVt_PV);
		switch (tmp) {
		case 'M': ftst = OP_FTMTIME;	break;
		case 'A': ftst = OP_FTATIME;	break;
		case 'C': ftst = OP_FTCTIME;	break;
		default:			break;
		}
		break;
	    default:
		Perl_croak(aTHX_ "Unrecognized file test: -%c", (int)tmp);
		break;
	    }
	    PL_last_lop_op = ftst;
	    DEBUG_T( { PerlIO_printf(Perl_debug_log,
				     "### Saw file test %c\n", (int)ftst);
	    } )
	    FTST(ftst);
	}
	tmp = *s++;
	if (*s == tmp) {
	    s++;
	    if (PL_expect == XOPERATOR)
		TERM(POSTDEC);
	    else
		OPERATOR(PREDEC);
	}
	else if (*s == '>') {
	    s++;
	    s = skipspace(s);
	    if (isIDFIRST_lazy_if(s,UTF)) {
		s = force_word(s,METHOD,FALSE,TRUE,FALSE);
		TOKEN(ARROW);
	    }
	    else if (*s == '$')
		OPERATOR(ARROW);
	    else
		TERM(ARROW);
	}
	if (PL_expect == XOPERATOR)
	    Aop(OP_SUBTRACT);
	else {
	    if (isSPACE(*s) || !isSPACE(*PL_bufptr))
		check_uni();
	    OPERATOR('-');		/* unary minus */
	}

    case '+':
	tmp = *s++;
	if (*s == tmp) {
	    s++;
	    if (PL_expect == XOPERATOR)
		TERM(POSTINC);
	    else
		OPERATOR(PREINC);
	}
	if (PL_expect == XOPERATOR)
	    Aop(OP_ADD);
	else {
	    if (isSPACE(*s) || !isSPACE(*PL_bufptr))
		check_uni();
	    OPERATOR('+');
	}

    case '*':
	if (PL_expect != XOPERATOR) {
	    s = scan_ident(s, PL_bufend, PL_tokenbuf, sizeof PL_tokenbuf, TRUE);
	    PL_expect = XOPERATOR;
	    force_ident(PL_tokenbuf, '*');
	    if (!*PL_tokenbuf)
		PREREF('*');
	    TERM('*');
	}
	s++;
	if (*s == '*') {
	    s++;
	    PWop(OP_POW);
	}
	Mop(OP_MULTIPLY);

    case '%':
	if (PL_expect == XOPERATOR) {
	    ++s;
	    Mop(OP_MODULO);
	}
	PL_tokenbuf[0] = '%';
	s = scan_ident(s, PL_bufend, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1, TRUE);
	if (!PL_tokenbuf[1]) {
	    if (s == PL_bufend)
		yyerror("Final % should be \\% or %name");
	    PREREF('%');
	}
	PL_pending_ident = '%';
	TERM('%');

    case '^':
	s++;
	BOop(OP_BIT_XOR);
    case '[':
	PL_lex_brackets++;
	/* FALL THROUGH */
    case '~':
    case ',':
	tmp = *s++;
	OPERATOR(tmp);
    case ':':
	if (s[1] == ':') {
	    len = 0;
	    goto just_a_word;
	}
	s++;
	switch (PL_expect) {
	    OP *attrs;
	case XOPERATOR:
	    if (!PL_in_my || PL_lex_state != LEX_NORMAL)
		break;
	    PL_bufptr = s;	/* update in case we back off */
	    goto grabattrs;
	case XATTRBLOCK:
	    PL_expect = XBLOCK;
	    goto grabattrs;
	case XATTRTERM:
	    PL_expect = XTERMBLOCK;
	 grabattrs:
	    s = skipspace(s);
	    attrs = Nullop;
	    while (isIDFIRST_lazy_if(s,UTF)) {
		d = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE, &len);
		if (isLOWER(*s) && (tmp = keyword(PL_tokenbuf, len))) {
		    if (tmp < 0) tmp = -tmp;
		    switch (tmp) {
		    case KEY_or:
		    case KEY_and:
		    case KEY_for:
		    case KEY_unless:
		    case KEY_if:
		    case KEY_while:
		    case KEY_until:
			goto got_attrs;
		    default:
			break;
		    }
		}
		if (*d == '(') {
		    d = scan_str(d,TRUE,TRUE);
		    if (!d) {
			/* MUST advance bufptr here to avoid bogus
			   "at end of line" context messages from yyerror().
			 */
			PL_bufptr = s + len;
			yyerror("Unterminated attribute parameter in attribute list");
			if (attrs)
			    op_free(attrs);
			return 0;	/* EOF indicator */
		    }
		}
		if (PL_lex_stuff) {
		    SV *sv = newSVpvn(s, len);
		    sv_catsv(sv, PL_lex_stuff);
		    attrs = append_elem(OP_LIST, attrs,
					newSVOP(OP_CONST, 0, sv));
		    SvREFCNT_dec(PL_lex_stuff);
		    PL_lex_stuff = Nullsv;
		}
		else {
		    if (!PL_in_my && len == 6 && strnEQ(s, "lvalue", len))
			CvLVALUE_on(PL_compcv);
		    else if (!PL_in_my && len == 6 && strnEQ(s, "locked", len))
			CvLOCKED_on(PL_compcv);
		    else if (!PL_in_my && len == 6 && strnEQ(s, "method", len))
			CvMETHOD_on(PL_compcv);
		    /* After we've set the flags, it could be argued that
		       we don't need to do the attributes.pm-based setting
		       process, and shouldn't bother appending recognized
		       flags. To experiment with that, uncomment the
		       following "else": */
		    /* else */
		        attrs = append_elem(OP_LIST, attrs,
					    newSVOP(OP_CONST, 0,
					      	    newSVpvn(s, len)));
		}
		s = skipspace(d);
		if (*s == ':' && s[1] != ':')
		    s = skipspace(s+1);
		else if (s == d)
		    break;	/* require real whitespace or :'s */
	    }
	    tmp = (PL_expect == XOPERATOR ? '=' : '{'); /*'}(' for vi */
	    if (*s != ';' && *s != tmp && (tmp != '=' || *s != ')')) {
		char q = ((*s == '\'') ? '"' : '\'');
		/* If here for an expression, and parsed no attrs, back off. */
		if (tmp == '=' && !attrs) {
		    s = PL_bufptr;
		    break;
		}
		/* MUST advance bufptr here to avoid bogus "at end of line"
		   context messages from yyerror().
		 */
		PL_bufptr = s;
		if (!*s)
		    yyerror("Unterminated attribute list");
		else
		    yyerror(Perl_form(aTHX_ "Invalid separator character %c%c%c in attribute list",
				      q, *s, q));
		if (attrs)
		    op_free(attrs);
		OPERATOR(':');
	    }
	got_attrs:
	    if (attrs) {
		PL_nextval[PL_nexttoke].opval = attrs;
		force_next(THING);
	    }
	    TOKEN(COLONATTR);
	}
	OPERATOR(':');
    case '(':
	s++;
	if (PL_last_lop == PL_oldoldbufptr || PL_last_uni == PL_oldoldbufptr)
	    PL_oldbufptr = PL_oldoldbufptr;		/* allow print(STDOUT 123) */
	else
	    PL_expect = XTERM;
	TOKEN('(');
    case ';':
	CLINE;
	tmp = *s++;
	OPERATOR(tmp);
    case ')':
	tmp = *s++;
	s = skipspace(s);
	if (*s == '{')
	    PREBLOCK(tmp);
	TERM(tmp);
    case ']':
	s++;
	if (PL_lex_brackets <= 0)
	    yyerror("Unmatched right square bracket");
	else
	    --PL_lex_brackets;
	if (PL_lex_state == LEX_INTERPNORMAL) {
	    if (PL_lex_brackets == 0) {
		if (*s != '[' && *s != '{' && (*s != '-' || s[1] != '>'))
		    PL_lex_state = LEX_INTERPEND;
	    }
	}
	TERM(']');
    case '{':
      leftbracket:
	s++;
	if (PL_lex_brackets > 100) {
	    char* newlb = Renew(PL_lex_brackstack, PL_lex_brackets + 1, char);
	    if (newlb != PL_lex_brackstack) {
		SAVEFREEPV(newlb);
		PL_lex_brackstack = newlb;
	    }
	}
	switch (PL_expect) {
	case XTERM:
	    if (PL_lex_formbrack) {
		s--;
		PRETERMBLOCK(DO);
	    }
	    if (PL_oldoldbufptr == PL_last_lop)
		PL_lex_brackstack[PL_lex_brackets++] = XTERM;
	    else
		PL_lex_brackstack[PL_lex_brackets++] = XOPERATOR;
	    OPERATOR(HASHBRACK);
	case XOPERATOR:
	    while (s < PL_bufend && SPACE_OR_TAB(*s))
		s++;
	    d = s;
	    PL_tokenbuf[0] = '\0';
	    if (d < PL_bufend && *d == '-') {
		PL_tokenbuf[0] = '-';
		d++;
		while (d < PL_bufend && SPACE_OR_TAB(*d))
		    d++;
	    }
	    if (d < PL_bufend && isIDFIRST_lazy_if(d,UTF)) {
		d = scan_word(d, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1,
			      FALSE, &len);
		while (d < PL_bufend && SPACE_OR_TAB(*d))
		    d++;
		if (*d == '}') {
		    char minus = (PL_tokenbuf[0] == '-');
		    s = force_word(s + minus, WORD, FALSE, TRUE, FALSE);
		    if (UTF && !IN_BYTE && is_utf8_string((U8*)PL_tokenbuf, 0) &&
			PL_nextval[PL_nexttoke-1].opval)
		      SvUTF8_on(((SVOP*)PL_nextval[PL_nexttoke-1].opval)->op_sv);
		    if (minus)
			force_next('-');
		}
	    }
	    /* FALL THROUGH */
	case XATTRBLOCK:
	case XBLOCK:
	    PL_lex_brackstack[PL_lex_brackets++] = XSTATE;
	    PL_expect = XSTATE;
	    break;
	case XATTRTERM:
	case XTERMBLOCK:
	    PL_lex_brackstack[PL_lex_brackets++] = XOPERATOR;
	    PL_expect = XSTATE;
	    break;
	default: {
		char *t;
		if (PL_oldoldbufptr == PL_last_lop)
		    PL_lex_brackstack[PL_lex_brackets++] = XTERM;
		else
		    PL_lex_brackstack[PL_lex_brackets++] = XOPERATOR;
		s = skipspace(s);
		if (*s == '}')
		    OPERATOR(HASHBRACK);
		/* This hack serves to disambiguate a pair of curlies
		 * as being a block or an anon hash.  Normally, expectation
		 * determines that, but in cases where we're not in a
		 * position to expect anything in particular (like inside
		 * eval"") we have to resolve the ambiguity.  This code
		 * covers the case where the first term in the curlies is a
		 * quoted string.  Most other cases need to be explicitly
		 * disambiguated by prepending a `+' before the opening
		 * curly in order to force resolution as an anon hash.
		 *
		 * XXX should probably propagate the outer expectation
		 * into eval"" to rely less on this hack, but that could
		 * potentially break current behavior of eval"".
		 * GSAR 97-07-21
		 */
		t = s;
		if (*s == '\'' || *s == '"' || *s == '`') {
		    /* common case: get past first string, handling escapes */
		    for (t++; t < PL_bufend && *t != *s;)
			if (*t++ == '\\' && (*t == '\\' || *t == *s))
			    t++;
		    t++;
		}
		else if (*s == 'q') {
		    if (++t < PL_bufend
			&& (!isALNUM(*t)
			    || ((*t == 'q' || *t == 'x') && ++t < PL_bufend
				&& !isALNUM(*t))))
		    {
			char *tmps;
			char open, close, term;
			I32 brackets = 1;

			while (t < PL_bufend && isSPACE(*t))
			    t++;
			term = *t;
			open = term;
			if (term && (tmps = strchr("([{< )]}> )]}>",term)))
			    term = tmps[5];
			close = term;
			if (open == close)
			    for (t++; t < PL_bufend; t++) {
				if (*t == '\\' && t+1 < PL_bufend && open != '\\')
				    t++;
				else if (*t == open)
				    break;
			    }
			else
			    for (t++; t < PL_bufend; t++) {
				if (*t == '\\' && t+1 < PL_bufend)
				    t++;
				else if (*t == close && --brackets <= 0)
				    break;
				else if (*t == open)
				    brackets++;
			    }
		    }
		    t++;
		}
		else if (isALNUM_lazy_if(t,UTF)) {
		    t += UTF8SKIP(t);
		    while (t < PL_bufend && isALNUM_lazy_if(t,UTF))
			 t += UTF8SKIP(t);
		}
		while (t < PL_bufend && isSPACE(*t))
		    t++;
		/* if comma follows first term, call it an anon hash */
		/* XXX it could be a comma expression with loop modifiers */
		if (t < PL_bufend && ((*t == ',' && (*s == 'q' || !isLOWER(*s)))
				   || (*t == '=' && t[1] == '>')))
		    OPERATOR(HASHBRACK);
		if (PL_expect == XREF)
		    PL_expect = XTERM;
		else {
		    PL_lex_brackstack[PL_lex_brackets-1] = XSTATE;
		    PL_expect = XSTATE;
		}
	    }
	    break;
	}
	yylval.ival = CopLINE(PL_curcop);
	if (isSPACE(*s) || *s == '#')
	    PL_copline = NOLINE;   /* invalidate current command line number */
	TOKEN('{');
    case '}':
      rightbracket:
	s++;
	if (PL_lex_brackets <= 0)
	    yyerror("Unmatched right curly bracket");
	else
	    PL_expect = (expectation)PL_lex_brackstack[--PL_lex_brackets];
	if (PL_lex_brackets < PL_lex_formbrack && PL_lex_state != LEX_INTERPNORMAL)
	    PL_lex_formbrack = 0;
	if (PL_lex_state == LEX_INTERPNORMAL) {
	    if (PL_lex_brackets == 0) {
		if (PL_expect & XFAKEBRACK) {
		    PL_expect &= XENUMMASK;
		    PL_lex_state = LEX_INTERPEND;
		    PL_bufptr = s;
		    return yylex();	/* ignore fake brackets */
		}
		if (*s == '-' && s[1] == '>')
		    PL_lex_state = LEX_INTERPENDMAYBE;
		else if (*s != '[' && *s != '{')
		    PL_lex_state = LEX_INTERPEND;
	    }
	}
	if (PL_expect & XFAKEBRACK) {
	    PL_expect &= XENUMMASK;
	    PL_bufptr = s;
	    return yylex();		/* ignore fake brackets */
	}
	force_next('}');
	TOKEN(';');
    case '&':
	s++;
	tmp = *s++;
	if (tmp == '&')
	    AOPERATOR(ANDAND);
	s--;
	if (PL_expect == XOPERATOR) {
	    if (ckWARN(WARN_SEMICOLON)
		&& isIDFIRST_lazy_if(s,UTF) && PL_bufptr == PL_linestart)
	    {
		CopLINE_dec(PL_curcop);
		Perl_warner(aTHX_ WARN_SEMICOLON, PL_warn_nosemi);
		CopLINE_inc(PL_curcop);
	    }
	    BAop(OP_BIT_AND);
	}

	s = scan_ident(s - 1, PL_bufend, PL_tokenbuf, sizeof PL_tokenbuf, TRUE);
	if (*PL_tokenbuf) {
	    PL_expect = XOPERATOR;
	    force_ident(PL_tokenbuf, '&');
	}
	else
	    PREREF('&');
	yylval.ival = (OPpENTERSUB_AMPER<<8);
	TERM('&');

    case '|':
	s++;
	tmp = *s++;
	if (tmp == '|')
	    AOPERATOR(OROR);
	s--;
	BOop(OP_BIT_OR);
    case '=':
	s++;
	tmp = *s++;
	if (tmp == '=')
	    Eop(OP_EQ);
	if (tmp == '>')
	    OPERATOR(',');
	if (tmp == '~')
	    PMop(OP_MATCH);
	if (ckWARN(WARN_SYNTAX) && tmp && isSPACE(*s) && strchr("+-*/%.^&|<",tmp))
	    Perl_warner(aTHX_ WARN_SYNTAX, "Reversed %c= operator",(int)tmp);
	s--;
	if (PL_expect == XSTATE && isALPHA(tmp) &&
		(s == PL_linestart+1 || s[-2] == '\n') )
	{
	    if (PL_in_eval && !PL_rsfp) {
		d = PL_bufend;
		while (s < d) {
		    if (*s++ == '\n') {
			incline(s);
			if (strnEQ(s,"=cut",4)) {
			    s = strchr(s,'\n');
			    if (s)
				s++;
			    else
				s = d;
			    incline(s);
			    goto retry;
			}
		    }
		}
		goto retry;
	    }
	    s = PL_bufend;
	    PL_doextract = TRUE;
	    goto retry;
	}
	if (PL_lex_brackets < PL_lex_formbrack) {
	    char *t;
#ifdef PERL_STRICT_CR
	    for (t = s; SPACE_OR_TAB(*t); t++) ;
#else
	    for (t = s; SPACE_OR_TAB(*t) || *t == '\r'; t++) ;
#endif
	    if (*t == '\n' || *t == '#') {
		s--;
		PL_expect = XBLOCK;
		goto leftbracket;
	    }
	}
	yylval.ival = 0;
	OPERATOR(ASSIGNOP);
    case '!':
	s++;
	tmp = *s++;
	if (tmp == '=')
	    Eop(OP_NE);
	if (tmp == '~')
	    PMop(OP_NOT);
	s--;
	OPERATOR('!');
    case '<':
	if (PL_expect != XOPERATOR) {
	    if (s[1] != '<' && !strchr(s,'>'))
		check_uni();
	    if (s[1] == '<')
		s = scan_heredoc(s);
	    else
		s = scan_inputsymbol(s);
	    TERM(sublex_start());
	}
	s++;
	tmp = *s++;
	if (tmp == '<')
	    SHop(OP_LEFT_SHIFT);
	if (tmp == '=') {
	    tmp = *s++;
	    if (tmp == '>')
		Eop(OP_NCMP);
	    s--;
	    Rop(OP_LE);
	}
	s--;
	Rop(OP_LT);
    case '>':
	s++;
	tmp = *s++;
	if (tmp == '>')
	    SHop(OP_RIGHT_SHIFT);
	if (tmp == '=')
	    Rop(OP_GE);
	s--;
	Rop(OP_GT);

    case '$':
	CLINE;

	if (PL_expect == XOPERATOR) {
	    if (PL_lex_formbrack && PL_lex_brackets == PL_lex_formbrack) {
		PL_expect = XTERM;
		depcom();
		return ','; /* grandfather non-comma-format format */
	    }
	}

	if (s[1] == '#' && (isIDFIRST_lazy_if(s+2,UTF) || strchr("{$:+-", s[2]))) {
	    PL_tokenbuf[0] = '@';
	    s = scan_ident(s + 1, PL_bufend, PL_tokenbuf + 1,
			   sizeof PL_tokenbuf - 1, FALSE);
	    if (PL_expect == XOPERATOR)
		no_op("Array length", s);
	    if (!PL_tokenbuf[1])
		PREREF(DOLSHARP);
	    PL_expect = XOPERATOR;
	    PL_pending_ident = '#';
	    TOKEN(DOLSHARP);
	}

	PL_tokenbuf[0] = '$';
	s = scan_ident(s, PL_bufend, PL_tokenbuf + 1,
		       sizeof PL_tokenbuf - 1, FALSE);
	if (PL_expect == XOPERATOR)
	    no_op("Scalar", s);
	if (!PL_tokenbuf[1]) {
	    if (s == PL_bufend)
		yyerror("Final $ should be \\$ or $name");
	    PREREF('$');
	}

	/* This kludge not intended to be bulletproof. */
	if (PL_tokenbuf[1] == '[' && !PL_tokenbuf[2]) {
	    yylval.opval = newSVOP(OP_CONST, 0,
				   newSViv(PL_compiling.cop_arybase));
	    yylval.opval->op_private = OPpCONST_ARYBASE;
	    TERM(THING);
	}

	d = s;
	tmp = (I32)*s;
	if (PL_lex_state == LEX_NORMAL)
	    s = skipspace(s);

	if ((PL_expect != XREF || PL_oldoldbufptr == PL_last_lop) && intuit_more(s)) {
	    char *t;
	    if (*s == '[') {
		PL_tokenbuf[0] = '@';
		if (ckWARN(WARN_SYNTAX)) {
		    for(t = s + 1;
			isSPACE(*t) || isALNUM_lazy_if(t,UTF) || *t == '$';
			t++) ;
		    if (*t++ == ',') {
			PL_bufptr = skipspace(PL_bufptr);
			while (t < PL_bufend && *t != ']')
			    t++;
			Perl_warner(aTHX_ WARN_SYNTAX,
				"Multidimensional syntax %.*s not supported",
			     	(t - PL_bufptr) + 1, PL_bufptr);
		    }
		}
	    }
	    else if (*s == '{') {
		PL_tokenbuf[0] = '%';
		if (ckWARN(WARN_SYNTAX) && strEQ(PL_tokenbuf+1, "SIG") &&
		    (t = strchr(s, '}')) && (t = strchr(t, '=')))
		{
		    char tmpbuf[sizeof PL_tokenbuf];
		    STRLEN len;
		    for (t++; isSPACE(*t); t++) ;
		    if (isIDFIRST_lazy_if(t,UTF)) {
			t = scan_word(t, tmpbuf, sizeof tmpbuf, TRUE, &len);
		        for (; isSPACE(*t); t++) ;
			if (*t == ';' && get_cv(tmpbuf, FALSE))
			    Perl_warner(aTHX_ WARN_SYNTAX,
				"You need to quote \"%s\"", tmpbuf);
		    }
		}
	    }
	}

	PL_expect = XOPERATOR;
	if (PL_lex_state == LEX_NORMAL && isSPACE((char)tmp)) {
	    bool islop = (PL_last_lop == PL_oldoldbufptr);
	    if (!islop || PL_last_lop_op == OP_GREPSTART)
		PL_expect = XOPERATOR;
	    else if (strchr("$@\"'`q", *s))
		PL_expect = XTERM;		/* e.g. print $fh "foo" */
	    else if (strchr("&*<%", *s) && isIDFIRST_lazy_if(s+1,UTF))
		PL_expect = XTERM;		/* e.g. print $fh &sub */
	    else if (isIDFIRST_lazy_if(s,UTF)) {
		char tmpbuf[sizeof PL_tokenbuf];
		scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
		if ((tmp = keyword(tmpbuf, len))) {
		    /* binary operators exclude handle interpretations */
		    switch (tmp) {
		    case -KEY_x:
		    case -KEY_eq:
		    case -KEY_ne:
		    case -KEY_gt:
		    case -KEY_lt:
		    case -KEY_ge:
		    case -KEY_le:
		    case -KEY_cmp:
			break;
		    default:
			PL_expect = XTERM;	/* e.g. print $fh length() */
			break;
		    }
		}
		else {
		    GV *gv = gv_fetchpv(tmpbuf, FALSE, SVt_PVCV);
		    if (gv && GvCVu(gv))
			PL_expect = XTERM;	/* e.g. print $fh subr() */
		}
	    }
	    else if (isDIGIT(*s))
		PL_expect = XTERM;		/* e.g. print $fh 3 */
	    else if (*s == '.' && isDIGIT(s[1]))
		PL_expect = XTERM;		/* e.g. print $fh .3 */
	    else if (strchr("/?-+", *s) && !isSPACE(s[1]) && s[1] != '=')
		PL_expect = XTERM;		/* e.g. print $fh -1 */
	    else if (*s == '<' && s[1] == '<' && !isSPACE(s[2]) && s[2] != '=')
		PL_expect = XTERM;		/* print $fh <<"EOF" */
	}
	PL_pending_ident = '$';
	TOKEN('$');

    case '@':
	if (PL_expect == XOPERATOR)
	    no_op("Array", s);
	PL_tokenbuf[0] = '@';
	s = scan_ident(s, PL_bufend, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1, FALSE);
	if (!PL_tokenbuf[1]) {
	    if (s == PL_bufend)
		yyerror("Final @ should be \\@ or @name");
	    PREREF('@');
	}
	if (PL_lex_state == LEX_NORMAL)
	    s = skipspace(s);
	if ((PL_expect != XREF || PL_oldoldbufptr == PL_last_lop) && intuit_more(s)) {
	    if (*s == '{')
		PL_tokenbuf[0] = '%';

	    /* Warn about @ where they meant $. */
	    if (ckWARN(WARN_SYNTAX)) {
		if (*s == '[' || *s == '{') {
		    char *t = s + 1;
		    while (*t && (isALNUM_lazy_if(t,UTF) || strchr(" \t$#+-'\"", *t)))
			t++;
		    if (*t == '}' || *t == ']') {
			t++;
			PL_bufptr = skipspace(PL_bufptr);
			Perl_warner(aTHX_ WARN_SYNTAX,
			    "Scalar value %.*s better written as $%.*s",
			    t-PL_bufptr, PL_bufptr, t-PL_bufptr-1, PL_bufptr+1);
		    }
		}
	    }
	}
	PL_pending_ident = '@';
	TERM('@');

    case '/':			/* may either be division or pattern */
    case '?':			/* may either be conditional or pattern */
	if (PL_expect != XOPERATOR) {
	    /* Disable warning on "study /blah/" */
	    if (PL_oldoldbufptr == PL_last_uni
		&& (*PL_last_uni != 's' || s - PL_last_uni < 5
		    || memNE(PL_last_uni, "study", 5)
		    || isALNUM_lazy_if(PL_last_uni+5,UTF)))
		check_uni();
	    s = scan_pat(s,OP_MATCH);
	    TERM(sublex_start());
	}
	tmp = *s++;
	if (tmp == '/')
	    Mop(OP_DIVIDE);
	OPERATOR(tmp);

    case '.':
	if (PL_lex_formbrack && PL_lex_brackets == PL_lex_formbrack
#ifdef PERL_STRICT_CR
	    && s[1] == '\n'
#else
	    && (s[1] == '\n' || (s[1] == '\r' && s[2] == '\n'))
#endif
	    && (s == PL_linestart || s[-1] == '\n') )
	{
	    PL_lex_formbrack = 0;
	    PL_expect = XSTATE;
	    goto rightbracket;
	}
	if (PL_expect == XOPERATOR || !isDIGIT(s[1])) {
	    tmp = *s++;
	    if (*s == tmp) {
		s++;
		if (*s == tmp) {
		    s++;
		    yylval.ival = OPf_SPECIAL;
		}
		else
		    yylval.ival = 0;
		OPERATOR(DOTDOT);
	    }
	    if (PL_expect != XOPERATOR)
		check_uni();
	    Aop(OP_CONCAT);
	}
	/* FALL THROUGH */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
	s = scan_num(s, &yylval);
        DEBUG_T( { PerlIO_printf(Perl_debug_log,
                    "### Saw number in '%s'\n", s);
        } )
	if (PL_expect == XOPERATOR)
	    no_op("Number",s);
	TERM(THING);

    case '\'':
	s = scan_str(s,FALSE,FALSE);
        DEBUG_T( { PerlIO_printf(Perl_debug_log,
                    "### Saw string before '%s'\n", s);
        } )
	if (PL_expect == XOPERATOR) {
	    if (PL_lex_formbrack && PL_lex_brackets == PL_lex_formbrack) {
		PL_expect = XTERM;
		depcom();
		return ',';	/* grandfather non-comma-format format */
	    }
	    else
		no_op("String",s);
	}
	if (!s)
	    missingterm((char*)0);
	yylval.ival = OP_CONST;
	TERM(sublex_start());

    case '"':
	s = scan_str(s,FALSE,FALSE);
        DEBUG_T( { PerlIO_printf(Perl_debug_log,
                    "### Saw string before '%s'\n", s);
        } )
	if (PL_expect == XOPERATOR) {
	    if (PL_lex_formbrack && PL_lex_brackets == PL_lex_formbrack) {
		PL_expect = XTERM;
		depcom();
		return ',';	/* grandfather non-comma-format format */
	    }
	    else
		no_op("String",s);
	}
	if (!s)
	    missingterm((char*)0);
	yylval.ival = OP_CONST;
	for (d = SvPV(PL_lex_stuff, len); len; len--, d++) {
	    if (*d == '$' || *d == '@' || *d == '\\' || UTF8_IS_CONTINUED(*d)) {
		yylval.ival = OP_STRINGIFY;
		break;
	    }
	}
	TERM(sublex_start());

    case '`':
	s = scan_str(s,FALSE,FALSE);
        DEBUG_T( { PerlIO_printf(Perl_debug_log,
                    "### Saw backtick string before '%s'\n", s);
        } )
	if (PL_expect == XOPERATOR)
	    no_op("Backticks",s);
	if (!s)
	    missingterm((char*)0);
	yylval.ival = OP_BACKTICK;
	set_csh();
	TERM(sublex_start());

    case '\\':
	s++;
	if (ckWARN(WARN_SYNTAX) && PL_lex_inwhat && isDIGIT(*s))
	    Perl_warner(aTHX_ WARN_SYNTAX,"Can't use \\%c to mean $%c in expression",
			*s, *s);
	if (PL_expect == XOPERATOR)
	    no_op("Backslash",s);
	OPERATOR(REFGEN);

    case 'v':
	if (isDIGIT(s[1]) && PL_expect != XOPERATOR) {
	    char *start = s;
	    start++;
	    start++;
	    while (isDIGIT(*start) || *start == '_')
		start++;
	    if (*start == '.' && isDIGIT(start[1])) {
		s = scan_num(s, &yylval);
		TERM(THING);
	    }
	    /* avoid v123abc() or $h{v1}, allow C<print v10;> */
	    else if (!isALPHA(*start) && (PL_expect == XTERM || PL_expect == XREF)) {
		char c = *start;
		GV *gv;
		*start = '\0';
		gv = gv_fetchpv(s, FALSE, SVt_PVCV);
		*start = c;
		if (!gv) {
		    s = scan_num(s, &yylval);
		    TERM(THING);
		}
	    }
	}
	goto keylookup;
    case 'x':
	if (isDIGIT(s[1]) && PL_expect == XOPERATOR) {
	    s++;
	    Mop(OP_REPEAT);
	}
	goto keylookup;

    case '_':
    case 'a': case 'A':
    case 'b': case 'B':
    case 'c': case 'C':
    case 'd': case 'D':
    case 'e': case 'E':
    case 'f': case 'F':
    case 'g': case 'G':
    case 'h': case 'H':
    case 'i': case 'I':
    case 'j': case 'J':
    case 'k': case 'K':
    case 'l': case 'L':
    case 'm': case 'M':
    case 'n': case 'N':
    case 'o': case 'O':
    case 'p': case 'P':
    case 'q': case 'Q':
    case 'r': case 'R':
    case 's': case 'S':
    case 't': case 'T':
    case 'u': case 'U':
	      case 'V':
    case 'w': case 'W':
	      case 'X':
    case 'y': case 'Y':
    case 'z': case 'Z':

      keylookup: {
	gv = Nullgv;
	gvp = 0;

	PL_bufptr = s;
	s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE, &len);

	/* Some keywords can be followed by any delimiter, including ':' */
	tmp = ((len == 1 && strchr("msyq", PL_tokenbuf[0])) ||
	       (len == 2 && ((PL_tokenbuf[0] == 't' && PL_tokenbuf[1] == 'r') ||
			     (PL_tokenbuf[0] == 'q' &&
			      strchr("qwxr", PL_tokenbuf[1])))));

	/* x::* is just a word, unless x is "CORE" */
	if (!tmp && *s == ':' && s[1] == ':' && strNE(PL_tokenbuf, "CORE"))
	    goto just_a_word;

	d = s;
	while (d < PL_bufend && isSPACE(*d))
		d++;	/* no comments skipped here, or s### is misparsed */

	/* Is this a label? */
	if (!tmp && PL_expect == XSTATE
	      && d < PL_bufend && *d == ':' && *(d + 1) != ':') {
	    s = d + 1;
	    yylval.pval = savepv(PL_tokenbuf);
	    CLINE;
	    TOKEN(LABEL);
	}

	/* Check for keywords */
	tmp = keyword(PL_tokenbuf, len);

	/* Is this a word before a => operator? */
	if (*d == '=' && d[1] == '>') {
	    CLINE;
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0, newSVpv(PL_tokenbuf,0));
	    yylval.opval->op_private = OPpCONST_BARE;
	    if (UTF && !IN_BYTE && is_utf8_string((U8*)PL_tokenbuf, len))
	      SvUTF8_on(((SVOP*)yylval.opval)->op_sv);
	    TERM(WORD);
	}

	if (tmp < 0) {			/* second-class keyword? */
	    GV *ogv = Nullgv;	/* override (winner) */
	    GV *hgv = Nullgv;	/* hidden (loser) */
	    if (PL_expect != XOPERATOR && (*s != ':' || s[1] != ':')) {
		CV *cv;
		if ((gv = gv_fetchpv(PL_tokenbuf, FALSE, SVt_PVCV)) &&
		    (cv = GvCVu(gv)))
		{
		    if (GvIMPORTED_CV(gv))
			ogv = gv;
		    else if (! CvMETHOD(cv))
			hgv = gv;
		}
		if (!ogv &&
		    (gvp = (GV**)hv_fetch(PL_globalstash,PL_tokenbuf,len,FALSE)) &&
		    (gv = *gvp) != (GV*)&PL_sv_undef &&
		    GvCVu(gv) && GvIMPORTED_CV(gv))
		{
		    ogv = gv;
		}
	    }
	    if (ogv) {
		tmp = 0;		/* overridden by import or by GLOBAL */
	    }
	    else if (gv && !gvp
		     && -tmp==KEY_lock	/* XXX generalizable kludge */
		     && GvCVu(gv)
		     && !hv_fetch(GvHVn(PL_incgv), "Thread.pm", 9, FALSE))
	    {
		tmp = 0;		/* any sub overrides "weak" keyword */
	    }
	    else {			/* no override */
		tmp = -tmp;
		gv = Nullgv;
		gvp = 0;
		if (ckWARN(WARN_AMBIGUOUS) && hgv
		    && tmp != KEY_x && tmp != KEY_CORE)	/* never ambiguous */
		    Perl_warner(aTHX_ WARN_AMBIGUOUS,
		    	"Ambiguous call resolved as CORE::%s(), %s",
			 GvENAME(hgv), "qualify as such or use &");
	    }
	}

      reserved_word:
	switch (tmp) {

	default:			/* not a keyword */
	  just_a_word: {
		SV *sv;
		char lastchar = (PL_bufptr == PL_oldoldbufptr ? 0 : PL_bufptr[-1]);

		/* Get the rest if it looks like a package qualifier */

		if (*s == '\'' || (*s == ':' && s[1] == ':')) {
		    STRLEN morelen;
		    s = scan_word(s, PL_tokenbuf + len, sizeof PL_tokenbuf - len,
				  TRUE, &morelen);
		    if (!morelen)
			Perl_croak(aTHX_ "Bad name after %s%s", PL_tokenbuf,
				*s == '\'' ? "'" : "::");
		    len += morelen;
		}

		if (PL_expect == XOPERATOR) {
		    if (PL_bufptr == PL_linestart) {
			CopLINE_dec(PL_curcop);
			Perl_warner(aTHX_ WARN_SEMICOLON, PL_warn_nosemi);
			CopLINE_inc(PL_curcop);
		    }
		    else
			no_op("Bareword",s);
		}

		/* Look for a subroutine with this name in current package,
		   unless name is "Foo::", in which case Foo is a bearword
		   (and a package name). */

		if (len > 2 &&
		    PL_tokenbuf[len - 2] == ':' && PL_tokenbuf[len - 1] == ':')
		{
		    if (ckWARN(WARN_BAREWORD) && ! gv_fetchpv(PL_tokenbuf, FALSE, SVt_PVHV))
			Perl_warner(aTHX_ WARN_BAREWORD,
		  	    "Bareword \"%s\" refers to nonexistent package",
			     PL_tokenbuf);
		    len -= 2;
		    PL_tokenbuf[len] = '\0';
		    gv = Nullgv;
		    gvp = 0;
		}
		else {
		    len = 0;
		    if (!gv)
			gv = gv_fetchpv(PL_tokenbuf, FALSE, SVt_PVCV);
		}

		/* if we saw a global override before, get the right name */

		if (gvp) {
		    sv = newSVpvn("CORE::GLOBAL::",14);
		    sv_catpv(sv,PL_tokenbuf);
		}
		else
		    sv = newSVpv(PL_tokenbuf,0);

		/* Presume this is going to be a bareword of some sort. */

		CLINE;
		yylval.opval = (OP*)newSVOP(OP_CONST, 0, sv);
		yylval.opval->op_private = OPpCONST_BARE;

		/* And if "Foo::", then that's what it certainly is. */

		if (len)
		    goto safe_bareword;

		/* See if it's the indirect object for a list operator. */

		if (PL_oldoldbufptr &&
		    PL_oldoldbufptr < PL_bufptr &&
		    (PL_oldoldbufptr == PL_last_lop
		     || PL_oldoldbufptr == PL_last_uni) &&
		    /* NO SKIPSPACE BEFORE HERE! */
		    (PL_expect == XREF ||
		     ((PL_opargs[PL_last_lop_op] >> OASHIFT)& 7) == OA_FILEREF))
		{
		    bool immediate_paren = *s == '(';

		    /* (Now we can afford to cross potential line boundary.) */
		    s = skipspace(s);

		    /* Two barewords in a row may indicate method call. */

		    if ((isIDFIRST_lazy_if(s,UTF) || *s == '$') && (tmp=intuit_method(s,gv)))
			return tmp;

		    /* If not a declared subroutine, it's an indirect object. */
		    /* (But it's an indir obj regardless for sort.) */

		    if ((PL_last_lop_op == OP_SORT ||
                         (!immediate_paren && (!gv || !GvCVu(gv)))) &&
                        (PL_last_lop_op != OP_MAPSTART &&
			 PL_last_lop_op != OP_GREPSTART))
		    {
			PL_expect = (PL_last_lop == PL_oldoldbufptr) ? XTERM : XOPERATOR;
			goto bareword;
		    }
		}


		PL_expect = XOPERATOR;
		s = skipspace(s);

		/* Is this a word before a => operator? */
		if (*s == '=' && s[1] == '>') {
		    CLINE;
		    sv_setpv(((SVOP*)yylval.opval)->op_sv, PL_tokenbuf);
		    if (UTF && !IN_BYTE && is_utf8_string((U8*)PL_tokenbuf, len))
		      SvUTF8_on(((SVOP*)yylval.opval)->op_sv);
		    TERM(WORD);
		}

		/* If followed by a paren, it's certainly a subroutine. */
		if (*s == '(') {
		    CLINE;
		    if (gv && GvCVu(gv)) {
			for (d = s + 1; SPACE_OR_TAB(*d); d++) ;
			if (*d == ')' && (sv = cv_const_sv(GvCV(gv)))) {
			    s = d + 1;
			    goto its_constant;
			}
		    }
		    PL_nextval[PL_nexttoke].opval = yylval.opval;
		    PL_expect = XOPERATOR;
		    force_next(WORD);
		    yylval.ival = 0;
		    TOKEN('&');
		}

		/* If followed by var or block, call it a method (unless sub) */

		if ((*s == '$' || *s == '{') && (!gv || !GvCVu(gv))) {
		    PL_last_lop = PL_oldbufptr;
		    PL_last_lop_op = OP_METHOD;
		    PREBLOCK(METHOD);
		}

		/* If followed by a bareword, see if it looks like indir obj. */

		if ((isIDFIRST_lazy_if(s,UTF) || *s == '$') && (tmp = intuit_method(s,gv)))
		    return tmp;

		/* Not a method, so call it a subroutine (if defined) */

		if (gv && GvCVu(gv)) {
		    CV* cv;
		    if (lastchar == '-' && ckWARN_d(WARN_AMBIGUOUS))
			Perl_warner(aTHX_ WARN_AMBIGUOUS,
				"Ambiguous use of -%s resolved as -&%s()",
				PL_tokenbuf, PL_tokenbuf);
		    /* Check for a constant sub */
		    cv = GvCV(gv);
		    if ((sv = cv_const_sv(cv))) {
		  its_constant:
			SvREFCNT_dec(((SVOP*)yylval.opval)->op_sv);
			((SVOP*)yylval.opval)->op_sv = SvREFCNT_inc(sv);
			yylval.opval->op_private = 0;
			TOKEN(WORD);
		    }

		    /* Resolve to GV now. */
		    op_free(yylval.opval);
		    yylval.opval = newCVREF(0, newGVOP(OP_GV, 0, gv));
		    yylval.opval->op_private |= OPpENTERSUB_NOPAREN;
		    PL_last_lop = PL_oldbufptr;
		    PL_last_lop_op = OP_ENTERSUB;
		    /* Is there a prototype? */
		    if (SvPOK(cv)) {
			STRLEN len;
			char *proto = SvPV((SV*)cv, len);
			if (!len)
			    TERM(FUNC0SUB);
			if (strEQ(proto, "$"))
			    OPERATOR(UNIOPSUB);
			if (*proto == '&' && *s == '{') {
			    sv_setpv(PL_subname,"__ANON__");
			    PREBLOCK(LSTOPSUB);
			}
		    }
		    PL_nextval[PL_nexttoke].opval = yylval.opval;
		    PL_expect = XTERM;
		    force_next(WORD);
		    TOKEN(NOAMP);
		}

		/* Call it a bare word */

		if (PL_hints & HINT_STRICT_SUBS)
		    yylval.opval->op_private |= OPpCONST_STRICT;
		else {
		bareword:
		    if (ckWARN(WARN_RESERVED)) {
			if (lastchar != '-') {
			    for (d = PL_tokenbuf; *d && isLOWER(*d); d++) ;
			    if (!*d)
				Perl_warner(aTHX_ WARN_RESERVED, PL_warn_reserved,
				       PL_tokenbuf);
			}
		    }
		}

	    safe_bareword:
		if (lastchar && strchr("*%&", lastchar) && ckWARN_d(WARN_AMBIGUOUS)) {
		    Perl_warner(aTHX_ WARN_AMBIGUOUS,
		  	"Operator or semicolon missing before %c%s",
			lastchar, PL_tokenbuf);
		    Perl_warner(aTHX_ WARN_AMBIGUOUS,
			"Ambiguous use of %c resolved as operator %c",
			lastchar, lastchar);
		}
		TOKEN(WORD);
	    }

	case KEY___FILE__:
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0,
					newSVpv(CopFILE(PL_curcop),0));
	    TERM(THING);

	case KEY___LINE__:
            yylval.opval = (OP*)newSVOP(OP_CONST, 0,
                                    Perl_newSVpvf(aTHX_ "%"IVdf, (IV)CopLINE(PL_curcop)));
	    TERM(THING);

	case KEY___PACKAGE__:
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0,
					(PL_curstash
					 ? newSVsv(PL_curstname)
					 : &PL_sv_undef));
	    TERM(THING);

	case KEY___DATA__:
	case KEY___END__: {
	    GV *gv;

	    /*SUPPRESS 560*/
	    if (PL_rsfp && (!PL_in_eval || PL_tokenbuf[2] == 'D')) {
		char *pname = "main";
		if (PL_tokenbuf[2] == 'D')
		    pname = HvNAME(PL_curstash ? PL_curstash : PL_defstash);
		gv = gv_fetchpv(Perl_form(aTHX_ "%s::DATA", pname), TRUE, SVt_PVIO);
		GvMULTI_on(gv);
		if (!GvIO(gv))
		    GvIOp(gv) = newIO();
		IoIFP(GvIOp(gv)) = PL_rsfp;
#if defined(HAS_FCNTL) && defined(F_SETFD)
		{
		    int fd = PerlIO_fileno(PL_rsfp);
		    fcntl(fd,F_SETFD,fd >= 3);
		}
#endif
		/* Mark this internal pseudo-handle as clean */
		IoFLAGS(GvIOp(gv)) |= IOf_UNTAINT;
		if (PL_preprocess)
		    IoTYPE(GvIOp(gv)) = IoTYPE_PIPE;
		else if ((PerlIO*)PL_rsfp == PerlIO_stdin())
		    IoTYPE(GvIOp(gv)) = IoTYPE_STD;
		else
		    IoTYPE(GvIOp(gv)) = IoTYPE_RDONLY;
#if defined(WIN32) && !defined(PERL_TEXTMODE_SCRIPTS)
		/* if the script was opened in binmode, we need to revert
		 * it to text mode for compatibility; but only iff it has CRs
		 * XXX this is a questionable hack at best. */
		if (PL_bufend-PL_bufptr > 2
		    && PL_bufend[-1] == '\n' && PL_bufend[-2] == '\r')
		{
		    Off_t loc = 0;
		    if (IoTYPE(GvIOp(gv)) == IoTYPE_RDONLY) {
			loc = PerlIO_tell(PL_rsfp);
			(void)PerlIO_seek(PL_rsfp, 0L, 0);
		    }
		    if (PerlLIO_setmode(PerlIO_fileno(PL_rsfp), O_TEXT) != -1) {
#if defined(__BORLANDC__)
			/* XXX see note in do_binmode() */
			((FILE*)PL_rsfp)->flags &= ~_F_BIN;
#endif
			if (loc > 0)
			    PerlIO_seek(PL_rsfp, loc, 0);
		    }
		}
#endif
		PL_rsfp = Nullfp;
	    }
	    goto fake_eof;
	}

	case KEY_AUTOLOAD:
	case KEY_DESTROY:
	case KEY_BEGIN:
	case KEY_CHECK:
	case KEY_INIT:
	case KEY_END:
	    if (PL_expect == XSTATE) {
		s = PL_bufptr;
		goto really_sub;
	    }
	    goto just_a_word;

	case KEY_CORE:
	    if (*s == ':' && s[1] == ':') {
		s += 2;
		d = s;
		s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE, &len);
		if (!(tmp = keyword(PL_tokenbuf, len)))
		    Perl_croak(aTHX_ "CORE::%s is not a keyword", PL_tokenbuf);
		if (tmp < 0)
		    tmp = -tmp;
		goto reserved_word;
	    }
	    goto just_a_word;

	case KEY_abs:
	    UNI(OP_ABS);

	case KEY_alarm:
	    UNI(OP_ALARM);

	case KEY_accept:
	    LOP(OP_ACCEPT,XTERM);

	case KEY_and:
	    OPERATOR(ANDOP);

	case KEY_atan2:
	    LOP(OP_ATAN2,XTERM);

	case KEY_bind:
	    LOP(OP_BIND,XTERM);

	case KEY_binmode:
	    LOP(OP_BINMODE,XTERM);

	case KEY_bless:
	    LOP(OP_BLESS,XTERM);

	case KEY_chop:
	    UNI(OP_CHOP);

	case KEY_continue:
	    PREBLOCK(CONTINUE);

	case KEY_chdir:
	    (void)gv_fetchpv("ENV",TRUE, SVt_PVHV);	/* may use HOME */
	    UNI(OP_CHDIR);

	case KEY_close:
	    UNI(OP_CLOSE);

	case KEY_closedir:
	    UNI(OP_CLOSEDIR);

	case KEY_cmp:
	    Eop(OP_SCMP);

	case KEY_caller:
	    UNI(OP_CALLER);

	case KEY_crypt:
#ifdef FCRYPT
	    if (!PL_cryptseen) {
		PL_cryptseen = TRUE;
		init_des();
	    }
#endif
	    LOP(OP_CRYPT,XTERM);

	case KEY_chmod:
	    if (ckWARN(WARN_CHMOD)) {
		for (d = s; d < PL_bufend && (isSPACE(*d) || *d == '('); d++) ;
		if (*d != '0' && isDIGIT(*d))
		    Perl_warner(aTHX_ WARN_CHMOD,
		    		"chmod() mode argument is missing initial 0");
	    }
	    LOP(OP_CHMOD,XTERM);

	case KEY_chown:
	    LOP(OP_CHOWN,XTERM);

	case KEY_connect:
	    LOP(OP_CONNECT,XTERM);

	case KEY_chr:
	    UNI(OP_CHR);

	case KEY_cos:
	    UNI(OP_COS);

	case KEY_chroot:
	    UNI(OP_CHROOT);

	case KEY_do:
	    s = skipspace(s);
	    if (*s == '{')
		PRETERMBLOCK(DO);
	    if (*s != '\'')
		s = force_word(s,WORD,FALSE,TRUE,FALSE);
	    OPERATOR(DO);

	case KEY_die:
	    PL_hints |= HINT_BLOCK_SCOPE;
	    LOP(OP_DIE,XTERM);

	case KEY_defined:
	    UNI(OP_DEFINED);

	case KEY_delete:
	    UNI(OP_DELETE);

	case KEY_dbmopen:
	    gv_fetchpv("AnyDBM_File::ISA", GV_ADDMULTI, SVt_PVAV);
	    LOP(OP_DBMOPEN,XTERM);

	case KEY_dbmclose:
	    UNI(OP_DBMCLOSE);

	case KEY_dump:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_DUMP);

	case KEY_else:
	    PREBLOCK(ELSE);

	case KEY_elsif:
	    yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(ELSIF);

	case KEY_eq:
	    Eop(OP_SEQ);

	case KEY_exists:
	    UNI(OP_EXISTS);
	
	case KEY_exit:
	    UNI(OP_EXIT);

	case KEY_eval:
	    s = skipspace(s);
	    PL_expect = (*s == '{') ? XTERMBLOCK : XTERM;
	    UNIBRACK(OP_ENTEREVAL);

	case KEY_eof:
	    UNI(OP_EOF);

	case KEY_exp:
	    UNI(OP_EXP);

	case KEY_each:
	    UNI(OP_EACH);

	case KEY_exec:
	    set_csh();
	    LOP(OP_EXEC,XREF);

	case KEY_endhostent:
	    FUN0(OP_EHOSTENT);

	case KEY_endnetent:
	    FUN0(OP_ENETENT);

	case KEY_endservent:
	    FUN0(OP_ESERVENT);

	case KEY_endprotoent:
	    FUN0(OP_EPROTOENT);

	case KEY_endpwent:
	    FUN0(OP_EPWENT);

	case KEY_endgrent:
	    FUN0(OP_EGRENT);

	case KEY_for:
	case KEY_foreach:
	    yylval.ival = CopLINE(PL_curcop);
	    s = skipspace(s);
	    if (PL_expect == XSTATE && isIDFIRST_lazy_if(s,UTF)) {
		char *p = s;
		if ((PL_bufend - p) >= 3 &&
		    strnEQ(p, "my", 2) && isSPACE(*(p + 2)))
		    p += 2;
		else if ((PL_bufend - p) >= 4 &&
		    strnEQ(p, "our", 3) && isSPACE(*(p + 3)))
		    p += 3;
		p = skipspace(p);
		if (isIDFIRST_lazy_if(p,UTF)) {
		    p = scan_ident(p, PL_bufend,
			PL_tokenbuf, sizeof PL_tokenbuf, TRUE);
		    p = skipspace(p);
		}
		if (*p != '$')
		    Perl_croak(aTHX_ "Missing $ on loop variable");
	    }
	    OPERATOR(FOR);

	case KEY_formline:
	    LOP(OP_FORMLINE,XTERM);

	case KEY_fork:
	    FUN0(OP_FORK);

	case KEY_fcntl:
	    LOP(OP_FCNTL,XTERM);

	case KEY_fileno:
	    UNI(OP_FILENO);

	case KEY_flock:
	    LOP(OP_FLOCK,XTERM);

	case KEY_gt:
	    Rop(OP_SGT);

	case KEY_ge:
	    Rop(OP_SGE);

	case KEY_grep:
	    LOP(OP_GREPSTART, XREF);

	case KEY_goto:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_GOTO);

	case KEY_gmtime:
	    UNI(OP_GMTIME);

	case KEY_getc:
	    UNI(OP_GETC);

	case KEY_getppid:
	    FUN0(OP_GETPPID);

	case KEY_getpgrp:
	    UNI(OP_GETPGRP);

	case KEY_getpriority:
	    LOP(OP_GETPRIORITY,XTERM);

	case KEY_getprotobyname:
	    UNI(OP_GPBYNAME);

	case KEY_getprotobynumber:
	    LOP(OP_GPBYNUMBER,XTERM);

	case KEY_getprotoent:
	    FUN0(OP_GPROTOENT);

	case KEY_getpwent:
	    FUN0(OP_GPWENT);

	case KEY_getpwnam:
	    UNI(OP_GPWNAM);

	case KEY_getpwuid:
	    UNI(OP_GPWUID);

	case KEY_getpeername:
	    UNI(OP_GETPEERNAME);

	case KEY_gethostbyname:
	    UNI(OP_GHBYNAME);

	case KEY_gethostbyaddr:
	    LOP(OP_GHBYADDR,XTERM);

	case KEY_gethostent:
	    FUN0(OP_GHOSTENT);

	case KEY_getnetbyname:
	    UNI(OP_GNBYNAME);

	case KEY_getnetbyaddr:
	    LOP(OP_GNBYADDR,XTERM);

	case KEY_getnetent:
	    FUN0(OP_GNETENT);

	case KEY_getservbyname:
	    LOP(OP_GSBYNAME,XTERM);

	case KEY_getservbyport:
	    LOP(OP_GSBYPORT,XTERM);

	case KEY_getservent:
	    FUN0(OP_GSERVENT);

	case KEY_getsockname:
	    UNI(OP_GETSOCKNAME);

	case KEY_getsockopt:
	    LOP(OP_GSOCKOPT,XTERM);

	case KEY_getgrent:
	    FUN0(OP_GGRENT);

	case KEY_getgrnam:
	    UNI(OP_GGRNAM);

	case KEY_getgrgid:
	    UNI(OP_GGRGID);

	case KEY_getlogin:
	    FUN0(OP_GETLOGIN);

	case KEY_glob:
	    set_csh();
	    LOP(OP_GLOB,XTERM);

	case KEY_hex:
	    UNI(OP_HEX);

	case KEY_if:
	    yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(IF);

	case KEY_index:
	    LOP(OP_INDEX,XTERM);

	case KEY_int:
	    UNI(OP_INT);

	case KEY_ioctl:
	    LOP(OP_IOCTL,XTERM);

	case KEY_join:
	    LOP(OP_JOIN,XTERM);

	case KEY_keys:
	    UNI(OP_KEYS);

	case KEY_kill:
	    LOP(OP_KILL,XTERM);

	case KEY_last:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_LAST);
	
	case KEY_lc:
	    UNI(OP_LC);

	case KEY_lcfirst:
	    UNI(OP_LCFIRST);

	case KEY_local:
	    yylval.ival = 0;
	    OPERATOR(LOCAL);

	case KEY_length:
	    UNI(OP_LENGTH);

	case KEY_lt:
	    Rop(OP_SLT);

	case KEY_le:
	    Rop(OP_SLE);

	case KEY_localtime:
	    UNI(OP_LOCALTIME);

	case KEY_log:
	    UNI(OP_LOG);

	case KEY_link:
	    LOP(OP_LINK,XTERM);

	case KEY_listen:
	    LOP(OP_LISTEN,XTERM);

	case KEY_lock:
	    UNI(OP_LOCK);

	case KEY_lstat:
	    UNI(OP_LSTAT);

	case KEY_m:
	    s = scan_pat(s,OP_MATCH);
	    TERM(sublex_start());

	case KEY_map:
	    LOP(OP_MAPSTART, XREF);

	case KEY_mkdir:
	    LOP(OP_MKDIR,XTERM);

	case KEY_msgctl:
	    LOP(OP_MSGCTL,XTERM);

	case KEY_msgget:
	    LOP(OP_MSGGET,XTERM);

	case KEY_msgrcv:
	    LOP(OP_MSGRCV,XTERM);

	case KEY_msgsnd:
	    LOP(OP_MSGSND,XTERM);

	case KEY_our:
	case KEY_my:
	    PL_in_my = tmp;
	    s = skipspace(s);
	    if (isIDFIRST_lazy_if(s,UTF)) {
		s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, TRUE, &len);
		if (len == 3 && strnEQ(PL_tokenbuf, "sub", 3))
		    goto really_sub;
		PL_in_my_stash = find_in_my_stash(PL_tokenbuf, len);
		if (!PL_in_my_stash) {
		    char tmpbuf[1024];
		    PL_bufptr = s;
		    sprintf(tmpbuf, "No such class %.1000s", PL_tokenbuf);
		    yyerror(tmpbuf);
		}
	    }
	    yylval.ival = 1;
	    OPERATOR(MY);

	case KEY_next:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_NEXT);

	case KEY_ne:
	    Eop(OP_SNE);

	case KEY_no:
	    if (PL_expect != XSTATE)
		yyerror("\"no\" not allowed in expression");
	    s = force_word(s,WORD,FALSE,TRUE,FALSE);
	    s = force_version(s);
	    yylval.ival = 0;
	    OPERATOR(USE);

	case KEY_not:
	    if (*s == '(' || (s = skipspace(s), *s == '('))
		FUN1(OP_NOT);
	    else
		OPERATOR(NOTOP);

	case KEY_open:
	    s = skipspace(s);
	    if (isIDFIRST_lazy_if(s,UTF)) {
		char *t;
		for (d = s; isALNUM_lazy_if(d,UTF); d++) ;
		t = skipspace(d);
		if (strchr("|&*+-=!?:.", *t) && ckWARN_d(WARN_PRECEDENCE))
		    Perl_warner(aTHX_ WARN_PRECEDENCE,
			   "Precedence problem: open %.*s should be open(%.*s)",
			    d-s,s, d-s,s);
	    }
	    LOP(OP_OPEN,XTERM);

	case KEY_or:
	    yylval.ival = OP_OR;
	    OPERATOR(OROP);

	case KEY_ord:
	    UNI(OP_ORD);

	case KEY_oct:
	    UNI(OP_OCT);

	case KEY_opendir:
	    LOP(OP_OPEN_DIR,XTERM);

	case KEY_print:
	    checkcomma(s,PL_tokenbuf,"filehandle");
	    LOP(OP_PRINT,XREF);

	case KEY_printf:
	    checkcomma(s,PL_tokenbuf,"filehandle");
	    LOP(OP_PRTF,XREF);

	case KEY_prototype:
	    UNI(OP_PROTOTYPE);

	case KEY_push:
	    LOP(OP_PUSH,XTERM);

	case KEY_pop:
	    UNI(OP_POP);

	case KEY_pos:
	    UNI(OP_POS);
	
	case KEY_pack:
	    LOP(OP_PACK,XTERM);

	case KEY_package:
	    s = force_word(s,WORD,FALSE,TRUE,FALSE);
	    OPERATOR(PACKAGE);

	case KEY_pipe:
	    LOP(OP_PIPE_OP,XTERM);

	case KEY_q:
	    s = scan_str(s,FALSE,FALSE);
	    if (!s)
		missingterm((char*)0);
	    yylval.ival = OP_CONST;
	    TERM(sublex_start());

	case KEY_quotemeta:
	    UNI(OP_QUOTEMETA);

	case KEY_qw:
	    s = scan_str(s,FALSE,FALSE);
	    if (!s)
		missingterm((char*)0);
	    force_next(')');
	    if (SvCUR(PL_lex_stuff)) {
		OP *words = Nullop;
		int warned = 0;
		d = SvPV_force(PL_lex_stuff, len);
		while (len) {
		    SV *sv;
		    for (; isSPACE(*d) && len; --len, ++d) ;
		    if (len) {
			char *b = d;
			if (!warned && ckWARN(WARN_QW)) {
			    for (; !isSPACE(*d) && len; --len, ++d) {
				if (*d == ',') {
				    Perl_warner(aTHX_ WARN_QW,
					"Possible attempt to separate words with commas");
				    ++warned;
				}
				else if (*d == '#') {
				    Perl_warner(aTHX_ WARN_QW,
					"Possible attempt to put comments in qw() list");
				    ++warned;
				}
			    }
			}
			else {
			    for (; !isSPACE(*d) && len; --len, ++d) ;
			}
			sv = newSVpvn(b, d-b);
			if (DO_UTF8(PL_lex_stuff))
			    SvUTF8_on(sv);
			words = append_elem(OP_LIST, words,
					    newSVOP(OP_CONST, 0, tokeq(sv)));
		    }
		}
		if (words) {
		    PL_nextval[PL_nexttoke].opval = words;
		    force_next(THING);
		}
	    }
	    if (PL_lex_stuff) {
		SvREFCNT_dec(PL_lex_stuff);
		PL_lex_stuff = Nullsv;
	    }
	    PL_expect = XTERM;
	    TOKEN('(');

	case KEY_qq:
	    s = scan_str(s,FALSE,FALSE);
	    if (!s)
		missingterm((char*)0);
	    yylval.ival = OP_STRINGIFY;
	    if (SvIVX(PL_lex_stuff) == '\'')
		SvIVX(PL_lex_stuff) = 0;	/* qq'$foo' should intepolate */
	    TERM(sublex_start());

	case KEY_qr:
	    s = scan_pat(s,OP_QR);
	    TERM(sublex_start());

	case KEY_qx:
	    s = scan_str(s,FALSE,FALSE);
	    if (!s)
		missingterm((char*)0);
	    yylval.ival = OP_BACKTICK;
	    set_csh();
	    TERM(sublex_start());

	case KEY_return:
	    OLDLOP(OP_RETURN);

	case KEY_require:
	    s = skipspace(s);
	    if (isDIGIT(*s) || (*s == 'v' && isDIGIT(s[1]))) {
		s = force_version(s);
	    }
	    else {
		*PL_tokenbuf = '\0';
		s = force_word(s,WORD,TRUE,TRUE,FALSE);
		if (isIDFIRST_lazy_if(PL_tokenbuf,UTF))
		    gv_stashpvn(PL_tokenbuf, strlen(PL_tokenbuf), TRUE);
		else if (*s == '<')
		    yyerror("<> should be quotes");
	    }
	    UNI(OP_REQUIRE);

	case KEY_reset:
	    UNI(OP_RESET);

	case KEY_redo:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_REDO);

	case KEY_rename:
	    LOP(OP_RENAME,XTERM);

	case KEY_rand:
	    UNI(OP_RAND);

	case KEY_rmdir:
	    UNI(OP_RMDIR);

	case KEY_rindex:
	    LOP(OP_RINDEX,XTERM);

	case KEY_read:
	    LOP(OP_READ,XTERM);

	case KEY_readdir:
	    UNI(OP_READDIR);

	case KEY_readline:
	    set_csh();
	    UNI(OP_READLINE);

	case KEY_readpipe:
	    set_csh();
	    UNI(OP_BACKTICK);

	case KEY_rewinddir:
	    UNI(OP_REWINDDIR);

	case KEY_recv:
	    LOP(OP_RECV,XTERM);

	case KEY_reverse:
	    LOP(OP_REVERSE,XTERM);

	case KEY_readlink:
	    UNI(OP_READLINK);

	case KEY_ref:
	    UNI(OP_REF);

	case KEY_s:
	    s = scan_subst(s);
	    if (yylval.opval)
		TERM(sublex_start());
	    else
		TOKEN(1);	/* force error */

	case KEY_chomp:
	    UNI(OP_CHOMP);
	
	case KEY_scalar:
	    UNI(OP_SCALAR);

	case KEY_select:
	    LOP(OP_SELECT,XTERM);

	case KEY_seek:
	    LOP(OP_SEEK,XTERM);

	case KEY_semctl:
	    LOP(OP_SEMCTL,XTERM);

	case KEY_semget:
	    LOP(OP_SEMGET,XTERM);

	case KEY_semop:
	    LOP(OP_SEMOP,XTERM);

	case KEY_send:
	    LOP(OP_SEND,XTERM);

	case KEY_setpgrp:
	    LOP(OP_SETPGRP,XTERM);

	case KEY_setpriority:
	    LOP(OP_SETPRIORITY,XTERM);

	case KEY_sethostent:
	    UNI(OP_SHOSTENT);

	case KEY_setnetent:
	    UNI(OP_SNETENT);

	case KEY_setservent:
	    UNI(OP_SSERVENT);

	case KEY_setprotoent:
	    UNI(OP_SPROTOENT);

	case KEY_setpwent:
	    FUN0(OP_SPWENT);

	case KEY_setgrent:
	    FUN0(OP_SGRENT);

	case KEY_seekdir:
	    LOP(OP_SEEKDIR,XTERM);

	case KEY_setsockopt:
	    LOP(OP_SSOCKOPT,XTERM);

	case KEY_shift:
	    UNI(OP_SHIFT);

	case KEY_shmctl:
	    LOP(OP_SHMCTL,XTERM);

	case KEY_shmget:
	    LOP(OP_SHMGET,XTERM);

	case KEY_shmread:
	    LOP(OP_SHMREAD,XTERM);

	case KEY_shmwrite:
	    LOP(OP_SHMWRITE,XTERM);

	case KEY_shutdown:
	    LOP(OP_SHUTDOWN,XTERM);

	case KEY_sin:
	    UNI(OP_SIN);

	case KEY_sleep:
	    UNI(OP_SLEEP);

	case KEY_socket:
	    LOP(OP_SOCKET,XTERM);

	case KEY_socketpair:
	    LOP(OP_SOCKPAIR,XTERM);

	case KEY_sort:
	    checkcomma(s,PL_tokenbuf,"subroutine name");
	    s = skipspace(s);
	    if (*s == ';' || *s == ')')		/* probably a close */
		Perl_croak(aTHX_ "sort is now a reserved word");
	    PL_expect = XTERM;
	    s = force_word(s,WORD,TRUE,TRUE,FALSE);
	    LOP(OP_SORT,XREF);

	case KEY_split:
	    LOP(OP_SPLIT,XTERM);

	case KEY_sprintf:
	    LOP(OP_SPRINTF,XTERM);

	case KEY_splice:
	    LOP(OP_SPLICE,XTERM);

	case KEY_sqrt:
	    UNI(OP_SQRT);

	case KEY_srand:
	    UNI(OP_SRAND);

	case KEY_stat:
	    UNI(OP_STAT);

	case KEY_study:
	    UNI(OP_STUDY);

	case KEY_substr:
	    LOP(OP_SUBSTR,XTERM);

	case KEY_format:
	case KEY_sub:
	  really_sub:
	    {
		char tmpbuf[sizeof PL_tokenbuf];
		SSize_t tboffset;
		expectation attrful;
		bool have_name, have_proto;
		int key = tmp;

		s = skipspace(s);

		if (isIDFIRST_lazy_if(s,UTF) || *s == '\'' ||
		    (*s == ':' && s[1] == ':'))
		{
		    PL_expect = XBLOCK;
		    attrful = XATTRBLOCK;
		    /* remember buffer pos'n for later force_word */
		    tboffset = s - PL_oldbufptr;
		    d = scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
		    if (strchr(tmpbuf, ':'))
			sv_setpv(PL_subname, tmpbuf);
		    else {
			sv_setsv(PL_subname,PL_curstname);
			sv_catpvn(PL_subname,"::",2);
			sv_catpvn(PL_subname,tmpbuf,len);
		    }
		    s = skipspace(d);
		    have_name = TRUE;
		}
		else {
		    if (key == KEY_my)
			Perl_croak(aTHX_ "Missing name in \"my sub\"");
		    PL_expect = XTERMBLOCK;
		    attrful = XATTRTERM;
		    sv_setpv(PL_subname,"?");
		    have_name = FALSE;
		}

		if (key == KEY_format) {
		    if (*s == '=')
			PL_lex_formbrack = PL_lex_brackets + 1;
		    if (have_name)
			(void) force_word(PL_oldbufptr + tboffset, WORD,
					  FALSE, TRUE, TRUE);
		    OPERATOR(FORMAT);
		}

		/* Look for a prototype */
		if (*s == '(') {
		    char *p;

		    s = scan_str(s,FALSE,FALSE);
		    if (!s)
			Perl_croak(aTHX_ "Prototype not terminated");
		    /* strip spaces */
		    d = SvPVX(PL_lex_stuff);
		    tmp = 0;
		    for (p = d; *p; ++p) {
			if (!isSPACE(*p))
			    d[tmp++] = *p;
		    }
		    d[tmp] = '\0';
		    SvCUR(PL_lex_stuff) = tmp;
		    have_proto = TRUE;

		    s = skipspace(s);
		}
		else
		    have_proto = FALSE;

		if (*s == ':' && s[1] != ':')
		    PL_expect = attrful;

		if (have_proto) {
		    PL_nextval[PL_nexttoke].opval =
			(OP*)newSVOP(OP_CONST, 0, PL_lex_stuff);
		    PL_lex_stuff = Nullsv;
		    force_next(THING);
		}
		if (!have_name) {
		    sv_setpv(PL_subname,"__ANON__");
		    TOKEN(ANONSUB);
		}
		(void) force_word(PL_oldbufptr + tboffset, WORD,
				  FALSE, TRUE, TRUE);
		if (key == KEY_my)
		    TOKEN(MYSUB);
		TOKEN(SUB);
	    }

	case KEY_system:
	    set_csh();
	    LOP(OP_SYSTEM,XREF);

	case KEY_symlink:
	    LOP(OP_SYMLINK,XTERM);

	case KEY_syscall:
	    LOP(OP_SYSCALL,XTERM);

	case KEY_sysopen:
	    LOP(OP_SYSOPEN,XTERM);

	case KEY_sysseek:
	    LOP(OP_SYSSEEK,XTERM);

	case KEY_sysread:
	    LOP(OP_SYSREAD,XTERM);

	case KEY_syswrite:
	    LOP(OP_SYSWRITE,XTERM);

	case KEY_tr:
	    s = scan_trans(s);
	    TERM(sublex_start());

	case KEY_tell:
	    UNI(OP_TELL);

	case KEY_telldir:
	    UNI(OP_TELLDIR);

	case KEY_tie:
	    LOP(OP_TIE,XTERM);

	case KEY_tied:
	    UNI(OP_TIED);

	case KEY_time:
	    FUN0(OP_TIME);

	case KEY_times:
	    FUN0(OP_TMS);

	case KEY_truncate:
	    LOP(OP_TRUNCATE,XTERM);

	case KEY_uc:
	    UNI(OP_UC);

	case KEY_ucfirst:
	    UNI(OP_UCFIRST);

	case KEY_untie:
	    UNI(OP_UNTIE);

	case KEY_until:
	    yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(UNTIL);

	case KEY_unless:
	    yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(UNLESS);

	case KEY_unlink:
	    LOP(OP_UNLINK,XTERM);

	case KEY_undef:
	    UNI(OP_UNDEF);

	case KEY_unpack:
	    LOP(OP_UNPACK,XTERM);

	case KEY_utime:
	    LOP(OP_UTIME,XTERM);

	case KEY_umask:
	    if (ckWARN(WARN_UMASK)) {
		for (d = s; d < PL_bufend && (isSPACE(*d) || *d == '('); d++) ;
		if (*d != '0' && isDIGIT(*d))
		    Perl_warner(aTHX_ WARN_UMASK,
		    		"umask: argument is missing initial 0");
	    }
	    UNI(OP_UMASK);

	case KEY_unshift:
	    LOP(OP_UNSHIFT,XTERM);

	case KEY_use:
	    if (PL_expect != XSTATE)
		yyerror("\"use\" not allowed in expression");
	    s = skipspace(s);
	    if (isDIGIT(*s) || (*s == 'v' && isDIGIT(s[1]))) {
		s = force_version(s);
		if (*s == ';' || (s = skipspace(s), *s == ';')) {
		    PL_nextval[PL_nexttoke].opval = Nullop;
		    force_next(WORD);
		}
	    }
	    else {
		s = force_word(s,WORD,FALSE,TRUE,FALSE);
		s = force_version(s);
	    }
	    yylval.ival = 1;
	    OPERATOR(USE);

	case KEY_values:
	    UNI(OP_VALUES);

	case KEY_vec:
	    LOP(OP_VEC,XTERM);

	case KEY_while:
	    yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(WHILE);

	case KEY_warn:
	    PL_hints |= HINT_BLOCK_SCOPE;
	    LOP(OP_WARN,XTERM);

	case KEY_wait:
	    FUN0(OP_WAIT);

	case KEY_waitpid:
	    LOP(OP_WAITPID,XTERM);

	case KEY_wantarray:
	    FUN0(OP_WANTARRAY);

	case KEY_write:
#ifdef EBCDIC
	{
	    static char ctl_l[2];

	    if (ctl_l[0] == '\0')
 		ctl_l[0] = toCTRL('L');
	    gv_fetchpv(ctl_l,TRUE, SVt_PV);
	}
#else
	    gv_fetchpv("\f",TRUE, SVt_PV);      /* Make sure $^L is defined */
#endif
	    UNI(OP_ENTERWRITE);

	case KEY_x:
	    if (PL_expect == XOPERATOR)
		Mop(OP_REPEAT);
	    check_uni();
	    goto just_a_word;

	case KEY_xor:
	    yylval.ival = OP_XOR;
	    OPERATOR(OROP);

	case KEY_y:
	    s = scan_trans(s);
	    TERM(sublex_start());
	}
    }}
}
#ifdef __SC__
#pragma segment Main
#endif

I32
Perl_keyword(pTHX_ register char *d, I32 len)
{
    switch (*d) {
    case '_':
	if (d[1] == '_') {
	    if (strEQ(d,"__FILE__"))		return -KEY___FILE__;
	    if (strEQ(d,"__LINE__"))		return -KEY___LINE__;
	    if (strEQ(d,"__PACKAGE__"))		return -KEY___PACKAGE__;
	    if (strEQ(d,"__DATA__"))		return KEY___DATA__;
	    if (strEQ(d,"__END__"))		return KEY___END__;
	}
	break;
    case 'A':
	if (strEQ(d,"AUTOLOAD"))		return KEY_AUTOLOAD;
	break;
    case 'a':
	switch (len) {
	case 3:
	    if (strEQ(d,"and"))			return -KEY_and;
	    if (strEQ(d,"abs"))			return -KEY_abs;
	    break;
	case 5:
	    if (strEQ(d,"alarm"))		return -KEY_alarm;
	    if (strEQ(d,"atan2"))		return -KEY_atan2;
	    break;
	case 6:
	    if (strEQ(d,"accept"))		return -KEY_accept;
	    break;
	}
	break;
    case 'B':
	if (strEQ(d,"BEGIN"))			return KEY_BEGIN;
	break;
    case 'b':
	if (strEQ(d,"bless"))			return -KEY_bless;
	if (strEQ(d,"bind"))			return -KEY_bind;
	if (strEQ(d,"binmode"))			return -KEY_binmode;
	break;
    case 'C':
	if (strEQ(d,"CORE"))			return -KEY_CORE;
	if (strEQ(d,"CHECK"))			return KEY_CHECK;
	break;
    case 'c':
	switch (len) {
	case 3:
	    if (strEQ(d,"cmp"))			return -KEY_cmp;
	    if (strEQ(d,"chr"))			return -KEY_chr;
	    if (strEQ(d,"cos"))			return -KEY_cos;
	    break;
	case 4:
	    if (strEQ(d,"chop"))		return -KEY_chop;
	    break;
	case 5:
	    if (strEQ(d,"close"))		return -KEY_close;
	    if (strEQ(d,"chdir"))		return -KEY_chdir;
	    if (strEQ(d,"chomp"))		return -KEY_chomp;
	    if (strEQ(d,"chmod"))		return -KEY_chmod;
	    if (strEQ(d,"chown"))		return -KEY_chown;
	    if (strEQ(d,"crypt"))		return -KEY_crypt;
	    break;
	case 6:
	    if (strEQ(d,"chroot"))		return -KEY_chroot;
	    if (strEQ(d,"caller"))		return -KEY_caller;
	    break;
	case 7:
	    if (strEQ(d,"connect"))		return -KEY_connect;
	    break;
	case 8:
	    if (strEQ(d,"closedir"))		return -KEY_closedir;
	    if (strEQ(d,"continue"))		return -KEY_continue;
	    break;
	}
	break;
    case 'D':
	if (strEQ(d,"DESTROY"))			return KEY_DESTROY;
	break;
    case 'd':
	switch (len) {
	case 2:
	    if (strEQ(d,"do"))			return KEY_do;
	    break;
	case 3:
	    if (strEQ(d,"die"))			return -KEY_die;
	    break;
	case 4:
	    if (strEQ(d,"dump"))		return -KEY_dump;
	    break;
	case 6:
	    if (strEQ(d,"delete"))		return KEY_delete;
	    break;
	case 7:
	    if (strEQ(d,"defined"))		return KEY_defined;
	    if (strEQ(d,"dbmopen"))		return -KEY_dbmopen;
	    break;
	case 8:
	    if (strEQ(d,"dbmclose"))		return -KEY_dbmclose;
	    break;
	}
	break;
    case 'E':
	if (strEQ(d,"EQ")) { deprecate(d);	return -KEY_eq;}
	if (strEQ(d,"END"))			return KEY_END;
	break;
    case 'e':
	switch (len) {
	case 2:
	    if (strEQ(d,"eq"))			return -KEY_eq;
	    break;
	case 3:
	    if (strEQ(d,"eof"))			return -KEY_eof;
	    if (strEQ(d,"exp"))			return -KEY_exp;
	    break;
	case 4:
	    if (strEQ(d,"else"))		return KEY_else;
	    if (strEQ(d,"exit"))		return -KEY_exit;
	    if (strEQ(d,"eval"))		return KEY_eval;
	    if (strEQ(d,"exec"))		return -KEY_exec;
           if (strEQ(d,"each"))                return -KEY_each;
	    break;
	case 5:
	    if (strEQ(d,"elsif"))		return KEY_elsif;
	    break;
	case 6:
	    if (strEQ(d,"exists"))		return KEY_exists;
	    if (strEQ(d,"elseif")) Perl_warn(aTHX_ "elseif should be elsif");
	    break;
	case 8:
	    if (strEQ(d,"endgrent"))		return -KEY_endgrent;
	    if (strEQ(d,"endpwent"))		return -KEY_endpwent;
	    break;
	case 9:
	    if (strEQ(d,"endnetent"))		return -KEY_endnetent;
	    break;
	case 10:
	    if (strEQ(d,"endhostent"))		return -KEY_endhostent;
	    if (strEQ(d,"endservent"))		return -KEY_endservent;
	    break;
	case 11:
	    if (strEQ(d,"endprotoent"))		return -KEY_endprotoent;
	    break;
	}
	break;
    case 'f':
	switch (len) {
	case 3:
	    if (strEQ(d,"for"))			return KEY_for;
	    break;
	case 4:
	    if (strEQ(d,"fork"))		return -KEY_fork;
	    break;
	case 5:
	    if (strEQ(d,"fcntl"))		return -KEY_fcntl;
	    if (strEQ(d,"flock"))		return -KEY_flock;
	    break;
	case 6:
	    if (strEQ(d,"format"))		return KEY_format;
	    if (strEQ(d,"fileno"))		return -KEY_fileno;
	    break;
	case 7:
	    if (strEQ(d,"foreach"))		return KEY_foreach;
	    break;
	case 8:
	    if (strEQ(d,"formline"))		return -KEY_formline;
	    break;
	}
	break;
    case 'G':
	if (len == 2) {
	    if (strEQ(d,"GT")) { deprecate(d);	return -KEY_gt;}
	    if (strEQ(d,"GE")) { deprecate(d);	return -KEY_ge;}
	}
	break;
    case 'g':
	if (strnEQ(d,"get",3)) {
	    d += 3;
	    if (*d == 'p') {
		switch (len) {
		case 7:
		    if (strEQ(d,"ppid"))	return -KEY_getppid;
		    if (strEQ(d,"pgrp"))	return -KEY_getpgrp;
		    break;
		case 8:
		    if (strEQ(d,"pwent"))	return -KEY_getpwent;
		    if (strEQ(d,"pwnam"))	return -KEY_getpwnam;
		    if (strEQ(d,"pwuid"))	return -KEY_getpwuid;
		    break;
		case 11:
		    if (strEQ(d,"peername"))	return -KEY_getpeername;
		    if (strEQ(d,"protoent"))	return -KEY_getprotoent;
		    if (strEQ(d,"priority"))	return -KEY_getpriority;
		    break;
		case 14:
		    if (strEQ(d,"protobyname"))	return -KEY_getprotobyname;
		    break;
		case 16:
		    if (strEQ(d,"protobynumber"))return -KEY_getprotobynumber;
		    break;
		}
	    }
	    else if (*d == 'h') {
		if (strEQ(d,"hostbyname"))	return -KEY_gethostbyname;
		if (strEQ(d,"hostbyaddr"))	return -KEY_gethostbyaddr;
		if (strEQ(d,"hostent"))		return -KEY_gethostent;
	    }
	    else if (*d == 'n') {
		if (strEQ(d,"netbyname"))	return -KEY_getnetbyname;
		if (strEQ(d,"netbyaddr"))	return -KEY_getnetbyaddr;
		if (strEQ(d,"netent"))		return -KEY_getnetent;
	    }
	    else if (*d == 's') {
		if (strEQ(d,"servbyname"))	return -KEY_getservbyname;
		if (strEQ(d,"servbyport"))	return -KEY_getservbyport;
		if (strEQ(d,"servent"))		return -KEY_getservent;
		if (strEQ(d,"sockname"))	return -KEY_getsockname;
		if (strEQ(d,"sockopt"))		return -KEY_getsockopt;
	    }
	    else if (*d == 'g') {
		if (strEQ(d,"grent"))		return -KEY_getgrent;
		if (strEQ(d,"grnam"))		return -KEY_getgrnam;
		if (strEQ(d,"grgid"))		return -KEY_getgrgid;
	    }
	    else if (*d == 'l') {
		if (strEQ(d,"login"))		return -KEY_getlogin;
	    }
	    else if (strEQ(d,"c"))		return -KEY_getc;
	    break;
	}
	switch (len) {
	case 2:
	    if (strEQ(d,"gt"))			return -KEY_gt;
	    if (strEQ(d,"ge"))			return -KEY_ge;
	    break;
	case 4:
	    if (strEQ(d,"grep"))		return KEY_grep;
	    if (strEQ(d,"goto"))		return KEY_goto;
	    if (strEQ(d,"glob"))		return KEY_glob;
	    break;
	case 6:
	    if (strEQ(d,"gmtime"))		return -KEY_gmtime;
	    break;
	}
	break;
    case 'h':
	if (strEQ(d,"hex"))			return -KEY_hex;
	break;
    case 'I':
	if (strEQ(d,"INIT"))			return KEY_INIT;
	break;
    case 'i':
	switch (len) {
	case 2:
	    if (strEQ(d,"if"))			return KEY_if;
	    break;
	case 3:
	    if (strEQ(d,"int"))			return -KEY_int;
	    break;
	case 5:
	    if (strEQ(d,"index"))		return -KEY_index;
	    if (strEQ(d,"ioctl"))		return -KEY_ioctl;
	    break;
	}
	break;
    case 'j':
	if (strEQ(d,"join"))			return -KEY_join;
	break;
    case 'k':
	if (len == 4) {
           if (strEQ(d,"keys"))                return -KEY_keys;
	    if (strEQ(d,"kill"))		return -KEY_kill;
	}
	break;
    case 'L':
	if (len == 2) {
	    if (strEQ(d,"LT")) { deprecate(d);	return -KEY_lt;}
	    if (strEQ(d,"LE")) { deprecate(d);	return -KEY_le;}
	}
	break;
    case 'l':
	switch (len) {
	case 2:
	    if (strEQ(d,"lt"))			return -KEY_lt;
	    if (strEQ(d,"le"))			return -KEY_le;
	    if (strEQ(d,"lc"))			return -KEY_lc;
	    break;
	case 3:
	    if (strEQ(d,"log"))			return -KEY_log;
	    break;
	case 4:
	    if (strEQ(d,"last"))		return KEY_last;
	    if (strEQ(d,"link"))		return -KEY_link;
	    if (strEQ(d,"lock"))		return -KEY_lock;
	    break;
	case 5:
	    if (strEQ(d,"local"))		return KEY_local;
	    if (strEQ(d,"lstat"))		return -KEY_lstat;
	    break;
	case 6:
	    if (strEQ(d,"length"))		return -KEY_length;
	    if (strEQ(d,"listen"))		return -KEY_listen;
	    break;
	case 7:
	    if (strEQ(d,"lcfirst"))		return -KEY_lcfirst;
	    break;
	case 9:
	    if (strEQ(d,"localtime"))		return -KEY_localtime;
	    break;
	}
	break;
    case 'm':
	switch (len) {
	case 1:					return KEY_m;
	case 2:
	    if (strEQ(d,"my"))			return KEY_my;
	    break;
	case 3:
	    if (strEQ(d,"map"))			return KEY_map;
	    break;
	case 5:
	    if (strEQ(d,"mkdir"))		return -KEY_mkdir;
	    break;
	case 6:
	    if (strEQ(d,"msgctl"))		return -KEY_msgctl;
	    if (strEQ(d,"msgget"))		return -KEY_msgget;
	    if (strEQ(d,"msgrcv"))		return -KEY_msgrcv;
	    if (strEQ(d,"msgsnd"))		return -KEY_msgsnd;
	    break;
	}
	break;
    case 'N':
	if (strEQ(d,"NE")) { deprecate(d);	return -KEY_ne;}
	break;
    case 'n':
	if (strEQ(d,"next"))			return KEY_next;
	if (strEQ(d,"ne"))			return -KEY_ne;
	if (strEQ(d,"not"))			return -KEY_not;
	if (strEQ(d,"no"))			return KEY_no;
	break;
    case 'o':
	switch (len) {
	case 2:
	    if (strEQ(d,"or"))			return -KEY_or;
	    break;
	case 3:
	    if (strEQ(d,"ord"))			return -KEY_ord;
	    if (strEQ(d,"oct"))			return -KEY_oct;
	    if (strEQ(d,"our"))			return KEY_our;
	    break;
	case 4:
	    if (strEQ(d,"open"))		return -KEY_open;
	    break;
	case 7:
	    if (strEQ(d,"opendir"))		return -KEY_opendir;
	    break;
	}
	break;
    case 'p':
	switch (len) {
	case 3:
           if (strEQ(d,"pop"))                 return -KEY_pop;
	    if (strEQ(d,"pos"))			return KEY_pos;
	    break;
	case 4:
           if (strEQ(d,"push"))                return -KEY_push;
	    if (strEQ(d,"pack"))		return -KEY_pack;
	    if (strEQ(d,"pipe"))		return -KEY_pipe;
	    break;
	case 5:
	    if (strEQ(d,"print"))		return KEY_print;
	    break;
	case 6:
	    if (strEQ(d,"printf"))		return KEY_printf;
	    break;
	case 7:
	    if (strEQ(d,"package"))		return KEY_package;
	    break;
	case 9:
	    if (strEQ(d,"prototype"))		return KEY_prototype;
	}
	break;
    case 'q':
	if (len <= 2) {
	    if (strEQ(d,"q"))			return KEY_q;
	    if (strEQ(d,"qr"))			return KEY_qr;
	    if (strEQ(d,"qq"))			return KEY_qq;
	    if (strEQ(d,"qw"))			return KEY_qw;
	    if (strEQ(d,"qx"))			return KEY_qx;
	}
	else if (strEQ(d,"quotemeta"))		return -KEY_quotemeta;
	break;
    case 'r':
	switch (len) {
	case 3:
	    if (strEQ(d,"ref"))			return -KEY_ref;
	    break;
	case 4:
	    if (strEQ(d,"read"))		return -KEY_read;
	    if (strEQ(d,"rand"))		return -KEY_rand;
	    if (strEQ(d,"recv"))		return -KEY_recv;
	    if (strEQ(d,"redo"))		return KEY_redo;
	    break;
	case 5:
	    if (strEQ(d,"rmdir"))		return -KEY_rmdir;
	    if (strEQ(d,"reset"))		return -KEY_reset;
	    break;
	case 6:
	    if (strEQ(d,"return"))		return KEY_return;
	    if (strEQ(d,"rename"))		return -KEY_rename;
	    if (strEQ(d,"rindex"))		return -KEY_rindex;
	    break;
	case 7:
	    if (strEQ(d,"require"))		return -KEY_require;
	    if (strEQ(d,"reverse"))		return -KEY_reverse;
	    if (strEQ(d,"readdir"))		return -KEY_readdir;
	    break;
	case 8:
	    if (strEQ(d,"readlink"))		return -KEY_readlink;
	    if (strEQ(d,"readline"))		return -KEY_readline;
	    if (strEQ(d,"readpipe"))		return -KEY_readpipe;
	    break;
	case 9:
	    if (strEQ(d,"rewinddir"))		return -KEY_rewinddir;
	    break;
	}
	break;
    case 's':
	switch (d[1]) {
	case 0:					return KEY_s;
	case 'c':
	    if (strEQ(d,"scalar"))		return KEY_scalar;
	    break;
	case 'e':
	    switch (len) {
	    case 4:
		if (strEQ(d,"seek"))		return -KEY_seek;
		if (strEQ(d,"send"))		return -KEY_send;
		break;
	    case 5:
		if (strEQ(d,"semop"))		return -KEY_semop;
		break;
	    case 6:
		if (strEQ(d,"select"))		return -KEY_select;
		if (strEQ(d,"semctl"))		return -KEY_semctl;
		if (strEQ(d,"semget"))		return -KEY_semget;
		break;
	    case 7:
		if (strEQ(d,"setpgrp"))		return -KEY_setpgrp;
		if (strEQ(d,"seekdir"))		return -KEY_seekdir;
		break;
	    case 8:
		if (strEQ(d,"setpwent"))	return -KEY_setpwent;
		if (strEQ(d,"setgrent"))	return -KEY_setgrent;
		break;
	    case 9:
		if (strEQ(d,"setnetent"))	return -KEY_setnetent;
		break;
	    case 10:
		if (strEQ(d,"setsockopt"))	return -KEY_setsockopt;
		if (strEQ(d,"sethostent"))	return -KEY_sethostent;
		if (strEQ(d,"setservent"))	return -KEY_setservent;
		break;
	    case 11:
		if (strEQ(d,"setpriority"))	return -KEY_setpriority;
		if (strEQ(d,"setprotoent"))	return -KEY_setprotoent;
		break;
	    }
	    break;
	case 'h':
	    switch (len) {
	    case 5:
               if (strEQ(d,"shift"))           return -KEY_shift;
		break;
	    case 6:
		if (strEQ(d,"shmctl"))		return -KEY_shmctl;
		if (strEQ(d,"shmget"))		return -KEY_shmget;
		break;
	    case 7:
		if (strEQ(d,"shmread"))		return -KEY_shmread;
		break;
	    case 8:
		if (strEQ(d,"shmwrite"))	return -KEY_shmwrite;
		if (strEQ(d,"shutdown"))	return -KEY_shutdown;
		break;
	    }
	    break;
	case 'i':
	    if (strEQ(d,"sin"))			return -KEY_sin;
	    break;
	case 'l':
	    if (strEQ(d,"sleep"))		return -KEY_sleep;
	    break;
	case 'o':
	    if (strEQ(d,"sort"))		return KEY_sort;
	    if (strEQ(d,"socket"))		return -KEY_socket;
	    if (strEQ(d,"socketpair"))		return -KEY_socketpair;
	    break;
	case 'p':
	    if (strEQ(d,"split"))		return KEY_split;
	    if (strEQ(d,"sprintf"))		return -KEY_sprintf;
           if (strEQ(d,"splice"))              return -KEY_splice;
	    break;
	case 'q':
	    if (strEQ(d,"sqrt"))		return -KEY_sqrt;
	    break;
	case 'r':
	    if (strEQ(d,"srand"))		return -KEY_srand;
	    break;
	case 't':
	    if (strEQ(d,"stat"))		return -KEY_stat;
	    if (strEQ(d,"study"))		return KEY_study;
	    break;
	case 'u':
	    if (strEQ(d,"substr"))		return -KEY_substr;
	    if (strEQ(d,"sub"))			return KEY_sub;
	    break;
	case 'y':
	    switch (len) {
	    case 6:
		if (strEQ(d,"system"))		return -KEY_system;
		break;
	    case 7:
		if (strEQ(d,"symlink"))		return -KEY_symlink;
		if (strEQ(d,"syscall"))		return -KEY_syscall;
		if (strEQ(d,"sysopen"))		return -KEY_sysopen;
		if (strEQ(d,"sysread"))		return -KEY_sysread;
		if (strEQ(d,"sysseek"))		return -KEY_sysseek;
		break;
	    case 8:
		if (strEQ(d,"syswrite"))	return -KEY_syswrite;
		break;
	    }
	    break;
	}
	break;
    case 't':
	switch (len) {
	case 2:
	    if (strEQ(d,"tr"))			return KEY_tr;
	    break;
	case 3:
	    if (strEQ(d,"tie"))			return KEY_tie;
	    break;
	case 4:
	    if (strEQ(d,"tell"))		return -KEY_tell;
	    if (strEQ(d,"tied"))		return KEY_tied;
	    if (strEQ(d,"time"))		return -KEY_time;
	    break;
	case 5:
	    if (strEQ(d,"times"))		return -KEY_times;
	    break;
	case 7:
	    if (strEQ(d,"telldir"))		return -KEY_telldir;
	    break;
	case 8:
	    if (strEQ(d,"truncate"))		return -KEY_truncate;
	    break;
	}
	break;
    case 'u':
	switch (len) {
	case 2:
	    if (strEQ(d,"uc"))			return -KEY_uc;
	    break;
	case 3:
	    if (strEQ(d,"use"))			return KEY_use;
	    break;
	case 5:
	    if (strEQ(d,"undef"))		return KEY_undef;
	    if (strEQ(d,"until"))		return KEY_until;
	    if (strEQ(d,"untie"))		return KEY_untie;
	    if (strEQ(d,"utime"))		return -KEY_utime;
	    if (strEQ(d,"umask"))		return -KEY_umask;
	    break;
	case 6:
	    if (strEQ(d,"unless"))		return KEY_unless;
	    if (strEQ(d,"unpack"))		return -KEY_unpack;
	    if (strEQ(d,"unlink"))		return -KEY_unlink;
	    break;
	case 7:
           if (strEQ(d,"unshift"))             return -KEY_unshift;
	    if (strEQ(d,"ucfirst"))		return -KEY_ucfirst;
	    break;
	}
	break;
    case 'v':
	if (strEQ(d,"values"))			return -KEY_values;
	if (strEQ(d,"vec"))			return -KEY_vec;
	break;
    case 'w':
	switch (len) {
	case 4:
	    if (strEQ(d,"warn"))		return -KEY_warn;
	    if (strEQ(d,"wait"))		return -KEY_wait;
	    break;
	case 5:
	    if (strEQ(d,"while"))		return KEY_while;
	    if (strEQ(d,"write"))		return -KEY_write;
	    break;
	case 7:
	    if (strEQ(d,"waitpid"))		return -KEY_waitpid;
	    break;
	case 9:
	    if (strEQ(d,"wantarray"))		return -KEY_wantarray;
	    break;
	}
	break;
    case 'x':
	if (len == 1)				return -KEY_x;
	if (strEQ(d,"xor"))			return -KEY_xor;
	break;
    case 'y':
	if (len == 1)				return KEY_y;
	break;
    case 'z':
	break;
    }
    return 0;
}

STATIC void
S_checkcomma(pTHX_ register char *s, char *name, char *what)
{
    char *w;

    if (*s == ' ' && s[1] == '(') {	/* XXX gotta be a better way */
	if (ckWARN(WARN_SYNTAX)) {
	    int level = 1;
	    for (w = s+2; *w && level; w++) {
		if (*w == '(')
		    ++level;
		else if (*w == ')')
		    --level;
	    }
	    if (*w)
		for (; *w && isSPACE(*w); w++) ;
	    if (!*w || !strchr(";|})]oaiuw!=", *w))	/* an advisory hack only... */
		Perl_warner(aTHX_ WARN_SYNTAX,
			    "%s (...) interpreted as function",name);
	}
    }
    while (s < PL_bufend && isSPACE(*s))
	s++;
    if (*s == '(')
	s++;
    while (s < PL_bufend && isSPACE(*s))
	s++;
    if (isIDFIRST_lazy_if(s,UTF)) {
	w = s++;
	while (isALNUM_lazy_if(s,UTF))
	    s++;
	while (s < PL_bufend && isSPACE(*s))
	    s++;
	if (*s == ',') {
	    int kw;
	    *s = '\0';
	    kw = keyword(w, s - w) || get_cv(w, FALSE) != 0;
	    *s = ',';
	    if (kw)
		return;
	    Perl_croak(aTHX_ "No comma allowed after %s", what);
	}
    }
}

/* Either returns sv, or mortalizes sv and returns a new SV*.
   Best used as sv=new_constant(..., sv, ...).
   If s, pv are NULL, calls subroutine with one argument,
   and type is used with error messages only. */

STATIC SV *
S_new_constant(pTHX_ char *s, STRLEN len, const char *key, SV *sv, SV *pv,
	       const char *type)
{
    dSP;
    HV *table = GvHV(PL_hintgv);		 /* ^H */
    SV *res;
    SV **cvp;
    SV *cv, *typesv;
    const char *why1, *why2, *why3;

    if (!table || !(PL_hints & HINT_LOCALIZE_HH)) {
	SV *msg;
	
	why2 = strEQ(key,"charnames")
	       ? "(possibly a missing \"use charnames ...\")"
	       : "";
	msg = Perl_newSVpvf(aTHX_ "Constant(%s) unknown: %s",
			    (type ? type: "undef"), why2);

	/* This is convoluted and evil ("goto considered harmful")
	 * but I do not understand the intricacies of all the different
	 * failure modes of %^H in here.  The goal here is to make
	 * the most probable error message user-friendly. --jhi */

	goto msgdone;

    report:
	msg = Perl_newSVpvf(aTHX_ "Constant(%s): %s%s%s",
			    (type ? type: "undef"), why1, why2, why3);
    msgdone:
	yyerror(SvPVX(msg));
 	SvREFCNT_dec(msg);
  	return sv;
    }
    cvp = hv_fetch(table, key, strlen(key), FALSE);
    if (!cvp || !SvOK(*cvp)) {
	why1 = "$^H{";
	why2 = key;
	why3 = "} is not defined";
	goto report;
    }
    sv_2mortal(sv);			/* Parent created it permanently */
    cv = *cvp;
    if (!pv && s)
  	pv = sv_2mortal(newSVpvn(s, len));
    if (type && pv)
  	typesv = sv_2mortal(newSVpv(type, 0));
    else
  	typesv = &PL_sv_undef;

    PUSHSTACKi(PERLSI_OVERLOAD);
    ENTER ;
    SAVETMPS;

    PUSHMARK(SP) ;
    EXTEND(sp, 3);
    if (pv)
 	PUSHs(pv);
    PUSHs(sv);
    if (pv)
 	PUSHs(typesv);
    PUTBACK;
    call_sv(cv, G_SCALAR | ( PL_in_eval ? 0 : G_EVAL));

    SPAGAIN ;

    /* Check the eval first */
    if (!PL_in_eval && SvTRUE(ERRSV)) {
	STRLEN n_a;
 	sv_catpv(ERRSV, "Propagated");
	yyerror(SvPV(ERRSV, n_a)); /* Duplicates the message inside eval */
	(void)POPs;
 	res = SvREFCNT_inc(sv);
    }
    else {
 	res = POPs;
 	(void)SvREFCNT_inc(res);
    }

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    POPSTACK;

    if (!SvOK(res)) {
 	why1 = "Call to &{$^H{";
 	why2 = key;
 	why3 = "}} did not return a defined value";
 	sv = res;
 	goto report;
    }

    return res;
}

STATIC char *
S_scan_word(pTHX_ register char *s, char *dest, STRLEN destlen, int allow_package, STRLEN *slp)
{
    register char *d = dest;
    register char *e = d + destlen - 3;  /* two-character token, ending NUL */
    for (;;) {
	if (d >= e)
	    Perl_croak(aTHX_ ident_too_long);
	if (isALNUM(*s))	/* UTF handled below */
	    *d++ = *s++;
	else if (*s == '\'' && allow_package && isIDFIRST_lazy_if(s+1,UTF)) {
	    *d++ = ':';
	    *d++ = ':';
	    s++;
	}
	else if (*s == ':' && s[1] == ':' && allow_package && s[2] != '$') {
	    *d++ = *s++;
	    *d++ = *s++;
	}
	else if (UTF && UTF8_IS_START(*s) && isALNUM_utf8((U8*)s)) {
	    char *t = s + UTF8SKIP(s);
	    while (UTF8_IS_CONTINUED(*t) && is_utf8_mark((U8*)t))
		t += UTF8SKIP(t);
	    if (d + (t - s) > e)
		Perl_croak(aTHX_ ident_too_long);
	    Copy(s, d, t - s, char);
	    d += t - s;
	    s = t;
	}
	else {
	    *d = '\0';
	    *slp = d - dest;
	    return s;
	}
    }
}

STATIC char *
S_scan_ident(pTHX_ register char *s, register char *send, char *dest, STRLEN destlen, I32 ck_uni)
{
    register char *d;
    register char *e;
    char *bracket = 0;
    char funny = *s++;

    if (isSPACE(*s))
	s = skipspace(s);
    d = dest;
    e = d + destlen - 3;	/* two-character token, ending NUL */
    if (isDIGIT(*s)) {
	while (isDIGIT(*s)) {
	    if (d >= e)
		Perl_croak(aTHX_ ident_too_long);
	    *d++ = *s++;
	}
    }
    else {
	for (;;) {
	    if (d >= e)
		Perl_croak(aTHX_ ident_too_long);
	    if (isALNUM(*s))	/* UTF handled below */
		*d++ = *s++;
	    else if (*s == '\'' && isIDFIRST_lazy_if(s+1,UTF)) {
		*d++ = ':';
		*d++ = ':';
		s++;
	    }
	    else if (*s == ':' && s[1] == ':') {
		*d++ = *s++;
		*d++ = *s++;
	    }
	    else if (UTF && UTF8_IS_START(*s) && isALNUM_utf8((U8*)s)) {
		char *t = s + UTF8SKIP(s);
		while (UTF8_IS_CONTINUED(*t) && is_utf8_mark((U8*)t))
		    t += UTF8SKIP(t);
		if (d + (t - s) > e)
		    Perl_croak(aTHX_ ident_too_long);
		Copy(s, d, t - s, char);
		d += t - s;
		s = t;
	    }
	    else
		break;
	}
    }
    *d = '\0';
    d = dest;
    if (*d) {
	if (PL_lex_state != LEX_NORMAL)
	    PL_lex_state = LEX_INTERPENDMAYBE;
	return s;
    }
    if (*s == '$' && s[1] &&
	(isALNUM_lazy_if(s+1,UTF) || strchr("${", s[1]) || strnEQ(s+1,"::",2)) )
    {
	return s;
    }
    if (*s == '{') {
	bracket = s;
	s++;
    }
    else if (ck_uni)
	check_uni();
    if (s < send)
	*d = *s++;
    d[1] = '\0';
    if (*d == '^' && *s && isCONTROLVAR(*s)) {
	*d = toCTRL(*s);
	s++;
    }
    if (bracket) {
	if (isSPACE(s[-1])) {
	    while (s < send) {
		char ch = *s++;
		if (!SPACE_OR_TAB(ch)) {
		    *d = ch;
		    break;
		}
	    }
	}
	if (isIDFIRST_lazy_if(d,UTF)) {
	    d++;
	    if (UTF) {
		e = s;
		while ((e < send && isALNUM_lazy_if(e,UTF)) || *e == ':') {
		    e += UTF8SKIP(e);
		    while (e < send && UTF8_IS_CONTINUED(*e) && is_utf8_mark((U8*)e))
			e += UTF8SKIP(e);
		}
		Copy(s, d, e - s, char);
		d += e - s;
		s = e;
	    }
	    else {
		while ((isALNUM(*s) || *s == ':') && d < e)
		    *d++ = *s++;
		if (d >= e)
		    Perl_croak(aTHX_ ident_too_long);
	    }
	    *d = '\0';
	    while (s < send && SPACE_OR_TAB(*s)) s++;
	    if ((*s == '[' || (*s == '{' && strNE(dest, "sub")))) {
		if (ckWARN(WARN_AMBIGUOUS) && keyword(dest, d - dest)) {
		    const char *brack = *s == '[' ? "[...]" : "{...}";
		    Perl_warner(aTHX_ WARN_AMBIGUOUS,
			"Ambiguous use of %c{%s%s} resolved to %c%s%s",
			funny, dest, brack, funny, dest, brack);
		}
		bracket++;
		PL_lex_brackstack[PL_lex_brackets++] = (char)(XOPERATOR | XFAKEBRACK);
		return s;
	    }
	}
	/* Handle extended ${^Foo} variables
	 * 1999-02-27 mjd-perl-patch@plover.com */
	else if (!isALNUM(*d) && !isPRINT(*d) /* isCTRL(d) */
		 && isALNUM(*s))
	{
	    d++;
	    while (isALNUM(*s) && d < e) {
		*d++ = *s++;
	    }
	    if (d >= e)
		Perl_croak(aTHX_ ident_too_long);
	    *d = '\0';
	}
	if (*s == '}') {
	    s++;
	    if (PL_lex_state == LEX_INTERPNORMAL && !PL_lex_brackets)
		PL_lex_state = LEX_INTERPEND;
	    if (funny == '#')
		funny = '@';
	    if (PL_lex_state == LEX_NORMAL) {
		if (ckWARN(WARN_AMBIGUOUS) &&
		    (keyword(dest, d - dest) || get_cv(dest, FALSE)))
		{
		    Perl_warner(aTHX_ WARN_AMBIGUOUS,
			"Ambiguous use of %c{%s} resolved to %c%s",
			funny, dest, funny, dest);
		}
	    }
	}
	else {
	    s = bracket;		/* let the parser handle it */
	    *dest = '\0';
	}
    }
    else if (PL_lex_state == LEX_INTERPNORMAL && !PL_lex_brackets && !intuit_more(s))
	PL_lex_state = LEX_INTERPEND;
    return s;
}

void
Perl_pmflag(pTHX_ U16 *pmfl, int ch)
{
    if (ch == 'i')
	*pmfl |= PMf_FOLD;
    else if (ch == 'g')
	*pmfl |= PMf_GLOBAL;
    else if (ch == 'c')
	*pmfl |= PMf_CONTINUE;
    else if (ch == 'o')
	*pmfl |= PMf_KEEP;
    else if (ch == 'm')
	*pmfl |= PMf_MULTILINE;
    else if (ch == 's')
	*pmfl |= PMf_SINGLELINE;
    else if (ch == 'x')
	*pmfl |= PMf_EXTENDED;
}

STATIC char *
S_scan_pat(pTHX_ char *start, I32 type)
{
    PMOP *pm;
    char *s;

    s = scan_str(start,FALSE,FALSE);
    if (!s)
	Perl_croak(aTHX_ "Search pattern not terminated");

    pm = (PMOP*)newPMOP(type, 0);
    if (PL_multi_open == '?')
	pm->op_pmflags |= PMf_ONCE;
    if(type == OP_QR) {
	while (*s && strchr("iomsx", *s))
	    pmflag(&pm->op_pmflags,*s++);
    }
    else {
	while (*s && strchr("iogcmsx", *s))
	    pmflag(&pm->op_pmflags,*s++);
    }
    pm->op_pmpermflags = pm->op_pmflags;

    PL_lex_op = (OP*)pm;
    yylval.ival = OP_MATCH;
    return s;
}

STATIC char *
S_scan_subst(pTHX_ char *start)
{
    register char *s;
    register PMOP *pm;
    I32 first_start;
    I32 es = 0;

    yylval.ival = OP_NULL;

    s = scan_str(start,FALSE,FALSE);

    if (!s)
	Perl_croak(aTHX_ "Substitution pattern not terminated");

    if (s[-1] == PL_multi_open)
	s--;

    first_start = PL_multi_start;
    s = scan_str(s,FALSE,FALSE);
    if (!s) {
	if (PL_lex_stuff) {
	    SvREFCNT_dec(PL_lex_stuff);
	    PL_lex_stuff = Nullsv;
	}
	Perl_croak(aTHX_ "Substitution replacement not terminated");
    }
    PL_multi_start = first_start;	/* so whole substitution is taken together */

    pm = (PMOP*)newPMOP(OP_SUBST, 0);
    while (*s) {
	if (*s == 'e') {
	    s++;
	    es++;
	}
	else if (strchr("iogcmsx", *s))
	    pmflag(&pm->op_pmflags,*s++);
	else
	    break;
    }

    if (es) {
	SV *repl;
	PL_sublex_info.super_bufptr = s;
	PL_sublex_info.super_bufend = PL_bufend;
	PL_multi_end = 0;
	pm->op_pmflags |= PMf_EVAL;
	repl = newSVpvn("",0);
	while (es-- > 0)
	    sv_catpv(repl, es ? "eval " : "do ");
	sv_catpvn(repl, "{ ", 2);
	sv_catsv(repl, PL_lex_repl);
	sv_catpvn(repl, " };", 2);
	SvEVALED_on(repl);
	SvREFCNT_dec(PL_lex_repl);
	PL_lex_repl = repl;
    }

    pm->op_pmpermflags = pm->op_pmflags;
    PL_lex_op = (OP*)pm;
    yylval.ival = OP_SUBST;
    return s;
}

STATIC char *
S_scan_trans(pTHX_ char *start)
{
    register char* s;
    OP *o;
    short *tbl;
    I32 squash;
    I32 del;
    I32 complement;
    I32 utf8;
    I32 count = 0;

    yylval.ival = OP_NULL;

    s = scan_str(start,FALSE,FALSE);
    if (!s)
	Perl_croak(aTHX_ "Transliteration pattern not terminated");
    if (s[-1] == PL_multi_open)
	s--;

    s = scan_str(s,FALSE,FALSE);
    if (!s) {
	if (PL_lex_stuff) {
	    SvREFCNT_dec(PL_lex_stuff);
	    PL_lex_stuff = Nullsv;
	}
	Perl_croak(aTHX_ "Transliteration replacement not terminated");
    }

    New(803,tbl,256,short);
    o = newPVOP(OP_TRANS, 0, (char*)tbl);

    complement = del = squash = 0;
    while (strchr("cds", *s)) {
	if (*s == 'c')
	    complement = OPpTRANS_COMPLEMENT;
	else if (*s == 'd')
	    del = OPpTRANS_DELETE;
	else if (*s == 's')
	    squash = OPpTRANS_SQUASH;
	s++;
    }
    o->op_private = del|squash|complement|
      (DO_UTF8(PL_lex_stuff)? OPpTRANS_FROM_UTF : 0)|
      (DO_UTF8(PL_lex_repl) ? OPpTRANS_TO_UTF   : 0);

    PL_lex_op = o;
    yylval.ival = OP_TRANS;
    return s;
}

STATIC char *
S_scan_heredoc(pTHX_ register char *s)
{
    SV *herewas;
    I32 op_type = OP_SCALAR;
    I32 len;
    SV *tmpstr;
    char term;
    register char *d;
    register char *e;
    char *peek;
    int outer = (PL_rsfp && !(PL_lex_inwhat == OP_SCALAR));

    s += 2;
    d = PL_tokenbuf;
    e = PL_tokenbuf + sizeof PL_tokenbuf - 1;
    if (!outer)
	*d++ = '\n';
    for (peek = s; SPACE_OR_TAB(*peek); peek++) ;
    if (*peek && strchr("`'\"",*peek)) {
	s = peek;
	term = *s++;
	s = delimcpy(d, e, s, PL_bufend, term, &len);
	d += len;
	if (s < PL_bufend)
	    s++;
    }
    else {
	if (*s == '\\')
	    s++, term = '\'';
	else
	    term = '"';
	if (!isALNUM_lazy_if(s,UTF))
	    deprecate("bare << to mean <<\"\"");
	for (; isALNUM_lazy_if(s,UTF); s++) {
	    if (d < e)
		*d++ = *s;
	}
    }
    if (d >= PL_tokenbuf + sizeof PL_tokenbuf - 1)
	Perl_croak(aTHX_ "Delimiter for here document is too long");
    *d++ = '\n';
    *d = '\0';
    len = d - PL_tokenbuf;
#ifndef PERL_STRICT_CR
    d = strchr(s, '\r');
    if (d) {
	char *olds = s;
	s = d;
	while (s < PL_bufend) {
	    if (*s == '\r') {
		*d++ = '\n';
		if (*++s == '\n')
		    s++;
	    }
	    else if (*s == '\n' && s[1] == '\r') {	/* \015\013 on a mac? */
		*d++ = *s++;
		s++;
	    }
	    else
		*d++ = *s++;
	}
	*d = '\0';
	PL_bufend = d;
	SvCUR_set(PL_linestr, PL_bufend - SvPVX(PL_linestr));
	s = olds;
    }
#endif
    d = "\n";
    if (outer || !(d=ninstr(s,PL_bufend,d,d+1)))
	herewas = newSVpvn(s,PL_bufend-s);
    else
	s--, herewas = newSVpvn(s,d-s);
    s += SvCUR(herewas);

    tmpstr = NEWSV(87,79);
    sv_upgrade(tmpstr, SVt_PVIV);
    if (term == '\'') {
	op_type = OP_CONST;
	SvIVX(tmpstr) = -1;
    }
    else if (term == '`') {
	op_type = OP_BACKTICK;
	SvIVX(tmpstr) = '\\';
    }

    CLINE;
    PL_multi_start = CopLINE(PL_curcop);
    PL_multi_open = PL_multi_close = '<';
    term = *PL_tokenbuf;
    if (PL_lex_inwhat == OP_SUBST && PL_in_eval && !PL_rsfp) {
	char *bufptr = PL_sublex_info.super_bufptr;
	char *bufend = PL_sublex_info.super_bufend;
	char *olds = s - SvCUR(herewas);
	s = strchr(bufptr, '\n');
	if (!s)
	    s = bufend;
	d = s;
	while (s < bufend &&
	  (*s != term || memNE(s,PL_tokenbuf,len)) ) {
	    if (*s++ == '\n')
		CopLINE_inc(PL_curcop);
	}
	if (s >= bufend) {
	    CopLINE_set(PL_curcop, PL_multi_start);
	    missingterm(PL_tokenbuf);
	}
	sv_setpvn(herewas,bufptr,d-bufptr+1);
	sv_setpvn(tmpstr,d+1,s-d);
	s += len - 1;
	sv_catpvn(herewas,s,bufend-s);
	(void)strcpy(bufptr,SvPVX(herewas));

	s = olds;
	goto retval;
    }
    else if (!outer) {
	d = s;
	while (s < PL_bufend &&
	  (*s != term || memNE(s,PL_tokenbuf,len)) ) {
	    if (*s++ == '\n')
		CopLINE_inc(PL_curcop);
	}
	if (s >= PL_bufend) {
	    CopLINE_set(PL_curcop, PL_multi_start);
	    missingterm(PL_tokenbuf);
	}
	sv_setpvn(tmpstr,d+1,s-d);
	s += len - 1;
	CopLINE_inc(PL_curcop);	/* the preceding stmt passes a newline */

	sv_catpvn(herewas,s,PL_bufend-s);
	sv_setsv(PL_linestr,herewas);
	PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = s = PL_linestart = SvPVX(PL_linestr);
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	PL_last_lop = PL_last_uni = Nullch;
    }
    else
	sv_setpvn(tmpstr,"",0);   /* avoid "uninitialized" warning */
    while (s >= PL_bufend) {	/* multiple line string? */
	if (!outer ||
	 !(PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = filter_gets(PL_linestr, PL_rsfp, 0))) {
	    CopLINE_set(PL_curcop, PL_multi_start);
	    missingterm(PL_tokenbuf);
	}
	CopLINE_inc(PL_curcop);
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	PL_last_lop = PL_last_uni = Nullch;
#ifndef PERL_STRICT_CR
	if (PL_bufend - PL_linestart >= 2) {
	    if ((PL_bufend[-2] == '\r' && PL_bufend[-1] == '\n') ||
		(PL_bufend[-2] == '\n' && PL_bufend[-1] == '\r'))
	    {
		PL_bufend[-2] = '\n';
		PL_bufend--;
		SvCUR_set(PL_linestr, PL_bufend - SvPVX(PL_linestr));
	    }
	    else if (PL_bufend[-1] == '\r')
		PL_bufend[-1] = '\n';
	}
	else if (PL_bufend - PL_linestart == 1 && PL_bufend[-1] == '\r')
	    PL_bufend[-1] = '\n';
#endif
	if (PERLDB_LINE && PL_curstash != PL_debstash) {
	    SV *sv = NEWSV(88,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setsv(sv,PL_linestr);
	    av_store(CopFILEAV(PL_curcop), (I32)CopLINE(PL_curcop),sv);
	}
	if (*s == term && memEQ(s,PL_tokenbuf,len)) {
	    s = PL_bufend - 1;
	    *s = ' ';
	    sv_catsv(PL_linestr,herewas);
	    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	}
	else {
	    s = PL_bufend;
	    sv_catsv(tmpstr,PL_linestr);
	}
    }
    s++;
retval:
    PL_multi_end = CopLINE(PL_curcop);
    if (SvCUR(tmpstr) + 5 < SvLEN(tmpstr)) {
	SvLEN_set(tmpstr, SvCUR(tmpstr) + 1);
	Renew(SvPVX(tmpstr), SvLEN(tmpstr), char);
    }
    SvREFCNT_dec(herewas);
    if (UTF && !IN_BYTE && is_utf8_string((U8*)SvPVX(tmpstr), SvCUR(tmpstr)))
	SvUTF8_on(tmpstr);
    PL_lex_stuff = tmpstr;
    yylval.ival = op_type;
    return s;
}

/* scan_inputsymbol
   takes: current position in input buffer
   returns: new position in input buffer
   side-effects: yylval and lex_op are set.

   This code handles:

   <>		read from ARGV
   <FH> 	read from filehandle
   <pkg::FH>	read from package qualified filehandle
   <pkg'FH>	read from package qualified filehandle
   <$fh>	read from filehandle in $fh
   <*.h>	filename glob

*/

STATIC char *
S_scan_inputsymbol(pTHX_ char *start)
{
    register char *s = start;		/* current position in buffer */
    register char *d;
    register char *e;
    char *end;
    I32 len;

    d = PL_tokenbuf;			/* start of temp holding space */
    e = PL_tokenbuf + sizeof PL_tokenbuf;	/* end of temp holding space */
    end = strchr(s, '\n');
    if (!end)
	end = PL_bufend;
    s = delimcpy(d, e, s + 1, end, '>', &len);	/* extract until > */

    /* die if we didn't have space for the contents of the <>,
       or if it didn't end, or if we see a newline
    */

    if (len >= sizeof PL_tokenbuf)
	Perl_croak(aTHX_ "Excessively long <> operator");
    if (s >= end)
	Perl_croak(aTHX_ "Unterminated <> operator");

    s++;

    /* check for <$fh>
       Remember, only scalar variables are interpreted as filehandles by
       this code.  Anything more complex (e.g., <$fh{$num}>) will be
       treated as a glob() call.
       This code makes use of the fact that except for the $ at the front,
       a scalar variable and a filehandle look the same.
    */
    if (*d == '$' && d[1]) d++;

    /* allow <Pkg'VALUE> or <Pkg::VALUE> */
    while (*d && (isALNUM_lazy_if(d,UTF) || *d == '\'' || *d == ':'))
	d++;

    /* If we've tried to read what we allow filehandles to look like, and
       there's still text left, then it must be a glob() and not a getline.
       Use scan_str to pull out the stuff between the <> and treat it
       as nothing more than a string.
    */

    if (d - PL_tokenbuf != len) {
	yylval.ival = OP_GLOB;
	set_csh();
	s = scan_str(start,FALSE,FALSE);
	if (!s)
	   Perl_croak(aTHX_ "Glob not terminated");
	return s;
    }
    else {
    	/* we're in a filehandle read situation */
	d = PL_tokenbuf;

	/* turn <> into <ARGV> */
	if (!len)
	    (void)strcpy(d,"ARGV");

	/* if <$fh>, create the ops to turn the variable into a
	   filehandle
	*/
	if (*d == '$') {
	    I32 tmp;

	    /* try to find it in the pad for this block, otherwise find
	       add symbol table ops
	    */
	    if ((tmp = pad_findmy(d)) != NOT_IN_PAD) {
		OP *o = newOP(OP_PADSV, 0);
		o->op_targ = tmp;
		PL_lex_op = (OP*)newUNOP(OP_READLINE, 0, o);
	    }
	    else {
		GV *gv = gv_fetchpv(d+1,TRUE, SVt_PV);
		PL_lex_op = (OP*)newUNOP(OP_READLINE, 0,
					    newUNOP(OP_RV2SV, 0,
						newGVOP(OP_GV, 0, gv)));
	    }
	    PL_lex_op->op_flags |= OPf_SPECIAL;
	    /* we created the ops in PL_lex_op, so make yylval.ival a null op */
	    yylval.ival = OP_NULL;
	}

	/* If it's none of the above, it must be a literal filehandle
	   (<Foo::BAR> or <FOO>) so build a simple readline OP */
	else {
	    GV *gv = gv_fetchpv(d,TRUE, SVt_PVIO);
	    PL_lex_op = (OP*)newUNOP(OP_READLINE, 0, newGVOP(OP_GV, 0, gv));
	    yylval.ival = OP_NULL;
	}
    }

    return s;
}


/* scan_str
   takes: start position in buffer
	  keep_quoted preserve \ on the embedded delimiter(s)
	  keep_delims preserve the delimiters around the string
   returns: position to continue reading from buffer
   side-effects: multi_start, multi_close, lex_repl or lex_stuff, and
   	updates the read buffer.

   This subroutine pulls a string out of the input.  It is called for:
   	q		single quotes		q(literal text)
	'		single quotes		'literal text'
	qq		double quotes		qq(interpolate $here please)
	"		double quotes		"interpolate $here please"
	qx		backticks		qx(/bin/ls -l)
	`		backticks		`/bin/ls -l`
	qw		quote words		@EXPORT_OK = qw( func() $spam )
	m//		regexp match		m/this/
	s///		regexp substitute	s/this/that/
	tr///		string transliterate	tr/this/that/
	y///		string transliterate	y/this/that/
	($*@)		sub prototypes		sub foo ($)
	(stuff)		sub attr parameters	sub foo : attr(stuff)
	<>		readline or globs	<FOO>, <>, <$fh>, or <*.c>
	
   In most of these cases (all but <>, patterns and transliterate)
   yylex() calls scan_str().  m// makes yylex() call scan_pat() which
   calls scan_str().  s/// makes yylex() call scan_subst() which calls
   scan_str().  tr/// and y/// make yylex() call scan_trans() which
   calls scan_str().

   It skips whitespace before the string starts, and treats the first
   character as the delimiter.  If the delimiter is one of ([{< then
   the corresponding "close" character )]}> is used as the closing
   delimiter.  It allows quoting of delimiters, and if the string has
   balanced delimiters ([{<>}]) it allows nesting.

   On success, the SV with the resulting string is put into lex_stuff or,
   if that is already non-NULL, into lex_repl. The second case occurs only
   when parsing the RHS of the special constructs s/// and tr/// (y///).
   For convenience, the terminating delimiter character is stuffed into
   SvIVX of the SV.
*/

STATIC char *
S_scan_str(pTHX_ char *start, int keep_quoted, int keep_delims)
{
    SV *sv;				/* scalar value: string */
    char *tmps;				/* temp string, used for delimiter matching */
    register char *s = start;		/* current position in the buffer */
    register char term;			/* terminating character */
    register char *to;			/* current position in the sv's data */
    I32 brackets = 1;			/* bracket nesting level */
    bool has_utf8 = FALSE;		/* is there any utf8 content? */

    /* skip space before the delimiter */
    if (isSPACE(*s))
	s = skipspace(s);

    /* mark where we are, in case we need to report errors */
    CLINE;

    /* after skipping whitespace, the next character is the terminator */
    term = *s;
    if (UTF8_IS_CONTINUED(term) && UTF)
	has_utf8 = TRUE;

    /* mark where we are */
    PL_multi_start = CopLINE(PL_curcop);
    PL_multi_open = term;

    /* find corresponding closing delimiter */
    if (term && (tmps = strchr("([{< )]}> )]}>",term)))
	term = tmps[5];
    PL_multi_close = term;

    /* create a new SV to hold the contents.  87 is leak category, I'm
       assuming.  79 is the SV's initial length.  What a random number. */
    sv = NEWSV(87,79);
    sv_upgrade(sv, SVt_PVIV);
    SvIVX(sv) = term;
    (void)SvPOK_only(sv);		/* validate pointer */

    /* move past delimiter and try to read a complete string */
    if (keep_delims)
	sv_catpvn(sv, s, 1);
    s++;
    for (;;) {
    	/* extend sv if need be */
	SvGROW(sv, SvCUR(sv) + (PL_bufend - s) + 1);
	/* set 'to' to the next character in the sv's string */
	to = SvPVX(sv)+SvCUR(sv);

	/* if open delimiter is the close delimiter read unbridle */
	if (PL_multi_open == PL_multi_close) {
	    for (; s < PL_bufend; s++,to++) {
	    	/* embedded newlines increment the current line number */
		if (*s == '\n' && !PL_rsfp)
		    CopLINE_inc(PL_curcop);
		/* handle quoted delimiters */
		if (*s == '\\' && s+1 < PL_bufend && term != '\\') {
		    if (!keep_quoted && s[1] == term)
			s++;
		/* any other quotes are simply copied straight through */
		    else
			*to++ = *s++;
		}
		/* terminate when run out of buffer (the for() condition), or
		   have found the terminator */
		else if (*s == term)
		    break;
		else if (!has_utf8 && UTF8_IS_CONTINUED(*s) && UTF)
		    has_utf8 = TRUE;
		*to = *s;
	    }
	}
	
	/* if the terminator isn't the same as the start character (e.g.,
	   matched brackets), we have to allow more in the quoting, and
	   be prepared for nested brackets.
	*/
	else {
	    /* read until we run out of string, or we find the terminator */
	    for (; s < PL_bufend; s++,to++) {
	    	/* embedded newlines increment the line count */
		if (*s == '\n' && !PL_rsfp)
		    CopLINE_inc(PL_curcop);
		/* backslashes can escape the open or closing characters */
		if (*s == '\\' && s+1 < PL_bufend) {
		    if (!keep_quoted &&
			((s[1] == PL_multi_open) || (s[1] == PL_multi_close)))
			s++;
		    else
			*to++ = *s++;
		}
		/* allow nested opens and closes */
		else if (*s == PL_multi_close && --brackets <= 0)
		    break;
		else if (*s == PL_multi_open)
		    brackets++;
		else if (!has_utf8 && UTF8_IS_CONTINUED(*s) && UTF)
		    has_utf8 = TRUE;
		*to = *s;
	    }
	}
	/* terminate the copied string and update the sv's end-of-string */
	*to = '\0';
	SvCUR_set(sv, to - SvPVX(sv));

	/*
	 * this next chunk reads more into the buffer if we're not done yet
	 */

  	if (s < PL_bufend)
	    break;		/* handle case where we are done yet :-) */

#ifndef PERL_STRICT_CR
	if (to - SvPVX(sv) >= 2) {
	    if ((to[-2] == '\r' && to[-1] == '\n') ||
		(to[-2] == '\n' && to[-1] == '\r'))
	    {
		to[-2] = '\n';
		to--;
		SvCUR_set(sv, to - SvPVX(sv));
	    }
	    else if (to[-1] == '\r')
		to[-1] = '\n';
	}
	else if (to - SvPVX(sv) == 1 && to[-1] == '\r')
	    to[-1] = '\n';
#endif
	
	/* if we're out of file, or a read fails, bail and reset the current
	   line marker so we can report where the unterminated string began
	*/
	if (!PL_rsfp ||
	 !(PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = filter_gets(PL_linestr, PL_rsfp, 0))) {
	    sv_free(sv);
	    CopLINE_set(PL_curcop, PL_multi_start);
	    return Nullch;
	}
	/* we read a line, so increment our line counter */
	CopLINE_inc(PL_curcop);

	/* update debugger info */
	if (PERLDB_LINE && PL_curstash != PL_debstash) {
	    SV *sv = NEWSV(88,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setsv(sv,PL_linestr);
	    av_store(CopFILEAV(PL_curcop), (I32)CopLINE(PL_curcop), sv);
	}

	/* having changed the buffer, we must update PL_bufend */
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	PL_last_lop = PL_last_uni = Nullch;
    }

    /* at this point, we have successfully read the delimited string */

    if (keep_delims)
	sv_catpvn(sv, s, 1);
    if (has_utf8)
	SvUTF8_on(sv);
    PL_multi_end = CopLINE(PL_curcop);
    s++;

    /* if we allocated too much space, give some back */
    if (SvCUR(sv) + 5 < SvLEN(sv)) {
	SvLEN_set(sv, SvCUR(sv) + 1);
	Renew(SvPVX(sv), SvLEN(sv), char);
    }

    /* decide whether this is the first or second quoted string we've read
       for this op
    */

    if (PL_lex_stuff)
	PL_lex_repl = sv;
    else
	PL_lex_stuff = sv;
    return s;
}

/*
  scan_num
  takes: pointer to position in buffer
  returns: pointer to new position in buffer
  side-effects: builds ops for the constant in yylval.op

  Read a number in any of the formats that Perl accepts:

  0(x[0-7A-F]+)|([0-7]+)|(b[01])
  [\d_]+(\.[\d_]*)?[Ee](\d+)

  Underbars (_) are allowed in decimal numbers.  If -w is on,
  underbars before a decimal point must be at three digit intervals.

  Like most scan_ routines, it uses the PL_tokenbuf buffer to hold the
  thing it reads.

  If it reads a number without a decimal point or an exponent, it will
  try converting the number to an integer and see if it can do so
  without loss of precision.
*/

char *
Perl_scan_num(pTHX_ char *start, YYSTYPE* lvalp)
{
    register char *s = start;		/* current position in buffer */
    register char *d;			/* destination in temp buffer */
    register char *e;			/* end of temp buffer */
    NV nv;				/* number read, as a double */
    SV *sv = Nullsv;			/* place to put the converted number */
    bool floatit;			/* boolean: int or float? */
    char *lastub = 0;			/* position of last underbar */
    static char number_too_long[] = "Number too long";

    /* We use the first character to decide what type of number this is */

    switch (*s) {
    default:
      Perl_croak(aTHX_ "panic: scan_num");

    /* if it starts with a 0, it could be an octal number, a decimal in
       0.13 disguise, or a hexadecimal number, or a binary number. */
    case '0':
	{
	  /* variables:
	     u		holds the "number so far"
	     shift	the power of 2 of the base
			(hex == 4, octal == 3, binary == 1)
	     overflowed	was the number more than we can hold?

	     Shift is used when we add a digit.  It also serves as an "are
	     we in octal/hex/binary?" indicator to disallow hex characters
	     when in octal mode.
	   */
	    NV n = 0.0;
	    UV u = 0;
	    I32 shift;
	    bool overflowed = FALSE;
	    static NV nvshift[5] = { 1.0, 2.0, 4.0, 8.0, 16.0 };
	    static char* bases[5] = { "", "binary", "", "octal",
				      "hexadecimal" };
	    static char* Bases[5] = { "", "Binary", "", "Octal",
				      "Hexadecimal" };
	    static char *maxima[5] = { "",
				       "0b11111111111111111111111111111111",
				       "",
				       "037777777777",
				       "0xffffffff" };
	    char *base, *Base, *max;

	    /* check for hex */
	    if (s[1] == 'x') {
		shift = 4;
		s += 2;
	    } else if (s[1] == 'b') {
		shift = 1;
		s += 2;
	    }
	    /* check for a decimal in disguise */
	    else if (s[1] == '.' || s[1] == 'e' || s[1] == 'E')
		goto decimal;
	    /* so it must be octal */
	    else
		shift = 3;

	    base = bases[shift];
	    Base = Bases[shift];
	    max  = maxima[shift];

	    /* read the rest of the number */
	    for (;;) {
		/* x is used in the overflow test,
		   b is the digit we're adding on. */
		UV x, b;

		switch (*s) {

		/* if we don't mention it, we're done */
		default:
		    goto out;

		/* _ are ignored */
		case '_':
		    s++;
		    break;

		/* 8 and 9 are not octal */
		case '8': case '9':
		    if (shift == 3)
			yyerror(Perl_form(aTHX_ "Illegal octal digit '%c'", *s));
		    /* FALL THROUGH */

	        /* octal digits */
		case '2': case '3': case '4':
		case '5': case '6': case '7':
		    if (shift == 1)
			yyerror(Perl_form(aTHX_ "Illegal binary digit '%c'", *s));
		    /* FALL THROUGH */

		case '0': case '1':
		    b = *s++ & 15;		/* ASCII digit -> value of digit */
		    goto digit;

	        /* hex digits */
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		    /* make sure they said 0x */
		    if (shift != 4)
			goto out;
		    b = (*s++ & 7) + 9;

		    /* Prepare to put the digit we have onto the end
		       of the number so far.  We check for overflows.
		    */

		  digit:
		    if (!overflowed) {
			x = u << shift;	/* make room for the digit */

			if ((x >> shift) != u
			    && !(PL_hints & HINT_NEW_BINARY)) {
			    overflowed = TRUE;
			    n = (NV) u;
			    if (ckWARN_d(WARN_OVERFLOW))
				Perl_warner(aTHX_ WARN_OVERFLOW,
					    "Integer overflow in %s number",
					    base);
			} else
			    u = x | b;		/* add the digit to the end */
		    }
		    if (overflowed) {
			n *= nvshift[shift];
			/* If an NV has not enough bits in its
			 * mantissa to represent an UV this summing of
			 * small low-order numbers is a waste of time
			 * (because the NV cannot preserve the
			 * low-order bits anyway): we could just
			 * remember when did we overflow and in the
			 * end just multiply n by the right
			 * amount. */
			n += (NV) b;
		    }
		    break;
		}
	    }

	  /* if we get here, we had success: make a scalar value from
	     the number.
	  */
	  out:
	    sv = NEWSV(92,0);
	    if (overflowed) {
		if (ckWARN(WARN_PORTABLE) && n > 4294967295.0)
		    Perl_warner(aTHX_ WARN_PORTABLE,
				"%s number > %s non-portable",
				Base, max);
		sv_setnv(sv, n);
	    }
	    else {
#if UVSIZE > 4
		if (ckWARN(WARN_PORTABLE) && u > 0xffffffff)
		    Perl_warner(aTHX_ WARN_PORTABLE,
				"%s number > %s non-portable",
				Base, max);
#endif
		sv_setuv(sv, u);
	    }
	    if (PL_hints & HINT_NEW_BINARY)
		sv = new_constant(start, s - start, "binary", sv, Nullsv, NULL);
	}
	break;

    /*
      handle decimal numbers.
      we're also sent here when we read a 0 as the first digit
    */
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '.':
      decimal:
	d = PL_tokenbuf;
	e = PL_tokenbuf + sizeof PL_tokenbuf - 6; /* room for various punctuation */
	floatit = FALSE;

	/* read next group of digits and _ and copy into d */
	while (isDIGIT(*s) || *s == '_') {
	    /* skip underscores, checking for misplaced ones
	       if -w is on
	    */
	    if (*s == '_') {
		if (ckWARN(WARN_SYNTAX) && lastub && s - lastub != 3)
		    Perl_warner(aTHX_ WARN_SYNTAX, "Misplaced _ in number");
		lastub = ++s;
	    }
	    else {
	        /* check for end of fixed-length buffer */
		if (d >= e)
		    Perl_croak(aTHX_ number_too_long);
		/* if we're ok, copy the character */
		*d++ = *s++;
	    }
	}

	/* final misplaced underbar check */
	if (lastub && s - lastub != 3) {
	    if (ckWARN(WARN_SYNTAX))
		Perl_warner(aTHX_ WARN_SYNTAX, "Misplaced _ in number");
	}

	/* read a decimal portion if there is one.  avoid
	   3..5 being interpreted as the number 3. followed
	   by .5
	*/
	if (*s == '.' && s[1] != '.') {
	    floatit = TRUE;
	    *d++ = *s++;

	    /* copy, ignoring underbars, until we run out of
	       digits.  Note: no misplaced underbar checks!
	    */
	    for (; isDIGIT(*s) || *s == '_'; s++) {
	        /* fixed length buffer check */
		if (d >= e)
		    Perl_croak(aTHX_ number_too_long);
		if (*s != '_')
		    *d++ = *s;
	    }
	    if (*s == '.' && isDIGIT(s[1])) {
		/* oops, it's really a v-string, but without the "v" */
		s = start - 1;
		goto vstring;
	    }
	}

	/* read exponent part, if present */
	if (*s && strchr("eE",*s) && strchr("+-0123456789",s[1])) {
	    floatit = TRUE;
	    s++;

	    /* regardless of whether user said 3E5 or 3e5, use lower 'e' */
	    *d++ = 'e';		/* At least some Mach atof()s don't grok 'E' */

	    /* allow positive or negative exponent */
	    if (*s == '+' || *s == '-')
		*d++ = *s++;

	    /* read digits of exponent (no underbars :-) */
	    while (isDIGIT(*s)) {
		if (d >= e)
		    Perl_croak(aTHX_ number_too_long);
		*d++ = *s++;
	    }
	}

	/* terminate the string */
	*d = '\0';

	/* make an sv from the string */
	sv = NEWSV(92,0);

#if defined(Strtol) && defined(Strtoul)

	/*
	   strtol/strtoll sets errno to ERANGE if the number is too big
	   for an integer. We try to do an integer conversion first
	   if no characters indicating "float" have been found.
	 */

	if (!floatit) {
    	    IV iv;
    	    UV uv;
	    errno = 0;
	    if (*PL_tokenbuf == '-')
		iv = Strtol(PL_tokenbuf, (char**)NULL, 10);
	    else
		uv = Strtoul(PL_tokenbuf, (char**)NULL, 10);
	    if (errno)
	    	floatit = TRUE; /* Probably just too large. */
	    else if (*PL_tokenbuf == '-')
	    	sv_setiv(sv, iv);
	    else if (uv <= IV_MAX)
		sv_setiv(sv, uv); /* Prefer IVs over UVs. */
	    else
	    	sv_setuv(sv, uv);
	}
	if (floatit) {
	    nv = Atof(PL_tokenbuf);
	    sv_setnv(sv, nv);
	}
#else
	/*
	   No working strtou?ll?.

	   Unfortunately atol() doesn't do range checks (returning
	   LONG_MIN/LONG_MAX, and setting errno to ERANGE on overflows)
	   everywhere [1], so we cannot use use atol() (or atoll()).
	   If we could, they would be used, as Atol(), very much like
	   Strtol() and Strtoul() are used above.

	   [1] XXX Configure test needed to check for atol()
	           (and atoll()) overflow behaviour XXX

	   --jhi

	   We need to do this the hard way.  */

	nv = Atof(PL_tokenbuf);

	/* See if we can make do with an integer value without loss of
	   precision.  We use U_V to cast to a UV, because some
	   compilers have issues.  Then we try casting it back and see
	   if it was the same [1].  We only do this if we know we
	   specifically read an integer.  If floatit is true, then we
	   don't need to do the conversion at all.

	   [1] Note that this is lossy if our NVs cannot preserve our
	   UVs.  There are metaconfig defines NV_PRESERVES_UV (a boolean)
	   and NV_PRESERVES_UV_BITS (a number), but in general we really
	   do hope all such potentially lossy platforms have strtou?ll?
	   to do a lossless IV/UV conversion.

	   Maybe could do some tricks with DBL_DIG, LDBL_DIG and
	   DBL_MANT_DIG and LDBL_MANT_DIG (these are already available
	   as NV_DIG and NV_MANT_DIG)?
	
	   --jhi
	   */
	{
	    UV uv = U_V(nv);
	    if (!floatit && (NV)uv == nv) {
		if (uv <= IV_MAX)
		    sv_setiv(sv, uv); /* Prefer IVs over UVs. */
		else
		    sv_setuv(sv, uv);
	    }
	    else
		sv_setnv(sv, nv);
	}
#endif
	if ( floatit ? (PL_hints & HINT_NEW_FLOAT) :
	               (PL_hints & HINT_NEW_INTEGER) )
	    sv = new_constant(PL_tokenbuf, d - PL_tokenbuf,
			      (floatit ? "float" : "integer"),
			      sv, Nullsv, NULL);
	break;

    /* if it starts with a v, it could be a v-string */
    case 'v':
vstring:
	{
	    char *pos = s;
	    pos++;
	    while (isDIGIT(*pos) || *pos == '_')
		pos++;
	    if (!isALPHA(*pos)) {
		UV rev;
		U8 tmpbuf[UTF8_MAXLEN+1];
		U8 *tmpend;
		bool utf8 = FALSE;
		s++;				/* get past 'v' */

		sv = NEWSV(92,5);
		sv_setpvn(sv, "", 0);

		for (;;) {
		    if (*s == '0' && isDIGIT(s[1]))
			yyerror("Octal number in vector unsupported");
		    rev = 0;
		    {
			/* this is atoi() that tolerates underscores */
			char *end = pos;
			UV mult = 1;
			while (--end >= s) {
			    UV orev;
			    if (*end == '_')
				continue;
			    orev = rev;
			    rev += (*end - '0') * mult;
			    mult *= 10;
			    if (orev > rev && ckWARN_d(WARN_OVERFLOW))
				Perl_warner(aTHX_ WARN_OVERFLOW,
					    "Integer overflow in decimal number");
			}
		    }
		    tmpend = uv_to_utf8(tmpbuf, rev);
		    utf8 = utf8 || rev > 127;
		    sv_catpvn(sv, (const char*)tmpbuf, tmpend - tmpbuf);
		    if (*pos == '.' && isDIGIT(pos[1]))
			s = ++pos;
		    else {
			s = pos;
			break;
		    }
		    while (isDIGIT(*pos) || *pos == '_')
			pos++;
		}

		SvPOK_on(sv);
		SvREADONLY_on(sv);
		if (utf8) {
		    SvUTF8_on(sv);
		    if (!UTF||IN_BYTE)
		      sv_utf8_downgrade(sv, TRUE);
		}
	    }
	}
	break;
    }

    /* make the op for the constant and return */

    if (sv)
	lvalp->opval = newSVOP(OP_CONST, 0, sv);
    else
	lvalp->opval = Nullop;

    return s;
}

STATIC char *
S_scan_formline(pTHX_ register char *s)
{
    register char *eol;
    register char *t;
    SV *stuff = newSVpvn("",0);
    bool needargs = FALSE;

    while (!needargs) {
	if (*s == '.' || *s == /*{*/'}') {
	    /*SUPPRESS 530*/
#ifdef PERL_STRICT_CR
	    for (t = s+1;SPACE_OR_TAB(*t); t++) ;
#else
	    for (t = s+1;SPACE_OR_TAB(*t) || *t == '\r'; t++) ;
#endif
	    if (*t == '\n' || t == PL_bufend)
		break;
	}
	if (PL_in_eval && !PL_rsfp) {
	    eol = strchr(s,'\n');
	    if (!eol++)
		eol = PL_bufend;
	}
	else
	    eol = PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	if (*s != '#') {
	    for (t = s; t < eol; t++) {
		if (*t == '~' && t[1] == '~' && SvCUR(stuff)) {
		    needargs = FALSE;
		    goto enough;	/* ~~ must be first line in formline */
		}
		if (*t == '@' || *t == '^')
		    needargs = TRUE;
	    }
	    sv_catpvn(stuff, s, eol-s);
#ifndef PERL_STRICT_CR
	    if (eol-s > 1 && eol[-2] == '\r' && eol[-1] == '\n') {
		char *end = SvPVX(stuff) + SvCUR(stuff);
		end[-2] = '\n';
		end[-1] = '\0';
		SvCUR(stuff)--;
	    }
#endif
	}
	s = eol;
	if (PL_rsfp) {
	    s = filter_gets(PL_linestr, PL_rsfp, 0);
	    PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = PL_linestart = SvPVX(PL_linestr);
	    PL_bufend = PL_bufptr + SvCUR(PL_linestr);
	    PL_last_lop = PL_last_uni = Nullch;
	    if (!s) {
		s = PL_bufptr;
		yyerror("Format not terminated");
		break;
	    }
	}
	incline(s);
    }
  enough:
    if (SvCUR(stuff)) {
	PL_expect = XTERM;
	if (needargs) {
	    PL_lex_state = LEX_NORMAL;
	    PL_nextval[PL_nexttoke].ival = 0;
	    force_next(',');
	}
	else
	    PL_lex_state = LEX_FORMLINE;
	PL_nextval[PL_nexttoke].opval = (OP*)newSVOP(OP_CONST, 0, stuff);
	force_next(THING);
	PL_nextval[PL_nexttoke].ival = OP_FORMLINE;
	force_next(LSTOP);
    }
    else {
	SvREFCNT_dec(stuff);
	PL_lex_formbrack = 0;
	PL_bufptr = s;
    }
    return s;
}

STATIC void
S_set_csh(pTHX)
{
#ifdef CSH
    if (!PL_cshlen)
	PL_cshlen = strlen(PL_cshname);
#endif
}

I32
Perl_start_subparse(pTHX_ I32 is_format, U32 flags)
{
    I32 oldsavestack_ix = PL_savestack_ix;
    CV* outsidecv = PL_compcv;
    AV* comppadlist;

    if (PL_compcv) {
	assert(SvTYPE(PL_compcv) == SVt_PVCV);
    }
    SAVEI32(PL_subline);
    save_item(PL_subname);
    SAVEI32(PL_padix);
    SAVECOMPPAD();
    SAVESPTR(PL_comppad_name);
    SAVESPTR(PL_compcv);
    SAVEI32(PL_comppad_name_fill);
    SAVEI32(PL_min_intro_pending);
    SAVEI32(PL_max_intro_pending);
    SAVEI32(PL_pad_reset_pending);

    PL_compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)PL_compcv, is_format ? SVt_PVFM : SVt_PVCV);
    CvFLAGS(PL_compcv) |= flags;

    PL_comppad = newAV();
    av_push(PL_comppad, Nullsv);
    PL_curpad = AvARRAY(PL_comppad);
    PL_comppad_name = newAV();
    PL_comppad_name_fill = 0;
    PL_min_intro_pending = 0;
    PL_padix = 0;
    PL_subline = CopLINE(PL_curcop);
#ifdef USE_THREADS
    av_store(PL_comppad_name, 0, newSVpvn("@_", 2));
    PL_curpad[0] = (SV*)newAV();
    SvPADMY_on(PL_curpad[0]);	/* XXX Needed? */
#endif /* USE_THREADS */

    comppadlist = newAV();
    AvREAL_off(comppadlist);
    av_store(comppadlist, 0, (SV*)PL_comppad_name);
    av_store(comppadlist, 1, (SV*)PL_comppad);

    CvPADLIST(PL_compcv) = comppadlist;
    CvOUTSIDE(PL_compcv) = (CV*)SvREFCNT_inc(outsidecv);
#ifdef USE_THREADS
    CvOWNER(PL_compcv) = 0;
    New(666, CvMUTEXP(PL_compcv), 1, perl_mutex);
    MUTEX_INIT(CvMUTEXP(PL_compcv));
#endif /* USE_THREADS */

    return oldsavestack_ix;
}

#ifdef __SC__
#pragma segment Perl_yylex
#endif
int
Perl_yywarn(pTHX_ char *s)
{
    PL_in_eval |= EVAL_WARNONLY;
    yyerror(s);
    PL_in_eval &= ~EVAL_WARNONLY;
    return 0;
}

int
Perl_yyerror(pTHX_ char *s)
{
    char *where = NULL;
    char *context = NULL;
    int contlen = -1;
    SV *msg;

    if (!yychar || (yychar == ';' && !PL_rsfp))
	where = "at EOF";
    else if (PL_bufptr > PL_oldoldbufptr && PL_bufptr - PL_oldoldbufptr < 200 &&
      PL_oldoldbufptr != PL_oldbufptr && PL_oldbufptr != PL_bufptr) {
	while (isSPACE(*PL_oldoldbufptr))
	    PL_oldoldbufptr++;
	context = PL_oldoldbufptr;
	contlen = PL_bufptr - PL_oldoldbufptr;
    }
    else if (PL_bufptr > PL_oldbufptr && PL_bufptr - PL_oldbufptr < 200 &&
      PL_oldbufptr != PL_bufptr) {
	while (isSPACE(*PL_oldbufptr))
	    PL_oldbufptr++;
	context = PL_oldbufptr;
	contlen = PL_bufptr - PL_oldbufptr;
    }
    else if (yychar > 255)
	where = "next token ???";
#ifdef USE_PURE_BISON
/*  GNU Bison sets the value -2 */
    else if (yychar == -2) {
#else
    else if ((yychar & 127) == 127) {
#endif
	if (PL_lex_state == LEX_NORMAL ||
	   (PL_lex_state == LEX_KNOWNEXT && PL_lex_defer == LEX_NORMAL))
	    where = "at end of line";
	else if (PL_lex_inpat)
	    where = "within pattern";
	else
	    where = "within string";
    }
    else {
	SV *where_sv = sv_2mortal(newSVpvn("next char ", 10));
	if (yychar < 32)
	    Perl_sv_catpvf(aTHX_ where_sv, "^%c", toCTRL(yychar));
	else if (isPRINT_LC(yychar))
	    Perl_sv_catpvf(aTHX_ where_sv, "%c", yychar);
	else
	    Perl_sv_catpvf(aTHX_ where_sv, "\\%03o", yychar & 255);
	where = SvPVX(where_sv);
    }
    msg = sv_2mortal(newSVpv(s, 0));
    Perl_sv_catpvf(aTHX_ msg, " at %s line %"IVdf", ",
		   CopFILE(PL_curcop), (IV)CopLINE(PL_curcop));
    if (context)
	Perl_sv_catpvf(aTHX_ msg, "near \"%.*s\"\n", contlen, context);
    else
	Perl_sv_catpvf(aTHX_ msg, "%s\n", where);
    if (PL_multi_start < PL_multi_end && (U32)(CopLINE(PL_curcop) - PL_multi_end) <= 1) {
        Perl_sv_catpvf(aTHX_ msg,
        "  (Might be a runaway multi-line %c%c string starting on line %"IVdf")\n",
                (int)PL_multi_open,(int)PL_multi_close,(IV)PL_multi_start);
        PL_multi_end = 0;
    }
    if (PL_in_eval & EVAL_WARNONLY)
	Perl_warn(aTHX_ "%"SVf, msg);
    else
	qerror(msg);
    if (PL_error_count >= 10) {
	if (PL_in_eval && SvCUR(ERRSV))
	    Perl_croak(aTHX_ "%"SVf"%s has too many errors.\n",
		       ERRSV, CopFILE(PL_curcop));
	else
	    Perl_croak(aTHX_ "%s has too many errors.\n",
		       CopFILE(PL_curcop));
    }
    PL_in_my = 0;
    PL_in_my_stash = Nullhv;
    return 0;
}
#ifdef __SC__
#pragma segment Main
#endif

STATIC char*
S_swallow_bom(pTHX_ U8 *s)
{
    STRLEN slen;
    slen = SvCUR(PL_linestr);
    switch (*s) {
    case 0xFF:
	if (s[1] == 0xFE) {
	    /* UTF-16 little-endian */
	    if (s[2] == 0 && s[3] == 0)  /* UTF-32 little-endian */
		Perl_croak(aTHX_ "Unsupported script encoding");
#ifndef PERL_NO_UTF16_FILTER
	    DEBUG_p(PerlIO_printf(Perl_debug_log, "UTF-LE script encoding\n"));
	    s += 2;
	    if (PL_bufend > (char*)s) {
		U8 *news;
		I32 newlen;

		filter_add(utf16rev_textfilter, NULL);
		New(898, news, (PL_bufend - (char*)s) * 3 / 2 + 1, U8);
		PL_bufend = (char*)utf16_to_utf8_reversed(s, news,
						 PL_bufend - (char*)s - 1,
						 &newlen);
		Copy(news, s, newlen, U8);
		SvCUR_set(PL_linestr, newlen);
		PL_bufend = SvPVX(PL_linestr) + newlen;
		news[newlen++] = '\0';
		Safefree(news);
	    }
#else
	    Perl_croak(aTHX_ "Unsupported script encoding");
#endif
	}
	break;

    case 0xFE:
	if (s[1] == 0xFF) {   /* UTF-16 big-endian */
#ifndef PERL_NO_UTF16_FILTER
	    DEBUG_p(PerlIO_printf(Perl_debug_log, "UTF-16BE script encoding\n"));
	    s += 2;
	    if (PL_bufend > (char *)s) {
		U8 *news;
		I32 newlen;

		filter_add(utf16_textfilter, NULL);
		New(898, news, (PL_bufend - (char*)s) * 3 / 2 + 1, U8);
		PL_bufend = (char*)utf16_to_utf8(s, news,
						 PL_bufend - (char*)s,
						 &newlen);
		Copy(news, s, newlen, U8);
		SvCUR_set(PL_linestr, newlen);
		PL_bufend = SvPVX(PL_linestr) + newlen;
		news[newlen++] = '\0';
		Safefree(news);
	    }
#else
	    Perl_croak(aTHX_ "Unsupported script encoding");
#endif
	}
	break;

    case 0xEF:
	if (slen > 2 && s[1] == 0xBB && s[2] == 0xBF) {
	    DEBUG_p(PerlIO_printf(Perl_debug_log, "UTF-8 script encoding\n"));
	    s += 3;                      /* UTF-8 */
	}
	break;
    case 0:
	if (slen > 3 && s[1] == 0 &&  /* UTF-32 big-endian */
	    s[2] == 0xFE && s[3] == 0xFF)
	{
	    Perl_croak(aTHX_ "Unsupported script encoding");
	}
    }
    return (char*)s;
}

#ifdef PERL_OBJECT
#include "XSUB.h"
#endif

/*
 * restore_rsfp
 * Restore a source filter.
 */

static void
restore_rsfp(pTHXo_ void *f)
{
    PerlIO *fp = (PerlIO*)f;

    if (PL_rsfp == PerlIO_stdin())
	PerlIO_clearerr(PL_rsfp);
    else if (PL_rsfp && (PL_rsfp != fp))
	PerlIO_close(PL_rsfp);
    PL_rsfp = fp;
}

#ifndef PERL_NO_UTF16_FILTER
static I32
utf16_textfilter(pTHXo_ int idx, SV *sv, int maxlen)
{
    I32 count = FILTER_READ(idx+1, sv, maxlen);
    if (count) {
	U8* tmps;
	U8* tend;
	I32 newlen;
	New(898, tmps, SvCUR(sv) * 3 / 2 + 1, U8);
	if (!*SvPV_nolen(sv))
	/* Game over, but don't feed an odd-length string to utf16_to_utf8 */
	return count;

	tend = utf16_to_utf8((U8*)SvPVX(sv), tmps, SvCUR(sv), &newlen);
	sv_usepvn(sv, (char*)tmps, tend - tmps);
    }
    return count;
}

static I32
utf16rev_textfilter(pTHXo_ int idx, SV *sv, int maxlen)
{
    I32 count = FILTER_READ(idx+1, sv, maxlen);
    if (count) {
	U8* tmps;
	U8* tend;
	I32 newlen;
	if (!*SvPV_nolen(sv))
	/* Game over, but don't feed an odd-length string to utf16_to_utf8 */
	return count;

	New(898, tmps, SvCUR(sv) * 3 / 2 + 1, U8);
	tend = utf16_to_utf8_reversed((U8*)SvPVX(sv), tmps, SvCUR(sv), &newlen);
	sv_usepvn(sv, (char*)tmps, tend - tmps);
    }
    return count;
}
#endif
