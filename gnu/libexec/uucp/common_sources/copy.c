/* copy.c
   Copy one file to another for the UUCP package.

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

#if USE_RCS_ID
const char copy_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/common_sources/copy.c,v 1.6 1999/08/27 23:33:04 peter Exp $";
#endif

#include "uudefs.h"
#include "system.h"
#include "sysdep.h"

#include <stdio.h>
#include <errno.h>

/* Copy one file to another.  */

#if USE_STDIO

boolean
fcopy_file (zfrom, zto, fpublic, fmkdirs, fsignals)
     const char *zfrom;
     const char *zto;
     boolean fpublic;
     boolean fmkdirs;
     boolean fsignals;
{
  FILE *efrom;
  boolean fret;

  efrom = fopen (zfrom, BINREAD);
  if (efrom == NULL)
    {
      ulog (LOG_ERROR, "fopen (%s): %s", zfrom, strerror (errno));
      return FALSE;
    }

  fret = fcopy_open_file (efrom, zto, fpublic, fmkdirs, fsignals);
  (void) fclose (efrom);
  return fret;
}

boolean
fcopy_open_file (efrom, zto, fpublic, fmkdirs, fsignals)
     FILE *efrom;
     const char *zto;
     boolean fpublic;
     boolean fmkdirs;
     boolean fsignals;
{
  FILE *eto;
  char ab[8192];
  int c;

  eto = esysdep_fopen (zto, fpublic, FALSE, fmkdirs);
  if (eto == NULL)
    return FALSE;

  while ((c = fread (ab, sizeof (char), sizeof ab, efrom)) != 0)
    {
      if (fwrite (ab, sizeof (char), (size_t) c, eto) != c)
	{
	  ulog (LOG_ERROR, "fwrite: %s", strerror (errno));
	  (void) fclose (eto);
	  (void) remove (zto);
	  return FALSE;
	}
      if (fsignals && FGOT_SIGNAL ())
	{
	  /* Log the signal.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	  (void) fclose (eto);
	  (void) remove (zto);
	  return FALSE;
	}
    }

  if (! fsysdep_sync (eto, zto))
    {
      (void) fclose (eto);
      (void) remove (zto);
      return FALSE;
    }

  if (fclose (eto) != 0)
    {
      ulog (LOG_ERROR, "fclose: %s", strerror (errno));
      (void) remove (zto);
      return FALSE;
    }

  return TRUE;
}

#else /* ! USE_STDIO */

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

boolean
fcopy_file (zfrom, zto, fpublic, fmkdirs, fsignals)
     const char *zfrom;
     const char *zto;
     boolean fpublic;
     boolean fmkdirs;
     boolean fsignals;
{
  int ofrom;
  boolean fret;

  ofrom = open (zfrom, O_RDONLY | O_NOCTTY, 0);
  if (ofrom < 0)
    {
      ulog (LOG_ERROR, "open (%s): %s", zfrom, strerror (errno));
      return FALSE;
    }

  fret = fcopy_open_file (ofrom, zto, fpublic, fmkdirs, fsignals);
  (void) close (ofrom);
  return fret;
}

boolean
fcopy_open_file (ofrom, zto, fpublic, fmkdirs, fsignals)
     int ofrom;
     const char *zto;
     boolean fpublic;
     boolean fmkdirs;
     boolean fsignals;
{
  int oto;
  char ab[8192];
  int c;

  /* These file mode arguments are from the UNIX version of sysdep.h;
     each system dependent header file will need their own
     definitions.  */
  oto = creat (zto, fpublic ? IPUBLIC_FILE_MODE : IPRIVATE_FILE_MODE);
  if (oto < 0)
    {
      if (errno == ENOENT && fmkdirs)
	{
	  if (! fsysdep_make_dirs (zto, fpublic))
	    return FALSE;
	  oto = creat (zto,
		       fpublic ? IPUBLIC_FILE_MODE : IPRIVATE_FILE_MODE);
	}
      if (oto < 0)
	{
	  ulog (LOG_ERROR, "open (%s): %s", zto, strerror (errno));
	  return FALSE;
	}
    }

  while ((c = read (ofrom, ab, sizeof ab)) > 0)
    {
      if (write (oto, ab, (size_t) c) != c)
	{
	  ulog (LOG_ERROR, "write: %s", strerror (errno));
	  (void) close (oto);
	  (void) remove (zto);
	  return FALSE;
	}
      if (fsignals && FGOT_SIGNAL ())
	{
	  /* Log the signal.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	  (void) fclose (eto);
	  (void) remove (zto);
	  return FALSE;
	}
    }

  if (! fsysdep_sync (oto, zto))
    {
      (void) close (oto);
      (void) remove (zto);
      return FALSE;
    }

  if (close (oto) < 0)
    {
      ulog (LOG_ERROR, "close: %s", strerror (errno));
      (void) remove (zto);
      return FALSE;
    }

  if (c < 0)
    {
      ulog (LOG_ERROR, "read: %s", strerror (errno));
      (void) remove (zto);
      return FALSE;
    }

  return TRUE;
}

#endif /* ! USE_STDIO */
