/* dup2.c
   The Unix dup2 function, for systems which only have dup.

   Copyright (C) 1985, 1986, 1987, 1988, 1990 Free Software Foundation, Inc.

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"
#include "sysdep.h"

#include <errno.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

/* I basically took this from the emacs 18.57 distribution, although I
   cleaned it up a bit and made it POSIX compliant.  */

int
dup2 (oold, onew)
     int oold;
     int onew;
{
  if (oold == onew)
    return onew;
  (void) close (onew);
  
#ifdef F_DUPFD
  return fcntl (oold, F_DUPFD, onew);
#else
  {
    int onext, oret, isave;

    onext = dup (oold);
    if (onext == onew)
      return onext;
    if (onext < 0)
      return -1;
    oret = dup2 (oold, onew);
    isave = errno;
    (void) close (onext);
    errno = isave;
    return oret;
  }
#endif
}
