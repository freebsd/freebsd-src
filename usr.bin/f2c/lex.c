/****************************************************************
Copyright 1990, 1992 - 1997 by AT&T, Lucent Technologies and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T, Bell Laboratories,
Lucent or Bellcore or any of their entities not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

AT&T, Lucent and Bellcore disclaim all warranties with regard to
this software, including all implied warranties of
merchantability and fitness.  In no event shall AT&T, Lucent or
Bellcore be liable for any special, indirect or consequential
damages or any damages whatsoever resulting from loss of use,
data or profits, whether in an action of contract, negligence or
other tortious action, arising out of or in connection with the
use or performance of this software.
****************************************************************/

#include "defs.h"
#include "tokdefs.h"
#include "p1defs.h"

#ifdef NO_EOF_CHAR_CHECK
#undef EOF_CHAR
#else
#ifndef EOF_CHAR
#define EOF_CHAR 26	/* ASCII control-Z */
#endif
#endif

#define BLANK	' '
#define MYQUOTE (2)
#define SEOF 0

/* card types */

#define STEOF 1
#define STINITIAL 2
#define STCONTINUE 3

/* lex states */

#define NEWSTMT	1
#define FIRSTTOKEN	2
#define OTHERTOKEN	3
#define RETEOS	4


LOCAL int stkey;	/* Type of the current statement (DO, END, IF, etc) */
static int needwkey;
ftnint yystno;
flag intonly;
extern int new_dcl;
LOCAL long int stno;
LOCAL long int nxtstno;	/* Statement label */
LOCAL int parlev;	/* Parentheses level */
LOCAL int parseen;
LOCAL int expcom;
LOCAL int expeql;
LOCAL char *nextch;
LOCAL char *lastch;
LOCAL char *nextcd 	= NULL;
LOCAL char *endcd;
LOCAL long prevlin;
LOCAL long thislin;
LOCAL int code;		/* Card type; INITIAL, CONTINUE or EOF */
LOCAL int lexstate	= NEWSTMT;
LOCAL char *sbuf;	/* Main buffer for Fortran source input. */
LOCAL char *send;	/* Was = sbuf+20*66 with sbuf[1390]. */
LOCAL int maxcont;
LOCAL int nincl	= 0;	/* Current number of include files */
LOCAL long firstline;
LOCAL char *laststb, *stb0;
extern int addftnsrc;
static char **linestart;
LOCAL int ncont;
LOCAL char comstart[Table_size];
#define USC (unsigned char *)

static char anum_buf[Table_size];
#define isalnum_(x) anum_buf[x]
#define isalpha_(x) (anum_buf[x] == 1)

#define COMMENT_BUF_STORE 4088

typedef struct comment_buf {
	struct comment_buf *next;
	char *last;
	char buf[COMMENT_BUF_STORE];
	} comment_buf;
static comment_buf *cbfirst, *cbcur;
static char *cbinit, *cbnext, *cblast;
static void flush_comments Argdcl((void));
extern flag use_bs;
static char *lastfile = "??", *lastfile0 = "?";
static char fbuf[P1_FILENAME_MAX];
static long lastline;
static void putlineno(Void);


/* Comment buffering data

	Comments are kept in a list until the statement before them has
   been parsed.  This list is implemented with the above comment_buf
   structure and the pointers cbnext and cblast.

	The comments are stored with terminating NULL, and no other
   intervening space.  The last few bytes of each block are likely to
   remain unused.
*/

/* struct Inclfile   holds the state information for each include file */
struct Inclfile
{
	struct Inclfile *inclnext;
	FILEP inclfp;
	char *inclname;
	int incllno;
	char *incllinp;
	int incllen;
	int inclcode;
	ftnint inclstno;
};

LOCAL struct Inclfile *inclp	=  NULL;
struct Keylist {
	char *keyname;
	int keyval;
	char notinf66;
};
struct Punctlist {
	char punchar;
	int punval;
};
struct Fmtlist {
	char fmtchar;
	int fmtval;
};
struct Dotlist {
	char *dotname;
	int dotval;
	};
LOCAL struct Keylist *keystart[26], *keyend[26];

/* KEYWORD AND SPECIAL CHARACTER TABLES
*/

static struct Punctlist puncts[ ] =
{
	'(', SLPAR,
	')', SRPAR,
	'=', SEQUALS,
	',', SCOMMA,
	'+', SPLUS,
	'-', SMINUS,
	'*', SSTAR,
	'/', SSLASH,
	'$', SCURRENCY,
	':', SCOLON,
	'<', SLT,
	'>', SGT,
	0, 0 };

LOCAL struct Dotlist  dots[ ] =
{
	"and.", SAND,
	    "or.", SOR,
	    "not.", SNOT,
	    "true.", STRUE,
	    "false.", SFALSE,
	    "eq.", SEQ,
	    "ne.", SNE,
	    "lt.", SLT,
	    "le.", SLE,
	    "gt.", SGT,
	    "ge.", SGE,
	    "neqv.", SNEQV,
	    "eqv.", SEQV,
	    0, 0 };

LOCAL struct Keylist  keys[ ] =
{
	{ "assign",  SASSIGN  },
	{ "automatic",  SAUTOMATIC, YES  },
	{ "backspace",  SBACKSPACE  },
	{ "blockdata",  SBLOCK  },
	{ "byte",	SBYTE	},
	{ "call",  SCALL  },
	{ "character",  SCHARACTER, YES  },
	{ "close",  SCLOSE, YES  },
	{ "common",  SCOMMON  },
	{ "complex",  SCOMPLEX  },
	{ "continue",  SCONTINUE  },
	{ "data",  SDATA  },
	{ "dimension",  SDIMENSION  },
	{ "doubleprecision",  SDOUBLE  },
	{ "doublecomplex", SDCOMPLEX, YES  },
	{ "elseif",  SELSEIF, YES  },
	{ "else",  SELSE, YES  },
	{ "endfile",  SENDFILE  },
	{ "endif",  SENDIF, YES  },
	{ "enddo", SENDDO, YES },
	{ "end",  SEND  },
	{ "entry",  SENTRY, YES  },
	{ "equivalence",  SEQUIV  },
	{ "external",  SEXTERNAL  },
	{ "format",  SFORMAT  },
	{ "function",  SFUNCTION  },
	{ "goto",  SGOTO  },
	{ "implicit",  SIMPLICIT, YES  },
	{ "include",  SINCLUDE, YES  },
	{ "inquire",  SINQUIRE, YES  },
	{ "intrinsic",  SINTRINSIC, YES  },
	{ "integer",  SINTEGER  },
	{ "logical",  SLOGICAL  },
	{ "namelist", SNAMELIST, YES },
	{ "none", SUNDEFINED, YES },
	{ "open",  SOPEN, YES  },
	{ "parameter",  SPARAM, YES  },
	{ "pause",  SPAUSE  },
	{ "print",  SPRINT  },
	{ "program",  SPROGRAM, YES  },
	{ "punch",  SPUNCH, YES  },
	{ "read",  SREAD  },
	{ "real",  SREAL  },
	{ "return",  SRETURN  },
	{ "rewind",  SREWIND  },
	{ "save",  SSAVE, YES  },
	{ "static",  SSTATIC, YES  },
	{ "stop",  SSTOP  },
	{ "subroutine",  SSUBROUTINE  },
	{ "then",  STHEN, YES  },
	{ "undefined", SUNDEFINED, YES  },
	{ "while", SWHILE, YES  },
	{ "write",  SWRITE  },
	{ 0, 0 }
};

static void analyz Argdcl((void));
static void crunch Argdcl((void));
static int getcd Argdcl((char*, int));
static int getcds Argdcl((void));
static int getkwd Argdcl((void));
static int gettok Argdcl((void));
static void store_comment Argdcl((char*));
LOCAL char *stbuf[3];

 int
#ifdef KR_headers
inilex(name)
	char *name;
#else
inilex(char *name)
#endif
{
	stbuf[0] = Alloc(3*P1_STMTBUFSIZE);
	stbuf[1] = stbuf[0] + P1_STMTBUFSIZE;
	stbuf[2] = stbuf[1] + P1_STMTBUFSIZE;
	nincl = 0;
	inclp = NULL;
	doinclude(name);
	lexstate = NEWSTMT;
	return(NO);
}



/* throw away the rest of the current line */
 void
flline(Void)
{
	lexstate = RETEOS;
}



 char *
#ifdef KR_headers
lexline(n)
	int *n;
#else
lexline(int *n)
#endif
{
	*n = (lastch - nextch) + 1;
	return(nextch);
}




 void
#ifdef KR_headers
doinclude(name)
	char *name;
#else
doinclude(char *name)
#endif
{
	FILEP fp;
	struct Inclfile *t;
	char *name0, *lastslash, *s, *s0, *temp;
	int j, k;
	chainp I;
	extern chainp Iargs;

	err_lineno = -1;
	if(inclp)
	{
		inclp->incllno = thislin;
		inclp->inclcode = code;
		inclp->inclstno = nxtstno;
		if(nextcd && (j = endcd - nextcd) > 0)
			inclp->incllinp = copyn(inclp->incllen = j, nextcd);
		else
			inclp->incllinp = 0;
	}
	nextcd = NULL;

	if(++nincl >= MAXINCLUDES)
		Fatal("includes nested too deep");
	if(name[0] == '\0')
		fp = stdin;
	else if(name[0] == '/' || inclp == NULL
#ifdef MSDOS
		|| name[0] == '\\'
		|| name[1] == ':'
#endif
		)
		fp = fopen(name, textread);
	else {
		lastslash = NULL;
		s = s0 = inclp->inclname;
#ifdef MSDOS
		if (s[1] == ':')
			lastslash = s + 1;
#endif
		for(; *s ; ++s)
			if(*s == '/'
#ifdef MSDOS
			|| *s == '\\'
#endif
			)
				lastslash = s;
		name0 = name;
		if(lastslash) {
			k = lastslash - s0 + 1;
			temp = Alloc(k + strlen(name) + 1);
			strncpy(temp, s0, k);
			strcpy(temp+k, name);
			name = temp;
			}
		fp = fopen(name, textread);
		if (!fp && (I = Iargs)) {
			k = strlen(name0) + 2;
			for(; I; I = I->nextp) {
				j = strlen(s = I->datap);
				name = Alloc(j + k);
				strcpy(name, s);
				switch(s[j-1]) {
					case '/':
#ifdef MSDOS
					case ':':
					case '\\':
#endif
						break;
					default:
						name[j++] = '/';
					}
				strcpy(name+j, name0);
				if (fp = fopen(name, textread)) {
					free(name0);
					goto havefp;
					}
				free(name);
				name = name0;
				}
			}
		}
	if (fp)
	{
 havefp:
		t = inclp;
		inclp = ALLOC(Inclfile);
		inclp->inclnext = t;
		prevlin = thislin = 0;
		infname = inclp->inclname = name;
		infile = inclp->inclfp = fp;
		lastline = 0;
		putlineno();
		lastline = 0;
	}
	else
	{
		fprintf(diagfile, "Cannot open file %s\n", name);
		done(1);
	}
}




 LOCAL int
popinclude(Void)
{
	struct Inclfile *t;
	register char *p;
	register int k;

	if(infile != stdin)
		clf(&infile, infname, 1);	/* Close the input file */
	free(infname);

	--nincl;
	err_lineno = -1;
	t = inclp->inclnext;
	free( (charptr) inclp);
	inclp = t;
	if(inclp == NULL) {
		infname = 0;
		return(NO);
		}

	infile = inclp->inclfp;
	infname = inclp->inclname;
	lineno = prevlin = thislin = inclp->incllno;
	code = inclp->inclcode;
	stno = nxtstno = inclp->inclstno;
	if(inclp->incllinp)
	{
		lastline = 0;
		putlineno();
		lastline = lineno;
		endcd = nextcd = sbuf;
		k = inclp->incllen;
		p = inclp->incllinp;
		while(--k >= 0)
			*endcd++ = *p++;
		free( (charptr) (inclp->incllinp) );
	}
	else
		nextcd = NULL;
	return(YES);
}


 void
#ifdef KR_headers
p1_line_number(line_number)
	long line_number;
#else
p1_line_number(long line_number)
#endif
{
	if (lastfile != lastfile0) {
		p1puts(P1_FILENAME, fbuf);
		lastfile0 = lastfile;
		}
	fprintf(pass1_file, "%d: %ld\n", P1_SET_LINE, line_number);
	}

 static void
putlineno(Void)
{
	extern int gflag;
	register char *s0, *s1;

	if (gflag) {
		if (lastline)
			p1_line_number(lastline);
		lastline = firstline;
		if (lastfile != infname)
			if (lastfile = infname) {
				strncpy(fbuf, lastfile, sizeof(fbuf));
				fbuf[sizeof(fbuf)-1] = 0;
				}
			else
				fbuf[0] = 0;
		}
	if (addftnsrc) {
		if (laststb && *laststb) {
			for(s1 = laststb; *s1; s1++) {
				for(s0 = s1; *s1 != '\n'; s1++)
					if (*s1 == '*' && s1[1] == '/')
						*s1 = '+';
				*s1 = 0;
				p1puts(P1_FORTRAN, s0);
				}
			*laststb = 0;	/* prevent trouble after EOF */
			}
		laststb = stb0;
		}
	}

 int
yylex(Void)
{
	static int  tokno;
	int retval;

	switch(lexstate)
	{
	case NEWSTMT :	/* need a new statement */
		retval = getcds();
		putlineno();
		if(retval == STEOF) {
			retval = SEOF;
			break;
		} /* if getcds() == STEOF */
		crunch();
		tokno = 0;
		lexstate = FIRSTTOKEN;
		yystno = stno;
		stno = nxtstno;
		toklen = 0;
		retval = SLABEL;
		break;

first:
	case FIRSTTOKEN :	/* first step on a statement */
		analyz();
		lexstate = OTHERTOKEN;
		tokno = 1;
		retval = stkey;
		break;

	case OTHERTOKEN :	/* return next token */
		if(nextch > lastch)
			goto reteos;
		++tokno;
		if( (stkey==SLOGIF || stkey==SELSEIF) && parlev==0 && tokno>3)
			goto first;

		if(stkey==SASSIGN && tokno==3 && nextch<lastch &&
		    nextch[0]=='t' && nextch[1]=='o')
		{
			nextch+=2;
			retval = STO;
			break;
		}
		if (tokno == 2 && stkey == SDO) {
			intonly = 1;
			retval = gettok();
			intonly = 0;
			}
		else
			retval = gettok();
		break;

reteos:
	case RETEOS:
		lexstate = NEWSTMT;
		retval = SEOS;
		break;
	default:
		fatali("impossible lexstate %d", lexstate);
		break;
	}

	if (retval == SEOF)
	    flush_comments ();

	return retval;
}

 LOCAL void
contmax(Void)
{
	lineno = thislin;
	many("continuation lines", 'C', maxcontin);
	}

/* Get Cards.

   Returns STEOF or STINITIAL, never STCONTINUE.  Any continuation cards get
merged into one long card (hence the size of the buffer named   sbuf)   */

 LOCAL int
getcds(Void)
{
	register char *p, *q;

	flush_comments ();
top:
	if(nextcd == NULL)
	{
		code = getcd( nextcd = sbuf, 1 );
		stno = nxtstno;
		prevlin = thislin;
	}
	if(code == STEOF)
		if( popinclude() )
			goto top;
		else
			return(STEOF);

	if(code == STCONTINUE)
	{
		lineno = thislin;
		nextcd = NULL;
		goto top;
	}

/* Get rid of unused space at the head of the buffer */

	if(nextcd > sbuf)
	{
		q = nextcd;
		p = sbuf;
		while(q < endcd)
			*p++ = *q++;
		endcd = p;
	}

/* Be aware that the input (i.e. the string at the address   nextcd)   is NOT
   NULL-terminated */

/* This loop merges all continuations into one long statement, AND puts the next
   card to be read at the end of the buffer (i.e. it stores the look-ahead card
   when there's room) */

	ncont = 0;
	for(;;) {
		nextcd = endcd;
		if (ncont >= maxcont || nextcd+66 > send)
			contmax();
		linestart[ncont++] = nextcd;
		if ((code = getcd(nextcd,0)) != STCONTINUE)
			break;
		if (ncont == 20 && noextflag) {
			lineno = thislin;
			errext("more than 19 continuation lines");
			}
		}
	nextch = sbuf;
	lastch = nextcd - 1;

	lineno = prevlin;
	prevlin = thislin;
	return(STINITIAL);
}

 static void
#ifdef KR_headers
bang(a, b, c, d, e)
	char *a;
	char *b;
	char *c;
	register char *d;
	register char *e;
#else
bang(char *a, char *b, char *c, register char *d, register char *e)
#endif
		/* save ! comments */
{
	char buf[COMMENT_BUFFER_SIZE + 1];
	register char *p, *pe;

	p = buf;
	pe = buf + COMMENT_BUFFER_SIZE;
	*pe = 0;
	while(a < b)
		if (!(*p++ = *a++))
			p[-1] = 0;
	if (b < c)
		*p++ = '\t';
	while(d < e) {
		if (!(*p++ = *d++))
			p[-1] = ' ';
		if (p == pe) {
			store_comment(buf);
			p = buf;
			}
		}
	if (p > buf) {
		while(--p >= buf && *p == ' ');
		p[1] = 0;
		store_comment(buf);
		}
	}


/* getcd - Get next input card

	This function reads the next input card from global file pointer   infile.
It assumes that   b   points to currently empty storage somewhere in  sbuf  */

 LOCAL int
#ifdef KR_headers
getcd(b, nocont)
	register char *b;
	int nocont;
#else
getcd(register char *b, int nocont)
#endif
{
	register int c;
	register char *p, *bend;
	int speclin;		/* Special line - true when the line is allowed
				   to have more than 66 characters (e.g. the
				   "&" shorthand for continuation, use of a "\t"
				   to skip part of the label columns) */
	static char a[6];	/* Statement label buffer */
	static char *aend	= a+6;
	static char *stb, *stbend;
	static int nst;
	char *atend, *endcd0;
	extern int warn72;
	char buf72[24];
	int amp, i;
	char storage[COMMENT_BUFFER_SIZE + 1];
	char *pointer;
	long L;

top:
	endcd = b;
	bend = b+66;
	amp = speclin = NO;
	atend = aend;

/* Handle the continuation shorthand of "&" in the first column, which stands
   for "     x" */

	if( (c = getc(infile)) == '&')
	{
		a[0] = c;
		a[1] = 0;
		a[5] = 'x';
		amp = speclin = YES;
		bend = send;
		p = aend;
	}

/* Handle the Comment cards (a 'C', 'c', '*', or '!' in the first column). */

	else if(comstart[c & (Table_size-1)])
	{
		if (feof (infile)
#ifdef EOF_CHAR
			 || c == EOF_CHAR
#endif
					)
		    return STEOF;

		if (c == '#') {
			*endcd++ = c;
			while((c = getc(infile)) != '\n')
				if (c == EOF)
					return STEOF;
				else if (endcd < bend)
					*endcd++ = c;
			++thislin;
			*endcd = 0;
			if (b[1] == ' ')
				p = b + 2;
			else if (!strncmp(b,"#line ",6))
				p = b + 6;
			else {
 bad_cpp:
				errstr("Bad # line: \"%s\"", b);
				goto top;
				}
			if (*p < '1' || *p > '9')
				goto bad_cpp;
			L = *p - '0';
			while((c = *++p) >= '0' && c <= '9')
				L = 10*L + c - '0';
			if (c != ' ' || *++p != '"')
				goto bad_cpp;
			bend = p;
			while(*++p != '"')
				if (!*p)
					goto bad_cpp;
			*p = 0;
			i = p - bend++;
			thislin = L - 1;
			if (!infname || strcmp(infname, bend)) {
				if (infname)
					free(infname);
				lastfile = 0;
				infname = Alloc(i);
				strcpy(infname, bend);
				if (inclp)
					inclp->inclname = infname;
				}
			goto top;
			}

		storage[COMMENT_BUFFER_SIZE] = c = '\0';
		pointer = storage;
		while( !feof (infile) && (*pointer++ = c = getc(infile)) != '\n') {

/* Handle obscure end of file conditions on many machines */

			if (feof (infile) && (c == '\377' || c == EOF)) {
			    pointer--;
			    break;
			} /* if (feof (infile)) */

			if (c == '\0')
				*(pointer - 1) = ' ';

			if (pointer == &storage[COMMENT_BUFFER_SIZE]) {
				store_comment (storage);
				pointer = storage;
			} /* if (pointer == BUFFER_SIZE) */
		} /* while */

		if (pointer > storage) {
		    if (c == '\n')

/* Get rid of the newline */

			pointer[-1] = 0;
		    else
			*pointer = 0;

		    store_comment (storage);
		} /* if */

		if (feof (infile))
		    if (c != '\n')	/* To allow the line index to
					   increment correctly */
			return STEOF;

		++thislin;
		goto top;
	}

	else if(c != EOF)
	{

/* Load buffer   a   with the statement label */

		/* a tab in columns 1-6 skips to column 7 */
		ungetc(c, infile);
		for(p=a; p<aend && (c=getc(infile)) != '\n' && c!=EOF; )
			if(c == '\t')

/* The tab character translates into blank characters in the statement label */

			{
				atend = p;
				while(p < aend)
					*p++ = BLANK;
				speclin = YES;
				bend = send;
			}
			else
				*p++ = c;
	}

/* By now we've read either a continuation character or the statement label
   field */

	if(c == EOF)
		return(STEOF);

/* The next 'if' block handles lines that have fewer than 7 characters */

	if(c == '\n')
	{
		while(p < aend)
			*p++ = BLANK;

/* Blank out the buffer on lines which are not longer than 66 characters */

		endcd0 = endcd;
		if( ! speclin )
			while(endcd < bend)
				*endcd++ = BLANK;
	}
	else	{	/* read body of line */
		if (warn72 & 2) {
			speclin = YES;
			bend = send;
			}
		while( endcd<bend && (c=getc(infile)) != '\n' && c!=EOF )
			*endcd++ = c;
		if(c == EOF)
			return(STEOF);

/* Drop any extra characters on the input card; this usually means those after
   column 72 */

		if(c != '\n')
		{
			i = 0;
			while( (c=getc(infile)) != '\n' && c != EOF)
				if (i < 23)
					buf72[i++] = c;
			if (warn72 && i && !speclin) {
				buf72[i] = 0;
				if (i >= 23)
					strcpy(buf72+20, "...");
				lineno = thislin + 1;
				errstr("text after column 72: %s", buf72);
				}
			if(c == EOF)
				return(STEOF);
		}

		endcd0 = endcd;
		if( ! speclin )
			while(endcd < bend)
				*endcd++ = BLANK;
	}

/* The flow of control usually gets to this line (unless an earlier RETURN has
   been taken) */

	++thislin;

	/* Fortran 77 specifies that a 0 in column 6 */
	/* does not signify continuation */

	if( !isspace(a[5]) && a[5]!='0') {
		if (!amp)
			for(p = a; p < aend;)
				if (*p++ == '!' && p != aend)
					goto initcheck;
		if (addftnsrc && stb) {
			if (stbend > stb + 7) { /* otherwise forget col 1-6 */
				/* kludge around funny p1gets behavior */
				*stb++ = '$';
				if (amp)
					*stb++ = '&';
				else
					for(p = a; p < atend;)
						*stb++ = *p++;
				}
			if (endcd0 - b > stbend - stb) {
				if (stb > stbend)
					stb = stbend;
				endcd0 = b + (stbend - stb);
				}
			for(p = b; p < endcd0;)
				*stb++ = *p++;
			*stb++ = '\n';
			*stb = 0;
			}
		if (nocont) {
			lineno = thislin;
			errstr("illegal continuation card (starts \"%.6s\")",a);
			}
		else if (!amp && strncmp(a,"     ",5)) {
			lineno = thislin;
			errstr("labeled continuation line (starts \"%.6s\")",a);
			}
		return(STCONTINUE);
		}
initcheck:
	for(p=a; p<atend; ++p)
		if( !isspace(*p) ) {
			if (*p++ != '!')
				goto initline;
			bang(p, atend, aend, b, endcd);
			goto top;
			}
	for(p = b ; p<endcd ; ++p)
		if( !isspace(*p) ) {
			if (*p++ != '!')
				goto initline;
			bang(a, a, a, p, endcd);
			goto top;
			}

/* Skip over blank cards by reading the next one right away */

	goto top;

initline:
	if (!lastline)
		lastline = thislin;
	if (addftnsrc) {
		nst = (nst+1)%3;
		if (!laststb && stb0)
			laststb = stb0;
		stb0 = stb = stbuf[nst];
		*stb++ = '$';	/* kludge around funny p1gets behavior */
		stbend = stb + sizeof(stbuf[0])-2;
		for(p = a; p < atend;)
			*stb++ = *p++;
		if (atend < aend)
			*stb++ = '\t';
		for(p = b; p < endcd0;)
			*stb++ = *p++;
		*stb++ = '\n';
		*stb = 0;
		}

/* Set   nxtstno   equal to the integer value of the statement label */

	nxtstno = 0;
	bend = a + 5;
	for(p = a ; p < bend ; ++p)
		if( !isspace(*p) )
			if(isdigit(*p))
				nxtstno = 10*nxtstno + (*p - '0');
			else if (*p == '!') {
				if (!addftnsrc)
					bang(p+1,atend,aend,b,endcd);
				endcd = b;
				break;
				}
			else	{
				lineno = thislin;
				errstr(
				"nondigit in statement label field \"%.5s\"", a);
				nxtstno = 0;
				break;
			}
	firstline = thislin;
	return(STINITIAL);
}

 LOCAL void
#ifdef KR_headers
adjtoklen(newlen)
	int newlen;
#else
adjtoklen(int newlen)
#endif
{
	while(maxtoklen < newlen)
		maxtoklen = 2*maxtoklen + 2;
	if (token = (char *)realloc(token, maxtoklen))
		return;
	fprintf(stderr, "adjtoklen: realloc(%d) failure!\n", maxtoklen);
	exit(2);
	}

/* crunch -- deletes all space characters, folds the backslash chars and
   Hollerith strings, quotes the Fortran strings */

 LOCAL void
crunch(Void)
{
	register char *i, *j, *j0, *j1, *prvstr;
	int k, ten, nh, nh0, quote;

	/* i is the next input character to be looked at
	   j is the next output character */

	new_dcl = needwkey = parlev = parseen = 0;
	expcom = 0;	/* exposed ','s */
	expeql = 0;	/* exposed equal signs */
	j = sbuf;
	prvstr = sbuf;
	k = 0;
	for(i=sbuf ; i<=lastch ; ++i)
	{
		if(isspace(*i) )
			continue;
		if (*i == '!') {
			while(i >= linestart[k])
				if (++k >= maxcont)
					contmax();
			j0 = linestart[k];
			if (!addftnsrc)
				bang(sbuf,sbuf,sbuf,i+1,j0);
			i = j0-1;
			continue;
			}

/* Keep everything in a quoted string */

		if(*i=='\'' ||  *i=='"')
		{
			int len = 0;

			quote = *i;
			*j = MYQUOTE; /* special marker */
			for(;;)
			{
				if(++i > lastch)
				{
					err("unbalanced quotes; closing quote supplied");
					if (j >= lastch)
						j = lastch - 1;
					break;
				}
				if(*i == quote)
					if(i<lastch && i[1]==quote) ++i;
					else break;
				else if(*i=='\\' && i<lastch && use_bs) {
					++i;
					*i = escapes[*(unsigned char *)i];
					}
				*++j = *i;
				len++;
			} /* for (;;) */

			if ((len = j - sbuf) > maxtoklen)
				adjtoklen(len);
			j[1] = MYQUOTE;
			j += 2;
			prvstr = j;
		}
		else if( (*i=='h' || *i=='H')  && j>prvstr)	/* test for Hollerith strings */
		{
			j0 = j - 1;
			if( ! isdigit(*j0)) goto copychar;
			nh = *j0 - '0';
			ten = 10;
			j1 = prvstr;
			if (j1 > sbuf && j1[-1] == MYQUOTE)
				--j1;
			if (j1+4 < j)
				j1 = j-4;
			for(;;) {
				if (j0-- <= j1)
					goto copychar;
				if( ! isdigit(*j0 ) ) break;
				nh += ten * (*j0-'0');
				ten*=10;
				}
/* A Hollerith string must be preceded by a punctuation mark.
   '*' is possible only as repetition factor in a data statement
   not, in particular, in character*2h .
   To avoid some confusion with missing commas in FORMAT statements,
   treat a preceding string as a punctuation mark.
 */

			if( !(*j0=='*'&&sbuf[0]=='d') && *j0!='/'
			&& *j0!='(' && *j0!=',' && *j0!='=' && *j0!='.'
			&& *j0 != MYQUOTE)
				goto copychar;
			nh0 = nh;
			if(i+nh > lastch)
			{
				erri("%dH too big", nh);
				nh = lastch - i;
				nh0 = -1;
			}
			if (nh > maxtoklen)
				adjtoklen(nh);
			j0[1] = MYQUOTE; /* special marker */
			j = j0 + 1;
			while(nh-- > 0)
			{
				if (++i > lastch) {
 hol_overflow:
					if (nh0 >= 0)
					  erri("escapes make %dH too big",
						nh0);
					break;
					}
				if(*i == '\\' && use_bs) {
					if (++i > lastch)
						goto hol_overflow;
					*i = escapes[*(unsigned char *)i];
					}
				*++j = *i;
			}
			j[1] = MYQUOTE;
			j+=2;
			prvstr = j;
		}
		else	{
			if(*i == '(') parseen = ++parlev;
			else if(*i == ')') --parlev;
			else if(parlev == 0)
				if(*i == '=') expeql = 1;
				else if(*i == ',') expcom = 1;
copychar:		/*not a string or space -- copy, shifting case if necessary */
			if(shiftcase && isupper(*i))
				*j++ = tolower(*i);
			else	*j++ = *i;
		}
	}
	lastch = j - 1;
	nextch = sbuf;
}

 LOCAL void
analyz(Void)
{
	register char *i;

	if(parlev != 0)
	{
		err("unbalanced parentheses, statement skipped");
		stkey = SUNKNOWN;
		lastch = sbuf - 1; /* prevent double error msg */
		return;
	}
	if(nextch+2<=lastch && nextch[0]=='i' && nextch[1]=='f' && nextch[2]=='(')
	{
		/* assignment or if statement -- look at character after balancing paren */
		parlev = 1;
		for(i=nextch+3 ; i<=lastch; ++i)
			if(*i == (MYQUOTE))
			{
				while(*++i != MYQUOTE)
					;
			}
			else if(*i == '(')
				++parlev;
			else if(*i == ')')
			{
				if(--parlev == 0)
					break;
			}
		if(i >= lastch)
			stkey = SLOGIF;
		else if(i[1] == '=')
			stkey = SLET;
		else if( isdigit(i[1]) )
			stkey = SARITHIF;
		else	stkey = SLOGIF;
		if(stkey != SLET)
			nextch += 2;
	}
	else if(expeql) /* may be an assignment */
	{
		if(expcom && nextch<lastch &&
		    nextch[0]=='d' && nextch[1]=='o')
		{
			stkey = SDO;
			nextch += 2;
		}
		else	stkey = SLET;
	}
	else if (parseen && nextch + 7 < lastch
			&& nextch[2] != 'u' /* screen out "double..." early */
			&& nextch[0] == 'd' && nextch[1] == 'o'
			&& ((nextch[2] >= '0' && nextch[2] <= '9')
				|| nextch[2] == ','
				|| nextch[2] == 'w'))
		{
		stkey = SDO;
		nextch += 2;
		needwkey = 1;
		}
	/* otherwise search for keyword */
	else	{
		stkey = getkwd();
		if(stkey==SGOTO && lastch>=nextch)
			if(nextch[0]=='(')
				stkey = SCOMPGOTO;
			else if(isalpha_(* USC nextch))
				stkey = SASGOTO;
	}
	parlev = 0;
}



 LOCAL int
getkwd(Void)
{
	register char *i, *j;
	register struct Keylist *pk, *pend;
	int k;

	if(! isalpha_(* USC nextch) )
		return(SUNKNOWN);
	k = letter(nextch[0]);
	if(pk = keystart[k])
		for(pend = keyend[k] ; pk<=pend ; ++pk )
		{
			i = pk->keyname;
			j = nextch;
			while(*++i==*++j && *i!='\0')
				;
			if(*i=='\0' && j<=lastch+1)
			{
				nextch = j;
				if(no66flag && pk->notinf66)
					errstr("Not a Fortran 66 keyword: %s",
					    pk->keyname);
				return(pk->keyval);
			}
		}
	return(SUNKNOWN);
}

 void
initkey(Void)
{
	register struct Keylist *p;
	register int i,j;
	register char *s;

	for(i = 0 ; i<26 ; ++i)
		keystart[i] = NULL;

	for(p = keys ; p->keyname ; ++p) {
		j = letter(p->keyname[0]);
		if(keystart[j] == NULL)
			keystart[j] = p;
		keyend[j] = p;
		}
	i = (maxcontin + 2) * 66;
	sbuf = (char *)ckalloc(i + 70);
	send = sbuf + i;
	maxcont = maxcontin + 1;
	linestart = (char **)ckalloc(maxcont*sizeof(char*));
	comstart['c'] = comstart['C'] = comstart['*'] = comstart['!'] =
	comstart['#'] = 1;
#ifdef EOF_CHAR
	comstart[EOF_CHAR] = 1;
#endif
	s = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
	while(i = *s++)
		anum_buf[i] = 1;
	s = "0123456789";
	while(i = *s++)
		anum_buf[i] = 2;
	}

 LOCAL int
#ifdef KR_headers
hexcheck(key)
	int key;
#else
hexcheck(int key)
#endif
{
	register int radix;
	register char *p;
	char *kind;

	switch(key) {
		case 'z':
		case 'Z':
		case 'x':
		case 'X':
			radix = 16;
			key = SHEXCON;
			kind = "hexadecimal";
			break;
		case 'o':
		case 'O':
			radix = 8;
			key = SOCTCON;
			kind = "octal";
			break;
		case 'b':
		case 'B':
			radix = 2;
			key = SBITCON;
			kind = "binary";
			break;
		default:
			err("bad bit identifier");
			return(SNAME);
		}
	for(p = token; *p; p++)
		if (hextoi(*p) >= radix) {
			errstr("invalid %s character", kind);
			break;
			}
	return key;
	}

/* gettok -- moves the right amount of text from   nextch   into the   token
   buffer.   token   initially contains garbage (leftovers from the prev token) */

 LOCAL int
gettok(Void)
{
	int havdot, havexp, havdbl;
	int radix, val;
	struct Punctlist *pp;
	struct Dotlist *pd;
	register int ch;
	static char	Exp_mi[] = "X**-Y treated as X**(-Y)",
			Exp_pl[] = "X**+Y treated as X**(+Y)";

	char *i, *j, *n1, *p;

	ch = * USC nextch;
	if(ch == (MYQUOTE))
	{
		++nextch;
		p = token;
		while(*nextch != MYQUOTE)
			*p++ = *nextch++;
		toklen = p - token;
		*p = 0;
		/* allow octal, binary, hex constants of the form 'abc'x (etc.) */
		if (++nextch <= lastch && isalpha_(val = * USC nextch)) {
			++nextch;
			return hexcheck(val);
			}
		return (SHOLLERITH);
	}

	if(needkwd)
	{
		needkwd = 0;
		return( getkwd() );
	}

	for(pp=puncts; pp->punchar; ++pp)
		if(ch == pp->punchar) {
			val = pp->punval;
			if (++nextch <= lastch)
			    switch(ch) {
				case '/':
					switch(*nextch) {
					  case '/':
						nextch++;
						val = SCONCAT;
						break;
					  case '=':
						goto sne;
					  default:
						if (new_dcl && parlev == 0)
							val = SSLASHD;
					  }
					return val;
				case '*':
					if (*nextch == '*') {
						nextch++;
						if (noextflag
						 && nextch <= lastch)
						 	switch(*nextch) {
							  case '-':
								errext(Exp_mi);
								break;
							  case '+':
								errext(Exp_pl);
								}
						return SPOWER;
						}
					break;
				case '<':
					switch(*nextch) {
					  case '=':
						nextch++;
						val = SLE;
						break;
					  case '>':
 sne:
						nextch++;
						val = SNE;
					  }
					goto extchk;
				case '=':
					if (*nextch == '=') {
						nextch++;
						val = SEQ;
						goto extchk;
						}
					break;
				case '>':
					if (*nextch == '=') {
						nextch++;
						val = SGE;
						}
 extchk:
					NOEXT("Fortran 8x comparison operator");
					return val;
				}
			else if (ch == '/' && new_dcl && parlev == 0)
				return SSLASHD;
			switch(val) {
				case SLPAR:
					++parlev;
					break;
				case SRPAR:
					--parlev;
				}
			return(val);
			}
	if(ch == '.')
		if(nextch >= lastch) goto badchar;
		else if(isdigit(nextch[1])) goto numconst;
		else	{
			for(pd=dots ; (j=pd->dotname) ; ++pd)
			{
				for(i=nextch+1 ; i<=lastch ; ++i)
					if(*i != *j) break;
					else if(*i != '.') ++j;
					else	{
						nextch = i+1;
						return(pd->dotval);
					}
			}
			goto badchar;
		}
	if( isalpha_(ch) )
	{
		p = token;
		*p++ = *nextch++;
		while(nextch<=lastch)
			if( isalnum_(* USC nextch) )
				*p++ = *nextch++;
			else break;
		toklen = p - token;
		*p = 0;
		if (needwkey) {
			needwkey = 0;
			if (toklen == 5
				&& nextch <= lastch && *nextch == '(' /*)*/
				&& !strcmp(token,"while"))
			return(SWHILE);
			}
		if(inioctl && nextch<=lastch && *nextch=='=')
		{
			++nextch;
			return(SNAMEEQ);
		}
		if(toklen>8 && eqn(8,token,"function")
		&& isalpha_(* USC (token+8)) &&
		    nextch<lastch && nextch[0]=='(' &&
		    (nextch[1]==')' || isalpha_(* USC (nextch+1))) )
		{
			nextch -= (toklen - 8);
			return(SFUNCTION);
		}

		if(toklen > MAXNAMELEN)
		{
			char buff[MAXNAMELEN+50];
			sprintf(buff, toklen >= MAXNAMELEN+10
				? "name %.*s... too long, truncated to %.*s"
				: "name %s too long, truncated to %.*s",
				MAXNAMELEN+6, token, MAXNAMELEN, token);
			err(buff);
			toklen = MAXNAMELEN;
			token[MAXNAMELEN] = '\0';
		}
		if(toklen==1 && *nextch==MYQUOTE) {
			val = token[0];
			++nextch;
			for(p = token ; *nextch!=MYQUOTE ; )
				*p++ = *nextch++;
			++nextch;
			toklen = p - token;
			*p = 0;
			return hexcheck(val);
		}
		return(SNAME);
	}

	if (isdigit(ch)) {

		/* Check for NAG's special hex constant */

		if (nextch[1] == '#' && nextch < lastch
		||  nextch[2] == '#' && isdigit(nextch[1])
				     && lastch - nextch >= 2) {

		    radix = atoi (nextch);
		    if (*++nextch != '#')
			nextch++;
		    if (radix != 2 && radix != 8 && radix != 16) {
		        erri("invalid base %d for constant, defaulting to hex",
				radix);
			radix = 16;
		    } /* if */
		    if (++nextch > lastch)
			goto badchar;
		    for (p = token; hextoi(*nextch) < radix;) {
			*p++ = *nextch++;
			if (nextch > lastch)
				break;
			}
		    toklen = p - token;
		    *p = 0;
		    return (radix == 16) ? SHEXCON : ((radix == 8) ? SOCTCON :
			    SBITCON);
		    }
		}
	else
		goto badchar;
numconst:
	havdot = NO;
	havexp = NO;
	havdbl = NO;
	for(n1 = nextch ; nextch<=lastch ; ++nextch)
	{
		if(*nextch == '.')
			if(havdot) break;
			else if(nextch+2<=lastch && isalpha_(* USC (nextch+1))
			    && isalpha_(* USC (nextch+2)))
				break;
			else	havdot = YES;
		else if( !intonly && (*nextch=='d' || *nextch=='e') )
		{
			p = nextch;
			havexp = YES;
			if(*nextch == 'd')
				havdbl = YES;
			if(nextch<lastch)
				if(nextch[1]=='+' || nextch[1]=='-')
					++nextch;
			if( ! isdigit(*++nextch) )
			{
				nextch = p;
				havdbl = havexp = NO;
				break;
			}
			for(++nextch ;
			    nextch<=lastch && isdigit(* USC nextch);
			    ++nextch);
			break;
		}
		else if( ! isdigit(* USC nextch) )
			break;
	}
	p = token;
	i = n1;
	while(i < nextch)
		*p++ = *i++;
	toklen = p - token;
	*p = 0;
	if(havdbl) return(SDCON);
	if(havdot || havexp) return(SRCON);
	return(SICON);
badchar:
	sbuf[0] = *nextch++;
	return(SUNKNOWN);
}

/* Comment buffering code */

 static void
#ifdef KR_headers
store_comment(str)
	char *str;
#else
store_comment(char *str)
#endif
{
	int len;
	comment_buf *ncb;

	if (nextcd == sbuf) {
		flush_comments();
		p1_comment(str);
		return;
		}
	len = strlen(str) + 1;
	if (cbnext + len > cblast) {
		if (!cbcur || !(ncb = cbcur->next)) {
			ncb = (comment_buf *) Alloc(sizeof(comment_buf));
			if (cbcur) {
				cbcur->last = cbnext;
				cbcur->next = ncb;
				}
			else {
				cbfirst = ncb;
				cbinit = ncb->buf;
				}
			ncb->next = 0;
			}
		cbcur = ncb;
		cbnext = ncb->buf;
		cblast = cbnext + COMMENT_BUF_STORE;
		}
	strcpy(cbnext, str);
	cbnext += len;
	}

 static void
flush_comments(Void)
{
	register char *s, *s1;
	register comment_buf *cb;
	if (cbnext == cbinit)
		return;
	cbcur->last = cbnext;
	for(cb = cbfirst;; cb = cb->next) {
		for(s = cb->buf; s < cb->last; s = s1) {
			/* compute s1 = new s value first, since */
			/* p1_comment may insert nulls into s */
			s1 = s + strlen(s) + 1;
			p1_comment(s);
			}
		if (cb == cbcur)
			break;
		}
	cbcur = cbfirst;
	cbnext = cbinit;
	cblast = cbnext + COMMENT_BUF_STORE;
	}

 void
unclassifiable(Void)
{
	register char *s, *se;

	s = sbuf;
	se = lastch;
	if (se < sbuf)
		return;
	lastch = s - 1;
	if (++se - s > 10)
		se = s + 10;
	for(; s < se; s++)
		if (*s == MYQUOTE) {
			se = s;
			break;
			}
	*se = 0;
	errstr("unclassifiable statement (starts \"%s\")", sbuf);
	}
