/*
 * memset test.
 *
 * Copyright (c) 2019-2020, Arm Limited.
 * SPDX-License-Identifier: MIT
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
  void *(*fun) (void *s, int c, size_t n);
  int test_mte;
} funtab[] = {
  // clang-format off
  F(memset, 0)
#if __aarch64__
  F(__memset_aarch64, 1)
#elif __arm__
  F(__memset_arm, 0)
#endif
  {0, 0, 0}
  // clang-format on
};
#undef F

#define A 32
#define LEN 250000
static unsigned char *sbuf;

static void *
alignup (void *p)
{
  return (void *) (((uintptr_t) p + A - 1) & -A);
}

static void
test (const struct fun *fun, int salign, int c, int len)
{
  unsigned char *src = alignup (sbuf);
  unsigned char *s = src + salign;
  void *p;
  int i;

  if (err_count >= ERR_LIMIT)
    return;
  if (len > LEN || salign >= A)
    abort ();
  for (i = 0; i < len + A; i++)
    src[i] = '?';
  for (i = 0; i < len; i++)
    s[i] = 'a' + i % 23;

  s = tag_buffer (s, len, fun->test_mte);
  p = fun->fun (s, c, len);
  untag_buffer (s, len, fun->test_mte);

  if (p != s)
    ERR ("%s(%p,..) returned %p\n", fun->name, s, p);

  for (i = 0; i < salign; i++)
    {
      if (src[i] != '?')
	{
	  ERR ("%s(align %d, %d, %d) failed\n", fun->name, salign, c, len);
	  quoteat ("got", src, len + A, i);
	  return;
	}
    }
  for (; i < salign + len; i++)
    {
      if (src[i] != (unsigned char) c)
	{
	  ERR ("%s(align %d, %d, %d) failed\n", fun->name, salign, c, len);
	  quoteat ("got", src, len + A, i);
	  return;
	}
    }
  for (; i < len + A; i++)
    {
      if (src[i] != '?')
	{
	  ERR ("%s(align %d, %d, %d) failed\n", fun->name, salign, c, len);
	  quoteat ("got", src, len + A, i);
	  return;
	}
    }
}

int
main ()
{
  sbuf = mte_mmap (LEN + 2 * A);
  int r = 0;
  for (int i = 0; funtab[i].name; i++)
    {
      err_count = 0;
      for (int s = 0; s < A; s++)
	{
	  int n;
	  for (n = 0; n < 100; n++)
	    {
	      test (funtab + i, s, 0, n);
	      test (funtab + i, s, 0x25, n);
	      test (funtab + i, s, 0xaa25, n);
	    }
	  for (; n < LEN; n *= 2)
	    {
	      test (funtab + i, s, 0, n);
	      test (funtab + i, s, 0x25, n);
	      test (funtab + i, s, 0xaa25, n);
	    }
	}
      char *pass = funtab[i].test_mte && mte_enabled () ? "MTE PASS" : "PASS";
      printf ("%s %s\n", err_count ? "FAIL" : pass, funtab[i].name);
      if (err_count)
	r = -1;
    }
  return r;
}
