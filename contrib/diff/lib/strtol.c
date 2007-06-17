/* Convert string representation of a number into an integer value.

   Copyright (C) 1991, 1992, 1994, 1995, 1996, 1997, 1998, 1999, 2003
   Free Software Foundation, Inc.

   NOTE: The canonical source of this file is maintained with the GNU C
   Library.  Bugs can be reported to bug-glibc@gnu.org.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

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

#ifdef _LIBC
# define USE_NUMBER_GROUPING
#endif

#include <ctype.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif
#ifndef __set_errno
# define __set_errno(Val) errno = (Val)
#endif

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_NUMBER_GROUPING
# include "../locale/localeinfo.h"
#endif

/* Nonzero if we are defining `strtoul' or `strtoull', operating on
   unsigned integers.  */
#ifndef UNSIGNED
# define UNSIGNED 0
# define INT LONG int
#else
# define INT unsigned LONG int
#endif

/* Determine the name.  */
#ifdef USE_IN_EXTENDED_LOCALE_MODEL
# if UNSIGNED
#  ifdef USE_WIDE_CHAR
#   ifdef QUAD
#    define strtol __wcstoull_l
#   else
#    define strtol __wcstoul_l
#   endif
#  else
#   ifdef QUAD
#    define strtol __strtoull_l
#   else
#    define strtol __strtoul_l
#   endif
#  endif
# else
#  ifdef USE_WIDE_CHAR
#   ifdef QUAD
#    define strtol __wcstoll_l
#   else
#    define strtol __wcstol_l
#   endif
#  else
#   ifdef QUAD
#    define strtol __strtoll_l
#   else
#    define strtol __strtol_l
#   endif
#  endif
# endif
#else
# if UNSIGNED
#  ifdef USE_WIDE_CHAR
#   ifdef QUAD
#    define strtol wcstoull
#   else
#    define strtol wcstoul
#   endif
#  else
#   ifdef QUAD
#    define strtol strtoull
#   else
#    define strtol strtoul
#   endif
#  endif
# else
#  ifdef USE_WIDE_CHAR
#   ifdef QUAD
#    define strtol wcstoll
#   else
#    define strtol wcstol
#   endif
#  else
#   ifdef QUAD
#    define strtol strtoll
#   endif
#  endif
# endif
#endif

/* If QUAD is defined, we are defining `strtoll' or `strtoull',
   operating on `long long int's.  */
#ifdef QUAD
# define LONG long long
# define STRTOL_LONG_MIN LONG_LONG_MIN
# define STRTOL_LONG_MAX LONG_LONG_MAX
# define STRTOL_ULONG_MAX ULONG_LONG_MAX

/* The extra casts work around common compiler bugs,
   e.g. Cray C 5.0.3.0 when t == time_t.  */
# ifndef TYPE_SIGNED
#  define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
# endif
# ifndef TYPE_MINIMUM
#  define TYPE_MINIMUM(t) ((t) (TYPE_SIGNED (t) \
				? ~ (t) 0 << (sizeof (t) * CHAR_BIT - 1) \
				: (t) 0))
# endif
# ifndef TYPE_MAXIMUM
#  define TYPE_MAXIMUM(t) ((t) (~ (t) 0 - TYPE_MINIMUM (t)))
# endif

# ifndef ULONG_LONG_MAX
#  define ULONG_LONG_MAX TYPE_MAXIMUM (unsigned long long)
# endif
# ifndef LONG_LONG_MAX
#  define LONG_LONG_MAX TYPE_MAXIMUM (long long int)
# endif
# ifndef LONG_LONG_MIN
#  define LONG_LONG_MIN TYPE_MINIMUM (long long int)
# endif

# if __GNUC__ == 2 && __GNUC_MINOR__ < 7
   /* Work around gcc bug with using this constant.  */
   static const unsigned long long int maxquad = ULONG_LONG_MAX;
#  undef STRTOL_ULONG_MAX
#  define STRTOL_ULONG_MAX maxquad
# endif
#else
# define LONG long
# define STRTOL_LONG_MIN LONG_MIN
# define STRTOL_LONG_MAX LONG_MAX
# define STRTOL_ULONG_MAX ULONG_MAX
#endif


/* We use this code also for the extended locale handling where the
   function gets as an additional argument the locale which has to be
   used.  To access the values we have to redefine the _NL_CURRENT
   macro.  */
#ifdef USE_IN_EXTENDED_LOCALE_MODEL
# undef _NL_CURRENT
# define _NL_CURRENT(category, item) \
  (current->values[_NL_ITEM_INDEX (item)].string)
# define LOCALE_PARAM , loc
# define LOCALE_PARAM_PROTO , __locale_t loc
#else
# define LOCALE_PARAM
# define LOCALE_PARAM_PROTO
#endif

#if defined _LIBC || defined HAVE_WCHAR_H
# include <wchar.h>
#endif

#ifdef USE_WIDE_CHAR
# include <wctype.h>
# define L_(Ch) L##Ch
# define UCHAR_TYPE wint_t
# define STRING_TYPE wchar_t
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
#  define ISSPACE(Ch) __iswspace_l ((Ch), loc)
#  define ISALPHA(Ch) __iswalpha_l ((Ch), loc)
#  define TOUPPER(Ch) __towupper_l ((Ch), loc)
# else
#  define ISSPACE(Ch) iswspace (Ch)
#  define ISALPHA(Ch) iswalpha (Ch)
#  define TOUPPER(Ch) towupper (Ch)
# endif
#else
# if defined STDC_HEADERS || (!defined isascii && !defined HAVE_ISASCII)
#  define IN_CTYPE_DOMAIN(c) 1
# else
#  define IN_CTYPE_DOMAIN(c) isascii(c)
# endif
# define L_(Ch) Ch
# define UCHAR_TYPE unsigned char
# define STRING_TYPE char
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
#  define ISSPACE(Ch) __isspace_l ((Ch), loc)
#  define ISALPHA(Ch) __isalpha_l ((Ch), loc)
#  define TOUPPER(Ch) __toupper_l ((Ch), loc)
# else
#  define ISSPACE(Ch) (IN_CTYPE_DOMAIN (Ch) && isspace (Ch))
#  define ISALPHA(Ch) (IN_CTYPE_DOMAIN (Ch) && isalpha (Ch))
#  define TOUPPER(Ch) (IN_CTYPE_DOMAIN (Ch) ? toupper (Ch) : (Ch))
# endif
#endif

#define INTERNAL(X) INTERNAL1(X)
#define INTERNAL1(X) __##X##_internal
#define WEAKNAME(X) WEAKNAME1(X)

#ifdef USE_NUMBER_GROUPING
/* This file defines a function to check for correct grouping.  */
# include "grouping.h"
#endif



/* Convert NPTR to an `unsigned long int' or `long int' in base BASE.
   If BASE is 0 the base is determined by the presence of a leading
   zero, indicating octal or a leading "0x" or "0X", indicating hexadecimal.
   If BASE is < 2 or > 36, it is reset to 10.
   If ENDPTR is not NULL, a pointer to the character after the last
   one converted is stored in *ENDPTR.  */

INT
INTERNAL (strtol) (const STRING_TYPE *nptr, STRING_TYPE **endptr,
		   int base, int group LOCALE_PARAM_PROTO)
{
  int negative;
  register unsigned LONG int cutoff;
  register unsigned int cutlim;
  register unsigned LONG int i;
  register const STRING_TYPE *s;
  register UCHAR_TYPE c;
  const STRING_TYPE *save, *end;
  int overflow;

#ifdef USE_NUMBER_GROUPING
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
  struct locale_data *current = loc->__locales[LC_NUMERIC];
# endif
  /* The thousands character of the current locale.  */
  wchar_t thousands = L'\0';
  /* The numeric grouping specification of the current locale,
     in the format described in <locale.h>.  */
  const char *grouping;

  if (group)
    {
      grouping = _NL_CURRENT (LC_NUMERIC, GROUPING);
      if (*grouping <= 0 || *grouping == CHAR_MAX)
	grouping = NULL;
      else
	{
	  /* Figure out the thousands separator character.  */
# if defined _LIBC || defined _HAVE_BTOWC
	  thousands = __btowc (*_NL_CURRENT (LC_NUMERIC, THOUSANDS_SEP));
	  if (thousands == WEOF)
	    thousands = L'\0';
# endif
	  if (thousands == L'\0')
	    grouping = NULL;
	}
    }
  else
    grouping = NULL;
#endif

  if (base < 0 || base == 1 || base > 36)
    {
      __set_errno (EINVAL);
      return 0;
    }

  save = s = nptr;

  /* Skip white space.  */
  while (ISSPACE (*s))
    ++s;
  if (*s == L_('\0'))
    goto noconv;

  /* Check for a sign.  */
  if (*s == L_('-'))
    {
      negative = 1;
      ++s;
    }
  else if (*s == L_('+'))
    {
      negative = 0;
      ++s;
    }
  else
    negative = 0;

  /* Recognize number prefix and if BASE is zero, figure it out ourselves.  */
  if (*s == L_('0'))
    {
      if ((base == 0 || base == 16) && TOUPPER (s[1]) == L_('X'))
	{
	  s += 2;
	  base = 16;
	}
      else if (base == 0)
	base = 8;
    }
  else if (base == 0)
    base = 10;

  /* Save the pointer so we can check later if anything happened.  */
  save = s;

#ifdef USE_NUMBER_GROUPING
  if (group)
    {
      /* Find the end of the digit string and check its grouping.  */
      end = s;
      for (c = *end; c != L_('\0'); c = *++end)
	if ((wchar_t) c != thousands
	    && ((wchar_t) c < L_('0') || (wchar_t) c > L_('9'))
	    && (!ISALPHA (c) || (int) (TOUPPER (c) - L_('A') + 10) >= base))
	  break;
      if (*s == thousands)
	end = s;
      else
	end = correctly_grouped_prefix (s, end, thousands, grouping);
    }
  else
#endif
    end = NULL;

  cutoff = STRTOL_ULONG_MAX / (unsigned LONG int) base;
  cutlim = STRTOL_ULONG_MAX % (unsigned LONG int) base;

  overflow = 0;
  i = 0;
  for (c = *s; c != L_('\0'); c = *++s)
    {
      if (s == end)
	break;
      if (c >= L_('0') && c <= L_('9'))
	c -= L_('0');
      else if (ISALPHA (c))
	c = TOUPPER (c) - L_('A') + 10;
      else
	break;
      if ((int) c >= base)
	break;
      /* Check for overflow.  */
      if (i > cutoff || (i == cutoff && c > cutlim))
	overflow = 1;
      else
	{
	  i *= (unsigned LONG int) base;
	  i += c;
	}
    }

  /* Check if anything actually happened.  */
  if (s == save)
    goto noconv;

  /* Store in ENDPTR the address of one character
     past the last character we converted.  */
  if (endptr != NULL)
    *endptr = (STRING_TYPE *) s;

#if !UNSIGNED
  /* Check for a value that is within the range of
     `unsigned LONG int', but outside the range of `LONG int'.  */
  if (overflow == 0
      && i > (negative
	      ? -((unsigned LONG int) (STRTOL_LONG_MIN + 1)) + 1
	      : (unsigned LONG int) STRTOL_LONG_MAX))
    overflow = 1;
#endif

  if (overflow)
    {
      __set_errno (ERANGE);
#if UNSIGNED
      return STRTOL_ULONG_MAX;
#else
      return negative ? STRTOL_LONG_MIN : STRTOL_LONG_MAX;
#endif
    }

  /* Return the result of the appropriate sign.  */
  return negative ? -i : i;

noconv:
  /* We must handle a special case here: the base is 0 or 16 and the
     first two characters are '0' and 'x', but the rest are no
     hexadecimal digits.  This is no error case.  We return 0 and
     ENDPTR points to the `x`.  */
  if (endptr != NULL)
    {
      if (save - nptr >= 2 && TOUPPER (save[-1]) == L_('X')
	  && save[-2] == L_('0'))
	*endptr = (STRING_TYPE *) &save[-1];
      else
	/*  There was no number to convert.  */
	*endptr = (STRING_TYPE *) nptr;
    }

  return 0L;
}

/* External user entry point.  */


INT
#ifdef weak_function
weak_function
#endif
strtol (const STRING_TYPE *nptr, STRING_TYPE **endptr,
	int base LOCALE_PARAM_PROTO)
{
  return INTERNAL (strtol) (nptr, endptr, base, 0 LOCALE_PARAM);
}
