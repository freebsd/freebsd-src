/* hunk.c
   Get information about an unknown system from the HDB Permissions file.

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
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_hunk_rcsid[] = "$Id: hunk.c,v 1.2 1994/05/07 18:12:30 ache Exp $";
#endif

#include <errno.h>

/* Get information about an unknown system from the HDB Permissions
   file.  This doesn't run the remote.unknown shell script, because
   that's too system dependent.  */

int
uuconf_hdb_system_unknown (pglobal, qsys)
     pointer pglobal;
     struct uuconf_system *qsys;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;
  boolean ffirst;
  struct shpermissions *qperm;
  struct uuconf_system *qalt;

  if (! qglobal->qprocess->fhdb_read_permissions)
    {
      iret = _uuconf_ihread_permissions (qglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  _uuconf_uclear_system (qsys);
  qsys->uuconf_palloc = uuconf_malloc_block ();
  if (qsys->uuconf_palloc == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  ffirst = TRUE;

  for (qperm = qglobal->qprocess->qhdb_permissions;
       qperm != NULL;
       qperm = qperm->qnext)
    {
      char **pz;

      if (qperm->pzlogname == NULL
	  || qperm->pzlogname == (char **) &_uuconf_unset)
	continue;

      for (pz = qperm->pzlogname; *pz != NULL; pz++)
	{
	  if (ffirst)
	    {
	      qalt = qsys;
	      ffirst = FALSE;
	    }
	  else
	    {
	      struct uuconf_system **pq;

	      qalt = ((struct uuconf_system *)
		      uuconf_malloc (qsys->uuconf_palloc,
				     sizeof (struct uuconf_system)));
	      if (qalt == NULL)
		{
		  qglobal->ierrno = errno;
		  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		}

	      _uuconf_uclear_system (qalt);
	      for (pq = &qsys->uuconf_qalternate;
		   *pq != NULL;
		   pq = &(*pq)->uuconf_qalternate)
		;
	      *pq = qalt;
	    }

	  /* We recognize LOGNAME=OTHER specially, although this
	     appears to be an SCO innovation.  */
	  if (strcmp (*pz, "OTHER") == 0)
	    qalt->uuconf_zcalled_login = (char *) "ANY";
	  else
	    qalt->uuconf_zcalled_login = *pz;
	  qalt->uuconf_fcall = FALSE;
	  qsys->uuconf_fcalled = TRUE;
	  if (qperm->frequest >= 0)
	    qsys->uuconf_fsend_request = qperm->frequest;
	  else
	    qsys->uuconf_fsend_request = FALSE;
	  qsys->uuconf_fcalled_transfer = qperm->fsendfiles;
	  qsys->uuconf_pzremote_send = qperm->pzread;
	  qsys->uuconf_pzremote_receive = qperm->pzwrite;
	  qsys->uuconf_fcallback = qperm->fcallback;
	  qsys->uuconf_zlocalname = qperm->zmyname;
	  qsys->uuconf_zpubdir = qperm->zpubdir;
	}
    }

  if (ffirst)
    return UUCONF_NOT_FOUND;

  /* HDB permits local requests to receive to any directory, which is
     not the default put in by _uuconf_isystem_basic_default.  We set
     it here instead.  */
  for (qalt = qsys; qalt != NULL; qalt = qalt->uuconf_qalternate)
    {
      iret = _uuconf_iadd_string (qglobal, (char *) ZROOTDIR,
				  FALSE, FALSE,
				  &qalt->uuconf_pzlocal_receive,
				  qsys->uuconf_palloc);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  return _uuconf_isystem_basic_default (qglobal, qsys);
}
