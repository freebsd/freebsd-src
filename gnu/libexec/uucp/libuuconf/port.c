/* port.c
   Find a port.

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
const char _uuconf_port_rcsid[] = "$FreeBSD$";
#endif

/* Find a port by name, baud rate, and special purpose function.  */

int
uuconf_find_port (pglobal, zname, ibaud, ihighbaud, pifn, pinfo, qport)
     pointer pglobal;
     const char *zname;
     long ibaud;
     long ihighbaud;
     int (*pifn) P((struct uuconf_port *, pointer));
     pointer pinfo;
     struct uuconf_port *qport;
{
#if HAVE_V2_CONFIG || HAVE_HDB_CONFIG
  struct sglobal *qglobal = (struct sglobal *) pglobal;
#endif
  int iret;

#if HAVE_TAYLOR_CONFIG
  iret = uuconf_taylor_find_port (pglobal, zname, ibaud, ihighbaud, pifn,
				  pinfo, qport);
  if (iret != UUCONF_NOT_FOUND)
    return iret;
#endif

#if HAVE_V2_CONFIG
  if (qglobal->qprocess->fv2)
    {
      iret = uuconf_v2_find_port (pglobal, zname, ibaud, ihighbaud, pifn,
				  pinfo, qport);
      if (iret != UUCONF_NOT_FOUND)
	return iret;
    }
#endif

#if HAVE_HDB_CONFIG
  if (qglobal->qprocess->fhdb)
    {
      iret = uuconf_hdb_find_port (pglobal, zname, ibaud, ihighbaud, pifn,
				   pinfo, qport);
      if (iret != UUCONF_NOT_FOUND)
	return iret;
    }
#endif

  return UUCONF_NOT_FOUND;
}
