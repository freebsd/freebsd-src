/*    toke.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *   "It all comes from here, the stench and the peril."  --Frodo
 */

#include "EXTERN.h"
#include "perl.h"

#ifndef PERL_OBJECT
static void check_uni _((void));
static void  force_next _((I32 type));
static char *force_version _((char *start));
static char *force_word _((char *start, int token, int check_keyword, int allow_pack, int allow_tick));
static SV *tokeq _((SV *sv));
static char *scan_const _((char *start));
static char *scan_formline _((char *s));
static char *scan_heredoc _((char *s));
static char *scan_ident _((char *s, char *send, char *dest, STRLEN destlen,
			   I32 ck_uni));
static char *scan_inputsymbol _((char *start));
static char *scan_pat _((char *start, I32 type));
static char *scan_str _((char *start));
static char *scan_subst _((char *start));
static char *scan_trans _((char *start));
static char *scan_word _((char *s, char *dest, STRLEN destlen,
			  int allow_package, STRLEN *slp));
static char *skipspace _((char *s));
static void checkcomma _((char *s, char *name, char *what));
static void force_ident _((char *s, int kind));
static void incline _((char *s));
static int intuit_method _((char *s, GV *gv));
static int intuit_more _((char *s));
static I32 lop _((I32 f, expectation x, char *s));
static void missingterm _((char *s));
static void no_op _((char *what, char *s));
static void set_csh _((void));
static I32 sublex_done _((void));
static I32 sublex_push _((void));
static I32 sublex_start _((void));
#ifdef CRIPPLED_CC
static int uni _((I32 f, char *s));
#endif
static char * filter_gets _((SV *sv, PerlIO *fp, STRLEN append));
static void restore_rsfp _((void *f));
static SV *new_constant _((char *s, STRLEN len, char *key, SV *sv, SV *pv, char *type));
static void restore_expect _((void *e));
static void restore_lex_expect _((void *e));

static char *PL_super_bufptr;
static char *PL_super_bufend;
#endif /* PERL_OBJECT */

static char ident_too_long[] = "Identifier too long";

/* The following are arranged oddly so that the guard on the switch statement
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

#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#  include <unistd.h> /* Needed for execv() */
#endif


#ifdef ff_next
#undef ff_next
#endif

#include "keywords.h"

#ifdef CLINE
#undef CLINE
#endif
#define CLINE (PL_copline = (PL_curcop->cop_line < PL_copline ? PL_curcop->cop_line : PL_copline))

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

STATIC int
ao(int toketype)
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

STATIC void
no_op(char *what, char *s)
{
    char *oldbp = PL_bufptr;
    bool is_first = (PL_oldbufptr == PL_linestart);

    PL_bufptr = s;
    yywarn(form("%s found where operator expected", what));
    if (is_first)
	warn("\t(Missing semicolon on previous line?)\n");
    else if (PL_oldoldbufptr && isIDFIRST(*PL_oldoldbufptr)) {
	char *t;
	for (t = PL_oldoldbufptr; *t && (isALNUM(*t) || *t == ':'); t++) ;
	if (t < PL_bufptr && isSPACE(*t))
	    warn("\t(Do you need to predeclare %.*s?)\n",
		t - PL_oldoldbufptr, PL_oldoldbufptr);

    }
    else
	warn("\t(Missing operator before %.*s?)\n", s - oldbp, oldbp);
    PL_bufptr = oldbp;
}

STATIC void
missingterm(char *s)
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
    croak("Can't find string terminator %c%s%c anywhere before EOF",q,s,q);
}

void
deprecate(char *s)
{
    if (PL_dowarn)
	warn("Use of %s is deprecated", s);
}

STATIC void
depcom(void)
{
    deprecate("comma-less variable list");
}

#ifdef WIN32

STATIC I32
win32_textfilter(int idx, SV *sv, int maxlen)
{
 I32 count = FILTER_READ(idx+1, sv, maxlen);
 if (count > 0 && !maxlen)
  win32_strip_return(sv);
 return count;
}
#endif


void
lex_start(SV *line)
{
    dTHR;
    char *s;
    STRLEN len;

    SAVEI32(PL_lex_dojoin);
    SAVEI32(PL_lex_brackets);
    SAVEI32(PL_lex_fakebrack);
    SAVEI32(PL_lex_casemods);
    SAVEI32(PL_lex_starts);
    SAVEI32(PL_lex_state);
    SAVESPTR(PL_lex_inpat);
    SAVEI32(PL_lex_inwhat);
    SAVEI16(PL_curcop->cop_line);
    SAVEPPTR(PL_bufptr);
    SAVEPPTR(PL_bufend);
    SAVEPPTR(PL_oldbufptr);
    SAVEPPTR(PL_oldoldbufptr);
    SAVEPPTR(PL_linestart);
    SAVESPTR(PL_linestr);
    SAVEPPTR(PL_lex_brackstack);
    SAVEPPTR(PL_lex_casestack);
    SAVEDESTRUCTOR(restore_rsfp, PL_rsfp);
    SAVESPTR(PL_lex_stuff);
    SAVEI32(PL_lex_defer);
    SAVESPTR(PL_lex_repl);
    SAVEDESTRUCTOR(restore_expect, PL_tokenbuf + PL_expect); /* encode as pointer */
    SAVEDESTRUCTOR(restore_lex_expect, PL_tokenbuf + PL_expect);

    PL_lex_state = LEX_NORMAL;
    PL_lex_defer = 0;
    PL_expect = XSTATE;
    PL_lex_brackets = 0;
    PL_lex_fakebrack = 0;
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
    PL_lex_inwhat = 0;
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
    SvREFCNT_dec(PL_rs);
    PL_rs = newSVpv("\n", 1);
    PL_rsfp = 0;
}

void
lex_end(void)
{
    PL_doextract = FALSE;
}

STATIC void
restore_rsfp(void *f)
{
    PerlIO *fp = (PerlIO*)f;

    if (PL_rsfp == PerlIO_stdin())
	PerlIO_clearerr(PL_rsfp);
    else if (PL_rsfp && (PL_rsfp != fp))
	PerlIO_close(PL_rsfp);
    PL_rsfp = fp;
}

STATIC void
restore_expect(void *e)
{
    /* a safe way to store a small integer in a pointer */
    PL_expect = (expectation)((char *)e - PL_tokenbuf);
}

STATIC void
restore_lex_expect(void *e)
{
    /* a safe way to store a small integer in a pointer */
    PL_lex_expect = (expectation)((char *)e - PL_tokenbuf);
}

STATIC void
incline(char *s)
{
    dTHR;
    char *t;
    char *n;
    char ch;
    int sawline = 0;

    PL_curcop->cop_line++;
    if (*s++ != '#')
	return;
    while (*s == ' ' || *s == '\t') s++;
    if (strnEQ(s, "line ", 5)) {
	s += 5;
	sawline = 1;
    }
    if (!isDIGIT(*s))
	return;
    n = s;
    while (isDIGIT(*s))
	s++;
    while (*s == ' ' || *s == '\t')
	s++;
    if (*s == '"' && (t = strchr(s+1, '"')))
	s++;
    else {
	if (!sawline)
	    return;		/* false alarm */
	for (t = s; !isSPACE(*t); t++) ;
    }
    ch = *t;
    *t = '\0';
    if (t - s > 0)
	PL_curcop->cop_filegv = gv_fetchfile(s);
    else
	PL_curcop->cop_filegv = gv_fetchfile(PL_origfilename);
    *t = ch;
    PL_curcop->cop_line = atoi(n)-1;
}

STATIC char *
skipspace(register char *s)
{
    dTHR;
    if (PL_lex_formbrack && PL_lex_brackets <= PL_lex_formbrack) {
	while (s < PL_bufend && (*s == ' ' || *s == '\t'))
	    s++;
	return s;
    }
    for (;;) {
	STRLEN prevlen;
	while (s < PL_bufend && isSPACE(*s)) {
	    if (*s++ == '\n' && PL_in_eval && !PL_rsfp)
		incline(s);
	}
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
	if (s < PL_bufend || !PL_rsfp || PL_lex_state != LEX_NORMAL)
	    return s;
	if ((s = filter_gets(PL_linestr, PL_rsfp, (prevlen = SvCUR(PL_linestr)))) == Nullch) {
	    if (PL_minus_n || PL_minus_p) {
		sv_setpv(PL_linestr,PL_minus_p ?
			 ";}continue{print or die qq(-p destination: $!\\n)" :
			 "");
		sv_catpv(PL_linestr,";}");
		PL_minus_n = PL_minus_p = 0;
	    }
	    else
		sv_setpv(PL_linestr,";");
	    PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = s = PL_linestart = SvPVX(PL_linestr);
	    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	    if (PL_preprocess && !PL_in_eval)
		(void)PerlProc_pclose(PL_rsfp);
	    else if ((PerlIO*)PL_rsfp == PerlIO_stdin())
		PerlIO_clearerr(PL_rsfp);
	    else
		(void)PerlIO_close(PL_rsfp);
	    PL_rsfp = Nullfp;
	    return s;
	}
	PL_linestart = PL_bufptr = s + prevlen;
	PL_bufend = s + SvCUR(PL_linestr);
	s = PL_bufptr;
	incline(s);
	if (PERLDB_LINE && PL_curstash != PL_debstash) {
	    SV *sv = NEWSV(85,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setpvn(sv,PL_bufptr,PL_bufend-PL_bufptr);
	    av_store(GvAV(PL_curcop->cop_filegv),(I32)PL_curcop->cop_line,sv);
	}
    }
}

STATIC void
check_uni(void) {
    char *s;
    char ch;
    char *t;

    if (PL_oldoldbufptr != PL_last_uni)
	return;
    while (isSPACE(*PL_last_uni))
	PL_last_uni++;
    for (s = PL_last_uni; isALNUM(*s) || *s == '-'; s++) ;
    if ((t = strchr(s, '(')) && t < PL_bufptr)
	return;
    ch = *s;
    *s = '\0';
    warn("Warning: Use of \"%s\" without parens is ambiguous", PL_last_uni);
    *s = ch;
}

#ifdef CRIPPLED_CC

#undef UNI
#define UNI(f) return uni(f,s)

STATIC int
uni(I32 f, char *s)
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

#define LOP(f,x) return lop(f,x,s)

STATIC I32
lop(I32 f, expectation x, char *s)
{
    dTHR;
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

STATIC void 
force_next(I32 type)
{
    PL_nexttype[PL_nexttoke] = type;
    PL_nexttoke++;
    if (PL_lex_state != LEX_KNOWNEXT) {
	PL_lex_defer = PL_lex_state;
	PL_lex_expect = PL_expect;
	PL_lex_state = LEX_KNOWNEXT;
    }
}

STATIC char *
force_word(register char *start, int token, int check_keyword, int allow_pack, int allow_initial_tick)
{
    register char *s;
    STRLEN len;
    
    start = skipspace(start);
    s = start;
    if (isIDFIRST(*s) ||
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
		force_next(')');
		force_next('(');
	    }
	}
	PL_nextval[PL_nexttoke].opval = (OP*)newSVOP(OP_CONST,0, newSVpv(PL_tokenbuf,0));
	PL_nextval[PL_nexttoke].opval->op_private |= OPpCONST_BARE;
	force_next(token);
    }
    return s;
}

STATIC void
force_ident(register char *s, int kind)
{
    if (s && *s) {
	OP* o = (OP*)newSVOP(OP_CONST, 0, newSVpv(s,0));
	PL_nextval[PL_nexttoke].opval = o;
	force_next(WORD);
	if (kind) {
	    dTHR;		/* just for in_eval */
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

STATIC char *
force_version(char *s)
{
    OP *version = Nullop;

    s = skipspace(s);

    /* default VERSION number -- GBARR */

    if(isDIGIT(*s)) {
        char *d;
        int c;
        for( d=s, c = 1; isDIGIT(*d) || *d == '_' || (*d == '.' && c--); d++);
        if((*d == ';' || isSPACE(*d)) && *(skipspace(d)) != ',') {
            s = scan_num(s);
            /* real VERSION number -- GBARR */
            version = yylval.opval;
        }
    }

    /* NOTE: The parser sees the package name and the VERSION swapped */
    PL_nextval[PL_nexttoke].opval = version;
    force_next(WORD); 

    return (s);
}

STATIC SV *
tokeq(SV *sv)
{
    register char *s;
    register char *send;
    register char *d;
    STRLEN len = 0;
    SV *pv = sv;

    if (!SvLEN(sv))
	goto finish;

    s = SvPV_force(sv, len);
    if (SvIVX(sv) == -1)
	goto finish;
    send = s + len;
    while (s < send && *s != '\\')
	s++;
    if (s == send)
	goto finish;
    d = s;
    if ( PL_hints & HINT_NEW_STRING )
	pv = sv_2mortal(newSVpv(SvPVX(pv), len));
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

STATIC I32
sublex_start(void)
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
	    nsv = newSVpv(p, len);
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

STATIC I32
sublex_push(void)
{
    dTHR;
    ENTER;

    PL_lex_state = PL_sublex_info.super_state;
    SAVEI32(PL_lex_dojoin);
    SAVEI32(PL_lex_brackets);
    SAVEI32(PL_lex_fakebrack);
    SAVEI32(PL_lex_casemods);
    SAVEI32(PL_lex_starts);
    SAVEI32(PL_lex_state);
    SAVESPTR(PL_lex_inpat);
    SAVEI32(PL_lex_inwhat);
    SAVEI16(PL_curcop->cop_line);
    SAVEPPTR(PL_bufptr);
    SAVEPPTR(PL_oldbufptr);
    SAVEPPTR(PL_oldoldbufptr);
    SAVEPPTR(PL_linestart);
    SAVESPTR(PL_linestr);
    SAVEPPTR(PL_lex_brackstack);
    SAVEPPTR(PL_lex_casestack);

    PL_linestr = PL_lex_stuff;
    PL_lex_stuff = Nullsv;

    PL_bufend = PL_bufptr = PL_oldbufptr = PL_oldoldbufptr = PL_linestart = SvPVX(PL_linestr);
    PL_bufend += SvCUR(PL_linestr);
    SAVEFREESV(PL_linestr);

    PL_lex_dojoin = FALSE;
    PL_lex_brackets = 0;
    PL_lex_fakebrack = 0;
    New(899, PL_lex_brackstack, 120, char);
    New(899, PL_lex_casestack, 12, char);
    SAVEFREEPV(PL_lex_brackstack);
    SAVEFREEPV(PL_lex_casestack);
    PL_lex_casemods = 0;
    *PL_lex_casestack = '\0';
    PL_lex_starts = 0;
    PL_lex_state = LEX_INTERPCONCAT;
    PL_curcop->cop_line = PL_multi_start;

    PL_lex_inwhat = PL_sublex_info.sub_inwhat;
    if (PL_lex_inwhat == OP_MATCH || PL_lex_inwhat == OP_QR || PL_lex_inwhat == OP_SUBST)
	PL_lex_inpat = PL_sublex_info.sub_op;
    else
	PL_lex_inpat = Nullop;

    return '(';
}

STATIC I32
sublex_done(void)
{
    if (!PL_lex_starts++) {
	PL_expect = XOPERATOR;
	yylval.opval = (OP*)newSVOP(OP_CONST, 0, newSVpv("",0));
	return THING;
    }

    if (PL_lex_casemods) {		/* oops, we've got some unbalanced parens */
	PL_lex_state = LEX_INTERPCASEMOD;
	return yylex();
    }

    /* Is there a right-hand side to take care of? */
    if (PL_lex_repl && (PL_lex_inwhat == OP_SUBST || PL_lex_inwhat == OP_TRANS)) {
	PL_linestr = PL_lex_repl;
	PL_lex_inpat = 0;
	PL_bufend = PL_bufptr = PL_oldbufptr = PL_oldoldbufptr = PL_linestart = SvPVX(PL_linestr);
	PL_bufend += SvCUR(PL_linestr);
	SAVEFREESV(PL_linestr);
	PL_lex_dojoin = FALSE;
	PL_lex_brackets = 0;
	PL_lex_fakebrack = 0;
	PL_lex_casemods = 0;
	*PL_lex_casestack = '\0';
	PL_lex_starts = 0;
	if (SvCOMPILED(PL_lex_repl)) {
	    PL_lex_state = LEX_INTERPNORMAL;
	    PL_lex_starts++;
	}
	else
	    PL_lex_state = LEX_INTERPCONCAT;
	PL_lex_repl = Nullsv;
	return ',';
    }
    else {
	LEAVE;
	PL_bufend = SvPVX(PL_linestr);
	PL_bufend += SvCUR(PL_linestr);
	PL_expect = XOPERATOR;
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
scan_const(char *start)
{
    register char *send = PL_bufend;		/* end of the constant */
    SV *sv = NEWSV(93, send - start);		/* sv for the constant */
    register char *s = start;			/* start of the constant */
    register char *d = SvPVX(sv);		/* destination for copies */
    bool dorange = FALSE;			/* are we in a translit range? */
    I32 len;					/* ? */

    /* leaveit is the set of acceptably-backslashed characters */
    char *leaveit =
	PL_lex_inpat
	    ? "\\.^$@AGZdDwWsSbB+*?|()-nrtfeaxcz0123456789[{]} \t\n\r\f\v#"
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
		SvGROW(sv, SvLEN(sv) + 256);	/* expand the sv -- there'll never be more'n 256 chars in a range for it to grow by */
		d = SvPVX(sv) + i;		/* restore d after the grow potentially has changed the ptr */
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
		dorange = TRUE;
		s++;
	    }
	}

	/* if we get here, we're not doing a transliteration */

	/* skip for regexp comments /(?#comment)/ */
	else if (*s == '(' && PL_lex_inpat && s[1] == '?') {
	    if (s[2] == '#') {
		while (s < send && *s != ')')
		    *d++ = *s++;
	    } else if (s[2] == '{') {	/* This should march regcomp.c */
		I32 count = 1;
		char *regparse = s + 3;
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
		if (*regparse == ')')
		    regparse++;
		else
		    yyerror("Sequence (?{...}) not terminated or not {}-balanced");
		while (s < regparse && *s != ')')
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
	else if (*s == '@' && s[1] && (isALNUM(s[1]) || strchr(":'{$", s[1])))
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
	    s++;

	    /* some backslashes we leave behind */
	    if (*s && strchr(leaveit, *s)) {
		*d++ = '\\';
		*d++ = *s++;
		continue;
	    }

	    /* deprecate \1 in strings and substitution replacements */
	    if (PL_lex_inwhat == OP_SUBST && !PL_lex_inpat &&
		isDIGIT(*s) && *s != '0' && !isDIGIT(s[1]))
	    {
		if (PL_dowarn)
		    warn("\\%c better written as $%c", *s, *s);
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
	    /* default action is to copy the quoted character */
	    default:
		*d++ = *s++;
		continue;

	    /* \132 indicates an octal constant */
	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
		*d++ = scan_oct(s, 3, &len);
		s += len;
		continue;

	    /* \x24 indicates a hex constant */
	    case 'x':
		*d++ = scan_hex(++s, 2, &len);
		s += len;
		continue;

	    /* \c is a control character */
	    case 'c':
		s++;
#ifdef EBCDIC
		*d = *s++;
		if (isLOWER(*d))
		   *d = toUPPER(*d);
		*d++ = toCTRL(*d); 
#else
		len = *s++;
		*d++ = toCTRL(len);
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
	    case 'e':
		*d++ = '\033';
		break;
	    case 'a':
		*d++ = '\007';
		break;
	    } /* end switch */

	    s++;
	    continue;
	} /* end if (backslash) */

	*d++ = *s++;
    } /* while loop to process each character */

    /* terminate the string and set up the sv */
    *d = '\0';
    SvCUR_set(sv, d - SvPVX(sv));
    SvPOK_on(sv);

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

/* This is the one truly awful dwimmer necessary to conflate C and sed. */
STATIC int
intuit_more(register char *s)
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
		if (isALNUM(s[1])) {
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

STATIC int
intuit_method(char *start, GV *gv)
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
						   newSVpv(tmpbuf,0));
	    PL_nextval[PL_nexttoke].opval->op_private = OPpCONST_BARE;
	    PL_expect = XTERM;
	    force_next(WORD);
	    PL_bufptr = s;
	    return *s == '(' ? FUNCMETH : METHOD;
	}
    }
    return 0;
}

STATIC char*
incl_perldb(void)
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
 * and the IoDIRP field is used to store the function pointer.
 * Note that IoTOP_NAME, IoFMT_NAME, IoBOTTOM_NAME, if set for
 * private use must be set using malloc'd pointers.
 */
#ifndef PERL_OBJECT
static int filter_debug = 0;
#endif

SV *
filter_add(filter_t funcp, SV *datasv)
{
    if (!funcp){ /* temporary handy debugging hack to be deleted */
	filter_debug = atoi((char*)datasv);
	return NULL;
    }
    if (!PL_rsfp_filters)
	PL_rsfp_filters = newAV();
    if (!datasv)
	datasv = NEWSV(255,0);
    if (!SvUPGRADE(datasv, SVt_PVIO))
        die("Can't upgrade filter_add data to SVt_PVIO");
    IoDIRP(datasv) = (DIR*)funcp; /* stash funcp into spare field */
    if (filter_debug) {
	STRLEN n_a;
	warn("filter_add func %p (%s)", funcp, SvPV(datasv,n_a));
    }
    av_unshift(PL_rsfp_filters, 1);
    av_store(PL_rsfp_filters, 0, datasv) ;
    return(datasv);
}
 

/* Delete most recently added instance of this filter function.	*/
void
filter_del(filter_t funcp)
{
    if (filter_debug)
	warn("filter_del func %p", funcp);
    if (!PL_rsfp_filters || AvFILLp(PL_rsfp_filters)<0)
	return;
    /* if filter is on top of stack (usual case) just pop it off */
    if (IoDIRP(FILTER_DATA(AvFILLp(PL_rsfp_filters))) == (DIR*)funcp){
	sv_free(av_pop(PL_rsfp_filters));

        return;
    }
    /* we need to search for the correct entry and clear it	*/
    die("filter_del can only delete in reverse order (currently)");
}


/* Invoke the n'th filter function for the current rsfp.	 */
I32
filter_read(int idx, SV *buf_sv, int maxlen)
            
               
               		/* 0 = read one text line */
{
    filter_t funcp;
    SV *datasv = NULL;

    if (!PL_rsfp_filters)
	return -1;
    if (idx > AvFILLp(PL_rsfp_filters)){       /* Any more filters?	*/
	/* Provide a default input filter to make life easy.	*/
	/* Note that we append to the line. This is handy.	*/
	if (filter_debug)
	    warn("filter_read %d: from rsfp\n", idx);
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
	if (filter_debug)
	    warn("filter_read %d: skipped (filter deleted)\n", idx);
	return FILTER_READ(idx+1, buf_sv, maxlen); /* recurse */
    }
    /* Get function pointer hidden within datasv	*/
    funcp = (filter_t)IoDIRP(datasv);
    if (filter_debug) {
	STRLEN n_a;
	warn("filter_read %d: via function %p (%s)\n",
		idx, funcp, SvPV(datasv,n_a));
    }
    /* Call function. The function is expected to 	*/
    /* call "FILTER_READ(idx+1, buf_sv)" first.		*/
    /* Return: <0:error, =0:eof, >0:not eof 		*/
    return (*funcp)(PERL_OBJECT_THIS_ idx, buf_sv, maxlen);
}

STATIC char *
filter_gets(register SV *sv, register PerlIO *fp, STRLEN append)
{
#ifdef WIN32FILTER
    if (!PL_rsfp_filters) {
	filter_add(win32_textfilter,NULL);
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


#ifdef DEBUGGING
    static char* exp_name[] =
	{ "OPERATOR", "TERM", "REF", "STATE", "BLOCK", "TERMBLOCK" };
#endif

EXT int yychar;		/* last token */

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

int
yylex(void)
{
    dTHR;
    register char *s;
    register char *d;
    register I32 tmp;
    STRLEN len;
    GV *gv = Nullgv;
    GV **gvp = 0;

    /* check if there's an identifier for us to look at */
    if (PL_pending_ident) {
        /* pit holds the identifier we read and pending_ident is reset */
	char pit = PL_pending_ident;
	PL_pending_ident = 0;

	/* if we're in a my(), we can't allow dynamics here.
	   $foo'bar has already been turned into $foo::bar, so
	   just check for colons.

	   if it's a legal name, the OP is a PADANY.
	*/
	if (PL_in_my) {
	    if (strchr(PL_tokenbuf,':'))
		croak(no_myglob,PL_tokenbuf);

	    yylval.opval = newOP(OP_PADANY, 0);
	    yylval.opval->op_targ = pad_allocmy(PL_tokenbuf);
	    return PRIVATEREF;
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
			    croak("Can't use \"my %s\" in sort comparison",
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
	    if (!gv || ((PL_tokenbuf[0] == '@') ? !GvAV(gv) : !GvHV(gv)))
		yyerror(form("In string, %s now must be written as \\%s",
			     PL_tokenbuf, PL_tokenbuf));
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

    /* when we're already built the next token, just pull it out the queue */
    case LEX_KNOWNEXT:
	PL_nexttoke--;
	yylval = PL_nextval[PL_nexttoke];
	if (!PL_nexttoke) {
	    PL_lex_state = PL_lex_defer;
	    PL_expect = PL_lex_expect;
	    PL_lex_defer = LEX_NORMAL;
	}
	return(PL_nexttype[PL_nexttoke]);

    /* interpolated case modifiers like \L \U, including \Q and \E.
       when we get here, PL_bufptr is at the \
    */
    case LEX_INTERPCASEMOD:
#ifdef DEBUGGING
	if (PL_bufptr != PL_bufend && *PL_bufptr != '\\')
	    croak("panic: INTERPCASEMOD");
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
		croak("panic: yylex");
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
	/* FALLTHROUGH */
    case LEX_INTERPCONCAT:
#ifdef DEBUGGING
	if (PL_lex_brackets)
	    croak("panic: INTERPCONCAT");
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
    DEBUG_p( {
	PerlIO_printf(PerlIO_stderr(), "### Tokener expecting %s at %s\n", exp_name[PL_expect], s);
    } )

  retry:
    switch (*s) {
    default:
	croak("Unrecognized character \\%03o", *s & 255);
    case 4:
    case 26:
	goto fake_eof;			/* emulate EOF on ^D or ^Z */
    case 0:
	if (!PL_rsfp) {
	    PL_last_uni = 0;
	    PL_last_lop = 0;
	    if (PL_lex_brackets)
		yyerror("Missing right bracket");
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
			    sv_catpvf(PL_linestr, "@F=split(%s);", PL_splitstr);
			else {
			    char delim;
			    s = "'~#\200\1'"; /* surely one char is unused...*/
			    while (s[1] && strchr(PL_splitstr, *s))  s++;
			    delim = *s;
			    sv_catpvf(PL_linestr, "@F=split(%s%c",
				      "q" + (delim == '\''), delim);
			    for (s = PL_splitstr; *s; s++) {
				if (*s == '\\')
				    sv_catpvn(PL_linestr, "\\", 1);
				sv_catpvn(PL_linestr, s, 1);
			    }
			    sv_catpvf(PL_linestr, "%c);", delim);
			}
		    }
		    else
		        sv_catpv(PL_linestr,"@F=split(' ');");
		}
	    }
	    sv_catpv(PL_linestr, "\n");
	    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
	    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	    if (PERLDB_LINE && PL_curstash != PL_debstash) {
		SV *sv = NEWSV(85,0);

		sv_upgrade(sv, SVt_PVMG);
		sv_setsv(sv,PL_linestr);
		av_store(GvAV(PL_curcop->cop_filegv),(I32)PL_curcop->cop_line,sv);
	    }
	    goto retry;
	}
	do {
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
		    PL_minus_n = PL_minus_p = 0;
		    goto retry;
		}
		PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
		sv_setpv(PL_linestr,"");
		TOKEN(';');	/* not infinite loop because rsfp is NULL now */
	    }
	    if (PL_doextract) {
		if (*s == '#' && s[1] == '!' && instr(s,"perl"))
		    PL_doextract = FALSE;

		/* Incest with pod. */
		if (*s == '=' && strnEQ(s, "=cut", 4)) {
		    sv_setpv(PL_linestr, "");
		    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
		    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
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
	    av_store(GvAV(PL_curcop->cop_filegv),(I32)PL_curcop->cop_line,sv);
	}
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	if (PL_curcop->cop_line == 1) {
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
		    if (sv_eq(x, GvSV(PL_curcop->cop_filegv))) {
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
		if (!d)
		    d = instr(s,"perl");
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
		    PerlProc_execv(ipath, newargv);
		    croak("Can't exec %s", ipath);
		}
		if (d) {
		    U32 oldpdb = PL_perldb;
		    bool oldn = PL_minus_n;
		    bool oldp = PL_minus_p;

		    while (*d && !isSPACE(*d)) d++;
		    while (*d == ' ' || *d == '\t') d++;

		    if (*d++ == '-') {
			do {
			    if (*d == 'M' || *d == 'm') {
				char *m = d;
				while (*d && !isSPACE(*d)) d++;
				croak("Too late for \"-%.*s\" option",
				      (int)(d - m), m);
			    }
			    d = moreswitches(d);
			} while (d);
			if (PERLDB_LINE && !oldpdb ||
			    ( PL_minus_n || PL_minus_p ) && !(oldn || oldp) )
			      /* if we have already added "LINE: while (<>) {",
			         we must not do it again */
			{
			    sv_setpv(PL_linestr, "");
			    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
			    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
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
	warn("Illegal character \\%03o (carriage return)", '\r');
	croak(
      "(Maybe you didn't strip carriage returns after a network transfer?)\n");
#endif
    case ' ': case '\t': case '\f': case 013:
	s++;
	goto retry;
    case '#':
    case '\n':
	if (PL_lex_state != LEX_NORMAL || (PL_in_eval && !PL_rsfp)) {
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
	    s++;
	    PL_bufptr = s;
	    tmp = *s++;

	    while (s < PL_bufend && (*s == ' ' || *s == '\t'))
		s++;

	    if (strnEQ(s,"=>",2)) {
		s = force_word(PL_bufptr,WORD,FALSE,FALSE,FALSE);
		OPERATOR('-');		/* unary minus */
	    }
	    PL_last_uni = PL_oldbufptr;
	    PL_last_lop_op = OP_FTEREAD;	/* good enough */
	    switch (tmp) {
	    case 'r': FTST(OP_FTEREAD);
	    case 'w': FTST(OP_FTEWRITE);
	    case 'x': FTST(OP_FTEEXEC);
	    case 'o': FTST(OP_FTEOWNED);
	    case 'R': FTST(OP_FTRREAD);
	    case 'W': FTST(OP_FTRWRITE);
	    case 'X': FTST(OP_FTREXEC);
	    case 'O': FTST(OP_FTROWNED);
	    case 'e': FTST(OP_FTIS);
	    case 'z': FTST(OP_FTZERO);
	    case 's': FTST(OP_FTSIZE);
	    case 'f': FTST(OP_FTFILE);
	    case 'd': FTST(OP_FTDIR);
	    case 'l': FTST(OP_FTLINK);
	    case 'p': FTST(OP_FTPIPE);
	    case 'S': FTST(OP_FTSOCK);
	    case 'u': FTST(OP_FTSUID);
	    case 'g': FTST(OP_FTSGID);
	    case 'k': FTST(OP_FTSVTX);
	    case 'b': FTST(OP_FTBLK);
	    case 'c': FTST(OP_FTCHR);
	    case 't': FTST(OP_FTTTY);
	    case 'T': FTST(OP_FTTEXT);
	    case 'B': FTST(OP_FTBINARY);
	    case 'M': gv_fetchpv("\024",TRUE, SVt_PV); FTST(OP_FTMTIME);
	    case 'A': gv_fetchpv("\024",TRUE, SVt_PV); FTST(OP_FTATIME);
	    case 'C': gv_fetchpv("\024",TRUE, SVt_PV); FTST(OP_FTCTIME);
	    default:
		croak("Unrecognized file test: -%c", (int)tmp);
		break;
	    }
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
	    if (isIDFIRST(*s)) {
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
	OPERATOR(':');
    case '(':
	s++;
	if (PL_last_lop == PL_oldoldbufptr || PL_last_uni == PL_oldoldbufptr)
	    PL_oldbufptr = PL_oldoldbufptr;		/* allow print(STDOUT 123) */
	else
	    PL_expect = XTERM;
	TOKEN('(');
    case ';':
	if (PL_curcop->cop_line < PL_copline)
	    PL_copline = PL_curcop->cop_line;
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
	    yyerror("Unmatched right bracket");
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
	    while (s < PL_bufend && (*s == ' ' || *s == '\t'))
		s++;
	    d = s;
	    PL_tokenbuf[0] = '\0';
	    if (d < PL_bufend && *d == '-') {
		PL_tokenbuf[0] = '-';
		d++;
		while (d < PL_bufend && (*d == ' ' || *d == '\t'))
		    d++;
	    }
	    if (d < PL_bufend && isIDFIRST(*d)) {
		d = scan_word(d, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1,
			      FALSE, &len);
		while (d < PL_bufend && (*d == ' ' || *d == '\t'))
		    d++;
		if (*d == '}') {
		    char minus = (PL_tokenbuf[0] == '-');
		    s = force_word(s + minus, WORD, FALSE, TRUE, FALSE);
		    if (minus)
			force_next('-');
		}
	    }
	    /* FALL THROUGH */
	case XBLOCK:
	    PL_lex_brackstack[PL_lex_brackets++] = XSTATE;
	    PL_expect = XSTATE;
	    break;
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
				&& !isALNUM(*t)))) {
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
		else if (isALPHA(*s)) {
		    for (t++; t < PL_bufend && isALNUM(*t); t++) ;
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
	yylval.ival = PL_curcop->cop_line;
	if (isSPACE(*s) || *s == '#')
	    PL_copline = NOLINE;   /* invalidate current command line number */
	TOKEN('{');
    case '}':
      rightbracket:
	s++;
	if (PL_lex_brackets <= 0)
	    yyerror("Unmatched right bracket");
	else
	    PL_expect = (expectation)PL_lex_brackstack[--PL_lex_brackets];
	if (PL_lex_brackets < PL_lex_formbrack)
	    PL_lex_formbrack = 0;
	if (PL_lex_state == LEX_INTERPNORMAL) {
	    if (PL_lex_brackets == 0) {
		if (PL_lex_fakebrack) {
		    PL_lex_state = LEX_INTERPEND;
		    PL_bufptr = s;
		    return yylex();		/* ignore fake brackets */
		}
		if (*s == '-' && s[1] == '>')
		    PL_lex_state = LEX_INTERPENDMAYBE;
		else if (*s != '[' && *s != '{')
		    PL_lex_state = LEX_INTERPEND;
	    }
	}
	if (PL_lex_brackets < PL_lex_fakebrack) {
	    PL_bufptr = s;
	    PL_lex_fakebrack = 0;
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
	    if (PL_dowarn && isALPHA(*s) && PL_bufptr == PL_linestart) {
		PL_curcop->cop_line--;
		warn(warn_nosemi);
		PL_curcop->cop_line++;
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
	if (PL_dowarn && tmp && isSPACE(*s) && strchr("+-*/%.^&|<",tmp))
	    warn("Reversed %c= operator",(int)tmp);
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
	    for (t = s; *t == ' ' || *t == '\t'; t++) ;
#else
	    for (t = s; *t == ' ' || *t == '\t' || *t == '\r'; t++) ;
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

	if (s[1] == '#' && (isALPHA(s[2]) || strchr("_{$:", s[2]))) {
	    if (PL_expect == XOPERATOR)
		no_op("Array length", PL_bufptr);
	    PL_tokenbuf[0] = '@';
	    s = scan_ident(s + 1, PL_bufend, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1,
			   FALSE);
	    if (!PL_tokenbuf[1])
		PREREF(DOLSHARP);
	    PL_expect = XOPERATOR;
	    PL_pending_ident = '#';
	    TOKEN(DOLSHARP);
	}

	if (PL_expect == XOPERATOR)
	    no_op("Scalar", PL_bufptr);
	PL_tokenbuf[0] = '$';
	s = scan_ident(s, PL_bufend, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1, FALSE);
	if (!PL_tokenbuf[1]) {
	    if (s == PL_bufend)
		yyerror("Final $ should be \\$ or $name");
	    PREREF('$');
	}

	/* This kludge not intended to be bulletproof. */
	if (PL_tokenbuf[1] == '[' && !PL_tokenbuf[2]) {
	    yylval.opval = newSVOP(OP_CONST, 0,
				   newSViv((IV)PL_compiling.cop_arybase));
	    yylval.opval->op_private = OPpCONST_ARYBASE;
	    TERM(THING);
	}

	d = s;
	if (PL_lex_state == LEX_NORMAL)
	    s = skipspace(s);

	if ((PL_expect != XREF || PL_oldoldbufptr == PL_last_lop) && intuit_more(s)) {
	    char *t;
	    if (*s == '[') {
		PL_tokenbuf[0] = '@';
		if (PL_dowarn) {
		    for(t = s + 1;
			isSPACE(*t) || isALNUM(*t) || *t == '$';
			t++) ;
		    if (*t++ == ',') {
			PL_bufptr = skipspace(PL_bufptr);
			while (t < PL_bufend && *t != ']')
			    t++;
			warn("Multidimensional syntax %.*s not supported",
			     (t - PL_bufptr) + 1, PL_bufptr);
		    }
		}
	    }
	    else if (*s == '{') {
		PL_tokenbuf[0] = '%';
		if (PL_dowarn && strEQ(PL_tokenbuf+1, "SIG") &&
		    (t = strchr(s, '}')) && (t = strchr(t, '=')))
		{
		    char tmpbuf[sizeof PL_tokenbuf];
		    STRLEN len;
		    for (t++; isSPACE(*t); t++) ;
		    if (isIDFIRST(*t)) {
			t = scan_word(t, tmpbuf, sizeof tmpbuf, TRUE, &len);
		        for (; isSPACE(*t); t++) ;
			if (*t == ';' && perl_get_cv(tmpbuf, FALSE))
			    warn("You need to quote \"%s\"", tmpbuf);
		    }
		}
	    }
	}

	PL_expect = XOPERATOR;
	if (PL_lex_state == LEX_NORMAL && isSPACE(*d)) {
	    bool islop = (PL_last_lop == PL_oldoldbufptr);
	    if (!islop || PL_last_lop_op == OP_GREPSTART)
		PL_expect = XOPERATOR;
	    else if (strchr("$@\"'`q", *s))
		PL_expect = XTERM;		/* e.g. print $fh "foo" */
	    else if (strchr("&*<%", *s) && isIDFIRST(s[1]))
		PL_expect = XTERM;		/* e.g. print $fh &sub */
	    else if (isIDFIRST(*s)) {
		char tmpbuf[sizeof PL_tokenbuf];
		scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
		if (tmp = keyword(tmpbuf, len)) {
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
	    if (PL_dowarn) {
		if (*s == '[' || *s == '{') {
		    char *t = s + 1;
		    while (*t && (isALNUM(*t) || strchr(" \t$#+-'\"", *t)))
			t++;
		    if (*t == '}' || *t == ']') {
			t++;
			PL_bufptr = skipspace(PL_bufptr);
			warn("Scalar value %.*s better written as $%.*s",
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
		    || memNE(PL_last_uni, "study", 5) || isALNUM(PL_last_uni[5])))
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
	s = scan_num(s);
	if (PL_expect == XOPERATOR)
	    no_op("Number",s);
	TERM(THING);

    case '\'':
	s = scan_str(s);
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
	s = scan_str(s);
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
	    if (*d == '$' || *d == '@' || *d == '\\') {
		yylval.ival = OP_STRINGIFY;
		break;
	    }
	}
	TERM(sublex_start());

    case '`':
	s = scan_str(s);
	if (PL_expect == XOPERATOR)
	    no_op("Backticks",s);
	if (!s)
	    missingterm((char*)0);
	yylval.ival = OP_BACKTICK;
	set_csh();
	TERM(sublex_start());

    case '\\':
	s++;
	if (PL_dowarn && PL_lex_inwhat && isDIGIT(*s))
	    warn("Can't use \\%c to mean $%c in expression", *s, *s);
	if (PL_expect == XOPERATOR)
	    no_op("Backslash",s);
	OPERATOR(REFGEN);

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
    case 'v': case 'V':
    case 'w': case 'W':
	      case 'X':
    case 'y': case 'Y':
    case 'z': case 'Z':

      keylookup: {
	STRLEN n_a;
	gv = Nullgv;
	gvp = 0;

	PL_bufptr = s;
	s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE, &len);

	/* Some keywords can be followed by any delimiter, including ':' */
	tmp = (len == 1 && strchr("msyq", PL_tokenbuf[0]) ||
	       len == 2 && ((PL_tokenbuf[0] == 't' && PL_tokenbuf[1] == 'r') ||
			    (PL_tokenbuf[0] == 'q' &&
			     strchr("qwxr", PL_tokenbuf[1]))));

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
	if (strnEQ(d,"=>",2)) {
	    CLINE;
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0, newSVpv(PL_tokenbuf,0));
	    yylval.opval->op_private = OPpCONST_BARE;
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
		     && !hv_fetch(GvHVn(PL_incgv), "Thread.pm", 9, FALSE))
	    {
		tmp = 0;		/* any sub overrides "weak" keyword */
	    }
	    else {			/* no override */
		tmp = -tmp;
		gv = Nullgv;
		gvp = 0;
		if (PL_dowarn && hgv
		    && tmp != KEY_x && tmp != KEY_CORE) /* never ambiguous */
		    warn("Ambiguous call resolved as CORE::%s(), %s",
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

		if (*s == '\'' || *s == ':' && s[1] == ':') {
		    STRLEN morelen;
		    s = scan_word(s, PL_tokenbuf + len, sizeof PL_tokenbuf - len,
				  TRUE, &morelen);
		    if (!morelen)
			croak("Bad name after %s%s", PL_tokenbuf,
				*s == '\'' ? "'" : "::");
		    len += morelen;
		}

		if (PL_expect == XOPERATOR) {
		    if (PL_bufptr == PL_linestart) {
			PL_curcop->cop_line--;
			warn(warn_nosemi);
			PL_curcop->cop_line++;
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
		    if (PL_dowarn && ! gv_fetchpv(PL_tokenbuf, FALSE, SVt_PVHV))
			warn("Bareword \"%s\" refers to nonexistent package",
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
		    sv = newSVpv("CORE::GLOBAL::",14);
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
		    (PL_oldoldbufptr == PL_last_lop || PL_oldoldbufptr == PL_last_uni) &&
		    /* NO SKIPSPACE BEFORE HERE! */
		    (PL_expect == XREF 
		     || ((opargs[PL_last_lop_op] >> OASHIFT)& 7) == OA_FILEREF
		     || (PL_last_lop_op == OP_ENTERSUB 
			 && PL_last_proto 
			 && PL_last_proto[PL_last_proto[0] == ';' ? 1 : 0] == '*')) )
		{
		    bool immediate_paren = *s == '(';

		    /* (Now we can afford to cross potential line boundary.) */
		    s = skipspace(s);

		    /* Two barewords in a row may indicate method call. */

		    if ((isALPHA(*s) || *s == '$') && (tmp=intuit_method(s,gv)))
			return tmp;

		    /* If not a declared subroutine, it's an indirect object. */
		    /* (But it's an indir obj regardless for sort.) */

		    if ((PL_last_lop_op == OP_SORT ||
                         (!immediate_paren && (!gv || !GvCVu(gv))) ) &&
                        (PL_last_lop_op != OP_MAPSTART && PL_last_lop_op != OP_GREPSTART)){
			PL_expect = (PL_last_lop == PL_oldoldbufptr) ? XTERM : XOPERATOR;
			goto bareword;
		    }
		}

		/* If followed by a paren, it's certainly a subroutine. */

		PL_expect = XOPERATOR;
		s = skipspace(s);
		if (*s == '(') {
		    CLINE;
		    if (gv && GvCVu(gv)) {
			CV *cv;
			if ((cv = GvCV(gv)) && SvPOK(cv))
			    PL_last_proto = SvPV((SV*)cv, n_a);
			for (d = s + 1; *d == ' ' || *d == '\t'; d++) ;
			if (*d == ')' && (sv = cv_const_sv(cv))) {
			    s = d + 1;
			    goto its_constant;
			}
		    }
		    PL_nextval[PL_nexttoke].opval = yylval.opval;
		    PL_expect = XOPERATOR;
		    force_next(WORD);
		    yylval.ival = 0;
		    PL_last_lop_op = OP_ENTERSUB;
		    TOKEN('&');
		}

		/* If followed by var or block, call it a method (unless sub) */

		if ((*s == '$' || *s == '{') && (!gv || !GvCVu(gv))) {
		    PL_last_lop = PL_oldbufptr;
		    PL_last_lop_op = OP_METHOD;
		    PREBLOCK(METHOD);
		}

		/* If followed by a bareword, see if it looks like indir obj. */

		if ((isALPHA(*s) || *s == '$') && (tmp = intuit_method(s,gv)))
		    return tmp;

		/* Not a method, so call it a subroutine (if defined) */

		if (gv && GvCVu(gv)) {
		    CV* cv;
		    if (lastchar == '-')
			warn("Ambiguous use of -%s resolved as -&%s()",
				PL_tokenbuf, PL_tokenbuf);
		    PL_last_lop = PL_oldbufptr;
		    PL_last_lop_op = OP_ENTERSUB;
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
		    PL_last_lop_op = OP_ENTERSUB;
		    /* Is there a prototype? */
		    if (SvPOK(cv)) {
			STRLEN len;
			PL_last_proto = SvPV((SV*)cv, len);
			if (!len)
			    TERM(FUNC0SUB);
			if (strEQ(PL_last_proto, "$"))
			    OPERATOR(UNIOPSUB);
			if (*PL_last_proto == '&' && *s == '{') {
			    sv_setpv(PL_subname,"__ANON__");
			    PREBLOCK(LSTOPSUB);
			}
		    } else
			PL_last_proto = NULL;
		    PL_nextval[PL_nexttoke].opval = yylval.opval;
		    PL_expect = XTERM;
		    force_next(WORD);
		    TOKEN(NOAMP);
		}

		if (PL_hints & HINT_STRICT_SUBS &&
		    lastchar != '-' &&
		    strnNE(s,"->",2) &&
		    PL_last_lop_op != OP_TRUNCATE &&  /* S/F prototype in opcode.pl */
		    PL_last_lop_op != OP_ACCEPT &&
		    PL_last_lop_op != OP_PIPE_OP &&
		    PL_last_lop_op != OP_SOCKPAIR &&
		    !(PL_last_lop_op == OP_ENTERSUB 
			 && PL_last_proto 
			 && PL_last_proto[PL_last_proto[0] == ';' ? 1 : 0] == '*'))
		{
		    warn(
		     "Bareword \"%s\" not allowed while \"strict subs\" in use",
			PL_tokenbuf);
		    ++PL_error_count;
		}

		/* Call it a bare word */

	    bareword:
		if (PL_dowarn) {
		    if (lastchar != '-') {
			for (d = PL_tokenbuf; *d && isLOWER(*d); d++) ;
			if (!*d)
			    warn(warn_reserved, PL_tokenbuf);
		    }
		}

	    safe_bareword:
		if (lastchar && strchr("*%&", lastchar)) {
		    warn("Operator or semicolon missing before %c%s",
			lastchar, PL_tokenbuf);
		    warn("Ambiguous use of %c resolved as operator %c",
			lastchar, lastchar);
		}
		TOKEN(WORD);
	    }

	case KEY___FILE__:
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0,
					newSVsv(GvSV(PL_curcop->cop_filegv)));
	    TERM(THING);

	case KEY___LINE__:
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0,
				    newSVpvf("%ld", (long)PL_curcop->cop_line));
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
		gv = gv_fetchpv(form("%s::DATA", pname), TRUE, SVt_PVIO);
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
		    IoTYPE(GvIOp(gv)) = '|';
		else if ((PerlIO*)PL_rsfp == PerlIO_stdin())
		    IoTYPE(GvIOp(gv)) = '-';
		else
		    IoTYPE(GvIOp(gv)) = '<';
		PL_rsfp = Nullfp;
	    }
	    goto fake_eof;
	}

	case KEY_AUTOLOAD:
	case KEY_DESTROY:
	case KEY_BEGIN:
	case KEY_END:
	case KEY_INIT:
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
		tmp = keyword(PL_tokenbuf, len);
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
	    UNI(OP_BINMODE);

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
	    if (!PL_cryptseen++)
		init_des();
#endif
	    LOP(OP_CRYPT,XTERM);

	case KEY_chmod:
	    if (PL_dowarn) {
		for (d = s; d < PL_bufend && (isSPACE(*d) || *d == '('); d++) ;
		if (*d != '0' && isDIGIT(*d))
		    yywarn("chmod: mode argument is missing initial 0");
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
	    yylval.ival = PL_curcop->cop_line;
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
	    yylval.ival = PL_curcop->cop_line;
	    s = skipspace(s);
	    if (PL_expect == XSTATE && isIDFIRST(*s)) {
		char *p = s;
		if ((PL_bufend - p) >= 3 &&
		    strnEQ(p, "my", 2) && isSPACE(*(p + 2)))
		    p += 2;
		p = skipspace(p);
		if (isIDFIRST(*p))
		    croak("Missing $ on loop variable");
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
	    LOP(OP_GREPSTART, *s == '(' ? XTERM : XREF);

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
	    yylval.ival = PL_curcop->cop_line;
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
	    LOP(OP_MAPSTART,XREF);
	    
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

	case KEY_my:
	    PL_in_my = TRUE;
	    s = skipspace(s);
	    if (isIDFIRST(*s)) {
		s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, TRUE, &len);
		PL_in_my_stash = gv_stashpv(PL_tokenbuf, FALSE);
		if (!PL_in_my_stash) {
		    char tmpbuf[1024];
		    PL_bufptr = s;
		    sprintf(tmpbuf, "No such class %.1000s", PL_tokenbuf);
		    yyerror(tmpbuf);
		}
	    }
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
	    OPERATOR(NOTOP);

	case KEY_open:
	    s = skipspace(s);
	    if (isIDFIRST(*s)) {
		char *t;
		for (d = s; isALNUM(*d); d++) ;
		t = skipspace(d);
		if (strchr("|&*+-=!?:.", *t))
		    warn("Precedence problem: open %.*s should be open(%.*s)",
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
	    s = scan_str(s);
	    if (!s)
		missingterm((char*)0);
	    yylval.ival = OP_CONST;
	    TERM(sublex_start());

	case KEY_quotemeta:
	    UNI(OP_QUOTEMETA);

	case KEY_qw:
	    s = scan_str(s);
	    if (!s)
		missingterm((char*)0);
	    if (PL_dowarn && SvLEN(PL_lex_stuff)) {
		d = SvPV_force(PL_lex_stuff, len);
		for (; len; --len, ++d) {
		    if (*d == ',') {
			warn("Possible attempt to separate words with commas");
			break;
		    }
		    if (*d == '#') {
			warn("Possible attempt to put comments in qw() list");
			break;
		    }
		}
	    }
	    force_next(')');
	    PL_nextval[PL_nexttoke].opval = (OP*)newSVOP(OP_CONST, 0, tokeq(PL_lex_stuff));
	    PL_lex_stuff = Nullsv;
	    force_next(THING);
	    force_next(',');
	    PL_nextval[PL_nexttoke].opval = (OP*)newSVOP(OP_CONST, 0, newSVpv(" ",1));
	    force_next(THING);
	    force_next('(');
	    yylval.ival = OP_SPLIT;
	    CLINE;
	    PL_expect = XTERM;
	    PL_bufptr = s;
	    PL_last_lop = PL_oldbufptr;
	    PL_last_lop_op = OP_SPLIT;
	    return FUNC;

	case KEY_qq:
	    s = scan_str(s);
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
	    s = scan_str(s);
	    if (!s)
		missingterm((char*)0);
	    yylval.ival = OP_BACKTICK;
	    set_csh();
	    TERM(sublex_start());

	case KEY_return:
	    OLDLOP(OP_RETURN);

	case KEY_require:
	    *PL_tokenbuf = '\0';
	    s = force_word(s,WORD,TRUE,TRUE,FALSE);
	    if (isIDFIRST(*PL_tokenbuf))
		gv_stashpvn(PL_tokenbuf, strlen(PL_tokenbuf), TRUE);
	    else if (*s == '<')
		yyerror("<> should be quotes");
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
		croak("sort is now a reserved word");
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
	    PL_sawstudy++;
	    UNI(OP_STUDY);

	case KEY_substr:
	    LOP(OP_SUBSTR,XTERM);

	case KEY_format:
	case KEY_sub:
	  really_sub:
	    s = skipspace(s);

	    if (isIDFIRST(*s) || *s == '\'' || *s == ':') {
		char tmpbuf[sizeof PL_tokenbuf];
		PL_expect = XBLOCK;
		d = scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
		if (strchr(tmpbuf, ':'))
		    sv_setpv(PL_subname, tmpbuf);
		else {
		    sv_setsv(PL_subname,PL_curstname);
		    sv_catpvn(PL_subname,"::",2);
		    sv_catpvn(PL_subname,tmpbuf,len);
		}
		s = force_word(s,WORD,FALSE,TRUE,TRUE);
		s = skipspace(s);
	    }
	    else {
		PL_expect = XTERMBLOCK;
		sv_setpv(PL_subname,"?");
	    }

	    if (tmp == KEY_format) {
		s = skipspace(s);
		if (*s == '=')
		    PL_lex_formbrack = PL_lex_brackets + 1;
		OPERATOR(FORMAT);
	    }

	    /* Look for a prototype */
	    if (*s == '(') {
		char *p;

		s = scan_str(s);
		if (!s) {
		    if (PL_lex_stuff)
			SvREFCNT_dec(PL_lex_stuff);
		    PL_lex_stuff = Nullsv;
		    croak("Prototype not terminated");
		}
		/* strip spaces */
		d = SvPVX(PL_lex_stuff);
		tmp = 0;
		for (p = d; *p; ++p) {
		    if (!isSPACE(*p))
			d[tmp++] = *p;
		}
		d[tmp] = '\0';
		SvCUR(PL_lex_stuff) = tmp;

		PL_nexttoke++;
		PL_nextval[1] = PL_nextval[0];
		PL_nexttype[1] = PL_nexttype[0];
		PL_nextval[0].opval = (OP*)newSVOP(OP_CONST, 0, PL_lex_stuff);
		PL_nexttype[0] = THING;
		if (PL_nexttoke == 1) {
		    PL_lex_defer = PL_lex_state;
		    PL_lex_expect = PL_expect;
		    PL_lex_state = LEX_KNOWNEXT;
		}
		PL_lex_stuff = Nullsv;
	    }

	    if (*SvPV(PL_subname,n_a) == '?') {
		sv_setpv(PL_subname,"__ANON__");
		TOKEN(ANONSUB);
	    }
	    PREBLOCK(SUB);

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
	    yylval.ival = PL_curcop->cop_line;
	    OPERATOR(UNTIL);

	case KEY_unless:
	    yylval.ival = PL_curcop->cop_line;
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
	    if (PL_dowarn) {
		for (d = s; d < PL_bufend && (isSPACE(*d) || *d == '('); d++) ;
		if (*d != '0' && isDIGIT(*d))
		    yywarn("umask: argument is missing initial 0");
	    }
	    UNI(OP_UMASK);

	case KEY_unshift:
	    LOP(OP_UNSHIFT,XTERM);

	case KEY_use:
	    if (PL_expect != XSTATE)
		yyerror("\"use\" not allowed in expression");
	    s = skipspace(s);
	    if(isDIGIT(*s)) {
		s = force_version(s);
		if(*s == ';' || (s = skipspace(s), *s == ';')) {
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
	    PL_sawvec = TRUE;
	    LOP(OP_VEC,XTERM);

	case KEY_while:
	    yylval.ival = PL_curcop->cop_line;
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

I32
keyword(register char *d, I32 len)
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
	break;
    case 'c':
	switch (len) {
	case 3:
	    if (strEQ(d,"cmp"))			return -KEY_cmp;
	    if (strEQ(d,"chr"))			return -KEY_chr;
	    if (strEQ(d,"cos"))			return -KEY_cos;
	    break;
	case 4:
	    if (strEQ(d,"chop"))		return KEY_chop;
	    break;
	case 5:
	    if (strEQ(d,"close"))		return -KEY_close;
	    if (strEQ(d,"chdir"))		return -KEY_chdir;
	    if (strEQ(d,"chomp"))		return KEY_chomp;
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
	    if (strEQ(d,"each"))		return KEY_each;
	    break;
	case 5:
	    if (strEQ(d,"elsif"))		return KEY_elsif;
	    break;
	case 6:
	    if (strEQ(d,"exists"))		return KEY_exists;
	    if (strEQ(d,"elseif")) warn("elseif should be elsif");
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
	    if (strEQ(d,"keys"))		return KEY_keys;
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
	    if (strEQ(d,"our")) { deprecate("reserved word \"our\"");
						return 0;}
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
	    if (strEQ(d,"pop"))			return KEY_pop;
	    if (strEQ(d,"pos"))			return KEY_pos;
	    break;
	case 4:
	    if (strEQ(d,"push"))		return KEY_push;
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
		if (strEQ(d,"shift"))		return KEY_shift;
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
	    if (strEQ(d,"splice"))		return KEY_splice;
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
	    if (strEQ(d,"unshift"))		return KEY_unshift;
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
checkcomma(register char *s, char *name, char *what)
{
    char *w;

    if (PL_dowarn && *s == ' ' && s[1] == '(') {	/* XXX gotta be a better way */
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
	    warn("%s (...) interpreted as function",name);
    }
    while (s < PL_bufend && isSPACE(*s))
	s++;
    if (*s == '(')
	s++;
    while (s < PL_bufend && isSPACE(*s))
	s++;
    if (isIDFIRST(*s)) {
	w = s++;
	while (isALNUM(*s))
	    s++;
	while (s < PL_bufend && isSPACE(*s))
	    s++;
	if (*s == ',') {
	    int kw;
	    *s = '\0';
	    kw = keyword(w, s - w) || perl_get_cv(w, FALSE) != 0;
	    *s = ',';
	    if (kw)
		return;
	    croak("No comma allowed after %s", what);
	}
    }
}

STATIC SV *
new_constant(char *s, STRLEN len, char *key, SV *sv, SV *pv, char *type) 
{
    dSP;
    HV *table = GvHV(PL_hintgv);		 /* ^H */
    BINOP myop;
    SV *res;
    bool oldcatch = CATCH_GET;
    SV **cvp;
    SV *cv, *typesv;
    char buf[128];
	    
    if (!table) {
	yyerror("%^H is not defined");
	return sv;
    }
    cvp = hv_fetch(table, key, strlen(key), FALSE);
    if (!cvp || !SvOK(*cvp)) {
	sprintf(buf,"$^H{%s} is not defined", key);
	yyerror(buf);
	return sv;
    }
    sv_2mortal(sv);			/* Parent created it permanently */
    cv = *cvp;
    if (!pv)
	pv = sv_2mortal(newSVpv(s, len));
    if (type)
	typesv = sv_2mortal(newSVpv(type, 0));
    else
	typesv = &PL_sv_undef;
    CATCH_SET(TRUE);
    Zero(&myop, 1, BINOP);
    myop.op_last = (OP *) &myop;
    myop.op_next = Nullop;
    myop.op_flags = OPf_WANT_SCALAR | OPf_STACKED;

    PUSHSTACKi(PERLSI_OVERLOAD);
    ENTER;
    SAVEOP();
    PL_op = (OP *) &myop;
    if (PERLDB_SUB && PL_curstash != PL_debstash)
	PL_op->op_private |= OPpENTERSUB_DB;
    PUTBACK;
    pp_pushmark(ARGS);

    EXTEND(sp, 4);
    PUSHs(pv);
    PUSHs(sv);
    PUSHs(typesv);
    PUSHs(cv);
    PUTBACK;

    if (PL_op = pp_entersub(ARGS))
      CALLRUNOPS();
    LEAVE;
    SPAGAIN;

    res = POPs;
    PUTBACK;
    CATCH_SET(oldcatch);
    POPSTACK;

    if (!SvOK(res)) {
	sprintf(buf,"Call to &{$^H{%s}} did not return a defined value", key);
	yyerror(buf);
    }
    return SvREFCNT_inc(res);
}

STATIC char *
scan_word(register char *s, char *dest, STRLEN destlen, int allow_package, STRLEN *slp)
{
    register char *d = dest;
    register char *e = d + destlen - 3;  /* two-character token, ending NUL */
    for (;;) {
	if (d >= e)
	    croak(ident_too_long);
	if (isALNUM(*s))
	    *d++ = *s++;
	else if (*s == '\'' && allow_package && isIDFIRST(s[1])) {
	    *d++ = ':';
	    *d++ = ':';
	    s++;
	}
	else if (*s == ':' && s[1] == ':' && allow_package && s[2] != '$') {
	    *d++ = *s++;
	    *d++ = *s++;
	}
	else {
	    *d = '\0';
	    *slp = d - dest;
	    return s;
	}
    }
}

STATIC char *
scan_ident(register char *s, register char *send, char *dest, STRLEN destlen, I32 ck_uni)
{
    register char *d;
    register char *e;
    char *bracket = 0;
    char funny = *s++;

    if (PL_lex_brackets == 0)
	PL_lex_fakebrack = 0;
    if (isSPACE(*s))
	s = skipspace(s);
    d = dest;
    e = d + destlen - 3;	/* two-character token, ending NUL */
    if (isDIGIT(*s)) {
	while (isDIGIT(*s)) {
	    if (d >= e)
		croak(ident_too_long);
	    *d++ = *s++;
	}
    }
    else {
	for (;;) {
	    if (d >= e)
		croak(ident_too_long);
	    if (isALNUM(*s))
		*d++ = *s++;
	    else if (*s == '\'' && isIDFIRST(s[1])) {
		*d++ = ':';
		*d++ = ':';
		s++;
	    }
	    else if (*s == ':' && s[1] == ':') {
		*d++ = *s++;
		*d++ = *s++;
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
      (isALNUM(s[1]) || strchr("${", s[1]) || strnEQ(s+1,"::",2)) )
    {
	if (isDIGIT(s[1]) && PL_lex_state == LEX_INTERPNORMAL)
	    deprecate("\"$$<digit>\" to mean \"${$}<digit>\"");
	else
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
    if (*d == '^' && *s && (isUPPER(*s) || strchr("[\\]^_?", *s))) {
	*d = toCTRL(*s);
	s++;
    }
    if (bracket) {
	if (isSPACE(s[-1])) {
	    while (s < send) {
		char ch = *s++;
		if (ch != ' ' && ch != '\t') {
		    *d = ch;
		    break;
		}
	    }
	}
	if (isIDFIRST(*d)) {
	    d++;
	    while (isALNUM(*s) || *s == ':')
		*d++ = *s++;
	    *d = '\0';
	    while (s < send && (*s == ' ' || *s == '\t')) s++;
	    if ((*s == '[' || (*s == '{' && strNE(dest, "sub")))) {
		if (PL_dowarn && keyword(dest, d - dest)) {
		    char *brack = *s == '[' ? "[...]" : "{...}";
		    warn("Ambiguous use of %c{%s%s} resolved to %c%s%s",
			funny, dest, brack, funny, dest, brack);
		}
		PL_lex_fakebrack = PL_lex_brackets+1;
		bracket++;
		PL_lex_brackstack[PL_lex_brackets++] = XOPERATOR;
		return s;
	    }
	}
	if (*s == '}') {
	    s++;
	    if (PL_lex_state == LEX_INTERPNORMAL && !PL_lex_brackets)
		PL_lex_state = LEX_INTERPEND;
	    if (funny == '#')
		funny = '@';
	    if (PL_dowarn && PL_lex_state == LEX_NORMAL &&
	      (keyword(dest, d - dest) || perl_get_cv(dest, FALSE)))
		warn("Ambiguous use of %c{%s} resolved to %c%s",
		    funny, dest, funny, dest);
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

void pmflag(U16 *pmfl, int ch)
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
scan_pat(char *start, I32 type)
{
    PMOP *pm;
    char *s;

    s = scan_str(start);
    if (!s) {
	if (PL_lex_stuff)
	    SvREFCNT_dec(PL_lex_stuff);
	PL_lex_stuff = Nullsv;
	croak("Search pattern not terminated");
    }

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
scan_subst(char *start)
{
    register char *s;
    register PMOP *pm;
    I32 first_start;
    I32 es = 0;

    yylval.ival = OP_NULL;

    s = scan_str(start);

    if (!s) {
	if (PL_lex_stuff)
	    SvREFCNT_dec(PL_lex_stuff);
	PL_lex_stuff = Nullsv;
	croak("Substitution pattern not terminated");
    }

    if (s[-1] == PL_multi_open)
	s--;

    first_start = PL_multi_start;
    s = scan_str(s);
    if (!s) {
	if (PL_lex_stuff)
	    SvREFCNT_dec(PL_lex_stuff);
	PL_lex_stuff = Nullsv;
	if (PL_lex_repl)
	    SvREFCNT_dec(PL_lex_repl);
	PL_lex_repl = Nullsv;
	croak("Substitution replacement not terminated");
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
	PL_super_bufptr = s;
	PL_super_bufend = PL_bufend;
	PL_multi_end = 0;
	pm->op_pmflags |= PMf_EVAL;
	repl = newSVpv("",0);
	while (es-- > 0)
	    sv_catpv(repl, es ? "eval " : "do ");
	sv_catpvn(repl, "{ ", 2);
	sv_catsv(repl, PL_lex_repl);
	sv_catpvn(repl, " };", 2);
	SvCOMPILED_on(repl);
	SvREFCNT_dec(PL_lex_repl);
	PL_lex_repl = repl;
    }

    pm->op_pmpermflags = pm->op_pmflags;
    PL_lex_op = (OP*)pm;
    yylval.ival = OP_SUBST;
    return s;
}

STATIC char *
scan_trans(char *start)
{
    register char* s;
    OP *o;
    short *tbl;
    I32 squash;
    I32 Delete;
    I32 complement;

    yylval.ival = OP_NULL;

    s = scan_str(start);
    if (!s) {
	if (PL_lex_stuff)
	    SvREFCNT_dec(PL_lex_stuff);
	PL_lex_stuff = Nullsv;
	croak("Transliteration pattern not terminated");
    }
    if (s[-1] == PL_multi_open)
	s--;

    s = scan_str(s);
    if (!s) {
	if (PL_lex_stuff)
	    SvREFCNT_dec(PL_lex_stuff);
	PL_lex_stuff = Nullsv;
	if (PL_lex_repl)
	    SvREFCNT_dec(PL_lex_repl);
	PL_lex_repl = Nullsv;
	croak("Transliteration replacement not terminated");
    }

    New(803,tbl,256,short);
    o = newPVOP(OP_TRANS, 0, (char*)tbl);

    complement = Delete = squash = 0;
    while (*s == 'c' || *s == 'd' || *s == 's') {
	if (*s == 'c')
	    complement = OPpTRANS_COMPLEMENT;
	else if (*s == 'd')
	    Delete = OPpTRANS_DELETE;
	else
	    squash = OPpTRANS_SQUASH;
	s++;
    }
    o->op_private = Delete|squash|complement;

    PL_lex_op = o;
    yylval.ival = OP_TRANS;
    return s;
}

STATIC char *
scan_heredoc(register char *s)
{
    dTHR;
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
    for (peek = s; *peek == ' ' || *peek == '\t'; peek++) ;
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
	if (!isALNUM(*s))
	    deprecate("bare << to mean <<\"\"");
	for (; isALNUM(*s); s++) {
	    if (d < e)
		*d++ = *s;
	}
    }
    if (d >= PL_tokenbuf + sizeof PL_tokenbuf - 1)
	croak("Delimiter for here document is too long");
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
	herewas = newSVpv(s,PL_bufend-s);
    else
	s--, herewas = newSVpv(s,d-s);
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
    PL_multi_start = PL_curcop->cop_line;
    PL_multi_open = PL_multi_close = '<';
    term = *PL_tokenbuf;
    if (PL_lex_inwhat == OP_SUBST && PL_in_eval && !PL_rsfp) {
	char *bufptr = PL_super_bufptr;
	char *bufend = PL_super_bufend;
	char *olds = s - SvCUR(herewas);
	s = strchr(bufptr, '\n');
	if (!s)
	    s = bufend;
	d = s;
	while (s < bufend &&
	  (*s != term || memNE(s,PL_tokenbuf,len)) ) {
	    if (*s++ == '\n')
		PL_curcop->cop_line++;
	}
	if (s >= bufend) {
	    PL_curcop->cop_line = PL_multi_start;
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
		PL_curcop->cop_line++;
	}
	if (s >= PL_bufend) {
	    PL_curcop->cop_line = PL_multi_start;
	    missingterm(PL_tokenbuf);
	}
	sv_setpvn(tmpstr,d+1,s-d);
	s += len - 1;
	PL_curcop->cop_line++;	/* the preceding stmt passes a newline */

	sv_catpvn(herewas,s,PL_bufend-s);
	sv_setsv(PL_linestr,herewas);
	PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = s = PL_linestart = SvPVX(PL_linestr);
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
    }
    else
	sv_setpvn(tmpstr,"",0);   /* avoid "uninitialized" warning */
    while (s >= PL_bufend) {	/* multiple line string? */
	if (!outer ||
	 !(PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = filter_gets(PL_linestr, PL_rsfp, 0))) {
	    PL_curcop->cop_line = PL_multi_start;
	    missingterm(PL_tokenbuf);
	}
	PL_curcop->cop_line++;
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
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
	    av_store(GvAV(PL_curcop->cop_filegv),
	      (I32)PL_curcop->cop_line,sv);
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
    PL_multi_end = PL_curcop->cop_line;
    if (SvCUR(tmpstr) + 5 < SvLEN(tmpstr)) {
	SvLEN_set(tmpstr, SvCUR(tmpstr) + 1);
	Renew(SvPVX(tmpstr), SvLEN(tmpstr), char);
    }
    SvREFCNT_dec(herewas);
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
scan_inputsymbol(char *start)
{
    register char *s = start;		/* current position in buffer */
    register char *d;
    register char *e;
    I32 len;

    d = PL_tokenbuf;			/* start of temp holding space */
    e = PL_tokenbuf + sizeof PL_tokenbuf;	/* end of temp holding space */
    s = delimcpy(d, e, s + 1, PL_bufend, '>', &len);	/* extract until > */

    /* die if we didn't have space for the contents of the <>,
       or if it didn't end
    */

    if (len >= sizeof PL_tokenbuf)
	croak("Excessively long <> operator");
    if (s >= PL_bufend)
	croak("Unterminated <> operator");

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
    while (*d && (isALNUM(*d) || *d == '\'' || *d == ':'))
	d++;

    /* If we've tried to read what we allow filehandles to look like, and
       there's still text left, then it must be a glob() and not a getline.
       Use scan_str to pull out the stuff between the <> and treat it
       as nothing more than a string.
    */

    if (d - PL_tokenbuf != len) {
	yylval.ival = OP_GLOB;
	set_csh();
	s = scan_str(start);
	if (!s)
	   croak("Glob not terminated");
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
		PL_lex_op = (OP*)newUNOP(OP_READLINE, 0, newUNOP(OP_RV2GV, 0, o));
	    }
	    else {
		GV *gv = gv_fetchpv(d+1,TRUE, SVt_PV);
		PL_lex_op = (OP*)newUNOP(OP_READLINE, 0,
					newUNOP(OP_RV2GV, 0,
					    newUNOP(OP_RV2SV, 0,
						newGVOP(OP_GV, 0, gv))));
	    }
	    /* we created the ops in lex_op, so make yylval.ival a null op */
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

   The lexer always reads these strings into lex_stuff, except in the
   case of the operators which take *two* arguments (s/// and tr///)
   when it checks to see if lex_stuff is full (presumably with the 1st
   arg to s or tr) and if so puts the string into lex_repl.

*/

STATIC char *
scan_str(char *start)
{
    dTHR;
    SV *sv;				/* scalar value: string */
    char *tmps;				/* temp string, used for delimiter matching */
    register char *s = start;		/* current position in the buffer */
    register char term;			/* terminating character */
    register char *to;			/* current position in the sv's data */
    I32 brackets = 1;			/* bracket nesting level */

    /* skip space before the delimiter */
    if (isSPACE(*s))
	s = skipspace(s);

    /* mark where we are, in case we need to report errors */
    CLINE;

    /* after skipping whitespace, the next character is the terminator */
    term = *s;
    /* mark where we are */
    PL_multi_start = PL_curcop->cop_line;
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
		    PL_curcop->cop_line++;
		/* handle quoted delimiters */
		if (*s == '\\' && s+1 < PL_bufend && term != '\\') {
		    if (s[1] == term)
			s++;
		/* any other quotes are simply copied straight through */
		    else
			*to++ = *s++;
		}
		/* terminate when run out of buffer (the for() condition), or
		   have found the terminator */
		else if (*s == term)
		    break;
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
		    PL_curcop->cop_line++;
		/* backslashes can escape the open or closing characters */
		if (*s == '\\' && s+1 < PL_bufend) {
		    if ((s[1] == PL_multi_open) || (s[1] == PL_multi_close))
			s++;
		    else
			*to++ = *s++;
		}
		/* allow nested opens and closes */
		else if (*s == PL_multi_close && --brackets <= 0)
		    break;
		else if (*s == PL_multi_open)
		    brackets++;
		*to = *s;
	    }
	}
	/* terminate the copied string and update the sv's end-of-string */
	*to = '\0';
	SvCUR_set(sv, to - SvPVX(sv));

	/*
	 * this next chunk reads more into the buffer if we're not done yet
	 */

  	if (s < PL_bufend) break;	/* handle case where we are done yet :-) */

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
	    PL_curcop->cop_line = PL_multi_start;
	    return Nullch;
	}
	/* we read a line, so increment our line counter */
	PL_curcop->cop_line++;
	
	/* update debugger info */
	if (PERLDB_LINE && PL_curstash != PL_debstash) {
	    SV *sv = NEWSV(88,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setsv(sv,PL_linestr);
	    av_store(GvAV(PL_curcop->cop_filegv),
	      (I32)PL_curcop->cop_line, sv);
	}
	
	/* having changed the buffer, we must update PL_bufend */
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
    }
    
    /* at this point, we have successfully read the delimited string */

    PL_multi_end = PL_curcop->cop_line;
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

  0(x[0-7A-F]+)|([0-7]+)
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
scan_num(char *start)
{
    register char *s = start;		/* current position in buffer */
    register char *d;			/* destination in temp buffer */
    register char *e;			/* end of temp buffer */
    I32 tryiv;				/* used to see if it can be an int */
    double value;			/* number read, as a double */
    SV *sv;				/* place to put the converted number */
    I32 floatit;			/* boolean: int or float? */
    char *lastub = 0;			/* position of last underbar */
    static char number_too_long[] = "Number too long";

    /* We use the first character to decide what type of number this is */

    switch (*s) {
    default:
      croak("panic: scan_num");
      
    /* if it starts with a 0, it could be an octal number, a decimal in
       0.13 disguise, or a hexadecimal number.
    */
    case '0':
	{
	  /* variables:
	     u		holds the "number so far"
	     shift	the power of 2 of the base (hex == 4, octal == 3)
	     overflowed	was the number more than we can hold?

	     Shift is used when we add a digit.  It also serves as an "are
	     we in octal or hex?" indicator to disallow hex characters when
	     in octal mode.
	   */
	    UV u;
	    I32 shift;
	    bool overflowed = FALSE;

	    /* check for hex */
	    if (s[1] == 'x') {
		shift = 4;
		s += 2;
	    }
	    /* check for a decimal in disguise */
	    else if (s[1] == '.')
		goto decimal;
	    /* so it must be octal */
	    else
		shift = 3;
	    u = 0;

	    /* read the rest of the octal number */
	    for (;;) {
		UV n, b;	/* n is used in the overflow test, b is the digit we're adding on */

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
		    if (shift != 4)
			yyerror("Illegal octal digit");
		    /* FALL THROUGH */

	        /* octal digits */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
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
		    n = u << shift;	/* make room for the digit */
		    if (!overflowed && (n >> shift) != u
			&& !(PL_hints & HINT_NEW_BINARY)) {
			warn("Integer overflow in %s number",
			     (shift == 4) ? "hex" : "octal");
			overflowed = TRUE;
		    }
		    u = n | b;		/* add the digit to the end */
		    break;
		}
	    }

	  /* if we get here, we had success: make a scalar value from
	     the number.
	  */
	  out:
	    sv = NEWSV(92,0);
	    sv_setuv(sv, u);
	    if ( PL_hints & HINT_NEW_BINARY)
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
		if (PL_dowarn && lastub && s - lastub != 3)
		    warn("Misplaced _ in number");
		lastub = ++s;
	    }
	    else {
	        /* check for end of fixed-length buffer */
		if (d >= e)
		    croak(number_too_long);
		/* if we're ok, copy the character */
		*d++ = *s++;
	    }
	}

	/* final misplaced underbar check */
	if (PL_dowarn && lastub && s - lastub != 3)
	    warn("Misplaced _ in number");

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
		    croak(number_too_long);
		if (*s != '_')
		    *d++ = *s;
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
		    croak(number_too_long);
		*d++ = *s++;
	    }
	}

	/* terminate the string */
	*d = '\0';

	/* make an sv from the string */
	sv = NEWSV(92,0);
	/* reset numeric locale in case we were earlier left in Swaziland */
	SET_NUMERIC_STANDARD();
	value = atof(PL_tokenbuf);

	/* 
	   See if we can make do with an integer value without loss of
	   precision.  We use I_V to cast to an int, because some
	   compilers have issues.  Then we try casting it back and see
	   if it was the same.  We only do this if we know we
	   specifically read an integer.

	   Note: if floatit is true, then we don't need to do the
	   conversion at all.
	*/
	tryiv = I_V(value);
	if (!floatit && (double)tryiv == value)
	    sv_setiv(sv, tryiv);
	else
	    sv_setnv(sv, value);
	if ( floatit ? (PL_hints & HINT_NEW_FLOAT) : (PL_hints & HINT_NEW_INTEGER) )
	    sv = new_constant(PL_tokenbuf, d - PL_tokenbuf, 
			      (floatit ? "float" : "integer"), sv, Nullsv, NULL);
	break;
    }

    /* make the op for the constant and return */

    yylval.opval = newSVOP(OP_CONST, 0, sv);

    return s;
}

STATIC char *
scan_formline(register char *s)
{
    dTHR;
    register char *eol;
    register char *t;
    SV *stuff = newSVpv("",0);
    bool needargs = FALSE;

    while (!needargs) {
	if (*s == '.' || *s == '}') {
	    /*SUPPRESS 530*/
#ifdef PERL_STRICT_CR
	    for (t = s+1;*t == ' ' || *t == '\t'; t++) ;
#else
	    for (t = s+1;*t == ' ' || *t == '\t' || *t == '\r'; t++) ;
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
	}
	s = eol;
	if (PL_rsfp) {
	    s = filter_gets(PL_linestr, PL_rsfp, 0);
	    PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = PL_linestart = SvPVX(PL_linestr);
	    PL_bufend = PL_bufptr + SvCUR(PL_linestr);
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
set_csh(void)
{
#ifdef CSH
    if (!PL_cshlen)
	PL_cshlen = strlen(PL_cshname);
#endif
}

I32
start_subparse(I32 is_format, U32 flags)
{
    dTHR;
    I32 oldsavestack_ix = PL_savestack_ix;
    CV* outsidecv = PL_compcv;
    AV* comppadlist;

    if (PL_compcv) {
	assert(SvTYPE(PL_compcv) == SVt_PVCV);
    }
    save_I32(&PL_subline);
    save_item(PL_subname);
    SAVEI32(PL_padix);
    SAVESPTR(PL_curpad);
    SAVESPTR(PL_comppad);
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
    PL_subline = PL_curcop->cop_line;
#ifdef USE_THREADS
    av_store(PL_comppad_name, 0, newSVpv("@_", 2));
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

int
yywarn(char *s)
{
    dTHR;
    --PL_error_count;
    PL_in_eval |= 2;
    yyerror(s);
    PL_in_eval &= ~2;
    return 0;
}

int
yyerror(char *s)
{
    dTHR;
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
    else if ((yychar & 127) == 127) {
	if (PL_lex_state == LEX_NORMAL ||
	   (PL_lex_state == LEX_KNOWNEXT && PL_lex_defer == LEX_NORMAL))
	    where = "at end of line";
	else if (PL_lex_inpat)
	    where = "within pattern";
	else
	    where = "within string";
    }
    else {
	SV *where_sv = sv_2mortal(newSVpv("next char ", 0));
	if (yychar < 32)
	    sv_catpvf(where_sv, "^%c", toCTRL(yychar));
	else if (isPRINT_LC(yychar))
	    sv_catpvf(where_sv, "%c", yychar);
	else
	    sv_catpvf(where_sv, "\\%03o", yychar & 255);
	where = SvPVX(where_sv);
    }
    msg = sv_2mortal(newSVpv(s, 0));
    sv_catpvf(msg, " at %_ line %ld, ",
	      GvSV(PL_curcop->cop_filegv), (long)PL_curcop->cop_line);
    if (context)
	sv_catpvf(msg, "near \"%.*s\"\n", contlen, context);
    else
	sv_catpvf(msg, "%s\n", where);
    if (PL_multi_start < PL_multi_end && (U32)(PL_curcop->cop_line - PL_multi_end) <= 1) {
	sv_catpvf(msg,
	"  (Might be a runaway multi-line %c%c string starting on line %ld)\n",
		(int)PL_multi_open,(int)PL_multi_close,(long)PL_multi_start);
        PL_multi_end = 0;
    }
    if (PL_in_eval & 2)
	warn("%_", msg);
    else if (PL_in_eval)
	sv_catsv(ERRSV, msg);
    else
	PerlIO_write(PerlIO_stderr(), SvPVX(msg), SvCUR(msg));
    if (++PL_error_count >= 10)
	croak("%_ has too many errors.\n", GvSV(PL_curcop->cop_filegv));
    PL_in_my = 0;
    PL_in_my_stash = Nullhv;
    return 0;
}


