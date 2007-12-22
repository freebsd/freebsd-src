/* quote.c - quote arguments for output
   Copyright (C) 1998, 1999, 2000, 2001, 2003 Free Software Foundation, Inc.

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

/* Written by Paul Eggert <eggert@twinsun.com> */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "quotearg.h"
#include "quote.h"

/* Return an unambiguous printable representation of NAME,
   allocated in slot N, suitable for diagnostics.  */
char const *
quote_n (int n, char const *name)
{
  return quotearg_n_style (n, locale_quoting_style, name);
}

/* Return an unambiguous printable representation of NAME,
   suitable for diagnostics.  */
char const *
quote (char const *name)
{
  return quote_n (0, name);
}
