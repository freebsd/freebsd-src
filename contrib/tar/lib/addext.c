/* addext.c -- add an extension to a file name
   Copyright 1990, 1997, 1998, 1999, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by David MacKenzie <djm@gnu.ai.mit.edu> and Paul Eggert */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef HAVE_DOS_FILE_NAMES
# define HAVE_DOS_FILE_NAMES 0
#endif
#ifndef HAVE_LONG_FILE_NAMES
# define HAVE_LONG_FILE_NAMES 0
#endif

#if HAVE_LIMITS_H
# include <limits.h>
#endif
#ifndef _POSIX_NAME_MAX
# define _POSIX_NAME_MAX 14
#endif

#include <sys/types.h>
#if HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#include "backupfile.h"
#include "dirname.h"

/* Append to FILENAME the extension EXT, unless the result would be too long,
   in which case just append the character E.  */

void
addext (char *filename, char const *ext, int e)
{
  char *s = base_name (filename);
  size_t slen = base_len (s);
  size_t extlen = strlen (ext);
  size_t slen_max = HAVE_LONG_FILE_NAMES ? 255 : _POSIX_NAME_MAX;

#if HAVE_PATHCONF && defined _PC_NAME_MAX
  if (_POSIX_NAME_MAX < slen + extlen || HAVE_DOS_FILE_NAMES)
    {
      /* The new base name is long enough to require a pathconf check.  */
      long name_max;
      errno = 0;
      if (s == filename)
	name_max = pathconf (".", _PC_NAME_MAX);
      else
	{
	  char c = *s;
	  if (! ISSLASH (c))
	    *s = 0;
	  name_max = pathconf (filename, _PC_NAME_MAX);
	  *s = c;
	}
      if (0 <= name_max || errno == 0)
	slen_max = name_max == (size_t) name_max ? name_max : -1;
    }
#endif

  if (HAVE_DOS_FILE_NAMES && slen_max <= 12)
    {
      /* Live within DOS's 8.3 limit.  */
      char *dot = strchr (s, '.');
      if (dot)
	{
	  slen -= dot + 1 - s;
	  s = dot + 1;
	  slen_max = 3;
	}
      else
	slen_max = 8;
      extlen = 9; /* Don't use EXT.  */
    }

  if (slen + extlen <= slen_max)
    strcpy (s + slen, ext);
  else
    {
      if (slen_max <= slen)
	slen = slen_max - 1;
      s[slen] = e;
      s[slen + 1] = 0;
    }
}
