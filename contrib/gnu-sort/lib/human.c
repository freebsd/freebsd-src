/* human.c -- print human readable file size

   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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

/* Written by Paul Eggert and Larry McVoy.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "human.h"

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif
#ifndef UINTMAX_MAX
# define UINTMAX_MAX ((uintmax_t) -1)
#endif

#if HAVE_LOCALE_H && HAVE_LOCALECONV
# include <locale.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)

#include <argmatch.h>
#include <error.h>
#include <xstrtol.h>

/* The maximum length of a suffix like "KiB".  */
#define HUMAN_READABLE_SUFFIX_LENGTH_MAX 3

static const char power_letter[] =
{
  0,	/* not used */
  'K',	/* kibi ('k' for kilo is a special case) */
  'M',	/* mega or mebi */
  'G',	/* giga or gibi */
  'T',	/* tera or tebi */
  'P',	/* peta or pebi */
  'E',	/* exa or exbi */
  'Z',	/* zetta or 2**70 */
  'Y'	/* yotta or 2**80 */
};


/* If INEXACT_STYLE is not human_round_to_nearest, and if easily
   possible, adjust VALUE according to the style.  */

static long double
adjust_value (int inexact_style, long double value)
{
  /* Do not use the floorl or ceill functions, as that would mean
     checking for their presence and possibly linking with the
     standard math library, which is a porting pain.  So leave the
     value alone if it is too large to easily round.  */
  if (inexact_style != human_round_to_nearest && value < UINTMAX_MAX)
    {
      uintmax_t u = value;
      value = u + (inexact_style == human_ceiling && u != value);
    }

  return value;
}

/* Group the digits of NUMBER according to the grouping rules of the
   current locale.  NUMBER contains NUMBERLEN digits.  Modify the
   bytes pointed to by NUMBER in place, subtracting 1 from NUMBER for
   each byte inserted.  Return the starting address of the modified
   number.

   To group the digits, use GROUPING and THOUSANDS_SEP as in `struct
   lconv' from <locale.h>.  */

static char *
group_number (char *number, size_t numberlen,
	      char const *grouping, char const *thousands_sep)
{
  register char *d;
  size_t grouplen = SIZE_MAX;
  size_t thousands_seplen = strlen (thousands_sep);
  size_t i = numberlen;

  /* The maximum possible value for NUMBERLEN is the number of digits
     in the square of the largest uintmax_t, so double the size of
     uintmax_t before converting to a bound.  302 / 1000 is ceil
     (log10 (2.0)).  Add 1 for integer division truncation.  */
  char buf[2 * sizeof (uintmax_t) * CHAR_BIT * 302 / 1000 + 1];

  memcpy (buf, number, numberlen);
  d = number + numberlen;

  for (;;)
    {
      unsigned char g = *grouping;

      if (g)
	{
	  grouplen = g < CHAR_MAX ? g : i;
	  grouping++;
	}

      if (i < grouplen)
	grouplen = i;

      d -= grouplen;
      i -= grouplen;
      memcpy (d, buf + i, grouplen);
      if (i == 0)
	return d;

      d -= thousands_seplen;
      memcpy (d, thousands_sep, thousands_seplen);
    }
}

/* Convert N to a human readable format in BUF, using the options OPTS.

   N is expressed in units of FROM_BLOCK_SIZE.  FROM_BLOCK_SIZE must
   be nonnegative.

   Use units of TO_BLOCK_SIZE in the output number.  TO_BLOCK_SIZE
   must be positive.

   Use (OPTS & (human_round_to_nearest | human_floor | human_ceiling))
   to determine whether to take the ceiling or floor of any result
   that cannot be expressed exactly.

   If (OPTS & human_group_digits), group the thousands digits
   according to the locale, e.g., `1,000,000' in an American English
   locale.

   If (OPTS & human_autoscale), deduce the output block size
   automatically; TO_BLOCK_SIZE must be 1 but it has no effect on the
   output.  Use powers of 1024 if (OPTS & human_base_1024), and powers
   of 1000 otherwise.  For example, assuming powers of 1024, 8500
   would be converted to 8.3, 133456345 to 127, 56990456345 to 53, and
   so on.  Numbers smaller than the power aren't modified.
   human_autoscale is normally used together with human_SI.

   If (OPTS & human_SI), append an SI prefix indicating which power is
   being used.  If in addition (OPTS & human_B), append "B" (if base
   1000) or "iB" (if base 1024) to the SI prefix.  When ((OPTS &
   human_SI) && ! (OPTS & human_autoscale)), TO_BLOCK_SIZE must be a
   power of 1024 or of 1000, depending on (OPTS &
   human_base_1024).  */

char *
human_readable (uintmax_t n, char *buf, int opts,
		uintmax_t from_block_size, uintmax_t to_block_size)
{
  int inexact_style =
    opts & (human_round_to_nearest | human_floor | human_ceiling);
  unsigned int base = opts & human_base_1024 ? 1024 : 1000;
  uintmax_t amt;
  int tenths;
  int exponent = -1;
  int exponent_max = sizeof power_letter - 1;
  char *p;
  char *psuffix;
  char const *integerlim;

  /* 0 means adjusted N == AMT.TENTHS;
     1 means AMT.TENTHS < adjusted N < AMT.TENTHS + 0.05;
     2 means adjusted N == AMT.TENTHS + 0.05;
     3 means AMT.TENTHS + 0.05 < adjusted N < AMT.TENTHS + 0.1.  */
  int rounding;

  char const *decimal_point = ".";
  size_t decimal_pointlen = 1;
  char const *grouping = "";
  char const *thousands_sep = "";
#if HAVE_LOCALE_H && HAVE_LOCALECONV
  struct lconv const *l = localeconv ();
  size_t pointlen = strlen (l->decimal_point);
  if (0 < pointlen && pointlen <= MB_LEN_MAX)
    {
      decimal_point = l->decimal_point;
      decimal_pointlen = pointlen;
    }
  grouping = l->grouping;
  if (strlen (l->thousands_sep) <= MB_LEN_MAX)
    thousands_sep = l->thousands_sep;
#endif

  psuffix = buf + LONGEST_HUMAN_READABLE - HUMAN_READABLE_SUFFIX_LENGTH_MAX;
  p = psuffix;

  /* Adjust AMT out of FROM_BLOCK_SIZE units and into TO_BLOCK_SIZE
     units.  If this can be done exactly with integer arithmetic, do
     not use floating point operations.  */
  if (to_block_size <= from_block_size)
    {
      if (from_block_size % to_block_size == 0)
	{
	  uintmax_t multiplier = from_block_size / to_block_size;
	  amt = n * multiplier;
	  if (amt / multiplier == n)
	    {
	      tenths = 0;
	      rounding = 0;
	      goto use_integer_arithmetic;
	    }
	}
    }
  else if (from_block_size != 0 && to_block_size % from_block_size == 0)
    {
      uintmax_t divisor = to_block_size / from_block_size;
      uintmax_t r10 = (n % divisor) * 10;
      uintmax_t r2 = (r10 % divisor) * 2;
      amt = n / divisor;
      tenths = r10 / divisor;
      rounding = r2 < divisor ? 0 < r2 : 2 + (divisor < r2);
      goto use_integer_arithmetic;
    }

  {
    /* Either the result cannot be computed easily using uintmax_t,
       or from_block_size is zero.  Fall back on floating point.
       FIXME: This can yield answers that are slightly off.  */

    long double dto_block_size = to_block_size;
    long double damt = n * (from_block_size / dto_block_size);
    size_t buflen;
    size_t nonintegerlen;

    if (! (opts & human_autoscale))
      {
	sprintf (buf, "%.0Lf", adjust_value (inexact_style, damt));
	buflen = strlen (buf);
	nonintegerlen = 0;
      }
    else
      {
	long double e = 1;
	exponent = 0;

	do
	  {
	    e *= base;
	    exponent++;
	  }
	while (e * base <= damt && exponent < exponent_max);

	damt /= e;

	sprintf (buf, "%.1Lf", adjust_value (inexact_style, damt));
	buflen = strlen (buf);
	nonintegerlen = decimal_pointlen + 1;

	if (1 + nonintegerlen + ! (opts & human_base_1024) < buflen
	    || ((opts & human_suppress_point_zero)
		&& buf[buflen - 1] == '0'))
	  {
	    sprintf (buf, "%.0Lf",
		     adjust_value (inexact_style, damt * 10) / 10);
	    buflen = strlen (buf);
	    nonintegerlen = 0;
	  }
      }

    p = psuffix - buflen;
    memmove (p, buf, buflen);
    integerlim = p + buflen - nonintegerlen;
  }
  goto do_grouping;

 use_integer_arithmetic:
  {
    /* The computation can be done exactly, with integer arithmetic.

       Use power of BASE notation if requested and if adjusted AMT is
       large enough.  */

    if (opts & human_autoscale)
      {
	exponent = 0;

	if (base <= amt)
	  {
	    do
	      {
		unsigned int r10 = (amt % base) * 10 + tenths;
		unsigned int r2 = (r10 % base) * 2 + (rounding >> 1);
		amt /= base;
		tenths = r10 / base;
		rounding = (r2 < base
			    ? (r2 + rounding) != 0
			    : 2 + (base < r2 + rounding));
		exponent++;
	      }
	    while (base <= amt && exponent < exponent_max);

	    if (amt < 10)
	      {
		if (inexact_style == human_round_to_nearest
		    ? 2 < rounding + (tenths & 1)
		    : inexact_style == human_ceiling && 0 < rounding)
		  {
		    tenths++;
		    rounding = 0;

		    if (tenths == 10)
		      {
			amt++;
			tenths = 0;
		      }
		  }

		if (amt < 10
		    && (tenths || ! (opts & human_suppress_point_zero)))
		  {
		    *--p = '0' + tenths;
		    p -= decimal_pointlen;
		    memcpy (p, decimal_point, decimal_pointlen);
		    tenths = rounding = 0;
		  }
	      }
	  }
      }

    if (inexact_style == human_round_to_nearest
	? 5 < tenths + (0 < rounding + (amt & 1))
	: inexact_style == human_ceiling && 0 < tenths + rounding)
      {
	amt++;

	if ((opts & human_autoscale)
	    && amt == base && exponent < exponent_max)
	  {
	    exponent++;
	    if (! (opts & human_suppress_point_zero))
	      {
		*--p = '0';
		p -= decimal_pointlen;
		memcpy (p, decimal_point, decimal_pointlen);
	      }
	    amt = 1;
	  }
      }

    integerlim = p;

    do
      {
	int digit = amt % 10;
	*--p = digit + '0';
      }
    while ((amt /= 10) != 0);
  }

 do_grouping:
  if (opts & human_group_digits)
    p = group_number (p, integerlim - p, grouping, thousands_sep);

  if (opts & human_SI)
    {
      if (exponent < 0)
	{
	  uintmax_t power;
	  exponent = 0;
	  for (power = 1; power < to_block_size; power *= base)
	    if (++exponent == exponent_max)
	      break;
	}

      if (exponent)
	*psuffix++ = (! (opts & human_base_1024) && exponent == 1
		      ? 'k'
		      : power_letter[exponent]);

      if (opts & human_B)
	{
	  if ((opts & human_base_1024) && exponent)
	    *psuffix++ = 'i';
	  *psuffix++ = 'B';
	}
    }

  *psuffix = '\0';

  return p;
}


/* The default block size used for output.  This number may change in
   the future as disks get larger.  */
#ifndef DEFAULT_BLOCK_SIZE
# define DEFAULT_BLOCK_SIZE 1024
#endif

static char const *const block_size_args[] = { "human-readable", "si", 0 };
static int const block_size_opts[] =
  {
    human_autoscale + human_SI + human_base_1024,
    human_autoscale + human_SI
  };

static uintmax_t
default_block_size (void)
{
  return getenv ("POSIXLY_CORRECT") ? 512 : DEFAULT_BLOCK_SIZE;
}

static strtol_error
humblock (char const *spec, uintmax_t *block_size, int *options)
{
  int i;
  int opts = 0;

  if (! spec
      && ! (spec = getenv ("BLOCK_SIZE"))
      && ! (spec = getenv ("BLOCKSIZE")))
    *block_size = default_block_size ();
  else
    {
      if (*spec == '\'')
	{
	  opts |= human_group_digits;
	  spec++;
	}

      if (0 <= (i = ARGMATCH (spec, block_size_args, block_size_opts)))
	{
	  opts |= block_size_opts[i];
	  *block_size = 1;
	}
      else
	{
	  char *ptr;
	  strtol_error e = xstrtoumax (spec, &ptr, 0, block_size,
				       "eEgGkKmMpPtTyYzZ0");
	  if (e != LONGINT_OK)
	    return e;
	  for (; ! ('0' <= *spec && *spec <= '9'); spec++)
	    if (spec == ptr)
	      {
		opts |= human_SI;
		if (ptr[-1] == 'B')
		  opts |= human_B;
		if (ptr[-1] != 'B' || ptr[-2] == 'i')
		  opts |= human_base_1024;
		break;
	      }
	}
    }

  *options = opts;
  return LONGINT_OK;
}

int
human_options (char const *spec, bool report_errors, uintmax_t *block_size)
{
  int opts;
  strtol_error e = humblock (spec, block_size, &opts);
  if (*block_size == 0)
    {
      *block_size = default_block_size ();
      e = LONGINT_INVALID;
    }
  if (e != LONGINT_OK && report_errors)
    STRTOL_FATAL_ERROR (spec, _("block size"), e);
  return opts;
}
