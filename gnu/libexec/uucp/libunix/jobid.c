/* jobid.c
   Convert file names to jobids and vice versa.

   Copyright (C) 1991, 1992, 1995 Ian Lance Taylor

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#include "uuconf.h"
#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* Translate a file name and an associated system into a job id.
   These job ids are used by uustat.  */

char *
zsfile_to_jobid (qsys, zfile, bgrade)
     const struct uuconf_system *qsys;
     const char *zfile;
     int bgrade;
{
  size_t clen;
  char *zret;

  clen = strlen (qsys->uuconf_zname);

#if ! SPOOLDIR_TAYLOR

  /* We use the system name attached to the grade and sequence number.
     This won't work correctly if the file name was actually created
     by some other version of uucp that uses a different length for
     the sequence number.  Too bad.  */

  zret = zbufalc (clen + CSEQLEN + 2);
  memcpy (zret, qsys->uuconf_zname, clen);
  zret[clen] = bgrade;
  memcpy (zret + clen + 1, zfile + strlen (zfile) - CSEQLEN, CSEQLEN + 1);

#else

  /* We use the system name followed by a dot, the grade, and the
     sequence number.  In this case, the sequence number is a long
     string.  */

  {
    size_t cseqlen;

    /* zfile is SYS/C./C.gseq.  */
    zfile = strrchr (zfile, '/');

#if DEBUG > 0
    if (zfile == NULL
	|| zfile[1] != 'C'
	|| zfile[2] != '.'
	|| zfile[3] == '\0')
      ulog (LOG_FATAL, "zsfile_to_jobid: Can't happen");
#endif

    /* Make zfile point at .gseq.  */
    zfile += 2;

    cseqlen = strlen (zfile);
    zret = zbufalc (clen + cseqlen + 1);
    memcpy (zret, qsys->uuconf_zname, clen);
    memcpy (zret + clen, zfile, cseqlen + 1);
  }

#endif

  return zret;
}

/* Turn a job id back into a file name.  */

char *
zsjobid_to_file (zid, pzsystem, pbgrade)
     const char *zid;
     char **pzsystem;
     char *pbgrade;
{
#if ! SPOOLDIR_TAYLOR
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
  sprintf (abname, "C.%.7s%s", zsys, zend);

  zret = zsfind_file (abname, zsys, *zend);

  if (zret != NULL && pzsystem != NULL)
    *pzsystem = zsys;
  else
    ubuffree (zsys);

  if (pbgrade != NULL)
    *pbgrade = *zend;

  return zret;
#else /* SPOOLDIR_TAYLOR */
  char *zdot;
  size_t csyslen;
  char *zsys;
  char ab[15];
  char *zret;

  zdot = strrchr (zid, '.');
  if (zdot == NULL)
    {
      ulog (LOG_ERROR, "%s: Bad job id", zid);
      return NULL;
    }

  csyslen = zdot - zid;
  zsys = zbufalc (csyslen + 1);
  memcpy (zsys, zid, csyslen);
  zsys[csyslen] = '\0';

  ab[0] = 'C';
  strcpy (ab + 1, zdot);

  zret = zsfind_file (ab, zsys, zdot[1]);

  if (zret != NULL && pzsystem != NULL)
    *pzsystem = zsys;
  else
    ubuffree (zsys);

  if (pbgrade != NULL)
    *pbgrade = zdot[1];

  return zret;
#endif /* SPOOLDIR_TAYLOR */
}
