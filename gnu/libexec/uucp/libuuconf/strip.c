/* maxuxq.c
   Get information about what things should be stripped.

   Copyright (C) 1995 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_strip_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/strip.c,v 1.4 1999/08/27 23:33:31 peter Exp $";
#endif

/* Get information about what types of global information should be
   stripped.  There are currently only two, which we return as a
   couple of bits.  We store them as two separate variables, so we
   don't need to have a special function to set the values correctly.  */

int
uuconf_strip (pglobal, pistrip)
     pointer pglobal;
     int *pistrip;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;

  *pistrip = 0;
  if (qglobal->qprocess->fstrip_login)
    *pistrip |= UUCONF_STRIP_LOGIN;
  if (qglobal->qprocess->fstrip_proto)
    *pistrip |= UUCONF_STRIP_PROTO;
  return UUCONF_SUCCESS;
}
