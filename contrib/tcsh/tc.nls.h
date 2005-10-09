/* $Header: /src/pub/tcsh/tc.nls.h,v 3.9 2005/03/03 15:52:20 kim Exp $ */
/*
 * tc.nls.h: NLS support
 *
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
#ifndef _h_tc_nls
#define _h_tc_nls

#include "sh.h"

#define NLSZEROT	((size_t)-1)

#ifdef WIDE_STRINGS

# define NLSSize(s, l) 1
# define NLSFrom(s, l, cp) (USE (l), (*(cp) = *(s) & CHAR), 1)
# define NLSFinished(s, l, c) (l != 0 && c != CHAR_ERR ? 2 : 1)
# define NLSChars(s) Strlen(s)
# define NLSQuote(s)
# define TRIM_AND_EXTEND(s, c) (c &= TRIM)

extern int NLSWidth __P((NLSChar));
extern int NLSStringWidth __P((Char *));

#define NLS_ILLEGAL INVALID_BYTE

#else
# ifdef SHORT_STRINGS

extern int NLSFrom __P((const Char *, size_t, NLSChar *));
extern int NLSFinished __P((Char *, size_t, eChar));
extern int NLSChars __P((Char *));
extern int NLSStringWidth __P((Char *));
extern int NLSWidth __P((NLSChar));
extern int NLSTo __P((Char *, NLSChar));
extern void NLSQuote __P((Char *));

#define NLSSize(s, l) NLSFrom(s, l, (NLSChar *)0)
#define TRIM_AND_EXTEND(s, c) (s += NLSFrom(s - 1, NLSZEROT, &c) - 1)
#define NLS_ILLEGAL 0x40000000


# else
#  define NLSSize(s, l) 1
#  define NLSFrom(s, l, cp) (USE (l), (*(cp) = *(s) & CHAR), 1)
#  define NLSFinished(s, l, c) (l != 0 && c != CHAR_ERR ? 2 : 1)
#  define NLSChars(s) Strlen(s)
#  define NLSStringWidth(s) Strlen(s)
#  define NLSWidth(c) 1
#  define NLSQuote(s)

#  define TRIM_AND_EXTEND(s, c) (c &= TRIM)
#  define NLS_ILLEGAL 0x40000000
# endif
#endif

extern int NLSExtend __P((Char *, int, int));
extern Char *NLSChangeCase __P((Char *, int));
extern int NLSClassify __P((NLSChar, int));

#define NLSCLASS_CTRL		(-1)
#define NLSCLASS_TAB		(-2)
#define NLSCLASS_NL		(-3)
#define NLSCLASS_ILLEGAL	(-4)
#define NLSCLASS_ILLEGAL2	(-5)
#define NLSCLASS_ILLEGAL3	(-6)
#define NLSCLASS_ILLEGAL4	(-7)

#define NLSCLASS_ILLEGAL_SIZE(x) (-(x) - (-(NLSCLASS_ILLEGAL) - 1))

#endif
