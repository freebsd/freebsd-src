/*
 * memchr test.
 *
 * Copyright (c) 2020, Arm Limited.
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
  void *(*fun) (const void *s, int c, size_t n);
  int test_mte;
} funtab[] = {
  // clang-format off
  F(memrchr, 0)
#if __aarch64__
  F(__memrchr_aarch64, 1)
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
  return (void *) (((uintptr_t) p + ALIGN) & -ALIGN);
}

static void
test (const struct fun *fun, int align, size_t seekpos, size_t len,
      size_t maxlen)
{
  char *src = alignup (sbuf);
  char *s = src + align;
  char *f = seekpos < maxlen ? s + seekpos : NULL;
  int seekchar = 1;
  void *p;

  if (err_count >= ERR_LIMIT)
    return;
  if (len > LEN || seekpos > LEN || align > ALIGN)
    abort ();

  for (int i = 0; src + i < s; i++)
    src[i] = seekchar;
  for (int i = 0; i <= ALIGN; i++)
    s[len + i] = seekchar;
  for (int i = 0; i < len; i++)
    s[i] = 'a' + (i & 31);
  s[seekpos] = seekchar;
  s[((len ^ align) & 1) && seekpos < maxlen ? seekpos - 1 : len] = seekchar;

  s = tag_buffer (s, maxlen, fun->test_mte);
  p = fun->fun (s, seekchar, maxlen);
  untag_buffer (s, maxlen, fun->test_mte);
  p = untag_pointer (p);

  if (p != f)
    {
      ERR ("%s (%p, 0x%02x, %zu) returned %p, expected %p\n", fun->name, s,
	   seekchar, maxlen, p, f);
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
	    for (int sp = 0; sp < LEN; sp++)
	      test (funtab + i, a, sp, n, n);
	  }
      char *pass = funtab[i].test_mte && mte_enabled () ? "MTE PASS" : "PASS";
      printf ("%s %s\n", err_count ? "FAIL" : pass, funtab[i].name);
      if (err_count)
	r = -1;
    }
  return r;
}
