/* Look up user and/or group names.
   Copyright (C) 1988, 1992 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Look up user and/or group names.
 *
 * This file should be modified for non-unix systems to do something
 * reasonable.
 */

#include <sys/types.h>
#include "tar.h"
#include "port.h"

#ifndef NONAMES
/* Whole module goes away if NONAMES defined.  Otherwise... */
#include <stdio.h>
#include <pwd.h>
#include <grp.h>

static int saveuid = -993;
static char saveuname[TUNMLEN];
static int my_uid = -993;

static int savegid = -993;
static char savegname[TGNMLEN];
static int my_gid = -993;

#define myuid	( my_uid < 0? (my_uid = getuid()): my_uid )
#define	mygid	( my_gid < 0? (my_gid = getgid()): my_gid )

/*
 * Look up a user or group name from a uid/gid, maintaining a cache.
 * FIXME, for now it's a one-entry cache.
 * FIXME2, the "-993" is to reduce the chance of a hit on the first lookup.
 *
 * This is ifdef'd because on Suns, it drags in about 38K of "yellow
 * pages" code, roughly doubling the program size.  Thanks guys.
 */
void
finduname (uname, uid)
     char uname[TUNMLEN];
     int uid;
{
  struct passwd *pw;
#ifndef HAVE_GETPWUID
  extern struct passwd *getpwuid ();
#endif

  if (uid != saveuid)
    {
      saveuid = uid;
      saveuname[0] = '\0';
      pw = getpwuid (uid);
      if (pw)
	strncpy (saveuname, pw->pw_name, TUNMLEN);
    }
  strncpy (uname, saveuname, TUNMLEN);
}

int
finduid (uname)
     char uname[TUNMLEN];
{
  struct passwd *pw;
  extern struct passwd *getpwnam ();

  if (uname[0] != saveuname[0]	/* Quick test w/o proc call */
      || 0 != strncmp (uname, saveuname, TUNMLEN))
    {
      strncpy (saveuname, uname, TUNMLEN);
      pw = getpwnam (uname);
      if (pw)
	{
	  saveuid = pw->pw_uid;
	}
      else
	{
	  saveuid = myuid;
	}
    }
  return saveuid;
}


void
findgname (gname, gid)
     char gname[TGNMLEN];
     int gid;
{
  struct group *gr;
#ifndef HAVE_GETGRGID
  extern struct group *getgrgid ();
#endif

  if (gid != savegid)
    {
      savegid = gid;
      savegname[0] = '\0';
      (void) setgrent ();
      gr = getgrgid (gid);
      if (gr)
	strncpy (savegname, gr->gr_name, TGNMLEN);
    }
  (void) strncpy (gname, savegname, TGNMLEN);
}


int
findgid (gname)
     char gname[TUNMLEN];
{
  struct group *gr;
  extern struct group *getgrnam ();

  if (gname[0] != savegname[0]	/* Quick test w/o proc call */
      || 0 != strncmp (gname, savegname, TUNMLEN))
    {
      strncpy (savegname, gname, TUNMLEN);
      gr = getgrnam (gname);
      if (gr)
	{
	  savegid = gr->gr_gid;
	}
      else
	{
	  savegid = mygid;
	}
    }
  return savegid;
}

#endif
