/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

#ifndef LESS_LANG_H_
#define LESS_LANG_H_ 1

/*
 * C language details.
 */
#if HAVE_CONST
#define constant        const
#else
#define constant
#endif

/*
 * mutable is the opposite of constant.
 * It documents that a pointer parameter will be written through by the
 * called function, more directly than by the mere absence of "constant".
 */
#define mutable

#define public          /* PUBLIC FUNCTION */

#undef  ptr_diff
#define ptr_diff(p1,p2)  ((size_t) ((p1)-(p2)))
#undef  countof
#define countof(a)       ((int)(sizeof(a)/sizeof(*a)))

#define size_t_null      ((size_t)-1)

#ifndef NULL
#define NULL    0
#endif

typedef enum lbool { LFALSE, LTRUE } lbool;

#undef  TRUE
#define TRUE  LTRUE
#undef  FALSE
#define FALSE LFALSE

#ifdef _MSC_VER
#if _WIN64
typedef __int64 ssize_t;
#else
typedef __int32 ssize_t;
#endif
#endif

#endif //  LESS_LANG_H_
