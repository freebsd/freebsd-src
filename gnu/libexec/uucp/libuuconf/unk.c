/* unk.c
   Get information about an unknown system.

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
const char _uuconf_unk_rcsid[] = "$Id: unk.c,v 1.1 1993/08/05 18:26:18 conklin Exp $";
#endif

#include <errno.h>

/* Get information about an unknown system.  If we are using
   HAVE_TAYLOR_CONFIG, we just use it.  Otherwise if we are using
   HAVE_HDB_CONFIG, we use it.  Otherwise we return a default system.
   This isn't right for HAVE_V2_CONFIG, because it is possible to
   specify default directories to read and write in USERFILE.
   However, I'm not going to bother to write that code unless somebody
   actually wants it.  */

/*ARGSUSED*/
int
uuconf_system_unknown (pglobal, qsys)
     pointer pglobal;
     struct uuconf_system *qsys;
{
#if HAVE_TAYLOR_CONFIG
  return uuconf_taylor_system_unknown (pglobal, qsys);
#else /* ! HAVE_TAYLOR_CONFIG */
#if HAVE_HDB_CONFIG
  return uuconf_hdb_system_unknown (pglobal, qsys);
#else /* ! HAVE_HDB_CONFIG */
#if HAVE_V2_CONFIG
  struct sglobal *qglobal = (struct sglobal *) pglobal;

  _uuconf_uclear_system (qsys);
  qsys->uuconf_palloc = uuconf_malloc_block ();
  if (qsys->uuconf_palloc == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }
  return _uuconf_isystem_basic_default (qglobal, qsys);
#else /* ! HAVE_V2_CONFIG */
  return UUCONF_NOT_FOUND;
#endif /* ! HAVE_V2_CONFIG */
#endif /* ! HAVE_HDB_CONFIG */
#endif /* ! HAVE_TAYLOR_CONFIG */
}
