/*
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *
 *      from: @(#)cdefs.h       8.1 (Berkeley) 6/2/93
 *      $FreeBSD$
 */

#ifndef _CDEFS_H_
#define _CDEFS_H_

#if defined(__cplusplus)
#define __BEGIN_DECLS   extern "C" {
#define __END_DECLS     };
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

/*
 * The __CONCAT macro is used to concatenate parts of symbol names, e.g.
 * with "#define OLD(foo) __CONCAT(old,foo)", OLD(foo) produces oldfoo.
 * The __CONCAT macro is a bit tricky -- make sure you don't put spaces
 * in between its arguments.  __CONCAT can also concatenate double-quoted
 * strings produced by the __STRING macro, but this only works with ANSI C.
 */
#if defined(__STDC__) || defined(__cplusplus)
#if defined(__P)
#undef __P
#endif  /* defined(__P) */
#define __P(protos)     protos          /* full-blown ANSI C */
#define __CONCAT(x,y)   x ## y
#define __STRING(x)     #x

#else   /* !(__STDC__ || __cplusplus) */
#if defined(__P)
#undef __P
#endif  /* defined(__P) */
#define __P(protos)     ()              /* traditional C preprocessor */
#define __CONCAT(x,y)   x/**/y
#define __STRING(x)     "x"

/* delete ANSI C keywords */
#define const                           
#define inline
#define signed
#define volatile
#endif  /* !(__STDC__ || __cplusplus) */

/*
 * GCC has extensions for declaring functions as const (`pure' - always returns
 * the same value given the same inputs, i.e., has no external state and
 * no side effects) and volatile (nonreturning or `dead').
 * These mainly affect optimization and warnings.  
 *
 * To facilitate portability of a non-standard extension we define __pure
 * and __dead and use these for qualifying functions. Non-gcc compilers
 * which have similar extensions can then define these appropriately.
 *
 * Unfortunately, GCC complains if these are used under strict ANSI mode 
 * (`gcc -ansi -pedantic'), hence we need to define them only if compiling 
 * without this.
 */
#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define __dead __volatile
#define __pure __const
#else
#define __dead
#define __pure
#endif

#endif /* !_CDEFS_H_ */
