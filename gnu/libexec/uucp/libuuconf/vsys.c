/* vsys.c
   User function to get a system from the V2 configuration files.

   Copyright (C) 1992 Ian Lance Taylor

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
const char _uuconf_vsys_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/vsys.c,v 1.6 1999/08/27 23:33:37 peter Exp $";
#endif

/* Get system information from the V2 configuration files.  This is a
   wrapper for the internal function which makes sure that every field
   gets a default value.  */

int
uuconf_v2_system_info (pglobal, zsystem, qsys)
     pointer pglobal;
     const char *zsystem;
     struct uuconf_system *qsys;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;

  iret = _uuconf_iv2_system_internal (qglobal, zsystem, qsys);
  if (iret != UUCONF_SUCCESS)
    return iret;
  return _uuconf_isystem_basic_default (qglobal, qsys);
}
