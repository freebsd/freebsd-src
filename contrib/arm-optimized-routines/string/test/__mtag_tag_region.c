/*
 * __mtag_tag_region test.
 *
 * Copyright (c) 2021, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if __ARM_FEATURE_MEMORY_TAGGING && WANT_MTE_TEST
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mte.h"
#include "stringlib.h"
#include "stringtest.h"

static void
mtag_quoteat (const char *prefix, void *p, int len, int at)
{
  /* Print tag, untag and quote the context.  */
  printf ("location: %p\n", __arm_mte_get_tag ((char *) p + at));
  untag_buffer (p, len, 1);
  p = untag_pointer (p);
  quoteat (prefix, p, len, at);
}

#define F(x) {#x, x},

static const struct fun
{
  const char *name;
  void *(*fun) (void *s, size_t n);
} funtab[] = {
// clang-format off
#if __aarch64__
  F(__mtag_tag_region)
#endif
  {0, 0}
  // clang-format on
};
#undef F

#define A 64
#define LEN 250000
static unsigned char *sbuf;

static void *
alignup (void *p)
{
  return (void *) (((uintptr_t) p + A - 1) & -A);
}

static void
test (const struct fun *fun, int salign, int len)
{
  unsigned char *src = alignup (sbuf);
  unsigned char *s = src + salign;
  void *p;
  int i;

  if (err_count >= ERR_LIMIT)
    return;
  if (len > LEN || salign >= A)
    abort ();
  for (i = 0; i < len + 2 * A; i++)
    src[i] = '?';
  for (i = 0; i < len; i++)
    s[i] = 'a';

  src = tag_buffer (src, len + 2 * A, 1);
  s = src + salign;
  /* Use different tag.  */
  s = __arm_mte_increment_tag (s, 1);
  p = fun->fun (s, len);

  if (p != s)
    ERR ("%s(%p,..) returned %p\n", fun->name, s, p);

  for (i = 0; i < salign; i++)
    {
      if (src[i] != '?')
	{
	  ERR ("%s(align %d, %d) failed\n", fun->name, salign, len);
	  mtag_quoteat ("got head", src, len + 2 * A, i);
	  return;
	}
    }

  for (; i < salign + len; i++)
    {
      if (s[i - salign] != 'a')
	{
	  ERR ("%s(align %d, %d) failed\n", fun->name, salign, len);
	  mtag_quoteat ("got body", src, len + 2 * A, i);
	  return;
	}
    }

  for (; i < len + 2 * A; i++)
    {
      if (src[i] != '?')
	{
	  ERR ("%s(align %d, %d) failed\n", fun->name, salign, len);
	  mtag_quoteat ("got tail", src, len + 2 * A, i);
	  return;
	}
    }

  untag_buffer (src, len + 2 * A, 1);
}

int
main ()
{
  if (!mte_enabled ())
    return 0;

  sbuf = mte_mmap (LEN + 3 * A);
  int r = 0;
  for (int i = 0; funtab[i].name; i++)
    {
      err_count = 0;
      for (int s = 0; s < A; s += 16)
	{
	  int n;
	  for (n = 0; n < 200; n += 16)
	    {
	      test (funtab + i, s, n);
	    }
	  for (; n < LEN; n *= 2)
	    {
	      test (funtab + i, s, n);
	    }
	}
      printf ("%s %s\n", err_count ? "FAIL" : "PASS", funtab[i].name);
      if (err_count)
	r = -1;
    }
  return r;
}
#else
int
main ()
{
  return 0;
}
#endif
