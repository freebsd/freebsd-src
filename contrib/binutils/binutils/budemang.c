/* demangle.c -- A wrapper calling libiberty cplus_demangle
   Copyright 2002 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

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
demangle (abfd, name)
     bfd *abfd;
     const char *name;
{
  char *res;
  const char *p;

  if (abfd != NULL && bfd_get_symbol_leading_char (abfd) == name[0])
    ++name;

  /* This is a hack for better error reporting on XCOFF, PowerPC64-ELF
     or the MS PE format.  These formats have a number of leading '.'s
     on at least some symbols, so we remove all dots to avoid
     confusing the demangler.  */
  p = name;
  while (*p == '.')
    ++p;

  res = cplus_demangle (p, DMGL_ANSI | DMGL_PARAMS);
  if (res)
    {
      size_t dots = p - name;

      /* Now put back any stripped dots.  */
      if (dots != 0)
	{
	  size_t len = strlen (res) + 1;
	  char *add_dots = xmalloc (len + dots);

	  memcpy (add_dots, name, dots);
	  memcpy (add_dots + dots, res, len);
	  free (res);
	  res = add_dots;
	}
      return res;
    }

  return xstrdup (name);
}
