/* jobid.c
   Convert file names to jobids and vice versa.

   Copyright (C) 1991, 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#include "uuconf.h"
#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* Translate a file name and an associated system into a job id.
   These job ids are used by uustat.  We use the system name attached
   to the grade and sequence number.  This won't work correctly if the
   file name was actually created by some other version of uucp that
   uses a different length for the sequence number.  Too bad.  */

char *
zsfile_to_jobid (qsys, zfile, bgrade)
     const struct uuconf_system *qsys;
     const char *zfile;
     int bgrade;
{
  size_t clen;
  char *zret;

  clen = strlen (qsys->uuconf_zname);
  zret = zbufalc (clen + CSEQLEN + 2);
  memcpy (zret, qsys->uuconf_zname, clen);
  zret[clen] = bgrade;
  memcpy (zret + clen + 1, zfile + strlen (zfile) - CSEQLEN, CSEQLEN + 1);
  return zret;
}

/* Turn a job id back into a file name.  */

char *
zsjobid_to_file (zid, pzsystem, pbgrade)
     const char *zid;
     char **pzsystem;
     char *pbgrade;
{
  size_t clen;
  const char *zend;
  char *zsys;
  char abname[CSEQLEN + 11];
  char *zret;

  clen = strlen (zid);
  if (clen <= CSEQLEN)
    {
      ulog (LOG_ERROR, "%s: Bad job id", zid);
      return NULL;
    }

  zend = zid + clen - CSEQLEN - 1;

  zsys = zbufalc (clen - CSEQLEN);
  memcpy (zsys, zid, clen - CSEQLEN - 1);
  zsys[clen - CSEQLEN - 1] = '\0';

  /* This must correspond to zsfile_name.  */
#if ! SPOOLDIR_TAYLOR
  sprintf (abname, "C.%.7s%s", zsys, zend);
#else
  sprintf (abname, "C.%s", zend);
#endif

  zret = zsfind_file (abname, zsys, *zend);

  if (zret != NULL && pzsystem != NULL)
    *pzsystem = zsys;
  else
    ubuffree (zsys);

  if (pbgrade != NULL)
    *pbgrade = *zend;

  return zret;
}
