/* ===-- int_lib.h - configuration header for compiler-rt  -----------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 *
 * This file is a configuration header for compiler-rt.
 * This file is not part of the interface of this library.
 *
 * ===----------------------------------------------------------------------===
 */

#ifndef INT_LIB_H
#define INT_LIB_H

/* Assumption: Signed integral is 2's complement. */
/* Assumption: Right shift of signed negative is arithmetic shift. */
/* Assumption: Endianness is little or big (not mixed). */

/* ABI macro definitions */

#if __ARM_EABI__
# define ARM_EABI_FNALIAS(aeabi_name, name)         \
  void __aeabi_##aeabi_name() __attribute__((alias("__" #name)));

# if !defined(__clang__) && defined(__GNUC__) && \
     (__GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ < 5)
/* The pcs attribute was introduced in GCC 4.5.0 */
#  define COMPILER_RT_ABI
# else
#  define COMPILER_RT_ABI __attribute__((pcs("aapcs")))
# endif

#else
# define ARM_EABI_FNALIAS(aeabi_name, name)
# if defined(__arm__) && defined(_WIN32)
#   define COMPILER_RT_ABI __attribute__((pcs("aapcs")))
# else
#   define COMPILER_RT_ABI
# endif
#endif

#if defined(__NetBSD__) && (defined(_KERNEL) || defined(_STANDALONE))
/*
 * Kernel and boot environment can't use normal headers,
 * so use the equivalent system headers.
 */
#  include <machine/limits.h>
#  include <sys/stdint.h>
#  include <sys/types.h>
#else
/* Include the standard compiler builtin headers we use functionality from. */
#  include <limits.h>
#  include <stdint.h>
#  include <stdbool.h>
#  include <float.h>
#endif

/* Include the commonly used internal type definitions. */
#include "int_types.h"

/* Include internal utility function declarations. */
#include "int_util.h"

/*
 * Workaround for LLVM bug 11663.  Prevent endless recursion in
 * __c?zdi2(), where calls to __builtin_c?z() are expanded to
 * __c?zdi2() instead of __c?zsi2().
 *
 * Instead of placing this workaround in c?zdi2.c, put it in this
 * global header to prevent other C files from making the detour
 * through __c?zdi2() as well.
 *
 * This problem has only been observed on FreeBSD for sparc64 and
 * mips64 with GCC 4.2.1.
 */
#if defined(__FreeBSD__) && (defined(__sparc64__) || \
    defined(__mips_n64) || defined(__mips_o64))
si_int __clzsi2(si_int);
si_int __ctzsi2(si_int);
#define	__builtin_clz __clzsi2
#define	__builtin_ctz __ctzsi2
#endif /* FreeBSD && (sparc64 || mips_n64 || mips_o64) */

COMPILER_RT_ABI si_int __paritysi2(si_int a);
COMPILER_RT_ABI si_int __paritydi2(di_int a);

COMPILER_RT_ABI di_int __divdi3(di_int a, di_int b);
COMPILER_RT_ABI si_int __divsi3(si_int a, si_int b);
COMPILER_RT_ABI su_int __udivsi3(su_int n, su_int d);

COMPILER_RT_ABI su_int __udivmodsi4(su_int a, su_int b, su_int* rem);
COMPILER_RT_ABI du_int __udivmoddi4(du_int a, du_int b, du_int* rem);
#ifdef CRT_HAS_128BIT
COMPILER_RT_ABI si_int __clzti2(ti_int a);
COMPILER_RT_ABI tu_int __udivmodti4(tu_int a, tu_int b, tu_int* rem);
#endif

#endif /* INT_LIB_H */
