/* Copyright (C) 1991,1992,1993,1996,1997,1998,1999,2000,2001,2002,2003,2004
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

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* Enable GNU extensions in fnmatch.h.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE	1
#endif

#ifdef __GNUC__
# define alloca __builtin_alloca
# define HAVE_ALLOCA 1
#else
# if defined HAVE_ALLOCA_H || defined _LIBC
#  include <alloca.h>
# else
#  ifdef _AIX
 #  pragma alloca
#  else
#   ifndef alloca
char *alloca ();
#   endif
#  endif
# endif
#endif

#if ! defined __builtin_expect && __GNUC__ < 3
# define __builtin_expect(expr, expected) (expr)
#endif

#include <fnmatch.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define WIDE_CHAR_SUPPORT (HAVE_WCTYPE_H && HAVE_WCHAR_H && HAVE_BTOWC)

/* For platform which support the ISO C amendement 1 functionality we
   support user defined character classes.  */
#if defined _LIBC || WIDE_CHAR_SUPPORT
/* Solaris 2.5 has a bug: <wchar.h> must be included before <wctype.h>.  */
# include <wchar.h>
# include <wctype.h>
#endif

/* We need some of the locale data (the collation sequence information)
   but there is no interface to get this information in general.  Therefore
   we support a correct implementation only in glibc.  */
#ifdef _LIBC
# include "../locale/localeinfo.h"
# include "../locale/elem-hash.h"
# include "../locale/coll-lookup.h"
# include <shlib-compat.h>

# define CONCAT(a,b) __CONCAT(a,b)
# define mbsrtowcs __mbsrtowcs
# define fnmatch __fnmatch
extern int fnmatch (const char *pattern, const char *string, int flags);
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

/* We often have to test for FNM_FILE_NAME and FNM_PERIOD being both set.  */
#define NO_LEADING_PERIOD(flags) \
  ((flags & (FNM_FILE_NAME | FNM_PERIOD)) == (FNM_FILE_NAME | FNM_PERIOD))

/* Comment out all this code if we are using the GNU C Library, and are not
   actually compiling the library itself, and have not detected a bug
   in the library.  This code is part of the GNU C
   Library, but also included in many other GNU distributions.  Compiling
   and linking in this code is a waste when using the GNU C library
   (especially if it is a shared library).  Rather than having every GNU
   program understand `configure --with-gnu-libc' and omit the object files,
   it is simpler to just do this in the source for each such file.  */

#if defined _LIBC || !defined __GNU_LIBRARY__ || !HAVE_FNMATCH_GNU


# if defined STDC_HEADERS || !defined isascii
#  define ISASCII(c) 1
# else
#  define ISASCII(c) isascii(c)
# endif

# ifdef isblank
#  define ISBLANK(c) (ISASCII (c) && isblank (c))
# else
#  define ISBLANK(c) ((c) == ' ' || (c) == '\t')
# endif
# ifdef isgraph
#  define ISGRAPH(c) (ISASCII (c) && isgraph (c))
# else
#  define ISGRAPH(c) (ISASCII (c) && isprint (c) && !isspace (c))
# endif

# define ISPRINT(c) (ISASCII (c) && isprint (c))
# define ISDIGIT(c) (ISASCII (c) && isdigit (c))
# define ISALNUM(c) (ISASCII (c) && isalnum (c))
# define ISALPHA(c) (ISASCII (c) && isalpha (c))
# define ISCNTRL(c) (ISASCII (c) && iscntrl (c))
# define ISLOWER(c) (ISASCII (c) && islower (c))
# define ISPUNCT(c) (ISASCII (c) && ispunct (c))
# define ISSPACE(c) (ISASCII (c) && isspace (c))
# define ISUPPER(c) (ISASCII (c) && isupper (c))
# define ISXDIGIT(c) (ISASCII (c) && isxdigit (c))

# define STREQ(s1, s2) ((strcmp (s1, s2) == 0))

# if defined _LIBC || WIDE_CHAR_SUPPORT
/* The GNU C library provides support for user-defined character classes
   and the functions from ISO C amendement 1.  */
#  ifdef CHARCLASS_NAME_MAX
#   define CHAR_CLASS_MAX_LENGTH CHARCLASS_NAME_MAX
#  else
/* This shouldn't happen but some implementation might still have this
   problem.  Use a reasonable default value.  */
#   define CHAR_CLASS_MAX_LENGTH 256
#  endif

#  ifdef _LIBC
#   define IS_CHAR_CLASS(string) __wctype (string)
#  else
#   define IS_CHAR_CLASS(string) wctype (string)
#  endif

#  ifdef _LIBC
#   define ISWCTYPE(WC, WT)	__iswctype (WC, WT)
#  else
#   define ISWCTYPE(WC, WT)	iswctype (WC, WT)
#  endif

#  if (HAVE_MBSTATE_T && HAVE_MBSRTOWCS) || _LIBC
/* In this case we are implementing the multibyte character handling.  */
#   define HANDLE_MULTIBYTE	1
#  endif

# else
#  define CHAR_CLASS_MAX_LENGTH  6 /* Namely, `xdigit'.  */

#  define IS_CHAR_CLASS(string)						      \
   (STREQ (string, "alpha") || STREQ (string, "upper")			      \
    || STREQ (string, "lower") || STREQ (string, "digit")		      \
    || STREQ (string, "alnum") || STREQ (string, "xdigit")		      \
    || STREQ (string, "space") || STREQ (string, "print")		      \
    || STREQ (string, "punct") || STREQ (string, "graph")		      \
    || STREQ (string, "cntrl") || STREQ (string, "blank"))
# endif

/* Avoid depending on library functions or files
   whose names are inconsistent.  */

# ifndef errno
extern int errno;
# endif

/* Global variable.  */
static int posixly_correct;

# ifndef internal_function
/* Inside GNU libc we mark some function in a special way.  In other
   environments simply ignore the marking.  */
#  define internal_function
# endif

/* Note that this evaluates C many times.  */
# ifdef _LIBC
#  define FOLD(c) ((flags & FNM_CASEFOLD) ? tolower (c) : (c))
# else
#  define FOLD(c) ((flags & FNM_CASEFOLD) && ISUPPER (c) ? tolower (c) : (c))
# endif
# define CHAR	char
# define UCHAR	unsigned char
# define INT	int
# define FCT	internal_fnmatch
# define EXT	ext_match
# define END	end_pattern
# define L(CS)	CS
# ifdef _LIBC
#  define BTOWC(C)	__btowc (C)
# else
#  define BTOWC(C)	btowc (C)
# endif
# define STRLEN(S) strlen (S)
# define STRCAT(D, S) strcat (D, S)
# ifdef _LIBC
#  define MEMPCPY(D, S, N) __mempcpy (D, S, N)
# else
#  if HAVE_MEMPCPY
#   define MEMPCPY(D, S, N) mempcpy (D, S, N)
#  else
#   define MEMPCPY(D, S, N) ((void *) ((char *) memcpy (D, S, N) + (N)))
#  endif
# endif
# define MEMCHR(S, C, N) memchr (S, C, N)
# define STRCOLL(S1, S2) strcoll (S1, S2)
# include "fnmatch_loop.c"


# if HANDLE_MULTIBYTE
#  define FOLD(c) ((flags & FNM_CASEFOLD) ? towlower (c) : (c))
#  define CHAR	wchar_t
#  define UCHAR	wint_t
#  define INT	wint_t
#  define FCT	internal_fnwmatch
#  define EXT	ext_wmatch
#  define END	end_wpattern
#  define L(CS)	L##CS
#  define BTOWC(C)	(C)
#  ifdef _LIBC
#   define STRLEN(S) __wcslen (S)
#   define STRCAT(D, S) __wcscat (D, S)
#   define MEMPCPY(D, S, N) __wmempcpy (D, S, N)
#  else
#   define STRLEN(S) wcslen (S)
#   define STRCAT(D, S) wcscat (D, S)
#   if HAVE_WMEMPCPY
#    define MEMPCPY(D, S, N) wmempcpy (D, S, N)
#   else
#    define MEMPCPY(D, S, N) (wmemcpy (D, S, N) + (N))
#   endif
#  endif
#  define MEMCHR(S, C, N) wmemchr (S, C, N)
#  define STRCOLL(S1, S2) wcscoll (S1, S2)
#  define WIDE_CHAR_VERSION 1

#  undef IS_CHAR_CLASS
/* We have to convert the wide character string in a multibyte string.  But
   we know that the character class names consist of alphanumeric characters
   from the portable character set, and since the wide character encoding
   for a member of the portable character set is the same code point as
   its single-byte encoding, we can use a simplified method to convert the
   string to a multibyte character string.  */
static wctype_t
is_char_class (const wchar_t *wcs)
{
  char s[CHAR_CLASS_MAX_LENGTH + 1];
  char *cp = s;

  do
    {
      /* Test for a printable character from the portable character set.  */
#  ifdef _LIBC
      if (*wcs < 0x20 || *wcs > 0x7e
	  || *wcs == 0x24 || *wcs == 0x40 || *wcs == 0x60)
	return (wctype_t) 0;
#  else
      switch (*wcs)
	{
	case L' ': case L'!': case L'"': case L'#': case L'%':
	case L'&': case L'\'': case L'(': case L')': case L'*':
	case L'+': case L',': case L'-': case L'.': case L'/':
	case L'0': case L'1': case L'2': case L'3': case L'4':
	case L'5': case L'6': case L'7': case L'8': case L'9':
	case L':': case L';': case L'<': case L'=': case L'>':
	case L'?':
	case L'A': case L'B': case L'C': case L'D': case L'E':
	case L'F': case L'G': case L'H': case L'I': case L'J':
	case L'K': case L'L': case L'M': case L'N': case L'O':
	case L'P': case L'Q': case L'R': case L'S': case L'T':
	case L'U': case L'V': case L'W': case L'X': case L'Y':
	case L'Z':
	case L'[': case L'\\': case L']': case L'^': case L'_':
	case L'a': case L'b': case L'c': case L'd': case L'e':
	case L'f': case L'g': case L'h': case L'i': case L'j':
	case L'k': case L'l': case L'm': case L'n': case L'o':
	case L'p': case L'q': case L'r': case L's': case L't':
	case L'u': case L'v': case L'w': case L'x': case L'y':
	case L'z': case L'{': case L'|': case L'}': case L'~':
	  break;
	default:
	  return (wctype_t) 0;
	}
#  endif

      /* Avoid overrunning the buffer.  */
      if (cp == s + CHAR_CLASS_MAX_LENGTH)
	return (wctype_t) 0;

      *cp++ = (char) *wcs++;
    }
  while (*wcs != L'\0');

  *cp = '\0';

#  ifdef _LIBC
  return __wctype (s);
#  else
  return wctype (s);
#  endif
}
#  define IS_CHAR_CLASS(string) is_char_class (string)

#  include "fnmatch_loop.c"
# endif


int
fnmatch (const char *pattern, const char *string, int flags)
{
# if HANDLE_MULTIBYTE
#  define ALLOCA_LIMIT 2000
  if (__builtin_expect (MB_CUR_MAX, 1) != 1)
    {
      mbstate_t ps;
      size_t patsize;
      size_t strsize;
      size_t totsize;
      wchar_t *wpattern;
      wchar_t *wstring;
      int res;

      /* Calculate the size needed to convert the strings to
	 wide characters.  */
      memset (&ps, '\0', sizeof (ps));
      patsize = mbsrtowcs (NULL, &pattern, 0, &ps) + 1;
      if (__builtin_expect (patsize == 0, 0))
	/* Something wrong.
	   XXX Do we have to set `errno' to something which mbsrtows hasn't
	   already done?  */
	return -1;
      assert (mbsinit (&ps));
      strsize = mbsrtowcs (NULL, &string, 0, &ps) + 1;
      if (__builtin_expect (strsize == 0, 0))
	/* Something wrong.
	   XXX Do we have to set `errno' to something which mbsrtows hasn't
	   already done?  */
	return -1;
      assert (mbsinit (&ps));
      totsize = patsize + strsize;
      if (__builtin_expect (! (patsize <= totsize
			       && totsize <= SIZE_MAX / sizeof (wchar_t)),
			    0))
	{
	  errno = ENOMEM;
	  return -1;
	}

      /* Allocate room for the wide characters.  */
      if (__builtin_expect (totsize < ALLOCA_LIMIT, 1))
	wpattern = (wchar_t *) alloca (totsize * sizeof (wchar_t));
      else
	{
	  wpattern = malloc (totsize * sizeof (wchar_t));
	  if (__builtin_expect (! wpattern, 0))
	    {
	      errno = ENOMEM;
	      return -1;
	    }
	}
      wstring = wpattern + patsize;

      /* Convert the strings into wide characters.  */
      mbsrtowcs (wpattern, &pattern, patsize, &ps);
      assert (mbsinit (&ps));
      mbsrtowcs (wstring, &string, strsize, &ps);

      res = internal_fnwmatch (wpattern, wstring, wstring + strsize - 1,
			       flags & FNM_PERIOD, flags);

      if (__builtin_expect (! (totsize < ALLOCA_LIMIT), 0))
	free (wpattern);
      return res;
    }
# endif /* HANDLE_MULTIBYTE */

  return internal_fnmatch (pattern, string, string + strlen (string),
			   flags & FNM_PERIOD, flags);
}

# ifdef _LIBC
#  undef fnmatch
versioned_symbol (libc, __fnmatch, fnmatch, GLIBC_2_2_3);
#  if SHLIB_COMPAT(libc, GLIBC_2_0, GLIBC_2_2_3)
strong_alias (__fnmatch, __fnmatch_old)
compat_symbol (libc, __fnmatch_old, fnmatch, GLIBC_2_0);
#  endif
libc_hidden_ver (__fnmatch, fnmatch)
# endif

#endif	/* _LIBC or not __GNU_LIBRARY__.  */
