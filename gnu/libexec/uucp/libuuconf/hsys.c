/* hsys.c
   User function to get a system from the HDB configuration files.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_hsys_rcsid[] = "$Id: hsys.c,v 1.1 1993/08/05 18:25:31 conklin Exp $";
#endif

/* Get system information from the HDB configuration files.  This is a
   wrapper for the internal function which makes sure that every field
   gets a default value.  */

int
uuconf_hdb_system_info (pglobal, zsystem, qsys)
     pointer pglobal;
     const char *zsystem;
     struct uuconf_system *qsys;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;

  iret = _uuconf_ihdb_system_internal (qglobal, zsystem, qsys);
  if (iret != UUCONF_SUCCESS)
    return iret;
  return _uuconf_isystem_basic_default (qglobal, qsys);
}
