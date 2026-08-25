//===-- comparekf2.c - IEEE-128 comparisons for powerpc64le -----*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// PowerPC names the IEEE binary128 comparison soft-float routines with a "kf"
// infix instead of the generic "tf".  The semantics are identical to the
// generic comparetf2.c routines; only the exported names differ.  We provide
// them here because comparetf2.c cannot be reused via a -D rename: its libgcc
// compatibility aliases stringize the (unrenamed) target symbol name.
//
//   __lekf2(a,b) returns -1 if a < b, 0 if a == b, 1 if a > b, 1 if NaN
//   __gekf2(a,b) returns -1 if a < b, 0 if a == b, 1 if a > b, -1 if NaN
//   __unordkf2(a,b) returns 1 if either operand is NaN, 0 otherwise
//
//===----------------------------------------------------------------------===//

#define QUAD_PRECISION
#include "fp_lib.h"

#if defined(CRT_HAS_TF_MODE)
#include "fp_compare_impl.inc"

COMPILER_RT_ABI CMP_RESULT __eqkf2(fp_t a, fp_t b) { return __leXf2__(a, b); }

COMPILER_RT_ABI CMP_RESULT __nekf2(fp_t a, fp_t b) { return __leXf2__(a, b); }

COMPILER_RT_ABI CMP_RESULT __ltkf2(fp_t a, fp_t b) { return __leXf2__(a, b); }

COMPILER_RT_ABI CMP_RESULT __lekf2(fp_t a, fp_t b) { return __leXf2__(a, b); }

COMPILER_RT_ABI CMP_RESULT __gtkf2(fp_t a, fp_t b) { return __geXf2__(a, b); }

COMPILER_RT_ABI CMP_RESULT __gekf2(fp_t a, fp_t b) { return __geXf2__(a, b); }

COMPILER_RT_ABI CMP_RESULT __unordkf2(fp_t a, fp_t b) {
  return __unordXf2__(a, b);
}

#endif
