/* hinit.c
   Initialize for reading HDB configuration files.

   Copyright (C) 1992, 1994 Ian Lance Taylor

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
const char _uuconf_hinit_rcsid[] = "$FreeBSD$";
#endif

#include <errno.h>
#include <ctype.h>

/* Avoid replicating OLDCONFIGLIB several times if not necessary.  */
static const char abHoldconfiglib[] = OLDCONFIGLIB;

/* Initialize the routines which read HDB configuration files.  */

int
uuconf_hdb_init (ppglobal, zprogram)
     pointer *ppglobal;
     const char *zprogram;
{
  struct sglobal **pqglobal = (struct sglobal **) ppglobal;
  int iret;
  struct sglobal *qglobal;
  pointer pblock;
  char abdialcodes[sizeof OLDCONFIGLIB + sizeof HDB_DIALCODES - 1];
  char *zsys;
  FILE *e;

  if (*pqglobal == NULL)
    {
      iret = _uuconf_iinit_global (pqglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  qglobal = *pqglobal;
  pblock = qglobal->pblock;

  if (zprogram == NULL
      || strcmp (zprogram, "uucp") == 0)
    zprogram = "uucico";

  /* Add the Dialcodes file to the global list.  */
  memcpy ((pointer) abdialcodes, (pointer) abHoldconfiglib,
	  sizeof OLDCONFIGLIB - 1);
  memcpy ((pointer) (abdialcodes + sizeof OLDCONFIGLIB - 1),
	  (pointer) HDB_DIALCODES, sizeof HDB_DIALCODES);
  iret = _uuconf_iadd_string (qglobal, abdialcodes, TRUE, FALSE,
			      &qglobal->qprocess->pzdialcodefiles,
			      pblock);
  if (iret != UUCONF_SUCCESS)
    return iret;

  /* Read the Sysfiles file.  We allocate the name on the heap rather
     than the stack so that we can return it in
     qerr->uuconf_zfilename.  */

  zsys = uuconf_malloc (pblock,
			sizeof OLDCONFIGLIB + sizeof HDB_SYSFILES - 1);
  if (zsys == NULL)
    {
      qglobal->ierrno = errno;
      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
    }
  memcpy ((pointer) zsys, (pointer) abHoldconfiglib, sizeof OLDCONFIGLIB - 1);
  memcpy ((pointer) (zsys + sizeof OLDCONFIGLIB - 1), (pointer) HDB_SYSFILES,
	  sizeof HDB_SYSFILES);

  iret = UUCONF_SUCCESS;

  e = fopen (zsys, "r");
  if (e == NULL)
    uuconf_free (pblock, zsys);
  else
    {
      char *zline;
      size_t cline;
      char **pzargs;
      size_t cargs;
      char **pzcolon;
      size_t ccolon;
      int cchars;

      zline = NULL;
      cline = 0;
      pzargs = NULL;
      cargs = 0;
      pzcolon = NULL;
      ccolon = 0;

      qglobal->ilineno = 0;

      while (iret == UUCONF_SUCCESS
	     && (cchars = _uuconf_getline (qglobal, &zline, &cline, e)) > 0)
	{
	  int ctypes, cnames;
	  int i;

	  ++qglobal->ilineno;

	  --cchars;
	  if (zline[cchars] == '\n')
	    zline[cchars] = '\0';
	  if (zline[0] == '#')
	    continue;

	  ctypes = _uuconf_istrsplit (zline, '\0', &pzargs, &cargs);
	  if (ctypes < 0)
	    {
	      qglobal->ierrno = errno;
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }

	  if (ctypes == 0)
	    continue;

	  if (strncmp (pzargs[0], "service=", sizeof "service=" - 1) != 0)
	    {
	      iret = UUCONF_SYNTAX_ERROR;
	      break;
	    }
	  pzargs[0] += sizeof "service=" - 1;

	  cnames = _uuconf_istrsplit (pzargs[0], ':', &pzcolon, &ccolon);
	  if (cnames < 0)
	    {
	      qglobal->ierrno = errno;
	      iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	      break;
	    }

	  for (i = 0; i < cnames; i++)
	    if (strcmp (zprogram, pzcolon[i]) == 0)
	      break;

	  if (i >= cnames)
	    continue;

	  for (i = 1; i < ctypes && iret == UUCONF_SUCCESS; i++)
	    {
	      char ***ppz;
	      int cfiles, ifile;
	      
	      if (strncmp (pzargs[i], "systems=", sizeof "systems=" - 1)
		  == 0)
		{
		  ppz = &qglobal->qprocess->pzhdb_systems;
		  pzargs[i] += sizeof "systems=" - 1;
		}
	      else if (strncmp (pzargs[i], "devices=", sizeof "devices=" - 1)
		       == 0)
		{
		  ppz = &qglobal->qprocess->pzhdb_devices;
		  pzargs[i] += sizeof "devices=" - 1;
		}
	      else if (strncmp (pzargs[i], "dialers=", sizeof "dialers=" - 1)
		       == 0)
		{
		  ppz = &qglobal->qprocess->pzhdb_dialers;
		  pzargs[i] += sizeof "dialers=" - 1;
		}
	      else
		{
		  iret = UUCONF_SYNTAX_ERROR;
		  break;
		}

	      cfiles = _uuconf_istrsplit (pzargs[i], ':', &pzcolon, &ccolon);
	      if (cfiles < 0)
		{
		  qglobal->ierrno = errno;
		  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		  break;
		}

	      for (ifile = 0;
		   ifile < cfiles && iret == UUCONF_SUCCESS;
		   ifile++)
		{
		  /* Looking for a leading '/' is Unix dependent, and
		     should probably be changed.  */
		  if (pzcolon[ifile][0] == '/')
		    iret = _uuconf_iadd_string (qglobal, pzcolon[ifile], TRUE,
						FALSE, ppz, pblock);
		  else
		    {
		      char *zdir;
		      size_t clen;

		      clen = strlen (pzcolon[ifile]);
		      zdir = (char *) uuconf_malloc (pblock,
						     (sizeof OLDCONFIGLIB
						      + sizeof HDB_SEPARATOR
						      + clen
						      - 1));
		      if (zdir == NULL)
			{
			  qglobal->ierrno = errno;
			  iret = UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
			  break;
			}
		      memcpy ((pointer) zdir, (pointer) abHoldconfiglib,
			      sizeof OLDCONFIGLIB - 1);
		      memcpy ((pointer) (zdir + sizeof OLDCONFIGLIB - 1),
			      HDB_SEPARATOR, sizeof HDB_SEPARATOR - 1);
		      memcpy ((pointer) (zdir
					 + sizeof OLDCONFIGLIB - 1
					 + sizeof HDB_SEPARATOR - 1),
			      (pointer) pzcolon[ifile], clen + 1);
		      iret = _uuconf_iadd_string (qglobal, zdir, FALSE, FALSE,
						  ppz, pblock);
		    }
		}
	    }
	}

      (void) fclose (e);
      if (zline != NULL)
	free ((pointer) zline);
      if (pzargs != NULL)
	free ((pointer) pzargs);
      if (pzcolon != NULL)
	free ((pointer) pzcolon);

      if (iret != UUCONF_SUCCESS)
	{
	  qglobal->zfilename = zsys;
	  return iret | UUCONF_ERROR_FILENAME | UUCONF_ERROR_LINENO;
	}
    }

  if (qglobal->qprocess->pzhdb_systems == NULL)
    {
      char ab[sizeof OLDCONFIGLIB + sizeof HDB_SYSTEMS - 1];

      memcpy ((pointer) ab, (pointer) abHoldconfiglib,
	      sizeof OLDCONFIGLIB - 1);
      memcpy ((pointer) (ab + sizeof OLDCONFIGLIB - 1),
	      (pointer) HDB_SYSTEMS, sizeof HDB_SYSTEMS);
      iret = _uuconf_iadd_string (qglobal, ab, TRUE, FALSE,
				  &qglobal->qprocess->pzhdb_systems,
				  pblock);
    }
  if (qglobal->qprocess->pzhdb_devices == NULL && iret == UUCONF_SUCCESS)
    {
      char ab[sizeof OLDCONFIGLIB + sizeof HDB_DEVICES - 1];

      memcpy ((pointer) ab, (pointer) abHoldconfiglib,
	      sizeof OLDCONFIGLIB - 1);
      memcpy ((pointer) (ab + sizeof OLDCONFIGLIB - 1),
	      (pointer) HDB_DEVICES, sizeof HDB_DEVICES);
      iret = _uuconf_iadd_string (qglobal, ab, TRUE, FALSE,
				  &qglobal->qprocess->pzhdb_devices,
				  pblock);
    }
  if (qglobal->qprocess->pzhdb_dialers == NULL && iret == UUCONF_SUCCESS)
    {
      char ab[sizeof OLDCONFIGLIB + sizeof HDB_DIALERS - 1];

      memcpy ((pointer) ab, (pointer) abHoldconfiglib,
	      sizeof OLDCONFIGLIB - 1);
      memcpy ((pointer) (ab + sizeof OLDCONFIGLIB - 1),
	      (pointer) HDB_DIALERS, sizeof HDB_DIALERS);
      iret = _uuconf_iadd_string (qglobal, ab, TRUE, FALSE,
				  &qglobal->qprocess->pzhdb_dialers,
				  pblock);
    }

  return iret;
}
