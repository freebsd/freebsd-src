/*	$NetBSD: test.c,v 1.21 1999/04/05 09:48:38 kleink Exp $	*/

/*
 * test(1); version 7-like  --  author Erik Baalbergen
 * modified by Eric Gisin to be used as built-in.
 * modified by Arnold Robbins to add SVR3 compatibility
 * (-x -c -b -p -u -g -k) plus Korn's -L -nt -ot -ef and new -S (socket).
 * modified by J.T. Conklin for NetBSD.
 *
 * This program is in the Public Domain.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef SHELL
#define main testcmd
#include "bltin/bltin.h"
#else
static void error(const char *, ...) __attribute__((__noreturn__));

static void
#ifdef __STDC__
error(const char *msg, ...)
#else
error(va_alist)
	va_dcl
#endif
{
	va_list ap;
#ifndef __STDC__
	const char *msg;

	va_start(ap);
	msg = va_arg(ap, const char *);
#else
	va_start(ap, msg);
#endif
	verrx(2, msg, ap);
	/*NOTREACHED*/
	va_end(ap);
}
#endif

/* test(1) accepts the following grammar:
	oexpr	::= aexpr | aexpr "-o" oexpr ;
	aexpr	::= nexpr | nexpr "-a" aexpr ;
	nexpr	::= primary | "!" primary
	primary	::= unary-operator operand
		| operand binary-operator operand
		| operand
		| "(" oexpr ")"
		;
	unary-operator ::= "-r"|"-w"|"-x"|"-f"|"-d"|"-c"|"-b"|"-p"|
		"-u"|"-g"|"-k"|"-s"|"-t"|"-z"|"-n"|"-o"|"-O"|"-G"|"-L"|"-S";

	binary-operator ::= "="|"!="|"-eq"|"-ne"|"-ge"|"-gt"|"-le"|"-lt"|
			"-nt"|"-ot"|"-ef";
	operand ::= <any legal UNIX file name>
*/

enum token {
	EOI,
	FILRD,
	FILWR,
	FILEX,
	FILEXIST,
	FILREG,
	FILDIR,
	FILCDEV,
	FILBDEV,
	FILFIFO,
	FILSOCK,
	FILSYM,
	FILGZ,
	FILTT,
	FILSUID,
	FILSGID,
	FILSTCK,
	FILNT,
	FILOT,
	FILEQ,
	FILUID,
	FILGID,
	STREZ,
	STRNZ,
	STREQ,
	STRNE,
	STRLT,
	STRGT,
	INTEQ,
	INTNE,
	INTGE,
	INTGT,
	INTLE,
	INTLT,
	UNOT,
	BAND,
	BOR,
	LPAREN,
	RPAREN,
	OPERAND
};

enum token_types {
	UNOP,
	BINOP,
	BUNOP,
	BBINOP,
	PAREN
};

struct t_op {
	const char *op_text;
	short op_num, op_type;
} const ops [] = {
	{"-r",	FILRD,	UNOP},
	{"-w",	FILWR,	UNOP},
	{"-x",	FILEX,	UNOP},
	{"-e",	FILEXIST,UNOP},
	{"-f",	FILREG,	UNOP},
	{"-d",	FILDIR,	UNOP},
	{"-c",	FILCDEV,UNOP},
	{"-b",	FILBDEV,UNOP},
	{"-p",	FILFIFO,UNOP},
	{"-u",	FILSUID,UNOP},
	{"-g",	FILSGID,UNOP},
	{"-k",	FILSTCK,UNOP},
	{"-s",	FILGZ,	UNOP},
	{"-t",	FILTT,	UNOP},
	{"-z",	STREZ,	UNOP},
	{"-n",	STRNZ,	UNOP},
	{"-h",	FILSYM,	UNOP},		/* for backwards compat */
	{"-O",	FILUID,	UNOP},
	{"-G",	FILGID,	UNOP},
	{"-L",	FILSYM,	UNOP},
	{"-S",	FILSOCK,UNOP},
	{"=",	STREQ,	BINOP},
	{"!=",	STRNE,	BINOP},
	{"<",	STRLT,	BINOP},
	{">",	STRGT,	BINOP},
	{"-eq",	INTEQ,	BINOP},
	{"-ne",	INTNE,	BINOP},
	{"-ge",	INTGE,	BINOP},
	{"-gt",	INTGT,	BINOP},
	{"-le",	INTLE,	BINOP},
	{"-lt",	INTLT,	BINOP},
	{"-nt",	FILNT,	BINOP},
	{"-ot",	FILOT,	BINOP},
	{"-ef",	FILEQ,	BINOP},
	{"!",	UNOT,	BUNOP},
	{"-a",	BAND,	BBINOP},
	{"-o",	BOR,	BBINOP},
	{"(",	LPAREN,	PAREN},
	{")",	RPAREN,	PAREN},
	{0,	0,	0}
};

struct t_op const *t_wp_op;
char **t_wp;

static int	aexpr __P((enum token));
static int	binop __P((void));
static int	equalf __P((const char *, const char *));
static int	filstat __P((char *, enum token));
static int	getn __P((const char *));
static quad_t	getq __P((const char *));
static int	intcmp __P((const char *, const char *));
static int	isoperand __P((void));
int		main __P((int, char **));
static int	newerf __P((const char *, const char *));
static int	nexpr __P((enum token));
static int	oexpr __P((enum token));
static int	olderf __P((const char *, const char *));
static int	primary __P((enum token));
static void	syntax __P((const char *, const char *));
static enum	token t_lex __P((char *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int	res;
	char	*p;

	if ((p = rindex(argv[0], '/')) == NULL)
		p = argv[0];
	else
		p++;
	if (strcmp(p, "[") == 0) {
		if (strcmp(argv[--argc], "]"))
			error("missing ]");
		argv[argc] = NULL;
	}

	/* XXX work around the absence of an eaccess(2) syscall */
	(void)setgid(getegid());
	(void)setuid(geteuid());

	if (--argc <= 0)
		return 1;

	t_wp = &argv[1];
	res = !oexpr(t_lex(*t_wp));

	if (*t_wp != NULL && *++t_wp != NULL)
		syntax(*t_wp, "unexpected operator");

	return res;
}

static void
syntax(op, msg)
	const char	*op;
	const char	*msg;
{

	if (op && *op)
		error("%s: %s", op, msg);
	else
		error("%s", msg);
}

static int
oexpr(n)
	enum token n;
{
	int res;

	res = aexpr(n);
	if (t_lex(*++t_wp) == BOR)
		return oexpr(t_lex(*++t_wp)) || res;
	t_wp--;
	return res;
}

static int
aexpr(n)
	enum token n;
{
	int res;

	res = nexpr(n);
	if (t_lex(*++t_wp) == BAND)
		return aexpr(t_lex(*++t_wp)) && res;
	t_wp--;
	return res;
}

static int
nexpr(n)
	enum token n;			/* token */
{
	if (n == UNOT)
		return !nexpr(t_lex(*++t_wp));
	return primary(n);
}

static int
primary(n)
	enum token n;
{
	enum token nn;
	int res;

	if (n == EOI)
		return 0;		/* missing expression */
	if (n == LPAREN) {
		if ((nn = t_lex(*++t_wp)) == RPAREN)
			return 0;	/* missing expression */
		res = oexpr(nn);
		if (t_lex(*++t_wp) != RPAREN)
			syntax(NULL, "closing paren expected");
		return res;
	}
	if (t_wp_op && t_wp_op->op_type == UNOP) {
		/* unary expression */
		if (*++t_wp == NULL)
			syntax(t_wp_op->op_text, "argument expected");
		switch (n) {
		case STREZ:
			return strlen(*t_wp) == 0;
		case STRNZ:
			return strlen(*t_wp) != 0;
		case FILTT:
			return isatty(getn(*t_wp));
		default:
			return filstat(*t_wp, n);
		}
	}

	if (t_lex(t_wp[1]), t_wp_op && t_wp_op->op_type == BINOP) {
		return binop();
	}

	return strlen(*t_wp) > 0;
}

static int
binop()
{
	const char *opnd1, *opnd2;
	struct t_op const *op;

	opnd1 = *t_wp;
	(void) t_lex(*++t_wp);
	op = t_wp_op;

	if ((opnd2 = *++t_wp) == NULL)
		syntax(op->op_text, "argument expected");

	switch (op->op_num) {
	case STREQ:
		return strcmp(opnd1, opnd2) == 0;
	case STRNE:
		return strcmp(opnd1, opnd2) != 0;
	case STRLT:
		return strcmp(opnd1, opnd2) < 0;
	case STRGT:
		return strcmp(opnd1, opnd2) > 0;
	case INTEQ:
		return intcmp(opnd1, opnd2) == 0;
	case INTNE:
		return intcmp(opnd1, opnd2) != 0;
	case INTGE:
		return intcmp(opnd1, opnd2) >= 0;
	case INTGT:
		return intcmp(opnd1, opnd2) > 0;
	case INTLE:
		return intcmp(opnd1, opnd2) <= 0;
	case INTLT:
		return intcmp(opnd1, opnd2) < 0;
	case FILNT:
		return newerf (opnd1, opnd2);
	case FILOT:
		return olderf (opnd1, opnd2);
	case FILEQ:
		return equalf (opnd1, opnd2);
	default:
		abort();
		/* NOTREACHED */
	}
}

static int
filstat(nm, mode)
	char *nm;
	enum token mode;
{
	struct stat s;

	if (mode == FILSYM ? lstat(nm, &s) : stat(nm, &s))
		return 0;

	switch (mode) {
	case FILRD:
		return access(nm, R_OK) == 0;
	case FILWR:
		return access(nm, W_OK) == 0;
	case FILEX:
		/* XXX work around access(2) false positives for superuser */
		if (access(nm, X_OK) != 0)
			return 0;
		if (S_ISDIR(s.st_mode) || getuid() != 0)
			return 1;
		return (s.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
	case FILEXIST:
		return access(nm, F_OK) == 0;
	case FILREG:
		return S_ISREG(s.st_mode);
	case FILDIR:
		return S_ISDIR(s.st_mode);
	case FILCDEV:
		return S_ISCHR(s.st_mode);
	case FILBDEV:
		return S_ISBLK(s.st_mode);
	case FILFIFO:
		return S_ISFIFO(s.st_mode);
	case FILSOCK:
		return S_ISSOCK(s.st_mode);
	case FILSYM:
		return S_ISLNK(s.st_mode);
	case FILSUID:
		return (s.st_mode & S_ISUID) != 0;
	case FILSGID:
		return (s.st_mode & S_ISGID) != 0;
	case FILSTCK:
		return (s.st_mode & S_ISVTX) != 0;
	case FILGZ:
		return s.st_size > (off_t)0;
	case FILUID:
		return s.st_uid == geteuid();
	case FILGID:
		return s.st_gid == getegid();
	default:
		return 1;
	}
}

static enum token
t_lex(s)
	char *s;
{
	struct t_op const *op = ops;

	if (s == 0) {
		t_wp_op = NULL;
		return EOI;
	}
	while (op->op_text) {
		if (strcmp(s, op->op_text) == 0) {
			if ((op->op_type == UNOP && isoperand()) ||
			    (op->op_num == LPAREN && *(t_wp+1) == 0))
				break;
			t_wp_op = op;
			return op->op_num;
		}
		op++;
	}
	t_wp_op = NULL;
	return OPERAND;
}

static int
isoperand()
{
	struct t_op const *op = ops;
	char *s;
	char *t;

	if ((s  = *(t_wp+1)) == 0)
		return 1;
	if ((t = *(t_wp+2)) == 0)
		return 0;
	while (op->op_text) {
		if (strcmp(s, op->op_text) == 0)
			return op->op_type == BINOP &&
			    (t[0] != ')' || t[1] != '\0');
		op++;
	}
	return 0;
}

/* atoi with error detection */
static int
getn(s)
	const char *s;
{
	char *p;
	long r;

	errno = 0;
	r = strtol(s, &p, 10);

	if (errno != 0)
	  error("%s: out of range", s);

	while (isspace((unsigned char)*p))
	  p++;

	if (*p)
	  error("%s: bad number", s);

	return (int) r;
}

/* atoi with error detection and 64 bit range */
static quad_t
getq(s)
	const char *s;
{
	char *p;
	quad_t r;

	errno = 0;
	r = strtoq(s, &p, 10);

	if (errno != 0)
	  error("%s: out of range", s);

	while (isspace((unsigned char)*p))
	  p++;

	if (*p)
	  error("%s: bad number", s);

	return r;
}

static int
intcmp (s1, s2)
	const char *s1, *s2;
{
	quad_t q1, q2;


	q1 = getq(s1);
	q2 = getq(s2);

	if (q1 > q2)
		return 1;

	if (q1 < q2)
		return -1;

	return 0;
}

static int
newerf (f1, f2)
	const char *f1, *f2;
{
	struct stat b1, b2;

	return (stat (f1, &b1) == 0 &&
		stat (f2, &b2) == 0 &&
		b1.st_mtime > b2.st_mtime);
}

static int
olderf (f1, f2)
	const char *f1, *f2;
{
	struct stat b1, b2;

	return (stat (f1, &b1) == 0 &&
		stat (f2, &b2) == 0 &&
		b1.st_mtime < b2.st_mtime);
}

static int
equalf (f1, f2)
	const char *f1, *f2;
{
	struct stat b1, b2;

	return (stat (f1, &b1) == 0 &&
		stat (f2, &b2) == 0 &&
		b1.st_dev == b2.st_dev &&
		b1.st_ino == b2.st_ino);
}
