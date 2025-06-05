/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/* This file is adapted from the SPAKE edwards25519 code in BoringSSL. */
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 the fiat-crypto authors (see the AUTHORS file).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
/*
 * Copyright (c) 2015-2016, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This code is adapted from the BoringSSL edwards25519 SPAKE2 implementation
 * from third_party/fiat and crypto/spake25519.c, with the following
 * adaptations:
 *
 * - The M and N points are the ones from draft-irtf-cfrg-spake2-05.  The
 *   BoringSSL M and N points were determined similarly, but were not
 *   restricted to members of the generator subgroup, so they use only one hash
 *   iteration for both points.  The intent in BoringSSL had been to multiply w
 *   by the cofactor so that wM and wN would be in the subgroup, but as that
 *   step was accidentally omitted, a hack had to be introduced after the fact
 *   to add multiples of the prime order to the scalar.  That hack is not
 *   present in this code, and the SPAKE preauth spec does not multiply w by
 *   the cofactor as it is unnecessary if M and N are chosen from the subgroup.
 *
 * - The SPAKE code is modified to fit the groups.h interface and the SPAKE
 *   preauth spec.
 *
 * - The required declarations and code are all here in one file (except for
 *   the generator point table, which is still in a separate header), so all of
 *   the functions are declared static.
 *
 * - BORINGSSL_CURVE25519_64BIT is defined here using autoconf tests.
 *
 * - curve25519_32.h and curve25519_64.h are combined into edwards25519_fiat.h
 *   (conditionalized on BORINGSSL_CURVE25519_64BIT) for predictable dependency
 *   generation.  The fiat_25519_selectznz and fiat_25519_carry_scmul_121666
 *   functions were removed from both branches as they are not used here (the
 *   former because it is not used by the BoringSSL code and the latter because
 *   it is only used by the X25519 code).  The fiat_25519_int128 and
 *   fiat_25519_uint128 typedefs were adjusted to work with older versions of
 *   gcc.
 *
 * - fe_cmov() has the initial "Silence an unused function warning" part
 *   removed, as we removed fiat_25519_selectznz instead.
 *
 * - The field element bounds assertion checks are disabled by default, as they
 *   slow the code down by roughly a factor of two.  The
 *   OPENSSL_COMPILE_ASSERT() in fe_copy_lt() is changed to a regular assert
 *   and is also conditionalized.  Do a build and "make check" with
 *   EDWARDS25519_ASSERTS defined when updating this code.
 *
 * - The copyright comments at the top are formatted the way we do so in other
 *   source files, for ease of extraction.
 *
 * - Declarations in for loops conflict with our compiler configuration in
 *   older versions of gcc, so they are moved outside of the for loop.
 *
 * - The preprocessor symbol OPENSSL_SMALL is changed to CONFIG_SMALL.
 *
 * - OPENSSL_memset and OPENSSL_memmove are changed to memset and memmove, in
 *   each case verifying that they are used with nonzero length arguments.
 *
 * - CRYPTO_memcmp is changed to k5_bcmp.
 *
 * - Functions used only by X25519 or Ed25519 interfaces but not SPAKE are
 *   removed, taking care to check for unused functions in both the 64-bit and
 *   32-bit preprocessor branches.  ge_p3_dbl() is unused here if CONFIG_SMALL
 *   is defined, so it is placed inside #ifndef CONFIG_SMALL.
 */

// Some of this code is taken from the ref10 version of Ed25519 in SUPERCOP
// 20141124 (https://bench.cr.yp.to/supercop.html). That code is released as
// public domain but parts have been replaced with code generated by Fiat
// (https://github.com/mit-plv/fiat-crypto), which is MIT licensed.

#include "groups.h"
#include "iana.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
#endif

#if SIZEOF_SIZE_T >= 8 && defined(HAVE___INT128_T) && defined(HAVE___UINT128_T)
#define BORINGSSL_CURVE25519_64BIT
typedef __int128_t int128_t;
typedef __uint128_t uint128_t;
#endif

/* From BoringSSL third-party/fiat/internal.h */

#if defined(BORINGSSL_CURVE25519_64BIT)
// fe means field element. Here the field is \Z/(2^255-19). An element t,
// entries t[0]...t[4], represents the integer t[0]+2^51 t[1]+2^102 t[2]+2^153
// t[3]+2^204 t[4].
// fe limbs are bounded by 1.125*2^51.
// Multiplication and carrying produce fe from fe_loose.
typedef struct fe { uint64_t v[5]; } fe;

// fe_loose limbs are bounded by 3.375*2^51.
// Addition and subtraction produce fe_loose from (fe, fe).
typedef struct fe_loose { uint64_t v[5]; } fe_loose;
#else
// fe means field element. Here the field is \Z/(2^255-19). An element t,
// entries t[0]...t[9], represents the integer t[0]+2^26 t[1]+2^51 t[2]+2^77
// t[3]+2^102 t[4]+...+2^230 t[9].
// fe limbs are bounded by 1.125*2^26,1.125*2^25,1.125*2^26,1.125*2^25,etc.
// Multiplication and carrying produce fe from fe_loose.
typedef struct fe { uint32_t v[10]; } fe;

// fe_loose limbs are bounded by 3.375*2^26,3.375*2^25,3.375*2^26,3.375*2^25,etc.
// Addition and subtraction produce fe_loose from (fe, fe).
typedef struct fe_loose { uint32_t v[10]; } fe_loose;
#endif

// ge means group element.
//
// Here the group is the set of pairs (x,y) of field elements (see fe.h)
// satisfying -x^2 + y^2 = 1 + d x^2y^2
// where d = -121665/121666.
//
// Representations:
//   ge_p2 (projective): (X:Y:Z) satisfying x=X/Z, y=Y/Z
//   ge_p3 (extended): (X:Y:Z:T) satisfying x=X/Z, y=Y/Z, XY=ZT
//   ge_p1p1 (completed): ((X:Z),(Y:T)) satisfying x=X/Z, y=Y/T
//   ge_precomp (Duif): (y+x,y-x,2dxy)

typedef struct {
  fe X;
  fe Y;
  fe Z;
} ge_p2;

typedef struct {
  fe X;
  fe Y;
  fe Z;
  fe T;
} ge_p3;

typedef struct {
  fe_loose X;
  fe_loose Y;
  fe_loose Z;
  fe_loose T;
} ge_p1p1;

typedef struct {
  fe_loose yplusx;
  fe_loose yminusx;
  fe_loose xy2d;
} ge_precomp;

typedef struct {
  fe_loose YplusX;
  fe_loose YminusX;
  fe_loose Z;
  fe_loose T2d;
} ge_cached;

#include "edwards25519_tables.h"
#include "edwards25519_fiat.h"

/* From BoringSSL third-party/fiat/curve25519.c */

static uint64_t load_3(const uint8_t *in) {
  uint64_t result;
  result = (uint64_t)in[0];
  result |= ((uint64_t)in[1]) << 8;
  result |= ((uint64_t)in[2]) << 16;
  return result;
}

static uint64_t load_4(const uint8_t *in) {
  uint64_t result;
  result = (uint64_t)in[0];
  result |= ((uint64_t)in[1]) << 8;
  result |= ((uint64_t)in[2]) << 16;
  result |= ((uint64_t)in[3]) << 24;
  return result;
}


// Field operations.

#if defined(BORINGSSL_CURVE25519_64BIT)

typedef uint64_t fe_limb_t;
#define FE_NUM_LIMBS 5

// assert_fe asserts that |f| satisfies bounds:
//
//  [[0x0 ~> 0x8cccccccccccc],
//   [0x0 ~> 0x8cccccccccccc],
//   [0x0 ~> 0x8cccccccccccc],
//   [0x0 ~> 0x8cccccccccccc],
//   [0x0 ~> 0x8cccccccccccc]]
//
// See comments in edwards25519_fiat.h for which functions use these bounds for
// inputs or outputs.
#define assert_fe(f)                                                    \
  do {                                                                  \
    for (unsigned _assert_fe_i = 0; _assert_fe_i < 5; _assert_fe_i++) { \
      assert(f[_assert_fe_i] <= UINT64_C(0x8cccccccccccc));             \
    }                                                                   \
  } while (0)

// assert_fe_loose asserts that |f| satisfies bounds:
//
//  [[0x0 ~> 0x1a666666666664],
//   [0x0 ~> 0x1a666666666664],
//   [0x0 ~> 0x1a666666666664],
//   [0x0 ~> 0x1a666666666664],
//   [0x0 ~> 0x1a666666666664]]
//
// See comments in edwards25519_fiat.h for which functions use these bounds for
// inputs or outputs.
#define assert_fe_loose(f)                                              \
  do {                                                                  \
    for (unsigned _assert_fe_i = 0; _assert_fe_i < 5; _assert_fe_i++) { \
      assert(f[_assert_fe_i] <= UINT64_C(0x1a666666666664));            \
    }                                                                   \
  } while (0)

#else

typedef uint32_t fe_limb_t;
#define FE_NUM_LIMBS 10

// assert_fe asserts that |f| satisfies bounds:
//
//  [[0x0 ~> 0x4666666], [0x0 ~> 0x2333333],
//   [0x0 ~> 0x4666666], [0x0 ~> 0x2333333],
//   [0x0 ~> 0x4666666], [0x0 ~> 0x2333333],
//   [0x0 ~> 0x4666666], [0x0 ~> 0x2333333],
//   [0x0 ~> 0x4666666], [0x0 ~> 0x2333333]]
//
// See comments in edwards25519_fiat.h for which functions use these bounds for
// inputs or outputs.
#define assert_fe(f)                                                     \
  do {                                                                   \
    for (unsigned _assert_fe_i = 0; _assert_fe_i < 10; _assert_fe_i++) { \
      assert(f[_assert_fe_i] <=                                          \
             ((_assert_fe_i & 1) ? 0x2333333u : 0x4666666u));            \
    }                                                                    \
  } while (0)

// assert_fe_loose asserts that |f| satisfies bounds:
//
//  [[0x0 ~> 0xd333332], [0x0 ~> 0x6999999],
//   [0x0 ~> 0xd333332], [0x0 ~> 0x6999999],
//   [0x0 ~> 0xd333332], [0x0 ~> 0x6999999],
//   [0x0 ~> 0xd333332], [0x0 ~> 0x6999999],
//   [0x0 ~> 0xd333332], [0x0 ~> 0x6999999]]
//
// See comments in edwards25519_fiat.h for which functions use these bounds for
// inputs or outputs.
#define assert_fe_loose(f)                                               \
  do {                                                                   \
    for (unsigned _assert_fe_i = 0; _assert_fe_i < 10; _assert_fe_i++) { \
      assert(f[_assert_fe_i] <=                                          \
             ((_assert_fe_i & 1) ? 0x6999999u : 0xd333332u));            \
    }                                                                    \
  } while (0)

#endif  // BORINGSSL_CURVE25519_64BIT

#ifndef EDWARDS25519_ASSERTS
#undef assert_fe
#undef assert_fe_loose
#define assert_fe(f)
#define assert_fe_loose(f)
#endif

static void fe_frombytes_strict(fe *h, const uint8_t s[32]) {
  // |fiat_25519_from_bytes| requires the top-most bit be clear.
  assert((s[31] & 0x80) == 0);
  fiat_25519_from_bytes(h->v, s);
  assert_fe(h->v);
}

static void fe_frombytes(fe *h, const uint8_t s[32]) {
  uint8_t s_copy[32];
  memcpy(s_copy, s, 32);
  s_copy[31] &= 0x7f;
  fe_frombytes_strict(h, s_copy);
}

static void fe_tobytes(uint8_t s[32], const fe *f) {
  assert_fe(f->v);
  fiat_25519_to_bytes(s, f->v);
}

// h = 0
static void fe_0(fe *h) {
  memset(h, 0, sizeof(fe));
}

static void fe_loose_0(fe_loose *h) {
  memset(h, 0, sizeof(fe_loose));
}

// h = 1
static void fe_1(fe *h) {
  memset(h, 0, sizeof(fe));
  h->v[0] = 1;
}

static void fe_loose_1(fe_loose *h) {
  memset(h, 0, sizeof(fe_loose));
  h->v[0] = 1;
}

// h = f + g
// Can overlap h with f or g.
static void fe_add(fe_loose *h, const fe *f, const fe *g) {
  assert_fe(f->v);
  assert_fe(g->v);
  fiat_25519_add(h->v, f->v, g->v);
  assert_fe_loose(h->v);
}

// h = f - g
// Can overlap h with f or g.
static void fe_sub(fe_loose *h, const fe *f, const fe *g) {
  assert_fe(f->v);
  assert_fe(g->v);
  fiat_25519_sub(h->v, f->v, g->v);
  assert_fe_loose(h->v);
}

static void fe_carry(fe *h, const fe_loose* f) {
  assert_fe_loose(f->v);
  fiat_25519_carry(h->v, f->v);
  assert_fe(h->v);
}

static void fe_mul_impl(fe_limb_t out[FE_NUM_LIMBS],
                        const fe_limb_t in1[FE_NUM_LIMBS],
                        const fe_limb_t in2[FE_NUM_LIMBS]) {
  assert_fe_loose(in1);
  assert_fe_loose(in2);
  fiat_25519_carry_mul(out, in1, in2);
  assert_fe(out);
}

static void fe_mul_ltt(fe_loose *h, const fe *f, const fe *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_llt(fe_loose *h, const fe_loose *f, const fe *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_ttt(fe *h, const fe *f, const fe *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_tlt(fe *h, const fe_loose *f, const fe *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_ttl(fe *h, const fe *f, const fe_loose *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_mul_tll(fe *h, const fe_loose *f, const fe_loose *g) {
  fe_mul_impl(h->v, f->v, g->v);
}

static void fe_sq_tl(fe *h, const fe_loose *f) {
  assert_fe_loose(f->v);
  fiat_25519_carry_square(h->v, f->v);
  assert_fe(h->v);
}

static void fe_sq_tt(fe *h, const fe *f) {
  assert_fe_loose(f->v);
  fiat_25519_carry_square(h->v, f->v);
  assert_fe(h->v);
}

// h = -f
static void fe_neg(fe_loose *h, const fe *f) {
  assert_fe(f->v);
  fiat_25519_opp(h->v, f->v);
  assert_fe_loose(h->v);
}

// Replace (f,g) with (g,g) if b == 1;
// replace (f,g) with (f,g) if b == 0.
//
// Preconditions: b in {0,1}.
static void fe_cmov(fe_loose *f, const fe_loose *g, fe_limb_t b) {
  b = 0-b;
  unsigned i;
  for (i = 0; i < FE_NUM_LIMBS; i++) {
    fe_limb_t x = f->v[i] ^ g->v[i];
    x &= b;
    f->v[i] ^= x;
  }
}

// h = f
static void fe_copy(fe *h, const fe *f) {
  memmove(h, f, sizeof(fe));
}

static void fe_copy_lt(fe_loose *h, const fe *f) {
#ifdef EDWARDS25519_ASSERTS
  assert(sizeof(fe_loose) == sizeof(fe));
#endif
  memmove(h, f, sizeof(fe));
}
#if !defined(CONFIG_SMALL)
static void fe_copy_ll(fe_loose *h, const fe_loose *f) {
  memmove(h, f, sizeof(fe_loose));
}
#endif // !defined(CONFIG_SMALL)

static void fe_loose_invert(fe *out, const fe_loose *z) {
  fe t0;
  fe t1;
  fe t2;
  fe t3;
  int i;

  fe_sq_tl(&t0, z);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 2; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_tlt(&t1, z, &t1);
  fe_mul_ttt(&t0, &t0, &t1);
  fe_sq_tt(&t2, &t0);
  fe_mul_ttt(&t1, &t1, &t2);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 5; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 10; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t2, &t2, &t1);
  fe_sq_tt(&t3, &t2);
  for (i = 1; i < 20; ++i) {
    fe_sq_tt(&t3, &t3);
  }
  fe_mul_ttt(&t2, &t3, &t2);
  fe_sq_tt(&t2, &t2);
  for (i = 1; i < 10; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 50; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t2, &t2, &t1);
  fe_sq_tt(&t3, &t2);
  for (i = 1; i < 100; ++i) {
    fe_sq_tt(&t3, &t3);
  }
  fe_mul_ttt(&t2, &t3, &t2);
  fe_sq_tt(&t2, &t2);
  for (i = 1; i < 50; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t1, &t1);
  for (i = 1; i < 5; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(out, &t1, &t0);
}

static void fe_invert(fe *out, const fe *z) {
  fe_loose l;
  fe_copy_lt(&l, z);
  fe_loose_invert(out, &l);
}

// return 0 if f == 0
// return 1 if f != 0
static int fe_isnonzero(const fe_loose *f) {
  fe tight;
  fe_carry(&tight, f);
  uint8_t s[32];
  fe_tobytes(s, &tight);

  static const uint8_t zero[32] = {0};
  return k5_bcmp(s, zero, sizeof(zero)) != 0;
}

// return 1 if f is in {1,3,5,...,q-2}
// return 0 if f is in {0,2,4,...,q-1}
static int fe_isnegative(const fe *f) {
  uint8_t s[32];
  fe_tobytes(s, f);
  return s[0] & 1;
}

static void fe_sq2_tt(fe *h, const fe *f) {
  // h = f^2
  fe_sq_tt(h, f);

  // h = h + h
  fe_loose tmp;
  fe_add(&tmp, h, h);
  fe_carry(h, &tmp);
}

static void fe_pow22523(fe *out, const fe *z) {
  fe t0;
  fe t1;
  fe t2;
  int i;

  fe_sq_tt(&t0, z);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 2; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t1, z, &t1);
  fe_mul_ttt(&t0, &t0, &t1);
  fe_sq_tt(&t0, &t0);
  fe_mul_ttt(&t0, &t1, &t0);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 5; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t0, &t1, &t0);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 10; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t1, &t1, &t0);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 20; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t1, &t1);
  for (i = 1; i < 10; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t0, &t1, &t0);
  fe_sq_tt(&t1, &t0);
  for (i = 1; i < 50; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t1, &t1, &t0);
  fe_sq_tt(&t2, &t1);
  for (i = 1; i < 100; ++i) {
    fe_sq_tt(&t2, &t2);
  }
  fe_mul_ttt(&t1, &t2, &t1);
  fe_sq_tt(&t1, &t1);
  for (i = 1; i < 50; ++i) {
    fe_sq_tt(&t1, &t1);
  }
  fe_mul_ttt(&t0, &t1, &t0);
  fe_sq_tt(&t0, &t0);
  for (i = 1; i < 2; ++i) {
    fe_sq_tt(&t0, &t0);
  }
  fe_mul_ttt(out, &t0, z);
}


// Group operations.

static void x25519_ge_tobytes(uint8_t s[32], const ge_p2 *h) {
  fe recip;
  fe x;
  fe y;

  fe_invert(&recip, &h->Z);
  fe_mul_ttt(&x, &h->X, &recip);
  fe_mul_ttt(&y, &h->Y, &recip);
  fe_tobytes(s, &y);
  s[31] ^= fe_isnegative(&x) << 7;
}

static int x25519_ge_frombytes_vartime(ge_p3 *h, const uint8_t s[32]) {
  fe u;
  fe_loose v;
  fe v3;
  fe vxx;
  fe_loose check;

  fe_frombytes(&h->Y, s);
  fe_1(&h->Z);
  fe_sq_tt(&v3, &h->Y);
  fe_mul_ttt(&vxx, &v3, &d);
  fe_sub(&v, &v3, &h->Z);  // u = y^2-1
  fe_carry(&u, &v);
  fe_add(&v, &vxx, &h->Z);  // v = dy^2+1

  fe_sq_tl(&v3, &v);
  fe_mul_ttl(&v3, &v3, &v);  // v3 = v^3
  fe_sq_tt(&h->X, &v3);
  fe_mul_ttl(&h->X, &h->X, &v);
  fe_mul_ttt(&h->X, &h->X, &u);  // x = uv^7

  fe_pow22523(&h->X, &h->X);  // x = (uv^7)^((q-5)/8)
  fe_mul_ttt(&h->X, &h->X, &v3);
  fe_mul_ttt(&h->X, &h->X, &u);  // x = uv^3(uv^7)^((q-5)/8)

  fe_sq_tt(&vxx, &h->X);
  fe_mul_ttl(&vxx, &vxx, &v);
  fe_sub(&check, &vxx, &u);
  if (fe_isnonzero(&check)) {
    fe_add(&check, &vxx, &u);
    if (fe_isnonzero(&check)) {
      return 0;
    }
    fe_mul_ttt(&h->X, &h->X, &sqrtm1);
  }

  if (fe_isnegative(&h->X) != (s[31] >> 7)) {
    fe_loose t;
    fe_neg(&t, &h->X);
    fe_carry(&h->X, &t);
  }

  fe_mul_ttt(&h->T, &h->X, &h->Y);
  return 1;
}

static void ge_p2_0(ge_p2 *h) {
  fe_0(&h->X);
  fe_1(&h->Y);
  fe_1(&h->Z);
}

static void ge_p3_0(ge_p3 *h) {
  fe_0(&h->X);
  fe_1(&h->Y);
  fe_1(&h->Z);
  fe_0(&h->T);
}

static void ge_cached_0(ge_cached *h) {
  fe_loose_1(&h->YplusX);
  fe_loose_1(&h->YminusX);
  fe_loose_1(&h->Z);
  fe_loose_0(&h->T2d);
}

static void ge_precomp_0(ge_precomp *h) {
  fe_loose_1(&h->yplusx);
  fe_loose_1(&h->yminusx);
  fe_loose_0(&h->xy2d);
}

// r = p
static void ge_p3_to_p2(ge_p2 *r, const ge_p3 *p) {
  fe_copy(&r->X, &p->X);
  fe_copy(&r->Y, &p->Y);
  fe_copy(&r->Z, &p->Z);
}

// r = p
static void x25519_ge_p3_to_cached(ge_cached *r, const ge_p3 *p) {
  fe_add(&r->YplusX, &p->Y, &p->X);
  fe_sub(&r->YminusX, &p->Y, &p->X);
  fe_copy_lt(&r->Z, &p->Z);
  fe_mul_ltt(&r->T2d, &p->T, &d2);
}

// r = p
static void x25519_ge_p1p1_to_p2(ge_p2 *r, const ge_p1p1 *p) {
  fe_mul_tll(&r->X, &p->X, &p->T);
  fe_mul_tll(&r->Y, &p->Y, &p->Z);
  fe_mul_tll(&r->Z, &p->Z, &p->T);
}

// r = p
static void x25519_ge_p1p1_to_p3(ge_p3 *r, const ge_p1p1 *p) {
  fe_mul_tll(&r->X, &p->X, &p->T);
  fe_mul_tll(&r->Y, &p->Y, &p->Z);
  fe_mul_tll(&r->Z, &p->Z, &p->T);
  fe_mul_tll(&r->T, &p->X, &p->Y);
}

// r = p
static void ge_p1p1_to_cached(ge_cached *r, const ge_p1p1 *p) {
  ge_p3 t;
  x25519_ge_p1p1_to_p3(&t, p);
  x25519_ge_p3_to_cached(r, &t);
}

// r = 2 * p
static void ge_p2_dbl(ge_p1p1 *r, const ge_p2 *p) {
  fe trX, trZ, trT;
  fe t0;

  fe_sq_tt(&trX, &p->X);
  fe_sq_tt(&trZ, &p->Y);
  fe_sq2_tt(&trT, &p->Z);
  fe_add(&r->Y, &p->X, &p->Y);
  fe_sq_tl(&t0, &r->Y);

  fe_add(&r->Y, &trZ, &trX);
  fe_sub(&r->Z, &trZ, &trX);
  fe_carry(&trZ, &r->Y);
  fe_sub(&r->X, &t0, &trZ);
  fe_carry(&trZ, &r->Z);
  fe_sub(&r->T, &trT, &trZ);
}

#ifndef CONFIG_SMALL
// r = 2 * p
static void ge_p3_dbl(ge_p1p1 *r, const ge_p3 *p) {
  ge_p2 q;
  ge_p3_to_p2(&q, p);
  ge_p2_dbl(r, &q);
}
#endif

// r = p + q
static void ge_madd(ge_p1p1 *r, const ge_p3 *p, const ge_precomp *q) {
  fe trY, trZ, trT;

  fe_add(&r->X, &p->Y, &p->X);
  fe_sub(&r->Y, &p->Y, &p->X);
  fe_mul_tll(&trZ, &r->X, &q->yplusx);
  fe_mul_tll(&trY, &r->Y, &q->yminusx);
  fe_mul_tlt(&trT, &q->xy2d, &p->T);
  fe_add(&r->T, &p->Z, &p->Z);
  fe_sub(&r->X, &trZ, &trY);
  fe_add(&r->Y, &trZ, &trY);
  fe_carry(&trZ, &r->T);
  fe_add(&r->Z, &trZ, &trT);
  fe_sub(&r->T, &trZ, &trT);
}

// r = p + q
static void x25519_ge_add(ge_p1p1 *r, const ge_p3 *p, const ge_cached *q) {
  fe trX, trY, trZ, trT;

  fe_add(&r->X, &p->Y, &p->X);
  fe_sub(&r->Y, &p->Y, &p->X);
  fe_mul_tll(&trZ, &r->X, &q->YplusX);
  fe_mul_tll(&trY, &r->Y, &q->YminusX);
  fe_mul_tlt(&trT, &q->T2d, &p->T);
  fe_mul_ttl(&trX, &p->Z, &q->Z);
  fe_add(&r->T, &trX, &trX);
  fe_sub(&r->X, &trZ, &trY);
  fe_add(&r->Y, &trZ, &trY);
  fe_carry(&trZ, &r->T);
  fe_add(&r->Z, &trZ, &trT);
  fe_sub(&r->T, &trZ, &trT);
}

// r = p - q
static void x25519_ge_sub(ge_p1p1 *r, const ge_p3 *p, const ge_cached *q) {
  fe trX, trY, trZ, trT;

  fe_add(&r->X, &p->Y, &p->X);
  fe_sub(&r->Y, &p->Y, &p->X);
  fe_mul_tll(&trZ, &r->X, &q->YminusX);
  fe_mul_tll(&trY, &r->Y, &q->YplusX);
  fe_mul_tlt(&trT, &q->T2d, &p->T);
  fe_mul_ttl(&trX, &p->Z, &q->Z);
  fe_add(&r->T, &trX, &trX);
  fe_sub(&r->X, &trZ, &trY);
  fe_add(&r->Y, &trZ, &trY);
  fe_carry(&trZ, &r->T);
  fe_sub(&r->Z, &trZ, &trT);
  fe_add(&r->T, &trZ, &trT);
}

static uint8_t equal(signed char b, signed char c) {
  uint8_t ub = b;
  uint8_t uc = c;
  uint8_t x = ub ^ uc;  // 0: yes; 1..255: no
  uint32_t y = x;       // 0: yes; 1..255: no
  y -= 1;               // 4294967295: yes; 0..254: no
  y >>= 31;             // 1: yes; 0: no
  return y;
}

static void cmov(ge_precomp *t, const ge_precomp *u, uint8_t b) {
  fe_cmov(&t->yplusx, &u->yplusx, b);
  fe_cmov(&t->yminusx, &u->yminusx, b);
  fe_cmov(&t->xy2d, &u->xy2d, b);
}

static void x25519_ge_scalarmult_small_precomp(
    ge_p3 *h, const uint8_t a[32], const uint8_t precomp_table[15 * 2 * 32]) {
  // precomp_table is first expanded into matching |ge_precomp|
  // elements.
  ge_precomp multiples[15];

  unsigned i;
  for (i = 0; i < 15; i++) {
    // The precomputed table is assumed to already clear the top bit, so
    // |fe_frombytes_strict| may be used directly.
    const uint8_t *bytes = &precomp_table[i*(2 * 32)];
    fe x, y;
    fe_frombytes_strict(&x, bytes);
    fe_frombytes_strict(&y, bytes + 32);

    ge_precomp *out = &multiples[i];
    fe_add(&out->yplusx, &y, &x);
    fe_sub(&out->yminusx, &y, &x);
    fe_mul_ltt(&out->xy2d, &x, &y);
    fe_mul_llt(&out->xy2d, &out->xy2d, &d2);
  }

  // See the comment above |k25519SmallPrecomp| about the structure of the
  // precomputed elements. This loop does 64 additions and 64 doublings to
  // calculate the result.
  ge_p3_0(h);

  for (i = 63; i < 64; i--) {
    unsigned j;
    signed char index = 0;

    for (j = 0; j < 4; j++) {
      const uint8_t bit = 1 & (a[(8 * j) + (i / 8)] >> (i & 7));
      index |= (bit << j);
    }

    ge_precomp e;
    ge_precomp_0(&e);

    for (j = 1; j < 16; j++) {
      cmov(&e, &multiples[j-1], equal(index, j));
    }

    ge_cached cached;
    ge_p1p1 r;
    x25519_ge_p3_to_cached(&cached, h);
    x25519_ge_add(&r, h, &cached);
    x25519_ge_p1p1_to_p3(h, &r);

    ge_madd(&r, h, &e);
    x25519_ge_p1p1_to_p3(h, &r);
  }
}

#if defined(CONFIG_SMALL)

static void x25519_ge_scalarmult_base(ge_p3 *h, const uint8_t a[32]) {
  x25519_ge_scalarmult_small_precomp(h, a, k25519SmallPrecomp);
}

#else

static uint8_t negative(signed char b) {
  uint32_t x = b;
  x >>= 31;  // 1: yes; 0: no
  return x;
}

static void table_select(ge_precomp *t, int pos, signed char b) {
  ge_precomp minust;
  uint8_t bnegative = negative(b);
  uint8_t babs = b - ((uint8_t)((-bnegative) & b) << 1);

  ge_precomp_0(t);
  cmov(t, &k25519Precomp[pos][0], equal(babs, 1));
  cmov(t, &k25519Precomp[pos][1], equal(babs, 2));
  cmov(t, &k25519Precomp[pos][2], equal(babs, 3));
  cmov(t, &k25519Precomp[pos][3], equal(babs, 4));
  cmov(t, &k25519Precomp[pos][4], equal(babs, 5));
  cmov(t, &k25519Precomp[pos][5], equal(babs, 6));
  cmov(t, &k25519Precomp[pos][6], equal(babs, 7));
  cmov(t, &k25519Precomp[pos][7], equal(babs, 8));
  fe_copy_ll(&minust.yplusx, &t->yminusx);
  fe_copy_ll(&minust.yminusx, &t->yplusx);

  // NOTE: the input table is canonical, but types don't encode it
  fe tmp;
  fe_carry(&tmp, &t->xy2d);
  fe_neg(&minust.xy2d, &tmp);

  cmov(t, &minust, bnegative);
}

// h = a * B
// where a = a[0]+256*a[1]+...+256^31 a[31]
// B is the Ed25519 base point (x,4/5) with x positive.
//
// Preconditions:
//   a[31] <= 127
static void x25519_ge_scalarmult_base(ge_p3 *h, const uint8_t *a) {
  signed char e[64];
  signed char carry;
  ge_p1p1 r;
  ge_p2 s;
  ge_precomp t;
  int i;

  for (i = 0; i < 32; ++i) {
    e[2 * i + 0] = (a[i] >> 0) & 15;
    e[2 * i + 1] = (a[i] >> 4) & 15;
  }
  // each e[i] is between 0 and 15
  // e[63] is between 0 and 7

  carry = 0;
  for (i = 0; i < 63; ++i) {
    e[i] += carry;
    carry = e[i] + 8;
    carry >>= 4;
    e[i] -= carry << 4;
  }
  e[63] += carry;
  // each e[i] is between -8 and 8

  ge_p3_0(h);
  for (i = 1; i < 64; i += 2) {
    table_select(&t, i / 2, e[i]);
    ge_madd(&r, h, &t);
    x25519_ge_p1p1_to_p3(h, &r);
  }

  ge_p3_dbl(&r, h);
  x25519_ge_p1p1_to_p2(&s, &r);
  ge_p2_dbl(&r, &s);
  x25519_ge_p1p1_to_p2(&s, &r);
  ge_p2_dbl(&r, &s);
  x25519_ge_p1p1_to_p2(&s, &r);
  ge_p2_dbl(&r, &s);
  x25519_ge_p1p1_to_p3(h, &r);

  for (i = 0; i < 64; i += 2) {
    table_select(&t, i / 2, e[i]);
    ge_madd(&r, h, &t);
    x25519_ge_p1p1_to_p3(h, &r);
  }
}

#endif

static void cmov_cached(ge_cached *t, ge_cached *u, uint8_t b) {
  fe_cmov(&t->YplusX, &u->YplusX, b);
  fe_cmov(&t->YminusX, &u->YminusX, b);
  fe_cmov(&t->Z, &u->Z, b);
  fe_cmov(&t->T2d, &u->T2d, b);
}

// r = scalar * A.
// where a = a[0]+256*a[1]+...+256^31 a[31].
static void x25519_ge_scalarmult(ge_p2 *r, const uint8_t *scalar,
                                 const ge_p3 *A) {
  ge_p2 Ai_p2[8];
  ge_cached Ai[16];
  ge_p1p1 t;

  ge_cached_0(&Ai[0]);
  x25519_ge_p3_to_cached(&Ai[1], A);
  ge_p3_to_p2(&Ai_p2[1], A);

  unsigned i;
  for (i = 2; i < 16; i += 2) {
    ge_p2_dbl(&t, &Ai_p2[i / 2]);
    ge_p1p1_to_cached(&Ai[i], &t);
    if (i < 8) {
      x25519_ge_p1p1_to_p2(&Ai_p2[i], &t);
    }
    x25519_ge_add(&t, A, &Ai[i]);
    ge_p1p1_to_cached(&Ai[i + 1], &t);
    if (i < 7) {
      x25519_ge_p1p1_to_p2(&Ai_p2[i + 1], &t);
    }
  }

  ge_p2_0(r);
  ge_p3 u;

  for (i = 0; i < 256; i += 4) {
    ge_p2_dbl(&t, r);
    x25519_ge_p1p1_to_p2(r, &t);
    ge_p2_dbl(&t, r);
    x25519_ge_p1p1_to_p2(r, &t);
    ge_p2_dbl(&t, r);
    x25519_ge_p1p1_to_p2(r, &t);
    ge_p2_dbl(&t, r);
    x25519_ge_p1p1_to_p3(&u, &t);

    uint8_t index = scalar[31 - i/8];
    index >>= 4 - (i & 4);
    index &= 0xf;

    unsigned j;
    ge_cached selected;
    ge_cached_0(&selected);
    for (j = 0; j < 16; j++) {
      cmov_cached(&selected, &Ai[j], equal(j, index));
    }

    x25519_ge_add(&t, &u, &selected);
    x25519_ge_p1p1_to_p2(r, &t);
  }
}

// int64_lshift21 returns |a << 21| but is defined when shifting bits into the
// sign bit. This works around a language flaw in C.
static inline int64_t int64_lshift21(int64_t a) {
  return (int64_t)((uint64_t)a << 21);
}

// The set of scalars is \Z/l
// where l = 2^252 + 27742317777372353535851937790883648493.

// Input:
//   s[0]+256*s[1]+...+256^63*s[63] = s
//
// Output:
//   s[0]+256*s[1]+...+256^31*s[31] = s mod l
//   where l = 2^252 + 27742317777372353535851937790883648493.
//   Overwrites s in place.
static void x25519_sc_reduce(uint8_t s[64]) {
  int64_t s0 = 2097151 & load_3(s);
  int64_t s1 = 2097151 & (load_4(s + 2) >> 5);
  int64_t s2 = 2097151 & (load_3(s + 5) >> 2);
  int64_t s3 = 2097151 & (load_4(s + 7) >> 7);
  int64_t s4 = 2097151 & (load_4(s + 10) >> 4);
  int64_t s5 = 2097151 & (load_3(s + 13) >> 1);
  int64_t s6 = 2097151 & (load_4(s + 15) >> 6);
  int64_t s7 = 2097151 & (load_3(s + 18) >> 3);
  int64_t s8 = 2097151 & load_3(s + 21);
  int64_t s9 = 2097151 & (load_4(s + 23) >> 5);
  int64_t s10 = 2097151 & (load_3(s + 26) >> 2);
  int64_t s11 = 2097151 & (load_4(s + 28) >> 7);
  int64_t s12 = 2097151 & (load_4(s + 31) >> 4);
  int64_t s13 = 2097151 & (load_3(s + 34) >> 1);
  int64_t s14 = 2097151 & (load_4(s + 36) >> 6);
  int64_t s15 = 2097151 & (load_3(s + 39) >> 3);
  int64_t s16 = 2097151 & load_3(s + 42);
  int64_t s17 = 2097151 & (load_4(s + 44) >> 5);
  int64_t s18 = 2097151 & (load_3(s + 47) >> 2);
  int64_t s19 = 2097151 & (load_4(s + 49) >> 7);
  int64_t s20 = 2097151 & (load_4(s + 52) >> 4);
  int64_t s21 = 2097151 & (load_3(s + 55) >> 1);
  int64_t s22 = 2097151 & (load_4(s + 57) >> 6);
  int64_t s23 = (load_4(s + 60) >> 3);
  int64_t carry0;
  int64_t carry1;
  int64_t carry2;
  int64_t carry3;
  int64_t carry4;
  int64_t carry5;
  int64_t carry6;
  int64_t carry7;
  int64_t carry8;
  int64_t carry9;
  int64_t carry10;
  int64_t carry11;
  int64_t carry12;
  int64_t carry13;
  int64_t carry14;
  int64_t carry15;
  int64_t carry16;

  s11 += s23 * 666643;
  s12 += s23 * 470296;
  s13 += s23 * 654183;
  s14 -= s23 * 997805;
  s15 += s23 * 136657;
  s16 -= s23 * 683901;
  s23 = 0;

  s10 += s22 * 666643;
  s11 += s22 * 470296;
  s12 += s22 * 654183;
  s13 -= s22 * 997805;
  s14 += s22 * 136657;
  s15 -= s22 * 683901;
  s22 = 0;

  s9 += s21 * 666643;
  s10 += s21 * 470296;
  s11 += s21 * 654183;
  s12 -= s21 * 997805;
  s13 += s21 * 136657;
  s14 -= s21 * 683901;
  s21 = 0;

  s8 += s20 * 666643;
  s9 += s20 * 470296;
  s10 += s20 * 654183;
  s11 -= s20 * 997805;
  s12 += s20 * 136657;
  s13 -= s20 * 683901;
  s20 = 0;

  s7 += s19 * 666643;
  s8 += s19 * 470296;
  s9 += s19 * 654183;
  s10 -= s19 * 997805;
  s11 += s19 * 136657;
  s12 -= s19 * 683901;
  s19 = 0;

  s6 += s18 * 666643;
  s7 += s18 * 470296;
  s8 += s18 * 654183;
  s9 -= s18 * 997805;
  s10 += s18 * 136657;
  s11 -= s18 * 683901;
  s18 = 0;

  carry6 = (s6 + (1 << 20)) >> 21;
  s7 += carry6;
  s6 -= int64_lshift21(carry6);
  carry8 = (s8 + (1 << 20)) >> 21;
  s9 += carry8;
  s8 -= int64_lshift21(carry8);
  carry10 = (s10 + (1 << 20)) >> 21;
  s11 += carry10;
  s10 -= int64_lshift21(carry10);
  carry12 = (s12 + (1 << 20)) >> 21;
  s13 += carry12;
  s12 -= int64_lshift21(carry12);
  carry14 = (s14 + (1 << 20)) >> 21;
  s15 += carry14;
  s14 -= int64_lshift21(carry14);
  carry16 = (s16 + (1 << 20)) >> 21;
  s17 += carry16;
  s16 -= int64_lshift21(carry16);

  carry7 = (s7 + (1 << 20)) >> 21;
  s8 += carry7;
  s7 -= int64_lshift21(carry7);
  carry9 = (s9 + (1 << 20)) >> 21;
  s10 += carry9;
  s9 -= int64_lshift21(carry9);
  carry11 = (s11 + (1 << 20)) >> 21;
  s12 += carry11;
  s11 -= int64_lshift21(carry11);
  carry13 = (s13 + (1 << 20)) >> 21;
  s14 += carry13;
  s13 -= int64_lshift21(carry13);
  carry15 = (s15 + (1 << 20)) >> 21;
  s16 += carry15;
  s15 -= int64_lshift21(carry15);

  s5 += s17 * 666643;
  s6 += s17 * 470296;
  s7 += s17 * 654183;
  s8 -= s17 * 997805;
  s9 += s17 * 136657;
  s10 -= s17 * 683901;
  s17 = 0;

  s4 += s16 * 666643;
  s5 += s16 * 470296;
  s6 += s16 * 654183;
  s7 -= s16 * 997805;
  s8 += s16 * 136657;
  s9 -= s16 * 683901;
  s16 = 0;

  s3 += s15 * 666643;
  s4 += s15 * 470296;
  s5 += s15 * 654183;
  s6 -= s15 * 997805;
  s7 += s15 * 136657;
  s8 -= s15 * 683901;
  s15 = 0;

  s2 += s14 * 666643;
  s3 += s14 * 470296;
  s4 += s14 * 654183;
  s5 -= s14 * 997805;
  s6 += s14 * 136657;
  s7 -= s14 * 683901;
  s14 = 0;

  s1 += s13 * 666643;
  s2 += s13 * 470296;
  s3 += s13 * 654183;
  s4 -= s13 * 997805;
  s5 += s13 * 136657;
  s6 -= s13 * 683901;
  s13 = 0;

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = (s0 + (1 << 20)) >> 21;
  s1 += carry0;
  s0 -= int64_lshift21(carry0);
  carry2 = (s2 + (1 << 20)) >> 21;
  s3 += carry2;
  s2 -= int64_lshift21(carry2);
  carry4 = (s4 + (1 << 20)) >> 21;
  s5 += carry4;
  s4 -= int64_lshift21(carry4);
  carry6 = (s6 + (1 << 20)) >> 21;
  s7 += carry6;
  s6 -= int64_lshift21(carry6);
  carry8 = (s8 + (1 << 20)) >> 21;
  s9 += carry8;
  s8 -= int64_lshift21(carry8);
  carry10 = (s10 + (1 << 20)) >> 21;
  s11 += carry10;
  s10 -= int64_lshift21(carry10);

  carry1 = (s1 + (1 << 20)) >> 21;
  s2 += carry1;
  s1 -= int64_lshift21(carry1);
  carry3 = (s3 + (1 << 20)) >> 21;
  s4 += carry3;
  s3 -= int64_lshift21(carry3);
  carry5 = (s5 + (1 << 20)) >> 21;
  s6 += carry5;
  s5 -= int64_lshift21(carry5);
  carry7 = (s7 + (1 << 20)) >> 21;
  s8 += carry7;
  s7 -= int64_lshift21(carry7);
  carry9 = (s9 + (1 << 20)) >> 21;
  s10 += carry9;
  s9 -= int64_lshift21(carry9);
  carry11 = (s11 + (1 << 20)) >> 21;
  s12 += carry11;
  s11 -= int64_lshift21(carry11);

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = s0 >> 21;
  s1 += carry0;
  s0 -= int64_lshift21(carry0);
  carry1 = s1 >> 21;
  s2 += carry1;
  s1 -= int64_lshift21(carry1);
  carry2 = s2 >> 21;
  s3 += carry2;
  s2 -= int64_lshift21(carry2);
  carry3 = s3 >> 21;
  s4 += carry3;
  s3 -= int64_lshift21(carry3);
  carry4 = s4 >> 21;
  s5 += carry4;
  s4 -= int64_lshift21(carry4);
  carry5 = s5 >> 21;
  s6 += carry5;
  s5 -= int64_lshift21(carry5);
  carry6 = s6 >> 21;
  s7 += carry6;
  s6 -= int64_lshift21(carry6);
  carry7 = s7 >> 21;
  s8 += carry7;
  s7 -= int64_lshift21(carry7);
  carry8 = s8 >> 21;
  s9 += carry8;
  s8 -= int64_lshift21(carry8);
  carry9 = s9 >> 21;
  s10 += carry9;
  s9 -= int64_lshift21(carry9);
  carry10 = s10 >> 21;
  s11 += carry10;
  s10 -= int64_lshift21(carry10);
  carry11 = s11 >> 21;
  s12 += carry11;
  s11 -= int64_lshift21(carry11);

  s0 += s12 * 666643;
  s1 += s12 * 470296;
  s2 += s12 * 654183;
  s3 -= s12 * 997805;
  s4 += s12 * 136657;
  s5 -= s12 * 683901;
  s12 = 0;

  carry0 = s0 >> 21;
  s1 += carry0;
  s0 -= int64_lshift21(carry0);
  carry1 = s1 >> 21;
  s2 += carry1;
  s1 -= int64_lshift21(carry1);
  carry2 = s2 >> 21;
  s3 += carry2;
  s2 -= int64_lshift21(carry2);
  carry3 = s3 >> 21;
  s4 += carry3;
  s3 -= int64_lshift21(carry3);
  carry4 = s4 >> 21;
  s5 += carry4;
  s4 -= int64_lshift21(carry4);
  carry5 = s5 >> 21;
  s6 += carry5;
  s5 -= int64_lshift21(carry5);
  carry6 = s6 >> 21;
  s7 += carry6;
  s6 -= int64_lshift21(carry6);
  carry7 = s7 >> 21;
  s8 += carry7;
  s7 -= int64_lshift21(carry7);
  carry8 = s8 >> 21;
  s9 += carry8;
  s8 -= int64_lshift21(carry8);
  carry9 = s9 >> 21;
  s10 += carry9;
  s9 -= int64_lshift21(carry9);
  carry10 = s10 >> 21;
  s11 += carry10;
  s10 -= int64_lshift21(carry10);

  s[0] = s0 >> 0;
  s[1] = s0 >> 8;
  s[2] = (s0 >> 16) | (s1 << 5);
  s[3] = s1 >> 3;
  s[4] = s1 >> 11;
  s[5] = (s1 >> 19) | (s2 << 2);
  s[6] = s2 >> 6;
  s[7] = (s2 >> 14) | (s3 << 7);
  s[8] = s3 >> 1;
  s[9] = s3 >> 9;
  s[10] = (s3 >> 17) | (s4 << 4);
  s[11] = s4 >> 4;
  s[12] = s4 >> 12;
  s[13] = (s4 >> 20) | (s5 << 1);
  s[14] = s5 >> 7;
  s[15] = (s5 >> 15) | (s6 << 6);
  s[16] = s6 >> 2;
  s[17] = s6 >> 10;
  s[18] = (s6 >> 18) | (s7 << 3);
  s[19] = s7 >> 5;
  s[20] = s7 >> 13;
  s[21] = s8 >> 0;
  s[22] = s8 >> 8;
  s[23] = (s8 >> 16) | (s9 << 5);
  s[24] = s9 >> 3;
  s[25] = s9 >> 11;
  s[26] = (s9 >> 19) | (s10 << 2);
  s[27] = s10 >> 6;
  s[28] = (s10 >> 14) | (s11 << 7);
  s[29] = s11 >> 1;
  s[30] = s11 >> 9;
  s[31] = s11 >> 17;
}

/* Loosely from BoringSSL crypto/curve25519/spake25519.c */

/*
 * Here BoringSSL uses different points, not restricted to the generator
 * subgroup, while we use the draft-irtf-cfrg-spake2-05 points.  The Python
 * code is modified to add the subgroup restriction.
 */

// The following precomputation tables are for the following
// points:
//
// N (found in 7 iterations):
//   x: 10742253510813957597047979962966927467575235974254765187031601461055699024931
//   y: 19796686047937480651099107989427797822652529149428697746066532921705571401683
//   encoded: d3bfb518f44f3430f29d0c92af503865a1ed3281dc69b35dd868ba85f886c4ab
//
// M (found in 21 iterations):
//   x: 8158688967149231307266666683326742915289288280191350817196911733632187385319
//   y: 21622333750659878624441478467798461427617029906629724657331223068277098105040
//   encoded: d048032c6ea0b6d697ddc2e86bda85a33adac920f1bf18e1b0c6d166a5cecdaf
//
// These points and their precomputation tables are generated with the
// following Python code.

/*
import hashlib
import ed25519 as E  # https://ed25519.cr.yp.to/python/ed25519.py

SEED_N = 'edwards25519 point generation seed (N)'
SEED_M = 'edwards25519 point generation seed (M)'

def genpoint(seed):
    v = hashlib.sha256(seed).digest()
    it = 1
    while True:
        try:
            x,y = E.decodepoint(v)
            if E.scalarmult((x,y), E.l) != [0, 1]:
                raise Exception('point has wrong order')
        except Exception, e:
            print e
            it += 1
            v = hashlib.sha256(v).digest()
            continue
        print "Found in %d iterations:" % it
        print "  x = %d" % x
        print "  y = %d" % y
        print " Encoded (hex)"
        print E.encodepoint((x,y)).encode('hex')
        return (x,y)

def gentable(P):
    t = []
    for i in range(1,16):
        k = (i >> 3 & 1) * (1 << 192) + \
            (i >> 2 & 1) * (1 << 128) + \
            (i >> 1 & 1) * (1 <<  64) + \
            (i      & 1)
        t.append(E.scalarmult(P, k))
    return ''.join(E.encodeint(x) + E.encodeint(y) for (x,y) in t)

def printtable(table, name):
    print "static const uint8_t %s[15 * 2 * 32] = {" % name,
    for i in range(15 * 2 * 32):
        if i % 12 == 0:
            print "\n   ",
        print " 0x%02x," % ord(table[i]),
    print "\n};"

if __name__ == "__main__":
    print "Searching for N"
    N = genpoint(SEED_N)
    print "Generating precomputation table for N"
    Ntable = gentable(N)
    printtable(Ntable, "kSpakeNSmallPrecomp")

    print "Searching for M"
    M = genpoint(SEED_M)
    print "Generating precomputation table for M"
    Mtable = gentable(M)
    printtable(Mtable, "kSpakeMSmallPrecomp")
*/

static const uint8_t kSpakeNSmallPrecomp[15 * 2 * 32] = {
    0x23, 0xfc, 0x27, 0x6c, 0x55, 0xaf, 0xb3, 0x9c, 0xd8, 0x99, 0x3a, 0x0d,
    0x7f, 0x08, 0xc9, 0xeb, 0x4d, 0x6e, 0x90, 0x99, 0x2f, 0x3c, 0x15, 0x2b,
    0x89, 0x5a, 0x0f, 0xf2, 0x67, 0xe6, 0xbf, 0x17, 0xd3, 0xbf, 0xb5, 0x18,
    0xf4, 0x4f, 0x34, 0x30, 0xf2, 0x9d, 0x0c, 0x92, 0xaf, 0x50, 0x38, 0x65,
    0xa1, 0xed, 0x32, 0x81, 0xdc, 0x69, 0xb3, 0x5d, 0xd8, 0x68, 0xba, 0x85,
    0xf8, 0x86, 0xc4, 0x2b, 0x53, 0x93, 0xb1, 0x99, 0x90, 0x30, 0xca, 0xb0,
    0xbd, 0xea, 0x14, 0x4c, 0x6f, 0x2b, 0x81, 0x1e, 0x23, 0x45, 0xb2, 0x32,
    0x2e, 0x2d, 0xe6, 0xb8, 0x5d, 0xc5, 0x15, 0x91, 0x63, 0x39, 0x18, 0x5b,
    0x62, 0x63, 0x9b, 0xf4, 0x8b, 0xe0, 0x34, 0xa2, 0x95, 0x11, 0x92, 0x68,
    0x54, 0xb7, 0xf3, 0x91, 0xca, 0x22, 0xad, 0x08, 0xd8, 0x9c, 0xa2, 0xf0,
    0xdc, 0x9c, 0x2c, 0x84, 0x32, 0x26, 0xe0, 0x17, 0x89, 0x53, 0x6b, 0xfd,
    0x76, 0x97, 0x25, 0xea, 0x99, 0x94, 0xf8, 0x29, 0x7c, 0xc4, 0x53, 0xc0,
    0x98, 0x9a, 0x20, 0xdc, 0x70, 0x01, 0x50, 0xaa, 0x05, 0xa3, 0x40, 0x50,
    0x66, 0x87, 0x30, 0x19, 0x12, 0xc3, 0xb8, 0x2d, 0x28, 0x8b, 0x7b, 0x48,
    0xf7, 0x7b, 0xab, 0x45, 0x70, 0x2e, 0xbb, 0x85, 0xc1, 0x6c, 0xdd, 0x35,
    0x00, 0x83, 0x20, 0x13, 0x82, 0x08, 0xaa, 0xa3, 0x03, 0x0f, 0xca, 0x27,
    0x3e, 0x8b, 0x52, 0xc2, 0xd7, 0xb1, 0x8c, 0x22, 0xfe, 0x04, 0x4a, 0xf2,
    0xe8, 0xac, 0xee, 0x2e, 0xd7, 0x77, 0x34, 0x49, 0xf2, 0xe9, 0xeb, 0x8c,
    0xa6, 0xc8, 0xc6, 0xcd, 0x8a, 0x8f, 0x7c, 0x5d, 0x51, 0xc8, 0xfa, 0x6f,
    0xb3, 0x93, 0xdb, 0x71, 0xef, 0x3e, 0x6e, 0xa7, 0x85, 0xc7, 0xd4, 0x3e,
    0xa2, 0xe2, 0xc0, 0xaa, 0x17, 0xb3, 0xa4, 0x7c, 0xc2, 0x3f, 0x7c, 0x7a,
    0xdd, 0x26, 0xde, 0x3e, 0xf1, 0x99, 0x06, 0xf7, 0x69, 0x1b, 0xc9, 0x20,
    0x55, 0x4f, 0x86, 0x7a, 0x93, 0x89, 0x68, 0xe9, 0x2b, 0x2d, 0xbc, 0x08,
    0x15, 0x5d, 0x2d, 0x0b, 0x4f, 0x1a, 0xb3, 0xd4, 0x8e, 0x77, 0x79, 0x2a,
    0x25, 0xf9, 0xb6, 0x46, 0xfb, 0x87, 0x02, 0xa6, 0xe0, 0xd3, 0xba, 0x84,
    0xea, 0x3e, 0x58, 0xa5, 0x7f, 0x8f, 0x8c, 0x39, 0x79, 0x28, 0xb5, 0xcf,
    0xe4, 0xca, 0x63, 0xdc, 0xac, 0xed, 0x4b, 0x74, 0x1e, 0x94, 0x85, 0x8c,
    0xe5, 0xf4, 0x76, 0x6f, 0x20, 0x67, 0x8b, 0xd8, 0xd6, 0x4b, 0xe7, 0x2d,
    0xa0, 0xbd, 0xcc, 0x1f, 0xdf, 0x46, 0x9c, 0xa2, 0x49, 0x64, 0xdf, 0x24,
    0x00, 0x11, 0x11, 0x45, 0x62, 0x5c, 0xd7, 0x8a, 0x00, 0x02, 0xf5, 0x9b,
    0x4f, 0x53, 0x42, 0xc5, 0xd5, 0x55, 0x80, 0x73, 0x9a, 0x5b, 0x31, 0x5a,
    0xbd, 0x3a, 0x43, 0xe9, 0x33, 0xe5, 0xaf, 0x1d, 0x92, 0x5e, 0x59, 0x37,
    0xae, 0x57, 0xfa, 0x3b, 0xd2, 0x31, 0xae, 0xa6, 0xf9, 0xc9, 0xc1, 0x82,
    0xa6, 0xa5, 0xed, 0x24, 0x53, 0x4b, 0x38, 0x22, 0xf2, 0x85, 0x8d, 0x13,
    0xa6, 0x5e, 0xd6, 0x57, 0x17, 0xd3, 0x33, 0x38, 0x8d, 0x65, 0xd3, 0xcb,
    0x1a, 0xa2, 0x3a, 0x2b, 0xbb, 0x61, 0x53, 0xd7, 0xff, 0xcd, 0x20, 0xb6,
    0xbb, 0x8c, 0xab, 0x63, 0xef, 0xb8, 0x26, 0x7e, 0x81, 0x65, 0xaf, 0x90,
    0xfc, 0xd2, 0xb6, 0x72, 0xdb, 0xe9, 0x23, 0x78, 0x12, 0x04, 0xc0, 0x03,
    0x82, 0xa8, 0x7a, 0x0f, 0x48, 0x6f, 0x82, 0x7f, 0x81, 0xcd, 0xa7, 0x89,
    0xdd, 0x86, 0xea, 0x5e, 0xa1, 0x50, 0x14, 0x34, 0x17, 0x64, 0x82, 0x0f,
    0xc4, 0x40, 0x20, 0x1d, 0x8f, 0xfe, 0xfa, 0x99, 0xaf, 0x5b, 0xc1, 0x5d,
    0xc8, 0x47, 0x07, 0x54, 0x4a, 0x22, 0x56, 0x57, 0xf1, 0x2c, 0x3b, 0x62,
    0x7f, 0x12, 0x62, 0xaf, 0xfd, 0xf8, 0x04, 0x11, 0xa8, 0x51, 0xf0, 0x46,
    0x5d, 0x79, 0x66, 0xff, 0x8a, 0x06, 0xef, 0x54, 0x64, 0x1b, 0x84, 0x3e,
    0x41, 0xf3, 0xfe, 0x19, 0x51, 0xf7, 0x44, 0x9c, 0x16, 0xd3, 0x7a, 0x09,
    0x59, 0xf5, 0x47, 0x45, 0xd0, 0x31, 0xef, 0x96, 0x2c, 0xc5, 0xc0, 0xd0,
    0x56, 0xef, 0x3f, 0x07, 0x2b, 0xb7, 0x28, 0x49, 0xf5, 0xb1, 0x42, 0x18,
    0xcf, 0x77, 0xd8, 0x2b, 0x71, 0x74, 0x80, 0xba, 0x34, 0x52, 0xce, 0x11,
    0xfe, 0xc4, 0xb9, 0xeb, 0xf9, 0xc4, 0x5e, 0x1f, 0xd3, 0xde, 0x4b, 0x14,
    0xe3, 0x6e, 0xe7, 0xd7, 0x83, 0x59, 0x98, 0xe8, 0x3d, 0x8e, 0xd6, 0x7d,
    0xc0, 0x9a, 0x79, 0xb9, 0x83, 0xf1, 0xc1, 0x00, 0x5d, 0x16, 0x1b, 0x44,
    0xe9, 0x02, 0xce, 0x99, 0x1e, 0x77, 0xef, 0xca, 0xbc, 0xf0, 0x6a, 0xb9,
    0x65, 0x3f, 0x3c, 0xd9, 0xe1, 0x63, 0x0b, 0xbf, 0xaa, 0xa7, 0xe6, 0x6d,
    0x6d, 0x3f, 0x44, 0x29, 0xa3, 0x8b, 0x6d, 0xc4, 0x81, 0xa9, 0xc3, 0x5a,
    0x90, 0x55, 0x72, 0x61, 0x17, 0x22, 0x7f, 0x3e, 0x5f, 0xfc, 0xba, 0xb3,
    0x7a, 0x99, 0x76, 0xe9, 0x20, 0xe5, 0xc5, 0xe8, 0x55, 0x56, 0x0f, 0x7a,
    0x48, 0xe7, 0xbc, 0xe1, 0x13, 0xf4, 0x90, 0xef, 0x97, 0x6c, 0x02, 0x89,
    0x4d, 0x22, 0x48, 0xda, 0xd3, 0x52, 0x45, 0x31, 0x26, 0xcc, 0xe8, 0x9e,
    0x5d, 0xdd, 0x75, 0xe4, 0x1d, 0xbc, 0xb1, 0x08, 0x55, 0xaf, 0x54, 0x70,
    0x0d, 0x0c, 0xf3, 0x50, 0xbc, 0x40, 0x83, 0xee, 0xdc, 0x6d, 0x8b, 0x40,
    0x79, 0x62, 0x18, 0x37, 0xc4, 0x78, 0x02, 0x58, 0x7c, 0x78, 0xd3, 0x54,
    0xed, 0x31, 0xbd, 0x7d, 0x48, 0xcf, 0xb6, 0x11, 0x27, 0x37, 0x9c, 0x86,
    0xf7, 0x2e, 0x00, 0x7a, 0x48, 0x1b, 0xa6, 0x72, 0x70, 0x7b, 0x44, 0x45,
    0xeb, 0x49, 0xbf, 0xbe, 0x09, 0x78, 0x66, 0x71, 0x12, 0x7f, 0x3d, 0x78,
    0x51, 0x24, 0x82, 0xa2, 0xf0, 0x1e, 0x83, 0x81, 0x81, 0x45, 0x53, 0xfd,
    0x5e, 0xf3, 0x03, 0x74, 0xbd, 0x23, 0x35, 0xf6, 0x10, 0xdd, 0x7c, 0x73,
    0x46, 0x32, 0x09, 0x54, 0x99, 0x95, 0x91, 0x25, 0xb8, 0x32, 0x09, 0xd8,
    0x2f, 0x97, 0x50, 0xa3, 0xf5, 0xd6, 0xb1, 0xed, 0x97, 0x51, 0x06, 0x42,
    0x12, 0x0c, 0x69, 0x38, 0x09, 0xa0, 0xd8, 0x19, 0x70, 0xf7, 0x8f, 0x61,
    0x0d, 0x56, 0x43, 0x66, 0x22, 0x8b, 0x0e, 0x0e, 0xf9, 0x81, 0x9f, 0xac,
    0x6f, 0xbf, 0x7d, 0x04, 0x13, 0xf2, 0xe4, 0xeb, 0xfd, 0xbe, 0x4e, 0x56,
    0xda, 0xe0, 0x22, 0x6d, 0x1b, 0x25, 0xc8, 0xa5, 0x9c, 0x05, 0x45, 0x52,
    0x3c, 0x3a, 0xde, 0x6b, 0xac, 0x9b, 0xf8, 0x81, 0x97, 0x21, 0x46, 0xac,
    0x7e, 0x89, 0xf8, 0x49, 0x58, 0xbb, 0x45, 0xac, 0xa2, 0xc4, 0x90, 0x1f,
    0xb2, 0xb4, 0xf8, 0xe0, 0xcd, 0xa1, 0x9d, 0x1c, 0xf2, 0xf1, 0xdf, 0xfb,
    0x88, 0x4e, 0xe5, 0x41, 0xd8, 0x6e, 0xac, 0x07, 0x87, 0x95, 0x35, 0xa6,
    0x12, 0x08, 0x5d, 0x57, 0x5e, 0xaf, 0x71, 0x0f, 0x07, 0x4e, 0x81, 0x77,
    0xf1, 0xef, 0xb5, 0x35, 0x5c, 0xfa, 0xf4, 0x4e, 0x42, 0xdc, 0x19, 0xfe,
    0xe4, 0xd2, 0xb4, 0x27, 0xfb, 0x34, 0x1f, 0xb2, 0x6f, 0xf2, 0x95, 0xcc,
    0xd4, 0x47, 0x63, 0xdc, 0x7e, 0x4f, 0x97, 0x2b, 0x7a, 0xe0, 0x80, 0x31,
};

static const uint8_t kSpakeMSmallPrecomp[15 * 2 * 32] = {
    0xe7, 0x45, 0x7e, 0x47, 0x49, 0x69, 0xbd, 0x1b, 0x35, 0x1c, 0x2c, 0x98,
    0x03, 0xf3, 0xb3, 0x37, 0xde, 0x39, 0xa5, 0xda, 0xc0, 0x2e, 0xa4, 0xac,
    0x7d, 0x08, 0x26, 0xfc, 0x80, 0xa7, 0x09, 0x12, 0xd0, 0x48, 0x03, 0x2c,
    0x6e, 0xa0, 0xb6, 0xd6, 0x97, 0xdd, 0xc2, 0xe8, 0x6b, 0xda, 0x85, 0xa3,
    0x3a, 0xda, 0xc9, 0x20, 0xf1, 0xbf, 0x18, 0xe1, 0xb0, 0xc6, 0xd1, 0x66,
    0xa5, 0xce, 0xcd, 0x2f, 0x80, 0xa8, 0x4e, 0xc3, 0x81, 0xae, 0x68, 0x3b,
    0x0d, 0xdb, 0x56, 0x32, 0x2f, 0xa8, 0x97, 0xa0, 0x5c, 0x15, 0xc1, 0xcb,
    0x6f, 0x7a, 0x5f, 0xc5, 0x32, 0xfb, 0x49, 0x17, 0x18, 0xfa, 0x85, 0x08,
    0x85, 0xf1, 0xe3, 0x11, 0x8e, 0x3d, 0x70, 0x20, 0x38, 0x4e, 0x0c, 0x17,
    0xa1, 0xa8, 0x20, 0xd2, 0xb1, 0x1d, 0x05, 0x8d, 0x0f, 0xc9, 0x96, 0x18,
    0x9d, 0x8c, 0x89, 0x8f, 0x46, 0x6a, 0x6c, 0x6e, 0x72, 0x03, 0xb2, 0x75,
    0x87, 0xd8, 0xa9, 0x60, 0x93, 0x2b, 0x8b, 0x66, 0xee, 0xaf, 0xce, 0x98,
    0xcd, 0x6b, 0x7c, 0x6a, 0xbe, 0x19, 0xda, 0x66, 0x7c, 0xda, 0x53, 0xa0,
    0xe3, 0x9a, 0x0e, 0x53, 0x3a, 0x7c, 0x73, 0x4a, 0x37, 0xa6, 0x53, 0x23,
    0x67, 0x31, 0xce, 0x8a, 0xab, 0xee, 0x72, 0x76, 0xc2, 0xb5, 0x54, 0x42,
    0xcf, 0x4b, 0xc7, 0x53, 0x24, 0x59, 0xaf, 0x76, 0x53, 0x10, 0x7e, 0x25,
    0x94, 0x5c, 0x23, 0xa6, 0x5e, 0x05, 0xea, 0x14, 0xad, 0x2b, 0xce, 0x50,
    0x77, 0xb3, 0x7a, 0x88, 0x4c, 0xf7, 0x74, 0x04, 0x35, 0xa4, 0x0c, 0x9e,
    0xee, 0x6a, 0x4c, 0x3c, 0xc1, 0x6a, 0x35, 0x4d, 0x6d, 0x8f, 0x94, 0x95,
    0xe4, 0x10, 0xca, 0x46, 0x4e, 0xfa, 0x38, 0x40, 0xeb, 0x1a, 0x1b, 0x5a,
    0xff, 0x73, 0x4d, 0xe9, 0xf2, 0xbe, 0x89, 0xf5, 0xd1, 0x72, 0xd0, 0x1a,
    0x7b, 0x82, 0x08, 0x19, 0xda, 0x54, 0x44, 0xa5, 0x3d, 0xd8, 0x10, 0x1c,
    0xcf, 0x3b, 0xc7, 0x54, 0xd5, 0x11, 0xd7, 0x2a, 0x69, 0x3f, 0xa6, 0x58,
    0x74, 0xfd, 0x90, 0xb2, 0xf4, 0xc2, 0x0e, 0xf3, 0x19, 0x8f, 0x51, 0x7c,
    0x31, 0x12, 0x79, 0x61, 0x16, 0xb4, 0x2f, 0x2f, 0xd0, 0x88, 0x97, 0xf2,
    0xc3, 0x8c, 0xa6, 0xa3, 0x29, 0xff, 0x7e, 0x12, 0x46, 0x2a, 0x9c, 0x09,
    0x7c, 0x5f, 0x87, 0x07, 0x6b, 0xa1, 0x9a, 0x57, 0x55, 0x8e, 0xb0, 0x56,
    0x5d, 0xc9, 0x4c, 0x5b, 0xae, 0xd3, 0xd0, 0x8e, 0xb8, 0xac, 0xba, 0xe8,
    0x54, 0x45, 0x30, 0x14, 0xf6, 0x59, 0x20, 0xc4, 0x03, 0xb7, 0x7a, 0x5d,
    0x6b, 0x5a, 0xcb, 0x28, 0x60, 0xf8, 0xef, 0x61, 0x60, 0x78, 0x6b, 0xf5,
    0x21, 0x4b, 0x75, 0xc2, 0x77, 0xba, 0x0e, 0x38, 0x98, 0xe0, 0xfb, 0xb7,
    0x5f, 0x75, 0x87, 0x04, 0x0c, 0xb4, 0x5c, 0x09, 0x04, 0x00, 0x38, 0x4e,
    0x4f, 0x7b, 0x73, 0xe5, 0xdb, 0xdb, 0xf1, 0xf4, 0x5c, 0x64, 0x68, 0xfd,
    0xb1, 0x86, 0xe8, 0x89, 0xbe, 0x9c, 0xd4, 0x96, 0x1d, 0xcb, 0xdc, 0x5c,
    0xef, 0xd4, 0x33, 0x28, 0xb9, 0xb6, 0xaf, 0x3b, 0xcf, 0x8d, 0x30, 0xba,
    0xe8, 0x08, 0xcf, 0x84, 0xba, 0x61, 0x10, 0x9b, 0x62, 0xf6, 0x18, 0x79,
    0x66, 0x87, 0x82, 0x7c, 0xaa, 0x71, 0xac, 0xd0, 0xd0, 0x32, 0xb0, 0x54,
    0x03, 0xa4, 0xad, 0x3f, 0x72, 0xca, 0x22, 0xff, 0x01, 0x87, 0x08, 0x36,
    0x61, 0x22, 0xaa, 0x18, 0xab, 0x3a, 0xbc, 0xf2, 0x78, 0x05, 0xe1, 0x99,
    0xa3, 0x59, 0x98, 0xcc, 0x21, 0xc6, 0x2b, 0x51, 0x6d, 0x43, 0x0a, 0x46,
    0x50, 0xae, 0x11, 0x7e, 0xd5, 0x23, 0x56, 0xef, 0x83, 0xc8, 0xbf, 0x42,
    0xf0, 0x45, 0x52, 0x1f, 0x34, 0xbc, 0x2f, 0xb0, 0xf0, 0xce, 0xf0, 0xec,
    0xd0, 0x99, 0x59, 0x2e, 0x1f, 0xab, 0xa8, 0x1e, 0x4b, 0xce, 0x1b, 0x9a,
    0x75, 0xc6, 0xc4, 0x71, 0x86, 0xf0, 0x8d, 0xec, 0xb0, 0x30, 0xb9, 0x62,
    0xb3, 0xb7, 0xdd, 0x96, 0x29, 0xc8, 0xbf, 0xe9, 0xb0, 0x74, 0x78, 0x7b,
    0xf7, 0xea, 0xa3, 0x14, 0x12, 0x56, 0xe0, 0xf3, 0x35, 0x7a, 0x26, 0x4a,
    0x4c, 0xe6, 0xdf, 0x13, 0xb5, 0x52, 0xb0, 0x2a, 0x5f, 0x2e, 0xac, 0x34,
    0xab, 0x5f, 0x1a, 0x01, 0xe4, 0x15, 0x1a, 0xd1, 0xbf, 0xc9, 0x95, 0x0a,
    0xac, 0x1d, 0xe7, 0x53, 0x59, 0x8d, 0xc3, 0x21, 0x78, 0x5e, 0x12, 0x97,
    0x8f, 0x4e, 0x1d, 0xf9, 0xe5, 0xe2, 0xc2, 0xc4, 0xba, 0xfb, 0x50, 0x96,
    0x5b, 0x43, 0xe8, 0xf7, 0x0d, 0x1b, 0x64, 0x58, 0xbe, 0xd3, 0x95, 0x7f,
    0x8e, 0xf1, 0x85, 0x35, 0xba, 0x25, 0x55, 0x2e, 0x02, 0x46, 0x5c, 0xad,
    0x1f, 0xc5, 0x03, 0xcc, 0xd0, 0x43, 0x4c, 0xf2, 0x5e, 0x64, 0x0a, 0x89,
    0xd9, 0xfd, 0x23, 0x7d, 0x4f, 0xbe, 0x2f, 0x0f, 0x1e, 0x12, 0x4a, 0xd9,
    0xf8, 0x82, 0xde, 0x8f, 0x4f, 0x98, 0xb9, 0x90, 0xf6, 0xfa, 0xd1, 0x11,
    0xa6, 0xdc, 0x7e, 0x32, 0x48, 0x6a, 0x8a, 0x14, 0x5e, 0x73, 0xb9, 0x6c,
    0x0e, 0xc2, 0xf9, 0xcc, 0xf0, 0x32, 0xc8, 0xb5, 0x56, 0xaa, 0x5d, 0xd2,
    0x07, 0xf1, 0x6f, 0x33, 0x6f, 0x05, 0x70, 0x49, 0x60, 0x49, 0x23, 0x23,
    0x14, 0x0e, 0x4c, 0x58, 0x92, 0xad, 0xa9, 0x50, 0xb1, 0x59, 0x43, 0x96,
    0x7b, 0xc1, 0x51, 0x45, 0xef, 0x0d, 0xef, 0xd1, 0xe4, 0xd0, 0xce, 0xdf,
    0x6a, 0xbc, 0x1b, 0xbf, 0x7a, 0x87, 0x4e, 0x47, 0x17, 0x9c, 0x34, 0x38,
    0xb0, 0x3c, 0xa1, 0x04, 0xfb, 0xe2, 0x66, 0xce, 0xb6, 0x82, 0xbb, 0xad,
    0xc3, 0x8e, 0x12, 0x35, 0xbc, 0x17, 0xce, 0x01, 0x2d, 0xa3, 0xa6, 0xb9,
    0xfa, 0x84, 0xc2, 0x2f, 0x5a, 0x4a, 0x8c, 0x4c, 0x11, 0x4e, 0xa8, 0x14,
    0xcb, 0xb8, 0x99, 0xaa, 0x2e, 0x8c, 0xa0, 0xc9, 0x5f, 0x62, 0x2a, 0x84,
    0x66, 0x60, 0x0a, 0x7e, 0xdc, 0x93, 0x17, 0x45, 0x19, 0xb3, 0x93, 0x4c,
    0xdc, 0xd0, 0xd5, 0x5c, 0x25, 0xd2, 0xcd, 0x4e, 0x84, 0x4c, 0x73, 0xb3,
    0x90, 0xa4, 0x22, 0x05, 0x2c, 0x7c, 0x39, 0x2b, 0x70, 0xd9, 0x61, 0x76,
    0xb2, 0x03, 0x71, 0xe9, 0x0e, 0xf8, 0x57, 0x85, 0xad, 0xb1, 0x2f, 0x34,
    0xa5, 0x66, 0xb0, 0x0f, 0x75, 0x94, 0x6e, 0x26, 0x79, 0x99, 0xb4, 0xe2,
    0xe2, 0xa3, 0x58, 0xdd, 0xb4, 0xfb, 0x74, 0xf4, 0xa1, 0xca, 0xc3, 0x30,
    0xe7, 0x86, 0xb2, 0xa2, 0x2c, 0x11, 0xc9, 0x58, 0xe3, 0xc1, 0xa6, 0x5f,
    0x86, 0x6a, 0xe7, 0x75, 0xd5, 0xd8, 0x63, 0x95, 0x64, 0x59, 0xbc, 0xb8,
    0xb7, 0xf5, 0x12, 0xe3, 0x03, 0xc6, 0x17, 0xea, 0x4e, 0xcb, 0xee, 0x4c,
    0xae, 0x03, 0xd1, 0x33, 0xd0, 0x39, 0x36, 0x00, 0x0f, 0xf4, 0x9c, 0xbd,
    0x35, 0x96, 0xfd, 0x0d, 0x26, 0xb7, 0x9e, 0xf4, 0x4b, 0x6f, 0x4b, 0xf1,
    0xec, 0x11, 0x00, 0x16, 0x21, 0x1e, 0xd4, 0x43, 0x23, 0x8c, 0x4a, 0xfa,
    0x9e, 0xd4, 0x2b, 0x36, 0x9a, 0x43, 0x1e, 0x58, 0x31, 0xe8, 0x1f, 0x83,
    0x15, 0x20, 0x31, 0x68, 0xfe, 0x27, 0xd3, 0xd8, 0x9b, 0x43, 0x81, 0x8f,
    0x57, 0x32, 0x14, 0xe6, 0x9e, 0xbf, 0xd1, 0xfb, 0xdf, 0xad, 0x7a, 0x52,
};

/* left_shift_3 sets |n| to |n|*8, where |n| is represented in little-endian
 * order. */
static void left_shift_3(uint8_t n[32]) {
  uint8_t carry = 0;
  unsigned i;

  for (i = 0; i < 32; i++) {
    const uint8_t next_carry = n[i] >> 5;
    n[i] = (n[i] << 3) | carry;
    carry = next_carry;
  }
}

static krb5_error_code
builtin_edwards25519_keygen(krb5_context context, groupdata *gdata,
                            const uint8_t *wbytes, krb5_boolean use_m,
                            uint8_t *priv_out, uint8_t *pub_out)
{
  uint8_t private[64];
  krb5_data data = make_data(private, 32);
  krb5_error_code ret;

  /* Pick x or y uniformly from [0, p*h) divisible by h. */
  ret = krb5_c_random_make_octets(context, &data);
  if (ret)
    return ret;
  memset(private + 32, 0, 32);
  x25519_sc_reduce(private);
  left_shift_3(private);

  /* Compute X=x*G or Y=y*G. */
  ge_p3 P;
  x25519_ge_scalarmult_base(&P, private);

  /* Compute w mod p. */
  uint8_t wreduced[64];
  memcpy(wreduced, wbytes, 32);
  memset(wreduced + 32, 0, 32);
  x25519_sc_reduce(wreduced);

  /* Compute the mask, w*M or w*N. */
  ge_p3 mask;
  x25519_ge_scalarmult_small_precomp(&mask, wreduced,
                                     use_m ? kSpakeMSmallPrecomp :
                                     kSpakeNSmallPrecomp);

  /* Compute the masked point T=w*M+X or S=w*N+Y. */
  ge_cached mask_cached;
  x25519_ge_p3_to_cached(&mask_cached, &mask);
  ge_p1p1 Pmasked;
  x25519_ge_add(&Pmasked, &P, &mask_cached);

  /* Encode T or S into pub_out. */
  ge_p2 Pmasked_proj;
  x25519_ge_p1p1_to_p2(&Pmasked_proj, &Pmasked);
  x25519_ge_tobytes(pub_out, &Pmasked_proj);

  /* Remember the private key in priv_out. */
  memcpy(priv_out, private, 32);
  return 0;
}

static krb5_error_code
builtin_edwards25519_result(krb5_context context, groupdata *gdata,
                            const uint8_t *wbytes, const uint8_t *ourpriv,
                            const uint8_t *theirpub, krb5_boolean use_m,
                            uint8_t *elem_out)
{
  /*
   * Check if the point received from peer is on the curve.  This does not
   * verify that it is in the generator subgroup, but since our private key is
   * a multiple of the cofactor, the shared point will be in the generator
   * subgroup even if a rogue peer sends a point which is not.
   */
  ge_p3 Qmasked;
  if (!x25519_ge_frombytes_vartime(&Qmasked, theirpub))
    return EINVAL;

  /* Compute w mod p. */
  uint8_t wreduced[64];
  memcpy(wreduced, wbytes, 32);
  memset(wreduced + 32, 0, 32);
  x25519_sc_reduce(wreduced);

  /* Compute the peer's mask, w*M or w*N. */
  ge_p3 peers_mask;
  x25519_ge_scalarmult_small_precomp(&peers_mask, wreduced,
                                     use_m ? kSpakeMSmallPrecomp :
                                     kSpakeNSmallPrecomp);

  ge_cached peers_mask_cached;
  x25519_ge_p3_to_cached(&peers_mask_cached, &peers_mask);

  /* Compute the peer's unmasked point, T-w*M or S-w*N. */
  ge_p1p1 Qcompl;
  ge_p3 Qunmasked;
  x25519_ge_sub(&Qcompl, &Qmasked, &peers_mask_cached);
  x25519_ge_p1p1_to_p3(&Qunmasked, &Qcompl);

  /* Multiply by our private value to compute K=x*(S-w*N) or K=y*(T-w*M). */
  ge_p2 K;
  x25519_ge_scalarmult(&K, ourpriv, &Qunmasked);

  /* Encode K into elem_out. */
  x25519_ge_tobytes(elem_out, &K);
  return 0;
}

static krb5_error_code
builtin_sha256(krb5_context context, groupdata *gdata, const krb5_data *dlist,
               size_t ndata, uint8_t *result_out)
{
  return k5_sha256(dlist, ndata, result_out);
}

groupdef builtin_edwards25519 = {
  .reg = &spake_iana_edwards25519,
  .keygen = builtin_edwards25519_keygen,
  .result = builtin_edwards25519_result,
  .hash = builtin_sha256
};
