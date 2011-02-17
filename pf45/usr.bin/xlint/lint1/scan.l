%{
/* $NetBSD: scan.l,v 1.26 2002/01/31 22:30:21 tv Exp $ */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All Rights Reserved.
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jochen Pohl for
 *      The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: scan.l,v 1.26 2002/01/31 22:30:21 tv Exp $");
#endif
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <math.h>

#include "lint1.h"
#include "cgram.h"

#define CHAR_MASK	(~(~0 << CHAR_BIT))
#define YY_NO_UNPUT

/* Current position (its also updated when an included file is parsed) */
pos_t	curr_pos = { 1, "", 0 };

/*
 * Current position in C source (not updated when an included file is
 * parsed).
 */
pos_t	csrc_pos = { 1, "", 0 };

static	void	incline(void);
static	void	badchar(int);
static	sbuf_t	*allocsb(void);
static	void	freesb(sbuf_t *);
static	int	inpc(void);
static	int	hash(const char *);
static	sym_t	*search(sbuf_t *);
static	int	name(void);
static	int	keyw(sym_t *);
static	int	icon(int);
static	int	fcon(void);
static	int	operator(int, op_t);
static	int	ccon(void);
static	int	wccon(void);
static	int	getescc(int);
static	void	directive(void);
static	void	comment(void);
static	void	slashslashcomment(void);
static	int	string(void);
static	int	wcstrg(void);

%}

L	[_A-Za-z]
D	[0-9]
NZD	[1-9]
OD	[0-7]
HD	[0-9A-Fa-f]
EX	([eE][+-]?[0-9]+)

%%

{L}({L}|{D})*		 	return (name());
0{OD}*[lLuU]*			return (icon(8));
{NZD}{D}*[lLuU]*		return (icon(10));
0[xX]{HD}+[lLuU]*		return (icon(16));
{D}+\.{D}*{EX}?[fFlL]?		|
{D}+{EX}[fFlL]?			|
\.{D}+{EX}?[fFlL]?		return (fcon());
"="				return (operator(T_ASSIGN, ASSIGN));
"*="				return (operator(T_OPASS, MULASS));
"/="				return (operator(T_OPASS, DIVASS));
"%="				return (operator(T_OPASS, MODASS));
"+="				return (operator(T_OPASS, ADDASS));
"-="				return (operator(T_OPASS, SUBASS));
"<<="				return (operator(T_OPASS, SHLASS));
">>="				return (operator(T_OPASS, SHRASS));
"&="				return (operator(T_OPASS, ANDASS));
"^="				return (operator(T_OPASS, XORASS));
"|="				return (operator(T_OPASS, ORASS));
"||"				return (operator(T_LOGOR, LOGOR));
"&&"				return (operator(T_LOGAND, LOGAND));
"|"				return (operator(T_OR, OR));
"&"				return (operator(T_AND, AND));
"^"				return (operator(T_XOR, XOR));
"=="				return (operator(T_EQOP, EQ));
"!="				return (operator(T_EQOP, NE));
"<"				return (operator(T_RELOP, LT));
">"				return (operator(T_RELOP, GT));
"<="				return (operator(T_RELOP, LE));
">="				return (operator(T_RELOP, GE));
"<<"				return (operator(T_SHFTOP, SHL));
">>"				return (operator(T_SHFTOP, SHR));
"++"				return (operator(T_INCDEC, INC));
"--"				return (operator(T_INCDEC, DEC));
"->"				return (operator(T_STROP, ARROW));
"."				return (operator(T_STROP, POINT));
"+"				return (operator(T_ADDOP, PLUS));
"-"				return (operator(T_ADDOP, MINUS));
"*"				return (operator(T_MULT, MULT));
"/"				return (operator(T_DIVOP, DIV));
"%"				return (operator(T_DIVOP, MOD));
"!"				return (operator(T_UNOP, NOT));
"~"				return (operator(T_UNOP, COMPL));
"\""				return (string());
"L\""				return (wcstrg());
";"				return (T_SEMI);
"{"				return (T_LBRACE);
"}"				return (T_RBRACE);
","				return (T_COMMA);
":"				return (T_COLON);
"?"				return (T_QUEST);
"["				return (T_LBRACK);
"]"				return (T_RBRACK);
"("				return (T_LPARN);
")"				return (T_RPARN);
"..."				return (T_ELLIPSE);
"'"				return (ccon());
"L'"				return (wccon());
^#.*$				directive();
\n				incline();
\t|" "|\f|\v			;
"/*"				comment();
"//"				slashslashcomment();
.				badchar(yytext[0]);

%%

static void
incline(void)
{
	curr_pos.p_line++;
	curr_pos.p_uniq = 0;
	if (curr_pos.p_file == csrc_pos.p_file) {
		csrc_pos.p_line++;
		csrc_pos.p_uniq = 0;
	}
}

static void
badchar(int c)
{

	/* unknown character \%o */
	error(250, c);
}

/*
 * Keywords.
 * During initialisation they are written to the symbol table.
 */
static	struct	kwtab {
	const	char *kw_name;	/* keyword */
	int	kw_token;	/* token returned by yylex() */
	scl_t	kw_scl;		/* storage class if kw_token T_SCLASS */
	tspec_t	kw_tspec;	/* type spec. if kw_token T_TYPE or T_SOU */
	tqual_t	kw_tqual;	/* type qual. fi kw_token T_QUAL */
	u_int	kw_stdc : 1;	/* STDC keyword */
	u_int	kw_gcc : 1;	/* GCC keyword */
} kwtab[] = {
	{ "asm",	T_ASM,		0,	0,	0,	  0, 1 },
	{ "__asm",	T_ASM,		0,	0,	0,	  0, 0 },
	{ "__asm__",	T_ASM,		0,	0,	0,	  0, 0 },
	{ "auto",	T_SCLASS,	AUTO,	0,	0,	  0, 0 },
	{ "break",	T_BREAK,	0,	0,	0,	  0, 0 },
	{ "case",	T_CASE,		0,	0,	0,	  0, 0 },
	{ "char",	T_TYPE,		0,	CHAR,	0,	  0, 0 },
	{ "const",	T_QUAL,		0,	0,	CONST,	  1, 0 },
	{ "__const__",	T_QUAL,		0,	0,	CONST,	  0, 0 },
	{ "__const",	T_QUAL,		0,	0,	CONST,	  0, 0 },
	{ "continue",	T_CONTINUE,	0,	0,	0,	  0, 0 },
	{ "default",	T_DEFAULT,	0,	0,	0,	  0, 0 },
	{ "do",		T_DO,		0,	0,	0,	  0, 0 },
	{ "double",	T_TYPE,		0,	DOUBLE,	0,	  0, 0 },
	{ "else",	T_ELSE,		0,	0,	0,	  0, 0 },
	{ "enum",	T_ENUM,		0,	0,	0,	  0, 0 },
	{ "extern",	T_SCLASS,	EXTERN,	0,	0,	  0, 0 },
	{ "float",	T_TYPE,		0,	FLOAT,	0,	  0, 0 },
	{ "for",	T_FOR,		0,	0,	0,	  0, 0 },
	{ "goto",	T_GOTO,		0,	0,	0,	  0, 0 },
	{ "if",		T_IF,		0,	0,	0,	  0, 0 },
	{ "inline",	T_SCLASS,	INLINE,	0,	0,	  0, 1 },
	{ "__inline__",	T_SCLASS,	INLINE,	0,	0,	  0, 0 },
	{ "__inline",	T_SCLASS,	INLINE,	0,	0,	  0, 0 },
	{ "int",	T_TYPE,		0,	INT,	0,	  0, 0 },
	{ "__symbolrename", T_SYMBOLRENAME, 0,	0,	0,	  0, 0 },
	{ "long",	T_TYPE,		0,	LONG,	0,	  0, 0 },
	{ "register",	T_SCLASS,	REG,	0,	0,	  0, 0 },
	{ "return",	T_RETURN,	0,	0,	0,	  0, 0 },
	{ "short",	T_TYPE,		0,	SHORT,	0,	  0, 0 },
	{ "signed",	T_TYPE,		0,	SIGNED,	0,	  1, 0 },
	{ "__signed__",	T_TYPE,		0,	SIGNED,	0,	  0, 0 },
	{ "__signed",	T_TYPE,		0,	SIGNED,	0,	  0, 0 },
	{ "sizeof",	T_SIZEOF,	0,	0,	0,	  0, 0 },
	{ "static",	T_SCLASS,	STATIC,	0,	0,	  0, 0 },
	{ "struct",	T_SOU,		0,	STRUCT,	0,	  0, 0 },
	{ "switch",	T_SWITCH,	0,	0,	0,	  0, 0 },
	{ "typedef",	T_SCLASS,	TYPEDEF, 0,	0,	  0, 0 },
	{ "union",	T_SOU,		0,	UNION,	0,	  0, 0 },
	{ "unsigned",	T_TYPE,		0,	UNSIGN,	0,	  0, 0 },
	{ "void",	T_TYPE,		0,	VOID,	0,	  0, 0 },
	{ "volatile",	T_QUAL,		0,	0,	VOLATILE, 1, 0 },
	{ "__volatile__", T_QUAL,	0,	0,	VOLATILE, 0, 0 },
	{ "__volatile",	T_QUAL,		0,	0,	VOLATILE, 0, 0 },
	{ "while",	T_WHILE,	0,	0,	0,	  0, 0 },
	{ NULL,		0,		0,	0,	0,	  0, 0 }
};

/* Symbol table */
static	sym_t	*symtab[HSHSIZ1];

/* bit i of the entry with index i is set */
uint64_t qbmasks[sizeof(uint64_t) * CHAR_BIT];

/* least significant i bits are set in the entry with index i */
uint64_t qlmasks[sizeof(uint64_t) * CHAR_BIT + 1];

/* least significant i bits are not set in the entry with index i */
uint64_t qumasks[sizeof(uint64_t) * CHAR_BIT + 1];

/* free list for sbuf structures */
static	sbuf_t	 *sbfrlst;

/* Typ of next expected symbol */
symt_t	symtyp;


/*
 * All keywords are written to the symbol table. This saves us looking
 * in an extra table for each name we found.
 */
void
initscan(void)
{
	struct	kwtab *kw;
	sym_t	*sym;
	int	h, i;
	uint64_t uq;

	for (kw = kwtab; kw->kw_name != NULL; kw++) {
		if (kw->kw_stdc && tflag)
			continue;
		if (kw->kw_gcc && !gflag)
			continue;
		sym = getblk(sizeof (sym_t));
		sym->s_name = kw->kw_name;
		sym->s_keyw = 1;
		sym->s_value.v_quad = kw->kw_token;
		if (kw->kw_token == T_TYPE || kw->kw_token == T_SOU) {
			sym->s_tspec = kw->kw_tspec;
		} else if (kw->kw_token == T_SCLASS) {
			sym->s_scl = kw->kw_scl;
		} else if (kw->kw_token == T_QUAL) {
			sym->s_tqual = kw->kw_tqual;
		}
		h = hash(sym->s_name);
		if ((sym->s_link = symtab[h]) != NULL)
			symtab[h]->s_rlink = &sym->s_link;
		(symtab[h] = sym)->s_rlink = &symtab[h];
	}

	/* initialize bit-masks for quads */
	for (i = 0; i < sizeof (uint64_t) * CHAR_BIT; i++) {
		qbmasks[i] = (uint64_t)1 << i;
		uq = ~(uint64_t)0 << i;
		qumasks[i] = uq;
		qlmasks[i] = ~uq;
	}
	qumasks[i] = 0;
	qlmasks[i] = ~(uint64_t)0;
}

/*
 * Get a free sbuf structure, if possible from the free list
 */
static sbuf_t *
allocsb(void)
{
	sbuf_t	*sb;

	if ((sb = sbfrlst) != NULL) {
		sbfrlst = sb->sb_nxt;
	} else {
		if ((sb = malloc(sizeof (sbuf_t))) == NULL)
			nomem();
	}
	(void)memset(sb, 0, sizeof (*sb));
	return (sb);
}

/*
 * Put a sbuf structure to the free list
 */
static void
freesb(sbuf_t *sb)
{

	sb->sb_nxt = sbfrlst;
	sbfrlst = sb;
}

/*
 * Read a character and ensure that it is positive (except EOF).
 * Increment line count(s) if necessary.
 */
static int
inpc(void)
{
	int	c;

	if ((c = input()) != EOF && (c &= CHAR_MASK) == '\n')
		incline();
	return (c);
}

static int
hash(const char *s)
{
	u_int	v;
	const	u_char *us;

	v = 0;
	for (us = (const u_char *)s; *us != '\0'; us++) {
		v = (v << sizeof (v)) + *us;
		v ^= v >> (sizeof (v) * CHAR_BIT - sizeof (v));
	}
	return (v % HSHSIZ1);
}

/*
 * Lex has found a letter followed by zero or more letters or digits.
 * It looks for a symbol in the symbol table with the same name. This
 * symbol must either be a keyword or a symbol of the type required by
 * symtyp (label, member, tag, ...).
 *
 * If it is a keyword, the token is returned. In some cases it is described
 * more deeply by data written to yylval.
 *
 * If it is a symbol, T_NAME is returned and the pointer to a sbuf struct
 * is stored in yylval. This struct contains the name of the symbol, it's
 * length and hash value. If there is already a symbol of the same name
 * and type in the symbol table, the sbuf struct also contains a pointer
 * to the symbol table entry.
 */
static int
name(void)
{
	char	*s;
	sbuf_t	*sb;
	sym_t	*sym;
	int	tok;

	sb = allocsb();
	sb->sb_name = yytext;
	sb->sb_len = yyleng;
	sb->sb_hash = hash(yytext);

	if ((sym = search(sb)) != NULL && sym->s_keyw) {
		freesb(sb);
		return (keyw(sym));
	}

	sb->sb_sym = sym;

	if (sym != NULL) {
		if (blklev < sym->s_blklev)
			lerror("name() 1");
		sb->sb_name = sym->s_name;
		sb->sb_len = strlen(sym->s_name);
		tok = sym->s_scl == TYPEDEF ? T_TYPENAME : T_NAME;
	} else {
		s = getblk(yyleng + 1);
		(void)memcpy(s, yytext, yyleng + 1);
		sb->sb_name = s;
		sb->sb_len = yyleng;
		tok = T_NAME;
	}

	yylval.y_sb = sb;
	return (tok);
}

static sym_t *
search(sbuf_t *sb)
{
	sym_t	*sym;

	for (sym = symtab[sb->sb_hash]; sym != NULL; sym = sym->s_link) {
		if (strcmp(sym->s_name, sb->sb_name) == 0) {
			if (sym->s_keyw || sym->s_kind == symtyp)
				return (sym);
		}
	}

	return (NULL);
}

static int
keyw(sym_t *sym)
{
	int	t;

	if ((t = (int)sym->s_value.v_quad) == T_SCLASS) {
		yylval.y_scl = sym->s_scl;
	} else if (t == T_TYPE || t == T_SOU) {
		yylval.y_tspec = sym->s_tspec;
	} else if (t == T_QUAL) {
		yylval.y_tqual = sym->s_tqual;
	}
	return (t);
}

/*
 * Convert a string representing an integer into internal representation.
 * The value is returned in yylval. icon() (and yylex()) returns T_CON.
 */
static int
icon(int base)
{
	int	l_suffix, u_suffix;
	int	len;
	const	char *cp;
	char	c, *eptr;
	tspec_t	typ;
	u_long	ul = 0;
	uint64_t uq = 0;
	int	ansiu;
	static	tspec_t contypes[2][3] = {
		{ INT,  LONG,  QUAD },
		{ UINT, ULONG, UQUAD }
	};

	cp = yytext;
	len = yyleng;

	/* skip 0x */
	if (base == 16) {
		cp += 2;
		len -= 2;
	}

	/* read suffixes */
	l_suffix = u_suffix = 0;
	for ( ; ; ) {
		if ((c = cp[len - 1]) == 'l' || c == 'L') {
			l_suffix++;
		} else if (c == 'u' || c == 'U') {
			u_suffix++;
		} else {
			break;
		}
		len--;
	}
	if (l_suffix > 2 || u_suffix > 1) {
		/* malformed integer constant */
		warning(251);
		if (l_suffix > 2)
			l_suffix = 2;
		if (u_suffix > 1)
			u_suffix = 1;
	}
	if (tflag && u_suffix != 0) {
		/* suffix U is illegal in traditional C */
		warning(97);
	}
	typ = contypes[u_suffix][l_suffix];

	errno = 0;
	if (l_suffix < 2) {
		ul = strtoul(cp, &eptr, base);
	} else {
		uq = strtouq(cp, &eptr, base);
	}
	if (eptr != cp + len)
		lerror("icon() 1");
	if (errno != 0)
		/* integer constant out of range */
		warning(252);

	/*
	 * If the value is to big for the current type, we must choose
	 * another type.
	 */
	ansiu = 0;
	switch (typ) {
	case INT:
		if (ul <= INT_MAX) {
			/* ok */
		} else if (ul <= (unsigned)UINT_MAX && base != 10) {
			typ = UINT;
		} else if (ul <= LONG_MAX) {
			typ = LONG;
		} else {
			typ = ULONG;
		}
		if (typ == UINT || typ == ULONG) {
			if (tflag) {
				typ = LONG;
			} else if (!sflag) {
				/*
				 * Remember that the constant is unsigned
				 * only in ANSI C
				 */
				ansiu = 1;
			}
		}
		break;
	case UINT:
		if (ul > (u_int)UINT_MAX)
			typ = ULONG;
		break;
	case LONG:
		if (ul > LONG_MAX && !tflag) {
			typ = ULONG;
			if (!sflag)
				ansiu = 1;
		}
		break;
	case QUAD:
		if (uq > QUAD_MAX && !tflag) {
			typ = UQUAD;
			if (!sflag)
				ansiu = 1;
		}
		break;
		/* LINTED (enumeration values not handled in switch) */
	case STRUCT:
	case VOID:
	case LDOUBLE:
	case FUNC:
	case ARRAY:
	case PTR:
	case ENUM:
	case UNION:
	case SIGNED:
	case NOTSPEC:
	case DOUBLE:
	case FLOAT:
	case UQUAD:
	case ULONG:
	case USHORT:
	case SHORT:
	case UCHAR:
	case SCHAR:
	case CHAR:
	case UNSIGN:
		break;

	case NTSPEC:	/* this value unused */
		break;
	}

	if (typ != QUAD && typ != UQUAD) {
		if (isutyp(typ)) {
			uq = ul;
		} else {
			uq = (int64_t)(long)ul;
		}
	}

	uq = (uint64_t)xsign((int64_t)uq, typ, -1);

	if ((yylval.y_val = calloc(1, sizeof(val_t))) == NULL)
		nomem();
	yylval.y_val->v_tspec = typ;
	yylval.y_val->v_ansiu = ansiu;
	yylval.y_val->v_quad = (int64_t)uq;

	return (T_CON);
}

/*
 * Returns 1 if t is a signed type and the value is negative.
 *
 * len is the number of significant bits. If len is -1, len is set
 * to the width of type t.
 */
int
sign(int64_t q, tspec_t t, int len)
{

	if (t == PTR || isutyp(t))
		return (0);
	return (msb(q, t, len));
}

int
msb(int64_t q, tspec_t t, int len)
{

	if (len <= 0)
		len = size(t);
	return ((q & qbmasks[len - 1]) != 0);
}

/*
 * Extends the sign of q.
 */
int64_t
xsign(int64_t q, tspec_t t, int len)
{

	if (len <= 0)
		len = size(t);

	if (t == PTR || isutyp(t) || !sign(q, t, len)) {
		q &= qlmasks[len];
	} else {
		q |= qumasks[len];
	}
	return (q);
}

/*
 * Convert a string representing a floating point value into its interal
 * representation. Type and value are returned in yylval. fcon()
 * (and yylex()) returns T_CON.
 * XXX Currently it is not possible to convert constants of type
 * long double which are greater than DBL_MAX.
 */
static int
fcon(void)
{
	const	char *cp;
	int	len;
	tspec_t typ;
	char	c, *eptr;
	double	d;
	float	f = 0;

	cp = yytext;
	len = yyleng;

	if ((c = cp[len - 1]) == 'f' || c == 'F') {
		typ = FLOAT;
		len--;
	} else if (c == 'l' || c == 'L') {
		typ = LDOUBLE;
		len--;
	} else {
		typ = DOUBLE;
	}

	if (tflag && typ != DOUBLE) {
		/* suffixes F and L are illegal in traditional C */
		warning(98);
	}

	errno = 0;
	d = strtod(cp, &eptr);
	if (eptr != cp + len)
		lerror("fcon() 1");
	if (errno != 0)
		/* floating-point constant out of range */
		warning(248);

	if (typ == FLOAT) {
		f = (float)d;
		if (!finite(f)) {
			/* floating-point constant out of range */
			warning(248);
			f = f > 0 ? FLT_MAX : -FLT_MAX;
		}
	}

	if ((yylval.y_val = calloc(1, sizeof (val_t))) == NULL)
		nomem();
	yylval.y_val->v_tspec = typ;
	if (typ == FLOAT) {
		yylval.y_val->v_ldbl = f;
	} else {
		yylval.y_val->v_ldbl = d;
	}

	return (T_CON);
}

static int
operator(int t, op_t o)
{

	yylval.y_op = o;
	return (t);
}

/*
 * Called if lex found a leading \'.
 */
static int
ccon(void)
{
	int	n, val, c;
	char	cv;

	n = 0;
	val = 0;
	while ((c = getescc('\'')) >= 0) {
		val = (val << CHAR_BIT) + c;
		n++;
	}
	if (c == -2) {
		/* unterminated character constant */
		error(253);
	} else {
		if (n > sizeof (int) || (n > 1 && (pflag || hflag))) {
			/* too many characters in character constant */
			error(71);
		} else if (n > 1) {
			/* multi-character character constant */
			warning(294);
		} else if (n == 0) {
			/* empty character constant */
			error(73);
		}
	}
	if (n == 1) {
		cv = (char)val;
		val = cv;
	}

	yylval.y_val = xcalloc(1, sizeof (val_t));
	yylval.y_val->v_tspec = INT;
	yylval.y_val->v_quad = val;

	return (T_CON);
}

/*
 * Called if lex found a leading L\'
 */
static int
wccon(void)
{
	static	char buf[MB_LEN_MAX + 1];
	int	i, c;
	wchar_t	wc;

	i = 0;
	while ((c = getescc('\'')) >= 0) {
		if (i < MB_CUR_MAX)
			buf[i] = (char)c;
		i++;
	}

	wc = 0;

	if (c == -2) {
		/* unterminated character constant */
		error(253);
	} else if (c == 0) {
		/* empty character constant */
		error(73);
	} else {
		if (i > MB_CUR_MAX) {
			i = MB_CUR_MAX;
			/* too many characters in character constant */
			error(71);
		} else {
			buf[i] = '\0';
			(void)mbtowc(NULL, NULL, 0);
			if (mbtowc(&wc, buf, MB_CUR_MAX) < 0)
				/* invalid multibyte character */
				error(291);
		}
	}

	if ((yylval.y_val = calloc(1, sizeof (val_t))) == NULL)
		nomem();
	yylval.y_val->v_tspec = WCHAR;
	yylval.y_val->v_quad = wc;

	return (T_CON);
}

/*
 * Read a character which is part of a character constant or of a string
 * and handle escapes.
 *
 * The Argument is the character which delimits the character constant or
 * string.
 *
 * Returns -1 if the end of the character constant or string is reached,
 * -2 if the EOF is reached, and the character otherwise.
 */
static int
getescc(int d)
{
	static	int pbc = -1;
	int	n, c, v;

	if (pbc == -1) {
		c = inpc();
	} else {
		c = pbc;
		pbc = -1;
	}
	if (c == d)
		return (-1);
	switch (c) {
	case '\n':
		if (tflag) {
			/* newline in string or char constant */
			error(254);
			return (-2);
		}
		return (c);
	case EOF:
		return (-2);
	case '\\':
		switch (c = inpc()) {
		case '"':
			if (tflag && d == '\'')
				/* \" inside character constant undef. ... */
				warning(262);
			return ('"');
		case '\'':
			return ('\'');
		case '?':
			if (tflag)
				/* \? undefined in traditional C */
				warning(263);
			return ('?');
		case '\\':
			return ('\\');
		case 'a':
			if (tflag)
				/* \a undefined in traditional C */
				warning(81);
			return ('\a');
		case 'b':
			return ('\b');
		case 'f':
			return ('\f');
		case 'n':
			return ('\n');
		case 'r':
			return ('\r');
		case 't':
			return ('\t');
		case 'v':
			if (tflag)
				/* \v undefined in traditional C */
				warning(264);
			return ('\v');
		case '8': case '9':
			/* bad octal digit %c */
			warning(77, c);
			/* FALLTHROUGH */
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			n = 3;
			v = 0;
			do {
				v = (v << 3) + (c - '0');
				c = inpc();
			} while (--n && isdigit(c) && (tflag || c <= '7'));
			if (tflag && n > 0 && isdigit(c))
				/* bad octal digit %c */
				warning(77, c);
			pbc = c;
			if (v > UCHAR_MAX) {
				/* character escape does not fit in char. */
				warning(76);
				v &= CHAR_MASK;
			}
			return (v);
		case 'x':
			if (tflag)
				/* \x undefined in traditional C */
				warning(82);
			v = 0;
			n = 0;
			while ((c = inpc()) >= 0 && isxdigit(c)) {
				c = isdigit(c) ?
					c - '0' : toupper(c) - 'A' + 10;
				v = (v << 4) + c;
				if (n >= 0) {
					if ((v & ~CHAR_MASK) != 0) {
						/* overflow in hex escape */
						warning(75);
						n = -1;
					} else {
						n++;
					}
				}
			}
			pbc = c;
			if (n == 0) {
				/* no hex digits follow \x */
				error(74);
			} if (n == -1) {
				v &= CHAR_MASK;
			}
			return (v);
		case '\n':
			return (getescc(d));
		case EOF:
			return (-2);
		default:
			if (isprint(c)) {
				/* dubious escape \%c */
				warning(79, c);
			} else {
				/* dubious escape \%o */
				warning(80, c);
			}
		}
	}
	return (c);
}

/*
 * Called for preprocessor directives. Currently implemented are:
 *	# lineno
 *	# lineno "filename"
 */
static void
directive(void)
{
	const	char *cp, *fn;
	char	c, *eptr;
	size_t	fnl;
	long	ln;
	static	int first = 1;

	/* Go to first non-whitespace after # */
	for (cp = yytext + 1; (c = *cp) == ' ' || c == '\t'; cp++)
		continue;

	if (!isdigit((unsigned char)c)) {
	error:
		/* undefined or invalid # directive */
		warning(255);
		return;
	}
	ln = strtol(--cp, &eptr, 10);
	if (cp == eptr)
		goto error;
	if ((c = *(cp = eptr)) != ' ' && c != '\t' && c != '\0')
		goto error;
	while ((c = *cp++) == ' ' || c == '\t')
		continue;
	if (c != '\0') {
		if (c != '"')
			goto error;
		fn = cp;
		while ((c = *cp) != '"' && c != '\0')
			cp++;
		if (c != '"')
			goto error;
		if ((fnl = cp++ - fn) > PATH_MAX)
			goto error;
		while ((c = *cp++) == ' ' || c == '\t')
			continue;
#if 0
		if (c != '\0')
			warning("extra character(s) after directive");
#endif

		/* empty string means stdin */
		if (fnl == 0) {
			fn = "{standard input}";
			fnl = 16;			/* strlen (fn) */
		}
		curr_pos.p_file = fnnalloc(fn, fnl);
		/*
		 * If this is the first directive, the name is the name
		 * of the C source file as specified at the command line.
		 * It is written to the output file.
		 */
		if (first) {
			csrc_pos.p_file = curr_pos.p_file;
			outsrc(curr_pos.p_file);
			first = 0;
		}
	}
	curr_pos.p_line = (int)ln - 1;
	curr_pos.p_uniq = 0;
	if (curr_pos.p_file == csrc_pos.p_file) {
		csrc_pos.p_line = (int)ln - 1;
		csrc_pos.p_uniq = 0;
	}
}

/*
 * Handle lint comments. Following comments are currently understood:
 *	ARGSUSEDn
 *	BITFIELDTYPE
 *	CONSTCOND CONSTANTCOND CONSTANTCONDITION
 *	FALLTHRU FALLTHROUGH
 *	LINTLIBRARY
 *	LINTED NOSTRICT
 *	LONGLONG
 *	NOTREACHED
 *	PRINTFLIKEn
 *	PROTOLIB
 *	SCANFLIKEn
 *	VARARGSn
 * If one of this comments is recognized, the arguments, if any, are
 * parsed and a function which handles this comment is called.
 */
static void
comment(void)
{
	int	c, lc;
	static struct {
		const	char *keywd;
		int	arg;
		void	(*func)(int);
	} keywtab[] = {
		{ "ARGSUSED",		1,	argsused	},
		{ "BITFIELDTYPE",	0,	bitfieldtype	},
		{ "CONSTCOND",		0,	constcond	},
		{ "CONSTANTCOND",	0,	constcond	},
		{ "CONSTANTCONDITION",	0,	constcond	},
		{ "FALLTHRU",		0,	fallthru	},
		{ "FALLTHROUGH",	0,	fallthru	},
		{ "LINTLIBRARY",	0,	lintlib		},
		{ "LINTED",		0,	linted		},
		{ "LONGLONG",		0,	longlong	},
		{ "NOSTRICT",		0,	linted		},
		{ "NOTREACHED",		0,	notreach	},
		{ "PRINTFLIKE",		1,	printflike	},
		{ "PROTOLIB",		1,	protolib	},
		{ "SCANFLIKE",		1,	scanflike	},
		{ "VARARGS",		1,	varargs		},
	};
	char	keywd[32];
	char	arg[32];
	int	l, i, a;
	int	eoc;

	eoc = 0;

	/* Skip white spaces after the start of the comment */
	while ((c = inpc()) != EOF && isspace(c))
		continue;

	/* Read the potential keyword to keywd */
	l = 0;
	while (c != EOF && isupper(c) && l < sizeof (keywd) - 1) {
		keywd[l++] = (char)c;
		c = inpc();
	}
	keywd[l] = '\0';

	/* look for the keyword */
	for (i = 0; i < sizeof (keywtab) / sizeof (keywtab[0]); i++) {
		if (strcmp(keywtab[i].keywd, keywd) == 0)
			break;
	}
	if (i == sizeof (keywtab) / sizeof (keywtab[0]))
		goto skip_rest;

	/* skip white spaces after the keyword */
	while (c != EOF && isspace(c))
		c = inpc();

	/* read the argument, if the keyword accepts one and there is one */
	l = 0;
	if (keywtab[i].arg) {
		while (c != EOF && isdigit(c) && l < sizeof (arg) - 1) {
			arg[l++] = (char)c;
			c = inpc();
		}
	}
	arg[l] = '\0';
	a = l != 0 ? atoi(arg) : -1;

	/* skip white spaces after the argument */
	while (c != EOF && isspace(c))
		c = inpc();

	if (c != '*' || (c = inpc()) != '/') {
		if (keywtab[i].func != linted)
			/* extra characters in lint comment */
			warning(257);
	} else {
		/*
		 * remember that we have already found the end of the
		 * comment
		 */
		eoc = 1;
	}

	if (keywtab[i].func != NULL)
		(*keywtab[i].func)(a);

 skip_rest:
	while (!eoc) {
		lc = c;
		if ((c = inpc()) == EOF) {
			/* unterminated comment */
			error(256);
			break;
		}
		if (lc == '*' && c == '/')
			eoc = 1;
	}
}

/*
 * Handle // style comments
 */
static void
slashslashcomment(void)
{
	int c;

	if (sflag < 2 && !gflag)
		/* // comments only supported in C99 */
		(void)gnuism(312, tflag ? "traditional" : "ANSI");

	while ((c = inpc()) != EOF && c != '\n')
		continue;
}

/*
 * Clear flags for lint comments LINTED, LONGLONG and CONSTCOND.
 * clrwflgs() is called after function definitions and global and
 * local declarations and definitions. It is also called between
 * the controlling expression and the body of control statements
 * (if, switch, for, while).
 */
void
clrwflgs(void)
{

	nowarn = 0;
	quadflg = 0;
	ccflg = 0;
}

/*
 * Strings are stored in a dynamically alloceted buffer and passed
 * in yylval.y_xstrg to the parser. The parser or the routines called
 * by the parser are responsible for freeing this buffer.
 */
static int
string(void)
{
	u_char	*s;
	int	c;
	size_t	len, max;
	strg_t	*strg;

	if ((s = malloc(max = 64)) == NULL)
		nomem();

	len = 0;
	while ((c = getescc('"')) >= 0) {
		/* +1 to reserve space for a trailing NUL character */
		if (len + 1 == max)
			if ((s = realloc(s, max *= 2)) == NULL)
				nomem();
		s[len++] = (char)c;
	}
	s[len] = '\0';
	if (c == -2)
		/* unterminated string constant */
		error(258);

	if ((strg = calloc(1, sizeof (strg_t))) == NULL)
		nomem();
	strg->st_tspec = CHAR;
	strg->st_len = len;
	strg->st_cp = s;

	yylval.y_strg = strg;
	return (T_STRING);
}

static int
wcstrg(void)
{
	char	*s;
	int	c, i, n, wi;
	size_t	len, max, wlen;
	wchar_t	*ws;
	strg_t	*strg;

	if ((s = malloc(max = 64)) == NULL)
		nomem();
	len = 0;
	while ((c = getescc('"')) >= 0) {
		/* +1 to save space for a trailing NUL character */
		if (len + 1 >= max)
			if ((s = realloc(s, max *= 2)) == NULL)
				nomem();
		s[len++] = (char)c;
	}
	s[len] = '\0';
	if (c == -2)
		/* unterminated string constant */
		error(258);

	/* get length of wide character string */
	(void)mblen(NULL, 0);
	for (i = 0, wlen = 0; i < len; i += n, wlen++) {
		if ((n = mblen(&s[i], MB_CUR_MAX)) == -1) {
			/* invalid multibyte character */
			error(291);
			break;
		}
		if (n == 0)
			n = 1;
	}

	if ((ws = malloc((wlen + 1) * sizeof (wchar_t))) == NULL)
		nomem();

	/* convert from multibyte to wide char */
	(void)mbtowc(NULL, NULL, 0);
	for (i = 0, wi = 0; i < len; i += n, wi++) {
		if ((n = mbtowc(&ws[wi], &s[i], MB_CUR_MAX)) == -1)
			break;
		if (n == 0)
			n = 1;
	}
	ws[wi] = 0;
	free(s);

	if ((strg = calloc(1, sizeof (strg_t))) == NULL)
		nomem();
	strg->st_tspec = WCHAR;
	strg->st_len = wlen;
	strg->st_wcp = ws;

	yylval.y_strg = strg;
	return (T_STRING);
}

/*
 * As noted above the scanner does not create new symbol table entries
 * for symbols it cannot find in the symbol table. This is to avoid
 * putting undeclared symbols into the symbol table if a syntax error
 * occurs.
 *
 * getsym() is called as soon as it is probably ok to put the symbol to
 * the symbol table. This does not mean that it is not possible that
 * symbols are put to the symbol table which are than not completely
 * declared due to syntax errors. To avoid too many problems in this
 * case symbols get type int in getsym().
 *
 * XXX calls to getsym() should be delayed until decl1*() is called
 */
sym_t *
getsym(sbuf_t *sb)
{
	dinfo_t	*di;
	char	*s;
	sym_t	*sym;

	sym = sb->sb_sym;

	/*
	 * During member declaration it is possible that name() looked
	 * for symbols of type FVFT, although it should have looked for
	 * symbols of type FTAG. Same can happen for labels. Both cases
	 * are compensated here.
	 */
	if (symtyp == FMOS || symtyp == FLAB) {
		if (sym == NULL || sym->s_kind == FVFT)
			sym = search(sb);
	}

	if (sym != NULL) {
		if (sym->s_kind != symtyp)
			lerror("storesym() 1");
		symtyp = FVFT;
		freesb(sb);
		return (sym);
	}

	/* create a new symbol table entry */

	/* labels must always be allocated at level 1 (outhermost block) */
	if (symtyp == FLAB) {
		sym = getlblk(1, sizeof (sym_t));
		s = getlblk(1, sb->sb_len + 1);
		(void)memcpy(s, sb->sb_name, sb->sb_len + 1);
		sym->s_name = s;
		sym->s_blklev = 1;
		di = dcs;
		while (di->d_nxt != NULL && di->d_nxt->d_nxt != NULL)
			di = di->d_nxt;
		if (di->d_ctx != AUTO)
			lerror("storesym() 2");
	} else {
		sym = getblk(sizeof (sym_t));
		sym->s_name = sb->sb_name;
		sym->s_blklev = blklev;
		di = dcs;
	}

	UNIQUE_CURR_POS(sym->s_dpos);
	if ((sym->s_kind = symtyp) != FLAB)
		sym->s_type = gettyp(INT);

	symtyp = FVFT;

	if ((sym->s_link = symtab[sb->sb_hash]) != NULL)
		symtab[sb->sb_hash]->s_rlink = &sym->s_link;
	(symtab[sb->sb_hash] = sym)->s_rlink = &symtab[sb->sb_hash];

	*di->d_ldlsym = sym;
	di->d_ldlsym = &sym->s_dlnxt;

	freesb(sb);
	return (sym);
}

/*
 * Remove a symbol forever from the symbol table. s_blklev
 * is set to -1 to avoid that the symbol will later be put
 * back to the symbol table.
 */
void
rmsym(sym_t *sym)
{

	if ((*sym->s_rlink = sym->s_link) != NULL)
		sym->s_link->s_rlink = sym->s_rlink;
	sym->s_blklev = -1;
	sym->s_link = NULL;
}

/*
 * Remove a list of symbols declared at one level from the symbol
 * table.
 */
void
rmsyms(sym_t *syms)
{
	sym_t	*sym;

	for (sym = syms; sym != NULL; sym = sym->s_dlnxt) {
		if (sym->s_blklev != -1) {
			if ((*sym->s_rlink = sym->s_link) != NULL)
				sym->s_link->s_rlink = sym->s_rlink;
			sym->s_link = NULL;
			sym->s_rlink = NULL;
		}
	}
}

/*
 * Put a symbol into the symbol table
 */
void
inssym(int bl, sym_t *sym)
{
	int	h;

	h = hash(sym->s_name);
	if ((sym->s_link = symtab[h]) != NULL)
		symtab[h]->s_rlink = &sym->s_link;
	(symtab[h] = sym)->s_rlink = &symtab[h];
	sym->s_blklev = bl;
	if (sym->s_link != NULL && sym->s_blklev < sym->s_link->s_blklev)
		lerror("inssym()");
}

/*
 * Called at level 0 after syntax errors
 * Removes all symbols which are not declared at level 0 from the
 * symbol table. Also frees all memory which is not associated with
 * level 0.
 */
void
cleanup(void)
{
	sym_t	*sym, *nsym;
	int	i;

	for (i = 0; i < HSHSIZ1; i++) {
		for (sym = symtab[i]; sym != NULL; sym = nsym) {
			nsym = sym->s_link;
			if (sym->s_blklev >= 1) {
				if ((*sym->s_rlink = nsym) != NULL)
					nsym->s_rlink = sym->s_rlink;
			}
		}
	}

	for (i = mblklev; i > 0; i--)
		freelblk(i);
}

/*
 * Create a new symbol with the name of an existing symbol.
 */
sym_t *
pushdown(sym_t *sym)
{
	int	h;
	sym_t	*nsym;

	h = hash(sym->s_name);
	nsym = getblk(sizeof (sym_t));
	if (sym->s_blklev > blklev)
		lerror("pushdown()");
	nsym->s_name = sym->s_name;
	UNIQUE_CURR_POS(nsym->s_dpos);
	nsym->s_kind = sym->s_kind;
	nsym->s_blklev = blklev;

	if ((nsym->s_link = symtab[h]) != NULL)
		symtab[h]->s_rlink = &nsym->s_link;
	(symtab[h] = nsym)->s_rlink = &symtab[h];

	*dcs->d_ldlsym = nsym;
	dcs->d_ldlsym = &nsym->s_dlnxt;

	return (nsym);
}

/*
 * Free any dynamically allocated memory referenced by
 * the value stack or yylval.
 * The type of information in yylval is described by tok.
 */
void
freeyyv(void *sp, int tok)
{
	if (tok == T_NAME || tok == T_TYPENAME) {
		sbuf_t *sb = *(sbuf_t **)sp;
		freesb(sb);
	} else if (tok == T_CON) {
		val_t *val = *(val_t **)sp;
		free(val);
	} else if (tok == T_STRING) {
		strg_t *strg = *(strg_t **)sp;
		if (strg->st_tspec == CHAR) {
			free(strg->st_cp);
		} else if (strg->st_tspec == WCHAR) {
			free(strg->st_wcp);
		} else {
			lerror("fryylv() 1");
		}
		free(strg);
	}
}
