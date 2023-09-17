/*
 * memmove test.
 *
 * Copyright (c) 2019-2022, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mte.h"
#include "stringlib.h"
#include "stringtest.h"

#define F(x, mte) {#x, x, mte},

static const struct fun
{
  const char *name;
  void *(*fun) (void *, const void *, size_t);
  int test_mte;
} funtab[] = {
  // clang-format off
  F(memmove, 0)
#if __aarch64__
  F(__memmove_aarch64, 1)
# if __ARM_NEON
  F(__memmove_aarch64_simd, 1)
# endif
# if __ARM_FEATURE_SVE
  F(__memmove_aarch64_sve, 1)
# endif
#endif
  {0, 0, 0}
  // clang-format on
};
#undef F

#define A 32
#define LEN 250000
static unsigned char *dbuf;
static unsigned char *sbuf;
static unsigned char wbuf[LEN + 2 * A];

static void *
alignup (void *p)
{
  return (void *) (((uintptr_t) p + A - 1) & -A);
}

static void
test (const struct fun *fun, int dalign, int salign, int len)
{
  unsigned char *src = alignup (sbuf);
  unsigned char *dst = alignup (dbuf);
  unsigned char *want = wbuf;
  unsigned char *s = src + salign;
  unsigned char *d = dst + dalign;
  unsigned char *w = want + dalign;
  void *p;
  int i;

  if (err_count >= ERR_LIMIT)
    return;
  if (len > LEN || dalign >= A || salign >= A)
    abort ();
  for (i = 0; i < len + A; i++)
    {
      src[i] = '?';
      want[i] = dst[i] = '*';
    }
  for (i = 0; i < len; i++)
    s[i] = w[i] = 'a' + i % 23;

  p = fun->fun (d, s, len);
  if (p != d)
    ERR ("%s(%p,..) returned %p\n", fun->name, d, p);
  for (i = 0; i < len + A; i++)
    {
      if (dst[i] != want[i])
	{
	  ERR ("%s(align %d, align %d, %d) failed\n", fun->name, dalign, salign,
	       len);
	  quoteat ("got", dst, len + A, i);
	  quoteat ("want", want, len + A, i);
	  break;
	}
    }
}

static void
test_overlap (const struct fun *fun, int dalign, int salign, int len)
{
  unsigned char *src = alignup (sbuf);
  unsigned char *dst = src;
  unsigned char *want = wbuf;
  unsigned char *s = src + salign;
  unsigned char *d = dst + dalign;
  unsigned char *w = wbuf + dalign;
  void *p;

  if (err_count >= ERR_LIMIT)
    return;
  if (len > LEN || dalign >= A || salign >= A)
    abort ();

  for (int i = 0; i < len + A; i++)
    src[i] = want[i] = '?';

  for (int i = 0; i < len; i++)
    s[i] = want[salign + i] = 'a' + i % 23;
  for (int i = 0; i < len; i++)
    w[i] = s[i];

  s = tag_buffer (s, len, fun->test_mte);
  d = tag_buffer (d, len, fun->test_mte);
  p = fun->fun (d, s, len);
  untag_buffer (s, len, fun->test_mte);
  untag_buffer (d, len, fun->test_mte);

  if (p != d)
    ERR ("%s(%p,..) returned %p\n", fun->name, d, p);
  for (int i = 0; i < len + A; i++)
    {
      if (dst[i] != want[i])
	{
	  ERR ("%s(align %d, align %d, %d) failed\n", fun->name, dalign, salign,
	       len);
	  quoteat ("got", dst, len + A, i);
	  quoteat ("want", want, len + A, i);
	  break;
	}
    }
}

int
main ()
{
  dbuf = mte_mmap (LEN + 2 * A);
  sbuf = mte_mmap (LEN + 2 * A);
  int r = 0;
  for (int i = 0; funtab[i].name; i++)
    {
      err_count = 0;
      for (int d = 0; d < A; d++)
	for (int s = 0; s < A; s++)
	  {
	    int n;
	    for (n = 0; n < 100; n++)
	      {
		test (funtab + i, d, s, n);
		test_overlap (funtab + i, d, s, n);
	      }
	    for (; n < LEN; n *= 2)
	      {
		test (funtab + i, d, s, n);
		test_overlap (funtab + i, d, s, n);
	      }
	  }
      char *pass = funtab[i].test_mte && mte_enabled () ? "MTE PASS" : "PASS";
      printf ("%s %s\n", err_count ? "FAIL" : pass, funtab[i].name);
      if (err_count)
	r = -1;
    }
  return r;
}
