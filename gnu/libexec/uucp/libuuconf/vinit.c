/* vinit.c
   Initialize for reading V2 configuration files.

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
const char _uuconf_vinit_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/vinit.c,v 1.6 1999/08/27 23:33:36 peter Exp $";
#endif

#include <errno.h>

static int ivinlib P((struct sglobal *qglobal, const char *z, size_t csize,
		      char **pz));

/* Return an allocated buffer holding a file name in OLDCONFIGLIB.
   The c argument is the size of z including the trailing null byte,
   since this is convenient for both the caller and this function.  */

static int
ivinlib (qglobal, z, c, pz)
     struct sglobal *qglobal;
     const char *z;
     size_t c;
     char **pz;
{
  char *zalc;

  zalc = uuconf_malloc (qglobal->pblock, sizeof OLDCONFIGLIB - 1 + c);
  if (zalc == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }

  memcpy ((pointer) zalc, (pointer) OLDCONFIGLIB,
	  sizeof OLDCONFIGLIB - 1);
  memcpy ((pointer) (zalc + sizeof OLDCONFIGLIB - 1), (pointer) z, c);

  *pz = zalc;

  return UUCONF_SUCCESS;
}

/* Initialize the routines which read V2 configuration files.  The
   only thing we do here is allocate the file names.  */

int
uuconf_v2_init (ppglobal)
     pointer *ppglobal;
{
  struct sglobal **pqglobal = (struct sglobal **) ppglobal;
  int iret;
  struct sglobal *qglobal;
  char *zdialcodes;

  if (*pqglobal == NULL)
    {
      iret = _uuconf_iinit_global (pqglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  qglobal = *pqglobal;

  iret = ivinlib (qglobal, V2_SYSTEMS, sizeof V2_SYSTEMS,
		  &qglobal->qprocess->zv2systems);
  if (iret != UUCONF_SUCCESS)
    return iret;
  iret = ivinlib (qglobal, V2_DEVICES, sizeof V2_DEVICES,
		  &qglobal->qprocess->zv2devices);
  if (iret != UUCONF_SUCCESS)
    return iret;
  iret = ivinlib (qglobal, V2_USERFILE, sizeof V2_USERFILE,
		  &qglobal->qprocess->zv2userfile);
  if (iret != UUCONF_SUCCESS)
    return iret;
  iret = ivinlib (qglobal, V2_CMDS, sizeof V2_CMDS,
		  &qglobal->qprocess->zv2cmds);
  if (iret != UUCONF_SUCCESS)
    return iret;

  iret = ivinlib (qglobal, V2_DIALCODES, sizeof V2_DIALCODES,
		  &zdialcodes);
  if (iret != UUCONF_SUCCESS)
    return iret;

  return _uuconf_iadd_string (qglobal, zdialcodes, FALSE, FALSE,
			      &qglobal->qprocess->pzdialcodefiles,
			      qglobal->pblock);
}
