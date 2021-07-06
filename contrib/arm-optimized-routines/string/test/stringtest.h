/*
 * Common string test code.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT
 */

#include <ctype.h>
#include <stdio.h>

/* Accounting errors for a test case.  */
static int err_count;
#define ERR_LIMIT 10
#define ERR(...) (err_count++, printf (__VA_ARGS__))

static inline void
quotechar (unsigned char c)
{
  if (isprint (c))
    putchar (c);
  else
    printf ("\\x%02x", c);
}

/* quoted print around at or the entire string if at < 0.  */
static void
quoteat (const char *prefix, const void *p, int len, int at)
{
  static const int CTXLEN = 15;
  int i;
  const char *pre = "\"";
  const char *post = "\"";
  const char *s = p;
  if (at > CTXLEN)
    {
      s += at - CTXLEN;
      len -= at - CTXLEN;
      pre = "...\"";
    }
  if (at >= 0 && len > 2 * CTXLEN + 1)
    {
      len = 2 * CTXLEN + 1;
      post = "\"...";
    }
  printf ("%4s: %s", prefix, pre);
  for (i = 0; i < len; i++)
    quotechar (s[i]);
  printf ("%s\n", post);
}

static inline void
quote (const char *prefix, const void *p, int len)
{
  quoteat (prefix, p, len, -1);
}
