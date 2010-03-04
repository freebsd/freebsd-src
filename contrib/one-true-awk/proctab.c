#include <stdio.h>
#include "awk.h"
#include "ytab.h"

static char *printname[93] = {
	(char *) "FIRSTTOKEN",	/* 258 */
	(char *) "PROGRAM",	/* 259 */
	(char *) "PASTAT",	/* 260 */
	(char *) "PASTAT2",	/* 261 */
	(char *) "XBEGIN",	/* 262 */
	(char *) "XEND",	/* 263 */
	(char *) "NL",	/* 264 */
	(char *) "ARRAY",	/* 265 */
	(char *) "MATCH",	/* 266 */
	(char *) "NOTMATCH",	/* 267 */
	(char *) "MATCHOP",	/* 268 */
	(char *) "FINAL",	/* 269 */
	(char *) "DOT",	/* 270 */
	(char *) "ALL",	/* 271 */
	(char *) "CCL",	/* 272 */
	(char *) "NCCL",	/* 273 */
	(char *) "CHAR",	/* 274 */
	(char *) "OR",	/* 275 */
	(char *) "STAR",	/* 276 */
	(char *) "QUEST",	/* 277 */
	(char *) "PLUS",	/* 278 */
	(char *) "EMPTYRE",	/* 279 */
	(char *) "AND",	/* 280 */
	(char *) "BOR",	/* 281 */
	(char *) "APPEND",	/* 282 */
	(char *) "EQ",	/* 283 */
	(char *) "GE",	/* 284 */
	(char *) "GT",	/* 285 */
	(char *) "LE",	/* 286 */
	(char *) "LT",	/* 287 */
	(char *) "NE",	/* 288 */
	(char *) "IN",	/* 289 */
	(char *) "ARG",	/* 290 */
	(char *) "BLTIN",	/* 291 */
	(char *) "BREAK",	/* 292 */
	(char *) "CLOSE",	/* 293 */
	(char *) "CONTINUE",	/* 294 */
	(char *) "DELETE",	/* 295 */
	(char *) "DO",	/* 296 */
	(char *) "EXIT",	/* 297 */
	(char *) "FOR",	/* 298 */
	(char *) "FUNC",	/* 299 */
	(char *) "SUB",	/* 300 */
	(char *) "GSUB",	/* 301 */
	(char *) "IF",	/* 302 */
	(char *) "INDEX",	/* 303 */
	(char *) "LSUBSTR",	/* 304 */
	(char *) "MATCHFCN",	/* 305 */
	(char *) "NEXT",	/* 306 */
	(char *) "NEXTFILE",	/* 307 */
	(char *) "ADD",	/* 308 */
	(char *) "MINUS",	/* 309 */
	(char *) "MULT",	/* 310 */
	(char *) "DIVIDE",	/* 311 */
	(char *) "MOD",	/* 312 */
	(char *) "ASSIGN",	/* 313 */
	(char *) "ASGNOP",	/* 314 */
	(char *) "ADDEQ",	/* 315 */
	(char *) "SUBEQ",	/* 316 */
	(char *) "MULTEQ",	/* 317 */
	(char *) "DIVEQ",	/* 318 */
	(char *) "MODEQ",	/* 319 */
	(char *) "POWEQ",	/* 320 */
	(char *) "PRINT",	/* 321 */
	(char *) "PRINTF",	/* 322 */
	(char *) "SPRINTF",	/* 323 */
	(char *) "ELSE",	/* 324 */
	(char *) "INTEST",	/* 325 */
	(char *) "CONDEXPR",	/* 326 */
	(char *) "POSTINCR",	/* 327 */
	(char *) "PREINCR",	/* 328 */
	(char *) "POSTDECR",	/* 329 */
	(char *) "PREDECR",	/* 330 */
	(char *) "VAR",	/* 331 */
	(char *) "IVAR",	/* 332 */
	(char *) "VARNF",	/* 333 */
	(char *) "CALL",	/* 334 */
	(char *) "NUMBER",	/* 335 */
	(char *) "STRING",	/* 336 */
	(char *) "REGEXPR",	/* 337 */
	(char *) "GETLINE",	/* 338 */
	(char *) "SUBSTR",	/* 339 */
	(char *) "SPLIT",	/* 340 */
	(char *) "RETURN",	/* 341 */
	(char *) "WHILE",	/* 342 */
	(char *) "CAT",	/* 343 */
	(char *) "UMINUS",	/* 344 */
	(char *) "NOT",	/* 345 */
	(char *) "POWER",	/* 346 */
	(char *) "INCR",	/* 347 */
	(char *) "DECR",	/* 348 */
	(char *) "INDIRECT",	/* 349 */
	(char *) "LASTTOKEN",	/* 350 */
};


Cell *(*proctab[93])(Node **, int) = {
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
	nullproc,	/* EMPTYRE */
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
	awkgetline,	/* GETLINE */
	substr,	/* SUBSTR */
	split,	/* SPLIT */
	jump,	/* RETURN */
	whilestat,	/* WHILE */
	cat,	/* CAT */
	arith,	/* UMINUS */
	boolop,	/* NOT */
	arith,	/* POWER */
	nullproc,	/* INCR */
	nullproc,	/* DECR */
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
