/* ufopen.c
   Open a file with the permissions of the invoking user.

   Copyright (C) 1992, 1995 Ian Lance Taylor

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
#include "sysdep.h"
#include "system.h"

#include <errno.h>

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

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

/* Open a file with the permissions of the invoking user.  Ignore the
   fbinary argument since Unix has no distinction between text and
   binary files.  */

/*ARGSUSED*/
openfile_t
esysdep_user_fopen (zfile, frd, fbinary)
     const char *zfile;
     boolean frd;
     boolean fbinary;
{
  uid_t ieuid;
  openfile_t e;
  const char *zerr;
  int o = 0;

  if (! fsuser_perms (&ieuid))
    return EFILECLOSED;

  zerr = NULL;

#if USE_STDIO
  e = fopen (zfile, frd ? "r" : "w");
  if (e == NULL)
    zerr = "fopen";
  else
    o = fileno (e);
#else
  if (frd)
    {
      e = open ((char *) zfile, O_RDONLY | O_NOCTTY, 0);
      zerr = "open";
    }
  else
    {
      e = creat ((char *) zfile, IPUBLIC_FILE_MODE);
      zerr = "creat";
    }
  if (e >= 0)
    {
      o = e;
      zerr = NULL;
    }
#endif

  if (! fsuucp_perms ((long) ieuid))
    {
      if (ffileisopen (e))
	(void) ffileclose (e);
      return EFILECLOSED;
    }

  if (zerr != NULL)
    {
      ulog (LOG_ERROR, "%s (%s): %s", zerr, zfile, strerror (errno));
#if ! HAVE_SETREUID
      /* Are these error messages helpful or confusing?  */
#if HAVE_SAVED_SETUID
      if (errno == EACCES && getuid () == 0)
	ulog (LOG_ERROR,
	      "The superuser may only transfer files that are readable by %s",
	      OWNER);
#else
      if (errno == EACCES)
	ulog (LOG_ERROR,
	      "You may only transfer files that are readable by %s", OWNER);
#endif
#endif /* ! HAVE_SETREUID */
      return EFILECLOSED;
    }

  if (fcntl (o, F_SETFD, fcntl (o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
      (void) ffileclose (e);
      return EFILECLOSED;
    }

  return e;
}
