/*
 * memcmp test.
 *
 * Copyright (c) 2019-2020, Arm Limited.
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
  int (*fun) (const void *s1, const void *s2, size_t n);
  int test_mte;
} funtab[] = {
  // clang-format off
  F(memcmp, 0)
#if __aarch64__
  F(__memcmp_aarch64, 1)
# if __ARM_FEATURE_SVE
  F(__memcmp_aarch64_sve, 1)
# endif
#endif
  {0, 0, 0}
  // clang-format on
};
#undef F

#define A 32
#define LEN 250000
static unsigned char *s1buf;
static unsigned char *s2buf;

static void *
alignup (void *p)
{
  return (void *) (((uintptr_t) p + A - 1) & -A);
}

static void
test (const struct fun *fun, int s1align, int s2align, int len, int diffpos,
      int delta)
{
  unsigned char *src1 = alignup (s1buf);
  unsigned char *src2 = alignup (s2buf);
  unsigned char *s1 = src1 + s1align;
  unsigned char *s2 = src2 + s2align;
  int r;

  if (err_count >= ERR_LIMIT)
    return;
  if (len > LEN || s1align >= A || s2align >= A)
    abort ();
  if (diffpos >= len)
    abort ();
  if ((diffpos < 0) != (delta == 0))
    abort ();

  for (int i = 0; i < len + A; i++)
    src1[i] = src2[i] = '?';
  for (int i = 0; i < len; i++)
    s1[i] = s2[i] = 'a' + i % 23;
  if (delta)
    s1[diffpos] += delta;

  s1 = tag_buffer (s1, len, fun->test_mte);
  s2 = tag_buffer (s2, len, fun->test_mte);
  r = fun->fun (s1, s2, len);
  untag_buffer (s1, len, fun->test_mte);
  untag_buffer (s2, len, fun->test_mte);

  if ((delta == 0 && r != 0) || (delta > 0 && r <= 0) || (delta < 0 && r >= 0))
    {
      ERR ("%s(align %d, align %d, %d) failed, returned %d\n", fun->name,
	   s1align, s2align, len, r);
      quoteat ("src1", src1, len + A, diffpos);
      quoteat ("src2", src2, len + A, diffpos);
    }
}

int
main ()
{
  s1buf = mte_mmap (LEN + 2 * A);
  s2buf = mte_mmap (LEN + 2 * A);
  int r = 0;
  for (int i = 0; funtab[i].name; i++)
    {
      err_count = 0;
      for (int d = 0; d < A; d++)
	for (int s = 0; s < A; s++)
	  {
	    int n;
	    test (funtab + i, d, s, 0, -1, 0);
	    test (funtab + i, d, s, 1, -1, 0);
	    test (funtab + i, d, s, 1, 0, -1);
	    test (funtab + i, d, s, 1, 0, 1);
	    for (n = 2; n < 100; n++)
	      {
		test (funtab + i, d, s, n, -1, 0);
		test (funtab + i, d, s, n, 0, -1);
		test (funtab + i, d, s, n, n - 1, -1);
		test (funtab + i, d, s, n, n / 2, 1);
	      }
	    for (; n < LEN; n *= 2)
	      {
		test (funtab + i, d, s, n, -1, 0);
		test (funtab + i, d, s, n, n / 2, -1);
	      }
	  }
      char *pass = funtab[i].test_mte && mte_enabled () ? "MTE PASS" : "PASS";
      printf ("%s %s\n", err_count ? "FAIL" : pass, funtab[i].name);
      if (err_count)
	r = -1;
    }
  return r;
}
