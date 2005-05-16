#include <stdio.h>
#include "awk.h"
#include "ytab.h"

static char *printname[92] = {
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
	(char *) "AND",	/* 279 */
	(char *) "BOR",	/* 280 */
	(char *) "APPEND",	/* 281 */
	(char *) "EQ",	/* 282 */
	(char *) "GE",	/* 283 */
	(char *) "GT",	/* 284 */
	(char *) "LE",	/* 285 */
	(char *) "LT",	/* 286 */
	(char *) "NE",	/* 287 */
	(char *) "IN",	/* 288 */
	(char *) "ARG",	/* 289 */
	(char *) "BLTIN",	/* 290 */
	(char *) "BREAK",	/* 291 */
	(char *) "CLOSE",	/* 292 */
	(char *) "CONTINUE",	/* 293 */
	(char *) "DELETE",	/* 294 */
	(char *) "DO",	/* 295 */
	(char *) "EXIT",	/* 296 */
	(char *) "FOR",	/* 297 */
	(char *) "FUNC",	/* 298 */
	(char *) "SUB",	/* 299 */
	(char *) "GSUB",	/* 300 */
	(char *) "IF",	/* 301 */
	(char *) "INDEX",	/* 302 */
	(char *) "LSUBSTR",	/* 303 */
	(char *) "MATCHFCN",	/* 304 */
	(char *) "NEXT",	/* 305 */
	(char *) "NEXTFILE",	/* 306 */
	(char *) "ADD",	/* 307 */
	(char *) "MINUS",	/* 308 */
	(char *) "MULT",	/* 309 */
	(char *) "DIVIDE",	/* 310 */
	(char *) "MOD",	/* 311 */
	(char *) "ASSIGN",	/* 312 */
	(char *) "ASGNOP",	/* 313 */
	(char *) "ADDEQ",	/* 314 */
	(char *) "SUBEQ",	/* 315 */
	(char *) "MULTEQ",	/* 316 */
	(char *) "DIVEQ",	/* 317 */
	(char *) "MODEQ",	/* 318 */
	(char *) "POWEQ",	/* 319 */
	(char *) "PRINT",	/* 320 */
	(char *) "PRINTF",	/* 321 */
	(char *) "SPRINTF",	/* 322 */
	(char *) "ELSE",	/* 323 */
	(char *) "INTEST",	/* 324 */
	(char *) "CONDEXPR",	/* 325 */
	(char *) "POSTINCR",	/* 326 */
	(char *) "PREINCR",	/* 327 */
	(char *) "POSTDECR",	/* 328 */
	(char *) "PREDECR",	/* 329 */
	(char *) "VAR",	/* 330 */
	(char *) "IVAR",	/* 331 */
	(char *) "VARNF",	/* 332 */
	(char *) "CALL",	/* 333 */
	(char *) "NUMBER",	/* 334 */
	(char *) "STRING",	/* 335 */
	(char *) "REGEXPR",	/* 336 */
	(char *) "GETLINE",	/* 337 */
	(char *) "SUBSTR",	/* 338 */
	(char *) "SPLIT",	/* 339 */
	(char *) "RETURN",	/* 340 */
	(char *) "WHILE",	/* 341 */
	(char *) "CAT",	/* 342 */
	(char *) "UMINUS",	/* 343 */
	(char *) "NOT",	/* 344 */
	(char *) "POWER",	/* 345 */
	(char *) "INCR",	/* 346 */
	(char *) "DECR",	/* 347 */
	(char *) "INDIRECT",	/* 348 */
	(char *) "LASTTOKEN",	/* 349 */
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
