/*
 * Helpful macros for TAP header files.
 *
 * This is not, strictly speaking, related to TAP, but any TAP add-on is
 * probably going to need these macros, so define them in one place so that
 * everyone can pull them in.
 *
 * This file is part of C TAP Harness.  The current version plus supporting
 * documentation is at <https://www.eyrie.org/~eagle/software/c-tap-harness/>.
 *
 * Copyright 2008, 2012-2013, 2015 Russ Allbery <eagle@eyrie.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TAP_MACROS_H
#define TAP_MACROS_H 1

/*
 * __attribute__ is available in gcc 2.5 and later, but only with gcc 2.7
 * could you use the __format__ form of the attributes, which is what we use
 * (to avoid confusion with other macros), and only with gcc 2.96 can you use
 * the attribute __malloc__.  2.96 is very old, so don't bother trying to get
 * the other attributes to work with GCC versions between 2.7 and 2.96.
 */
#ifndef __attribute__
#    if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#        define __attribute__(spec) /* empty */
#    endif
#endif

/*
 * We use __alloc_size__, but it was only available in fairly recent versions
 * of GCC.  Suppress warnings about the unknown attribute if GCC is too old.
 * We know that we're GCC at this point, so we can use the GCC variadic macro
 * extension, which will still work with versions of GCC too old to have C99
 * variadic macro support.
 */
#if !defined(__attribute__) && !defined(__alloc_size__)
#    if defined(__GNUC__) && !defined(__clang__)
#        if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3)
#            define __alloc_size__(spec, args...) /* empty */
#        endif
#    endif
#endif

/* Suppress __warn_unused_result__ if gcc is too old. */
#if !defined(__attribute__) && !defined(__warn_unused_result__)
#    if __GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
#        define __warn_unused_result__ /* empty */
#    endif
#endif

/*
 * LLVM and Clang pretend to be GCC but don't support all of the __attribute__
 * settings that GCC does.  For them, suppress warnings about unknown
 * attributes on declarations.  This unfortunately will affect the entire
 * compilation context, but there's no push and pop available.
 */
#if !defined(__attribute__) && (defined(__llvm__) || defined(__clang__))
#    pragma GCC diagnostic ignored "-Wattributes"
#endif

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))

/*
 * BEGIN_DECLS is used at the beginning of declarations so that C++
 * compilers don't mangle their names.  END_DECLS is used at the end.
 */
#undef BEGIN_DECLS
#undef END_DECLS
#ifdef __cplusplus
#    define BEGIN_DECLS extern "C" {
#    define END_DECLS   }
#else
#    define BEGIN_DECLS /* empty */
#    define END_DECLS   /* empty */
#endif

#endif /* TAP_MACROS_H */
