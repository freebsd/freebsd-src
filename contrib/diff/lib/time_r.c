/* Reentrant time functions like localtime_r.

   Copyright (C) 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "time_r.h"

#include <string.h>

static char *
copy_string_result (char *dest, char const *src)
{
  if (! src)
    return 0;
  return strcpy (dest, src);
}

static struct tm *
copy_tm_result (struct tm *dest, struct tm const *src)
{
  if (! src)
    return 0;
  *dest = *src;
  return dest;
}


char *
asctime_r (struct tm const * restrict tm, char * restrict buf)
{
  return copy_string_result (buf, asctime (tm));
}

char *
ctime_r (time_t const *t, char *buf)
{
  return copy_string_result (buf, ctime (t));
}

struct tm *
gmtime_r (time_t const * restrict t, struct tm * restrict tp)
{
  return copy_tm_result (tp, gmtime (t));
}

struct tm *
localtime_r (time_t const * restrict t, struct tm * restrict tp)
{
  return copy_tm_result (tp, localtime (t));
}
