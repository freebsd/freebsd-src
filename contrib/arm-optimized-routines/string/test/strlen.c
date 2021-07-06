/*
 * strlen test.
 *
 * Copyright (c) 2019-2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <limits.h>
#include "mte.h"
#include "stringlib.h"
#include "stringtest.h"

#define F(x, mte) {#x, x, mte},

static const struct fun
{
  const char *name;
  size_t (*fun) (const char *s);
  int test_mte;
} funtab[] = {
  // clang-format off
  F(strlen, 0)
#if __aarch64__
  F(__strlen_aarch64, 0)
  F(__strlen_aarch64_mte, 1)
# if __ARM_FEATURE_SVE
  F(__strlen_aarch64_sve, 1)
# endif
#elif __arm__
# if __ARM_ARCH >= 6 && __ARM_ARCH_ISA_THUMB == 2
  F(__strlen_armv6t2, 0)
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
test (const struct fun *fun, int align, int len)
{
  char *src = alignup (sbuf);
  char *s = src + align;
  size_t r;

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
  s[len] = '\0';

  s = tag_buffer (s, len + 1, fun->test_mte);
  r = fun->fun (s);
  untag_buffer (s, len + 1, fun->test_mte);

  if (r != len)
    {
      ERR ("%s (%p) returned %zu expected %d\n", fun->name, s, r, len);
      quote ("input", src, len);
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
	  test (funtab + i, a, n);

      char *pass = funtab[i].test_mte && mte_enabled () ? "MTE PASS" : "PASS";
      printf ("%s %s\n", err_count ? "FAIL" : pass, funtab[i].name);
      if (err_count)
	r = -1;
    }
  return r;
}
