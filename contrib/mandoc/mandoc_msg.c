/* $OpenBSD: mandoc_msg.c,v 1.8 2020/01/19 17:59:01 schwarze Exp $ */
/*
 * Copyright (c) 2014-2021 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Implementation of warning and error messages for mandoc(1).
 */
#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "mandoc.h"

static	const enum mandocerr lowest_type[MANDOCLEVEL_MAX] = {
	MANDOCERR_OK,
	MANDOCERR_OK,
	MANDOCERR_WARNING,
	MANDOCERR_ERROR,
	MANDOCERR_UNSUPP,
	MANDOCERR_BADARG,
	MANDOCERR_SYSERR
};

static	const char *const level_name[MANDOCLEVEL_MAX] = {
	"SUCCESS",
	"STYLE",
	"WARNING",
	"ERROR",
	"UNSUPP",
	"BADARG",
	"SYSERR"
};

static	const char *const type_message[MANDOCERR_MAX] = {
	"ok",

	"base system convention",

	"Mdocdate found",
	"Mdocdate missing",
	"unknown architecture",
	"operating system explicitly specified",
	"RCS id missing",

	"generic style suggestion",

	"legacy man(7) date format",
	"normalizing date format to",
	"lower case character in document title",
	"duplicate RCS id",
	"possible typo in section name",
	"unterminated quoted argument",
	"useless macro",
	"consider using OS macro",
	"errnos out of order",
	"duplicate errno",
	"referenced manual not found",
	"trailing delimiter",
	"no blank before trailing delimiter",
	"fill mode already enabled, skipping",
	"fill mode already disabled, skipping",
	"input text line longer than 80 bytes",
	"verbatim \"--\", maybe consider using \\(em",
	"function name without markup",
	"whitespace at end of input line",
	"bad comment style",

	"generic warning",

	/* related to the prologue */
	"missing manual title, using UNTITLED",
	"missing manual title, using \"\"",
	"missing manual section, using \"\"",
	"unknown manual section",
	"filename/section mismatch",
	"missing date, using \"\"",
	"cannot parse date, using it verbatim",
	"date in the future, using it anyway",
	"missing Os macro, using \"\"",
	"late prologue macro",
	"prologue macros out of order",

	/* related to document structure */
	".so is fragile, better use ln(1)",
	"no document body",
	"content before first section header",
	"first section is not \"NAME\"",
	"NAME section without Nm before Nd",
	"NAME section without description",
	"description not at the end of NAME",
	"bad NAME section content",
	"missing comma before name",
	"missing description line, using \"\"",
	"description line outside NAME section",
	"sections out of conventional order",
	"duplicate section title",
	"unexpected section",
	"cross reference to self",
	"unusual Xr order",
	"unusual Xr punctuation",
	"AUTHORS section without An macro",

	/* related to macros and nesting */
	"obsolete macro",
	"macro neither callable nor escaped",
	"skipping paragraph macro",
	"moving paragraph macro out of list",
	"skipping no-space macro",
	"blocks badly nested",
	"nested displays are not portable",
	"moving content out of list",
	"first macro on line",
	"line scope broken",
	"skipping blank line in line scope",

	/* related to missing macro arguments */
	"skipping empty request",
	"conditional request controls empty scope",
	"skipping empty macro",
	"empty block",
	"empty argument, using 0n",
	"missing display type, using -ragged",
	"list type is not the first argument",
	"missing -width in -tag list, using 6n",
	"missing utility name, using \"\"",
	"missing function name, using \"\"",
	"empty head in list item",
	"empty list item",
	"missing argument, using next line",
	"missing font type, using \\fR",
	"unknown font type, using \\fR",
	"nothing follows prefix",
	"empty reference block",
	"missing section argument",
	"missing -std argument, adding it",
	"missing option string, using \"\"",
	"missing resource identifier, using \"\"",
	"missing eqn box, using \"\"",

	/* related to bad macro arguments */
	"duplicate argument",
	"skipping duplicate argument",
	"skipping duplicate display type",
	"skipping duplicate list type",
	"skipping -width argument",
	"wrong number of cells",
	"unknown AT&T UNIX version",
	"comma in function argument",
	"parenthesis in function name",
	"unknown library name",
	"invalid content in Rs block",
	"invalid Boolean argument",
	"argument contains two font escapes",
	"unknown font, skipping request",
	"odd number of characters in request",

	/* related to plain text */
	"blank line in fill mode, using .sp",
	"tab in filled text",
	"new sentence, new line",
	"invalid escape sequence",
	"undefined escape, printing literally",
	"undefined string, using \"\"",

	/* related to tables */
	"tbl line starts with span",
	"tbl column starts with span",
	"skipping vertical bar in tbl layout",

	"generic error",

	/* related to tables */
	"non-alphabetic character in tbl options",
	"skipping unknown tbl option",
	"missing tbl option argument",
	"wrong tbl option argument size",
	"empty tbl layout",
	"invalid character in tbl layout",
	"unmatched parenthesis in tbl layout",
	"ignoring excessive spacing in tbl layout",
	"tbl without any data cells",
	"ignoring data in spanned tbl cell",
	"ignoring extra tbl data cells",
	"data block open at end of tbl",

	/* related to document structure and macros */
	"duplicate prologue macro",
	"skipping late title macro",
	"input stack limit exceeded, infinite loop?",
	"skipping bad character",
	"skipping unknown macro",
	"ignoring request outside macro",
	"skipping insecure request",
	"skipping item outside list",
	"skipping column outside column list",
	"skipping end of block that is not open",
	"fewer RS blocks open, skipping",
	"inserting missing end of block",
	"appending missing end of block",

	/* related to request and macro arguments */
	"escaped character not allowed in a name",
	"using macro argument outside macro",
	"argument number is not numeric",
	"NOT IMPLEMENTED: Bd -file",
	"skipping display without arguments",
	"missing list type, using -item",
	"argument is not numeric, using 1",
	"argument is not a character",
	"missing manual name, using \"\"",
	"uname(3) system call failed, using UNKNOWN",
	"unknown standard specifier",
	"skipping request without numeric argument",
	"excessive shift",
	"NOT IMPLEMENTED: .so with absolute path or \"..\"",
	".so request failed",
	"skipping tag containing whitespace",
	"skipping all arguments",
	"skipping excess arguments",
	"divide by zero",

	"unsupported feature",
	"input too large",
	"unsupported control character",
	"unsupported escape sequence",
	"unsupported roff request",
	"nested .while loops",
	"end of scope with open .while loop",
	"end of .while loop in inner scope",
	"cannot continue this .while loop",
	"eqn delim option in tbl",
	"unsupported tbl layout modifier",
	"ignoring macro in table",
	"skipping tbl in -Tman mode",
	"skipping eqn in -Tman mode",

	/* bad command line arguments */
	NULL,
	"bad command line argument",
	"duplicate command line argument",
	"option has a superfluous value",
	"missing option value",
	"bad option value",
	"duplicate option value",
	"no such tag",
	"-Tmarkdown unsupported for man(7) input",

	/* system errors */
	NULL,
	"dup",
	"exec",
	"fdopen",
	"fflush",
	"fork",
	"fstat",
	"getline",
	"glob",
	"gzclose",
	"gzdopen",
	"mkstemp",
	"open",
	"pledge",
	"read",
	"wait",
	"write",
};

static	FILE		*fileptr = NULL;
static	const char	*filename = NULL;
static	enum mandocerr	 min_type = MANDOCERR_BADARG;
static	enum mandoclevel rc = MANDOCLEVEL_OK;


void
mandoc_msg_setoutfile(FILE *fp)
{
	fileptr = fp;
}

const char *
mandoc_msg_getinfilename(void)
{
	return filename;
}

void
mandoc_msg_setinfilename(const char *fn)
{
	filename = fn;
}

enum mandocerr
mandoc_msg_getmin(void)
{
	return min_type;
}

void
mandoc_msg_setmin(enum mandocerr t)
{
	min_type = t;
}

enum mandoclevel
mandoc_msg_getrc(void)
{
	return rc;
}

void
mandoc_msg_setrc(enum mandoclevel level)
{
	if (rc < level)
		rc = level;
}

void
mandoc_msg(enum mandocerr t, int line, int col, const char *fmt, ...)
{
	va_list			 ap;
	enum mandoclevel	 level;

	if (t < min_type)
		return;

	level = MANDOCLEVEL_SYSERR;
	while (t < lowest_type[level])
		level--;
	mandoc_msg_setrc(level);

	if (fileptr == NULL)
		return;

	fprintf(fileptr, "%s:", getprogname());
	if (filename != NULL)
		fprintf(fileptr, " %s:", filename);

	if (line > 0)
		fprintf(fileptr, "%d:%d:", line, col + 1);

	fprintf(fileptr, " %s", level_name[level]);
	if (type_message[t] != NULL)
		fprintf(fileptr, ": %s", type_message[t]);

	if (fmt != NULL) {
		fprintf(fileptr, ": ");
		va_start(ap, fmt);
		vfprintf(fileptr, fmt, ap);
		va_end(ap);
	}
	fputc('\n', fileptr);
}

void
mandoc_msg_summary(void)
{
	if (fileptr != NULL && rc != MANDOCLEVEL_OK)
		fprintf(fileptr,
		    "%s: see above the output for %s messages\n",
		    getprogname(), level_name[rc]);
}
