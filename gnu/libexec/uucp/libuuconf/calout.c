/* calout.c
   Find callout login name and password for a system.

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
const char _uuconf_calout_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/calout.c,v 1.7 1999/08/27 23:33:15 peter Exp $";
#endif

#include <errno.h>

/* Find callout login name and password for a system.  */

/*ARGSUSED*/
int
uuconf_callout (pglobal, qsys, pzlog, pzpass)
     pointer pglobal;
     const struct uuconf_system *qsys;
     char **pzlog;
     char **pzpass;
{
#if HAVE_TAYLOR_CONFIG

  return uuconf_taylor_callout (pglobal, qsys, pzlog, pzpass);

#else /* ! HAVE_TAYLOR_CONFIG */

  struct sglobal *qglobal = (struct sglobal *) pglobal;

  *pzlog = NULL;
  *pzpass = NULL;

  if (qsys->uuconf_zcall_login == NULL
      && qsys->uuconf_zcall_password == NULL)
    return UUCONF_NOT_FOUND;

  if ((qsys->uuconf_zcall_login != NULL
       && strcmp (qsys->uuconf_zcall_login, "*") == 0)
      || (qsys->uuconf_zcall_password != NULL
	  && strcmp (qsys->uuconf_zcall_password, "*") == 0))
    return UUCONF_NOT_FOUND;
      
  if (qsys->uuconf_zcall_login != NULL)
    {
      *pzlog = strdup (qsys->uuconf_zcall_login);
      if (*pzlog == NULL)
	{
	  qglobal->ierrno = errno;
	  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	}
    }

  if (qsys->uuconf_zcall_password != NULL)
    {
      *pzpass = strdup (qsys->uuconf_zcall_password);
      if (*pzpass == NULL)
	{
	  qglobal->ierrno = errno;
	  if (*pzlog != NULL)
	    {
	      free ((pointer) *pzlog);
	      *pzlog = NULL;
	    }
	  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	}
    }

  return UUCONF_SUCCESS;

#endif /* ! HAVE_TAYLOR_CONFIG */
}
