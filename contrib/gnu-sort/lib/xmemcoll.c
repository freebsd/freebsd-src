/* Locale-specific memory comparison.
   Copyright (C) 2002, 2003, 2004 Free Software Foundation, Inc.

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

/* Contributed by Paul Eggert <eggert@twinsun.com>.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)

#include "error.h"
#include "exitfail.h"
#include "memcoll.h"
#include "quotearg.h"
#include "xmemcoll.h"

/* Compare S1 (with length S1LEN) and S2 (with length S2LEN) according
   to the LC_COLLATE locale.  S1 and S2 do not overlap, and are not
   adjacent.  Temporarily modify the bytes after S1 and S2, but
   restore their original contents before returning.  Report an error
   and exit if there is an error.  */

int
xmemcoll (char *s1, size_t s1len, char *s2, size_t s2len)
{
  int diff = memcoll (s1, s1len, s2, s2len);
  int collation_errno = errno;

  if (collation_errno)
    {
      error (0, collation_errno, _("string comparison failed"));
      error (0, 0, _("Set LC_ALL='C' to work around the problem."));
      error (exit_failure, 0,
	     _("The strings compared were %s and %s."),
	     quotearg_n_style_mem (0, locale_quoting_style, s1, s1len),
	     quotearg_n_style_mem (1, locale_quoting_style, s2, s2len));
    }

  return diff;
}
