/* demangle.c -- A wrapper calling libiberty cplus_demangle
   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "config.h"
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include "bfd.h"
#include "libiberty.h"
#include "demangle.h"
#include "budemang.h"

/* Wrapper around cplus_demangle.  Strips leading underscores and
   other such chars that would otherwise confuse the demangler.  */

char *
demangle (bfd *abfd, const char *name)
{
  char *res, *alloc;
  const char *pre, *suf;
  size_t pre_len;

  if (abfd != NULL && bfd_get_symbol_leading_char (abfd) == name[0])
    ++name;

  /* This is a hack for better error reporting on XCOFF, PowerPC64-ELF
     or the MS PE format.  These formats have a number of leading '.'s
     on at least some symbols, so we remove all dots to avoid
     confusing the demangler.  */
  pre = name;
  while (*name == '.')
    ++name;
  pre_len = name - pre;

  alloc = NULL;
  suf = strchr (name, '@');
  if (suf != NULL)
    {
      alloc = xmalloc (suf - name + 1);
      memcpy (alloc, name, suf - name);
      alloc[suf - name] = '\0';
      name = alloc;
    }

  res = cplus_demangle (name, DMGL_ANSI | DMGL_PARAMS);
  if (res != NULL)
    {
      /* Now put back any suffix, or stripped dots.  */
      if (pre_len != 0 || suf != NULL)
	{
	  size_t len;
	  size_t suf_len;
	  char *final;

	  if (alloc != NULL)
	    free (alloc);

	  len = strlen (res);
	  if (suf == NULL)
	    suf = res + len;
	  suf_len = strlen (suf) + 1;
	  final = xmalloc (pre_len + len + suf_len);

	  memcpy (final, pre, pre_len);
	  memcpy (final + pre_len, res, len);
	  memcpy (final + pre_len + len, suf, suf_len);
	  free (res);
	  res = final;
	}

      return res;
    }

  if (alloc != NULL)
    free (alloc);

  return xstrdup (pre);
}
