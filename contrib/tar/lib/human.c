/* human.c -- print human readable file size
   Copyright (C) 1996, 1997, 1998, 1999, 2000 Free Software Foundation, Inc.

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

/* Originally contributed by lm@sgi.com;
   --si, output block size selection, and large file support
   added by eggert@twinsun.com.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#ifndef CHAR_BIT
# define CHAR_BIT 8
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifndef HAVE_DECL_GETENV
"this configure-time declaration test was not run"
#endif
#if !HAVE_DECL_GETENV
char *getenv ();
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#endif

#include <argmatch.h>
#include <error.h>
#include <xstrtol.h>

#include "human.h"

static const char suffixes[] =
{
  0,	/* not used */
  'k',	/* kilo */
  'M',	/* Mega */
  'G',	/* Giga */
  'T',	/* Tera */
  'P',	/* Peta */
  'E',	/* Exa */
  'Z',	/* Zetta */
  'Y'	/* Yotta */
};

/* If INEXACT_STYLE is not human_round_to_even, and if easily
   possible, adjust VALUE according to the style.  */
static double
adjust_value (enum human_inexact_style inexact_style, double value)
{
  /* Do not use the floor or ceil functions, as that would mean
     linking with the standard math library, which is a porting pain.
     So leave the value alone if it is too large to easily round.  */
  if (inexact_style != human_round_to_even && value < (uintmax_t) -1)
    {
      uintmax_t u = value;
      value = u + (inexact_style == human_ceiling && u != value);
    }

  return value;
}

/* Like human_readable_inexact, except always round to even.  */
char *
human_readable (uintmax_t n, char *buf,
		int from_block_size, int output_block_size)
{
  return human_readable_inexact (n, buf, from_block_size, output_block_size,
				 human_round_to_even);
}

/* Convert N to a human readable format in BUF.

   N is expressed in units of FROM_BLOCK_SIZE.  FROM_BLOCK_SIZE must
   be nonnegative.

   OUTPUT_BLOCK_SIZE must be nonzero.  If it is positive, use units of
   OUTPUT_BLOCK_SIZE in the output number.

   Use INEXACT_STYLE to determine whether to take the ceiling or floor
   of any result that cannot be expressed exactly.

   If OUTPUT_BLOCK_SIZE is negative, use a format like "127k" if
   possible, using powers of -OUTPUT_BLOCK_SIZE; otherwise, use
   ordinary decimal format.  Normally -OUTPUT_BLOCK_SIZE is either
   1000 or 1024; it must be at least 2.  Most people visually process
   strings of 3-4 digits effectively, but longer strings of digits are
   more prone to misinterpretation.  Hence, converting to an
   abbreviated form usually improves readability.  Use a suffix
   indicating which power is being used.  For example, assuming
   -OUTPUT_BLOCK_SIZE is 1024, 8500 would be converted to 8.3k,
   133456345 to 127M, 56990456345 to 53G, and so on.  Numbers smaller
   than -OUTPUT_BLOCK_SIZE aren't modified.  */

char *
human_readable_inexact (uintmax_t n, char *buf,
			int from_block_size, int output_block_size,
			enum human_inexact_style inexact_style)
{
  uintmax_t amt;
  int base;
  int to_block_size;
  int tenths = 0;
  int power;
  char *p;

  /* 0 means adjusted N == AMT.TENTHS;
     1 means AMT.TENTHS < adjusted N < AMT.TENTHS + 0.05;
     2 means adjusted N == AMT.TENTHS + 0.05;
     3 means AMT.TENTHS + 0.05 < adjusted N < AMT.TENTHS + 0.1.  */
  int rounding = 0;

  if (output_block_size < 0)
    {
      base = -output_block_size;
      to_block_size = 1;
    }
  else
    {
      base = 0;
      to_block_size = output_block_size;
    }

  p = buf + LONGEST_HUMAN_READABLE;
  *p = '\0';

#ifdef lint
  /* Suppress `used before initialized' warning.  */
  power = 0;
#endif

  /* Adjust AMT out of FROM_BLOCK_SIZE units and into TO_BLOCK_SIZE units.  */

  {
    int multiplier;
    int divisor;
    int r2;
    int r10;
    if (to_block_size <= from_block_size
	? (from_block_size % to_block_size != 0
	   || (multiplier = from_block_size / to_block_size,
	       (amt = n * multiplier) / multiplier != n))
	: (from_block_size == 0
	   || to_block_size % from_block_size != 0
	   || (divisor = to_block_size / from_block_size,
	       r10 = (n % divisor) * 10,
	       r2 = (r10 % divisor) * 2,
	       amt = n / divisor,
	       tenths = r10 / divisor,
	       rounding = r2 < divisor ? 0 < r2 : 2 + (divisor < r2),
	       0)))
      {
	/* Either the result cannot be computed easily using uintmax_t,
	   or from_block_size is zero.  Fall back on floating point.
	   FIXME: This can yield answers that are slightly off.  */

	double damt = n * (from_block_size / (double) to_block_size);

	if (! base)
	  sprintf (buf, "%.0f", adjust_value (inexact_style, damt));
	else
	  {
	    double e = 1;
	    power = 0;

	    do
	      {
		e *= base;
		power++;
	      }
	    while (e * base <= damt && power < sizeof suffixes - 1);

	    damt /= e;

	    sprintf (buf, "%.1f%c", adjust_value (inexact_style, damt),
		     suffixes[power]);
	    if (4 < strlen (buf))
	      sprintf (buf, "%.0f%c",
		       adjust_value (inexact_style, damt * 10) / 10,
		       suffixes[power]);
	  }

	return buf;
      }
  }

  /* Use power of BASE notation if adjusted AMT is large enough.  */

  if (base && base <= amt)
    {
      power = 0;

      do
	{
	  int r10 = (amt % base) * 10 + tenths;
	  int r2 = (r10 % base) * 2 + (rounding >> 1);
	  amt /= base;
	  tenths = r10 / base;
	  rounding = (r2 < base
		      ? 0 < r2 + rounding
		      : 2 + (base < r2 + rounding));
	  power++;
	}
      while (base <= amt && power < sizeof suffixes - 1);

      *--p = suffixes[power];

      if (amt < 10)
	{
	  if (2 * (1 - (int) inexact_style)
	      < rounding + (tenths & (inexact_style == human_round_to_even)))
	    {
	      tenths++;
	      rounding = 0;

	      if (tenths == 10)
		{
		  amt++;
		  tenths = 0;
		}
	    }

	  if (amt < 10)
	    {
	      *--p = '0' + tenths;
	      *--p = '.';
	      tenths = rounding = 0;
	    }
	}
    }

  if (inexact_style == human_ceiling
      ? 0 < tenths + rounding
      : inexact_style == human_round_to_even
      ? 5 < tenths + (2 < rounding + (amt & 1))
      : /* inexact_style == human_floor */ 0)
    {
      amt++;

      if (amt == base && power < sizeof suffixes - 1)
	{
	  *p = suffixes[power + 1];
	  *--p = '0';
	  *--p = '.';
	  amt = 1;
	}
    }

  do
    *--p = '0' + (int) (amt % 10);
  while ((amt /= 10) != 0);

  return p;
}


/* The default block size used for output.  This number may change in
   the future as disks get larger.  */
#ifndef DEFAULT_BLOCK_SIZE
# define DEFAULT_BLOCK_SIZE 1024
#endif

static char const *const block_size_args[] = { "human-readable", "si", 0 };
static int const block_size_types[] = { -1024, -1000 };

static int
default_block_size (void)
{
  return getenv ("POSIXLY_CORRECT") ? 512 : DEFAULT_BLOCK_SIZE;
}

static strtol_error
humblock (char const *spec, int *block_size)
{
  int i;

  if (! spec && ! (spec = getenv ("BLOCK_SIZE")))
    *block_size = default_block_size ();
  else if (0 <= (i = ARGMATCH (spec, block_size_args, block_size_types)))
    *block_size = block_size_types[i];
  else
    {
      char *ptr;
      unsigned long val;
      strtol_error e = xstrtoul (spec, &ptr, 0, &val, "eEgGkKmMpPtTyYzZ0");
      if (e != LONGINT_OK)
	return e;
      if (*ptr)
	return LONGINT_INVALID_SUFFIX_CHAR;
      if ((int) val < 0 || val != (int) val)
	return LONGINT_OVERFLOW;
      *block_size = (int) val;
    }

  return LONGINT_OK;
}

void
human_block_size (char const *spec, int report_errors, int *block_size)
{
  strtol_error e = humblock (spec, block_size);
  if (*block_size == 0)
    {
      *block_size = default_block_size ();
      e = LONGINT_INVALID;
    }
  if (e != LONGINT_OK && report_errors)
    STRTOL_FATAL_ERROR (spec, _("block size"), e);
}
