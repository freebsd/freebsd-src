/* Set a file descriptor's mode to binary or to text.

   Copyright (C) 2001, 2004 Free Software Foundation, Inc.

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

/* Written by Paul Eggert <eggert@twinsun.com>  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_STDBOOL_H
# include <stdbool.h>
#else
typedef enum {false = 0, true = 1} bool;
#endif

#if HAVE_SETMODE_DOS
# include <io.h>
# if HAVE_FCNTL_H
#  include <fcntl.h>
# endif
# if HAVE_UNISTD_H
#  include <unistd.h>
# endif
#endif

#include "setmode.h"
#undef set_binary_mode

#ifndef __attribute__
# if __GNUC__ < 3 || __STRICT_ANSI__
#  define __attribute__(x)
# endif
#endif

/* Set the binary mode of FD to MODE, returning its previous mode.
   MODE is 1 for binary and 0 for text.  If setting the mode might
   cause problems, ignore the request and return MODE.  Always return
   1 on POSIX platforms, which do not distinguish between text and
   binary.  */

#if HAVE_SETMODE_DOS
bool
set_binary_mode (int fd, bool mode)
{
  if (isatty (fd))
    return mode;
  return setmode (fd, mode ? O_BINARY : O_TEXT) != O_TEXT;
}
#else
static char dummy;
#endif
