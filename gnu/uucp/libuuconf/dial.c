/* dial.c
   Find a dialer.

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
const char _uuconf_dial_rcsid[] = "$Id: dial.c,v 1.1 1993/08/05 18:25:12 conklin Exp $";
#endif

/* Find a dialer by name.  */

int
uuconf_dialer_info (pglobal, zdialer, qdialer)
     pointer pglobal;
     const char *zdialer;
     struct uuconf_dialer *qdialer;
{
#if HAVE_HDB_CONFIG
  struct sglobal *qglobal = (struct sglobal *) pglobal;
#endif
  int iret;

#if HAVE_TAYLOR_CONFIG
  iret = uuconf_taylor_dialer_info (pglobal, zdialer, qdialer);
  if (iret != UUCONF_NOT_FOUND)
    return iret;
#endif

#if HAVE_HDB_CONFIG
  if (qglobal->qprocess->fhdb)
    {
      iret = uuconf_hdb_dialer_info (pglobal, zdialer, qdialer);
      if (iret != UUCONF_NOT_FOUND)
	return iret;
    }
#endif

  return UUCONF_NOT_FOUND;
}
