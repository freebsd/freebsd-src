/* srmdir.c
   Remove a directory and all its contents.

   Copyright (C) 1992 Ian Lance Taylor

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
#include "sysdep.h"
#include "system.h"

#include <errno.h>

#if HAVE_FTW_H
#include <ftw.h>
#endif

static int isremove_dir P((const char *, struct stat *, int));

/* Keep a list of directories to be removed.  */

struct sdirlist
{
  struct sdirlist *qnext;
  char *zdir;
};

static struct sdirlist *qSdirlist;

/* Remove a directory and all files in it.  */

boolean
fsysdep_rmdir (zdir)
     const char *zdir;
{
  boolean fret;
  struct sdirlist *q;

  qSdirlist = NULL;

  fret = TRUE;
  if (ftw ((char *) zdir, isremove_dir, 5) != 0)
    {
      ulog (LOG_ERROR, "ftw: %s", strerror (errno));
      fret = FALSE;
    }

  q = qSdirlist;
  while (q != NULL)
    {
      struct sdirlist *qnext;
      
      if (rmdir (q->zdir) != 0)
	{
	  ulog (LOG_ERROR, "rmdir (%s): %s", q->zdir, strerror (errno));
	  fret = FALSE;
	}
      ubuffree (q->zdir);
      qnext = q->qnext;
      xfree ((pointer) q);
      q = qnext;
    }

  return fret;
}

/* Remove a file in a directory.  */

/*ARGSUSED*/
static int
isremove_dir (zfile, qstat, iflag)
     const char *zfile;
     struct stat *qstat;
     int iflag;
{
  if (iflag == FTW_D || iflag == FTW_DNR)
    {
      struct sdirlist *q;

      q = (struct sdirlist *) xmalloc (sizeof (struct sdirlist));
      q->qnext = qSdirlist;
      q->zdir = zbufcpy (zfile);
      qSdirlist = q;
    }
  else
    {
      if (remove (zfile) != 0)
	ulog (LOG_ERROR, "remove (%s): %s", zfile, strerror (errno));
    }

  return 0;
}
