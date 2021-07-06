/*
 * strnlen test.
 *
 * Copyright (c) 2019-2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "mte.h"
#include "stringlib.h"
#include "stringtest.h"

#define F(x, mte) {#x, x, mte},

static const struct fun
{
  const char *name;
  size_t (*fun) (const char *s, size_t m);
  int test_mte;
} funtab[] = {
  // clang-format off
  F(strnlen, 0)
#if __aarch64__
  F(__strnlen_aarch64, 1)
# if __ARM_FEATURE_SVE
  F(__strnlen_aarch64_sve, 1)
# endif
#endif
  {0, 0, 0}
  // clang-format on
};
#undef F

#define ALIGN 32
#define LEN 512
static char *sbuf;

static void *
alignup (void *p)
{
  return (void *) (((uintptr_t) p + ALIGN - 1) & -ALIGN);
}

static void
test (const struct fun *fun, int align, size_t maxlen, size_t len)
{
  char *src = alignup (sbuf);
  char *s = src + align;
  size_t r;
  size_t e = maxlen < len ? maxlen : len;

  if (err_count >= ERR_LIMIT)
    return;
  if (len > LEN || align >= ALIGN)
    abort ();

  for (int i = 0; src + i < s; i++)
    src[i] = 0;
  for (int i = 1; i <= ALIGN; i++)
    s[len + i] = (len + align) & 1 ? 1 : 0;
  for (int i = 0; i < len; i++)
    s[i] = 'a' + (i & 31);
  s[len] = 0;
  if ((len + align) & 1)
    s[e + 1] = 0;

  size_t mte_len = maxlen < len + 1 ? maxlen : len + 1;
  s = tag_buffer (s, mte_len, fun->test_mte);
  r = fun->fun (s, maxlen);
  untag_buffer (s, mte_len, fun->test_mte);

  if (r != e)
    {
      ERR ("%s (%p, %zu) len %zu returned %zu, expected %zu\n",
	   fun->name, s, maxlen, len, r, e);
      quote ("input", s, len);
    }
}

int
main (void)
{
  sbuf = mte_mmap (LEN + 3 * ALIGN);
  int r = 0;
  for (int i = 0; funtab[i].name; i++)
    {
      err_count = 0;
      for (int a = 0; a < ALIGN; a++)
	for (int n = 0; n < LEN; n++)
	  {
	    for (int maxlen = 0; maxlen < LEN; maxlen++)
	      test (funtab + i, a, maxlen, n);
	    test (funtab + i, a, SIZE_MAX - a, n);
	  }
      char *pass = funtab[i].test_mte && mte_enabled () ? "MTE PASS" : "PASS";
      printf ("%s %s\n", err_count ? "FAIL" : pass, funtab[i].name);
      if (err_count)
	r = -1;
    }
  return r;
}
