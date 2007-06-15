/* strcasecmp.c -- case insensitive string comparator
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef LENGTH_LIMIT
# define STRXCASECMP_FUNCTION strncasecmp
# define STRXCASECMP_DECLARE_N , size_t n
# define LENGTH_LIMIT_EXPR(Expr) Expr
#else
# define STRXCASECMP_FUNCTION strcasecmp
# define STRXCASECMP_DECLARE_N /* empty */
# define LENGTH_LIMIT_EXPR(Expr) 0
#endif

#include <stddef.h>
#include <ctype.h>

#define TOLOWER(Ch) (isupper (Ch) ? tolower (Ch) : (Ch))

/* Compare {{no more than N characters of }}strings S1 and S2,
   ignoring case, returning less than, equal to or
   greater than zero if S1 is lexicographically less
   than, equal to or greater than S2.  */

int
STRXCASECMP_FUNCTION (const char *s1, const char *s2 STRXCASECMP_DECLARE_N)
{
  register const unsigned char *p1 = (const unsigned char *) s1;
  register const unsigned char *p2 = (const unsigned char *) s2;
  unsigned char c1, c2;

  if (p1 == p2 || LENGTH_LIMIT_EXPR (n == 0))
    return 0;

  do
    {
      c1 = TOLOWER (*p1);
      c2 = TOLOWER (*p2);

      if (LENGTH_LIMIT_EXPR (--n == 0) || c1 == '\0')
	break;

      ++p1;
      ++p2;
    }
  while (c1 == c2);

  return c1 - c2;
}
