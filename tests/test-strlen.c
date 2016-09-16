/* Test and measure strlen functions.
   Copyright (C) 1999, 2002, 2003, 2005 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#define TEST_MAIN
#include "test-string.h"

typedef size_t (*proto_t) (const char *);
size_t simple_strlen (const char *);
size_t builtin_strlen (const char *);

IMPL (simple_strlen, 0)
IMPL (builtin_strlen, 0)
IMPL (strlen, 1)

size_t
simple_strlen (const char *s)
{
  const char *p;

  for (p = s; *p; ++p);
  return p - s;
}

size_t
builtin_strlen (const char *p)
{
  return __builtin_strlen (p);
}

static void
do_one_test (impl_t *impl, const char *s, size_t exp_len)
{
  size_t len = CALL (impl, s);
  if (len != exp_len)
    {
      error (0, 0, "Wrong result in function %s %zd %zd", impl->name,
	     len, exp_len);
      ret = 1;
      return;
    }

  if (HP_TIMING_AVAIL)
    {
      hp_timing_t start __attribute ((unused));
      hp_timing_t stop __attribute ((unused));
      hp_timing_t best_time = ~ (hp_timing_t) 0;
      size_t i;

      for (i = 0; i < 32; ++i)
	{
	  HP_TIMING_NOW (start);
	  CALL (impl, s);
	  HP_TIMING_NOW (stop);
	  HP_TIMING_BEST (best_time, start, stop);
	}

      printf ("\t%zd", (size_t) best_time);
    }
}

static void
do_test (size_t align, size_t len, int max_char)
{
  size_t i;

  align &= 7;
  if (align + len >= page_size)
    return;

  for (i = 0; i < len; ++i)
    buf1[align + i] = 1 + 7 * i % max_char;
  buf1[align + len] = 0;

  if (HP_TIMING_AVAIL)
    printf ("Length %4zd, alignment %2zd:", len, align);

  FOR_EACH_IMPL (impl, 0)
    do_one_test (impl, (char *) (buf1 + align), len);

  if (HP_TIMING_AVAIL)
    putchar ('\n');
}

static void
do_random_tests (void)
{
  size_t i, j, n, align, len;
  unsigned char *p = buf1 + page_size - 512;

  for (n = 0; n < ITERATIONS; n++)
    {
      align = random () & 15;
      len = random () & 511;
      if (len + align > 510)
	len = 511 - align - (random () & 7);
      j = len + align + 64;
      if (j > 512)
	j = 512;

      for (i = 0; i < j; i++)
	{
	  if (i == len + align)
	    p[i] = 0;
	  else
	    {
	      p[i] = random () & 255;
	      if (i >= align && i < len + align && !p[i])
		p[i] = (random () & 127) + 1;
	    }
	}

      FOR_EACH_IMPL (impl, 1)
	if (CALL (impl, (char *) (p + align)) != len)
	  {
	    error (0, 0, "Iteration %zd - wrong result in function %s (%zd) %zd != %zd, p %p",
		   n, impl->name, align, CALL (impl, (char *) (p + align)),
		   len, p);
	    ret = 1;
	  }
    }
}

int
test_main (void)
{
  size_t i;

  test_init ();

  printf ("%20s", "");
  FOR_EACH_IMPL (impl, 0)
    printf ("\t%s", impl->name);
  putchar ('\n');

  for (i = 1; i < 8; ++i)
    do_test (0, i, 127);

  for (i = 1; i < 8; ++i)
    do_test (i, i, 127);

  for (i = 2; i <= 10; ++i)
    {
      do_test (0, 1 << i, 127);
      do_test (1, 1 << i, 127);
    }

  for (i = 1; i < 8; ++i)
    do_test (0, i, 255);

  for (i = 1; i < 8; ++i)
    do_test (i, i, 255);

  for (i = 2; i <= 10; ++i)
    {
      do_test (0, 1 << i, 255);
      do_test (1, 1 << i, 255);
    }

  do_random_tests ();
  return ret;
}

#include "test-skeleton.c"
