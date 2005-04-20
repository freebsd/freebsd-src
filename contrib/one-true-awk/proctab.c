#include <stdio.h>
#include "awk.h"
#include "ytab.h"

static char *printname[92] = {
	(char *) "FIRSTTOKEN",	/* 57346 */
	(char *) "PROGRAM",	/* 57347 */
	(char *) "PASTAT",	/* 57348 */
	(char *) "PASTAT2",	/* 57349 */
	(char *) "XBEGIN",	/* 57350 */
	(char *) "XEND",	/* 57351 */
	(char *) "NL",	/* 57352 */
	(char *) "ARRAY",	/* 57353 */
	(char *) "MATCH",	/* 57354 */
	(char *) "NOTMATCH",	/* 57355 */
	(char *) "MATCHOP",	/* 57356 */
	(char *) "FINAL",	/* 57357 */
	(char *) "DOT",	/* 57358 */
	(char *) "ALL",	/* 57359 */
	(char *) "CCL",	/* 57360 */
	(char *) "NCCL",	/* 57361 */
	(char *) "CHAR",	/* 57362 */
	(char *) "OR",	/* 57363 */
	(char *) "STAR",	/* 57364 */
	(char *) "QUEST",	/* 57365 */
	(char *) "PLUS",	/* 57366 */
	(char *) "AND",	/* 57367 */
	(char *) "BOR",	/* 57368 */
	(char *) "APPEND",	/* 57369 */
	(char *) "EQ",	/* 57370 */
	(char *) "GE",	/* 57371 */
	(char *) "GT",	/* 57372 */
	(char *) "LE",	/* 57373 */
	(char *) "LT",	/* 57374 */
	(char *) "NE",	/* 57375 */
	(char *) "IN",	/* 57376 */
	(char *) "ARG",	/* 57377 */
	(char *) "BLTIN",	/* 57378 */
	(char *) "BREAK",	/* 57379 */
	(char *) "CLOSE",	/* 57380 */
	(char *) "CONTINUE",	/* 57381 */
	(char *) "DELETE",	/* 57382 */
	(char *) "DO",	/* 57383 */
	(char *) "EXIT",	/* 57384 */
	(char *) "FOR",	/* 57385 */
	(char *) "FUNC",	/* 57386 */
	(char *) "SUB",	/* 57387 */
	(char *) "GSUB",	/* 57388 */
	(char *) "IF",	/* 57389 */
	(char *) "INDEX",	/* 57390 */
	(char *) "LSUBSTR",	/* 57391 */
	(char *) "MATCHFCN",	/* 57392 */
	(char *) "NEXT",	/* 57393 */
	(char *) "NEXTFILE",	/* 57394 */
	(char *) "ADD",	/* 57395 */
	(char *) "MINUS",	/* 57396 */
	(char *) "MULT",	/* 57397 */
	(char *) "DIVIDE",	/* 57398 */
	(char *) "MOD",	/* 57399 */
	(char *) "ASSIGN",	/* 57400 */
	(char *) "ASGNOP",	/* 57401 */
	(char *) "ADDEQ",	/* 57402 */
	(char *) "SUBEQ",	/* 57403 */
	(char *) "MULTEQ",	/* 57404 */
	(char *) "DIVEQ",	/* 57405 */
	(char *) "MODEQ",	/* 57406 */
	(char *) "POWEQ",	/* 57407 */
	(char *) "PRINT",	/* 57408 */
	(char *) "PRINTF",	/* 57409 */
	(char *) "SPRINTF",	/* 57410 */
	(char *) "ELSE",	/* 57411 */
	(char *) "INTEST",	/* 57412 */
	(char *) "CONDEXPR",	/* 57413 */
	(char *) "POSTINCR",	/* 57414 */
	(char *) "PREINCR",	/* 57415 */
	(char *) "POSTDECR",	/* 57416 */
	(char *) "PREDECR",	/* 57417 */
	(char *) "VAR",	/* 57418 */
	(char *) "IVAR",	/* 57419 */
	(char *) "VARNF",	/* 57420 */
	(char *) "CALL",	/* 57421 */
	(char *) "NUMBER",	/* 57422 */
	(char *) "STRING",	/* 57423 */
	(char *) "REGEXPR",	/* 57424 */
	(char *) "GETLINE",	/* 57425 */
	(char *) "RETURN",	/* 57426 */
	(char *) "SPLIT",	/* 57427 */
	(char *) "SUBSTR",	/* 57428 */
	(char *) "WHILE",	/* 57429 */
	(char *) "CAT",	/* 57430 */
	(char *) "NOT",	/* 57431 */
	(char *) "UMINUS",	/* 57432 */
	(char *) "POWER",	/* 57433 */
	(char *) "DECR",	/* 57434 */
	(char *) "INCR",	/* 57435 */
	(char *) "INDIRECT",	/* 57436 */
	(char *) "LASTTOKEN",	/* 57437 */
};


Cell *(*proctab[92])(Node **, int) = {
	nullproc,	/* FIRSTTOKEN */
	program,	/* PROGRAM */
	pastat,	/* PASTAT */
	dopa2,	/* PASTAT2 */
	nullproc,	/* XBEGIN */
	nullproc,	/* XEND */
	nullproc,	/* NL */
	array,	/* ARRAY */
	matchop,	/* MATCH */
	matchop,	/* NOTMATCH */
	nullproc,	/* MATCHOP */
	nullproc,	/* FINAL */
	nullproc,	/* DOT */
	nullproc,	/* ALL */
	nullproc,	/* CCL */
	nullproc,	/* NCCL */
	nullproc,	/* CHAR */
	nullproc,	/* OR */
	nullproc,	/* STAR */
	nullproc,	/* QUEST */
	nullproc,	/* PLUS */
	boolop,	/* AND */
	boolop,	/* BOR */
	nullproc,	/* APPEND */
	relop,	/* EQ */
	relop,	/* GE */
	relop,	/* GT */
	relop,	/* LE */
	relop,	/* LT */
	relop,	/* NE */
	instat,	/* IN */
	arg,	/* ARG */
	bltin,	/* BLTIN */
	jump,	/* BREAK */
	closefile,	/* CLOSE */
	jump,	/* CONTINUE */
	awkdelete,	/* DELETE */
	dostat,	/* DO */
	jump,	/* EXIT */
	forstat,	/* FOR */
	nullproc,	/* FUNC */
	sub,	/* SUB */
	gsub,	/* GSUB */
	ifstat,	/* IF */
	sindex,	/* INDEX */
	nullproc,	/* LSUBSTR */
	matchop,	/* MATCHFCN */
	jump,	/* NEXT */
	jump,	/* NEXTFILE */
	arith,	/* ADD */
	arith,	/* MINUS */
	arith,	/* MULT */
	arith,	/* DIVIDE */
	arith,	/* MOD */
	assign,	/* ASSIGN */
	nullproc,	/* ASGNOP */
	assign,	/* ADDEQ */
	assign,	/* SUBEQ */
	assign,	/* MULTEQ */
	assign,	/* DIVEQ */
	assign,	/* MODEQ */
	assign,	/* POWEQ */
	printstat,	/* PRINT */
	awkprintf,	/* PRINTF */
	awksprintf,	/* SPRINTF */
	nullproc,	/* ELSE */
	intest,	/* INTEST */
	condexpr,	/* CONDEXPR */
	incrdecr,	/* POSTINCR */
	incrdecr,	/* PREINCR */
	incrdecr,	/* POSTDECR */
	incrdecr,	/* PREDECR */
	nullproc,	/* VAR */
	nullproc,	/* IVAR */
	getnf,	/* VARNF */
	call,	/* CALL */
	nullproc,	/* NUMBER */
	nullproc,	/* STRING */
	nullproc,	/* REGEXPR */
	getline,	/* GETLINE */
	jump,	/* RETURN */
	split,	/* SPLIT */
	substr,	/* SUBSTR */
	whilestat,	/* WHILE */
	cat,	/* CAT */
	boolop,	/* NOT */
	arith,	/* UMINUS */
	arith,	/* POWER */
	nullproc,	/* DECR */
	nullproc,	/* INCR */
	indirect,	/* INDIRECT */
	nullproc,	/* LASTTOKEN */
};

char *tokname(int n)
{
	static char buf[100];

	if (n < FIRSTTOKEN || n > LASTTOKEN) {
		sprintf(buf, "token %d", n);
		return buf;
	}
	return printname[n-FIRSTTOKEN];
}
