/* status.c
   Routines to get and set the status for a system.

   Copyright (C) 1991, 1992, 1993, 1995 Ian Lance Taylor

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

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>
#include <ctype.h>

#if SPOOLDIR_HDB || SPOOLDIR_SVR4

/* If we are using HDB spool layout, store status using HDB status
   values.  SVR4 is a variant of HDB.  */

#define MAP_STATUS 1

static const int aiMapstatus[] =
{
  0, 13, 7, 6, 20, 4, 3, 2
};
#define CMAPENTRIES (sizeof (aiMapstatus) / sizeof (aiMapstatus[0]))

#else /* ! SPOOLDIR_HDB && ! SPOOLDIR_SVR4 */

#define MAP_STATUS 0

#endif /* ! SPOOLDIR_HDB && ! SPOOLDIR_SVR4 */

/* Get the status of a system.  This assumes that we are in the spool
   directory.  */

boolean
fsysdep_get_status (qsys, qret, pfnone)
     const struct uuconf_system *qsys;
     struct sstatus *qret;
     boolean *pfnone;
{
  char *zname;
  FILE *e;
  char *zline;
  char *zend, *znext;
  boolean fbad;
  int istat;

  if (pfnone != NULL)
    *pfnone = FALSE;

  zname = zsysdep_in_dir (".Status", qsys->uuconf_zname);
  e = fopen (zname, "r");
  if (e == NULL)
    {
      if (errno != ENOENT)
	{
	  ulog (LOG_ERROR, "fopen (%s): %s", zname, strerror (errno));
	  ubuffree (zname);
	  return FALSE;
	}
      zline = NULL;
    }
  else
    {
      size_t cline;

      zline = NULL;
      cline = 0;
      if (getline (&zline, &cline, e) <= 0)
	{
	  xfree ((pointer) zline);
	  zline = NULL;
	}
      (void) fclose (e);
    }

  if (zline == NULL)
    {
      /* There is either no status file for this system, or it's been
	 truncated, so fake a good status.  */
      qret->ttype = STATUS_COMPLETE;
      qret->cretries = 0;
      qret->ilast = 0;
      qret->cwait = 0;
      qret->zstring = NULL;
      if (pfnone != NULL)
	*pfnone = TRUE;
      ubuffree (zname);
      return TRUE;
    }

  /* It turns out that scanf is not used much in this program, so for
     the benefit of small computers we avoid linking it in.  This is
     basically

     sscanf (zline, "%d %d %ld %d", &qret->ttype, &qret->cretries,
             &qret->ilast, &qret->cwait);

     except that it's done with strtol.  */

  fbad = FALSE;
  istat = (int) strtol (zline, &zend, 10);
  if (zend == zline)
    fbad = TRUE;

#if MAP_STATUS
  /* On some systems it may be appropriate to map system dependent status
     values on to our status values.  */
  {
    int i;

    for (i = 0; i < CMAPENTRIES; ++i)
      {
	if (aiMapstatus[i] == istat)
	  {
	    istat = i;
	    break;
	  }
      }
  }
#endif /* MAP_STATUS */

  if (istat < 0 || istat >= (int) STATUS_VALUES)
    istat = (int) STATUS_COMPLETE;
  qret->ttype = (enum tstatus_type) istat;
  znext = zend;
  qret->cretries = (int) strtol (znext, &zend, 10);
  if (zend == znext)
    fbad = TRUE;
  znext = zend;
  qret->ilast = strtol (znext, &zend, 10);
  if (zend == znext)
    fbad = TRUE;
  znext = zend;
  qret->cwait = (int) strtol (znext, &zend, 10);
  if (zend == znext)
    fbad = TRUE;

  if (! fbad)
    {
      znext = zend;
      while (isspace (BUCHAR (*znext)))
	++znext;
      if (*znext == '\0')
	qret->zstring = NULL;
      else
	{
	  if (*znext == '"')
	    ++znext;
	  qret->zstring = zbufcpy (znext);
	  zend = qret->zstring + strlen (qret->zstring);
	  while (zend != qret->zstring && *zend != ' ')
	    --zend;
	  if (*zend == '"' && zend != qret->zstring)
	    --zend;
	  if (zend != qret->zstring)
	    *zend = '\0';
	  else
	    {
	      ubuffree (qret->zstring);
	      qret->zstring = NULL;
	    }
	}
    }

  xfree ((pointer) zline);

  if (fbad)
    {
      ulog (LOG_ERROR, "%s: Bad status file format", zname);
      ubuffree (zname);
      return FALSE;
    }

  ubuffree (zname);

  return TRUE;
}

/* Set the status of a remote system.  This assumes the system is
   locked when this is called, and that the program is in the spool
   directory.  */

boolean
fsysdep_set_status (qsys, qset)
     const struct uuconf_system *qsys;
     const struct sstatus *qset;
{
  char *zname;
  FILE *e;
  int istat;

  zname = zsysdep_in_dir (".Status", qsys->uuconf_zname);

  e = esysdep_fopen (zname, TRUE, FALSE, TRUE);
  ubuffree (zname);
  if (e == NULL)
    return FALSE;
  istat = (int) qset->ttype;

#if MAP_STATUS
  /* On some systems it may be appropriate to map istat onto a system
     dependent number.  */
  if (istat >= 0 && istat < CMAPENTRIES)
    istat = aiMapstatus[istat];
#endif /* MAP_STATUS */

  fprintf (e, "%d %d %ld %d ", istat, qset->cretries, qset->ilast,
	   qset->cwait);

#if SPOOLDIR_SVR4
  fprintf (e, "\"%s\"", azStatus[(int) qset->ttype]);
#else
  fprintf (e, "%s", azStatus[(int) qset->ttype]);
#endif

  fprintf (e, " %s\n", qsys->uuconf_zname);
  if (fclose (e) != 0)
    {
      ulog (LOG_ERROR, "fclose: %s", strerror (errno));
      return FALSE;
    }

  return TRUE;
}
