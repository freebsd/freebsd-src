/* $RCSfile: toke.c,v $$Revision: 1.2.4.1 $$Date: 1997/08/08 20:55:53 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: toke.c,v $
 * Revision 1.2.4.1  1997/08/08 20:55:53  joerg
 * MFC: fix buffer overflow condition.
 *
 * Revision 1.2  1995/05/30 05:03:26  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1994/09/10  06:27:34  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:40  nate
 * PERL!
 *
 * Revision 4.0.1.9  1993/02/05  19:48:43  lwall
 * patch36: now detects ambiguous use of filetest operators as well as unary
 * patch36: fixed ambiguity on - within tr///
 *
 * Revision 4.0.1.8  92/06/23  12:33:45  lwall
 * patch35: bad interaction between backslash and hyphen in tr///
 *
 * Revision 4.0.1.7  92/06/11  21:16:30  lwall
 * patch34: expectterm incorrectly set to indicate start of program or block
 *
 * Revision 4.0.1.6  92/06/08  16:03:49  lwall
 * patch20: an EXPR may now start with a bareword
 * patch20: print $fh EXPR can now expect term rather than operator in EXPR
 * patch20: added ... as variant on ..
 * patch20: new warning on spurious backslash
 * patch20: new warning on missing $ for foreach variable
 * patch20: "foo"x1024 now legal without space after x
 * patch20: new warning on print accidentally used as function
 * patch20: tr/stuff// wasn't working right
 * patch20: 2. now eats the dot
 * patch20: <@ARGV> now notices @ARGV
 * patch20: tr/// now lets you say \-
 *
 * Revision 4.0.1.5  91/11/11  16:45:51  lwall
 * patch19: default arg for shift was wrong after first subroutine definition
 *
 * Revision 4.0.1.4  91/11/05  19:02:48  lwall
 * patch11: \x and \c were subject to double interpretation in regexps
 * patch11: prepared for ctype implementations that don't define isascii()
 * patch11: nested list operators could miscount parens
 * patch11: once-thru blocks didn't display right in the debugger
 * patch11: sort eval "whatever" didn't work
 * patch11: underscore is now allowed within literal octal and hex numbers
 *
 * Revision 4.0.1.3  91/06/10  01:32:26  lwall
 * patch10: m'$foo' now treats string as single quoted
 * patch10: certain pattern optimizations were botched
 *
 * Revision 4.0.1.2  91/06/07  12:05:56  lwall
 * patch4: new copyright notice
 * patch4: debugger lost track of lines in eval
 * patch4: //o and s///o now optimize themselves fully at runtime
 * patch4: added global modifier for pattern matches
 *
 * Revision 4.0.1.1  91/04/12  09:18:18  lwall
 * patch1: perl -de "print" wouldn't stop at the first statement
 *
 * Revision 4.0  91/03/20  01:42:14  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"
#include "perly.h"

static void set_csh();

#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

#ifdef f_next
#undef f_next
#endif

/* which backslash sequences to keep in m// or s// */

static char *patleave = "\\.^$@dDwWsSbB+*?|()-nrtfeaxc0123456789[{]}";

char *reparse;		/* if non-null, scanident found ${foo[$bar]} */

void checkcomma();

#ifdef CLINE
#undef CLINE
#endif
#define CLINE (cmdline = (curcmd->c_line < cmdline ? curcmd->c_line : cmdline))

#ifdef atarist
#define PERL_META(c) ((c) | 128)
#else
#define META(c) ((c) | 128)
#endif

#define RETURN(retval) return (bufptr = s,(int)retval)
#define OPERATOR(retval) return (expectterm = TRUE,bufptr = s,(int)retval)
#define TERM(retval) return (CLINE, expectterm = FALSE,bufptr = s,(int)retval)
#define LOOPX(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)LOOPEX)
#define FTST(f) return(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)FILETEST)
#define FUN0(f) return(yylval.ival = f,expectterm = FALSE,bufptr = s,(int)FUNC0)
#define FUN1(f) return(yylval.ival = f,expectterm = FALSE,bufptr = s,(int)FUNC1)
#define FUN2(f) return(yylval.ival = f,expectterm = FALSE,bufptr = s,(int)FUNC2)
#define FUN2x(f) return(yylval.ival = f,expectterm = FALSE,bufptr = s,(int)FUNC2x)
#define FUN3(f) return(yylval.ival = f,expectterm = FALSE,bufptr = s,(int)FUNC3)
#define FUN4(f) return(yylval.ival = f,expectterm = FALSE,bufptr = s,(int)FUNC4)
#define FUN5(f) return(yylval.ival = f,expectterm = FALSE,bufptr = s,(int)FUNC5)
#define FL(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)FLIST)
#define FL2(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)FLIST2)
#define HFUN(f) return(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)HSHFUN)
#define HFUN3(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)HSHFUN3)
#define LFUN(f) return(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)LVALFUN)
#define AOP(f) return(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)ADDOP)
#define MOP(f) return(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)MULOP)
#define EOP(f) return(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)EQOP)
#define ROP(f) return(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)RELOP)
#define FOP(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)FILOP)
#define FOP2(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)FILOP2)
#define FOP3(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)FILOP3)
#define FOP4(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)FILOP4)
#define FOP22(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)FILOP22)
#define FOP25(f) return(yylval.ival=f,expectterm = FALSE,bufptr = s,(int)FILOP25)

static char *last_uni;

/* This bit of chicanery makes a unary function followed by
 * a parenthesis into a function with one argument, highest precedence.
 */
#define UNI(f) return(yylval.ival = f, \
	expectterm = TRUE, \
	bufptr = s, \
	last_uni = oldbufptr, \
	(*s == '(' || (s = skipspace(s), *s == '(') ? (int)FUNC1 : (int)UNIOP) )

/* This does similarly for list operators, merely by pretending that the
 * paren came before the listop rather than after.
 */
#ifdef atarist
#define LOP(f) return(CLINE, *s == '(' || (s = skipspace(s), *s == '(') ? \
	(*s = (char) PERL_META('('), bufptr = oldbufptr, '(') : \
	(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)LISTOP))
#else
#define LOP(f) return(CLINE, *s == '(' || (s = skipspace(s), *s == '(') ? \
	(*s = (char) META('('), bufptr = oldbufptr, '(') : \
	(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)LISTOP))
#endif
/* grandfather return to old style */
#define OLDLOP(f) return(yylval.ival=f,expectterm = TRUE,bufptr = s,(int)LISTOP)

char *
skipspace(s)
register char *s;
{
    while (s < bufend && isSPACE(*s))
	s++;
    return s;
}

void
check_uni() {
    char *s;
    char ch;

    if (oldoldbufptr != last_uni)
	return;
    while (isSPACE(*last_uni))
	last_uni++;
    for (s = last_uni; isALNUM(*s) || *s == '-'; s++) ;
    ch = *s;
    *s = '\0';
    warn("Warning: Use of \"%s\" without parens is ambiguous", last_uni);
    *s = ch;
}

#ifdef CRIPPLED_CC

#undef UNI
#undef LOP
#define UNI(f) return uni(f,s)
#define LOP(f) return lop(f,s)

int
uni(f,s)
int f;
char *s;
{
    yylval.ival = f;
    expectterm = TRUE;
    bufptr = s;
    last_uni = oldbufptr;
    if (*s == '(')
	return FUNC1;
    s = skipspace(s);
    if (*s == '(')
	return FUNC1;
    else
	return UNIOP;
}

int
lop(f,s)
int f;
char *s;
{
    CLINE;
    if (*s != '(')
	s = skipspace(s);
    if (*s == '(') {
#ifdef atarist
	*s = PERL_META('(');
#else
	*s = META('(');
#endif
	bufptr = oldbufptr;
	return '(';
    }
    else {
	yylval.ival=f;
	expectterm = TRUE;
	bufptr = s;
	return LISTOP;
    }
}

#endif /* CRIPPLED_CC */

int
yylex()
{
    register char *s = bufptr;
    register char *d;
    register int tmp;
    static bool in_format = FALSE;
    static bool firstline = TRUE;
    extern int yychar;		/* last token */

    oldoldbufptr = oldbufptr;
    oldbufptr = s;

  retry:
#ifdef YYDEBUG
    if (debug & 1)
	if (index(s,'\n'))
	    fprintf(stderr,"Tokener at %s",s);
	else
	    fprintf(stderr,"Tokener at %s\n",s);
#endif
#ifdef BADSWITCH
    if (*s & 128) {
	if ((*s & 127) == '(') {
	    *s++ = '(';
	    oldbufptr = s;
	}
	else if ((*s & 127) == '}') {
	    *s++ = '}';
	    RETURN('}');
	}
	else
	    warn("Unrecognized character \\%03o ignored", *s++ & 255);
	goto retry;
    }
#endif
    switch (*s) {
    default:
	if ((*s & 127) == '(') {
	    *s++ = '(';
	    oldbufptr = s;
	}
	else if ((*s & 127) == '}') {
	    *s++ = '}';
	    RETURN('}');
	}
	else
	    warn("Unrecognized character \\%03o ignored", *s++ & 255);
	goto retry;
    case 4:
    case 26:
	goto fake_eof;			/* emulate EOF on ^D or ^Z */
    case 0:
	if (!rsfp)
	    RETURN(0);
	if (s++ < bufend)
	    goto retry;			/* ignore stray nulls */
	last_uni = 0;
	if (firstline) {
	    firstline = FALSE;
	    if (minus_n || minus_p || perldb) {
		str_set(linestr,"");
		if (perldb) {
		    char *getenv();
		    char *pdb = getenv("PERLDB");

		    str_cat(linestr, pdb ? pdb : "require 'perldb.pl'");
		    str_cat(linestr, ";");
		}
		if (minus_n || minus_p) {
		    str_cat(linestr,"line: while (<>) {");
		    if (minus_l)
			str_cat(linestr,"chop;");
		    if (minus_a)
			str_cat(linestr,"@F=split(' ');");
		}
		oldoldbufptr = oldbufptr = s = str_get(linestr);
		bufend = linestr->str_ptr + linestr->str_cur;
		goto retry;
	    }
	}
	if (in_format) {
	    bufptr = bufend;
	    yylval.formval = load_format();
	    in_format = FALSE;
	    oldoldbufptr = oldbufptr = s = str_get(linestr) + 1;
	    bufend = linestr->str_ptr + linestr->str_cur;
	    OPERATOR(FORMLIST);
	}
	curcmd->c_line++;
#ifdef CRYPTSCRIPT
	cryptswitch();
#endif /* CRYPTSCRIPT */
	do {
	    if ((s = str_gets(linestr, rsfp, 0)) == Nullch) {
	      fake_eof:
		if (rsfp) {
		    if (preprocess)
			(void)mypclose(rsfp);
		    else if ((FILE*)rsfp == stdin)
			clearerr(stdin);
		    else
			(void)fclose(rsfp);
		    rsfp = Nullfp;
		}
		if (minus_n || minus_p) {
		    str_set(linestr,minus_p ? ";}continue{print" : "");
		    str_cat(linestr,";}");
		    oldoldbufptr = oldbufptr = s = str_get(linestr);
		    bufend = linestr->str_ptr + linestr->str_cur;
		    minus_n = minus_p = 0;
		    goto retry;
		}
		oldoldbufptr = oldbufptr = s = str_get(linestr);
		str_set(linestr,"");
		RETURN(';');	/* not infinite loop because rsfp is NULL now */
	    }
	    if (doextract && *linestr->str_ptr == '#')
		doextract = FALSE;
	} while (doextract);
	oldoldbufptr = oldbufptr = bufptr = s;
	if (perldb) {
	    STR *str = Str_new(85,0);

	    str_sset(str,linestr);
	    astore(stab_xarray(curcmd->c_filestab),(int)curcmd->c_line,str);
	}
#ifdef DEBUG
	if (firstline) {
	    char *showinput();
	    s = showinput();
	}
#endif
	bufend = linestr->str_ptr + linestr->str_cur;
	if (curcmd->c_line == 1) {
	    if (*s == '#' && s[1] == '!') {
		if (!in_eval && !instr(s,"perl") && instr(origargv[0],"perl")) {
		    char **newargv;
		    char *cmd;

		    s += 2;
		    if (*s == ' ')
			s++;
		    cmd = s;
		    while (s < bufend && !isSPACE(*s))
			s++;
		    *s++ = '\0';
		    while (s < bufend && isSPACE(*s))
			s++;
		    if (s < bufend) {
			Newz(899,newargv,origargc+3,char*);
			newargv[1] = s;
			while (s < bufend && !isSPACE(*s))
			    s++;
			*s = '\0';
			Copy(origargv+1, newargv+2, origargc+1, char*);
		    }
		    else
			newargv = origargv;
		    newargv[0] = cmd;
		    execv(cmd,newargv);
		    fatal("Can't exec %s", cmd);
		}
	    }
	    else {
		while (s < bufend && isSPACE(*s))
		    s++;
		if (*s == ':')	/* for csh's that have to exec sh scripts */
		    s++;
	    }
	}
	goto retry;
    case ' ': case '\t': case '\f': case '\r': case 013:
	s++;
	goto retry;
    case '#':
	if (preprocess && s == str_get(linestr) &&
	       s[1] == ' ' && (isDIGIT(s[2]) || strnEQ(s+2,"line ",5)) ) {
	    while (*s && !isDIGIT(*s))
		s++;
	    curcmd->c_line = atoi(s)-1;
	    while (isDIGIT(*s))
		s++;
	    d = bufend;
	    while (s < d && isSPACE(*s)) s++;
	    s[strlen(s)-1] = '\0';	/* wipe out newline */
	    if (*s == '"') {
		s++;
		s[strlen(s)-1] = '\0';	/* wipe out trailing quote */
	    }
	    if (*s)
		curcmd->c_filestab = fstab(s);
	    else
		curcmd->c_filestab = fstab(origfilename);
	    oldoldbufptr = oldbufptr = s = str_get(linestr);
	}
	/* FALL THROUGH */
    case '\n':
	if (in_eval && !rsfp) {
	    d = bufend;
	    while (s < d && *s != '\n')
		s++;
	    if (s < d)
		s++;
	    if (in_format) {
		bufptr = s;
		yylval.formval = load_format();
		in_format = FALSE;
		oldoldbufptr = oldbufptr = s = bufptr + 1;
		TERM(FORMLIST);
	    }
	    curcmd->c_line++;
	}
	else {
	    *s = '\0';
	    bufend = s;
	}
	goto retry;
    case '-':
	if (s[1] && isALPHA(s[1]) && !isALPHA(s[2])) {
	    s++;
	    last_uni = oldbufptr;
	    switch (*s++) {
	    case 'r': FTST(O_FTEREAD);
	    case 'w': FTST(O_FTEWRITE);
	    case 'x': FTST(O_FTEEXEC);
	    case 'o': FTST(O_FTEOWNED);
	    case 'R': FTST(O_FTRREAD);
	    case 'W': FTST(O_FTRWRITE);
	    case 'X': FTST(O_FTREXEC);
	    case 'O': FTST(O_FTROWNED);
	    case 'e': FTST(O_FTIS);
	    case 'z': FTST(O_FTZERO);
	    case 's': FTST(O_FTSIZE);
	    case 'f': FTST(O_FTFILE);
	    case 'd': FTST(O_FTDIR);
	    case 'l': FTST(O_FTLINK);
	    case 'p': FTST(O_FTPIPE);
	    case 'S': FTST(O_FTSOCK);
	    case 'u': FTST(O_FTSUID);
	    case 'g': FTST(O_FTSGID);
	    case 'k': FTST(O_FTSVTX);
	    case 'b': FTST(O_FTBLK);
	    case 'c': FTST(O_FTCHR);
	    case 't': FTST(O_FTTTY);
	    case 'T': FTST(O_FTTEXT);
	    case 'B': FTST(O_FTBINARY);
	    case 'M': stabent("\024",TRUE); FTST(O_FTMTIME);
	    case 'A': stabent("\024",TRUE); FTST(O_FTATIME);
	    case 'C': stabent("\024",TRUE); FTST(O_FTCTIME);
	    default:
		s -= 2;
		break;
	    }
	}
	tmp = *s++;
	if (*s == tmp) {
	    s++;
	    RETURN(DEC);
	}
	if (expectterm) {
	    if (isSPACE(*s) || !isSPACE(*bufptr))
		check_uni();
	    OPERATOR('-');
	}
	else
	    AOP(O_SUBTRACT);
    case '+':
	tmp = *s++;
	if (*s == tmp) {
	    s++;
	    RETURN(INC);
	}
	if (expectterm) {
	    if (isSPACE(*s) || !isSPACE(*bufptr))
		check_uni();
	    OPERATOR('+');
	}
	else
	    AOP(O_ADD);

    case '*':
	if (expectterm) {
	    check_uni();
	    s = scanident(s,bufend,tokenbuf,sizeof tokenbuf);
	    yylval.stabval = stabent(tokenbuf,TRUE);
	    TERM(STAR);
	}
	tmp = *s++;
	if (*s == tmp) {
	    s++;
	    OPERATOR(POW);
	}
	MOP(O_MULTIPLY);
    case '%':
	if (expectterm) {
	    if (!isALPHA(s[1]))
		check_uni();
	    s = scanident(s,bufend,tokenbuf,sizeof tokenbuf);
	    yylval.stabval = hadd(stabent(tokenbuf,TRUE));
	    TERM(HSH);
	}
	s++;
	MOP(O_MODULO);

    case '^':
    case '~':
    case '(':
    case ',':
    case ':':
    case '[':
	tmp = *s++;
	OPERATOR(tmp);
    case '{':
	tmp = *s++;
	yylval.ival = curcmd->c_line;
	if (isSPACE(*s) || *s == '#')
	    cmdline = NOLINE;   /* invalidate current command line number */
	expectterm = 2;
	RETURN(tmp);
    case ';':
	if (curcmd->c_line < cmdline)
	    cmdline = curcmd->c_line;
	tmp = *s++;
	OPERATOR(tmp);
    case ')':
    case ']':
	tmp = *s++;
	TERM(tmp);
    case '}':
	*s |= 128;
	RETURN(';');
    case '&':
	s++;
	tmp = *s++;
	if (tmp == '&')
	    OPERATOR(ANDAND);
	s--;
	if (expectterm) {
	    d = bufend;
	    while (s < d && isSPACE(*s))
		s++;
	    if (isALPHA(*s) || *s == '_' || *s == '\'')
		*(--s) = '\\';	/* force next ident to WORD */
	    else
		check_uni();
	    OPERATOR(AMPER);
	}
	OPERATOR('&');
    case '|':
	s++;
	tmp = *s++;
	if (tmp == '|')
	    OPERATOR(OROR);
	s--;
	OPERATOR('|');
    case '=':
	s++;
	tmp = *s++;
	if (tmp == '=')
	    EOP(O_EQ);
	if (tmp == '~')
	    OPERATOR(MATCH);
	s--;
	OPERATOR('=');
    case '!':
	s++;
	tmp = *s++;
	if (tmp == '=')
	    EOP(O_NE);
	if (tmp == '~')
	    OPERATOR(NMATCH);
	s--;
	OPERATOR('!');
    case '<':
	if (expectterm) {
	    if (s[1] != '<' && !index(s,'>'))
		check_uni();
	    s = scanstr(s, SCAN_DEF);
	    TERM(RSTRING);
	}
	s++;
	tmp = *s++;
	if (tmp == '<')
	    OPERATOR(LS);
	if (tmp == '=') {
	    tmp = *s++;
	    if (tmp == '>')
		EOP(O_NCMP);
	    s--;
	    ROP(O_LE);
	}
	s--;
	ROP(O_LT);
    case '>':
	s++;
	tmp = *s++;
	if (tmp == '>')
	    OPERATOR(RS);
	if (tmp == '=')
	    ROP(O_GE);
	s--;
	ROP(O_GT);

#define SNARFWORD \
	d = tokenbuf; \
	while (isALNUM(*s) || *s == '\'') \
	    *d++ = *s++; \
	while (d[-1] == '\'') \
	    d--,s--; \
	*d = '\0'; \
	d = tokenbuf;

    case '$':
	if (s[1] == '#' && (isALPHA(s[2]) || s[2] == '_')) {
	    s++;
	    s = scanident(s,bufend,tokenbuf,sizeof tokenbuf);
	    yylval.stabval = aadd(stabent(tokenbuf,TRUE));
	    TERM(ARYLEN);
	}
	d = s;
	s = scanident(s,bufend,tokenbuf,sizeof tokenbuf);
	if (reparse) {		/* turn ${foo[bar]} into ($foo[bar]) */
	  do_reparse:
	    s[-1] = ')';
	    s = d;
	    s[1] = s[0];
	    s[0] = '(';
	    goto retry;
	}
	yylval.stabval = stabent(tokenbuf,TRUE);
	expectterm = FALSE;
	if (isSPACE(*s) && oldoldbufptr && oldoldbufptr < bufptr) {
	    s++;
	    while (isSPACE(*oldoldbufptr))
		oldoldbufptr++;
	    if (*oldoldbufptr == 'p' && strnEQ(oldoldbufptr,"print",5)) {
		if (index("&*<%", *s) && isALPHA(s[1]))
		    expectterm = TRUE;		/* e.g. print $fh &sub */
		else if (*s == '.' && isDIGIT(s[1]))
		    expectterm = TRUE;		/* e.g. print $fh .3 */
		else if (index("/?-+", *s) && !isSPACE(s[1]))
		    expectterm = TRUE;		/* e.g. print $fh -1 */
	    }
	}
	RETURN(REG);

    case '@':
	d = s;
	s = scanident(s,bufend,tokenbuf,sizeof tokenbuf);
	if (reparse)
	    goto do_reparse;
	yylval.stabval = aadd(stabent(tokenbuf,TRUE));
	TERM(ARY);

    case '/':			/* may either be division or pattern */
    case '?':			/* may either be conditional or pattern */
	if (expectterm) {
	    check_uni();
	    s = scanpat(s);
	    TERM(PATTERN);
	}
	tmp = *s++;
	if (tmp == '/')
	    MOP(O_DIVIDE);
	OPERATOR(tmp);

    case '.':
	if (!expectterm || !isDIGIT(s[1])) {
	    tmp = *s++;
	    if (*s == tmp) {
		s++;
		if (*s == tmp) {
		    s++;
		    yylval.ival = 0;
		}
		else
		    yylval.ival = AF_COMMON;
		OPERATOR(DOTDOT);
	    }
	    if (expectterm)
		check_uni();
	    AOP(O_CONCAT);
	}
	/* FALL THROUGH */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '\'': case '"': case '`':
	s = scanstr(s, SCAN_DEF);
	TERM(RSTRING);

    case '\\':	/* some magic to force next word to be a WORD */
	s++;	/* used by do and sub to force a separate namespace */
	if (!isALPHA(*s) && *s != '_' && *s != '\'') {
	    warn("Spurious backslash ignored");
	    goto retry;
	}
	/* FALL THROUGH */
    case '_':
	SNARFWORD;
	if (d[1] == '_') {
	    if (strEQ(d,"__LINE__") || strEQ(d,"__FILE__")) {
		ARG *arg = op_new(1);

		yylval.arg = arg;
		arg->arg_type = O_ITEM;
		if (d[2] == 'L')
		    (void)sprintf(tokenbuf,"%ld",(long)curcmd->c_line);
		else
		    strcpy(tokenbuf, stab_val(curcmd->c_filestab)->str_ptr);
		arg[1].arg_type = A_SINGLE;
		arg[1].arg_ptr.arg_str = str_make(tokenbuf,strlen(tokenbuf));
		TERM(RSTRING);
	    }
	    else if (strEQ(d,"__END__")) {
		STAB *stab;
		int fd;

		/*SUPPRESS 560*/
		if (!in_eval && (stab = stabent("DATA",FALSE))) {
		    stab->str_pok |= SP_MULTI;
		    if (!stab_io(stab))
			stab_io(stab) = stio_new();
		    stab_io(stab)->ifp = rsfp;
#if defined(HAS_FCNTL) && defined(F_SETFD)
		    fd = fileno(rsfp);
		    fcntl(fd,F_SETFD,fd >= 3);
#endif
		    if (preprocess)
			stab_io(stab)->type = '|';
		    else if ((FILE*)rsfp == stdin)
			stab_io(stab)->type = '-';
		    else
			stab_io(stab)->type = '<';
		    rsfp = Nullfp;
		}
		goto fake_eof;
	    }
	}
	break;
    case 'a': case 'A':
	SNARFWORD;
	if (strEQ(d,"alarm"))
	    UNI(O_ALARM);
	if (strEQ(d,"accept"))
	    FOP22(O_ACCEPT);
	if (strEQ(d,"atan2"))
	    FUN2(O_ATAN2);
	break;
    case 'b': case 'B':
	SNARFWORD;
	if (strEQ(d,"bind"))
	    FOP2(O_BIND);
	if (strEQ(d,"binmode"))
	    FOP(O_BINMODE);
	break;
    case 'c': case 'C':
	SNARFWORD;
	if (strEQ(d,"chop"))
	    LFUN(O_CHOP);
	if (strEQ(d,"continue"))
	    OPERATOR(CONTINUE);
	if (strEQ(d,"chdir")) {
	    (void)stabent("ENV",TRUE);	/* may use HOME */
	    UNI(O_CHDIR);
	}
	if (strEQ(d,"close"))
	    FOP(O_CLOSE);
	if (strEQ(d,"closedir"))
	    FOP(O_CLOSEDIR);
	if (strEQ(d,"cmp"))
	    EOP(O_SCMP);
	if (strEQ(d,"caller"))
	    UNI(O_CALLER);
	if (strEQ(d,"crypt")) {
#ifdef FCRYPT
	    static int cryptseen = 0;

	    if (!cryptseen++)
		init_des();
#endif
	    FUN2(O_CRYPT);
	}
	if (strEQ(d,"chmod"))
	    LOP(O_CHMOD);
	if (strEQ(d,"chown"))
	    LOP(O_CHOWN);
	if (strEQ(d,"connect"))
	    FOP2(O_CONNECT);
	if (strEQ(d,"cos"))
	    UNI(O_COS);
	if (strEQ(d,"chroot"))
	    UNI(O_CHROOT);
	break;
    case 'd': case 'D':
	SNARFWORD;
	if (strEQ(d,"do")) {
	    d = bufend;
	    while (s < d && isSPACE(*s))
		s++;
	    if (isALPHA(*s) || *s == '_')
		*(--s) = '\\';	/* force next ident to WORD */
	    OPERATOR(DO);
	}
	if (strEQ(d,"die"))
	    LOP(O_DIE);
	if (strEQ(d,"defined"))
	    LFUN(O_DEFINED);
	if (strEQ(d,"delete"))
	    OPERATOR(DELETE);
	if (strEQ(d,"dbmopen"))
	    HFUN3(O_DBMOPEN);
	if (strEQ(d,"dbmclose"))
	    HFUN(O_DBMCLOSE);
	if (strEQ(d,"dump"))
	    LOOPX(O_DUMP);
	break;
    case 'e': case 'E':
	SNARFWORD;
	if (strEQ(d,"else"))
	    OPERATOR(ELSE);
	if (strEQ(d,"elsif")) {
	    yylval.ival = curcmd->c_line;
	    OPERATOR(ELSIF);
	}
	if (strEQ(d,"eq") || strEQ(d,"EQ"))
	    EOP(O_SEQ);
	if (strEQ(d,"exit"))
	    UNI(O_EXIT);
	if (strEQ(d,"eval")) {
	    allstabs = TRUE;		/* must initialize everything since */
	    UNI(O_EVAL);		/* we don't know what will be used */
	}
	if (strEQ(d,"eof"))
	    FOP(O_EOF);
	if (strEQ(d,"exp"))
	    UNI(O_EXP);
	if (strEQ(d,"each"))
	    HFUN(O_EACH);
	if (strEQ(d,"exec")) {
	    set_csh();
	    LOP(O_EXEC_OP);
	}
	if (strEQ(d,"endhostent"))
	    FUN0(O_EHOSTENT);
	if (strEQ(d,"endnetent"))
	    FUN0(O_ENETENT);
	if (strEQ(d,"endservent"))
	    FUN0(O_ESERVENT);
	if (strEQ(d,"endprotoent"))
	    FUN0(O_EPROTOENT);
	if (strEQ(d,"endpwent"))
	    FUN0(O_EPWENT);
	if (strEQ(d,"endgrent"))
	    FUN0(O_EGRENT);
	break;
    case 'f': case 'F':
	SNARFWORD;
	if (strEQ(d,"for") || strEQ(d,"foreach")) {
	    yylval.ival = curcmd->c_line;
	    while (s < bufend && isSPACE(*s))
		s++;
	    if (isALPHA(*s))
		fatal("Missing $ on loop variable");
	    OPERATOR(FOR);
	}
	if (strEQ(d,"format")) {
	    d = bufend;
	    while (s < d && isSPACE(*s))
		s++;
	    if (isALPHA(*s) || *s == '_')
		*(--s) = '\\';	/* force next ident to WORD */
	    in_format = TRUE;
	    allstabs = TRUE;		/* must initialize everything since */
	    OPERATOR(FORMAT);		/* we don't know what will be used */
	}
	if (strEQ(d,"fork"))
	    FUN0(O_FORK);
	if (strEQ(d,"fcntl"))
	    FOP3(O_FCNTL);
	if (strEQ(d,"fileno"))
	    FOP(O_FILENO);
	if (strEQ(d,"flock"))
	    FOP2(O_FLOCK);
	break;
    case 'g': case 'G':
	SNARFWORD;
	if (strEQ(d,"gt") || strEQ(d,"GT"))
	    ROP(O_SGT);
	if (strEQ(d,"ge") || strEQ(d,"GE"))
	    ROP(O_SGE);
	if (strEQ(d,"grep"))
	    FL2(O_GREP);
	if (strEQ(d,"goto"))
	    LOOPX(O_GOTO);
	if (strEQ(d,"gmtime"))
	    UNI(O_GMTIME);
	if (strEQ(d,"getc"))
	    FOP(O_GETC);
	if (strnEQ(d,"get",3)) {
	    d += 3;
	    if (*d == 'p') {
		if (strEQ(d,"ppid"))
		    FUN0(O_GETPPID);
		if (strEQ(d,"pgrp"))
		    UNI(O_GETPGRP);
		if (strEQ(d,"priority"))
		    FUN2(O_GETPRIORITY);
		if (strEQ(d,"protobyname"))
		    UNI(O_GPBYNAME);
		if (strEQ(d,"protobynumber"))
		    FUN1(O_GPBYNUMBER);
		if (strEQ(d,"protoent"))
		    FUN0(O_GPROTOENT);
		if (strEQ(d,"pwent"))
		    FUN0(O_GPWENT);
		if (strEQ(d,"pwnam"))
		    FUN1(O_GPWNAM);
		if (strEQ(d,"pwuid"))
		    FUN1(O_GPWUID);
		if (strEQ(d,"peername"))
		    FOP(O_GETPEERNAME);
	    }
	    else if (*d == 'h') {
		if (strEQ(d,"hostbyname"))
		    UNI(O_GHBYNAME);
		if (strEQ(d,"hostbyaddr"))
		    FUN2(O_GHBYADDR);
		if (strEQ(d,"hostent"))
		    FUN0(O_GHOSTENT);
	    }
	    else if (*d == 'n') {
		if (strEQ(d,"netbyname"))
		    UNI(O_GNBYNAME);
		if (strEQ(d,"netbyaddr"))
		    FUN2(O_GNBYADDR);
		if (strEQ(d,"netent"))
		    FUN0(O_GNETENT);
	    }
	    else if (*d == 's') {
		if (strEQ(d,"servbyname"))
		    FUN2(O_GSBYNAME);
		if (strEQ(d,"servbyport"))
		    FUN2(O_GSBYPORT);
		if (strEQ(d,"servent"))
		    FUN0(O_GSERVENT);
		if (strEQ(d,"sockname"))
		    FOP(O_GETSOCKNAME);
		if (strEQ(d,"sockopt"))
		    FOP3(O_GSOCKOPT);
	    }
	    else if (*d == 'g') {
		if (strEQ(d,"grent"))
		    FUN0(O_GGRENT);
		if (strEQ(d,"grnam"))
		    FUN1(O_GGRNAM);
		if (strEQ(d,"grgid"))
		    FUN1(O_GGRGID);
	    }
	    else if (*d == 'l') {
		if (strEQ(d,"login"))
		    FUN0(O_GETLOGIN);
	    }
	    d -= 3;
	}
	break;
    case 'h': case 'H':
	SNARFWORD;
	if (strEQ(d,"hex"))
	    UNI(O_HEX);
	break;
    case 'i': case 'I':
	SNARFWORD;
	if (strEQ(d,"if")) {
	    yylval.ival = curcmd->c_line;
	    OPERATOR(IF);
	}
	if (strEQ(d,"index"))
	    FUN2x(O_INDEX);
	if (strEQ(d,"int"))
	    UNI(O_INT);
	if (strEQ(d,"ioctl"))
	    FOP3(O_IOCTL);
	break;
    case 'j': case 'J':
	SNARFWORD;
	if (strEQ(d,"join"))
	    FL2(O_JOIN);
	break;
    case 'k': case 'K':
	SNARFWORD;
	if (strEQ(d,"keys"))
	    HFUN(O_KEYS);
	if (strEQ(d,"kill"))
	    LOP(O_KILL);
	break;
    case 'l': case 'L':
	SNARFWORD;
	if (strEQ(d,"last"))
	    LOOPX(O_LAST);
	if (strEQ(d,"local"))
	    OPERATOR(LOCAL);
	if (strEQ(d,"length"))
	    UNI(O_LENGTH);
	if (strEQ(d,"lt") || strEQ(d,"LT"))
	    ROP(O_SLT);
	if (strEQ(d,"le") || strEQ(d,"LE"))
	    ROP(O_SLE);
	if (strEQ(d,"localtime"))
	    UNI(O_LOCALTIME);
	if (strEQ(d,"log"))
	    UNI(O_LOG);
	if (strEQ(d,"link"))
	    FUN2(O_LINK);
	if (strEQ(d,"listen"))
	    FOP2(O_LISTEN);
	if (strEQ(d,"lstat"))
	    FOP(O_LSTAT);
	break;
    case 'm': case 'M':
	if (s[1] == '\'') {
	    d = "m";
	    s++;
	}
	else {
	    SNARFWORD;
	}
	if (strEQ(d,"m")) {
	    s = scanpat(s-1);
	    if (yylval.arg)
		TERM(PATTERN);
	    else
		RETURN(1);	/* force error */
	}
	switch (d[1]) {
	case 'k':
	    if (strEQ(d,"mkdir"))
		FUN2(O_MKDIR);
	    break;
	case 's':
	    if (strEQ(d,"msgctl"))
		FUN3(O_MSGCTL);
	    if (strEQ(d,"msgget"))
		FUN2(O_MSGGET);
	    if (strEQ(d,"msgrcv"))
		FUN5(O_MSGRCV);
	    if (strEQ(d,"msgsnd"))
		FUN3(O_MSGSND);
	    break;
	}
	break;
    case 'n': case 'N':
	SNARFWORD;
	if (strEQ(d,"next"))
	    LOOPX(O_NEXT);
	if (strEQ(d,"ne") || strEQ(d,"NE"))
	    EOP(O_SNE);
	break;
    case 'o': case 'O':
	SNARFWORD;
	if (strEQ(d,"open"))
	    OPERATOR(OPEN);
	if (strEQ(d,"ord"))
	    UNI(O_ORD);
	if (strEQ(d,"oct"))
	    UNI(O_OCT);
	if (strEQ(d,"opendir"))
	    FOP2(O_OPEN_DIR);
	break;
    case 'p': case 'P':
	SNARFWORD;
	if (strEQ(d,"print")) {
	    checkcomma(s,d,"filehandle");
	    LOP(O_PRINT);
	}
	if (strEQ(d,"printf")) {
	    checkcomma(s,d,"filehandle");
	    LOP(O_PRTF);
	}
	if (strEQ(d,"push")) {
	    yylval.ival = O_PUSH;
	    OPERATOR(PUSH);
	}
	if (strEQ(d,"pop"))
	    OPERATOR(POP);
	if (strEQ(d,"pack"))
	    FL2(O_PACK);
	if (strEQ(d,"package"))
	    OPERATOR(PACKAGE);
	if (strEQ(d,"pipe"))
	    FOP22(O_PIPE_OP);
	break;
    case 'q': case 'Q':
	SNARFWORD;
	if (strEQ(d,"q")) {
	    s = scanstr(s-1, SCAN_DEF);
	    TERM(RSTRING);
	}
	if (strEQ(d,"qq")) {
	    s = scanstr(s-2, SCAN_DEF);
	    TERM(RSTRING);
	}
	if (strEQ(d,"qx")) {
	    s = scanstr(s-2, SCAN_DEF);
	    TERM(RSTRING);
	}
	break;
    case 'r': case 'R':
	SNARFWORD;
	if (strEQ(d,"return"))
	    OLDLOP(O_RETURN);
	if (strEQ(d,"require")) {
	    allstabs = TRUE;		/* must initialize everything since */
	    UNI(O_REQUIRE);		/* we don't know what will be used */
	}
	if (strEQ(d,"reset"))
	    UNI(O_RESET);
	if (strEQ(d,"redo"))
	    LOOPX(O_REDO);
	if (strEQ(d,"rename"))
	    FUN2(O_RENAME);
	if (strEQ(d,"rand"))
	    UNI(O_RAND);
	if (strEQ(d,"rmdir"))
	    UNI(O_RMDIR);
	if (strEQ(d,"rindex"))
	    FUN2x(O_RINDEX);
	if (strEQ(d,"read"))
	    FOP3(O_READ);
	if (strEQ(d,"readdir"))
	    FOP(O_READDIR);
	if (strEQ(d,"rewinddir"))
	    FOP(O_REWINDDIR);
	if (strEQ(d,"recv"))
	    FOP4(O_RECV);
	if (strEQ(d,"reverse"))
	    LOP(O_REVERSE);
	if (strEQ(d,"readlink"))
	    UNI(O_READLINK);
	break;
    case 's': case 'S':
	if (s[1] == '\'') {
	    d = "s";
	    s++;
	}
	else {
	    SNARFWORD;
	}
	if (strEQ(d,"s")) {
	    s = scansubst(s);
	    if (yylval.arg)
		TERM(SUBST);
	    else
		RETURN(1);	/* force error */
	}
	switch (d[1]) {
	case 'a':
	case 'b':
	    break;
	case 'c':
	    if (strEQ(d,"scalar"))
		UNI(O_SCALAR);
	    break;
	case 'd':
	    break;
	case 'e':
	    if (strEQ(d,"select"))
		OPERATOR(SSELECT);
	    if (strEQ(d,"seek"))
		FOP3(O_SEEK);
	    if (strEQ(d,"semctl"))
		FUN4(O_SEMCTL);
	    if (strEQ(d,"semget"))
		FUN3(O_SEMGET);
	    if (strEQ(d,"semop"))
		FUN2(O_SEMOP);
	    if (strEQ(d,"send"))
		FOP3(O_SEND);
	    if (strEQ(d,"setpgrp"))
		FUN2(O_SETPGRP);
	    if (strEQ(d,"setpriority"))
		FUN3(O_SETPRIORITY);
	    if (strEQ(d,"sethostent"))
		FUN1(O_SHOSTENT);
	    if (strEQ(d,"setnetent"))
		FUN1(O_SNETENT);
	    if (strEQ(d,"setservent"))
		FUN1(O_SSERVENT);
	    if (strEQ(d,"setprotoent"))
		FUN1(O_SPROTOENT);
	    if (strEQ(d,"setpwent"))
		FUN0(O_SPWENT);
	    if (strEQ(d,"setgrent"))
		FUN0(O_SGRENT);
	    if (strEQ(d,"seekdir"))
		FOP2(O_SEEKDIR);
	    if (strEQ(d,"setsockopt"))
		FOP4(O_SSOCKOPT);
	    break;
	case 'f':
	case 'g':
	    break;
	case 'h':
	    if (strEQ(d,"shift"))
		TERM(SHIFT);
	    if (strEQ(d,"shmctl"))
		FUN3(O_SHMCTL);
	    if (strEQ(d,"shmget"))
		FUN3(O_SHMGET);
	    if (strEQ(d,"shmread"))
		FUN4(O_SHMREAD);
	    if (strEQ(d,"shmwrite"))
		FUN4(O_SHMWRITE);
	    if (strEQ(d,"shutdown"))
		FOP2(O_SHUTDOWN);
	    break;
	case 'i':
	    if (strEQ(d,"sin"))
		UNI(O_SIN);
	    break;
	case 'j':
	case 'k':
	    break;
	case 'l':
	    if (strEQ(d,"sleep"))
		UNI(O_SLEEP);
	    break;
	case 'm':
	case 'n':
	    break;
	case 'o':
	    if (strEQ(d,"socket"))
		FOP4(O_SOCKET);
	    if (strEQ(d,"socketpair"))
		FOP25(O_SOCKPAIR);
	    if (strEQ(d,"sort")) {
		checkcomma(s,d,"subroutine name");
		d = bufend;
		while (s < d && isSPACE(*s)) s++;
		if (*s == ';' || *s == ')')		/* probably a close */
		    fatal("sort is now a reserved word");
		if (isALPHA(*s) || *s == '_') {
		    /*SUPPRESS 530*/
		    for (d = s; isALNUM(*d); d++) ;
		    strncpy(tokenbuf,s,d-s);
		    tokenbuf[d-s] = '\0';
		    if (strNE(tokenbuf,"keys") &&
			strNE(tokenbuf,"values") &&
			strNE(tokenbuf,"split") &&
			strNE(tokenbuf,"grep") &&
			strNE(tokenbuf,"readdir") &&
			strNE(tokenbuf,"unpack") &&
			strNE(tokenbuf,"do") &&
			strNE(tokenbuf,"eval") &&
			(d >= bufend || isSPACE(*d)) )
			*(--s) = '\\';	/* force next ident to WORD */
		}
		LOP(O_SORT);
	    }
	    break;
	case 'p':
	    if (strEQ(d,"split"))
		TERM(SPLIT);
	    if (strEQ(d,"sprintf"))
		FL(O_SPRINTF);
	    if (strEQ(d,"splice")) {
		yylval.ival = O_SPLICE;
		OPERATOR(PUSH);
	    }
	    break;
	case 'q':
	    if (strEQ(d,"sqrt"))
		UNI(O_SQRT);
	    break;
	case 'r':
	    if (strEQ(d,"srand"))
		UNI(O_SRAND);
	    break;
	case 's':
	    break;
	case 't':
	    if (strEQ(d,"stat"))
		FOP(O_STAT);
	    if (strEQ(d,"study")) {
		sawstudy++;
		LFUN(O_STUDY);
	    }
	    break;
	case 'u':
	    if (strEQ(d,"substr"))
		FUN2x(O_SUBSTR);
	    if (strEQ(d,"sub")) {
		yylval.ival = savestack->ary_fill; /* restore stuff on reduce */
		savelong(&subline);
		saveitem(subname);

		subline = curcmd->c_line;
		d = bufend;
		while (s < d && isSPACE(*s))
		    s++;
		if (isALPHA(*s) || *s == '_' || *s == '\'') {
		    str_sset(subname,curstname);
		    str_ncat(subname,"'",1);
		    for (d = s+1; isALNUM(*d) || *d == '\''; d++)
			/*SUPPRESS 530*/
			;
		    if (d[-1] == '\'')
			d--;
		    str_ncat(subname,s,d-s);
		    *(--s) = '\\';	/* force next ident to WORD */
		}
		else
		    str_set(subname,"?");
		OPERATOR(SUB);
	    }
	    break;
	case 'v':
	case 'w':
	case 'x':
	    break;
	case 'y':
	    if (strEQ(d,"system")) {
		set_csh();
		LOP(O_SYSTEM);
	    }
	    if (strEQ(d,"symlink"))
		FUN2(O_SYMLINK);
	    if (strEQ(d,"syscall"))
		LOP(O_SYSCALL);
	    if (strEQ(d,"sysread"))
		FOP3(O_SYSREAD);
	    if (strEQ(d,"syswrite"))
		FOP3(O_SYSWRITE);
	    break;
	case 'z':
	    break;
	}
	break;
    case 't': case 'T':
	SNARFWORD;
	if (strEQ(d,"tr")) {
	    s = scantrans(s);
	    if (yylval.arg)
		TERM(TRANS);
	    else
		RETURN(1);	/* force error */
	}
	if (strEQ(d,"tell"))
	    FOP(O_TELL);
	if (strEQ(d,"telldir"))
	    FOP(O_TELLDIR);
	if (strEQ(d,"time"))
	    FUN0(O_TIME);
	if (strEQ(d,"times"))
	    FUN0(O_TMS);
	if (strEQ(d,"truncate"))
	    FOP2(O_TRUNCATE);
	break;
    case 'u': case 'U':
	SNARFWORD;
	if (strEQ(d,"using"))
	    OPERATOR(USING);
	if (strEQ(d,"until")) {
	    yylval.ival = curcmd->c_line;
	    OPERATOR(UNTIL);
	}
	if (strEQ(d,"unless")) {
	    yylval.ival = curcmd->c_line;
	    OPERATOR(UNLESS);
	}
	if (strEQ(d,"unlink"))
	    LOP(O_UNLINK);
	if (strEQ(d,"undef"))
	    LFUN(O_UNDEF);
	if (strEQ(d,"unpack"))
	    FUN2(O_UNPACK);
	if (strEQ(d,"utime"))
	    LOP(O_UTIME);
	if (strEQ(d,"umask"))
	    UNI(O_UMASK);
	if (strEQ(d,"unshift")) {
	    yylval.ival = O_UNSHIFT;
	    OPERATOR(PUSH);
	}
	break;
    case 'v': case 'V':
	SNARFWORD;
	if (strEQ(d,"values"))
	    HFUN(O_VALUES);
	if (strEQ(d,"vec")) {
	    sawvec = TRUE;
	    FUN3(O_VEC);
	}
	break;
    case 'w': case 'W':
	SNARFWORD;
	if (strEQ(d,"while")) {
	    yylval.ival = curcmd->c_line;
	    OPERATOR(WHILE);
	}
	if (strEQ(d,"warn"))
	    LOP(O_WARN);
	if (strEQ(d,"wait"))
	    FUN0(O_WAIT);
	if (strEQ(d,"waitpid"))
	    FUN2(O_WAITPID);
	if (strEQ(d,"wantarray")) {
	    yylval.arg = op_new(1);
	    yylval.arg->arg_type = O_ITEM;
	    yylval.arg[1].arg_type = A_WANTARRAY;
	    TERM(RSTRING);
	}
	if (strEQ(d,"write"))
	    FOP(O_WRITE);
	break;
    case 'x': case 'X':
	if (*s == 'x' && isDIGIT(s[1]) && !expectterm) {
	    s++;
	    MOP(O_REPEAT);
	}
	SNARFWORD;
	if (strEQ(d,"x")) {
	    if (!expectterm)
		MOP(O_REPEAT);
	    check_uni();
	}
	break;
    case 'y': case 'Y':
	if (s[1] == '\'') {
	    d = "y";
	    s++;
	}
	else {
	    SNARFWORD;
	}
	if (strEQ(d,"y")) {
	    s = scantrans(s);
	    TERM(TRANS);
	}
	break;
    case 'z': case 'Z':
	SNARFWORD;
	break;
    }
    yylval.cval = savestr(d);
    if (expectterm == 2) {		/* special case: start of statement */
	while (isSPACE(*s)) s++;
	if (*s == ':') {
	    s++;
	    CLINE;
	    OPERATOR(LABEL);
	}
	TERM(WORD);
    }
    expectterm = FALSE;
    if (oldoldbufptr && oldoldbufptr < bufptr) {
	while (isSPACE(*oldoldbufptr))
	    oldoldbufptr++;
	if (*oldoldbufptr == 'p' && strnEQ(oldoldbufptr,"print",5))
	    expectterm = TRUE;
	else if (*oldoldbufptr == 's' && strnEQ(oldoldbufptr,"sort",4))
	    expectterm = TRUE;
    }
    return (CLINE, bufptr = s, (int)WORD);
}

void
checkcomma(s,name,what)
register char *s;
char *name;
char *what;
{
    char *w;

    if (dowarn && *s == ' ' && s[1] == '(') {
	w = index(s,')');
	if (w)
	    for (w++; *w && isSPACE(*w); w++) ;
	if (!w || !*w || !index(";|}", *w))	/* an advisory hack only... */
	    warn("%s (...) interpreted as function",name);
    }
    while (s < bufend && isSPACE(*s))
	s++;
    if (*s == '(')
	s++;
    while (s < bufend && isSPACE(*s))
	s++;
    if (isALPHA(*s) || *s == '_') {
	w = s++;
	while (isALNUM(*s))
	    s++;
	while (s < bufend && isSPACE(*s))
	    s++;
	if (*s == ',') {
	    *s = '\0';
	    w = instr(
	      "tell eof times getlogin wait length shift umask getppid \
	      cos exp int log rand sin sqrt ord wantarray",
	      w);
	    *s = ',';
	    if (w)
		return;
	    fatal("No comma allowed after %s", what);
	}
    }
}

char *
scanident(s,send,dest,destlen)
register char *s;
register char *send;
char *dest;
STRLEN destlen;
{
    register char *d;
    register char *e;
    int brackets = 0;

    reparse = Nullch;
    s++;
    d = dest;
    e = d + destlen - 3;	/* two-character token, ending NUL */
    if (isDIGIT(*s)) {
	while (isDIGIT(*s)) {
	    if (d >= e)
		fatal("Identifier too long");
            *d++ = *s++;
	}
    }
    else {
	while (isALNUM(*s) || *s == '\'') {
	    if (d >= e)
		fatal("Identifier too long");
	    *d++ = *s++;
	}
    }
    while (d > dest+1 && d[-1] == '\'')
	d--,s--;
    *d = '\0';
    d = dest;
    if (!*d) {
	*d = *s++;
	if (*d == '{' /* } */ ) {
	    d = dest;
	    brackets++;
	    while (s < send && brackets) {
		if (!reparse && (d == dest || (*s && isALNUM(*s) ))) {
		    *d++ = *s++;
		    continue;
		}
		else if (!reparse)
		    reparse = s;
		switch (*s++) {
		/* { */
		case '}':
		    brackets--;
		    if (reparse && reparse == s - 1)
			reparse = Nullch;
		    break;
		case '{':   /* } */
		    brackets++;
		    break;
		}
	    }
	    *d = '\0';
	    d = dest;
	}
	else
	    d[1] = '\0';
    }
    if (*d == '^' && (isUPPER(*s) || index("[\\]^_?", *s))) {
#ifdef DEBUGGING
	if (*s == 'D')
	    debug |= 32768;
#endif
	*d = *s++ ^ 64;
    }
    return s;
}

void
scanconst(spat,string,len)
SPAT *spat;
char *string;
int len;
{
    register STR *tmpstr;
    register char *t;
    register char *d;
    register char *e;
    char *origstring = string;
    static char *vert = "|";

    if (ninstr(string, string+len, vert, vert+1))
	return;
    if (*string == '^')
	string++, len--;
    tmpstr = Str_new(86,len);
    str_nset(tmpstr,string,len);
    t = str_get(tmpstr);
    e = t + len;
    tmpstr->str_u.str_useful = 100;
    for (d=t; d < e; ) {
	switch (*d) {
	case '{':
	    if (isDIGIT(d[1]))
		e = d;
	    else
		goto defchar;
	    break;
	case '.': case '[': case '$': case '(': case ')': case '|': case '+':
	case '^':
	    e = d;
	    break;
	case '\\':
	    if (d[1] && index("wWbB0123456789sSdDlLuUExc",d[1])) {
		e = d;
		break;
	    }
	    Move(d+1,d,e-d,char);
	    e--;
	    switch(*d) {
	    case 'n':
		*d = '\n';
		break;
	    case 't':
		*d = '\t';
		break;
	    case 'f':
		*d = '\f';
		break;
	    case 'r':
		*d = '\r';
		break;
	    case 'e':
		*d = '\033';
		break;
	    case 'a':
		*d = '\007';
		break;
	    }
	    /* FALL THROUGH */
	default:
	  defchar:
	    if (d[1] == '*' || (d[1] == '{' && d[2] == '0') || d[1] == '?') {
		e = d;
		break;
	    }
	    d++;
	}
    }
    if (d == t) {
	str_free(tmpstr);
	return;
    }
    *d = '\0';
    tmpstr->str_cur = d - t;
    if (d == t+len)
	spat->spat_flags |= SPAT_ALL;
    if (*origstring != '^')
	spat->spat_flags |= SPAT_SCANFIRST;
    spat->spat_short = tmpstr;
    spat->spat_slen = d - t;
}

char *
scanpat(s)
register char *s;
{
    register SPAT *spat;
    register char *d;
    register char *e;
    int len;
    SPAT savespat;
    STR *str = Str_new(93,0);
    char delim;

    Newz(801,spat,1,SPAT);
    spat->spat_next = curstash->tbl_spatroot;	/* link into spat list */
    curstash->tbl_spatroot = spat;

    switch (*s++) {
    case 'm':
	s++;
	break;
    case '/':
	break;
    case '?':
	spat->spat_flags |= SPAT_ONCE;
	break;
    default:
	fatal("panic: scanpat");
    }
    s = str_append_till(str,s,bufend,s[-1],patleave);
    if (s >= bufend) {
	str_free(str);
	yyerror("Search pattern not terminated");
	yylval.arg = Nullarg;
	return s;
    }
    delim = *s++;
    while (*s == 'i' || *s == 'o' || *s == 'g') {
	if (*s == 'i') {
	    s++;
	    sawi = TRUE;
	    spat->spat_flags |= SPAT_FOLD;
	}
	if (*s == 'o') {
	    s++;
	    spat->spat_flags |= SPAT_KEEP;
	}
	if (*s == 'g') {
	    s++;
	    spat->spat_flags |= SPAT_GLOBAL;
	}
    }
    len = str->str_cur;
    e = str->str_ptr + len;
    if (delim == '\'')
	d = e;
    else
	d = str->str_ptr;
    for (; d < e; d++) {
	if (*d == '\\')
	    d++;
	else if ((*d == '$' && d[1] && d[1] != '|' && d[1] != ')') ||
		 (*d == '@')) {
	    register ARG *arg;

	    spat->spat_runtime = arg = op_new(1);
	    arg->arg_type = O_ITEM;
	    arg[1].arg_type = A_DOUBLE;
	    arg[1].arg_ptr.arg_str = str_smake(str);
	    d = scanident(d,bufend,buf,sizeof buf);
	    (void)stabent(buf,TRUE);		/* make sure it's created */
	    for (; d < e; d++) {
		if (*d == '\\')
		    d++;
		else if (*d == '$' && d[1] && d[1] != '|' && d[1] != ')') {
		    d = scanident(d,bufend,buf,sizeof buf);
		    (void)stabent(buf,TRUE);
		}
		else if (*d == '@') {
		    d = scanident(d,bufend,buf,sizeof buf);
		    if (strEQ(buf,"ARGV") || strEQ(buf,"ENV") ||
		      strEQ(buf,"SIG") || strEQ(buf,"INC"))
			(void)stabent(buf,TRUE);
		}
	    }
	    goto got_pat;		/* skip compiling for now */
	}
    }
    if (spat->spat_flags & SPAT_FOLD)
	StructCopy(spat, &savespat, SPAT);
    scanconst(spat,str->str_ptr,len);
    if ((spat->spat_flags & SPAT_ALL) && (spat->spat_flags & SPAT_SCANFIRST)) {
	fbmcompile(spat->spat_short, spat->spat_flags & SPAT_FOLD);
	spat->spat_regexp = regcomp(str->str_ptr,str->str_ptr+len,
	    spat->spat_flags & SPAT_FOLD);
		/* Note that this regexp can still be used if someone says
		 * something like /a/ && s//b/;  so we can't delete it.
		 */
    }
    else {
	if (spat->spat_flags & SPAT_FOLD)
	StructCopy(&savespat, spat, SPAT);
	if (spat->spat_short)
	    fbmcompile(spat->spat_short, spat->spat_flags & SPAT_FOLD);
	spat->spat_regexp = regcomp(str->str_ptr,str->str_ptr+len,
	    spat->spat_flags & SPAT_FOLD);
	hoistmust(spat);
    }
  got_pat:
    str_free(str);
    yylval.arg = make_match(O_MATCH,stab2arg(A_STAB,defstab),spat);
    return s;
}

char *
scansubst(start)
char *start;
{
    register char *s = start;
    register SPAT *spat;
    register char *d;
    register char *e;
    int len;
    STR *str = Str_new(93,0);
    char term = *s;

    if (term && (d = index("([{< )]}> )]}>",term)))
	term = d[5];

    Newz(802,spat,1,SPAT);
    spat->spat_next = curstash->tbl_spatroot;	/* link into spat list */
    curstash->tbl_spatroot = spat;

    s = str_append_till(str,s+1,bufend,term,patleave);
    if (s >= bufend) {
	str_free(str);
	yyerror("Substitution pattern not terminated");
	yylval.arg = Nullarg;
	return s;
    }
    len = str->str_cur;
    e = str->str_ptr + len;
    for (d = str->str_ptr; d < e; d++) {
	if (*d == '\\')
	    d++;
	else if ((*d == '$' && d[1] && d[1] != '|' && /*(*/ d[1] != ')') ||
	    *d == '@' ) {
	    register ARG *arg;

	    spat->spat_runtime = arg = op_new(1);
	    arg->arg_type = O_ITEM;
	    arg[1].arg_type = A_DOUBLE;
	    arg[1].arg_ptr.arg_str = str_smake(str);
	    d = scanident(d,e,buf,sizeof buf);
	    (void)stabent(buf,TRUE);		/* make sure it's created */
	    for (; *d; d++) {
		if (*d == '$' && d[1] && d[-1] != '\\' && d[1] != '|') {
		    d = scanident(d,e,buf,sizeof buf);
		    (void)stabent(buf,TRUE);
		}
		else if (*d == '@' && d[-1] != '\\') {
		    d = scanident(d,e,buf,sizeof buf);
		    if (strEQ(buf,"ARGV") || strEQ(buf,"ENV") ||
		      strEQ(buf,"SIG") || strEQ(buf,"INC"))
			(void)stabent(buf,TRUE);
		}
	    }
	    goto get_repl;		/* skip compiling for now */
	}
    }
    scanconst(spat,str->str_ptr,len);
get_repl:
    if (term != *start)
	s++;
    s = scanstr(s, SCAN_REPL);
    if (s >= bufend) {
	str_free(str);
	yyerror("Substitution replacement not terminated");
	yylval.arg = Nullarg;
	return s;
    }
    spat->spat_repl = yylval.arg;
    if ((spat->spat_repl[1].arg_type & A_MASK) == A_SINGLE)
	spat->spat_flags |= SPAT_CONST;
    else if ((spat->spat_repl[1].arg_type & A_MASK) == A_DOUBLE) {
	STR *tmpstr;
	register char *t;

	spat->spat_flags |= SPAT_CONST;
	tmpstr = spat->spat_repl[1].arg_ptr.arg_str;
	e = tmpstr->str_ptr + tmpstr->str_cur;
	for (t = tmpstr->str_ptr; t < e; t++) {
	    if (*t == '$' && t[1] && (index("`'&+0123456789",t[1]) ||
	      (t[1] == '{' /*}*/ && isDIGIT(t[2])) ))
		spat->spat_flags &= ~SPAT_CONST;
	}
    }
    while (*s == 'g' || *s == 'i' || *s == 'e' || *s == 'o') {
	int es = 0;

	if (*s == 'e') {
	    s++;
	    es++;
	    if ((spat->spat_repl[1].arg_type & A_MASK) == A_DOUBLE)
		spat->spat_repl[1].arg_type = A_SINGLE;
	    spat->spat_repl = make_op(
		(!es && spat->spat_repl[1].arg_type == A_SINGLE
			? O_EVALONCE
			: O_EVAL),
		2,
		spat->spat_repl,
		Nullarg,
		Nullarg);
	    spat->spat_flags &= ~SPAT_CONST;
	}
	if (*s == 'g') {
	    s++;
	    spat->spat_flags |= SPAT_GLOBAL;
	}
	if (*s == 'i') {
	    s++;
	    sawi = TRUE;
	    spat->spat_flags |= SPAT_FOLD;
	    if (!(spat->spat_flags & SPAT_SCANFIRST)) {
		str_free(spat->spat_short);	/* anchored opt doesn't do */
		spat->spat_short = Nullstr;	/* case insensitive match */
		spat->spat_slen = 0;
	    }
	}
	if (*s == 'o') {
	    s++;
	    spat->spat_flags |= SPAT_KEEP;
	}
    }
    if (spat->spat_short && (spat->spat_flags & SPAT_SCANFIRST))
	fbmcompile(spat->spat_short, spat->spat_flags & SPAT_FOLD);
    if (!spat->spat_runtime) {
	spat->spat_regexp = regcomp(str->str_ptr,str->str_ptr+len,
	  spat->spat_flags & SPAT_FOLD);
	hoistmust(spat);
    }
    yylval.arg = make_match(O_SUBST,stab2arg(A_STAB,defstab),spat);
    str_free(str);
    return s;
}

void
hoistmust(spat)
register SPAT *spat;
{
    if (!spat->spat_short && spat->spat_regexp->regstart &&
	(!spat->spat_regexp->regmust || spat->spat_regexp->reganch & ROPT_ANCH)
       ) {
	if (!(spat->spat_regexp->reganch & ROPT_ANCH))
	    spat->spat_flags |= SPAT_SCANFIRST;
	else if (spat->spat_flags & SPAT_FOLD)
	    return;
	spat->spat_short = str_smake(spat->spat_regexp->regstart);
    }
    else if (spat->spat_regexp->regmust) {/* is there a better short-circuit? */
	if (spat->spat_short &&
	  str_eq(spat->spat_short,spat->spat_regexp->regmust))
	{
	    if (spat->spat_flags & SPAT_SCANFIRST) {
		str_free(spat->spat_short);
		spat->spat_short = Nullstr;
	    }
	    else {
		str_free(spat->spat_regexp->regmust);
		spat->spat_regexp->regmust = Nullstr;
		return;
	    }
	}
	if (!spat->spat_short ||	/* promote the better string */
	  ((spat->spat_flags & SPAT_SCANFIRST) &&
	   (spat->spat_short->str_cur < spat->spat_regexp->regmust->str_cur) )){
	    str_free(spat->spat_short);		/* ok if null */
	    spat->spat_short = spat->spat_regexp->regmust;
	    spat->spat_regexp->regmust = Nullstr;
	    spat->spat_flags |= SPAT_SCANFIRST;
	}
    }
}

char *
scantrans(start)
char *start;
{
    register char *s = start;
    ARG *arg =
	l(make_op(O_TRANS,2,stab2arg(A_STAB,defstab),Nullarg,Nullarg));
    STR *tstr;
    STR *rstr;
    register char *t;
    register char *r;
    register short *tbl;
    register int i;
    register int j;
    int tlen, rlen;
    int squash;
    int delete;
    int complement;

    New(803,tbl,256,short);
    arg[2].arg_type = A_NULL;
    arg[2].arg_ptr.arg_cval = (char*) tbl;

    s = scanstr(s, SCAN_TR);
    if (s >= bufend) {
	yyerror("Translation pattern not terminated");
	yylval.arg = Nullarg;
	return s;
    }
    tstr = yylval.arg[1].arg_ptr.arg_str;
    yylval.arg[1].arg_ptr.arg_str = Nullstr;
    arg_free(yylval.arg);
    t = tstr->str_ptr;
    tlen = tstr->str_cur;

    if (s[-1] == *start)
	s--;

    s = scanstr(s, SCAN_TR|SCAN_REPL);
    if (s >= bufend) {
	yyerror("Translation replacement not terminated");
	yylval.arg = Nullarg;
	return s;
    }
    rstr = yylval.arg[1].arg_ptr.arg_str;
    yylval.arg[1].arg_ptr.arg_str = Nullstr;
    arg_free(yylval.arg);
    r = rstr->str_ptr;
    rlen = rstr->str_cur;

    complement = delete = squash = 0;
    while (*s == 'c' || *s == 'd' || *s == 's') {
	if (*s == 'c')
	    complement = 1;
	else if (*s == 'd')
	    delete = 2;
	else
	    squash = 1;
	s++;
    }
    arg[2].arg_len = delete|squash;
    yylval.arg = arg;
    if (complement) {
	Zero(tbl, 256, short);
	for (i = 0; i < tlen; i++)
	    tbl[t[i] & 0377] = -1;
	for (i = 0, j = 0; i < 256; i++) {
	    if (!tbl[i]) {
		if (j >= rlen) {
		    if (delete)
			tbl[i] = -2;
		    else if (rlen)
			tbl[i] = r[j-1] & 0377;
		    else
			tbl[i] = i;
		}
		else
		    tbl[i] = r[j++] & 0377;
	    }
	}
    }
    else {
	if (!rlen && !delete) {
	    r = t; rlen = tlen;
	}
	for (i = 0; i < 256; i++)
	    tbl[i] = -1;
	for (i = 0, j = 0; i < tlen; i++,j++) {
	    if (j >= rlen) {
		if (delete) {
		    if (tbl[t[i] & 0377] == -1)
			tbl[t[i] & 0377] = -2;
		    continue;
		}
		--j;
	    }
	    if (tbl[t[i] & 0377] == -1)
		tbl[t[i] & 0377] = r[j] & 0377;
	}
    }
    str_free(tstr);
    str_free(rstr);
    return s;
}

char *
scanstr(start, in_what)
char *start;
int in_what;
{
    register char *s = start;
    register char term;
    register char *d;
    register ARG *arg;
    register char *send;
    register bool makesingle = FALSE;
    register STAB *stab;
    bool alwaysdollar = FALSE;
    bool hereis = FALSE;
    STR *herewas;
    STR *str;
    /* which backslash sequences to keep */
    char *leave = (in_what & SCAN_TR)
	? "\\$@nrtfbeacx0123456789-"
	: "\\$@nrtfbeacx0123456789[{]}lLuUE";
    int len;

    arg = op_new(1);
    yylval.arg = arg;
    arg->arg_type = O_ITEM;

    switch (*s) {
    default:			/* a substitution replacement */
	arg[1].arg_type = A_DOUBLE;
	makesingle = TRUE;	/* maybe disable runtime scanning */
	term = *s;
	if (term == '\'')
	    leave = Nullch;
	goto snarf_it;
    case '0':
	{
	    unsigned long i;
	    int shift;

	    arg[1].arg_type = A_SINGLE;
	    if (s[1] == 'x') {
		shift = 4;
		s += 2;
	    }
	    else if (s[1] == '.')
		goto decimal;
	    else
		shift = 3;
	    i = 0;
	    for (;;) {
		switch (*s) {
		default:
		    goto out;
		case '_':
		    s++;
		    break;
		case '8': case '9':
		    if (shift != 4)
			yyerror("Illegal octal digit");
		    /* FALL THROUGH */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
		    i <<= shift;
		    i += *s++ & 15;
		    break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		    if (shift != 4)
			goto out;
		    i <<= 4;
		    i += (*s++ & 7) + 9;
		    break;
		}
	    }
	  out:
	    str = Str_new(92,0);
	    str_numset(str,(double)i);
	    if (str->str_ptr) {
		Safefree(str->str_ptr);
		str->str_ptr = Nullch;
		str->str_len = str->str_cur = 0;
	    }
	    arg[1].arg_ptr.arg_str = str;
	}
	break;
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '.':
      decimal:
	arg[1].arg_type = A_SINGLE;
	d = tokenbuf;
	while (isDIGIT(*s) || *s == '_') {
	    if (*s == '_')
		s++;
	    else
		*d++ = *s++;
	}
	if (*s == '.' && s[1] != '.') {
	    *d++ = *s++;
	    while (isDIGIT(*s) || *s == '_') {
		if (*s == '_')
		    s++;
		else
		    *d++ = *s++;
	    }
	}
	if (*s && index("eE",*s) && index("+-0123456789",s[1])) {
	    *d++ = *s++;
	    if (*s == '+' || *s == '-')
		*d++ = *s++;
	    while (isDIGIT(*s))
		*d++ = *s++;
	}
	*d = '\0';
	str = Str_new(92,0);
	str_numset(str,atof(tokenbuf));
	if (str->str_ptr) {
	    Safefree(str->str_ptr);
	    str->str_ptr = Nullch;
	    str->str_len = str->str_cur = 0;
	}
	arg[1].arg_ptr.arg_str = str;
	break;
    case '<':
	if (in_what & (SCAN_REPL|SCAN_TR))
	    goto do_double;
	if (*++s == '<') {
	    hereis = TRUE;
	    d = tokenbuf;
	    if (!rsfp)
		*d++ = '\n';
	    if (*++s && index("`'\"",*s)) {
		term = *s++;
		s = cpytill(d,s,bufend,term,&len);
		if (s < bufend)
		    s++;
		d += len;
	    }
	    else {
		if (*s == '\\')
		    s++, term = '\'';
		else
		    term = '"';
		while (isALNUM(*s))
		    *d++ = *s++;
	    }				/* assuming tokenbuf won't clobber */
	    *d++ = '\n';
	    *d = '\0';
	    len = d - tokenbuf;
	    d = "\n";
	    if (rsfp || !(d=ninstr(s,bufend,d,d+1)))
		herewas = str_make(s,bufend-s);
	    else
		s--, herewas = str_make(s,d-s);
	    s += herewas->str_cur;
	    if (term == '\'')
		goto do_single;
	    if (term == '`')
		goto do_back;
	    goto do_double;
	}
	d = tokenbuf;
	s = cpytill(d,s,bufend,'>',&len);
	if (s < bufend)
	    s++;
	else
	    fatal("Unterminated <> operator");

	if (*d == '$') d++;
	while (*d && (isALNUM(*d) || *d == '\''))
	    d++;
	if (d - tokenbuf != len) {
	    s = start;
	    term = *s;
	    arg[1].arg_type = A_GLOB;
	    set_csh();
	    alwaysdollar = TRUE;	/* treat $) and $| as variables */
	    goto snarf_it;
	}
	else {
	    d = tokenbuf;
	    if (!len)
		(void)strcpy(d,"ARGV");
	    if (*d == '$') {
		arg[1].arg_type = A_INDREAD;
		arg[1].arg_ptr.arg_stab = stabent(d+1,TRUE);
	    }
	    else {
		arg[1].arg_type = A_READ;
		arg[1].arg_ptr.arg_stab = stabent(d,TRUE);
		if (!stab_io(arg[1].arg_ptr.arg_stab))
		    stab_io(arg[1].arg_ptr.arg_stab) = stio_new();
		if (strEQ(d,"ARGV")) {
		    (void)aadd(arg[1].arg_ptr.arg_stab);
		    stab_io(arg[1].arg_ptr.arg_stab)->flags |=
		      IOF_ARGV|IOF_START;
		}
	    }
	}
	break;

    case 'q':
	s++;
	if (*s == 'q') {
	    s++;
	    goto do_double;
	}
	if (*s == 'x') {
	    s++;
	    goto do_back;
	}
	/* FALL THROUGH */
    case '\'':
      do_single:
	term = *s;
	arg[1].arg_type = A_SINGLE;
	leave = Nullch;
	goto snarf_it;

    case '"':
      do_double:
	term = *s;
	arg[1].arg_type = A_DOUBLE;
	makesingle = TRUE;	/* maybe disable runtime scanning */
	alwaysdollar = TRUE;	/* treat $) and $| as variables */
	goto snarf_it;
    case '`':
      do_back:
	term = *s;
	arg[1].arg_type = A_BACKTICK;
	set_csh();
	alwaysdollar = TRUE;	/* treat $) and $| as variables */
      snarf_it:
	{
	    STR *tmpstr;
	    STR *tmpstr2 = Nullstr;
	    char *tmps;
	    char *start;
	    bool dorange = FALSE;

	    CLINE;
	    multi_start = curcmd->c_line;
	    if (hereis)
		multi_open = multi_close = '<';
	    else {
		multi_open = term;
		if (term && (tmps = index("([{< )]}> )]}>",term)))
		    term = tmps[5];
		multi_close = term;
	    }
	    tmpstr = Str_new(87,80);
	    if (hereis) {
		term = *tokenbuf;
		if (!rsfp) {
		    d = s;
		    while (s < bufend &&
		      (*s != term || bcmp(s,tokenbuf,len) != 0) ) {
			if (*s++ == '\n')
			    curcmd->c_line++;
		    }
		    if (s >= bufend) {
			curcmd->c_line = multi_start;
			fatal("EOF in string");
		    }
		    str_nset(tmpstr,d+1,s-d);
		    s += len - 1;
		    str_ncat(herewas,s,bufend-s);
		    str_replace(linestr,herewas);
		    oldoldbufptr = oldbufptr = bufptr = s = str_get(linestr);
		    bufend = linestr->str_ptr + linestr->str_cur;
		    hereis = FALSE;
		}
		else
		    str_nset(tmpstr,"",0);   /* avoid "uninitialized" warning */
	    }
	    else
		s = str_append_till(tmpstr,s+1,bufend,term,leave);
	    while (s >= bufend) {	/* multiple line string? */
		if (!rsfp ||
		 !(oldoldbufptr = oldbufptr = s = str_gets(linestr, rsfp, 0))) {
		    curcmd->c_line = multi_start;
		    fatal("EOF in string");
		}
		curcmd->c_line++;
		if (perldb) {
		    STR *str = Str_new(88,0);

		    str_sset(str,linestr);
		    astore(stab_xarray(curcmd->c_filestab),
		      (int)curcmd->c_line,str);
		}
		bufend = linestr->str_ptr + linestr->str_cur;
		if (hereis) {
		    if (*s == term && bcmp(s,tokenbuf,len) == 0) {
			s = bufend - 1;
			*s = ' ';
			str_scat(linestr,herewas);
			bufend = linestr->str_ptr + linestr->str_cur;
		    }
		    else {
			s = bufend;
			str_scat(tmpstr,linestr);
		    }
		}
		else
		    s = str_append_till(tmpstr,s,bufend,term,leave);
	    }
	    multi_end = curcmd->c_line;
	    s++;
	    if (tmpstr->str_cur + 5 < tmpstr->str_len) {
		tmpstr->str_len = tmpstr->str_cur + 1;
		Renew(tmpstr->str_ptr, tmpstr->str_len, char);
	    }
	    if (arg[1].arg_type == A_SINGLE) {
		arg[1].arg_ptr.arg_str = tmpstr;
		break;
	    }
	    tmps = s;
	    s = tmpstr->str_ptr;
	    send = s + tmpstr->str_cur;
	    while (s < send) {		/* see if we can make SINGLE */
		if (*s == '\\' && s[1] && isDIGIT(s[1]) && !isDIGIT(s[2]) &&
		  !alwaysdollar && s[1] != '0')
		    *s = '$';		/* grandfather \digit in subst */
		if ((*s == '$' || *s == '@') && s+1 < send &&
		  (alwaysdollar || (s[1] != ')' && s[1] != '|'))) {
		    makesingle = FALSE;	/* force interpretation */
		}
		else if (*s == '\\' && s+1 < send) {
		    if (index("lLuUE",s[1]))
			makesingle = FALSE;
		    s++;
		}
		s++;
	    }
	    s = d = start = tmpstr->str_ptr;	/* assuming shrinkage only */
	    while (s < send || dorange) {
		if (in_what & SCAN_TR) {
		    if (dorange) {
			int i;
			int max;
			if (!tmpstr2) {	/* oops, have to grow */
			    tmpstr2 = str_smake(tmpstr);
			    s = tmpstr2->str_ptr + (s - tmpstr->str_ptr);
			    send = tmpstr2->str_ptr + (send - tmpstr->str_ptr);
			}
			i = d - tmpstr->str_ptr;
			STR_GROW(tmpstr, tmpstr->str_len + 256);
			d = tmpstr->str_ptr + i;
			d -= 2;
			max = d[1] & 0377;
			for (i = (*d & 0377); i <= max; i++)
			    *d++ = i;
			start = s;
			dorange = FALSE;
			continue;
		    }
		    else if (*s == '-' && s+1 < send  && s != start) {
			dorange = TRUE;
			s++;
		    }
		}
		else {
		    if ((*s == '$' && s+1 < send &&
			(alwaysdollar || /*(*/(s[1] != ')' && s[1] != '|')) ) ||
			(*s == '@' && s+1 < send) ) {
			if (s[1] == '#' && (isALPHA(s[2]) || s[2] == '_'))
			    *d++ = *s++;
			len = scanident(s,send,tokenbuf,sizeof tokenbuf) - s;
			if (*s == '$' || strEQ(tokenbuf,"ARGV")
			  || strEQ(tokenbuf,"ENV")
			  || strEQ(tokenbuf,"SIG")
			  || strEQ(tokenbuf,"INC") )
			    (void)stabent(tokenbuf,TRUE); /* add symbol */
			while (len--)
			    *d++ = *s++;
			continue;
		    }
		}
		if (*s == '\\' && s+1 < send) {
		    s++;
		    switch (*s) {
		    case '-':
			if (in_what & SCAN_TR) {
			    *d++ = *s++;
			    continue;
			}
			/* FALL THROUGH */
		    default:
			if (!makesingle && (!leave || (*s && index(leave,*s))))
			    *d++ = '\\';
			*d++ = *s++;
			continue;
		    case '0': case '1': case '2': case '3':
		    case '4': case '5': case '6': case '7':
			*d++ = scanoct(s, 3, &len);
			s += len;
			continue;
		    case 'x':
			*d++ = scanhex(++s, 2, &len);
			s += len;
			continue;
		    case 'c':
			s++;
			*d = *s++;
			if (isLOWER(*d))
			    *d = toupper(*d);
			*d++ ^= 64;
			continue;
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
		    }
		    s++;
		    continue;
		}
		*d++ = *s++;
	    }
	    *d = '\0';

	    if (arg[1].arg_type == A_DOUBLE && makesingle)
		arg[1].arg_type = A_SINGLE;	/* now we can optimize on it */

	    tmpstr->str_cur = d - tmpstr->str_ptr;
	    if (arg[1].arg_type == A_GLOB) {
		arg[1].arg_ptr.arg_stab = stab = genstab();
		stab_io(stab) = stio_new();
		str_sset(stab_val(stab), tmpstr);
	    }
	    else
		arg[1].arg_ptr.arg_str = tmpstr;
	    s = tmps;
	    if (tmpstr2)
		str_free(tmpstr2);
	    break;
	}
    }
    if (hereis)
	str_free(herewas);
    return s;
}

FCMD *
load_format()
{
    FCMD froot;
    FCMD *flinebeg;
    char *eol;
    register FCMD *fprev = &froot;
    register FCMD *fcmd;
    register char *s;
    register char *t;
    register STR *str;
    bool noblank;
    bool repeater;

    Zero(&froot, 1, FCMD);
    s = bufptr;
    while (s < bufend || (rsfp && (s = str_gets(linestr,rsfp, 0)) != Nullch)) {
	curcmd->c_line++;
	if (in_eval && !rsfp) {
	    eol = index(s,'\n');
	    if (!eol++)
		eol = bufend;
	}
	else
	    eol = bufend = linestr->str_ptr + linestr->str_cur;
	if (perldb) {
	    STR *tmpstr = Str_new(89,0);

	    str_nset(tmpstr, s, eol-s);
	    astore(stab_xarray(curcmd->c_filestab), (int)curcmd->c_line,tmpstr);
	}
	if (*s == '.') {
	    /*SUPPRESS 530*/
	    for (t = s+1; *t == ' ' || *t == '\t'; t++) ;
	    if (*t == '\n') {
		bufptr = s;
		return froot.f_next;
	    }
	}
	if (*s == '#') {
	    s = eol;
	    continue;
	}
	flinebeg = Nullfcmd;
	noblank = FALSE;
	repeater = FALSE;
	while (s < eol) {
	    Newz(804,fcmd,1,FCMD);
	    fprev->f_next = fcmd;
	    fprev = fcmd;
	    for (t=s; t < eol && *t != '@' && *t != '^'; t++) {
		if (*t == '~') {
		    noblank = TRUE;
		    *t = ' ';
		    if (t[1] == '~') {
			repeater = TRUE;
			t[1] = ' ';
		    }
		}
	    }
	    fcmd->f_pre = nsavestr(s, t-s);
	    fcmd->f_presize = t-s;
	    s = t;
	    if (s >= eol) {
		if (noblank)
		    fcmd->f_flags |= FC_NOBLANK;
		if (repeater)
		    fcmd->f_flags |= FC_REPEAT;
		break;
	    }
	    if (!flinebeg)
		flinebeg = fcmd;		/* start values here */
	    if (*s++ == '^')
		fcmd->f_flags |= FC_CHOP;	/* for doing text filling */
	    switch (*s) {
	    case '*':
		fcmd->f_type = F_LINES;
		*s = '\0';
		break;
	    case '<':
		fcmd->f_type = F_LEFT;
		while (*s == '<')
		    s++;
		break;
	    case '>':
		fcmd->f_type = F_RIGHT;
		while (*s == '>')
		    s++;
		break;
	    case '|':
		fcmd->f_type = F_CENTER;
		while (*s == '|')
		    s++;
		break;
	    case '#':
	    case '.':
		/* Catch the special case @... and handle it as a string
		   field. */
		if (*s == '.' && s[1] == '.') {
		    goto default_format;
		}
		fcmd->f_type = F_DECIMAL;
		{
		    char *p;

		    /* Read a format in the form @####.####, where either group
		       of ### may be empty, or the final .### may be missing. */
		    while (*s == '#')
			s++;
		    if (*s == '.') {
			s++;
			p = s;
			while (*s == '#')
			    s++;
			fcmd->f_decimals = s-p;
			fcmd->f_flags |= FC_DP;
		    } else {
			fcmd->f_decimals = 0;
		    }
		}
		break;
	    default:
	    default_format:
		fcmd->f_type = F_LEFT;
		break;
	    }
	    if (fcmd->f_flags & FC_CHOP && *s == '.') {
		fcmd->f_flags |= FC_MORE;
		while (*s == '.')
		    s++;
	    }
	    fcmd->f_size = s-t;
	}
	if (flinebeg) {
	  again:
	    if (s >= bufend &&
	      (!rsfp || (s = str_gets(linestr, rsfp, 0)) == Nullch) )
		goto badform;
	    curcmd->c_line++;
	    if (in_eval && !rsfp) {
		eol = index(s,'\n');
		if (!eol++)
		    eol = bufend;
	    }
	    else
		eol = bufend = linestr->str_ptr + linestr->str_cur;
	    if (perldb) {
		STR *tmpstr = Str_new(90,0);

		str_nset(tmpstr, s, eol-s);
		astore(stab_xarray(curcmd->c_filestab),
		    (int)curcmd->c_line,tmpstr);
	    }
	    if (strnEQ(s,".\n",2)) {
		bufptr = s;
		yyerror("Missing values line");
		return froot.f_next;
	    }
	    if (*s == '#') {
		s = eol;
		goto again;
	    }
	    str = flinebeg->f_unparsed = Str_new(91,eol - s);
	    str->str_u.str_hash = curstash;
	    str_nset(str,"(",1);
	    flinebeg->f_line = curcmd->c_line;
	    eol[-1] = '\0';
	    if (!flinebeg->f_next->f_type || index(s, ',')) {
		eol[-1] = '\n';
		str_ncat(str, s, eol - s - 1);
		str_ncat(str,",$$);",5);
		s = eol;
	    }
	    else {
		eol[-1] = '\n';
		while (s < eol && isSPACE(*s))
		    s++;
		t = s;
		while (s < eol) {
		    switch (*s) {
		    case ' ': case '\t': case '\n': case ';':
			str_ncat(str, t, s - t);
			str_ncat(str, "," ,1);
			while (s < eol && (isSPACE(*s) || *s == ';'))
			    s++;
			t = s;
			break;
		    case '$':
			str_ncat(str, t, s - t);
			t = s;
			s = scanident(s,eol,tokenbuf,sizeof tokenbuf);
			str_ncat(str, t, s - t);
			t = s;
			if (s < eol && *s && index("$'\"",*s))
			    str_ncat(str, ",", 1);
			break;
		    case '"': case '\'':
			str_ncat(str, t, s - t);
			t = s;
			s++;
			while (s < eol && (*s != *t || s[-1] == '\\'))
			    s++;
			if (s < eol)
			    s++;
			str_ncat(str, t, s - t);
			t = s;
			if (s < eol && *s && index("$'\"",*s))
			    str_ncat(str, ",", 1);
			break;
		    default:
			yyerror("Please use commas to separate fields");
		    }
		}
		str_ncat(str,"$$);",4);
	    }
	}
    }
  badform:
    bufptr = str_get(linestr);
    yyerror("Format not terminated");
    return froot.f_next;
}

static void
set_csh()
{
#ifdef CSH
    if (!cshlen)
	cshlen = strlen(cshname);
#endif
}
