/* -*- Mode: c; tab-width: 8; indent-tabs-mode: 1; c-basic-offset: 8; -*- */
/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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

#ifndef lib_funcattrs_h
#define lib_funcattrs_h

#include "compiler-tests.h"

/*
 * Attributes to apply to functions and their arguments, using various
 * compiler-specific extensions.
 */

/*
 * NORETURN, before a function declaration, means "this function
 * never returns".  (It must go before the function declaration, e.g.
 * "extern NORETURN func(...)" rather than after the function
 * declaration, as the MSVC version has to go before the declaration.)
 */
#if __has_attribute(noreturn) \
    || ND_IS_AT_LEAST_GNUC_VERSION(2,5) \
    || ND_IS_AT_LEAST_SUNC_VERSION(5,9) \
    || ND_IS_AT_LEAST_XL_C_VERSION(10,1) \
    || ND_IS_AT_LEAST_HP_C_VERSION(6,10) \
    || __TINYC__
  /*
   * Compiler with support for __attribute((noreturn)), or GCC 2.5 and
   * later, or some compiler asserting compatibility with GCC 2.5 and
   * later, or Solaris Studio 12 (Sun C 5.9) and later, or IBM XL C 10.1
   * and later (do any earlier versions of XL C support this?), or HP aCC
   * A.06.10 and later, or current TinyCC.
   */
  #define NORETURN __attribute((noreturn))

  /*
   * However, GCC didn't support that for function *pointers* until GCC
   * 4.1.0; see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=3481.
   *
   * Sun C/Oracle Studio C doesn't seem to support it, either.
   */
  #if (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) < 401)) \
      || (defined(__SUNPRO_C))
    #define NORETURN_FUNCPTR
  #else
    #define NORETURN_FUNCPTR __attribute((noreturn))
  #endif
#elif defined(_MSC_VER)
  /*
   * MSVC.
   * It doesn't allow __declspec(noreturn) to be applied to function
   * pointers.
   */
  #define NORETURN __declspec(noreturn)
  #define NORETURN_FUNCPTR
#else
  #define NORETURN
  #define NORETURN_FUNCPTR
#endif

/*
 * WARN_UNUSED_RESULT, before a function declaration, means "the caller
 * should use the result of this function" (even if it's just a success/
 * failure indication).
 */
#if __has_attribute(warn_unused_result) \
    || ND_IS_AT_LEAST_GNUC_VERSION(3,4) \
    || ND_IS_AT_LEAST_HP_C_VERSION(6,25)
  #define WARN_UNUSED_RESULT __attribute((warn_unused_result))
#else
  #define WARN_UNUSED_RESULT
#endif

/*
 * PRINTFLIKE(x,y), after a function declaration, means "this function
 * does printf-style formatting, with the xth argument being the format
 * string and the yth argument being the first argument for the format
 * string".
 */
#if __has_attribute(__format__) \
    || ND_IS_AT_LEAST_GNUC_VERSION(2,3) \
    || ND_IS_AT_LEAST_XL_C_VERSION(10,1) \
    || ND_IS_AT_LEAST_HP_C_VERSION(6,10)
  /*
   * Compiler with support for it, or GCC 2.3 and later, or some compiler
   * asserting compatibility with GCC 2.3 and later, or IBM XL C 10.1
   * and later (do any earlier versions of XL C support this?),
   * or HP aCC A.06.10 and later.
   */
  #define PRINTFLIKE(x,y) __attribute__((__format__(__printf__,x,y)))

  /*
   * However, GCC didn't support that for function *pointers* until GCC
   * 4.1.0; see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=3481.
   * XL C 16.1 (and possibly some earlier versions, but not 12.1 or 13.1) has
   * a similar bug, the bugfix for which was made in:
   * * version 16.1.1.8 for Linux (25 June 2020), which fixes
   *   https://www.ibm.com/support/pages/apar/LI81402
   * * version 16.1.0.5 for AIX (5 May 2020), which fixes
   *   https://www.ibm.com/support/pages/apar/IJ24678
   *
   * When testing versions, keep in mind that XL C 16.1 pretends to be both
   * GCC 4.2 and Clang 4.0 at once.
   */
  #if (ND_IS_AT_LEAST_GNUC_VERSION(4,1) \
       && !ND_IS_AT_LEAST_XL_C_VERSION(10,1)) \
      || (ND_IS_AT_LEAST_XL_C_VERSION(16,1) \
          && (ND_IS_AT_LEAST_XL_C_MODFIX(1, 8) && defined(__linux__)) \
              || (ND_IS_AT_LEAST_XL_C_MODFIX(0, 5) && defined(_AIX)))
    #define PRINTFLIKE_FUNCPTR(x,y) __attribute__((__format__(__printf__,x,y)))
  #endif
#endif

#if !defined(PRINTFLIKE)
#define PRINTFLIKE(x,y)
#endif
#if !defined(PRINTFLIKE_FUNCPTR)
#define PRINTFLIKE_FUNCPTR(x,y)
#endif

/*
 * For flagging arguments as format strings in MSVC.
 */
#ifdef _MSC_VER
 #include <sal.h>
 #define FORMAT_STRING(p) _Printf_format_string_ p
#else
 #define FORMAT_STRING(p) p
#endif

#endif /* lib_funcattrs_h */
