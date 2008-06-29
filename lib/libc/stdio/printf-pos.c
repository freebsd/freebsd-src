/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)vfprintf.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This is the code responsible for handling positional arguments
 * (%m$ and %m$.n$) for vfprintf() and vfwprintf().
 */

#include "namespace.h"
#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <printf.h>

#include <stdarg.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "local.h"
#include "fvwrite.h"
#include "printflocal.h"

/*
 * Type ids for argument type table.
 */
enum typeid {
	T_UNUSED, TP_SHORT, T_INT, T_U_INT, TP_INT,
	T_LONG, T_U_LONG, TP_LONG, T_LLONG, T_U_LLONG, TP_LLONG,
	T_PTRDIFFT, TP_PTRDIFFT, T_SIZET, TP_SIZET,
	T_INTMAXT, T_UINTMAXT, TP_INTMAXT, TP_VOID, TP_CHAR, TP_SCHAR,
	T_DOUBLE, T_LONG_DOUBLE, T_WINT, TP_WCHAR
};

/* An expandable array of types. */
struct typetable {
	enum typeid *table; /* table of types */
	enum typeid stattable[STATIC_ARG_TBL_SIZE];
	int tablesize;		/* current size of type table */
	int tablemax;		/* largest used index in table */
	int nextarg;		/* 1-based argument index */
};

static void	__grow_type_table(struct typetable *);

/*
 * Initialize a struct typetable.
 */
static inline void
inittypes(struct typetable *types)
{
	int n;

	types->table = types->stattable;
	types->tablesize = STATIC_ARG_TBL_SIZE;
	types->tablemax = 0; 
	types->nextarg = 1;
	for (n = 0; n < STATIC_ARG_TBL_SIZE; n++)
		types->table[n] = T_UNUSED;
}

/*
 * struct typetable destructor.
 */ 
static inline void
freetypes(struct typetable *types)
{

	if (types->table != types->stattable)
		free (types->table);
}

/*
 * Add an argument type to the table, expanding if necessary.
 */
static inline void
addtype(struct typetable *types, enum typeid type)
{

	if (types->nextarg >= types->tablesize)
		__grow_type_table(types);
	if (types->nextarg > types->tablemax)
		types->tablemax = types->nextarg;
	types->table[types->nextarg++] = type;
}

static inline void
addsarg(struct typetable *types, int flags)
{

	if (flags & INTMAXT)
		addtype(types, T_INTMAXT);
	else if (flags & SIZET)
		addtype(types, T_SIZET);
	else if (flags & PTRDIFFT)
		addtype(types, T_PTRDIFFT);
	else if (flags & LLONGINT)
		addtype(types, T_LLONG);
	else if (flags & LONGINT)
		addtype(types, T_LONG);
	else
		addtype(types, T_INT);
}

static inline void
adduarg(struct typetable *types, int flags)
{

	if (flags & INTMAXT)
		addtype(types, T_UINTMAXT);
	else if (flags & SIZET)
		addtype(types, T_SIZET);
	else if (flags & PTRDIFFT)
		addtype(types, T_PTRDIFFT);
	else if (flags & LLONGINT)
		addtype(types, T_U_LLONG);
	else if (flags & LONGINT)
		addtype(types, T_U_LONG);
	else
		addtype(types, T_U_INT);
}

/*
 * Add * arguments to the type array.
 */
static inline void
addaster(struct typetable *types, char **fmtp)
{
	char *cp;
	int n2;

	n2 = 0;
	cp = *fmtp;
	while (is_digit(*cp)) {
		n2 = 10 * n2 + to_digit(*cp);
		cp++;
	}
	if (*cp == '$') {
		int hold = types->nextarg;
		types->nextarg = n2;
		addtype(types, T_INT);
		types->nextarg = hold;
		*fmtp = ++cp;
	} else {
		addtype(types, T_INT);
	}
}

static inline void
addwaster(struct typetable *types, wchar_t **fmtp)
{
	wchar_t *cp;
	int n2;

	n2 = 0;
	cp = *fmtp;
	while (is_digit(*cp)) {
		n2 = 10 * n2 + to_digit(*cp);
		cp++;
	}
	if (*cp == '$') {
		int hold = types->nextarg;
		types->nextarg = n2;
		addtype(types, T_INT);
		types->nextarg = hold;
		*fmtp = ++cp;
	} else {
		addtype(types, T_INT);
	}
}

/*
 * Find all arguments when a positional parameter is encountered.  Returns a
 * table, indexed by argument number, of pointers to each arguments.  The
 * initial argument table should be an array of STATIC_ARG_TBL_SIZE entries.
 * It will be replaces with a malloc-ed one if it overflows.
 */ 
void
__find_arguments (const char *fmt0, va_list ap, union arg **argtable)
{
	char *fmt;		/* format string */
	int ch;			/* character from fmt */
	int n;			/* handy integer (short term usage) */
	int flags;		/* flags as above */
	int width;		/* width from format (%8d), or 0 */
	struct typetable types;	/* table of types */

	fmt = (char *)fmt0;
	inittypes(&types);

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		while ((ch = *fmt) != '\0' && ch != '%')
			fmt++;
		if (ch == '\0')
			goto done;
		fmt++;		/* skip over '%' */

		flags = 0;
		width = 0;

rflag:		ch = *fmt++;
reswitch:	switch (ch) {
		case ' ':
		case '#':
			goto rflag;
		case '*':
			addaster(&types, &fmt);
			goto rflag;
		case '-':
		case '+':
		case '\'':
			goto rflag;
		case '.':
			if ((ch = *fmt++) == '*') {
				addaster(&types, &fmt);
				goto rflag;
			}
			while (is_digit(ch)) {
				ch = *fmt++;
			}
			goto reswitch;
		case '0':
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			} while (is_digit(ch));
			if (ch == '$') {
				types.nextarg = n;
				goto rflag;
			}
			width = n;
			goto reswitch;
#ifndef NO_FLOATING_POINT
		case 'L':
			flags |= LONGDBL;
			goto rflag;
#endif
		case 'h':
			if (flags & SHORTINT) {
				flags &= ~SHORTINT;
				flags |= CHARINT;
			} else
				flags |= SHORTINT;
			goto rflag;
		case 'j':
			flags |= INTMAXT;
			goto rflag;
		case 'l':
			if (flags & LONGINT) {
				flags &= ~LONGINT;
				flags |= LLONGINT;
			} else
				flags |= LONGINT;
			goto rflag;
		case 'q':
			flags |= LLONGINT;	/* not necessarily */
			goto rflag;
		case 't':
			flags |= PTRDIFFT;
			goto rflag;
		case 'z':
			flags |= SIZET;
			goto rflag;
		case 'C':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'c':
			if (flags & LONGINT)
				addtype(&types, T_WINT);
			else
				addtype(&types, T_INT);
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
			addsarg(&types, flags);
			break;
#ifndef NO_FLOATING_POINT
		case 'a':
		case 'A':
		case 'e':
		case 'E':
		case 'f':
		case 'g':
		case 'G':
			if (flags & LONGDBL)
				addtype(&types, T_LONG_DOUBLE);
			else
				addtype(&types, T_DOUBLE);
			break;
#endif /* !NO_FLOATING_POINT */
		case 'n':
			if (flags & INTMAXT)
				addtype(&types, TP_INTMAXT);
			else if (flags & PTRDIFFT)
				addtype(&types, TP_PTRDIFFT);
			else if (flags & SIZET)
				addtype(&types, TP_SIZET);
			else if (flags & LLONGINT)
				addtype(&types, TP_LLONG);
			else if (flags & LONGINT)
				addtype(&types, TP_LONG);
			else if (flags & SHORTINT)
				addtype(&types, TP_SHORT);
			else if (flags & CHARINT)
				addtype(&types, TP_SCHAR);
			else
				addtype(&types, TP_INT);
			continue;	/* no output */
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
			adduarg(&types, flags);
			break;
		case 'p':
			addtype(&types, TP_VOID);
			break;
		case 'S':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 's':
			if (flags & LONGINT)
				addtype(&types, TP_WCHAR);
			else
				addtype(&types, TP_CHAR);
			break;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
		case 'X':
		case 'x':
			adduarg(&types, flags);
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
			break;
		}
	}
done:
	/*
	 * Build the argument table.
	 */
	if (types.tablemax >= STATIC_ARG_TBL_SIZE) {
		*argtable = (union arg *)
		    malloc (sizeof (union arg) * (types.tablemax + 1));
	}

	(*argtable) [0].intarg = 0;
	for (n = 1; n <= types.tablemax; n++) {
		switch (types.table[n]) {
		    case T_UNUSED: /* whoops! */
			(*argtable) [n].intarg = va_arg (ap, int);
			break;
		    case TP_SCHAR:
			(*argtable) [n].pschararg = va_arg (ap, signed char *);
			break;
		    case TP_SHORT:
			(*argtable) [n].pshortarg = va_arg (ap, short *);
			break;
		    case T_INT:
			(*argtable) [n].intarg = va_arg (ap, int);
			break;
		    case T_U_INT:
			(*argtable) [n].uintarg = va_arg (ap, unsigned int);
			break;
		    case TP_INT:
			(*argtable) [n].pintarg = va_arg (ap, int *);
			break;
		    case T_LONG:
			(*argtable) [n].longarg = va_arg (ap, long);
			break;
		    case T_U_LONG:
			(*argtable) [n].ulongarg = va_arg (ap, unsigned long);
			break;
		    case TP_LONG:
			(*argtable) [n].plongarg = va_arg (ap, long *);
			break;
		    case T_LLONG:
			(*argtable) [n].longlongarg = va_arg (ap, long long);
			break;
		    case T_U_LLONG:
			(*argtable) [n].ulonglongarg = va_arg (ap, unsigned long long);
			break;
		    case TP_LLONG:
			(*argtable) [n].plonglongarg = va_arg (ap, long long *);
			break;
		    case T_PTRDIFFT:
			(*argtable) [n].ptrdiffarg = va_arg (ap, ptrdiff_t);
			break;
		    case TP_PTRDIFFT:
			(*argtable) [n].pptrdiffarg = va_arg (ap, ptrdiff_t *);
			break;
		    case T_SIZET:
			(*argtable) [n].sizearg = va_arg (ap, size_t);
			break;
		    case TP_SIZET:
			(*argtable) [n].psizearg = va_arg (ap, size_t *);
			break;
		    case T_INTMAXT:
			(*argtable) [n].intmaxarg = va_arg (ap, intmax_t);
			break;
		    case T_UINTMAXT:
			(*argtable) [n].uintmaxarg = va_arg (ap, uintmax_t);
			break;
		    case TP_INTMAXT:
			(*argtable) [n].pintmaxarg = va_arg (ap, intmax_t *);
			break;
		    case T_DOUBLE:
#ifndef NO_FLOATING_POINT
			(*argtable) [n].doublearg = va_arg (ap, double);
#endif
			break;
		    case T_LONG_DOUBLE:
#ifndef NO_FLOATING_POINT
			(*argtable) [n].longdoublearg = va_arg (ap, long double);
#endif
			break;
		    case TP_CHAR:
			(*argtable) [n].pchararg = va_arg (ap, char *);
			break;
		    case TP_VOID:
			(*argtable) [n].pvoidarg = va_arg (ap, void *);
			break;
		    case T_WINT:
			(*argtable) [n].wintarg = va_arg (ap, wint_t);
			break;
		    case TP_WCHAR:
			(*argtable) [n].pwchararg = va_arg (ap, wchar_t *);
			break;
		}
	}

	freetypes(&types);
}

/* wchar version of __find_arguments. */
void
__find_warguments (const wchar_t *fmt0, va_list ap, union arg **argtable)
{
	wchar_t *fmt;		/* format string */
	wchar_t ch;		/* character from fmt */
	int n;			/* handy integer (short term usage) */
	int flags;		/* flags as above */
	int width;		/* width from format (%8d), or 0 */
	struct typetable types;	/* table of types */

	fmt = (wchar_t *)fmt0;
	inittypes(&types);

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		while ((ch = *fmt) != '\0' && ch != '%')
			fmt++;
		if (ch == '\0')
			goto done;
		fmt++;		/* skip over '%' */

		flags = 0;
		width = 0;

rflag:		ch = *fmt++;
reswitch:	switch (ch) {
		case ' ':
		case '#':
			goto rflag;
		case '*':
			addwaster(&types, &fmt);
			goto rflag;
		case '-':
		case '+':
		case '\'':
			goto rflag;
		case '.':
			if ((ch = *fmt++) == '*') {
				addwaster(&types, &fmt);
				goto rflag;
			}
			while (is_digit(ch)) {
				ch = *fmt++;
			}
			goto reswitch;
		case '0':
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			} while (is_digit(ch));
			if (ch == '$') {
				types.nextarg = n;
				goto rflag;
			}
			width = n;
			goto reswitch;
#ifndef NO_FLOATING_POINT
		case 'L':
			flags |= LONGDBL;
			goto rflag;
#endif
		case 'h':
			if (flags & SHORTINT) {
				flags &= ~SHORTINT;
				flags |= CHARINT;
			} else
				flags |= SHORTINT;
			goto rflag;
		case 'j':
			flags |= INTMAXT;
			goto rflag;
		case 'l':
			if (flags & LONGINT) {
				flags &= ~LONGINT;
				flags |= LLONGINT;
			} else
				flags |= LONGINT;
			goto rflag;
		case 'q':
			flags |= LLONGINT;	/* not necessarily */
			goto rflag;
		case 't':
			flags |= PTRDIFFT;
			goto rflag;
		case 'z':
			flags |= SIZET;
			goto rflag;
		case 'C':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'c':
			if (flags & LONGINT)
				addtype(&types, T_WINT);
			else
				addtype(&types, T_INT);
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
			addsarg(&types, flags);
			break;
#ifndef NO_FLOATING_POINT
		case 'a':
		case 'A':
		case 'e':
		case 'E':
		case 'f':
		case 'g':
		case 'G':
			if (flags & LONGDBL)
				addtype(&types, T_LONG_DOUBLE);
			else
				addtype(&types, T_DOUBLE);
			break;
#endif /* !NO_FLOATING_POINT */
		case 'n':
			if (flags & INTMAXT)
				addtype(&types, TP_INTMAXT);
			else if (flags & PTRDIFFT)
				addtype(&types, TP_PTRDIFFT);
			else if (flags & SIZET)
				addtype(&types, TP_SIZET);
			else if (flags & LLONGINT)
				addtype(&types, TP_LLONG);
			else if (flags & LONGINT)
				addtype(&types, TP_LONG);
			else if (flags & SHORTINT)
				addtype(&types, TP_SHORT);
			else if (flags & CHARINT)
				addtype(&types, TP_SCHAR);
			else
				addtype(&types, TP_INT);
			continue;	/* no output */
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
			adduarg(&types, flags);
			break;
		case 'p':
			addtype(&types, TP_VOID);
			break;
		case 'S':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 's':
			if (flags & LONGINT)
				addtype(&types, TP_WCHAR);
			else
				addtype(&types, TP_CHAR);
			break;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
		case 'X':
		case 'x':
			adduarg(&types, flags);
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
			break;
		}
	}
done:
	/*
	 * Build the argument table.
	 */
	if (types.tablemax >= STATIC_ARG_TBL_SIZE) {
		*argtable = (union arg *)
		    malloc (sizeof (union arg) * (types.tablemax + 1));
	}

	(*argtable) [0].intarg = 0;
	for (n = 1; n <= types.tablemax; n++) {
		switch (types.table[n]) {
		    case T_UNUSED: /* whoops! */
			(*argtable) [n].intarg = va_arg (ap, int);
			break;
		    case TP_SCHAR:
			(*argtable) [n].pschararg = va_arg (ap, signed char *);
			break;
		    case TP_SHORT:
			(*argtable) [n].pshortarg = va_arg (ap, short *);
			break;
		    case T_INT:
			(*argtable) [n].intarg = va_arg (ap, int);
			break;
		    case T_U_INT:
			(*argtable) [n].uintarg = va_arg (ap, unsigned int);
			break;
		    case TP_INT:
			(*argtable) [n].pintarg = va_arg (ap, int *);
			break;
		    case T_LONG:
			(*argtable) [n].longarg = va_arg (ap, long);
			break;
		    case T_U_LONG:
			(*argtable) [n].ulongarg = va_arg (ap, unsigned long);
			break;
		    case TP_LONG:
			(*argtable) [n].plongarg = va_arg (ap, long *);
			break;
		    case T_LLONG:
			(*argtable) [n].longlongarg = va_arg (ap, long long);
			break;
		    case T_U_LLONG:
			(*argtable) [n].ulonglongarg = va_arg (ap, unsigned long long);
			break;
		    case TP_LLONG:
			(*argtable) [n].plonglongarg = va_arg (ap, long long *);
			break;
		    case T_PTRDIFFT:
			(*argtable) [n].ptrdiffarg = va_arg (ap, ptrdiff_t);
			break;
		    case TP_PTRDIFFT:
			(*argtable) [n].pptrdiffarg = va_arg (ap, ptrdiff_t *);
			break;
		    case T_SIZET:
			(*argtable) [n].sizearg = va_arg (ap, size_t);
			break;
		    case TP_SIZET:
			(*argtable) [n].psizearg = va_arg (ap, size_t *);
			break;
		    case T_INTMAXT:
			(*argtable) [n].intmaxarg = va_arg (ap, intmax_t);
			break;
		    case T_UINTMAXT:
			(*argtable) [n].uintmaxarg = va_arg (ap, uintmax_t);
			break;
		    case TP_INTMAXT:
			(*argtable) [n].pintmaxarg = va_arg (ap, intmax_t *);
			break;
		    case T_DOUBLE:
#ifndef NO_FLOATING_POINT
			(*argtable) [n].doublearg = va_arg (ap, double);
#endif
			break;
		    case T_LONG_DOUBLE:
#ifndef NO_FLOATING_POINT
			(*argtable) [n].longdoublearg = va_arg (ap, long double);
#endif
			break;
		    case TP_CHAR:
			(*argtable) [n].pchararg = va_arg (ap, char *);
			break;
		    case TP_VOID:
			(*argtable) [n].pvoidarg = va_arg (ap, void *);
			break;
		    case T_WINT:
			(*argtable) [n].wintarg = va_arg (ap, wint_t);
			break;
		    case TP_WCHAR:
			(*argtable) [n].pwchararg = va_arg (ap, wchar_t *);
			break;
		}
	}

	freetypes(&types);
}

/*
 * Increase the size of the type table.
 */
static void
__grow_type_table(struct typetable *types)
{
	enum typeid *const oldtable = types->table;
	const int oldsize = types->tablesize;
	enum typeid *newtable;
	int n, newsize = oldsize * 2;

	if (newsize < types->nextarg + 1)
		newsize = types->nextarg + 1;
	if (oldsize == STATIC_ARG_TBL_SIZE) {
		if ((newtable = malloc(newsize * sizeof(enum typeid))) == NULL)
			abort();			/* XXX handle better */
		bcopy(oldtable, newtable, oldsize * sizeof(enum typeid));
	} else {
		newtable = reallocf(oldtable, newsize * sizeof(enum typeid));
		if (newtable == NULL)
			abort();			/* XXX handle better */
	}
	for (n = oldsize; n < newsize; n++)
		newtable[n] = T_UNUSED;

	types->table = newtable;
	types->tablesize = newsize;
}
