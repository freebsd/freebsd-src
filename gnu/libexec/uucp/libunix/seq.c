/* seq.c
   Get and increment the conversation sequence number for a system.

   Copyright (C) 1991, 1992, 1993 Ian Lance Taylor

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

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

/* Get the current conversation sequence number for a remote system,
   and increment it for next time.  The conversation sequence number
   is kept in a file named for the system in the directory .Sequence
   in the spool directory.  This is not compatible with other versions
   of UUCP, but it makes more sense to me.  The sequence file is only
   used if specified in the information for that system.  */

long
ixsysdep_get_sequence (qsys)
     const struct uuconf_system *qsys;
{
  FILE *e;
  char *zname;
  struct stat s;
  long iseq;

  /* This will only be called when the system is locked anyhow, so there
     is no need to use a separate lock for the conversation sequence
     file.  */
  zname = zsysdep_in_dir (".Sequence", qsys->uuconf_zname);

  iseq = 0;
  if (stat (zname, &s) == 0)
    {
      boolean fok;
      char *zline;
      size_t cline;

      /* The file should only be readable and writable by uucp.  */
      if ((s.st_mode & (S_IRWXG | S_IRWXO)) != 0)
	{
	  ulog (LOG_ERROR,
		"Bad file protection for conversation sequence file");
	  ubuffree (zname);
	  return -1;
	}
    
      e = fopen (zname, "r+");
      if (e == NULL)
	{
	  ulog (LOG_ERROR, "fopen (%s): %s", zname, strerror (errno));
	  ubuffree (zname);
	  return -1;
	}

      ubuffree (zname);

      fok = TRUE;
      zline = NULL;
      cline = 0;
      if (getline (&zline, &cline, e) <= 0)
	fok = FALSE;
      else
	{
	  char *zend;

	  iseq = strtol (zline, &zend, 10);
	  if (zend == zline)
	    fok = FALSE;
	}

      xfree ((pointer) zline);

      if (! fok)
	{
	  ulog (LOG_ERROR, "Bad format for conversation sequence file");
	  (void) fclose (e);
	  return -1;
	}

      rewind (e);
    }
  else
    {
      e = esysdep_fopen (zname, FALSE, FALSE, TRUE);
      ubuffree (zname);
      if (e == NULL)
	return -1;
    }

  ++iseq;

  fprintf (e, "%ld", iseq);

  if (fclose (e) != 0)
    {
      ulog (LOG_ERROR, "fclose: %s", strerror (errno));
      return -1;
    }

  return iseq;
}
