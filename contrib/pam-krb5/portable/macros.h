/*
 * Portability macros used in include files.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2015 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2011-2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_MACROS_H
#define PORTABLE_MACROS_H 1

/*
 * __attribute__ is available in gcc 2.5 and later, but only with gcc 2.7
 * could you use the __format__ form of the attributes, which is what we use
 * (to avoid confusion with other macros).
 */
#ifndef __attribute__
#    if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
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
#    if (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3)) \
        && !defined(__clang__)
#        define __alloc_size__(spec, args...) /* empty */
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

#endif /* !PORTABLE_MACROS_H */
