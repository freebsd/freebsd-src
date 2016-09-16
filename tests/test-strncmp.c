/* Test and measure strncmp functions.
   Copyright (C) 1999-2012 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Written by Jakub Jelinek <jakub@redhat.com>, 1999.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#define TEST_MAIN
#define TEST_NAME "strncmp"
#include "test-string.h"

typedef int (*proto_t) (const char *, const char *, size_t);
int simple_strncmp (const char *, const char *, size_t);
int stupid_strncmp (const char *, const char *, size_t);

IMPL (stupid_strncmp, 0)
IMPL (simple_strncmp, 0)
IMPL (strncmp, 1)

int
simple_strncmp (const char *s1, const char *s2, size_t n)
{
  int ret = 0;

  while (n-- && (ret = *(unsigned char *) s1 - * (unsigned char *) s2++) == 0
	 && *s1++);
  return ret;
}

int
stupid_strncmp (const char *s1, const char *s2, size_t n)
{
  size_t ns1 = strnlen (s1, n) + 1, ns2 = strnlen (s2, n) + 1;
  int ret = 0;

  n = ns1 < n ? ns1 : n;
  n = ns2 < n ? ns2 : n;
  while (n-- && (ret = *(unsigned char *) s1++ - * (unsigned char *) s2++) == 0);
  return ret;
}

static int
check_result (impl_t *impl, const char *s1, const char *s2, size_t n,
	     int exp_result)
{
  int result = CALL (impl, s1, s2, n);
  if ((exp_result == 0 && result != 0)
      || (exp_result < 0 && result >= 0)
      || (exp_result > 0 && result <= 0))
    {
      error (0, 0, "Wrong result in function %s %d %d", impl->name,
	     result, exp_result);
      ret = 1;
      return -1;
    }

  return 0;
}

static void
do_one_test (impl_t *impl, const char *s1, const char *s2, size_t n,
	     int exp_result)
{
  if (check_result (impl, s1, s2, n, exp_result) < 0)
    return;

  if (HP_TIMING_AVAIL)
    {
      hp_timing_t start __attribute ((unused));
      hp_timing_t stop __attribute ((unused));
      hp_timing_t best_time = ~ (hp_timing_t) 0;
      size_t i;

      for (i = 0; i < 32; ++i)
	{
	  HP_TIMING_NOW (start);
	  CALL (impl, s1, s2, n);
	  HP_TIMING_NOW (stop);
	  HP_TIMING_BEST (best_time, start, stop);
	}

      printf ("\t%zd", (size_t) best_time);
    }
}

static void
do_test_limit (size_t align1, size_t align2, size_t len, size_t n, int max_char,
	 int exp_result)
{
  size_t i, align_n;
  char *s1, *s2;

  if (n == 0)
    {
      s1 = (char*)(buf1 + page_size);
      s2 = (char*)(buf2 + page_size);
      if (HP_TIMING_AVAIL)
	printf ("Length %4zd/%4zd:", len, n);

      FOR_EACH_IMPL (impl, 0)
	do_one_test (impl, s1, s2, n, 0);

      if (HP_TIMING_AVAIL)
	putchar ('\n');

      return;
    }

  align1 &= 15;
  align2 &= 15;
  align_n = (page_size - n) & 15;

  s1 = (char*)(buf1 + page_size - n);
  s2 = (char*)(buf2 + page_size - n);

  if (align1 < align_n)
    s1 -= (align_n - align1);

  if (align2 < align_n)
    s2 -= (align_n - align2);

  for (i = 0; i < n; i++)
    s1[i] = s2[i] = 1 + 23 * i % max_char;

  if (len < n)
    {
      s1[len] = 0;
      s2[len] = 0;
      if (exp_result < 0)
	s2[len] = 32;
      else if (exp_result > 0)
	s1[len] = 64;
    }

  if (HP_TIMING_AVAIL)
    printf ("Length %4zd/%4zd, alignment %2zd/%2zd:", len, n, align1, align2);

  FOR_EACH_IMPL (impl, 0)
    do_one_test (impl, s1, s2, n, exp_result);

  if (HP_TIMING_AVAIL)
    putchar ('\n');
}

static void
do_test (size_t align1, size_t align2, size_t len, size_t n, int max_char,
	 int exp_result)
{
  size_t i;
  char *s1, *s2;

  if (n == 0)
    return;

  align1 &= 7;
  if (align1 + n + 1 >= page_size)
    return;

  align2 &= 7;
  if (align2 + n + 1 >= page_size)
    return;

  s1 = (char*)(buf1 + align1);
  s2 = (char*)(buf2 + align2);

  for (i = 0; i < n; i++)
    s1[i] = s2[i] = 1 + 23 * i % max_char;

  s1[n] = 24 + exp_result;
  s2[n] = 23;
  s1[len] = 0;
  s2[len] = 0;
  if (exp_result < 0)
    s2[len] = 32;
  else if (exp_result > 0)
    s1[len] = 64;
  if (len >= n)
    s2[n - 1] -= exp_result;

  if (HP_TIMING_AVAIL)
    printf ("Length %4zd/%4zd, alignment %2zd/%2zd:", len, n, align1, align2);

  FOR_EACH_IMPL (impl, 0)
    do_one_test (impl, (char*)s1, (char*)s2, n, exp_result);

  if (HP_TIMING_AVAIL)
    putchar ('\n');
}

static void
do_page_test (size_t offset1, size_t offset2, char *s2)
{
  char *s1;
  int exp_result;

  if (offset1 >= page_size || offset2 >= page_size)
    return;

  s1 = (char *) (buf1 + offset1);
  s2 += offset2;

  exp_result= *s1;

  FOR_EACH_IMPL (impl, 0)
    {
      check_result (impl, s1, s2, page_size, -exp_result);
      check_result (impl, s2, s1, page_size, exp_result);
    }
}

static void
do_random_tests (void)
{
  size_t i, j, n, align1, align2, pos, len1, len2, size;
  int result;
  long r;
  unsigned char *p1 = buf1 + page_size - 512;
  unsigned char *p2 = buf2 + page_size - 512;

  for (n = 0; n < ITERATIONS; n++)
    {
      align1 = random () & 31;
      if (random () & 1)
	align2 = random () & 31;
      else
	align2 = align1 + (random () & 24);
      pos = random () & 511;
      size = random () & 511;
      j = align1 > align2 ? align1 : align2;
      if (pos + j >= 511)
	pos = 510 - j - (random () & 7);
      len1 = random () & 511;
      if (pos >= len1 && (random () & 1))
	len1 = pos + (random () & 7);
      if (len1 + j >= 512)
	len1 = 511 - j - (random () & 7);
      if (pos >= len1)
	len2 = len1;
      else
	len2 = len1 + (len1 != 511 - j ? random () % (511 - j - len1) : 0);
      j = (pos > len2 ? pos : len2) + align1 + 64;
      if (j > 512)
	j = 512;
      for (i = 0; i < j; ++i)
	{
	  p1[i] = random () & 255;
	  if (i < len1 + align1 && !p1[i])
	    {
	      p1[i] = random () & 255;
	      if (!p1[i])
		p1[i] = 1 + (random () & 127);
	    }
	}
      for (i = 0; i < j; ++i)
	{
	  p2[i] = random () & 255;
	  if (i < len2 + align2 && !p2[i])
	    {
	      p2[i] = random () & 255;
	      if (!p2[i])
		p2[i] = 1 + (random () & 127);
	    }
	}

      result = 0;
      memcpy (p2 + align2, p1 + align1, pos);
      if (pos < len1)
	{
	  if (p2[align2 + pos] == p1[align1 + pos])
	    {
	      p2[align2 + pos] = random () & 255;
	      if (p2[align2 + pos] == p1[align1 + pos])
		p2[align2 + pos] = p1[align1 + pos] + 3 + (random () & 127);
	    }

	  if (pos < size)
	    {
	      if (p1[align1 + pos] < p2[align2 + pos])
		result = -1;
	      else
		result = 1;
	    }
	}
      p1[len1 + align1] = 0;
      p2[len2 + align2] = 0;

      FOR_EACH_IMPL (impl, 1)
	{
	  r = CALL (impl, (char*)(p1 + align1), (char*)(p2 + align2), size);
	  /* Test whether on 64-bit architectures where ABI requires
	     callee to promote has the promotion been done.  */
	  asm ("" : "=g" (r) : "0" (r));
	  if ((r == 0 && result)
	      || (r < 0 && result >= 0)
	      || (r > 0 && result <= 0))
	    {
	      error (0, 0, "Iteration %zd - wrong result in function %s (%zd, %zd, %zd, %zd, %zd, %zd) %ld != %d, p1 %p p2 %p",
		     n, impl->name, align1, align2, len1, len2, pos, size, r, result, p1, p2);
	      ret = 1;
	    }
	}
    }
}

static void
check1 (void)
{
  char *s1 = (char *)(buf1 + 0xb2c);
  char *s2 = (char *)(buf1 + 0xfd8);
  size_t i;
  int exp_result;

  strcpy(s1, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrs");
  strcpy(s2, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkLMNOPQRSTUV");

  for (i = 0; i < 80; i++)
    {
      exp_result = simple_strncmp (s1, s2, i);
      FOR_EACH_IMPL (impl, 0)
	 check_result (impl, s1, s2, i, exp_result);
    }
}

static void
check2 (void)
{
  size_t i;
  char *s1, *s2;

  s1 = (char *) buf1;
  for (i = 0; i < page_size - 1; i++)
    s1[i] = 23;
  s1[i] = 0;

  s2 = strdup (s1);

  for (i = 0; i < 64; ++i)
    do_page_test (3990 + i, 2635, s2);

  free (s2);
}

int
test_main (void)
{
  size_t i;

  test_init ();

  check1 ();
  check2 ();

  printf ("%23s", "");
  FOR_EACH_IMPL (impl, 0)
    printf ("\t%s", impl->name);
  putchar ('\n');

  for (i =0; i < 16; ++i)
    {
      do_test (0, 0, 8, i, 127, 0);
      do_test (0, 0, 8, i, 127, -1);
      do_test (0, 0, 8, i, 127, 1);
      do_test (i, i, 8, i, 127, 0);
      do_test (i, i, 8, i, 127, 1);
      do_test (i, i, 8, i, 127, -1);
      do_test (i, 2 * i, 8, i, 127, 0);
      do_test (2 * i, i, 8, i, 127, 1);
      do_test (i, 3 * i, 8, i, 127, -1);
      do_test (0, 0, 8, i, 255, 0);
      do_test (0, 0, 8, i, 255, -1);
      do_test (0, 0, 8, i, 255, 1);
      do_test (i, i, 8, i, 255, 0);
      do_test (i, i, 8, i, 255, 1);
      do_test (i, i, 8, i, 255, -1);
      do_test (i, 2 * i, 8, i, 255, 0);
      do_test (2 * i, i, 8, i, 255, 1);
      do_test (i, 3 * i, 8, i, 255, -1);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (0, 0, 8 << i, 16 << i, 127, 0);
      do_test (0, 0, 8 << i, 16 << i, 127, 1);
      do_test (0, 0, 8 << i, 16 << i, 127, -1);
      do_test (0, 0, 8 << i, 16 << i, 255, 0);
      do_test (0, 0, 8 << i, 16 << i, 255, 1);
      do_test (0, 0, 8 << i, 16 << i, 255, -1);
      do_test (8 - i, 2 * i, 8 << i, 16 << i, 127, 0);
      do_test (8 - i, 2 * i, 8 << i, 16 << i, 127, 1);
      do_test (2 * i, i, 8 << i, 16 << i, 255, 0);
      do_test (2 * i, i, 8 << i, 16 << i, 255, 1);
    }

  do_test_limit (0, 0, 0, 0, 127, 0);
  do_test_limit (4, 0, 21, 20, 127, 0);
  do_test_limit (0, 4, 21, 20, 127, 0);
  do_test_limit (8, 0, 25, 24, 127, 0);
  do_test_limit (0, 8, 25, 24, 127, 0);

  for (i = 0; i < 8; ++i)
    {
      do_test_limit (0, 0, 17 - i, 16 - i, 127, 0);
      do_test_limit (0, 0, 17 - i, 16 - i, 255, 0);
      do_test_limit (0, 0, 15 - i, 16 - i, 127, 0);
      do_test_limit (0, 0, 15 - i, 16 - i, 127, 1);
      do_test_limit (0, 0, 15 - i, 16 - i, 127, -1);
      do_test_limit (0, 0, 15 - i, 16 - i, 255, 0);
      do_test_limit (0, 0, 15 - i, 16 - i, 255, 1);
      do_test_limit (0, 0, 15 - i, 16 - i, 255, -1);
    }

  do_random_tests ();
  return ret;
}

#include "test-skeleton.c"
