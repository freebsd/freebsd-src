/*
 * strchrnul test.
 *
 * Copyright (c) 2019-2020, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
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
  char *(*fun) (const char *s, int c);
  int test_mte;
} funtab[] = {
  // clang-format off
  F(strchrnul, 0)
#if __aarch64__
  F(__strchrnul_aarch64, 0)
  F(__strchrnul_aarch64_mte, 1)
# if __ARM_FEATURE_SVE
  F(__strchrnul_aarch64_sve, 1)
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
test (const struct fun *fun, int align, int seekpos, int len)
{
  char *src = alignup (sbuf);
  char *s = src + align;
  char *f = seekpos != -1 ? s + seekpos : s + len;
  int seekchar = 0x1;
  void *p;

  if (err_count >= ERR_LIMIT)
    return;
  if (len > LEN || seekpos >= len || align >= ALIGN)
    abort ();

  for (int i = 0; src + i < s; i++)
    src[i] = (i + len) & 1 ? seekchar : 0;
  for (int i = 1; i <= ALIGN; i++)
    s[len + i] = (i + len) & 1 ? seekchar : 0;
  for (int i = 0; i < len; i++)
    s[i] = 'a' + (i & 31);
  if (seekpos != -1)
    s[seekpos] = seekchar;
  if (seekpos != -1 && (len + align) & 1)
    s[seekpos + 1] = seekchar;
  s[len] = '\0';

  int mte_len = seekpos != -1 ? seekpos + 1 : len + 1;
  s = tag_buffer (s, mte_len, fun->test_mte);
  p = fun->fun (s, seekchar);
  untag_buffer (s, mte_len, fun->test_mte);
  p = untag_pointer (p);

  if (p != f)
    {
      ERR ("%s (%p, 0x%02x) len %d returned %p, expected %p pos %d\n",
	   fun->name, s, seekchar, len, p, f, seekpos);
      quote ("input", s, len);
    }

  s = tag_buffer (s, len + 1, fun->test_mte);
  p = fun->fun (s, 0);
  untag_buffer (s, len + 1, fun->test_mte);

  if (p != s + len)
    {
      ERR ("%s (%p, 0x%02x) len %d returned %p, expected %p pos %d\n",
	   fun->name, s, 0, len, p, f, len);
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
	    for (int sp = 0; sp < n; sp++)
	      test (funtab + i, a, sp, n);
	    test (funtab + i, a, -1, n);
	  }

      char *pass = funtab[i].test_mte && mte_enabled () ? "MTE PASS" : "PASS";
      printf ("%s %s\n", err_count ? "FAIL" : pass, funtab[i].name);
      if (err_count)
	r = -1;
    }
  return r;
}
